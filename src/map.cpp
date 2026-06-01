#include "map.hpp"
#include "tile_manager.hpp"
#include "imgui.h"
#include <cmath>
#include <vector>
#include <mutex>
#include <GL/gl.h>

static TileManager tileManager;
static std::vector<MapPoint> map_points;
static std::mutex points_mutex;

static const double PI = 3.141592653589793;

static int zoom = 12;
static double center_lat = 55.0084;
static double center_lon = 82.9357;

static double lon_to_x(double lon, int z) {
    return (lon + 180.0) / 360.0 * (1 << z);
}

static double lat_to_y(double lat, int z) {
    double r = lat * PI / 180.0;
    return (1.0 - std::log(std::tan(PI / 4.0 + r / 2.0)) / PI) / 2.0 * (1 << z);
}

static double x_to_lon(double x, int z) {
    return x / (double)(1 << z) * 360.0 - 180.0;
}

static double y_to_lat(double y, int z) {
    double n = PI - 2.0 * PI * y / (1 << z);
    return 180.0 / PI * std::atan(std::sinh(n));
}

void init_map() {}

void update_map_points(const std::vector<MapPoint>& points) {
    std::lock_guard<std::mutex> lock(points_mutex);
    map_points = points;
}

void set_map_center(double lat, double lon, int z) {
    center_lat = lat;
    center_lon = lon;
    zoom = z;
}

double get_map_center_lat() {
    return center_lat;
}

double get_map_center_lon() {
    return center_lon;
}

int get_map_zoom() {
    return zoom;
}

void draw_map(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    double center_x = lon_to_x(center_lon, zoom);
    double center_y = lat_to_y(center_lat, zoom);
    double start_tile_x = center_x - (size.x / 2.0) / 256.0;
    double start_tile_y = center_y - (size.y / 2.0) / 256.0;
    int start_i = (int)std::floor(start_tile_x);
    int start_j = (int)std::floor(start_tile_y);
    int end_i = (int)std::ceil(start_tile_x + size.x / 256.0);
    int end_j = (int)std::ceil(start_tile_y + size.y / 256.0);

    tileManager.updateGL();
    
    for (int i = start_i; i <= end_i; ++i) {
        for (int j = start_j; j <= end_j; ++j) {
            Tile* tile = tileManager.get(zoom, i, j);
            float screen_x = pos.x + (float)(i - start_tile_x) * 256.0f;
            float screen_y = pos.y + (float)(j - start_tile_y) * 256.0f;
            if (tile && tile->ready && tile->tex) {
                dl->AddImage((ImTextureID)(uintptr_t)tile->tex,
                             ImVec2(screen_x, screen_y),
                             ImVec2(screen_x + 256.0f, screen_y + 256.0f));
            } else {
                dl->AddRectFilled(ImVec2(screen_x, screen_y),
                                  ImVec2(screen_x + 256.0f, screen_y + 256.0f),
                                  IM_COL32(35,35,35,255));
            }
        }
    }

    std::vector<MapPoint> points_copy;
    {
        std::lock_guard<std::mutex> lock(points_mutex);
        points_copy = map_points;
    }

    for (const auto& p : points_copy) {
        double px = lon_to_x(p.lon, zoom);
        double py = lat_to_y(p.lat, zoom);
        float screen_x = pos.x + (float)(px - start_tile_x) * 256.0f;
        float screen_y = pos.y + (float)(py - start_tile_y) * 256.0f;
        if (screen_x >= pos.x && screen_x <= pos.x + size.x && 
            screen_y >= pos.y && screen_y <= pos.y + size.y) {
            dl->AddCircleFilled(ImVec2(screen_x, screen_y), 5.0f, IM_COL32(255,50,50,255));
            dl->AddCircle(ImVec2(screen_x, screen_y), 5.0f, IM_COL32(0,0,0,255), 0, 1.5f);
        }
    }
}

void draw_map_ui() {
    std::lock_guard<std::mutex> lock(points_mutex);
    ImGui::Text("Center: %.4f, %.4f", center_lat, center_lon);
    ImGui::Text("Zoom: %d", zoom);
    ImGui::Text("Points: %zu", map_points.size());
}

void handle_map_input(ImVec2 pos, ImVec2 size) {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = io.MouseDelta;
        double s = 256.0 * (1 << zoom);
        center_lon -= (delta.x / s) * 360.0;
        double lat_rad = center_lat * PI / 180.0;
        center_lat += (delta.y / s) * 360.0 * std::cos(lat_rad);
    }
    
    if (io.MouseWheel != 0) {
        double mouse_x = io.MousePos.x - pos.x;
        double mouse_y = io.MousePos.y - pos.y;
        double center_x = lon_to_x(center_lon, zoom);
        double center_y = lat_to_y(center_lat, zoom);
        double start_tile_x = center_x - (size.x / 2.0) / 256.0;
        double start_tile_y = center_y - (size.y / 2.0) / 256.0;
        double mouse_lon = x_to_lon(start_tile_x + mouse_x / 256.0, zoom);
        double mouse_lat = y_to_lat(start_tile_y + mouse_y / 256.0, zoom);
        zoom += (io.MouseWheel > 0) ? 1 : -1;
        if (zoom < 1) zoom = 1;
        if (zoom > 19) zoom = 19;
        double new_mouse_x = lon_to_x(mouse_lon, zoom);
        double new_mouse_y = lat_to_y(mouse_lat, zoom);
        center_lon = x_to_lon(new_mouse_x - (mouse_x - size.x / 2.0) / 256.0, zoom);
        center_lat = y_to_lat(new_mouse_y - (mouse_y - size.y / 2.0) / 256.0, zoom);
    }
}