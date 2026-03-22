#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <vector>
#include <deque>
#include <cmath>
#include <set>
#include <numeric>
#include "imgui.h"
#include "implot.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

using namespace std;
using json = nlohmann::json;

struct CellData {
    int lteCount = 0;
    int gsmCount = 0;
    int nrCount = 0;
    string lastLteInfo;
    string lastGsmInfo;
    string lastNrInfo;
    json fullCellData;
    json lastRawData;
    
    vector<int> rsrpValues;
    vector<int> rsrqValues;
    vector<int> rssiValues;
    vector<double> timestamps;
    vector<int> pciValues;
    vector<int> cellIds;
    
    deque<double> rsrpHistory;
    deque<double> rsrqHistory;
    deque<double> timeHistory;
    int maxHistorySize = 100;
};

struct location
{
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    string timeStr;
    int count = 0;
    CellData cellData;
};

atomic<bool> run{true};

void save_to_json(const location& loc, const json& mobileNetworkData, const json& locationData) {
    try {
        json j;
        j["timestamp"] = locationData.value("Time", 0LL);
        j["timestamp_formatted"] = locationData.value("TimeFormatted", "");
        j["send_id"] = locationData.value("send_id", 0);
        
        j["location"] = {
            {"latitude", loc.latitude},
            {"longitude", loc.longitude},
            {"altitude", loc.altitude},
            {"speed", locationData.value("Speed", 0.0)},
            {"bearing", locationData.value("Bearing", 0.0)},
            {"accuracy", locationData.value("Accuracy", 0.0)},
            {"vertical_accuracy", locationData.value("VerticalAccuracy", 0.0)},
            {"time", locationData.value("Time", 0LL)},
            {"time_formatted", locationData.value("TimeFormatted", "")}
        };
        
        j["mobile_networks"] = mobileNetworkData;
        
        string filename = "location_data.json";
        ofstream file(filename, ios::app);
        if (file.is_open()) {
            file << j.dump(4) << ",\n";
            file.close();
        }
    } catch (const exception& e) {
        cerr << "Ошибка сохранения в JSON: " << e.what() << endl;
    }
}

void upd_CellData_History(location* loc, const json& networks) {
    if (!networks.is_array()) return;
    
    double currentTime;
    if (loc->cellData.timestamps.empty()) {
        currentTime = 0;
    } else {
    currentTime = loc->cellData.timestamps.back() + 1;  
    }
    
    loc->cellData.timestamps.push_back(currentTime);
    loc->cellData.timeHistory.push_back(currentTime);
    if (loc->cellData.timeHistory.size() > loc->cellData.maxHistorySize) {
        loc->cellData.timeHistory.pop_front();
    }
    
    int bestRSRP = -1000;
    int bestRSRQ = -1000;
    int bestRSSI = -1000;
    int bestPCI = 0;
    
    for (const auto& network : networks) {
        string netType = network.value("NetworkType", "");
        
        if (netType.find("Lte") != string::npos) {
            int rsrp = network.value("RSRP",-1000);
            int rsrq = network.value("RSRQ",-1000);
            int rssi = network.value("RSSI",-1000);
            int pci = network.value("PCI",0);
            
            if (rsrp > bestRSRP) {
                bestRSRP = rsrp;
                bestRSRQ = rsrq;
                bestRSSI = rssi;
                bestPCI = pci;
            }
            
            loc->cellData.rsrpValues.push_back(rsrp);
            loc->cellData.rsrqValues.push_back(rsrq);
            loc->cellData.rssiValues.push_back(rssi);
            loc->cellData.pciValues.push_back(pci);
            
            if (network.contains("CellIdentity")) {
                string cellId = network.value("CellIdentity", "0");
                try {
                    loc->cellData.cellIds.push_back(stoi(cellId));
                } catch (...) {
                    loc->cellData.cellIds.push_back(0);
                }
            }
        }
    }
    
    if (bestRSRP > -1000) {
        loc->cellData.rsrpHistory.push_back(bestRSRP);
        loc->cellData.rsrqHistory.push_back(bestRSRQ);
        
        if (loc->cellData.rsrpHistory.size() > loc->cellData.maxHistorySize) {
            loc->cellData.rsrpHistory.pop_front();
            loc->cellData.rsrqHistory.pop_front();
        }
    }
}

