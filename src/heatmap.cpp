#include "heatmap.hpp"
#include "tile_manager.hpp"
#include "imgui.h"
#include <cmath>
#include <vector>
#include <mutex>
#include <algorithm>
#include <future>
#include <thread>
#include <GL/gl.h>

static TileManager tileManager;
static std::vector<MapPoint> current_points;
static std::mutex points_mutex;

static const double PI = 3.141592653589793;
static const double METERS_PER_DEGREE = 111320.0;

static int zoom = 12;
static double center_lat = 55.0084;
static double center_lon = 82.9357;

static float idw_radius_meters = 25.0f;
static HeatmapCriterion current_criterion = HeatmapCriterion::RSRP;

static GLuint heatmap_tex = 0;
static std::vector<uint32_t> heatmap_pixels;
static int heatmap_tex_w = 0, heatmap_tex_h = 0;
static bool texture_needs_upload = false;
static std::mutex tex_mutex;
static std::future<void> calculation_future;
static bool is_calculating = false;

static double geo_min_lat = 0, geo_max_lat = 0, geo_min_lon = 0, geo_max_lon = 0;
static float last_radius = 0;
static HeatmapCriterion last_criterion = (HeatmapCriterion)-1;
static size_t last_points_count = 0;

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

void init_heatmap() {}

void update_heatmap_points(const std::vector<MapPoint>& points) {
    std::lock_guard<std::mutex> lock(points_mutex);
        current_points = points;
}

void set_heatmap_center(double lat, double lon, int z) {
    center_lat = lat;
    center_lon = lon;
    zoom = z;
}

void set_heatmap_criterion(HeatmapCriterion criterion) {
    current_criterion = criterion;
}

void set_heatmap_radius(float radius) {
    idw_radius_meters = radius;
}

static uint32_t getSignalColor(double rsrp) {
    double t = (rsrp + 110.0) / 60.0;
    t = std::clamp(t, 0.0, 1.0);
    
    int r, g, b;
    
    if (t < 0.33) {
        double t2 = t / 0.33;
        r = 255;
        g = (int)(255 * t2);
        b = 0;
    } else if (t < 0.66) {
        double t2 = (t - 0.33) / 0.33;
        r = (int)(255 * (1 - t2));
        g = 255;
        b = 0;
    } else {
        double t2 = (t - 0.66) / 0.34;
        r = 0;
        g = (int)(255 * (1 - t2));
        b = (int)(255 * t2);
    }
    
    return IM_COL32(r, g, b, 200);
}

static void calculate_heatmap_async(int w, int h, double min_lat, double max_lat, double min_lon, double max_lon,
                                    float radius, std::vector<MapPoint> points, HeatmapCriterion criterion) {
    std::vector<uint32_t> local_pixels(w * h, 0x00000000);
    double radius_lat = radius / METERS_PER_DEGREE;
    double center_lat_rad = (min_lat + max_lat) / 2.0 * PI / 180.0;
    double radius_lon = radius / (METERS_PER_DEGREE * std::cos(center_lat_rad));
    
    for (int sy = 0; sy < h; ++sy) {
        double ratio_y = (double)(h - 1 - sy) / (double)h;
        double p_lat = min_lat + ratio_y * (max_lat - min_lat);
        double cos_lat = std::cos(p_lat * PI / 180.0);
        double local_radius_lon = radius / (METERS_PER_DEGREE * cos_lat);
        
        for (int sx = 0; sx < w; ++sx) {
            double ratio_x = (double)sx / (double)w;
            double p_lon = min_lon + ratio_x * (max_lon - min_lon);
            
            double total_weight = 0.0;
            double total_value = 0.0;
            
            for (const auto& pt : points) {
                if (std::abs(pt.lat - p_lat) > radius_lat) continue;
                if (std::abs(pt.lon - p_lon) > local_radius_lon) continue;
                
                double d_lat = (pt.lat - p_lat) * METERS_PER_DEGREE;
                double d_lon = (pt.lon - p_lon) * METERS_PER_DEGREE * cos_lat;
                double dist = std::sqrt(d_lat * d_lat + d_lon * d_lon);
                
                if (dist <= radius) {
                    double weight = 1.0 / (dist * dist + 0.1);
                    double val = pt.signal_strength;
                    total_weight += weight;
                    total_value += val * weight;
                }
            }
            
            if (total_weight > 0) {
                double avg_val = total_value / total_weight;
                local_pixels[sy * w + sx] = getSignalColor(avg_val);
            }
        }
    }
    
    std::lock_guard<std::mutex> lock(tex_mutex);
    heatmap_pixels = std::move(local_pixels);
    heatmap_tex_w = w;
    heatmap_tex_h = h;
    texture_needs_upload = true;
    is_calculating = false;
}

