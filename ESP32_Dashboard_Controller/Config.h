#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Versioning
#include "version.h"
extern const char* BUILD_DATE;
extern const char* BUILD_TIME;

// Network Config
extern char wifi_ssid[32];
extern char wifi_password[64];
extern char mqtt_server[64];
extern int mqtt_port;
extern char mqtt_user[32];
extern char mqtt_password[32];
extern char mqtt_topic_prefix[32];

// Advanced Config
extern char saved_wifi_ssid[32];
extern char saved_wifi_password[64];
extern char network_mode[10];
extern char static_ip[16];
extern char static_gateway[16];
extern char static_subnet[16];
extern char static_dns[16];
extern char gateway_id[32];

// NTP Config
extern char ntp_server[64];
extern float ntp_timezone;
extern bool ntp_dst;
extern long gmt_offset_sec;
extern int daylight_offset_sec;

// State flags
extern bool wifi_credentials_loaded;
extern bool apTransitionPending;
extern unsigned long apTransitionStart;
extern bool configMode;

// Global objects
extern bool dataChanged;
extern unsigned long lastBroadcast;

#endif
