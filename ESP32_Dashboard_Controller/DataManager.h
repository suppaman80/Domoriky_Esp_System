#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Structs.h"
#include "Config.h"
#include "WebLog.h"

extern std::map<String, GatewayInfo> gateways;
extern std::map<String, PeerInfo> peers;
extern std::vector<String> knownPrefixes;

void loadDiscoveredPrefixes();
void saveDiscoveredPrefixes();

// Network State Persistence
void saveNetworkState(); // Requests a save (non-blocking)
void handleNetworkSave(); // Processes pending saves (call in loop)
void forceSaveNetworkState(); // Forces immediate save (blocking)

void loadNetworkState();
void loadConfiguration();

#endif
