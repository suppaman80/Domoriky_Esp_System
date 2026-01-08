#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// --- PIN CONFIGURATION ---
#define RELAY1_PIN 12  // GPIO12 - Relè 1
#define RELAY2_PIN 13  // GPIO13 - Relè 2
#define RELAY3_PIN 14  // GPIO14 - Relè 3
#define RELAY4_PIN 15  // GPIO15 - Relè 4
#define LED_STATUS 2   // GPIO2 - LED di stato (Attivo BASSO)
#define SETUP_PIN 0    // GPIO0 - Pin per modalità setup (con GND)

// --- CONFIGURAZIONE DEFAULT ---
#define DEFAULT_NODE_ID "4_RELAY_CONTROLLER"
#define DEFAULT_GATEWAY_ID "GATEWAY_MAIN"
#define NODE_TYPE "4_RELAY_CONTROLLER"

// --- CONFIGURAZIONE AP ---
#define AP_SSID "Domoriky_4_RELAY_CONTROLLER"
#define AP_PASSWORD ""  // Nessuna password per accesso libero
#define CONFIG_TIMEOUT 300000  // 5 minuti timeout configurazione

// --- TIMING CONFIGURATION ---
#define DISCOVERY_INTERVAL 10000
#define LED_FEEDBACK_DURATION 200
#define DISCOVERY_TIMEOUT 30000  // 30 secondi timeout per discovery
#define HEARTBEAT_INTERVAL 300000 // 5 minuti intervallo heartbeat
#define WATCHDOG_TIMEOUT 8000     // 8 secondi

#endif
