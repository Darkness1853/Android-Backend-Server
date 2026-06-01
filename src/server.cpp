#include "server.hpp"
#include "db_client.hpp"
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>

using json = nlohmann::json;

static std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string time_str = std::ctime(&now_c);
    time_str.pop_back();
    return time_str;
}

static bool isActiveCell(const std::string& cell_identity, int pci, int rsrp) {
    if (cell_identity.empty() || cell_identity == "0" || cell_identity == "268435455") {
        return false;
    }
    if (pci == 0 || pci == 2147483647) {
        return false;
    }
    if (rsrp > 0 || rsrp < -150) {
        return false;
    }
    return true;
}

static int filterSignalValue(int value, int min_val, int max_val, int default_val) {
    if (value >= min_val && value <= max_val) {
        return value;
    }
    return default_val;
}

static void save_to_json(Location* loc, const json& mobileNetworkData, const json& locationData) {
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
        
        std::string filename = "location_data.json";
        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
            file << output.dump() << "\n";
            file.close();
            loc->saved_count++;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving to JSON: " << e.what() << std::endl;
    }
}

void run_server(Location* loc, int port) {
    std::string conninfo = "host=localhost port=5435 dbname=visual_db user=postgres password=postgres";
    auto db_client = std::make_unique<DBClient>(conninfo);
    loc->db_connected = db_client->isConnected();

    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::rep);
    
    socket.bind("tcp://*:" + std::to_string(port));
    
    auto last_db_check = std::chrono::steady_clock::now();

    while (loc->running) {
        zmq::pollitem_t items[] = { { socket, 0, ZMQ_POLLIN, 0 } };
        zmq::poll(items, 1, std::chrono::milliseconds(100));

        auto now = std::chrono::steady_clock::now();
        if (now - last_db_check >= std::chrono::seconds(30)) {
            last_db_check = now;
            if (db_client) db_client->checkAndReconnect();
            loc->db_connected = db_client->isConnected();
        }

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
                    
                    int best_rsrp = -200;
                    int active_pci = 0;
                    
                    for (const auto& net : root["mobile_networks"]["MobileNetworks"]) {
                        MobileNetworkData data;
                        data.network_type = net.value("NetworkType", "");
                        data.mcc = net.value("MCC", "");
                        data.mnc = net.value("MNC", "");
                        data.cell_identity = net.value("CellIdentity", "");
                        data.pci = net.value("PCI", 0);
                        data.tac = net.value("TAC", 0);
                        
                        int raw_rsrp = net.value("RSRP", 0);
                        int raw_rsrq = net.value("RSRQ", 0);
                        int raw_rssi = net.value("RSSI", 0);
                        int raw_sinr = net.value("SINR", 0);
                        
                        data.rsrp = filterSignalValue(raw_rsrp, -140, -44, -120);
                        data.rsrq = filterSignalValue(raw_rsrq, -34, -3, -20);
                        data.rssi = filterSignalValue(raw_rssi, -120, -20, -100);
                        data.sinr = filterSignalValue(raw_sinr, -10, 40, 0);
                        
                        data.signal_strength = net.value("SignalStrength", "");
                        data.time = net.value("Time", 0LL);
                        data.is_active = isActiveCell(data.cell_identity, data.pci, data.rsrp);
                        
                        if (data.is_active && data.rsrp > best_rsrp) {
                            best_rsrp = data.rsrp;
                            active_pci = data.pci;
                        }
                        
                        loc->mobile_networks.push_back(data);
                    }
                    
                    loc->active_pci = active_pci;
                    
                    for (const auto& net : loc->mobile_networks) {
                        if (!net.is_active) continue;
                        
                        int pci = net.pci;
                        int rsrp = net.rsrp;
                        int rsrq = net.rsrq;
                        int rssi = net.rssi;
                        int sinr = net.sinr;
                        
                        PerPCIData& pci_data = loc->per_pci_data[pci];
                        pci_data.pci = pci;
                        
                        for (int i = 0; i < 99; ++i) {
                            pci_data.rsrp_history[i] = pci_data.rsrp_history[i+1];
                            pci_data.rsrq_history[i] = pci_data.rsrq_history[i+1];
                            pci_data.rssi_history[i] = pci_data.rssi_history[i+1];
                            pci_data.sinr_history[i] = pci_data.sinr_history[i+1];
                        }
                        pci_data.rsrp_history[99] = rsrp;
                        pci_data.rsrq_history[99] = rsrq;
                        pci_data.rssi_history[99] = rssi;
                        pci_data.sinr_history[99] = sinr;
                    }
                }

                if (loc->recording && !location_data.empty() && !mobile_data.empty()) {
                    save_to_json(loc, mobile_data, location_data);
                }

                if (db_client && db_client->isConnected()) {
                    db_client->saveJsonData(root);
                }

                loc->message_count++;

                std::string log_entry = "[" + getCurrentTime() + "] Received #" + std::to_string(loc->last_send_id)
                                      + " | Lat: " + std::to_string(loc->current_location.latitude)
                                      + " | Lon: " + std::to_string(loc->current_location.longitude)
                                      + " | Active PCI: " + std::to_string(loc->active_pci);
                if (loc->recording) {
                    log_entry += " [SAVED]";
                }
                if (db_client && db_client->isConnected()) {
                    log_entry += " [DB]";
                }
                log_entry.erase(std::remove(log_entry.begin(), log_entry.end(), '\n'), log_entry.end());
                loc->message_log.push_back(log_entry);
                if (loc->message_log.size() > 100) loc->message_log.pop_front();

                json response = {{"status", "ok"}, {"count", loc->message_count}, {"message", "Data received"}};
                std::string resp_str = response.dump();
                socket.send(zmq::buffer(resp_str), zmq::send_flags::none);

            } catch (const json::parse_error&) {
                socket.send(zmq::buffer("ERROR"), zmq::send_flags::none);
            } catch (const std::exception& e) {
                std::string error_msg = "ERROR: " + std::string(e.what());
                socket.send(zmq::buffer(error_msg), zmq::send_flags::none);
            }
        }
    }
}