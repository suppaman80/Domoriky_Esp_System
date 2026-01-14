/*
 * Configurable Relay Node - Controllo 4 Relè via ESP-NOW con Configurazione Web
 * Basato su Simple_Relay_Node con aggiunta configurazione via interfaccia web
 * 
 * FUNZIONALITÀ:
 * - Modalità AP per configurazione (GPIO0 + GND all'avvio)
 * - Webserver per impostare Node ID e Gateway Target
 * - Salvataggio configurazione in LittleFS
 * - Riavvio automatico dopo configurazione
 * - Modalità normale: solo ESP-NOW (no WiFi)
 * 
 * HARDWARE SETUP:
 * - GPIO0 + GND all'avvio = Modalità configurazione AP
 * - Avvio normale = Modalità operativa ESP-NOW
 * 
 * Schema JSON comandi (identico a Simple_Relay_Node):
 * {
 *   "Node": "RELAY_NODE_01",
 *   "Topic": "relay_1|relay_2|relay_3|relay_4|Life",
 *   "Command": "0|1|2",  // 0=OFF, 1=ON, 2=SWITCH
 *   "Status": "risposta del comando",
 *   "Type": "COMMAND|FEEDBACK"
 * }
 */

#include "DomoticaEspNow.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESP8266httpUpdate.h>
#include "version.h"

const char* BUILD_DATE = __DATE__;
const char* BUILD_TIME = __TIME__;

extern "C" {
  #include "user_interface.h"
}

// --- PIN CONFIGURATION ---
// Pin configurabili - Default values
int relayPins[4] = {12, 13, 14, 15};

#define LED_STATUS 2   // GPIO2 - LED di stato
#define SETUP_PIN 0    // GPIO0 - Pin per modalità setup (con GND)

// Helper per options HTML
String getPinOptions(int selected) {
    String options = "";
    struct PinDef { int gpio; const char* name; };
    PinDef pins[] = {
        {16, "GPIO 16 (D0)"},
        {5, "GPIO 5 (D1)"},
        {4, "GPIO 4 (D2)"},
        {0, "GPIO 0 (D3)"},
        {2, "GPIO 2 (D4)"},
        {14, "GPIO 14 (D5)"},
        {12, "GPIO 12 (D6)"},
        {13, "GPIO 13 (D7)"},
        {15, "GPIO 15 (D8)"},
        {3, "GPIO 3 (RX)"},
        {1, "GPIO 1 (TX)"}
    };
    
    for(int i=0; i<11; i++) {
        options += "<option value='" + String(pins[i].gpio) + "'";
        if(pins[i].gpio == selected) options += " selected";
        options += ">" + String(pins[i].name) + "</option>";
    }
    return options;
}

// Configurazione Logica Relè (true = modulo relè attivo basso, comune per ESP8266)
#define RELAY_ACTIVE_LOW false

// --- CONFIGURAZIONE DEFAULT ---
#define DEFAULT_NODE_ID "NODE_NAME"
#define DEFAULT_GATEWAY_ID "GATEWAY_MAIN"

// --- HARDCODED CONFIGURATION OVERRIDE ---
// Decommentare per forzare la configurazione ed evitare la modalità AP
//#define FORCE_CONFIG_OVERRIDE

#ifdef FORCE_CONFIG_OVERRIDE
  #define FORCE_NODE_ID       "CUCINA"   // <--- Inserisci qui il tuo ID NODO
  #define FORCE_GATEWAY_ID    "GATEWAY_02"      // <--- Inserisci qui il tuo ID GATEWAY
  // PIN dei Relè: D6, D7, D5, D8 (Standard Wemos D1 Mini / NodeMCU)
  #define FORCE_RELAY_PIN_1   12
  #define FORCE_RELAY_PIN_2   13
  #define FORCE_RELAY_PIN_3   4
  #define FORCE_RELAY_PIN_4   5
#endif

#define NODE_TYPE "4_RELAY_CONTROLLER"

// --- CONFIGURAZIONE AP ---
#define AP_SSID "Domoriky_4_RelayNode"
#define AP_PASSWORD ""  // Nessuna password per accesso libero
#define CONFIG_TIMEOUT 300000  // 5 minuti timeout configurazione

// --- TIMING CONFIGURATION ---
#define DISCOVERY_INTERVAL 10000
#define LED_FEEDBACK_DURATION 200
#define DISCOVERY_TIMEOUT 30000  // 30 secondi timeout per discovery
#define HEARTBEAT_INTERVAL 300000 // 5 minuti intervallo heartbeat

// --- VARIABILI GLOBALI ---
DomoticaEspNow espNow;
struct_message myData;
struct_message receivedData;
ESP8266WebServer server(80);
DNSServer dnsServer;
unsigned long lastMessageReceived = 0;
unsigned long lastActivity = 0;
unsigned long lastHeartbeatTime = 0; // Timer per heartbeat

// Configurazione nodo (caricata da LittleFS)
String nodeId = DEFAULT_NODE_ID;
String targetGatewayId = DEFAULT_GATEWAY_ID;

// Flag modalità
bool configMode = false;
bool configSaved = false;

// Flag per proteggere callback ESP-NOW
volatile bool processingCommand = false;

// Variabili per gestione LED non bloccante
unsigned long ledOnTime = 0;

// Variabili per la gestione del gateway
bool gatewayFound = false;
uint8_t gatewayMac[6];
unsigned long lastDiscoveryAttempt = 0;
unsigned long discoveryStartTime = 0;
bool waitingForDiscoveryResponse = false;
bool gatewayConnectionLost = false;
unsigned long lastGatewayMessage = 0;

// Stati dei relè
bool relayStates[4] = {false, false, false, false};

// Variabili per gestione riavvio asincrono
bool restartPending = false;
bool factoryResetPending = false;
unsigned long restartTime = 0;

// Variabili per OTA Update
bool otaPending = false;
String otaUrl = "";
String otaSsid = "";
String otaPass = "";
unsigned long otaStartTime = 0;

// --- FUNZIONI DI UTILITÀ ---
void printMacAddress(const uint8_t* mac, const char* prefix) {
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print(prefix);
    Serial.println(macStr);
}

// --- GESTIONE CONFIGURAZIONE LITTLEFS ---
bool loadGatewayMac() {
    if (!LittleFS.begin()) {
        Serial.println("Errore inizializzazione LittleFS per MAC gateway");
        return false;
    }
    
    if (!LittleFS.exists("/gateway_mac.dat")) {
        Serial.println("MAC address gateway non trovato in LittleFS");
        return false;
    }
    
    File macFile = LittleFS.open("/gateway_mac.dat", "r");
    if (!macFile) {
        Serial.println("Errore apertura file MAC gateway");
        return false;
    }
    
    uint8_t savedMac[6];
    size_t bytesRead = macFile.read(savedMac, 6);
    macFile.close();
    
    if (bytesRead != 6) {
        Serial.println("File MAC gateway corrotto");
        return false;
    }
    
    // Verifica se il MAC è valido (non tutti zeri)
    bool isAllZeros = true;
    for (int i = 0; i < 6; i++) {
        if (savedMac[i] != 0) {
            isAllZeros = false;
            break;
        }
    }
    
    if (isAllZeros) {
        Serial.println("MAC gateway salvato non valido (tutti zeri)");
        return false;
    }
    
    memcpy(gatewayMac, savedMac, 6);
    gatewayFound = true;
    
    Serial.print("MAC gateway caricato da LittleFS: ");
    printMacAddress(gatewayMac, "");
    
    return true;
}

