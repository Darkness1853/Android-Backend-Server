#include "db_client.hpp"
#include <thread>
#include <chrono>

DBClient::DBClient(const std::string& conninfo) 
    : conn(nullptr), connected(false), conninfo(conninfo)
{
    connectWithRetries();
}

DBClient::~DBClient() {
    if (conn) PQfinish(conn);
}

bool DBClient::isConnected() const {
    return connected;
}

bool DBClient::connectWithRetries() {
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        if (conn) {
            PQfinish(conn);
            conn = nullptr;
        }
        conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) == CONNECTION_OK) {
            connected = true;
            initializeSchema();
            return true;
        } else {
            connected = false;
            PQfinish(conn);
            conn = nullptr;
            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
            }
        }
    }
    return false;
}

void DBClient::checkAndReconnect() {
    if (connected) {
        PGresult* res = PQexec(conn, "SELECT 1");
        if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
            PQclear(res);
            return;
        }
        if (res) PQclear(res);
        connected = false;
    }
    if (!connected) {
        connectWithRetries();
    }
}

void DBClient::initializeSchema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS location (
            id SERIAL PRIMARY KEY,
            timestamp BIGINT,
            send_id INTEGER
        );
        CREATE TABLE IF NOT EXISTS locations (
            id SERIAL PRIMARY KEY,
            location_id INTEGER REFERENCES location(id) ON DELETE CASCADE,
            latitude DOUBLE PRECISION,
            longitude DOUBLE PRECISION,
            altitude DOUBLE PRECISION,
            accuracy DOUBLE PRECISION,
            vertical_accuracy DOUBLE PRECISION,
            bearing DOUBLE PRECISION,
            speed DOUBLE PRECISION,
            time BIGINT,
            time_formatted TEXT
        );
        CREATE TABLE IF NOT EXISTS cells (
            id SERIAL PRIMARY KEY,
            location_id INTEGER REFERENCES location(id) ON DELETE CASCADE,
            network_type TEXT,
            mcc TEXT,
            mnc TEXT,
            cell_identity TEXT,
            pci INTEGER,
            tac INTEGER,
            rsrp INTEGER,
            rsrq INTEGER,
            rssi INTEGER,
            signal_strength TEXT,
            time BIGINT
        );
    )";
    PGresult* res = PQexec(conn, schema);
    PQclear(res);
}

long long DBClient::insertLocationRecord(long long timestamp, int send_id) {
    const char* query = "INSERT INTO location (timestamp, send_id) VALUES ($1, $2) RETURNING id";
    const char* params[2];
    std::string ts_str = std::to_string(timestamp);
    std::string sid_str = std::to_string(send_id);
    params[0] = ts_str.c_str();
    params[1] = sid_str.c_str();

    PGresult* res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    long long location_id = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        location_id = atoll(PQgetvalue(res, 0, 0));
    }
    PQclear(res);
    return location_id;
}

void DBClient::saveLocationData(long long location_id, const LocationData& loc) {
    const char* query = "INSERT INTO locations (location_id, latitude, longitude, altitude, accuracy, vertical_accuracy, bearing, speed, time, time_formatted) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)";
    const char* params[10];
    std::string loc_id_str = std::to_string(location_id);
    std::string lat_str = std::to_string(loc.latitude);
    std::string lon_str = std::to_string(loc.longitude);
    std::string alt_str = std::to_string(loc.altitude);
    std::string acc_str = std::to_string(loc.accuracy);
    std::string vert_acc_str = std::to_string(loc.vertical_accuracy);
    std::string bearing_str = std::to_string(loc.bearing);
    std::string speed_str = std::to_string(loc.speed);
    std::string time_str = std::to_string(loc.time);

    params[0] = loc_id_str.c_str();
    params[1] = lat_str.c_str();
    params[2] = lon_str.c_str();
    params[3] = alt_str.c_str();
    params[4] = acc_str.c_str();
    params[5] = vert_acc_str.c_str();
    params[6] = bearing_str.c_str();
    params[7] = speed_str.c_str();
    params[8] = time_str.c_str();
    params[9] = loc.time_formatted.c_str();

    PGresult* res = PQexecParams(conn, query, 10, NULL, params, NULL, NULL, 0);
    PQclear(res);
}

void DBClient::saveCellData(long long location_id, const MobileNetworkData& cell) {
    const char* query = "INSERT INTO cells (location_id, network_type, mcc, mnc, cell_identity, pci, tac, rsrp, rsrq, rssi, signal_strength, time) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
    const char* params[12];
    std::string loc_id_str = std::to_string(location_id);
    std::string pci_str = std::to_string(cell.pci);
    std::string tac_str = std::to_string(cell.tac);
    std::string rsrp_str = std::to_string(cell.rsrp);
    std::string rsrq_str = std::to_string(cell.rsrq);
    std::string rssi_str = std::to_string(cell.rssi);
    std::string time_str = std::to_string(cell.time);

    params[0] = loc_id_str.c_str();
    params[1] = cell.network_type.c_str();
    params[2] = cell.mcc.c_str();
    params[3] = cell.mnc.c_str();
    params[4] = cell.cell_identity.c_str();
    params[5] = pci_str.c_str();
    params[6] = tac_str.c_str();
    params[7] = rsrp_str.c_str(); 
    params[8] = rsrq_str.c_str();
    params[9] = rssi_str.c_str();
    params[10] = cell.signal_strength.c_str();
    params[11] = time_str.c_str();

    PGresult* res = PQexecParams(conn, query, 12, NULL, params, NULL, NULL, 0);
    PQclear(res);
}

void DBClient::saveJsonData(const json& root) {
    checkAndReconnect();
    if (!connected) return;

    long long timestamp = root.value("timestamp", 0LL);
    int send_id = root.value("send_id", 0);

    long long location_id = insertLocationRecord(timestamp, send_id);
    if (location_id == -1) return;

    if (root.contains("location")) {
        const auto& loc = root["location"];
        LocationData ld;
        ld.latitude = loc.value("latitude", 0.0);
        ld.longitude = loc.value("longitude", 0.0);
        ld.altitude = loc.value("altitude", 0.0);
        ld.accuracy = loc.value("accuracy", 0.0);
        ld.vertical_accuracy = loc.value("vertical_accuracy", 0.0);
        ld.bearing = loc.value("bearing", 0.0);
        ld.speed = loc.value("speed", 0.0);
        ld.time = loc.value("time", 0LL);
        ld.time_formatted = loc.value("time_formatted", "");
        saveLocationData(location_id, ld);
    }

    if (root.contains("mobile_networks") && root["mobile_networks"].contains("MobileNetworks")) {
        const auto& networks = root["mobile_networks"]["MobileNetworks"];
        for (const auto& net : networks) {
            MobileNetworkData cell;
            cell.network_type = net.value("NetworkType", "");
            cell.mcc = net.value("MCC", "");
            cell.mnc = net.value("MNC", "");
            cell.cell_identity = net.value("CellIdentity", "");
            cell.pci = net.value("PCI", 0);
            cell.tac = net.value("TAC", 0);
            cell.rsrp = net.value("RSRP", 0);
            cell.rsrq = net.value("RSRQ", 0);
            cell.rssi = net.value("RSSI", 0);
            cell.signal_strength = net.value("SignalStrength", "");
            cell.time = net.value("Time", 0LL);
            saveCellData(location_id, cell);
        }
    }
}