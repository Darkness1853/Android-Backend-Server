#include "gui.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <vector>
#include <algorithm>

void run_gui(Location* loc) {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 700, "Location Server", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewInit();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::vector<float> rsrp_history(100, 0);
    std::vector<float> rsrq_history(100, 0);

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
                    ImGui::Text("Networks: %d", (int)loc->mobile_networks.size());
                    ImGui::Separator();
                    for (size_t i = 0; i < loc->mobile_networks.size(); ++i) {
                        const auto& net = loc->mobile_networks[i];
                        ImGui::Text("Cell %d:", (int)(i+1));
                        ImGui::Text("  Type: %s", net.network_type.c_str());
                        ImGui::Text("  MCC: %s, MNC: %s", net.mcc.c_str(), net.mnc.c_str());
                        ImGui::Text("  Cell ID: %s", net.cell_identity.c_str());
                        ImGui::Text("  PCI: %d, TAC: %d", net.pci, net.tac);
                        ImGui::Text("  RSRP: %d dBm", net.rsrp);
                        ImGui::Text("  RSRQ: %d dB", net.rsrq);
                        ImGui::Text("  RSSI: %d", net.rssi);
                        ImGui::Text("  Signal: %s", net.signal_strength.c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Graphs")) {
                ImGui::Text("RSRP History (dBm)");
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    std::fill(rsrp_history.begin(), rsrp_history.end(), 0);
                    std::fill(rsrq_history.begin(), rsrq_history.end(), 0);
                } 
                ImGui::Separator();

                {
                    std::lock_guard<std::mutex> lock(loc->mtx);
                    if (!loc->mobile_networks.empty()) {
                        float avg_rsrp = 0, avg_rsrq = 0;
                        for (const auto& net : loc->mobile_networks) {
                            avg_rsrp += net.rsrp;
                            avg_rsrq += net.rsrq;
                        }
                        avg_rsrp /= loc->mobile_networks.size();
                        avg_rsrq /= loc->mobile_networks.size();

                        for (int i = 0; i < 99; ++i) {
                            rsrp_history[i] = rsrp_history[i+1];
                            rsrq_history[i] = rsrq_history[i+1];
                        }
                        rsrp_history[99] = avg_rsrp;
                        rsrq_history[99] = avg_rsrq;
                    }
                }
                if (ImPlot::BeginPlot("RSRP", ImVec2(-1, 250))) {
                    ImPlot::SetupAxes("Sample", "dBm");
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                    ImPlot::PlotLine("RSRP", rsrp_history.data(), rsrp_history.size());
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("RSRQ", ImVec2(-1, 250))) {
                    ImPlot::SetupAxes("Sample", "dB");
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -34, -3);
                    ImPlot::PlotLine("RSRQ", rsrq_history.data(), rsrq_history.size());
                    ImPlot::EndPlot();
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
                ImGui::Text("Saved count: %d", loc->saved_count);
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