void performFactoryReset() {
    Serial.println("⚠️ ESECUZIONE RESET TOTALE (AP MODE)...");
    
    // Assicura che LittleFS sia in uno stato pulito
    LittleFS.end();
    
    if (LittleFS.begin()) {
        Serial.println("📂 LittleFS montato correttamente");
        
        // Cancella config.json (WiFi, NodeID, ecc)
        if (LittleFS.exists("/config.json")) {
            if (LittleFS.remove("/config.json")) {
                Serial.println("✅ Configurazione WiFi/Nodo cancellata (/config.json)");
            } else {
                Serial.println("❌ Errore rimozione /config.json");
            }
        } else {
            Serial.println("ℹ️ /config.json non trovato (già cancellato?)");
        }
        
        // Cancella gateway_mac.dat
        if (LittleFS.exists("/gateway_mac.dat")) {
            if (LittleFS.remove("/gateway_mac.dat")) {
                Serial.println("✅ MAC gateway cancellato (/gateway_mac.dat)");
            } else {
                Serial.println("❌ Errore rimozione /gateway_mac.dat");
            }
        } else {
            Serial.println("ℹ️ /gateway_mac.dat non trovato");
        }
        
        LittleFS.end();
    } else {
         Serial.println("❌ Errore accesso LittleFS - Impossibile cancellare file!");
    }
    
    Serial.println("🔄 Riavvio sistema in modalità AP tra 1 secondo...");
    delay(1000);
    ESP.restart();
}

bool saveGatewayMac() {
    if (!LittleFS.begin()) {
        Serial.println("Errore inizializzazione LittleFS per salvataggio MAC");
        return false;
    }
    
    File macFile = LittleFS.open("/gateway_mac.dat", "w");
    if (!macFile) {
        Serial.println("Errore creazione file MAC gateway");
        return false;
    }
    
    macFile.write(gatewayMac, 6);
    macFile.close();
    
    Serial.print("MAC gateway salvato in LittleFS: ");
    printMacAddress(gatewayMac, "");
    
    return true;
}

bool deleteGatewayMac() {
    if (!LittleFS.begin()) {
        Serial.println("Errore inizializzazione LittleFS per eliminazione MAC");
        return false;
    }
    
    if (LittleFS.exists("/gateway_mac.dat")) {
        if (LittleFS.remove("/gateway_mac.dat")) {
            Serial.println("MAC gateway eliminato da LittleFS");
            return true;
        } else {
            Serial.println("Errore eliminazione MAC gateway");
            return false;
        }
    }
    
    return true;
}

void applyHardcodedConfig() {
#ifdef FORCE_CONFIG_OVERRIDE
    Serial.println("\n!!! APPLICAZIONE CONFIGURAZIONE HARDCODED !!!");
    
    // Inizializza LittleFS se necessario
    if (!LittleFS.begin()) {
        Serial.println("Errore mount LittleFS per hardcode config");
        return;
    }

    DynamicJsonDocument doc(1024);
    doc["nodeId"] = FORCE_NODE_ID;
    doc["gatewayId"] = FORCE_GATEWAY_ID;
    
    JsonArray pinArray = doc.createNestedArray("pins");
    pinArray.add(FORCE_RELAY_PIN_1);
    pinArray.add(FORCE_RELAY_PIN_2);
    pinArray.add(FORCE_RELAY_PIN_3);
    pinArray.add(FORCE_RELAY_PIN_4);
    
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Errore scrittura file config hardcoded");
        return;
    }
    
    serializeJson(doc, configFile);
    configFile.close();
    
    Serial.println("Configurazione hardcoded scritta su /config.json");
    Serial.print("Node ID: "); Serial.println(FORCE_NODE_ID);
    Serial.print("Gateway ID: "); Serial.println(FORCE_GATEWAY_ID);
    Serial.println("Modalità AP verrà bypassata.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
#endif
}

bool loadConfiguration() {
    bool fsMounted = false;
    for (int i = 0; i < 3; i++) {
        if (LittleFS.begin()) {
            fsMounted = true;
            break;
        }
        Serial.println("Errore mount LittleFS, riprovo...");
        delay(200);
    }

    if (!fsMounted) {
        Serial.println("Errore CRITICO inizializzazione LittleFS");
        return false;
    }
    
    if (!LittleFS.exists("/config.json")) {
        Serial.println("File configurazione non trovato - uso valori default");
        return false;
    }
    
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile) {
        Serial.println("Errore apertura file configurazione");
        return false;
    }
    
    String configData = configFile.readString();
    configFile.close();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, configData);
    
    if (error) {
        Serial.println("Errore parsing configurazione JSON");
        return false;
    }
    
    nodeId = doc["nodeId"].as<String>();
    targetGatewayId = doc["gatewayId"].as<String>();
    
    // Carica PIN se presenti
    if (doc.containsKey("pins") && doc["pins"].is<JsonArray>()) {
        JsonArray pinArray = doc["pins"];
        if (pinArray.size() >= 4) {
            for(int i=0; i<4; i++) {
                relayPins[i] = pinArray[i];
            }
        }
    }

    Serial.println("=== CONFIGURAZIONE CARICATA ===");
    Serial.print("Node ID: "); Serial.println(nodeId);
    Serial.print("Gateway ID: "); Serial.println(targetGatewayId);
    Serial.print("Pins: "); 
    for(int i=0; i<4; i++) { Serial.print(relayPins[i]); Serial.print(" "); }
    Serial.println("\n===============================");
    
    return true;
}

bool saveConfiguration() {
    if (!LittleFS.begin()) {
        Serial.println("Errore inizializzazione LittleFS per salvataggio");
        return false;
    }
    
    DynamicJsonDocument doc(1024);
    doc["nodeId"] = nodeId;
    doc["gatewayId"] = targetGatewayId;
    
    JsonArray pinArray = doc.createNestedArray("pins");
    for(int i=0; i<4; i++) {
        pinArray.add(relayPins[i]);
    }
    
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Errore creazione file configurazione");
        return false;
    }
    
    serializeJson(doc, configFile);
    configFile.close();
    
    Serial.println("=== CONFIGURAZIONE SALVATA ===");
    Serial.print("Node ID: "); Serial.println(nodeId);
    Serial.print("Gateway ID: "); Serial.println(targetGatewayId);
    Serial.println("==============================");
    
    return true;
}

