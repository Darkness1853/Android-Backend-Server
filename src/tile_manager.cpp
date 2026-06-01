#include "tile_manager.hpp"
#include <stb_image.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

static size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
    auto* vec = (std::vector<unsigned char>*)userp;
    size_t total = size * nmemb;
    vec->insert(vec->end(), (unsigned char*)data, (unsigned char*)data + total);
    return total;
}

static std::string makeUrl(int z, int x, int y) {
    return "https://tile.openstreetmap.org/" +
           std::to_string(z) + "/" +
           std::to_string(x) + "/" +
           std::to_string(y) + ".png";
}

std::string TileManager::getTilePath(int z, int x, int y) {
    return "tile_cache/" + std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".png";
}

TileManager::TileManager() {
    th = std::thread(&TileManager::worker, this);
}

TileManager::~TileManager() {
    running = false;
    cv.notify_all();
    if (th.joinable()) th.join();

    for (auto& [k, t] : tiles) {
        if (t.tex) glDeleteTextures(1, &t.tex);
    }
}

Tile* TileManager::get(int z, int x, int y) {
    TileKey k{z, x, y};
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = tiles.find(k);
        if (it != tiles.end())
            return &it->second;
        tiles.emplace(k, Tile{});
    }
    request(z, x, y);
    std::lock_guard<std::mutex> lock(mtx);
    return &tiles[k];
}

void TileManager::request(int z, int x, int y) {
    std::lock_guard<std::mutex> lock(mtx);
    TileKey k{z, x, y};
    if (tiles[k].loading || tiles[k].ready)
        return;
    tiles[k].loading = true;
    {
        std::lock_guard<std::mutex> qlock(job_mtx);
        jobs.push({z, x, y});
    }
    cv.notify_one();
}

bool TileManager::loadFromDisk(int z, int x, int y, std::vector<unsigned char>& out) {
    std::string path = getTilePath(z, x, y);
    if (!fs::exists(path)) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return !out.empty();
}

void TileManager::saveToDisk(int z, int x, int y, const std::vector<unsigned char>& data) {
    std::string dir = "tile_cache/" + std::to_string(z) + "/" + std::to_string(x);
    fs::create_directories(dir);
    std::string path = dir + "/" + std::to_string(y) + ".png";
    std::ofstream file(path, std::ios::binary);
    if (file) {
        file.write((char*)data.data(), data.size());
    }
}

void TileManager::worker() {
    CURL* curl = curl_easy_init();
    while (running) {
        Job j;
        {
            std::unique_lock<std::mutex> lock(job_mtx);
            cv.wait(lock, [&] { return !jobs.empty() || !running; });
            if (!running) break;
            j = jobs.front();
            jobs.pop();
        }

        std::vector<unsigned char> data;
        if (loadFromDisk(j.z, j.x, j.y, data)) {
        } else {
            curl_easy_reset(curl);
            curl_easy_setopt(curl, CURLOPT_URL, makeUrl(j.z, j.x, j.y).c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MyGpsMonitor/1.0");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            if (curl_easy_perform(curl) == CURLE_OK && !data.empty()) {
                saveToDisk(j.z, j.x, j.y, data);
            } else {
                continue;
            }
        }

        int w, h, c;
        unsigned char* img = stbi_load_from_memory(data.data(), data.size(), &w, &h, &c, 4);
        if (!img) continue;

        std::lock_guard<std::mutex> lock(mtx);
        TileKey k{j.z, j.x, j.y};
        auto& t = tiles[k];
        t.w = w;
        t.h = h;
        t.rgba.assign(img, img + w * h * 4);
        t.ready = true;
        t.loading = false;
        stbi_image_free(img);
    }
    curl_easy_cleanup(curl);
}

void TileManager::updateGL() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& [k, t] : tiles) {
        if (!t.ready || t.tex || t.rgba.empty()) continue;
        glGenTextures(1, &t.tex);
        glBindTexture(GL_TEXTURE_2D, t.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
        t.rgba.clear();
    }
}