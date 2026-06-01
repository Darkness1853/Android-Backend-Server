#pragma once

#include "imgui.h"
#include "MapPoint.hpp"
#include <vector>

void init_map();
void update_map_points(const std::vector<MapPoint>& points);
void set_map_center(double lat, double lon, int zoom);
double get_map_center_lat();
double get_map_center_lon();
int get_map_zoom();
void draw_map(ImDrawList* dl, ImVec2 pos, ImVec2 size);
void draw_map_ui();
void handle_map_input(ImVec2 pos, ImVec2 size);