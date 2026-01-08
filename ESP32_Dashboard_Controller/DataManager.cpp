#include "DataManager.h"
#include "WebLog.h"

std::map<String, GatewayInfo> gateways;
std::map<String, PeerInfo> peers;
std::vector<String> knownPrefixes;

void loadDiscoveredPrefixes() {
    DevLog.println("[CONFIG] Caricamento prefissi MQTT scoperti...");
    if (LittleFS.exists("/prefixes.json")) {
        File file = LittleFS.open("/prefixes.json", "r");
        if (file) {
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonVariant v : arr) {
                    String p = v.as<String>();
                    bool exists = false;
                    for (const auto& existing : knownPrefixes) {
                        if (existing == p) { exists = true; break; }
                    }
                    if (!exists) {
                        knownPrefixes.push_back(p);
                        DevLog.printf("[CONFIG] Prefisso caricato: %s\n", p.c_str());
                    }
                }
            } else {
                DevLog.println("[CONFIG] Errore parsing prefixes.json");
            }
            file.close();
        }
    } else {
        DevLog.println("[CONFIG] prefixes.json non trovato (primo avvio o reset)");
    }
}

void saveDiscoveredPrefixes() {
    File file = LittleFS.open("/prefixes.json", "w");
    if (file) {
        DynamicJsonDocument doc(1024);
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& p : knownPrefixes) {
            arr.add(p);
        }
        serializeJson(doc, file);
        file.close();
        DevLog.println("[CONFIG] Prefissi MQTT salvati su LittleFS");
    } else {
        DevLog.println("[CONFIG] Errore scrittura prefixes.json");
    }
}

// --- Network State Persistence (Debounced) ---
bool networkStateNeedsSave = false;
unsigned long lastNetworkSave = 0;
const unsigned long NETWORK_SAVE_INTERVAL = 5000; // 5 seconds debounce

void forceSaveNetworkState() {
    File file = LittleFS.open("/network_state.json", "w");
    if (file) {
        DynamicJsonDocument doc(4096);
        
        // Save Gateways
        JsonObject gatewaysObj = doc.createNestedObject("gateways");
        for (const auto& kv : gateways) {
            JsonObject g = gatewaysObj.createNestedObject(kv.first);
            g["id"] = kv.second.id;
            g["ip"] = kv.second.ip;
            g["mqttStatus"] = kv.second.mqttStatus;
            g["uptime"] = kv.second.uptime;
            g["lastSeen"] = kv.second.lastSeen;
            g["mac"] = kv.second.mac;
            g["version"] = kv.second.version;
            g["buildDate"] = kv.second.buildDate;
            g["mqttPrefix"] = kv.second.mqttPrefix;
        }

        // Save Peers
        JsonObject peersObj = doc.createNestedObject("peers");
        for (const auto& kv : peers) {
            JsonObject p = peersObj.createNestedObject(kv.first);
            p["nodeId"] = kv.second.nodeId;
            p["nodeType"] = kv.second.nodeType;
            p["gatewayId"] = kv.second.gatewayId;
            p["status"] = kv.second.status;
            p["mac"] = kv.second.mac;
            p["firmwareVersion"] = kv.second.firmwareVersion;
            p["attributes"] = kv.second.attributes;
            p["lastSeen"] = kv.second.lastSeen;
        }

        serializeJson(doc, file);
        file.close();
        // DevLog.println("[STATE] Stato rete salvato su LittleFS");
    } else {
        DevLog.println("[STATE] Errore salvataggio network_state.json");
    }
    
    // Always trigger UI update when data changes, even if save is deferred
    dataChanged = true; 
}

void saveNetworkState() {
    // Request a save
    networkStateNeedsSave = true;
    dataChanged = true; // Update UI immediately
}

void handleNetworkSave() {
    if (networkStateNeedsSave) {
        if (millis() - lastNetworkSave > NETWORK_SAVE_INTERVAL) {
            forceSaveNetworkState();
            networkStateNeedsSave = false;
            lastNetworkSave = millis();
        }
    }
}

