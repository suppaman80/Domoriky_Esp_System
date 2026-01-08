#include "StorageManager.h"

StorageManager::StorageManager() {
    _fsInitialized = false;
}

bool StorageManager::begin() {
    if (!_fsInitialized) {
        if (LittleFS.begin()) {
            _fsInitialized = true;
        } else {
            Serial.println("Errore montaggio LittleFS");
        }
    }
    return _fsInitialized;
}

bool StorageManager::loadConfig(String &nodeId, String &gatewayId, int pins[4]) {
    if (!begin()) return false;
    
    if (!LittleFS.exists("/config.json")) {
        return false;
    }
    
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile) return false;
    
    String configData = configFile.readString();
    configFile.close();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, configData);
    
    if (error) {
        Serial.println("Errore parsing JSON configurazione");
        return false;
    }
    
    nodeId = doc["nodeId"].as<String>();
    gatewayId = doc["gatewayId"].as<String>();
    
    // Carica PIN se presenti, altrimenti usa default
    if (doc.containsKey("pins") && doc["pins"].is<JsonArray>()) {
        JsonArray pinArray = doc["pins"];
        if (pinArray.size() >= 4) {
            for(int i=0; i<4; i++) {
                pins[i] = pinArray[i];
            }
        }
    } else {
        // Fallback to defaults defined in Settings.h
        pins[0] = RELAY1_PIN;
        pins[1] = RELAY2_PIN;
        pins[2] = RELAY3_PIN;
        pins[3] = RELAY4_PIN;
    }

    return true;
}

bool StorageManager::saveConfig(const String &nodeId, const String &gatewayId, const int pins[4]) {
    if (!begin()) return false;
    
    DynamicJsonDocument doc(1024);
    doc["nodeId"] = nodeId;
    doc["gatewayId"] = gatewayId;
    
    JsonArray pinArray = doc.createNestedArray("pins");
    for(int i=0; i<4; i++) {
        pinArray.add(pins[i]);
    }
    
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) return false;
    
    serializeJson(doc, configFile);
    configFile.close();
    return true;
}

bool StorageManager::deleteConfig() {
    if (!begin()) return false;
    if (LittleFS.exists("/config.json")) {
        return LittleFS.remove("/config.json");
    }
    return true;
}

bool StorageManager::loadGatewayMac(uint8_t* mac) {
    if (!begin()) return false;
    
    if (!LittleFS.exists("/gateway_mac.dat")) return false;
    
    File macFile = LittleFS.open("/gateway_mac.dat", "r");
    if (!macFile) return false;
    
    size_t bytesRead = macFile.read(mac, 6);
    macFile.close();
    
    if (bytesRead != 6) return false;
    
    // Verifica validità (non tutti zero)
    bool isAllZeros = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) isAllZeros = false;
    }
    
    return !isAllZeros;
}

bool StorageManager::saveGatewayMac(const uint8_t* mac) {
    if (!begin()) return false;
    
    File macFile = LittleFS.open("/gateway_mac.dat", "w");
    if (!macFile) return false;
    
    macFile.write(mac, 6);
    macFile.close();
    return true;
}

bool StorageManager::deleteGatewayMac() {
    if (!begin()) return false;
    if (LittleFS.exists("/gateway_mac.dat")) {
        return LittleFS.remove("/gateway_mac.dat");
    }
    return true;
}
