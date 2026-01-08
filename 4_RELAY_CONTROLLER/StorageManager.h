#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Settings.h"

class StorageManager {
public:
    StorageManager();
    bool begin();
    
    // Configurazione Nodo
    bool loadConfig(String &nodeId, String &gatewayId, int pins[4]);
    bool saveConfig(const String &nodeId, const String &gatewayId, const int pins[4]);
    bool deleteConfig();
    
    // Configurazione MAC Gateway
    bool loadGatewayMac(uint8_t* mac);
    bool saveGatewayMac(const uint8_t* mac);
    bool deleteGatewayMac();

private:
    bool _fsInitialized;
};

#endif