// --- WEBSERVER HANDLERS ---
void handleRoot() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Configurazione Relay Node</title>";
    html += "<meta charset='UTF-8'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; }";
    html += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
    html += "label { display: block; margin: 15px 0 5px 0; font-weight: bold; color: #555; }";
    html += "input[type='text'] { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
    html += "input[type='text']:focus { border-color: #4CAF50; outline: none; }";
    html += "button { background: #4CAF50; color: white; padding: 15px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; margin-top: 20px; }";
    html += "button:hover { background: #45a049; }";
    html += ".info { background: #e7f3ff; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🔧 Configurazione Relay Node</h1>";
    html += "<p style='text-align: center; color: #666; font-size: 12px;'>FW: " + String(FIRMWARE_VERSION) + " (Build: " + String(BUILD_DATE) + ")</p>";
    html += "<div class='info'>";
    html += "<strong>Istruzioni:</strong><br>";
    html += "• Inserisci l'ID univoco per questo nodo relay<br>";
    html += "• Specifica l'ID del gateway di destinazione<br>";
    html += "• Clicca Salva per applicare e riavviare";
    html += "</div>";
    html += "<form action='/save' method='POST'>";
    html += "<label for='nodeId'>ID Nodo Relay:</label>";
    html += "<input type='text' id='nodeId' name='nodeId' value='" + nodeId + "' placeholder='es: CUCINA_RELAY' required>";
    html += "<label for='gatewayId'>ID Gateway Destinazione:</label>";
    html += "<input type='text' id='gatewayId' name='gatewayId' value='" + targetGatewayId + "' placeholder='es: GATEWAY_MAIN_2' required>";
    
    html += "<div style='background:#f9f9f9;padding:10px;margin-top:10px;border-radius:5px;border:1px solid #eee;'>";
    html += "<h3 style='margin:5px 0 10px 0;color:#666'>Assegnazione GPIO Relè</h3>";
    for(int i=0; i<4; i++) {
        html += "<label for='pin" + String(i) + "'>Relè " + String(i+1) + ":</label>";
        html += "<select name='pin" + String(i) + "' id='pin" + String(i) + "' style='width:100%;padding:10px;border:2px solid #ddd;border-radius:5px;font-size:16px;box-sizing:border-box;margin-bottom:10px;'>";
        html += getPinOptions(relayPins[i]);
        html += "</select>";
    }
    html += "</div>";
    
    html += "<button type='submit'>💾 Salva e Riavvia</button>";
    html += "</form>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleSave() {
    if (server.hasArg("nodeId") && server.hasArg("gatewayId")) {
        nodeId = server.arg("nodeId");
        targetGatewayId = server.arg("gatewayId");
        
        // Rimuovi spazi e caratteri non validi
        nodeId.trim();
        targetGatewayId.trim();
        
        // Leggi PIN dai parametri POST
        for(int i=0; i<4; i++) {
            String argName = "pin" + String(i);
            if(server.hasArg(argName)) {
                relayPins[i] = server.arg(argName).toInt();
            }
        }
        
        if (nodeId.length() > 0 && targetGatewayId.length() > 0) {
            if (saveConfiguration()) {
                String html = "<!DOCTYPE html>";
                html += "<html><head><title>Configurazione Salvata</title>";
                html += "<meta charset='UTF-8'>";
                html += "<style>body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; text-align: center; }";
                html += ".success { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; }";
                html += "h1 { color: #4CAF50; }";
                html += ".info { background: #d4edda; padding: 15px; border-radius: 5px; margin: 20px 0; border-left: 4px solid #28a745; }";
                html += "</style></head><body>";
                html += "<div class='success'>";
                html += "<h1>✅ Configurazione Salvata!</h1>";
                html += "<div class='info'>";
                html += "<strong>Node ID:</strong> " + nodeId + "<br>";
                html += "<strong>Gateway ID:</strong> " + targetGatewayId;
                html += "</div>";
                html += "<p>Il nodo si riavvierà automaticamente in 3 secondi...</p>";
                html += "<script>setTimeout(function(){ window.close(); }, 3000);</script>";
                html += "</div></body></html>";
                
                server.send(200, "text/html", html);
                configSaved = true;
                return;
            }
        }
    }
    
    // Errore nel salvataggio
    server.send(400, "text/plain", "Errore: parametri non validi");
}

// --- CALLBACK ESP-NOW ---
void OnDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
    // Aggiorna timestamp ultimo messaggio ricevuto per gestione light sleep
    lastMessageReceived = millis();
    lastActivity = millis(); // Aggiorna attività reale
    
    // Protezione callback - ignora se già in elaborazione
    if (processingCommand) {
        return;
    }
    
    if (len == sizeof(receivedData)) {
        memcpy(&receivedData, incomingData, sizeof(receivedData));
        
        // Assicura terminazione stringhe
        receivedData.node[sizeof(receivedData.node) - 1] = '\0';
        receivedData.topic[sizeof(receivedData.topic) - 1] = '\0';
        receivedData.command[sizeof(receivedData.command) - 1] = '\0';
        receivedData.status[sizeof(receivedData.status) - 1] = '\0';
        receivedData.type[sizeof(receivedData.type) - 1] = '\0';
        receivedData.gateway_id[sizeof(receivedData.gateway_id) - 1] = '\0';
        
        char debugBuffer[256];
        snprintf(debugBuffer, sizeof(debugBuffer), "RICEVUTO - (\"node\":\"%s\")(\"topic\":\"%s\")(\"command\":\"%s\")(\"status\":\"%s\")(\"type\":\"%s\")(\"gateway_id\":\"%s\")",
                 receivedData.node, receivedData.topic, receivedData.command, receivedData.status, receivedData.type, receivedData.gateway_id);
        Serial.println(debugBuffer);
        
        // Debug: mostra MAC address ricevuto
         printMacAddress(mac, "[DEBUG] Messaggio ricevuto da MAC: ");
        
        // Gestione discovery response dal gateway
        if (strcmp(receivedData.type, "GATEWAY_INFO") == 0 && 
            strcmp(receivedData.topic, "DISCOVERY") == 0 &&
            strcmp(receivedData.command, "RESPONSE") == 0) {
            
            if (strcmp(receivedData.gateway_id, targetGatewayId.c_str()) == 0) {
                memcpy(gatewayMac, mac, 6);
                gatewayFound = true;
                waitingForDiscoveryResponse = false;
                
                // Debug: mostra MAC address memorizzato
                printMacAddress(gatewayMac, "[DEBUG] MAC Gateway memorizzato: ");
                
                // Salva il MAC address in LittleFS
                saveGatewayMac();
                
                if (!espNow.hasPeer(gatewayMac)) { espNow.addPeer(gatewayMac); }
    
                sendGatewayRegistration();
            }
            return;
        }
        
        // Gestione comandi dal gateway (nuovo schema: topic-based)
        if (strcmp(receivedData.gateway_id, targetGatewayId.c_str()) == 0 && 
            strcmp(receivedData.type, "COMMAND") == 0) {
            
            // Verifica se il comando è destinato a questo nodo
            if (strcmp(receivedData.node, nodeId.c_str()) == 0 || strlen(receivedData.node) == 0 || strcmp(receivedData.node, "CUCINA") == 0) {
                lastGatewayMessage = millis();
                gatewayConnectionLost = false;
                
                handleControlCommand(mac, &receivedData);
            }
            return;
        }
        
        // Gestione speciale per comando DISCOVERY dal gateway - bypassa il controllo gateway_id
        if ((strcmp(receivedData.topic, "DISCOVERY") == 0 && strcmp(receivedData.command, "DISCOVERY") == 0) ||
            (strcmp(receivedData.topic, "CONTROL") == 0 && strcmp(receivedData.command, "DISCOVERY") == 0)) {
            
            Serial.print("[DEBUG] Comando DISCOVERY ricevuto - targetGatewayId: ");
            Serial.println(targetGatewayId.c_str());
            Serial.print("[DEBUG] Gateway ID nel comando: ");
            Serial.println(receivedData.gateway_id);
            
            // Processa il comando DISCOVERY indipendentemente dal gateway_id
            lastGatewayMessage = millis();
            gatewayConnectionLost = false;
            
            handleControlCommand(mac, &receivedData);
            return;
        }
        

        

    }
}