void loadNetworkState() {
    if (LittleFS.exists("/network_state.json")) {
        File file = LittleFS.open("/network_state.json", "r");
        if (file) {
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                // Load Gateways
                JsonObject gatewaysObj = doc["gateways"];
                for (JsonPair kv : gatewaysObj) {
                    JsonObject g = kv.value().as<JsonObject>();
                    GatewayInfo info;
                    info.id = g["id"].as<String>();
                    // Sanitize invalid gateways
                    if (info.id == "null" || info.id == "NULL" || info.id.length() == 0) continue;

                    info.ip = g["ip"].as<String>();
                    info.mqttStatus = g["mqttStatus"].as<String>();
                    info.uptime = g["uptime"].as<unsigned long>();
                    info.lastSeen = g["lastSeen"].as<unsigned long>();
                    info.mac = g["mac"].as<String>();
                    info.version = g["version"].as<String>();
                    info.buildDate = g["buildDate"].as<String>();
                    info.mqttPrefix = g["mqttPrefix"].as<String>();
                    gateways[info.id] = info;
                }

                // Load Peers
                JsonObject peersObj = doc["peers"];
                for (JsonPair kv : peersObj) {
                    String key = kv.key().c_str(); // Restore original map key
                    JsonObject p = kv.value().as<JsonObject>();
                    PeerInfo info;
                    info.nodeId = p["nodeId"].as<String>();
                    info.nodeType = p["nodeType"].as<String>();
                    info.gatewayId = p["gatewayId"].as<String>();
                    info.status = p["status"].as<String>();
                    info.mac = p["mac"].as<String>();
                    info.firmwareVersion = p["firmwareVersion"].as<String>();
                    if (p.containsKey("attributes")) info.attributes = p["attributes"].as<String>();
                    info.lastSeen = p["lastSeen"].as<unsigned long>();
                    peers[key] = info; // Use restored key instead of nodeId
                }
                DevLog.println("[STATE] Stato rete caricato da LittleFS");
            } else {
                DevLog.println("[STATE] Errore parsing network_state.json");
            }
            file.close();
        }
    } else {
        DevLog.println("[STATE] network_state.json non trovato (primo avvio)");
    }
}

void loadConfiguration() {
    DevLog.println("[CONFIG] Tentativo montaggio LittleFS...");
    if (LittleFS.begin(true)) { // true = formatta se non esiste
        DevLog.println("[CONFIG] LittleFS montato.");
        if (LittleFS.exists("/config.json")) {
            DevLog.println("[CONFIG] Trovato config.json");
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                DevLog.println("[CONFIG] Lettura file...");
                DynamicJsonDocument doc(4096);
                DeserializationError error = deserializeJson(doc, configFile);
                if (!error) {
                    DevLog.println("[CONFIG] Parsing JSON OK.");
                    // Caricamento variabili standard
                    strlcpy(wifi_ssid, doc["wifi_ssid"] | "", sizeof(wifi_ssid));
                    strlcpy(wifi_password, doc["wifi_password"] | "", sizeof(wifi_password));
                    strlcpy(mqtt_server, doc["mqtt_server"] | "", sizeof(mqtt_server));
                    mqtt_port = doc["mqtt_port"] | 1883;
                    strlcpy(mqtt_user, doc["mqtt_user"] | "", sizeof(mqtt_user));
                    strlcpy(mqtt_password, doc["mqtt_password"] | "", sizeof(mqtt_password));
                    strlcpy(mqtt_topic_prefix, doc["mqtt_topic_prefix"] | "domoriky", sizeof(mqtt_topic_prefix));
                    
                    // Caricamento variabili avanzate
                    strlcpy(saved_wifi_ssid, wifi_ssid, sizeof(saved_wifi_ssid));
                    strlcpy(saved_wifi_password, wifi_password, sizeof(saved_wifi_password));
                    strlcpy(gateway_id, doc["gateway_id"] | "DASHBOARD_1", sizeof(gateway_id));
                    strlcpy(network_mode, doc["network_mode"] | "dhcp", sizeof(network_mode));
                    strlcpy(static_ip, doc["static_ip"] | "", sizeof(static_ip));
                    strlcpy(static_gateway, doc["static_gateway"] | "", sizeof(static_gateway));
                    strlcpy(static_subnet, doc["static_subnet"] | "", sizeof(static_subnet));
                    strlcpy(static_dns, doc["static_dns"] | "", sizeof(static_dns));
                    
                    // Caricamento NTP
                    strlcpy(ntp_server, doc["ntp_server"] | "pool.ntp.org", sizeof(ntp_server));
                    ntp_timezone = doc["ntp_timezone"] | 1.0;
                    ntp_dst = doc["ntp_dst"] | true;
                    
                    gmt_offset_sec = (long)(ntp_timezone * 3600);
                    daylight_offset_sec = ntp_dst ? 3600 : 0;

                    wifi_credentials_loaded = true;
                    DevLog.println("[CONFIG] Configurazione caricata correttamente.");
                } else {
                    DevLog.print("[CONFIG] Errore parsing config.json: ");
                    DevLog.println(error.c_str());
                    wifi_credentials_loaded = false;
                }
                configFile.close();
            }
        } else {
            DevLog.println("[CONFIG] config.json non trovato");
            wifi_credentials_loaded = false;
        }
    } else {
        DevLog.println("[CONFIG] Errore critico montaggio LittleFS");
    }
}