void run_gui(location *loc){
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    int screenWidth = DM.w;
    int screenHeight = DM.h;
    
    SDL_Window* window = SDL_CreateWindow(
        "Server", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED,
        screenWidth, 
        screenHeight, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.3f;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                run = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                run = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        {   
            ImGui::StyleColorsDark();
            
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Network");
            
            if (ImGui::BeginTable("MainTable", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Location Info", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                ImGui::TableSetupColumn("Network Data", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableSetupColumn("Graphs", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableHeadersRow();
                
                ImGui::TableNextColumn();
                ImGui::BeginChild("LocationPane", ImVec2(0, 0), true);
                
                ImGui::Text("LOCATION INFORMATION");
                ImGui::Separator();
                ImGui::Text("Messages: %d", loc->count);
                ImGui::Separator();
                ImGui::Text("Lat: %.6f", loc->latitude);
                ImGui::Text("Lon: %.6f", loc->longitude);
                ImGui::Text("Alt: %.2f m", loc->altitude);
                ImGui::Text("Time: %s", loc->timeStr.c_str());
                
                ImGui::Separator();
                ImGui::Text("CELL SUMMARY");
                ImGui::Separator();
                ImGui::Text("LTE: %d", loc->cellData.lteCount);
                ImGui::Text("GSM: %d", loc->cellData.gsmCount);
                ImGui::Text("5G: %d", loc->cellData.nrCount);
                
                if (!loc->cellData.lastLteInfo.empty()) {
                    ImGui::Separator();
                    ImGui::Text ("Last LTE:");
                    ImGui::TextWrapped("%s", loc->cellData.lastLteInfo.c_str());
                }
                
                ImGui::EndChild();
                
                ImGui::TableNextColumn();
                ImGui::BeginChild("NetworkPane", ImVec2(0, 0), true);
                ImGui::Text ("NETWORK DATA");
                ImGui::Separator();
                
                if (loc->cellData.fullCellData.is_array() && !loc->cellData.fullCellData.empty()) {
                    for (size_t i = 0; i < loc->cellData.fullCellData.size(); i++) {
                        const auto& cell = loc->cellData.fullCellData[i];
                        
                        string cellType = cell.value("NetworkType", "Unknown");
                        ImGui::PushID(static_cast<int>(i));
                        
                        ImGui::Text ("%s Cell #%d", 
                            cellType.c_str(), static_cast<int>(i+1));
                        
                        if (ImGui::TreeNode(("Details##" + to_string(i)).c_str())) {
                            for (auto& [key, value] : cell.items()) {
                                if (value.is_string()) {
                                    ImGui::Text("%s: %s", key.c_str(), value.get<string>().c_str());
                                } else if (value.is_number()) {
                                    ImGui::Text("%s: %g", key.c_str(), value.get<double>());
                                }
                            }
                            
                            if (cell.contains("RSRP") && cell.contains("RSRQ")) {
                                int rsrp = cell.value("RSRP", 0);
                                int rsrq = cell.value("RSRQ", 0);
                                ImGui::Text("RSRP: %d dBm", rsrp);
                                ImGui::Text("RSRQ: %d dB", rsrq);
                            }
                            
                            ImGui::TreePop();
                        }
                        
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                } else {
                    ImGui::Text ("No network data");
                }
                
                ImGui::EndChild();
                
                ImGui::TableNextColumn();
                ImGui::BeginChild("GraphsPane", ImVec2(0, 0), true);
                ImGui::Text ("SIGNAL GRAPHS");
                ImGui::Separator();
                
                if (ImPlot::BeginPlot("Signal Strength History", ImVec2(-1, 250))) {
                    ImPlot::SetupAxes("Time", "dBm");
                    ImPlot::SetupAxisLimits(ImAxis_X1, 
                        loc->cellData.timeHistory.empty() ? 0 : loc->cellData.timeHistory.front(),
                        loc->cellData.timeHistory.empty() ? 10 : loc->cellData.timeHistory.back() + 1);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                    
                    if (!loc->cellData.timeHistory.empty() && !loc->cellData.rsrpHistory.empty()) {
                        vector<double> timeVec(loc->cellData.timeHistory.begin(), 
                                              loc->cellData.timeHistory.end());
                        vector<double> rsrpVec(loc->cellData.rsrpHistory.begin(), 
                                              loc->cellData.rsrpHistory.end());
                        
                        ImPlot::PlotLine("RSRP (dBm)", 
                            timeVec.data(), rsrpVec.data(), 
                            static_cast<int>(min(timeVec.size(), rsrpVec.size())));
                    }
                    
                    ImPlot::EndPlot();
                }
                
                if (ImPlot::BeginPlot("RSRQ History", ImVec2(-1, 250))) {
                    ImPlot::SetupAxes("Time", "dB");
                    ImPlot::SetupAxisLimits(ImAxis_X1, 
                        loc->cellData.timeHistory.empty() ? 0 : loc->cellData.timeHistory.front(),
                        loc->cellData.timeHistory.empty() ? 10 : loc->cellData.timeHistory.back() + 1);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, -25, -5);
                    
                    if (!loc->cellData.timeHistory.empty() && !loc->cellData.rsrqHistory.empty()) {
                        vector<double> timeVec(loc->cellData.timeHistory.begin(), 
                                              loc->cellData.timeHistory.end());
                        vector<double> rsrqVec(loc->cellData.rsrqHistory.begin(), 
                                              loc->cellData.rsrqHistory.end());
                        
                        ImPlot::PlotLine("RSRQ (dB)", 
                            timeVec.data(), rsrqVec.data(), 
                            static_cast<int>(min(timeVec.size(), rsrqVec.size())));
                    }
                    
                    ImPlot::EndPlot();
                }
                
                ImGui::Separator();
                ImGui::Text ("STATISTICS");
                ImGui::Separator();
                
                if (!loc->cellData.rsrpValues.empty()) {
                    int minRSRP = *min_element(loc->cellData.rsrpValues.begin(), 
                                               loc->cellData.rsrpValues.end());
                    int maxRSRP = *max_element(loc->cellData.rsrpValues.begin(), 
                                               loc->cellData.rsrpValues.end());
                    double avgRSRP = accumulate(loc->cellData.rsrpValues.begin(), 
                                                loc->cellData.rsrpValues.end(), 0.0) / 
                                                loc->cellData.rsrpValues.size();
                    
                    ImGui::Text("RSRP Range: %d to %d dBm", minRSRP, maxRSRP);
                    ImGui::Text("Average RSRP: %.1f dBm", avgRSRP);
                    
                    if (!loc->cellData.pciValues.empty()) {
                        set<int> uniquePCIs(loc->cellData.pciValues.begin(), 
                                           loc->cellData.pciValues.end());
                        ImGui::Text("Unique PCI count: %zu", uniquePCIs.size());
                    }
                }
                
                ImGui::EndChild();
                
                ImGui::EndTable();
            }
            
            ImGui::End();
        }

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void run_server(location *loc){
    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::rep);
    
    sock.bind("tcp://*:5050");
    
    int count = 0;
    
    while (run) {
        zmq::message_t request;
        
        if (sock.recv(request, zmq::recv_flags::none)) {
            string received_data(static_cast<char*>(request.data()), request.size());
            
            try {
                json j = json::parse(received_data);
                
                if (j.value("type", "") == "ping_client") {
                    sock.send(zmq::buffer(R"({"type":"ping_server"})"), zmq::send_flags::none);
                    continue;
                }
                
                count++;
                
                if (j.contains("location_data")) {
                    json locationData = j["location_data"];
                    
                    loc->latitude = locationData.value("Latitude", 0.0);
                    loc->longitude = locationData.value("Longitude", 0.0);
                    loc->altitude = locationData.value("Altitude", 0.0);
                    loc->timeStr = locationData.value("TimeFormatted", "");
                    loc->count = count;
                }
                
                loc->cellData.lteCount = 0;
                loc->cellData.gsmCount = 0;
                loc->cellData.nrCount = 0;
                loc->cellData.lastRawData = j;
                
                if (j.contains("mobile_network_data_list") && 
                    j["mobile_network_data_list"].contains("MobileNetworks")) {
                    
                    auto& networks = j["mobile_network_data_list"]["MobileNetworks"];
                    loc->cellData.fullCellData = networks;
                    
                    upd_CellData_History(loc, networks);
                    
                    for (const auto& network : networks) {
                        string netType = network.value("NetworkType", "");
                        
                        if (netType.find("Lte") != string::npos) {
                            loc->cellData.lteCount++;
                            if (loc->cellData.lteCount == 1) {
                                int rsrp = network.value("RSRP", 0);
                                int pci = network.value("PCI", 0);
                                int tac = network.value("TAC", 0);
                                int rsrq = network.value("RSRQ", 0);
                                loc->cellData.lastLteInfo = "PCI: " + to_string(pci) + " | TAC: " + to_string(tac) +" | RSRP: " + to_string(rsrp) + " dBm" + " | RSRQ: " + to_string(rsrq) + " dB";
                            }
                        }
                        else if (netType.find("Gsm") != string::npos) {
                            loc->cellData.gsmCount++;
                        }
                        else if (netType.find("Nr") != string::npos) {
                            loc->cellData.nrCount++;
                        }
                    }
                }
                
                json locationData = j.value("location_data", json::object());
                json mobileNetworkData = j.value("mobile_network_data_list", json::object());
                save_to_json(*loc, mobileNetworkData, locationData);
                
                json response;
                response["status"] = "ok";
                response["count"] = count;
                response["received_at"] = time(nullptr);
                sock.send(zmq::buffer(response.dump()), zmq::send_flags::none);
                
            } catch (const exception& e) {
                json error_response;
                error_response["status"] = "error";
                error_response["message"] = e.what();
                sock.send(zmq::buffer(error_response.dump()), zmq::send_flags::none);
            }
        } 
    }
}

int main() {
    location locationInfo;
    
    thread gui_thread(run_gui, &locationInfo);
    thread server_thread(run_server, &locationInfo);

    gui_thread.join();
    server_thread.join();
    
    return 0;
}