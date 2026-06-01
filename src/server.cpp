#include "server.hpp"
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

using json = nlohmann::json;

static std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string time_str = std::ctime(&now_c);
    time_str.pop_back();
    return time_str;
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
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::rep);
    
    socket.bind("tcp://*:" + std::to_string(port));
    
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