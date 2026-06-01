#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <atomic>
#include <zmq.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

struct LocationData {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double accuracy = 0.0;
    double vertical_accuracy = 0.0;
    double bearing = 0.0;
    double speed = 0.0;
    long long time = 0;
    string time_formatted;
    int send_id = 0;
};

struct MobileNetworkData {
    string network_type;
    string mcc;
    string mnc;
    string cell_identity;
    int pci = 0;
    int tac = 0;
    int rsrp = 0;
    int rsrq = 0;
    int rssi = 0;
    string signal_strength;
    long long time = 0;
};

struct Location {
    mutex mtx;
    LocationData current_location;
    deque<MobileNetworkData> mobile_networks;
    deque<string> message_log;
    int message_count = 0;
    int last_send_id = 0;
    bool running = true;
    atomic<bool> recording{false};
    int saved_count = 0;
};

void save_to_json(Location* loc, const json& mobileNetworkData, const json& locationData) {
    try {
        json output;
        
        output["location"] = {
            {"latitude", locationData.value("latitude", 0.0)},
            {"longitude", locationData.value("longitude", 0.0)},
            {"altitude", locationData.value("altitude", 0.0)},
            {"speed", locationData.value("speed", 0.0)},
            {"bearing", locationData.value("bearing", 0.0)},
            {"accuracy", locationData.value("accuracy", 0.0)},
            {"vertical_accuracy", locationData.value("vertical_accuracy", 0.0)}
        };
        
        json mobile_network;
        json networks = json::array();
        
        if (mobileNetworkData.contains("MobileNetworks")) {
            networks = mobileNetworkData["MobileNetworks"];
        }
        
        mobile_network["MobileNetworks"] = networks;
        output["mobile_network"] = mobile_network;
        
        string filename = "location_data.json";
        ofstream file(filename, ios::app);
        if (file.is_open()) {
            file << output.dump() << "\n";
            file.close();
            loc->saved_count++;
        }
        
    } catch (const exception& e) {
        cerr << "Error saving to JSON: " << e.what() << endl;
    }
}

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

    vector<float> rsrp_history(100, 0);
    vector<float> rsrq_history(100, 0);

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
                lock_guard<mutex> lock(loc->mtx);
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
                        string log_entry = "[System] Stop save Json. Saved " + to_string(loc->saved_count);
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
                        string log_entry = "[System] Start save Json. To file location_data.json";
                        loc->message_log.push_back(log_entry);
                        if (loc->message_log.size() > 100) loc->message_log.pop_front();
                    }
                }
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Mobile Networks")) {
                lock_guard<mutex> lock(loc->mtx);
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
                    fill(rsrp_history.begin(), rsrp_history.end(), 0);
                    fill(rsrq_history.begin(), rsrq_history.end(), 0);
                } 
                ImGui::Separator();

                {
                    lock_guard<mutex> lock(loc->mtx);
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
                lock_guard<mutex> lock(loc->mtx);
                ImGui::BeginChild("LogScroll", ImVec2(0, 400), true);
                for (const auto& log : loc->message_log) {
                    ImGui::Text("%s", log.c_str());
                }
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

void run_server(Location* loc, int port) {
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::rep);
    
    socket.bind("tcp://*:" + std::to_string(port));
    
    auto getCurrentTime = []() -> string {
        auto now = chrono::system_clock::now();
        auto now_c = chrono::system_clock::to_time_t(now);
        string time_str = ctime(&now_c);
        time_str.pop_back();
        return time_str;
    };
    
    while (loc->running) {
        zmq::pollitem_t items[] = { { socket, 0, ZMQ_POLLIN, 0 } };
        zmq::poll(items, 1, std::chrono::milliseconds(100));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t request;
            auto recv_result = socket.recv(request, zmq::recv_flags::none);
            if (!recv_result.has_value()) continue;

            std::string json_str(static_cast<char*>(request.data()), request.size());

            try {
                json root = json::parse(json_str);

                std::lock_guard<std::mutex> lock(loc->mtx);

                if (root.contains("type") && root["type"] == "ping_client") {
                    json response = {{"status", "ok"}, {"type", "ping_server"}, {"message", "Pong"}};
                    std::string resp_str = response.dump();
                    socket.send(zmq::buffer(resp_str), zmq::send_flags::none);
                    continue;
                }

                if (root.contains("send_id")) {
                    loc->last_send_id = root["send_id"].get<int>();
                    loc->current_location.send_id = loc->last_send_id;
                }

                json location_data;
                json mobile_data;

                if (root.contains("location")) {
                    const auto& jloc = root["location"];
                    location_data = jloc;
                    loc->current_location.latitude = jloc.value("latitude", 0.0);
                    loc->current_location.longitude = jloc.value("longitude", 0.0);
                    loc->current_location.altitude = jloc.value("altitude", 0.0);
                    loc->current_location.accuracy = jloc.value("accuracy", 0.0);
                    loc->current_location.vertical_accuracy = jloc.value("vertical_accuracy", 0.0);
                    loc->current_location.bearing = jloc.value("bearing", 0.0);
                    loc->current_location.speed = jloc.value("speed", 0.0);
                    loc->current_location.time = jloc.value("time", 0LL);
                    loc->current_location.time_formatted = jloc.value("time_formatted", "");
                }

                if (root.contains("mobile_networks") && root["mobile_networks"].contains("MobileNetworks")) {
                    mobile_data = root["mobile_networks"];
                    loc->mobile_networks.clear();
                    for (const auto& net : root["mobile_networks"]["MobileNetworks"]) {
                        MobileNetworkData data;
                        data.network_type = net.value("NetworkType", "");
                        data.mcc = net.value("MCC", "");
                        data.mnc = net.value("MNC", "");
                        data.cell_identity = net.value("CellIdentity", "");
                        data.pci = net.value("PCI", 0);
                        data.tac = net.value("TAC", 0);
                        data.rsrp = net.value("RSRP", 0);
                        data.rsrq = net.value("RSRQ", 0);
                        data.rssi = net.value("RSSI", 0);
                        data.signal_strength = net.value("SignalStrength", "");
                        data.time = net.value("Time", 0LL);
                        loc->mobile_networks.push_back(data);
                    }
                }

                if (loc->recording && !location_data.empty() && !mobile_data.empty()) {
                    save_to_json(loc, mobile_data, location_data);
                }

                loc->message_count++;

                std::string log_entry = "[" + getCurrentTime() + "] Received #" + std::to_string(loc->last_send_id)
                                      + " | Lat: " + std::to_string(loc->current_location.latitude)
                                      + " | Lon: " + std::to_string(loc->current_location.longitude);
                if (loc->recording) {
                    log_entry += " [SAVED]";
                }
                log_entry.erase(remove(log_entry.begin(), log_entry.end(), '\n'), log_entry.end());
                loc->message_log.push_back(log_entry);
                if (loc->message_log.size() > 100) loc->message_log.pop_front();

                json response = {{"status", "ok"}, {"count", loc->message_count}, {"message", "Data received"}};
                std::string resp_str = response.dump();
                socket.send(zmq::buffer(resp_str), zmq::send_flags::none);

            } catch (const json::parse_error& e) {
                std::string error_msg = "ERROR: JSON parse error";
                socket.send(zmq::buffer(error_msg), zmq::send_flags::none);
            } catch (const std::exception& e) {
                std::string error_msg = "ERROR: " + std::string(e.what());
                socket.send(zmq::buffer(error_msg), zmq::send_flags::none);
            }
        }
    }
}

int main() {
    Location locationInfo;
    
    thread gui_thread(run_gui, &locationInfo);
    thread server_thread(run_server, &locationInfo, 5050);

    gui_thread.join();
    server_thread.join();
    
    return 0;
}