// --- GESTIONE COMANDI (identica a Simple_Relay_Node) ---
void handleControlCommand(uint8_t* senderMac, struct_message* msg) {
    // Imposta flag protezione
    processingCommand = true;
    
    bool commandExecuted = false;
    const char* response = "UNKNOWN";
    
    // Gestione relè (relay_1, relay_2, relay_3, relay_4)
    if (strncmp(msg->topic, "relay_", 6) == 0) {
        int relayIndex = msg->topic[6] - '1'; // Converte '1','2','3','4' in 0,1,2,3
        
        if (relayIndex >= 0 && relayIndex < 4) {
            int command = atoi(msg->command); // Converte stringa in numero
            int relayPin = relayPins[relayIndex];
            
            // Esegue il comando
            switch (command) {
                case 0: // OFF
                    digitalWrite(relayPin, LOW);
                    relayStates[relayIndex] = false;
                    response = "OFF";
                    commandExecuted = true;
                    break;
                    
                case 1: // ON
                    digitalWrite(relayPin, HIGH);
                    relayStates[relayIndex] = true;
                    response = "ON";
                    commandExecuted = true;
                    break;
                    
                case 2: // SWITCH
                    relayStates[relayIndex] = !relayStates[relayIndex];
                    digitalWrite(relayPin, relayStates[relayIndex] ? HIGH : LOW);
                    response = relayStates[relayIndex] ? "ON" : "OFF";
                    commandExecuted = true;
                    break;
                    
                default:
                    Serial.println("Comando non valido (usa 0=OFF, 1=ON, 2=SWITCH)");
                    break;
            }
        }
    }
    // Comandi di sistema
    else if (strcmp(msg->topic, "Life") == 0) {
        response = "ALIVE";
        commandExecuted = true;
    }
    // Gestione PING di heartbeat dal gateway - RISPONDE CON PONG
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "PING") == 0) {
        
        // Risponde con PONG al gateway
        if (gatewayFound) {
            String relayStatus = "";
            for(int i=0; i<4; i++) relayStatus += relayStates[i] ? "1" : "0";
            String statusWithVersion = "ALIVE|" + String(FIRMWARE_VERSION) + "|" + relayStatus;
            espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "PONG", statusWithVersion.c_str(), "FEEDBACK", targetGatewayId.c_str());
        }
        processingCommand = false;
        return;
    }
    // Gestione richieste DISCOVERY dal gateway - RIPETE IL DISCOVERY COME ALL'AVVIO
    else if ((strcmp(msg->topic, "DISCOVERY") == 0 && strcmp(msg->command, "DISCOVERY") == 0) ||
             (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "DISCOVERY") == 0)) {
        Serial.println("Discovery request ricevuta dal gateway. Riavvio discovery come all'avvio...");
        Serial.println("[DEBUG] Forzatura discovery - cancellazione MAC e riavvio procedura");
        
        // Resetta lo stato del gateway e cancella il MAC salvato per forzare un nuovo discovery
        gatewayFound = false;
        gatewayConnectionLost = false;
        waitingForDiscoveryResponse = true;
        
        // Cancella il MAC salvato in LittleFS per forzare un nuovo discovery
        deleteGatewayMac();
        
        // Resetta le variabili di stato
        memset(gatewayMac, 0, 6);
        lastDiscoveryAttempt = 0;
        discoveryStartTime = millis();
        
        // Invia immediatamente la richiesta di discovery
        sendDiscoveryRequest();
        
        processingCommand = false;
        return;
    }
    // Comando RESTART - riavvia il nodo
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "RESTART") == 0) {
        response = "RESTARTING";
        commandExecuted = true;
        
        // Invia conferma prima del riavvio usando la libreria DomoticaEspNow
        if (gatewayFound) {
            espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "RESTART", "RESTARTING", "FEEDBACK", targetGatewayId.c_str());
        }
        
        // Usa un delay molto breve e poi gestisci il riavvio nel loop
        Serial.println("Restart programmato...");
        restartPending = true;
        restartTime = millis() + 200; // Riavvio dopo 200ms
    }
    // Comando FACTORY_RESET - resetta la configurazione e riavvia in modalità AP
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "FACTORY_RESET") == 0) {
        response = "RESETTING";
        commandExecuted = true;
        
        Serial.println("⚠️ COMANDO FACTORY_RESET RICEVUTO VIA ESP-NOW!");
        
        // Invia conferma ricezione (ACK)
        if (gatewayFound) {
            espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "FACTORY_RESET", "RESETTING", "FEEDBACK", targetGatewayId.c_str());
        }
        
        // Schedule factory reset - Il lavoro sporco lo fa il loop
        factoryResetPending = true;
        restartTime = millis() + 1000;
        
        // MANTENERE IL COMANDO PER DEBUG
        performFactoryReset();
    }
    // Comando OTA_UPDATE - avvia aggiornamento firmware
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "OTA_UPDATE") == 0) {
        // Payload format: SSID|PASS|URL
        String payload = String(msg->status);
        int firstPipe = payload.indexOf('|');
        int secondPipe = payload.indexOf('|', firstPipe + 1);
        
        if (firstPipe > 0 && secondPipe > 0) {
            otaSsid = payload.substring(0, firstPipe);
            otaPass = payload.substring(firstPipe + 1, secondPipe);
            otaUrl = payload.substring(secondPipe + 1);
            
            response = "OTA_STARTING";
            commandExecuted = true;
            
            // Invia conferma ricezione comando
            if (gatewayFound) {
                espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "OTA_UPDATE", "OTA_STARTING", "FEEDBACK", targetGatewayId.c_str());
            }
            
            otaPending = true;
            otaStartTime = millis() + 1000; // Avvio tra 1 secondo per dare tempo all'invio del feedback
        } else {
             response = "OTA_INVALID_PAYLOAD";
             commandExecuted = true;
             if (gatewayFound) {
                espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "OTA_UPDATE", "INVALID_PAYLOAD", "FEEDBACK", targetGatewayId.c_str());
            }
        }
    }
    // Comando SLEEP_STATUS - mostra informazioni sullo stato di sleep
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "SLEEP_STATUS") == 0) {
        response = "SLEEP_INFO";
        commandExecuted = true;
        Serial.println("SLEEP_STATUS ricevuto - Invio informazioni power management...");
        
        // Invia informazioni di stato
        if (gatewayFound) {
            char sleepInfo[128];
            snprintf(sleepInfo, sizeof(sleepInfo), "uptime:%lu|last_activity:%lu|mode:light_sleep", 
                     millis(), millis() - lastMessageReceived);
            espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "SLEEP_STATUS", sleepInfo, "FEEDBACK", targetGatewayId.c_str());
        }
    }
    // Comando NETWORK_DISCOVERY - forza un nuovo discovery
    else if (strcmp(msg->topic, "CONTROL") == 0 && strcmp(msg->command, "NETWORK_DISCOVERY") == 0) {
        response = "DISCOVERY_STARTED";
        commandExecuted = true;
        Serial.println("NETWORK_DISCOVERY ricevuto - Avvio discovery...");
        
        // Resetta lo stato del gateway e forza un nuovo discovery
        gatewayFound = false;
        waitingForDiscoveryResponse = true;
        lastDiscoveryAttempt = 0; // Forza invio immediato
        discoveryStartTime = millis();
        
        // Invia conferma
        if (gatewayFound) {
            espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "NETWORK_DISCOVERY", "DISCOVERY_STARTED", "FEEDBACK", targetGatewayId.c_str());
        }
        
        // Invia immediatamente la richiesta di discovery
        sendDiscoveryRequest();
    }
    else {
        // Topic non riconosciuto
    }
    
    if (commandExecuted) {
        // Per i comandi relay, invia lo stato effettivo invece del comando originale
        if (strncmp(msg->topic, "relay_", 6) == 0) {
            // Determina lo stato del relè specifico per inviarlo come comando confermato
            int relayIndex = msg->topic[6] - '1';
            const char* actualCommandState = "0";
            if (relayIndex >= 0 && relayIndex < 4) {
                actualCommandState = relayStates[relayIndex] ? "1" : "0";
            }
            
            // Invia lo stato effettivo di TUTTI i relay per aggiornare gli attributi su HA
            String relayStatus = "";
            for(int i=0; i<4; i++) relayStatus += relayStates[i] ? "1" : "0";
            
            // Usa actualCommandState invece di msg->command per confermare lo stato reale
            sendGatewayFeedback(msg->topic, actualCommandState, relayStatus.c_str());
        } else {
            // Per altri comandi mantieni la struttura originale
            sendGatewayFeedback(msg->topic, msg->command, "COMPLETED");
        }
        
        // Breve feedback LED senza delay
        digitalWrite(LED_STATUS, LOW);  // Accendi LED
        ledOnTime = millis(); // Imposta timestamp per spegnimento automatico
    }
    
    // Reset flag protezione
    processingCommand = false;
}

