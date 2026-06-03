#include "gui.hpp"
#include "map.hpp"
#include "heatmap.hpp"
#include "db_client.hpp"
#include "gui.hpp"
#include "map.hpp"
#include "db_client.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <pqxx/pqxx>
#include <iostream>

static std::vector<MapPoint> map_points;

static void window_close_callback(GLFWwindow* window, Location* loc) {
    loc->running = false;  
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void load_points_from_db(std::vector<MapPoint>& points, pqxx::connection& db_conn) {
    points.clear();
    try {
        pqxx::work txn(db_conn);
        
        std::string query = 
            "SELECT DISTINCT ON (loc.timestamp) "
            "   l.latitude, l.longitude, loc.timestamp, "
            "   c.rsrp, c.pci, c.network_type "
            "FROM locations l "
            "JOIN location loc ON l.location_id = loc.id "
            "LEFT JOIN cells c ON loc.id = c.location_id "
            "WHERE l.latitude IS NOT NULL "
            "   AND l.longitude IS NOT NULL "
            "   AND c.rsrp IS NOT NULL "
            "   AND c.rsrp BETWEEN -140 AND -40 "
            "ORDER BY loc.timestamp DESC, c.id";
        
        pqxx::result result = txn.exec(query);
        
        for (const auto& row : result) {
            MapPoint p;
            p.lat = row[0].as<double>();
            p.lon = row[1].as<double>();
            p.timestamp = row[2].as<long long>();
            p.signal_strength = row[3].as<int>();
            p.pci = row[4].as<int>();
            p.type = row[5].as<std::string>();
            points.push_back(p);
        }
        
        txn.commit();
        update_map_points(points);
        update_heatmap_points(points);
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading points: " << e.what() << std::endl;
    }
}

void run_gui(Location* loc) {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1400, 800, "Location Server", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewInit();

    glfwSetWindowUserPointer(window, loc);
    glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
        Location* loc = static_cast<Location*>(glfwGetWindowUserPointer(w));
        if (loc) loc->running = false;
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int current_graph = 0;
    const char* graph_items[] = { "RSRP", "RSRQ", "RSSI", "SINR" };

    init_map();
    init_heatmap();

    std::string conn_str = "dbname=visual_db user=postgres password=postgres host=localhost port=5435";
    std::unique_ptr<pqxx::connection> db_conn = nullptr;
    try {
        db_conn = std::make_unique<pqxx::connection>(conn_str);
        if (db_conn->is_open()) {
            load_points_from_db(map_points, *db_conn);
            loc->db_connected = true;
        } else {
            loc->db_connected = false;
        }
    } catch (const std::exception& e) {
        loc->db_connected = false;
        std::cerr << "Failed to connect to database: " << e.what() << std::endl;
    }

    while (!glfwWindowShouldClose(window) && loc->running) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Server", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Location")) {
                std::lock_guard<std::mutex> lock(loc->mtx);
                ImGui::Text("Send ID: %d", loc->current_location.send_id);
                ImGui::Separator();
                ImGui::Text("Latitude: %.6f", loc->current_location.latitude);
                ImGui::Text("Longitude: %.6f", loc->current_location.longitude);
                ImGui::Text("Altitude: %.1f m", loc->current_location.altitude);
                ImGui::Text("Accuracy: %.2f m", loc->current_location.accuracy);
                ImGui::Text("Vertical Accuracy: %.2f m", loc->current_location.vertical_accuracy);
                ImGui::Text("Speed: %.2f m/s", loc->current_location.speed);
                ImGui::Text("Bearing: %.1f deg", loc->current_location.bearing);
                ImGui::Text("Time: %s", loc->current_location.time_formatted.c_str());
                ImGui::Separator();
                
                if (loc->recording) {
                    if (ImGui::Button("STOP SAVE JSON")) {
                        loc->recording = false;
                        std::string log_entry = "[System] Stop save Json. Saved " + std::to_string(loc->saved_count);
                        loc->message_log.push_back(log_entry);
                        if (loc->message_log.size() > 100) loc->message_log.pop_front();
                    }
                    ImGui::SameLine();
                    ImGui::Text("SAVE JSON");
                    ImGui::SameLine();
                    ImGui::Text("Saved: %d", loc->saved_count);
                } else {
                    if (ImGui::Button("START SAVE JSON")) {
                        loc->recording = true;
                        loc->saved_count = 0;
                        std::string log_entry = "[System] Start save Json. To file location_data.json";
                        loc->message_log.push_back(log_entry);
                        if (loc->message_log.size() > 100) loc->message_log.pop_front();
                    }
                }
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Mobile Networks")) {
                std::lock_guard<std::mutex> lock(loc->mtx);
                if (loc->mobile_networks.empty()) {
                    ImGui::Text("No Data");
                } else {
                    int active_count = 0;
                    for (const auto& net : loc->mobile_networks) {
                        if (net.is_active) active_count++;
                    }
                    ImGui::Text("Total cells: %d | Active cells: %d | Active PCI: %d", 
                                (int)loc->mobile_networks.size(), active_count, loc->active_pci);
                    ImGui::Separator();
                    for (size_t i = 0; i < loc->mobile_networks.size(); ++i) {
                        const auto& net = loc->mobile_networks[i];
                        if (net.is_active) {
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Cell %d (ACTIVE):", (int)(i+1));
                        } else {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Cell %d (Neighbor):", (int)(i+1));
                        }
                        ImGui::Text("  Type: %s", net.network_type.c_str());
                        ImGui::Text("  MCC: %s, MNC: %s", net.mcc.c_str(), net.mnc.c_str());
                        ImGui::Text("  Cell ID: %s", net.cell_identity.c_str());
                        ImGui::Text("  PCI: %d, TAC: %d", net.pci, net.tac);
                        ImGui::Text("  RSRP: %d dBm", net.rsrp);
                        ImGui::Text("  RSRQ: %d dB", net.rsrq);
                        ImGui::Text("  RSSI: %d dBm", net.rssi);
                        ImGui::Text("  SINR: %d dB", net.sinr);
                        ImGui::Text("  Signal: %s", net.signal_strength.c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Graphs")) {
                ImGui::Combo("Metric", &current_graph, graph_items, IM_ARRAYSIZE(graph_items));
                ImGui::SameLine();
                if (ImGui::Button("Clear History")) {
                    std::lock_guard<std::mutex> lock(loc->mtx);
                    for (auto& pair : loc->per_pci_data) {
                        PerPCIData& data = pair.second;
                        std::fill(data.rsrp_history.begin(), data.rsrp_history.end(), -120);
                        std::fill(data.rsrq_history.begin(), data.rsrq_history.end(), -20);
                        std::fill(data.rssi_history.begin(), data.rssi_history.end(), -120);
                        std::fill(data.sinr_history.begin(), data.sinr_history.end(), 0);
                    }
                }
                ImGui::Separator();

                ImVec2 graph_size = ImVec2(-1, 300);
                
                {
                    std::lock_guard<std::mutex> lock(loc->mtx);
                    
                    if (current_graph == 0) {
                        if (ImPlot::BeginPlot("RSRP - Active Cell Only", graph_size)) {
                            ImPlot::SetupAxes("Sample", "dBm");
                            ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                            if (loc->per_pci_data.find(loc->active_pci) != loc->per_pci_data.end()) {
                                PerPCIData& data = loc->per_pci_data[loc->active_pci];
                                std::string label = "PCI " + std::to_string(data.pci) + " (Active)";
                                ImPlot::PlotLine(label.c_str(), &data.rsrp_history[0], data.rsrp_history.size());
                            }
                            ImPlot::EndPlot();
                        }
                    } else if (current_graph == 1) {
                        if (ImPlot::BeginPlot("RSRQ - Active Cell Only", graph_size)) {
                            ImPlot::SetupAxes("Sample", "dB");
                            ImPlot::SetupAxisLimits(ImAxis_Y1, -34, -3);
                            if (loc->per_pci_data.find(loc->active_pci) != loc->per_pci_data.end()) {
                                PerPCIData& data = loc->per_pci_data[loc->active_pci];
                                std::string label = "PCI " + std::to_string(data.pci) + " (Active)";
                                ImPlot::PlotLine(label.c_str(), &data.rsrq_history[0], data.rsrq_history.size());
                            }
                            ImPlot::EndPlot();
                        }
                    } else if (current_graph == 2) {
                        if (ImPlot::BeginPlot("RSSI - Active Cell Only", graph_size)) {
                            ImPlot::SetupAxes("Sample", "dBm");
                            ImPlot::SetupAxisLimits(ImAxis_Y1, -120, -20);
                            if (loc->per_pci_data.find(loc->active_pci) != loc->per_pci_data.end()) {
                                PerPCIData& data = loc->per_pci_data[loc->active_pci];
                                std::string label = "PCI " + std::to_string(data.pci) + " (Active)";
                                ImPlot::PlotLine(label.c_str(), &data.rssi_history[0], data.rssi_history.size());
                            }
                            ImPlot::EndPlot();
                        }
                    } else if (current_graph == 3) {
                        if (ImPlot::BeginPlot("SINR - Active Cell Only", graph_size)) {
                            ImPlot::SetupAxes("Sample", "dB");
                            ImPlot::SetupAxisLimits(ImAxis_Y1, -10, 40);
                            if (loc->per_pci_data.find(loc->active_pci) != loc->per_pci_data.end()) {
                                PerPCIData& data = loc->per_pci_data[loc->active_pci];
                                std::string label = "PCI " + std::to_string(data.pci) + " (Active)";
                                ImPlot::PlotLine(label.c_str(), &data.sinr_history[0], data.sinr_history.size());
                            }
                            ImPlot::EndPlot();
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Active Cell Info:");
                std::lock_guard<std::mutex> lock(loc->mtx);
                if (loc->active_pci != 0 && loc->per_pci_data.find(loc->active_pci) != loc->per_pci_data.end()) {
                    PerPCIData& data = loc->per_pci_data[loc->active_pci];
                    ImVec4 color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                    ImGui::TextColored(color, "Currently displaying: PCI %d (Active Cell)", data.pci);
                    ImGui::Text("Last values - RSRP: %.0f dBm, RSRQ: %.0f dB, RSSI: %.0f dBm, SINR: %.0f dB",
                                data.rsrp_history.back(), data.rsrq_history.back(), 
                                data.rssi_history.back(), data.sinr_history.back());
                } else {
                    ImGui::Text("No active cell data available");
                }
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Log")) {
                std::lock_guard<std::mutex> lock(loc->mtx);
                ImGui::BeginChild("LogScroll", ImVec2(0, 400), true);
                for (const auto& log : loc->message_log) {
                    ImGui::Text("%s", log.c_str());
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Stats")) {
                std::lock_guard<std::mutex> lock(loc->mtx);
                ImGui::Text("Total messages: %d", loc->message_count);
                ImGui::Text("Last ID: %d", loc->last_send_id);
                ImGui::Text("Networks in cache: %zu", loc->mobile_networks.size());
                ImGui::Text("Messages in log: %zu", loc->message_log.size());
                ImGui::Text("Recording: %s", loc->recording ? "ON" : "OFF");
                ImGui::Text("Saved to JSON: %d", loc->saved_count);
                ImGui::Text("Active PCI: %d", loc->active_pci);
                ImGui::Text("Tracked PCIs: %zu", loc->per_pci_data.size());
                ImGui::Separator();
                if (loc->db_connected) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Database: Connected");
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Database: Disconnected");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Map")) {
                if (ImGui::Button("Center on Latest")) {
                    std::lock_guard<std::mutex> lock(loc->mtx);
                    if (loc->current_location.latitude != 0 || loc->current_location.longitude != 0) {
                        set_map_center(loc->current_location.latitude, loc->current_location.longitude, 15);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Zoom In")) {
                    set_map_center(get_map_center_lat(), get_map_center_lon(), get_map_zoom() + 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Zoom Out")) {
                    set_map_center(get_map_center_lat(), get_map_center_lon(), get_map_zoom() - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset View")) {
                    set_map_center(55.0084, 82.9357, 12);
                }
                ImGui::SameLine();
                if (ImGui::Button("Refresh Points")) {
                    if (db_conn && db_conn->is_open()) {
                        load_points_from_db(map_points, *db_conn);
                    }
                }
                
                ImGui::Separator();
                draw_map_ui();
                ImGui::Separator();
                
                float w = ImGui::GetContentRegionAvail().x;
                float h = w * 0.6f;
                if (h < 200) h = 200;
                
                ImGui::BeginChild("MapArea", ImVec2(w, h), ImGuiChildFlags_Borders);
                
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cp = ImGui::GetCursorScreenPos();
                
                ImGui::InvisibleButton("osm_map", ImVec2(w, h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
                
                draw_map(dl, cp, ImVec2(w, h));
                handle_map_input(cp, ImVec2(w, h));
                
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Heatmap")) {
                if (ImGui::Button("Refresh Heatmap")) {
                    if (db_conn && db_conn->is_open()) {
                        load_points_from_db(map_points, *db_conn);
                        update_heatmap_points(map_points);
                    }
                }
                ImGui::SameLine();
                ImGui::Text("Points: %zu", map_points.size());
                
                ImGui::Separator();

                draw_heatmap_ui();
                
                ImGui::Separator();

                float w = ImGui::GetContentRegionAvail().x;
                float h = w * 0.6f;
                if (h < 200) h = 200;
                
                ImGui::BeginChild("HeatmapArea", ImVec2(w, h), ImGuiChildFlags_Borders);
                
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cp = ImGui::GetCursorScreenPos();
                
                ImGui::InvisibleButton("heatmap_area", ImVec2(w, h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
                
                draw_heatmap(dl, cp, ImVec2(w, h));
                handle_heatmap_input(cp, ImVec2(w, h));
                
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}