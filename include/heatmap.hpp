
#pragma once
#include "imgui.h"
#include "MapPoint.hpp"
#include <vector>

enum class HeatmapCriterion {
    RSRP, RSRQ, RSSI, ALTITUDE
};

void init_heatmap();
void update_heatmap_points(const std::vector<MapPoint>& points);
void set_heatmap_center(double lat, double lon, int zoom);
void set_heatmap_criterion(HeatmapCriterion criterion);
void set_heatmap_radius(float radius);
void draw_heatmap(ImDrawList* dl, ImVec2 pos, ImVec2 size);
void draw_heatmap_ui();
void handle_heatmap_input(ImVec2 pos, ImVec2 size);