// --- FUNZIONI DI COMUNICAZIONE ---
void sendDiscoveryRequest() {
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    espNow.send(broadcastAddress, nodeId.c_str(), "DISCOVERY", "REQUEST", targetGatewayId.c_str(), "DISCOVERY", targetGatewayId.c_str());
    lastActivity = millis(); // Aggiorna attività reale
}

void sendGatewayRegistration() {
    if (gatewayFound) {
        // Formato status: NODE_TYPE|FIRMWARE_VERSION
        String registrationStatus = String(NODE_TYPE) + "|" + String(FIRMWARE_VERSION);
        
        char regBuffer[256];
        snprintf(regBuffer, sizeof(regBuffer), "INVIATO - (\"node\":\"%s\")(\"topic\":\"CONTROL\")(\"command\":\"%s\")(\"status\":\"%s\")(\"type\":\"REGISTRATION\")(\"gateway_id\":\"%s\")",
                 nodeId.c_str(), NODE_TYPE, registrationStatus.c_str(), targetGatewayId.c_str());
        Serial.println(regBuffer);
        
        // Debug: mostra MAC prima dell'invio
         printMacAddress(gatewayMac, "[DEBUG] Invio registrazione a MAC: ");
         Serial.print("Registration Payload: ");
         Serial.println(registrationStatus);
        
        if (!espNow.hasPeer(gatewayMac)) { espNow.addPeer(gatewayMac); }
        
        espNow.send(gatewayMac, nodeId.c_str(), "CONTROL", "REGISTER", registrationStatus.c_str(), "REGISTRATION", targetGatewayId.c_str());
        lastActivity = millis(); // Aggiorna attività reale
        
        // Nota: La versione è ora inclusa nel messaggio di registrazione.
        // Il messaggio di heartbeat aggiuntivo è stato rimosso in favore di questo metodo più pulito.
    } else {
        Serial.println("[DEBUG] Gateway non trovato - registrazione non inviata");
    }
}

void sendGatewayFeedback(const char* topic, const char* command, const char* status) {
    if (gatewayFound) {
        // Debug: mostra MAC prima dell'invio
         printMacAddress(gatewayMac, "[DEBUG] Invio feedback a MAC: ");
        
        if (!espNow.hasPeer(gatewayMac)) { espNow.addPeer(gatewayMac); }
        
        espNow.send(gatewayMac, nodeId.c_str(), topic, command, status, "FEEDBACK", targetGatewayId.c_str());
        lastActivity = millis(); // Aggiorna attività reale
    } else {
        Serial.println("[DEBUG] Gateway non trovato - feedback non inviato");
    }
}

// --- GESTIONE LED NON BLOCCANTE ---
void manageLedFeedback() {
    static unsigned long lastLedToggle = 0;
    static bool ledState = false;
    unsigned long currentTime = millis();
    
    // Spegnimento automatico dopo feedback comando
    if (ledOnTime > 0 && currentTime - ledOnTime >= LED_FEEDBACK_DURATION) {
        digitalWrite(LED_STATUS, HIGH); // Spegni LED
        ledOnTime = 0; // Reset timestamp
    }
    
    // Indicatore stato gateway/sleep
    if (!configMode) {
        if (!gatewayFound) {
            // Lampeggio rapido quando gateway non trovato
            if (currentTime - lastLedToggle >= 200) {
                ledState = !ledState;
                digitalWrite(LED_STATUS, ledState ? LOW : HIGH);
                lastLedToggle = currentTime;
            }
        } else {
            // Lampeggio lento quando gateway trovato
            if (currentTime - lastLedToggle >= 1000) {
                ledState = !ledState;
                digitalWrite(LED_STATUS, ledState ? LOW : HIGH);
                lastLedToggle = currentTime;
            }
        }
        
        // LED spento quando in light sleep da più di 3 secondi
        if (currentTime - lastMessageReceived > 3000 && ledOnTime == 0) {
            digitalWrite(LED_STATUS, HIGH); // LED spento (attivo basso)
        }
    }
}

