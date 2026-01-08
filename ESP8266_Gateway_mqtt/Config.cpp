#include "Config.h"
#include "WebLog.h"

// Parametri WiFi hardcoded per FORCE_HARDCODED_CONFIG = true
const char* wifi_ssid = "riky14hobby2";     // SSID della rete WiFi
const char* wifi_password = "monte80pora";   // Password della rete WiFi

// Parametri MQTT e Gateway
char mqtt_server[100] = "192.168.99.15"; // Default MQTT broker
int mqtt_port = 1883; // Default MQTT port
char mqtt_user[50] = "mqtt"; // MQTT username (opzionale)
char mqtt_password[50] = "mqtt"; // MQTT password (opzionale)
char mqtt_topic_prefix[50] = "domoriky"; // Prefisso topic MQTT
char gateway_id[50] = ""; // ID univoco del gateway per auto-discovery
char static_ip[16] = "192.168.99.19"; // IP statico di default
char static_gateway[16] = "192.168.99.1"; // Gateway di default
char static_subnet[16] = "255.255.255.0"; // Subnet mask di default
char static_dns[16] = "8.8.8.8"; // DNS di default
char network_mode[10] = "static"; // Modalità di rete: "static" o "dhcp"
bool led_enabled = true; // LED abilitato di default

// Credenziali WiFi salvate (caricate da config.json)
char saved_wifi_ssid[64] = "";
char saved_wifi_password[64] = "";
bool wifi_credentials_loaded = false;
const char* BOARD_NAME = "ESP8266Gateway";

// NTP Defaults
char ntp_server[64] = "pool.ntp.org";
long gmt_offset_sec = 3600;      // GMT+1
int daylight_offset_sec = 3600;  // DST+1

void loadConfigFromLittleFS() {
    if (!LittleFS.exists("/config.json")) {
        return;
    }
    
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile) {
        return;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    
    if (error) {
        return;
    }
    
    // Carica i valori salvati
    if (doc["mqtt_server"]) strncpy(mqtt_server, doc["mqtt_server"], sizeof(mqtt_server) - 1);
    if (doc["mqtt_port"]) mqtt_port = doc["mqtt_port"];
    if (doc["mqtt_user"]) strncpy(mqtt_user, doc["mqtt_user"], sizeof(mqtt_user) - 1);
    if (doc["mqtt_password"]) strncpy(mqtt_password, doc["mqtt_password"], sizeof(mqtt_password) - 1);
    if (doc["mqtt_topic_prefix"]) {
        const char* val = doc["mqtt_topic_prefix"];
        if (val && strlen(val) > 0 && strcmp(val, "null") != 0) {
            strncpy(mqtt_topic_prefix, val, sizeof(mqtt_topic_prefix) - 1);
        }
    }
    if (doc["gateway_id"]) {
        const char* val = doc["gateway_id"];
        if (val && strlen(val) > 0 && strcmp(val, "null") != 0) {
            strncpy(gateway_id, val, sizeof(gateway_id) - 1);
            DevLog.printf("[CONFIG] Loaded gateway_id from JSON: %s\n", gateway_id);
        } else {
            DevLog.println("[CONFIG] gateway_id in JSON is null or empty");
        }
    } else {
        DevLog.println("[CONFIG] gateway_id key missing in JSON");
    }
    if (doc["static_ip"]) strncpy(static_ip, doc["static_ip"], sizeof(static_ip) - 1);
    if (doc["static_gateway"]) strncpy(static_gateway, doc["static_gateway"], sizeof(static_gateway) - 1);
    if (doc["static_subnet"]) strncpy(static_subnet, doc["static_subnet"], sizeof(static_subnet) - 1);
    if (doc["static_dns"]) strncpy(static_dns, doc["static_dns"], sizeof(static_dns) - 1);
    if (doc["network_mode"]) strncpy(network_mode, doc["network_mode"], sizeof(network_mode) - 1);
    
    // LED Config
    if (doc.containsKey("led_enabled")) {
        led_enabled = doc["led_enabled"];
    }
    
    // NTP Config
    if (doc["ntp_server"]) strncpy(ntp_server, doc["ntp_server"], sizeof(ntp_server) - 1);
    if (doc.containsKey("gmt_offset_sec")) gmt_offset_sec = doc["gmt_offset_sec"];
    if (doc.containsKey("daylight_offset_sec")) daylight_offset_sec = doc["daylight_offset_sec"];

    // Carica le credenziali WiFi se presenti
    if (doc["wifi_ssid"] && doc["wifi_password"]) {
        strncpy(saved_wifi_ssid, doc["wifi_ssid"], sizeof(saved_wifi_ssid) - 1);
        strncpy(saved_wifi_password, doc["wifi_password"], sizeof(saved_wifi_password) - 1);
        wifi_credentials_loaded = true;
    } else {
        wifi_credentials_loaded = false;
    }
    
    DevLog.printf("✅ Configurazione caricata. Gateway ID: %s\n", gateway_id);
}

void saveConfigToLittleFS() {
    DynamicJsonDocument doc(2048);
    
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_port"] = mqtt_port;
    doc["mqtt_user"] = mqtt_user;
    doc["mqtt_password"] = mqtt_password;
    doc["mqtt_topic_prefix"] = mqtt_topic_prefix;
    doc["gateway_id"] = gateway_id;
    doc["network_mode"] = network_mode;
    doc["led_enabled"] = led_enabled;
    
    // NTP Config
    doc["ntp_server"] = ntp_server;
    doc["gmt_offset_sec"] = gmt_offset_sec;
    doc["daylight_offset_sec"] = daylight_offset_sec;

    // Salva valori IP solo se modalità statica, altrimenti azzera
    if (strcmp(network_mode, "static") == 0) {
        doc["static_ip"] = static_ip;
        doc["static_gateway"] = static_gateway;
        doc["static_subnet"] = static_subnet;
        doc["static_dns"] = static_dns;
    } else {
        // Azzera valori IP in modalità DHCP
        doc["static_ip"] = "";
        doc["static_gateway"] = "";
        doc["static_subnet"] = "";
        doc["static_dns"] = "";
        // Azzera anche le variabili globali
        strcpy(static_ip, "");
        strcpy(static_gateway, "");
        strcpy(static_subnet, "");
        strcpy(static_dns, "");
    }
    
    // NUOVO METODO: Salva solo le credenziali dalle variabili locali (mai dalla flash WiFi)
    // Usa solo saved_wifi_ssid e saved_wifi_password caricate da config.json
    if (strlen(saved_wifi_ssid) > 0) {
        doc["wifi_ssid"] = saved_wifi_ssid;
        doc["wifi_password"] = saved_wifi_password;
    } else {
        // Se non ci sono credenziali caricate, usa quelle hardcoded
        doc["wifi_ssid"] = wifi_ssid;
        doc["wifi_password"] = wifi_password;
    }
    
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
        return;
    }
    
    serializeJson(doc, configFile);
    configFile.close();
}
