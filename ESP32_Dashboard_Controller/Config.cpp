#include "Config.h"

// Versioning
const char* BUILD_DATE = __DATE__;
const char* BUILD_TIME = __TIME__;

// Config Defaults
#ifdef WOKWI_SIMULATION
  char wifi_ssid[32] = "Wokwi-GUEST";
  char wifi_password[64] = "";
  char mqtt_server[64] = "192.168.99.15"; 
  char mqtt_user[32] = "mqtt";
  char mqtt_password[32] = "mqtt";
#else
  char wifi_ssid[32] = "";
  char wifi_password[64] = "";
  char mqtt_server[64] = "";
  char mqtt_user[32] = "";
  char mqtt_password[32] = "";
#endif

int mqtt_port = 1883;
char mqtt_topic_prefix[32] = "domoriky";

char saved_wifi_ssid[32] = "";
char saved_wifi_password[64] = "";
char network_mode[10] = "dhcp";
char static_ip[16] = "";
char static_gateway[16] = "";
char static_subnet[16] = "";
char static_dns[16] = "";
char gateway_id[32] = "DASHBOARD_1"; 

// NTP Defaults
char ntp_server[64] = "pool.ntp.org";
float ntp_timezone = 1.0;
bool ntp_dst = true;
long gmt_offset_sec = 3600;      // GMT+1
int daylight_offset_sec = 3600;  // DST+1

bool wifi_credentials_loaded = false;
bool apTransitionPending = false;
unsigned long apTransitionStart = 0;
bool configMode = false;

bool dataChanged = false;
unsigned long lastBroadcast = 0;