// --- SETUP ---
void setup() {
    // Early init to prevent relay glitch on boot
    // Inizializza immediatamente i pin di default a LOW per evitare attivazioni spurie
    for(int i=0; i<4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
    }

    // Inizializzazione Watchdog Timer (8 secondi)
    ESP.wdtEnable(8000);
    
    Serial.begin(115200);
    Serial.println("\n=== CONFIGURABLE RELAY NODE STARTUP ===");
    Serial.printf("Version: %s (Build: %s %s)\n", FIRMWARE_VERSION, BUILD_DATE, BUILD_TIME);
    
    // Tenta di applicare la configurazione hardcoded (se abilitata)
    applyHardcodedConfig();
    
    // Controlla se esiste una configurazione salvata (Carica PIN prima di usarli)
    bool configExists = loadConfiguration();

    // Configurazione pin
    pinMode(SETUP_PIN, INPUT_PULLUP);
    for(int i=0; i<4; i++) pinMode(relayPins[i], OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
    
    // Stato iniziale: tutti i relè spenti
    int initialState = RELAY_ACTIVE_LOW ? HIGH : LOW;
    for(int i=0; i<4; i++) digitalWrite(relayPins[i], initialState);
    digitalWrite(LED_STATUS, HIGH); // LED spento (attivo basso)
    
    Serial.println("Pin configurati - Tutti i relè spenti");
    
    // Configura light sleep per risparmio energetico
    wifi_set_sleep_type(LIGHT_SLEEP_T);
    
    if (configExists) {
        Serial.println("\n📡 CONFIGURAZIONE TROVATA - MODALITÀ OPERATIVA ESP-NOW");
        configMode = false;
    } else {
        Serial.println("\n🔧 PRIMA CONFIGURAZIONE - MODALITÀ SETUP AUTOMATICA");
        Serial.println("💡 Nessuna configurazione trovata, avvio modalità AP");
        configMode = true;
    }
    
    if (configMode) {
        // Avvia modalità AP senza password
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID);  // AP aperto senza password
        
        Serial.println("=== ACCESS POINT ATTIVO ===");
        Serial.print("SSID: "); Serial.println(AP_SSID);
        Serial.println("Password: NESSUNA (AP Aperto)");
        Serial.print("IP: "); Serial.println(WiFi.softAPIP());
        Serial.println("==============================");
        
        // Avvia DNS server per portale captive
        dnsServer.start(53, "*", WiFi.softAPIP());
        
        // Avvia webserver con portale captive
        server.on("/", handleRoot);
        server.on("/save", HTTP_POST, handleSave);
        // Portale captive - reindirizza qualsiasi richiesta alla pagina principale
        server.onNotFound(handleRoot);
        server.begin();
        
        Serial.println("Webserver avviato - Apri http://" + WiFi.softAPIP().toString() + " nel browser");
        
        // LED lampeggiante per indicare modalità configurazione
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_STATUS, LOW);
            delay(100);
            digitalWrite(LED_STATUS, HIGH);
            delay(100);
        }
        
    } else {
        Serial.print("Node ID: "); Serial.println(nodeId);
        Serial.print("Target Gateway: "); Serial.println(targetGatewayId);
        Serial.print("Node Type: "); Serial.println(NODE_TYPE);
        Serial.print("Version: "); Serial.println(FIRMWARE_VERSION);
        
        // Inizializzazione WiFi per ESP-NOW
        WiFi.mode(WIFI_STA);
        Serial.print("Node MAC Address: ");
        Serial.println(WiFi.macAddress());
        
        // Inizializzazione ESP-NOW
        espNow.begin(false);
        DomoticaEspNow::onDataReceived(OnDataRecv);
        
        // Aggiungi peer broadcast
        uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        espNow.addPeer(broadcastAddress);
        
        Serial.println("ESP-NOW inizializzato");
        
        // Debug: mostra stato iniziale MAC gateway
        Serial.print("[DEBUG] Stato iniziale MAC Gateway: ");
        
        // Prova a caricare il MAC address salvato
        if (loadGatewayMac()) {
            Serial.println("MAC gateway caricato da LittleFS - NESSUN discovery richiesto");
            
            // Aggiungi il peer e invia heartbeat iniziale
            if (!espNow.hasPeer(gatewayMac)) { espNow.addPeer(gatewayMac); }
            
            Serial.print("Sending Firmware Version from version.h: ");
            Serial.println(FIRMWARE_VERSION);
            
            // Invia REGISTRATION più volte per assicurare la ricezione
            // Aumentato delay iniziale e aggiunto ciclo di ritrasmissione
            delay(500); 
            for(int i=0; i<3; i++) {
                Serial.printf("Tentativo invio registrazione %d/3...\n", i+1);
                sendGatewayRegistration();
                delay(200);
            }
            
        } else {
            Serial.println("MAC gateway non trovato - Avvio discovery...");
            sendDiscoveryRequest();
            lastDiscoveryAttempt = millis();
            discoveryStartTime = millis();
            waitingForDiscoveryResponse = true;
        }
        
        // Configura modem sleep per risparmio energetico (alternativa stabile al light sleep)
        WiFi.setSleepMode(WIFI_MODEM_SLEEP);
        Serial.println("Modem sleep configurato - Consumo: ~15mA (alternativa stabile)");
        Serial.println("Comandi disponibili: status, mac, resetmac, restart, help");
        Serial.println("  - status: mostra stato sistema");
        Serial.println("  - mac: mostra MAC gateway salvato");
        Serial.println("  - resetmac: cancella MAC gateway da LittleFS");
        Serial.println("  - restart: riavvia il modulo");
        Serial.println("  - help: mostra elenco completo comandi");
        
        lastActivity = millis(); // Inizializza attività
    }
    
    Serial.println("=== SETUP COMPLETATO ===");
    
    // Resetta il watchdog timer per evitare timeout durante l'operazione
    ESP.wdtEnable(3000); // Imposta watchdog a 3 secondi
}

