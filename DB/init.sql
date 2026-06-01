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