void draw_heatmap(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
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

    {
        std::lock_guard<std::mutex> lock(tex_mutex);
        if (texture_needs_upload) {
            if (heatmap_tex == 0) glGenTextures(1, &heatmap_tex);
            glBindTexture(GL_TEXTURE_2D, heatmap_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, heatmap_tex_w, heatmap_tex_h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, heatmap_pixels.data());
            texture_needs_upload = false;
        }
    }

    std::vector<MapPoint> points_copy;
    {
        std::lock_guard<std::mutex> lock(points_mutex);
        points_copy = current_points;
    }

    static size_t last_calc_points_count = 0;
    bool need_recalc = (idw_radius_meters != last_radius || 
                        current_criterion != last_criterion ||
                        points_copy.size() != last_calc_points_count);
    
    if (need_recalc && !is_calculating && !points_copy.empty()) {
        is_calculating = true;
        last_calc_points_count = points_copy.size();
        
        double min_lat = 90.0, max_lat = -90.0, min_lon = 180.0, max_lon = -180.0;
        for (const auto& p : points_copy) {
            min_lat = std::min(min_lat, p.lat);
            max_lat = std::max(max_lat, p.lat);
            min_lon = std::min(min_lon, p.lon);
            max_lon = std::max(max_lon, p.lon);
        }
        double padding_lat = (idw_radius_meters / METERS_PER_DEGREE) + 0.005;
        double padding_lon = (idw_radius_meters / (METERS_PER_DEGREE * std::cos(min_lat * PI / 180.0))) + 0.005;
        geo_min_lat = min_lat - padding_lat;
        geo_max_lat = max_lat + padding_lat;
        geo_min_lon = min_lon - padding_lon;
        geo_max_lon = max_lon + padding_lon;
        last_radius = idw_radius_meters;
        last_criterion = current_criterion;
        last_points_count = points_copy.size();
        
        calculation_future = std::async(std::launch::async, calculate_heatmap_async,
                                        256, 256, geo_min_lat, geo_max_lat, geo_min_lon, geo_max_lon,
                                        idw_radius_meters, std::move(points_copy), current_criterion);
    }

    if (heatmap_tex != 0 && geo_max_lat > geo_min_lat) {
        double tex_start_x = lon_to_x(geo_min_lon, zoom);
        double tex_start_y = lat_to_y(geo_max_lat, zoom);
        double tex_end_x = lon_to_x(geo_max_lon, zoom);
        double tex_end_y = lat_to_y(geo_min_lat, zoom);
        float h_screen_start_x = pos.x + (float)(tex_start_x - start_tile_x) * 256.0f;
        float h_screen_start_y = pos.y + (float)(tex_start_y - start_tile_y) * 256.0f;
        float h_screen_end_x = pos.x + (float)(tex_end_x - start_tile_x) * 256.0f;
        float h_screen_end_y = pos.y + (float)(tex_end_y - start_tile_y) * 256.0f;
        dl->AddImage((ImTextureID)(uintptr_t)heatmap_tex,
                     ImVec2(h_screen_start_x, h_screen_start_y),
                     ImVec2(h_screen_end_x, h_screen_end_y));
    }

    for (const auto& p : points_copy) {
        double px = lon_to_x(p.lon, zoom);
        double py = lat_to_y(p.lat, zoom);
        float screen_x = pos.x + (float)(px - start_tile_x) * 256.0f;
        float screen_y = pos.y + (float)(py - start_tile_y) * 256.0f;
        if (screen_x >= pos.x && screen_x <= pos.x + size.x && screen_y >= pos.y && screen_y <= pos.y + size.y) {
            dl->AddCircleFilled(ImVec2(screen_x, screen_y), 3.0f, IM_COL32(255,255,255,255));
            dl->AddCircle(ImVec2(screen_x, screen_y), 3.0f, IM_COL32(0,0,0,255), 0, 1.0f);
        }
    }
}

void draw_heatmap_ui() {
    const char* items[] = { "RSRP", "RSRQ", "RSSI", "ALTITUDE" };
    int item_current = (int)current_criterion;
    if (ImGui::Combo("Criterion", &item_current, items, IM_ARRAYSIZE(items)))
        current_criterion = (HeatmapCriterion)item_current;
    ImGui::SliderFloat("Radius (m)", &idw_radius_meters, 10.0f, 40.0f, "%.1f m");
    std::lock_guard<std::mutex> lock(points_mutex);
    ImGui::Text("Center: %.4f, %.4f", center_lat, center_lon);
    ImGui::Text("Zoom: %d", zoom);
    ImGui::Text("Points: %zu", current_points.size());
    if (is_calculating) ImGui::SameLine(), ImGui::TextColored(ImVec4(1,1,0,1), "[Wait...]");
}

void handle_heatmap_input(ImVec2 pos, ImVec2 size) {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = io.MouseDelta;
        double s = 256.0 * (1 << zoom);
        center_lon -= (delta.x / s) * 360.0;
        double lat_rad = center_lat * PI / 180.0;
        center_lat += (delta.y / s) * 360.0 * std::cos(lat_rad);
        
        if (center_lon < -180.0) center_lon = -180.0;
        if (center_lon > 180.0) center_lon = 180.0;
        if (center_lat < -85.0) center_lat = -85.0;
        if (center_lat > 85.0) center_lat = 85.0;
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
        int new_zoom = zoom + ((io.MouseWheel > 0) ? 1 : -1);
        new_zoom = std::clamp(new_zoom, 3, 18);
        if (new_zoom != zoom) {
            zoom = new_zoom;
            double new_mouse_x = lon_to_x(mouse_lon, zoom);
            double new_mouse_y = lat_to_y(mouse_lat, zoom);
            center_lon = x_to_lon(new_mouse_x - (mouse_x - size.x / 2.0) / 256.0, zoom);
            center_lat = y_to_lat(new_mouse_y - (mouse_y - size.y / 2.0) / 256.0, zoom);
            
            if (center_lon < -180.0) center_lon = -180.0;
            if (center_lon > 180.0) center_lon = 180.0;
            if (center_lat < -85.0) center_lat = -85.0;
            if (center_lat > 85.0) center_lat = 85.0;
        }
    }
}