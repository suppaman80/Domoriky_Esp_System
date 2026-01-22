#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// --- PIN CONFIGURATION ---
#define MAX_RELAYS 6
#define PIN_DISABLED -1

// Safe GPIOs for ESP8266 Relay Control
// D1(5), D2(4), D5(14), D6(12), D7(13), D8(15)
static const int SAFE_GPIOS[] = {5, 4, 14, 12, 13, 15};
static const char* SAFE_GPIO_NAMES[] = {"D1 (GPIO 5)", "D2 (GPIO 4)", "D5 (GPIO 14)", "D6 (GPIO 12)", "D7 (GPIO 13)", "D8 (GPIO 15)"};

// Default Pins (First 4 mapped to D6, D7, D5, D8 as per original default, others disabled)
#define DEFAULT_RELAY1_PIN 12  // D6
#define DEFAULT_RELAY2_PIN 13  // D7
#define DEFAULT_RELAY3_PIN 14  // D5
#define DEFAULT_RELAY4_PIN 15  // D8
#define DEFAULT_RELAY5_PIN -1  // Disabled
#define DEFAULT_RELAY6_PIN -1  // Disabled

#define LED_STATUS 2   // GPIO2 - LED di stato (Attivo BASSO)
#define SETUP_PIN 0    // GPIO0 - Pin per modalità setup (con GND)

// --- CONFIGURAZIONE DEFAULT ---
#define DEFAULT_NODE_ID "NOME_NODO"
#define DEFAULT_GATEWAY_ID "GATEWAY_MAIN"
#define NODE_TYPE "RL_CTRL_ESP8266"

// --- CONFIGURAZIONE AP ---
#define AP_SSID "RL_CTRL_ESP8266_XCH"
#define AP_PASSWORD ""  // Nessuna password per accesso libero
#define CONFIG_TIMEOUT 300000  // 5 minuti timeout configurazione

// --- TIMING CONFIGURATION ---
#define DISCOVERY_INTERVAL 10000
#define LED_FEEDBACK_DURATION 200
#define DISCOVERY_TIMEOUT 30000  // 30 secondi timeout per discovery
#define HEARTBEAT_INTERVAL 300000 // 5 minuti intervallo heartbeat
#define WATCHDOG_TIMEOUT 8000     // 8 secondi

#endif
