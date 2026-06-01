#pragma once

#include <string>

struct MapPoint {
    double lat;
    double lon;
    long long timestamp;
    int signal_strength;
    int earfcn;
    int pci;
    std::string type;

    MapPoint() : lat(0.0), lon(0.0), signal_strength(0), earfcn(0), pci(0) {}
    MapPoint(double _lat, double _lon) : lat(_lat), lon(_lon), signal_strength(0), earfcn(0), pci(0) {}
};
