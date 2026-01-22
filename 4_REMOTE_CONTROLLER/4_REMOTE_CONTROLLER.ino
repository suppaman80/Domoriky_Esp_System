/*
 * 4_REMOTE_CONTROLLER - Telecomando WiFi ESP-NOW Deep Sleep
 * 
 * FUNZIONALITÀ:
 * - 4 Pulsanti per controllare 4 canali di un nodo remoto (es. 4_RELAY_CONTROLLER)
 * - Light Sleep per risparmio energetico e risveglio rapido da GPIO
 * - Auto-Discovery del nodo target tramite nome (WHOIS protocol)
 * - Configurazione AP Mode per impostare il nome del target e i pin
 * 
 * HARDWARE:
 * - ESP8266 (Wemos D1 Mini / NodeMCU)
 * - 4 Pulsanti collegati ai pin configurati (Default: D6, D7, D2, D1)
 * - Moduli Touch TTP223 (Configurazione standard) senza componenti extra
 * - LED di stato su D4 (Builtin LED)
 * - Jumper/Pulsante Config su D3 (GPIO0)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h> // Aggiunto per Web OTA
#include <DNSServer.h> // Aggiunto per Captive Portal
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "DomoticaEspNow.h" // Assicurati che la libreria sia installata
#include "version.h"

extern "C" {
  #include "user_interface.h"
  #include "gpio.h"
}

// --- PIN CONFIGURATION ---
const int LED_PIN = 2; // D4 (Builtin LED)
const int CONFIG_PIN = 0; // D3 (Flash Button)

// --- CONFIGURATION ---
struct Config {
  char targetNodeName[32];
  uint8_t targetMac[6];
  bool validMac;
  int btn1Pin;
  int btn2Pin;
  int btn3Pin;
  int btn4Pin;
};

Config config;
const char* CONFIG_FILE = "/config.json";

// --- GLOBALS ---
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater; // Istanza Updater
DNSServer dnsServer; // Istanza DNS Server
DomoticaEspNow espNow;
bool isConfigMode = false; // Flag per modalità configurazione
bool discoveryComplete = false;
unsigned long discoveryStartTime = 0;
const int DISCOVERY_TIMEOUT = 5000; // 5 secondi
bool ackReceived = false;
unsigned long lastActivity = 0; // Timer per sleep
int currentChannel = 1;         // Canale corrente
bool btnBlocked[] = {false, false, false, false, false}; // Stato blocco per i 4 pulsanti (indice 1-4)

// --- PROTOTYPES ---
void loadConfig();
void saveConfig();
void startConfigMode();
void handleButtonPress(int btnIndex);
void scanForTarget();
void onDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len);
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus);
void enterSleep();

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== 4 REMOTE CONTROLLER STARTUP (LIGHT SLEEP ENABLED) ===");
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED OFF (Active Low)
  
  // Init Buttons
  pinMode(config.btn1Pin, INPUT);
  pinMode(config.btn2Pin, INPUT);
  pinMode(config.btn3Pin, INPUT);
  pinMode(config.btn4Pin, INPUT);
  pinMode(CONFIG_PIN, INPUT_PULLUP);

  // Init LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
  }
  loadConfig();
  bool configLoaded = LittleFS.exists(CONFIG_FILE);

  // Check Config Mode (Hold Flash Button at boot OR No Config Found)
  if (digitalRead(CONFIG_PIN) == LOW || !configLoaded) {
    Serial.println(!configLoaded ? "No Config Found -> Entering AP Mode" : "Config Button Detected -> Entering AP Mode");
    isConfigMode = true;
    startConfigMode();
    return;
  }

  // Init WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  currentChannel = WiFi.channel();
  Serial.printf("WiFi Channel: %d\n", currentChannel);
  
  espNow.begin();
  espNow.onDataReceived(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // --- SAFETY CHECK FOR STUCK BUTTONS AT BOOT ---
  // If a button is held down during boot (e.g. after a reset), wait for it to be released
  // BEFORE entering the main loop. This prevents immediate re-triggering of "Long Press Reset".
  Serial.println("Checking for stuck buttons...");
  unsigned long safeStart = millis();
  
  // Check individually to block specific stuck buttons
  int stuckCount = 0;
  
  // Helper lambda or macro could be used, but explicit checks are clear
  if (config.btn1Pin != -1 && digitalRead(config.btn1Pin) == HIGH) { stuckCount++; }
  if (config.btn2Pin != -1 && digitalRead(config.btn2Pin) == HIGH) { stuckCount++; }
  if (config.btn3Pin != -1 && digitalRead(config.btn3Pin) == HIGH) { stuckCount++; }
  if (config.btn4Pin != -1 && digitalRead(config.btn4Pin) == HIGH) { stuckCount++; }

  if (stuckCount > 0) {
      Serial.println("Buttons detected active at boot. Waiting 2s for release...");
      while(millis() - safeStart < 2000) {
         stuckCount = 0;
         if (config.btn1Pin != -1 && digitalRead(config.btn1Pin) == HIGH) stuckCount++;
         if (config.btn2Pin != -1 && digitalRead(config.btn2Pin) == HIGH) stuckCount++;
         if (config.btn3Pin != -1 && digitalRead(config.btn3Pin) == HIGH) stuckCount++;
         if (config.btn4Pin != -1 && digitalRead(config.btn4Pin) == HIGH) stuckCount++;
         
         if(stuckCount == 0) break; // All released
         delay(100);
         yield();
      }
      
      // If still stuck after wait, BLOCK them
      if (config.btn1Pin != -1 && digitalRead(config.btn1Pin) == HIGH) { Serial.println("Btn1 Stuck -> Blocked"); btnBlocked[1] = true; }
      if (config.btn2Pin != -1 && digitalRead(config.btn2Pin) == HIGH) { Serial.println("Btn2 Stuck -> Blocked"); btnBlocked[2] = true; }
      if (config.btn3Pin != -1 && digitalRead(config.btn3Pin) == HIGH) { Serial.println("Btn3 Stuck -> Blocked"); btnBlocked[3] = true; }
      if (config.btn4Pin != -1 && digitalRead(config.btn4Pin) == HIGH) { Serial.println("Btn4 Stuck -> Blocked"); btnBlocked[4] = true; }
  }
  Serial.println("Safety check complete.");

  lastActivity = millis();
  Serial.println("Ready. Waiting for buttons...");
}

void loop() {
  // --- CONFIG MODE HANDLER ---
  if (isConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Blink LED slowly to indicate AP Mode
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000) {
      lastBlink = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    return; // SKIP ALL OTHER LOGIC IN CONFIG MODE
  }

  // Check Buttons (TTP223 Active High: Pressed = HIGH)
  int pressedBtn = -1;
  
  if (config.btn1Pin != -1 && digitalRead(config.btn1Pin) == HIGH) pressedBtn = 1;
  else if (config.btn2Pin != -1 && digitalRead(config.btn2Pin) == HIGH) pressedBtn = 2;
  else if (config.btn3Pin != -1 && digitalRead(config.btn3Pin) == HIGH) pressedBtn = 3;
  else if (config.btn4Pin != -1 && digitalRead(config.btn4Pin) == HIGH) pressedBtn = 4;

  // Check Blocked Buttons Recovery
  for(int i=1; i<=4; i++) {
    if(btnBlocked[i]) {
      int pin = -1;
      if(i==1) pin = config.btn1Pin;
      else if(i==2) pin = config.btn2Pin;
      else if(i==3) pin = config.btn3Pin;
      else if(i==4) pin = config.btn4Pin;
      
      if(digitalRead(pin) == LOW) {
        btnBlocked[i] = false; // Reset block if pin goes LOW
        Serial.printf("Button %d Recovered (Unblocked)\n", i);
      } else {
        if(pressedBtn == i) pressedBtn = -1; // Ignore if still blocked
      }
    }
  }

  if (pressedBtn > 0) {
    // Debounce
    delay(50);
    int confirmedBtn = -1;
    if (pressedBtn == 1 && digitalRead(config.btn1Pin) == HIGH) confirmedBtn = 1;
    else if (pressedBtn == 2 && digitalRead(config.btn2Pin) == HIGH) confirmedBtn = 2;
    else if (pressedBtn == 3 && digitalRead(config.btn3Pin) == HIGH) confirmedBtn = 3;
    else if (pressedBtn == 4 && digitalRead(config.btn4Pin) == HIGH) confirmedBtn = 4;
    
    if (confirmedBtn > 0) {
      lastActivity = millis(); // Reset sleep timer
      Serial.printf("Button %d Pressed!\n", confirmedBtn);
      handleButtonPress(confirmedBtn);
      lastActivity = millis(); // Reset again after action
      
      // Wait for release with Long Press Reset check
      Serial.println("Waiting for release...");
      int pin = -1;
      if (confirmedBtn == 1) pin = config.btn1Pin;
      else if (confirmedBtn == 2) pin = config.btn2Pin;
      else if (confirmedBtn == 3) pin = config.btn3Pin;
      else if (confirmedBtn == 4) pin = config.btn4Pin;
      
      unsigned long startWait = millis();
      bool timeoutOccurred = false;
      
      while(digitalRead(pin) == HIGH) {
        delay(10);
        yield();
        unsigned long pressDuration = millis() - startWait;
        
        // --- LONG PRESS RESET FEATURE ---
        // Se premuto per più di 6 secondi: Lampeggio rapido -> Reset Config -> Riavvio
        if (pressDuration > 6000) {
           Serial.println("!!! LONG PRESS DETECTED: FACTORY RESET INITIATED !!!");
           
           // Feedback Visivo: Lampeggio veloce LED
           for(int i=0; i<20; i++) {
             digitalWrite(LED_PIN, LOW); // ON
             delay(50);
             digitalWrite(LED_PIN, HIGH); // OFF
             delay(50);
           }
           
           // Cancellazione Configurazione
           Serial.println("Clearing config.json...");
           if (LittleFS.exists(CONFIG_FILE)) LittleFS.remove(CONFIG_FILE);
           
           Serial.println("Restarting into AP Mode...");
           delay(100);
           ESP.restart();
        }

        // Timeout di sicurezza per tasto incastrato (se non resetta per qualche motivo o se logica cambia)
        if (pressDuration > 12000) {
           timeoutOccurred = true;
           break; 
        }
      }
      
      if (timeoutOccurred) {
        Serial.printf("Button %d STUCK HIGH! Blocking input until release.\n", confirmedBtn);
        btnBlocked[confirmedBtn] = true;
      } else {
        delay(100); // Debounce release
      }
      
      lastActivity = millis(); // Reset again
    }
  }
  
  // Discovery retry if needed (every 10 seconds if no valid MAC)
  static unsigned long lastDiscovery = 0;
  if (!config.validMac && millis() - lastDiscovery > 10000) {
    lastDiscovery = millis();
    Serial.println("Auto-Discovery Retry...");
    scanForTarget();
    lastActivity = millis(); // Reset sleep timer if we did something
  }
  
  // Sleep Check (10 seconds timeout)
  if (millis() - lastActivity > 10000) {
      enterSleep();
      lastActivity = millis();
  }
  
  // Handle network callbacks
  yield();

  // --- SERIAL COMMANDS ---
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    lastActivity = millis(); // Reset sleep timer
    
    if (cmd == "help") {
      Serial.println("\n=== COMMANDS ===");
      Serial.println("status   - Show current configuration and state");
      Serial.println("restart  - Reboot the module");
      Serial.println("reset    - Factory reset (clears config)");
      Serial.println("scan     - Force new discovery scan");
      Serial.println("config   - Enter AP Config Mode manually");
      Serial.println("================\n");
    }
    else if (cmd == "status") {
      Serial.println("\n=== STATUS ===");
      Serial.printf("Target Name: %s\n", config.targetNodeName);
      Serial.printf("Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                    config.targetMac[0], config.targetMac[1], config.targetMac[2], 
                    config.targetMac[3], config.targetMac[4], config.targetMac[5]);
      Serial.printf("MAC Valid: %s\n", config.validMac ? "YES" : "NO");
      Serial.printf("Current Channel: %d\n", WiFi.channel());
      Serial.printf("Pins: B1=%d, B2=%d, B3=%d, B4=%d\n", 
                    config.btn1Pin, config.btn2Pin, config.btn3Pin, config.btn4Pin);
      Serial.println("================\n");
    }
    else if (cmd == "restart") {
      Serial.println("Restarting...");
      delay(100);
      ESP.restart();
    }
    else if (cmd == "reset") {
      Serial.println("Clearing config...");
      if (LittleFS.exists(CONFIG_FILE)) LittleFS.remove(CONFIG_FILE);
      Serial.println("Done. Restarting...");
      delay(500);
      ESP.restart();
    }
    else if (cmd == "scan") {
      config.validMac = false;
      scanForTarget();
    }
    else if (cmd == "config") {
      startConfigMode();
    }
    else {
      Serial.println("Unknown command. Type 'help' for list.");
    }
  }
}

// --- LOGIC ---

void enterSleep() {
  Serial.println("Entering Light Sleep (Wake on GPIO High)...");
  Serial.println("NOTE: Serial commands disabled during sleep. Press any button to wake.");
  delay(50); // Flush serial

  // Configure Wakeup Sources (TTP223 Active High)
  if (config.btn1Pin != -1) gpio_pin_wakeup_enable(GPIO_ID_PIN(config.btn1Pin), GPIO_PIN_INTR_HILEVEL);
  if (config.btn2Pin != -1) gpio_pin_wakeup_enable(GPIO_ID_PIN(config.btn2Pin), GPIO_PIN_INTR_HILEVEL);
  if (config.btn3Pin != -1) gpio_pin_wakeup_enable(GPIO_ID_PIN(config.btn3Pin), GPIO_PIN_INTR_HILEVEL);
  if (config.btn4Pin != -1) gpio_pin_wakeup_enable(GPIO_ID_PIN(config.btn4Pin), GPIO_PIN_INTR_HILEVEL);

  // Turn off WiFi to save power
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();

  while(true) {
      // Check if any button is pressed (Wakeup reason)
      if ((config.btn1Pin != -1 && digitalRead(config.btn1Pin)) || 
          (config.btn2Pin != -1 && digitalRead(config.btn2Pin)) || 
          (config.btn3Pin != -1 && digitalRead(config.btn3Pin)) || 
          (config.btn4Pin != -1 && digitalRead(config.btn4Pin))) {
          break;
      }
      
      // Sleep for max time (~268s) or until interrupt
      wifi_fpm_do_sleep(0xFFFFFFF);
      delay(10); // Enter sleep
  }

  Serial.println("Woke up!");
  
  // Restore WiFi
  wifi_fpm_close();
  wifi_set_opmode(STATION_MODE);
  wifi_set_sleep_type(NONE_SLEEP_T);
  
  WiFi.mode(WIFI_STA);
  if (currentChannel > 0) wifi_set_channel(currentChannel); // Restore channel
  
  espNow.begin();
  espNow.onDataReceived(onDataRecv);
  esp_now_register_send_cb(onDataSent);
}

void handleButtonPress(int btnIndex) {
  // Check if we have a valid target MAC
  if (!config.validMac) {
    Serial.println("Target MAC unknown. Starting Discovery...");
    scanForTarget();
    
    if (!config.validMac) {
      Serial.println("Discovery Failed. Target not found.");
      return;
    }
  }

  // Send Command
  Serial.printf("Sending Command for Relay %d to Target...\n", btnIndex);
  
  // Register peer if needed
  if (!espNow.hasPeer(config.targetMac)) {
    espNow.addPeer(config.targetMac);
  }

  // Construct Message
  String topic = "relay_" + String(btnIndex);
  
  // Use Library Send
  ackReceived = false;
  // FIXED: Send Target Node Name in 'node' field
  // FIXED: Set 'status' to "REQUEST" to match Gateway protocol
  // Format: send(mac, node_name, topic, command, status, type, gateway_id)
  espNow.send(config.targetMac, config.targetNodeName, topic.c_str(), "2", "REQUEST", "COMMAND", "");
  
  // Wait for Send Callback & Feedback
  unsigned long waitStart = millis();
  while (!ackReceived && millis() - waitStart < 500) {
      delay(10);
      yield();
  }
  
  // Visual Feedback
  if (ackReceived) {
      digitalWrite(LED_PIN, LOW); // ON
      delay(200);
      digitalWrite(LED_PIN, HIGH); // OFF
  } else {
      // Short blink for sent but no feedback
      digitalWrite(LED_PIN, LOW); delay(50); digitalWrite(LED_PIN, HIGH);
  }
}

void scanForTarget() {
  Serial.printf("Scanning for '%s' on all channels...\n", config.targetNodeName);
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  for(int ch = 1; ch <= 13; ch++) {
    wifi_set_channel(ch);
    delay(20); // Tempo per cambio canale
    
    // Send WHOIS
    espNow.send(broadcastAddress, "REMOTE_01", "DISCOVERY", "WHOIS", config.targetNodeName, "DISCOVERY", "");
    
    // Wait for potential response
    unsigned long startWait = millis();
    while(millis() - startWait < 50) {
      yield(); // Permette gestione pacchetti in ingresso
      if(config.validMac) {
        Serial.printf("Target found on Channel %d!\n", ch);
        currentChannel = ch; // Update current channel
        return; 
      }
    }
  }
  Serial.println("Scan complete. Target not found.");
}

void onDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  struct_message msg;
  memcpy(&msg, incomingData, sizeof(msg));
  
  Serial.printf("Msg Received: Topic=%s, Cmd=%s\n", msg.topic, msg.command);
  
  // Check for Discovery Response
  if (strcmp(msg.topic, "DISCOVERY") == 0 && strcmp(msg.command, "HERE_I_AM") == 0) {
    Serial.println("Target Found!");
    memcpy(config.targetMac, mac, 6);
    config.validMac = true;
    saveConfig();
    discoveryComplete = true;
  }
  
  // Check for Command Feedback
  if (strcmp(msg.type, "FEEDBACK") == 0) {
    Serial.println("Feedback Received!");
    ackReceived = true;
  }
}

void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(sendStatus == 0 ? "Delivery Success" : "Delivery Fail");
}

// --- CONFIGURATION MODE ---
String getPinOptions(int selectedPin) {
  String s = "<option value='-1'" + String(selectedPin == -1 ? " selected" : "") + ">DISABLED</option>";
  int pins[] = {12, 13, 4, 5, 14, 0, 2, 15};
  const char* labels[] = {"D6 (Safe)", "D7 (Safe)", "D2 (Safe)", "D1 (Safe)", "D5 (Safe)", "D3 (Flash)", "D4 (Led)", "D8 (PullDown)"};
  
  for(int i=0; i<8; i++) {
    s += "<option value='" + String(pins[i]) + "'";
    if(pins[i] == selectedPin) s += " selected";
    s += ">GPIO " + String(pins[i]) + " - " + String(labels[i]) + "</option>";
  }
  return s;
}

void startConfigMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP_REMOTE_4CH_SETUP"); // AP aperto, nessuna password
  
  Serial.println("AP Started: ESP_REMOTE_4CH_SETUP");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  // Avvia DNS Server per Captive Portal (reindirizza tutto a questo IP)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Configura Web Updater (Path nascosto per la gestione POST)
  httpUpdater.setup(&server, "/ota_handler");

  // Pagina Custom per l'Update (Solo Firmware, UI Migliorata)
  server.on("/update", []() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Aggiornamento Firmware</title>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; text-align: center; }";
    html += "h1 { color: #333; margin-bottom: 20px; }";
    html += "input[type='file'] { width: 100%; padding: 10px; margin: 20px 0; border: 2px dashed #ddd; border-radius: 5px; box-sizing: border-box; }";
    html += ".btn { display: inline-block; background: #2196F3; color: white; padding: 15px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; text-decoration: none; width: 100%; box-sizing: border-box; margin-top: 10px; }";
    html += ".btn:hover { background: #1976D2; }";
    html += ".btn-back { background: #757575; margin-top: 10px; }";
    html += ".btn-back:hover { background: #616161; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🔄 Aggiornamento Firmware</h1>";
    html += "<p>Seleziona il file .bin compilato</p>";
    
    html += "<form method='POST' action='/ota_handler' enctype='multipart/form-data'>";
    html += "<input type='file' name='firmware' accept='.bin' required>";
    html += "<button type='submit' class='btn'>🚀 Avvia Aggiornamento</button>";
    html += "</form>";
    
    html += "<a href='/' class='btn btn-back'>⬅ Torna alla Configurazione</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
  });

  server.on("/", []() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Configurazione Remote Controller</title>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 20px; font-size: 24px; }";
    html += "label { display: block; margin: 10px 0 5px 0; font-weight: bold; color: #555; }";
    html += "input[type='text'], select { width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
    html += "input[type='text']:focus, select:focus { border-color: #4CAF50; outline: none; }";
    html += "button, .btn-link { background: #4CAF50; color: white; padding: 15px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; margin-top: 20px; display: block; text-align: center; text-decoration: none; box-sizing: border-box; }";
    html += "button:hover, .btn-link:hover { background: #45a049; }";
    html += ".btn-ota { background: #2196F3; margin-top: 10px; }";
    html += ".btn-ota:hover { background: #1976D2; }";
    html += ".info { background: #e7f3ff; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3; font-size: 14px; }";
    html += ".pin-group { background: #f9f9f9; padding: 10px; border-radius: 5px; margin-top: 10px; border: 1px solid #eee; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🎮 Configurazione Remote</h1>";
    html += "<p style='text-align: center; color: #666; font-size: 12px;'>FW: " + String(FIRMWARE_VERSION) + "</p>";
    
    html += "<div class='info'>";
    html += "<strong>Istruzioni:</strong><br>";
    html += "• Imposta il Nome del Nodo Target<br>";
    html += "• Assegna i GPIO ai Pulsanti<br>";
    html += "• Salva per riavviare<br>";
    html += "</div>";
    
    html += "<form action='/save' method='POST'>";
    
    html += "<label for='target'>Nome Nodo Target:</label>";
    html += "<input type='text' id='target' name='target' value='" + String(config.targetNodeName) + "' required>";
    
    html += "<div class='pin-group'>";
    html += "<h3 style='margin:5px 0 10px 0;color:#666'>Assegnazione Pulsanti</h3>";
    
    html += "<label for='btn1'>Pulsante 1 (Relay 1):</label>";
    html += "<select name='btn1' id='btn1'>" + getPinOptions(config.btn1Pin) + "</select>";
    
    html += "<label for='btn2'>Pulsante 2 (Relay 2):</label>";
    html += "<select name='btn2' id='btn2'>" + getPinOptions(config.btn2Pin) + "</select>";
    
    html += "<label for='btn3'>Pulsante 3 (Relay 3):</label>";
    html += "<select name='btn3' id='btn3'>" + getPinOptions(config.btn3Pin) + "</select>";
    
    html += "<label for='btn4'>Pulsante 4 (Relay 4):</label>";
    html += "<select name='btn4' id='btn4'>" + getPinOptions(config.btn4Pin) + "</select>";
    
    html += "</div>";
    
    html += "<button type='submit'>💾 Salva e Riavvia</button>";
    html += "</form>";
    
    html += "<a href='/update' class='btn-link btn-ota'>🔄 Aggiorna Firmware (OTA)</a>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
  });
  
  server.on("/save", []() {
    bool changed = false;
    if (server.hasArg("target")) {
      String t = server.arg("target");
      if (strcmp(t.c_str(), config.targetNodeName) != 0) {
        t.toCharArray(config.targetNodeName, sizeof(config.targetNodeName));
        config.validMac = false; // Reset MAC to force discovery
        changed = true;
      }
    }
    
    if (server.hasArg("btn1")) { config.btn1Pin = server.arg("btn1").toInt(); changed = true; }
    if (server.hasArg("btn2")) { config.btn2Pin = server.arg("btn2").toInt(); changed = true; }
    if (server.hasArg("btn3")) { config.btn3Pin = server.arg("btn3").toInt(); changed = true; }
    if (server.hasArg("btn4")) { config.btn4Pin = server.arg("btn4").toInt(); changed = true; }

    if (changed) {
      saveConfig();
      String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:Arial,sans-serif;margin:20px;text-align:center;}.container{max-width:500px;margin:0 auto;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}h1{color:#4CAF50;}</style></head><body><div class='container'><h1>✅ Configurazione Salvata!</h1><p>Il dispositivo si sta riavviando...</p></div></body></html>";
      server.send(200, "text/html", html);
      delay(1000);
      ESP.restart();
    } else {
      String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:Arial,sans-serif;margin:20px;text-align:center;}.container{max-width:500px;margin:0 auto;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}</style></head><body><div class='container'><h1>Nessuna Modifica</h1><p><a href='/'>Torna Indietro</a></p></div></body></html>";
      server.send(200, "text/html", html);
    }
  });
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  
  while(true) {
    dnsServer.processNextRequest(); // Gestisci richieste DNS
    server.handleClient();
    // Blink LED slowly to indicate Config Mode
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000) {
      lastBlink = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    yield();
  }
}

// --- STORAGE ---
void loadConfig() {
  // Default values
  strcpy(config.targetNodeName, "CUCINA"); // Default target
  memset(config.targetMac, 0, 6);
  config.validMac = false;
  config.btn1Pin = -1; // Default DISABLED
  config.btn2Pin = -1; // Default DISABLED
  config.btn3Pin = -1; // Default DISABLED
  config.btn4Pin = -1; // Default DISABLED
  
  if (LittleFS.exists(CONFIG_FILE)) {
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (f) {
      size_t size = f.size();
      std::unique_ptr<char[]> buf(new char[size]);
      f.readBytes(buf.get(), size);
      f.close();
      
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, buf.get());
      
      if (doc.containsKey("targetNodeName")) {
        strlcpy(config.targetNodeName, doc["targetNodeName"], sizeof(config.targetNodeName));
      }
      if (doc.containsKey("validMac")) {
        config.validMac = doc["validMac"];
      }
      if (doc.containsKey("targetMac")) {
        JsonArray mac = doc["targetMac"];
        for(int i=0; i<6; i++) config.targetMac[i] = mac[i];
      }
      if (doc.containsKey("btn1Pin")) config.btn1Pin = doc["btn1Pin"];
      if (doc.containsKey("btn2Pin")) config.btn2Pin = doc["btn2Pin"];
      if (doc.containsKey("btn3Pin")) config.btn3Pin = doc["btn3Pin"];
      if (doc.containsKey("btn4Pin")) config.btn4Pin = doc["btn4Pin"];
    }
  }
}

void saveConfig() {
  DynamicJsonDocument doc(1024);
  doc["targetNodeName"] = config.targetNodeName;
  doc["validMac"] = config.validMac;
  doc["btn1Pin"] = config.btn1Pin;
  doc["btn2Pin"] = config.btn2Pin;
  doc["btn3Pin"] = config.btn3Pin;
  doc["btn4Pin"] = config.btn4Pin;
  
  JsonArray mac = doc.createNestedArray("targetMac");
  for(int i=0; i<6; i++) mac.add(config.targetMac[i]);
  
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}