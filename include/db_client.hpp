#pragma once
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <string>
#include "location.hpp"

using json = nlohmann::json;

class DBClient {
public:
    DBClient(const std::string& conninfo);
    ~DBClient();
    bool isConnected() const;
    void checkAndReconnect();
    void saveJsonData(const json& root);

private:
    PGconn* conn;
    bool connected;
    std::string conninfo;
    int max_retries = 10;
    int retry_delay_ms = 2000;
    bool connectWithRetries();
    void initializeSchema();
    long long insertLocationRecord(long long timestamp, int send_id);
    void saveLocationData(long long location_id, const LocationData& loc);
    void saveCellData(long long location_id, const MobileNetworkData& cell);
};