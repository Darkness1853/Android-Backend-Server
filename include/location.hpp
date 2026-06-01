#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <atomic>
#include <map>

struct LocationData {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double accuracy = 0.0;
    double vertical_accuracy = 0.0;
    double bearing = 0.0;
    double speed = 0.0;
    long long time = 0;
    std::string time_formatted;
    int send_id = 0;
};

struct MobileNetworkData {
    std::string network_type;
    std::string mcc;
    std::string mnc;
    std::string cell_identity;
    int pci = 0;
    int tac = 0;
    int rsrp = 0;
    int rsrq = 0;
    int rssi = 0;
    int sinr = 0;
    std::string signal_strength;
    long long time = 0;
    bool is_active = false;
};

struct PerPCIData {
    int pci;
    std::deque<float> rsrp_history;
    std::deque<float> rsrq_history;
    std::deque<float> rssi_history;
    std::deque<float> sinr_history;
    
    PerPCIData() : pci(0) {
        rsrp_history.resize(100, -120);
        rsrq_history.resize(100, -20);
        rssi_history.resize(100, -120);
        sinr_history.resize(100, 0);
    }
    
    PerPCIData(int _pci) : pci(_pci) {
        rsrp_history.resize(100, -120);
        rsrq_history.resize(100, -20);
        rssi_history.resize(100, -120);
        sinr_history.resize(100, 0);
    }
};

struct Location {
    std::mutex mtx;
    LocationData current_location;
    std::deque<MobileNetworkData> mobile_networks;
    std::deque<std::string> message_log;
    std::map<int, PerPCIData> per_pci_data;
    int message_count = 0;
    int last_send_id = 0;
    bool running = true;
    std::atomic<bool> recording{false};
    int saved_count = 0;
    bool db_connected = false;
    int active_pci = 0;
};