// --- LOOP PRINCIPALE ---
void loop() {
    // Reset del Watchdog Timer ad ogni ciclo
    ESP.wdtFeed();
    
    // Gestione transizione da modalità AP a modalità operativa
    static bool espNowInitialized = false;
    if (!configMode && !espNowInitialized) {
        Serial.println("\n🔄 Inizializzazione ESP-NOW dopo configurazione...");
        
        // Carica la configurazione appena salvata
        if (loadConfiguration()) {
            Serial.print("Node ID: "); Serial.println(nodeId);
            Serial.print("Target Gateway: "); Serial.println(targetGatewayId);
            
            // Inizializzazione WiFi per ESP-NOW
            WiFi.mode(WIFI_STA);
            Serial.print("Node MAC Address: ");
            Serial.println(WiFi.macAddress());
            
            // Inizializzazione ESP-NOW
            espNow.begin(false);
            DomoticaEspNow::onDataReceived(OnDataRecv);
            
            // Aggiungi peer broadcast
            uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            espNow.addPeer(broadcastAddress);
            
            Serial.println("ESP-NOW inizializzato");
            
            // Prova a caricare il MAC address salvato
            if (loadGatewayMac()) {
                Serial.println("MAC gateway caricato da LittleFS - Invio registrazione...");
                delay(500);
                sendGatewayRegistration();
                delay(500);
                sendGatewayRegistration();
            } else {
                Serial.println("MAC gateway non trovato - Avvio discovery...");
                sendDiscoveryRequest();
                lastDiscoveryAttempt = millis();
                discoveryStartTime = millis();
                waitingForDiscoveryResponse = true;
            }
            
            // Configura modem sleep
            WiFi.setSleepMode(WIFI_MODEM_SLEEP);
            Serial.println("Modem sleep configurato");
            
            espNowInitialized = true;
        } else {
             Serial.println("❌ Errore caricamento configurazione - ritorno modalità AP");
             
             // Cancella MAC gateway per forzare nuovo discovery
             deleteGatewayMac();
             gatewayFound = false;
             waitingForDiscoveryResponse = false;
             
             configMode = true;
             espNowInitialized = false; // Reset per permettere reinizializzazione
         }
    }
    
    // Controlla reset fabbrica (GPIO0 + GND) a runtime
    static bool resetButtonPressed = false;
    static unsigned long resetButtonPressTime = 0;
    static bool ledState = false;
    
    bool currentResetState = (digitalRead(SETUP_PIN) == LOW);
    
    if (currentResetState && !resetButtonPressed) {
        // Pulsante appena premuto
        resetButtonPressed = true;
        resetButtonPressTime = millis();
        Serial.println("🔘 GPIO0 collegato a GND - Reset fabbrica in corso...");
        Serial.println("💡 Tenere premuto per 5 secondi");
    }
    else if (currentResetState && resetButtonPressed) {
        // Pulsante ancora premuto - lampeggia LED
        unsigned long pressDuration = millis() - resetButtonPressTime;
        
        if (pressDuration >= 5000) { // 5 secondi
            Serial.println("\n🏭 RESET FABBRICA CONFERMATO!");
            
            // Invia comando di REMOVE_PEER al Gateway prima di cancellare tutto
            // Questo rimuove immediatamente il nodo dalla dashboard
            if (gatewayFound) {
                 Serial.println("👋 Invio comando REMOVE_PEER al Gateway...");
                 // Send: Node, Topic, Command, Status, Type, GatewayID
                 // Topic: SYSTEM, Command: REMOVE_PEER, Status: LEAVING, Type: STATUS
                 espNow.send(gatewayMac, nodeId.c_str(), "SYSTEM", "REMOVE_PEER", "LEAVING", "STATUS", targetGatewayId.c_str());
                 delay(200); // Attesa tecnica per assicurare l'invio fisico del pacchetto
            }

            Serial.println("Cancellazione configurazione e riavvio...");
            
            // Cancella configurazione e MAC gateway
            if (LittleFS.begin()) {
                if (LittleFS.exists("/config.json")) {
                    LittleFS.remove("/config.json");
                    Serial.println("✅ Configurazione cancellata");
                }
                deleteGatewayMac(); // Cancella anche il MAC address salvato
            }
            
            // Programma il riavvio in modo asincrono
            factoryResetPending = true;
            restartTime = millis() + 100; // Riavvio dopo 100ms
        } else {
            // Lampeggia LED durante la pressione
            static unsigned long lastLedToggle = 0;
            if (millis() - lastLedToggle > 200) { // Lampeggia ogni 200ms
                ledState = !ledState;
                digitalWrite(LED_STATUS, ledState ? LOW : HIGH); // LED attivo basso
                lastLedToggle = millis();
            }
        }
    }
    else if (!currentResetState && resetButtonPressed) {
        // Pulsante rilasciato
        resetButtonPressed = false;
        digitalWrite(LED_STATUS, HIGH); // Spegni LED (attivo basso)
        ledState = false;
        
        unsigned long pressDuration = millis() - resetButtonPressTime;
        if (pressDuration < 5000) {
            Serial.println("⚠️ Reset annullato - tenere premuto per 5 secondi");
        }
    }

    // --- GESTIONE HEARTBEAT UNICAST ---
    if (espNowInitialized && !configMode && !resetButtonPressed && gatewayFound) {
        if (millis() - lastHeartbeatTime > HEARTBEAT_INTERVAL) {
            lastHeartbeatTime = millis();
            Serial.println("💓 Invio Heartbeat Unicast al Gateway...");
            String statusWithVersion = "ONLINE|" + String(FIRMWARE_VERSION);
            espNow.send(gatewayMac, nodeId.c_str(), "STATUS", "HEARTBEAT", statusWithVersion.c_str(), "FEEDBACK", targetGatewayId.c_str());
        }
    }
    
    if (configMode) {
        // Modalità configurazione - nessun sleep
        dnsServer.processNextRequest();
        server.handleClient();
        
        // Controlla se la configurazione è stata salvata
        if (configSaved) {
            Serial.println("\n✅ Configurazione salvata - uscita da modalità AP...");
            
            // Cancella MAC gateway per forzare nuovo discovery con il nuovo gateway
            deleteGatewayMac();
            gatewayFound = false;
            waitingForDiscoveryResponse = false;
            Serial.println("🗑️ MAC gateway cancellato - nuovo discovery richiesto");
            
            configSaved = false; // Reset flag
            
            // Riavvio forzato per applicare la configurazione in modo pulito
            // La transizione soft da AP a STA può causare problemi di connessione
            delay(1000); // Attendi completamento invio risposta web
            safeRestart("Configurazione aggiornata");
        }
        
        // Timeout configurazione
        static unsigned long configStartTime = millis();
        if (millis() - configStartTime > CONFIG_TIMEOUT) {
            Serial.println("\n⏰ Timeout configurazione - riavvio in modalità operativa");
            restartPending = true;
            restartTime = millis() + 100; // Riavvio dopo 100ms
        }
        lastActivity = millis(); // Aggiorna attività
        
    } else {
        // Modalità operativa normale con light sleep
        unsigned long currentTime = millis();
        
        // Gestione riavvio pendente
        if (restartPending && currentTime >= restartTime) {
            safeRestart("Comando RESTART ricevuto");
        }
        
        // Gestione reset di fabbrica pendente
        if (factoryResetPending && currentTime >= restartTime) {
            // Il reset è già stato avviato da performFactoryReset() nella gestione del comando
            // Ma per sicurezza se siamo qui significa che il restart non è ancora avvenuto
            performFactoryReset();
        }
        
        // Gestione OTA pendente
        if (otaPending && currentTime >= otaStartTime) {
            performOTA();
        }
        
        // Gestione discovery del gateway - SOLO all'avvio o su comando
        // Rimosso il ciclo automatico ogni 10 secondi
        
        // Il discovery continua in background - nessun timeout forzato
        // Se il gateway non viene trovato, il nodo continua a funzionare
        // e può essere riconfigurato tramite factory reset se necessario
        
        // Gestione comandi seriale
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            if (command == "status") {
                printSystemStatus();
            } else if (command == "mac") {
                if (gatewayFound) {
                    printMacAddress(gatewayMac, "MAC Gateway: ");
                } else {
                    Serial.println("Gateway non trovato");
                }
            } else if (command == "factoryreset") {
                performFactoryReset();
            } else if (command == "restart") {
                Serial.println("🔄 Riavvio richiesto via seriale...");
                restartPending = true;
                restartTime = millis() + 100;
            } else if (command == "help") {
                Serial.println("\n=== COMANDI SERIALI DISPONIBILI ===");
                Serial.println("status    - Mostra stato completo del sistema");
                Serial.println("mac       - Mostra MAC address del gateway salvato");
                Serial.println("factoryreset - Reset totale (cancella WiFi/Config) e riavvia in AP");
                Serial.println("restart   - Riavvia il modulo");
                Serial.println("help      - Mostra questo elenco comandi");
                Serial.println("=====================================");
            } else {
                Serial.println("Comando non riconosciuto. Usa 'help' per lista comandi.");
            }
        }
        
        // Gestione LED feedback
        manageLedFeedback();
        
        // Gestione sleep mode dinamico (alternativa stabile)
        static bool sleepModeActive = false;
        unsigned long inactivityTime = currentTime - lastActivity;
        
        if (inactivityTime > 5000) {
            // Modalità deep sleep simulation - delay più lungo ma sicuro
            if (!sleepModeActive) {
                Serial.println("Modalità risparmio energetico attiva...");
                sleepModeActive = true;
            }
            delay(500); // Delay sicuro che non triggera watchdog
        } else {
            if (sleepModeActive) {
                Serial.println("");
                Serial.println("Modalità normale - alta reattività");
                sleepModeActive = false;
            }
            if (inactivityTime > 2000) {
                // Modalità moderata risparmio
                delay(200);
            } else {
                // Modalità normale - alta reattività
                delay(50);
            }
        }
        
        // Non aggiornare lastActivity qui - viene già gestito dagli eventi
    }
    
    delay(50); // Delay ridotto per reattività
}

