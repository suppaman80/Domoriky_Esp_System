#ifndef STRUCTS_H
#define STRUCTS_H

#include <Arduino.h>

struct GatewayInfo {
    String id;
    String ip;
    String mqttStatus;
    unsigned long uptime;
    unsigned long lastSeen;
    String mac;
    String version;
    String buildDate;
    String mqttPrefix;
};

struct PeerInfo {
    String nodeId;
    String nodeType;
    String gatewayId;
    String status;
    String mac;
    String firmwareVersion;
    String attributes;
    unsigned long lastSeen;
};

struct UpdateInfo {
    bool available = false;
    String version = "";
    String url = "";
    String notes = "";
    bool mandatory = false;
};

struct SystemUpdates {
    UpdateInfo dashboard;
    UpdateInfo gateway;
    std::map<String, UpdateInfo> nodes;
    unsigned long lastCheck = 0;
    String lastResult = "";
};

#endif
