#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// --- MODALITÀ DI AVVIO --- //
// Cambia questo valore per scegliere la modalità di avvio:
// true  = Forza utilizzo configurazione hardcoded
// false = Modalità intelligente (MQTT se config esiste, altrimenti AP per configurazione)
#define FORCE_HARDCODED_CONFIG false

// --- CONFIGURAZIONE GLOBALE --- //
extern const char* wifi_ssid;
extern const char* wifi_password;

extern char mqtt_server[100];
extern int mqtt_port;
extern char mqtt_user[50];
extern char mqtt_password[50];
extern char mqtt_topic_prefix[50];
extern char gateway_id[50];
extern char static_ip[16];
extern char static_gateway[16];
extern char static_subnet[16];
extern char static_dns[16];
extern char network_mode[10];
extern bool led_enabled;

extern char saved_wifi_ssid[64];
extern char saved_wifi_password[64];
extern bool wifi_credentials_loaded;
extern const char* BOARD_NAME;

// --- NTP CONFIGURATION --- //
extern char ntp_server[64];
extern long gmt_offset_sec;
extern int daylight_offset_sec;

// --- TIMEOUT CONFIGURATION --- //
const unsigned long NETWORK_DISCOVERY_TIMEOUT = 5000;  // Increased to 5s
const unsigned long PING_RESPONSE_TIMEOUT = 10000;     // Increased to 10s (was 3s)
const unsigned long NODE_OFFLINE_TIMEOUT = 2400000;    // Increased to 40 min (was 20 min)

// Funzioni
void loadConfigFromLittleFS();
void saveConfigToLittleFS();

#endif