// --- FUNZIONI UTILITY ---
void performOTA() {
    Serial.println("\n=== AVVIO PROCEDURA OTA ===");
    
    // Chiudi LittleFS per sicurezza
    LittleFS.end();
    
    // Disabilita watchdog per sicurezza durante la connessione
    ESP.wdtDisable();
    
    // Configura WiFi in modalità Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    Serial.print("Connessione a WiFi: "); Serial.println(otaSsid);
    WiFi.begin(otaSsid.c_str(), otaPass.c_str());
    
    unsigned long startConnect = millis();
    bool connected = false;
    
    // Attesa connessione (max 30 secondi)
    while (millis() - startConnect < 30000) {
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
        delay(500);
        Serial.print(".");
        ESP.wdtFeed(); // Nutri il watchdog se abilitato (anche se disabilitato sopra, male non fa)
    }
    
    if (!connected) {
        Serial.println("\n❌ Errore: Timeout connessione WiFi per OTA");
        safeRestart("OTA WiFi Timeout");
        return;
    }
    
    Serial.println("\n✅ WiFi Connesso!");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    Serial.println("Avvio download firmware da: " + otaUrl);
    
    // Necessario per ESP8266httpUpdate
    WiFiClient client;
    
    // Disabilita auto-reboot per gestire noi il post-update
    ESPhttpUpdate.rebootOnUpdate(false);
    
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, otaUrl);
    
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("❌ HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            safeRestart("OTA Failed");
            break;
            
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("⚠️ HTTP_UPDATE_NO_UPDATES");
            safeRestart("OTA No Updates");
            break;
            
        case HTTP_UPDATE_OK:
            Serial.println("✅ HTTP_UPDATE_OK - Aggiornamento completato con successo!");
            Serial.println("Riavvio del sistema...");
            delay(1000);
            ESP.restart();
            break;
    }
}

void safeRestart(const char* reason) {
    Serial.print("Safe restart - Motivo: ");
    Serial.println(reason);
    
    // Pulizia rapida delle risorse
    Serial.println("Pulizia risorse prima del restart...");
    
    LittleFS.end();
    
    // Resetta il watchdog timer
    ESP.wdtDisable();
    
    // Spegni tutti i relè
    for(int i=0; i<4; i++) {
        digitalWrite(relayPins[i], LOW);
    }
    
    // Spegni LED
    digitalWrite(LED_STATUS, HIGH);
    
    // Disconnetti WiFi
    WiFi.disconnect();
    
    // Delay minimo necessario
    delay(100);
    
    // Riavvio sicuro
    ESP.restart();
}

void printSystemStatus() {
    Serial.println("\n=== STATO SISTEMA ===");
    Serial.print("Modalità: "); Serial.println(configMode ? "CONFIGURAZIONE" : "OPERATIVA");
    Serial.printf("Versione Firmware: %s (Build: %s %s)\n", FIRMWARE_VERSION, BUILD_DATE, BUILD_TIME);
    Serial.print("Node ID: "); Serial.println(nodeId);
    Serial.print("Tipo Nodo: "); Serial.println(NODE_TYPE);
    Serial.print("MAC Nodo: "); Serial.println(WiFi.macAddress());
    Serial.print("Canale WiFi: "); Serial.println(WiFi.channel());
    Serial.print("Gateway Target: "); Serial.println(targetGatewayId);
    Serial.print("Gateway trovato: "); Serial.println(gatewayFound ? "SI" : "NO");
    Serial.print("Connessione gateway: "); Serial.println(gatewayConnectionLost ? "PERSA" : "OK");
    Serial.print("Uptime: "); Serial.println(millis());
    Serial.print("Ultimo messaggio: "); Serial.println(millis() - lastMessageReceived);
    
    if (gatewayFound) {
        Serial.print("MAC Gateway: ");
        printMacAddress(gatewayMac, "");
    } else {
        Serial.println("MAC Gateway: NON DEFINITO");
    }
    
    Serial.println("\n=== STATO RELÈ ===");
    for (int i = 0; i < 4; i++) {
        Serial.print("Relè "); Serial.print(i+1); Serial.print(": ");
        Serial.println(relayStates[i] ? "ACCESO" : "SPENTO");
    }
    Serial.println("===================");
}

void printPowerStatus() {
    Serial.println("[POWER] Modem sleep attivo - Consumo: ~15-20mA");
    Serial.print("[POWER] Ultima attività: "); Serial.println(millis() - lastMessageReceived);
}

void resetGatewayConnection() {
    gatewayFound = false;
    gatewayConnectionLost = false;
    waitingForDiscoveryResponse = false;
    lastGatewayMessage = 0;
    Serial.println("Connessione gateway resettata");
}
