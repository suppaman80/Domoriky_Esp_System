/*
 * UNIVERSAL_REMOTE - Console Domotica Universale ESP-NOW
 * 
 * DESCRIZIONE:
 * Evoluzione del 4_REMOTE_CONTROLLER.
 * - Gestisce molteplici nodi target (Luci, Tapparelle, ecc.)
 * - Interfaccia Grafica su OLED 0.96" (I2C)
 * - Input tramite Keypad TTP229 16 Tasti (I2C)
 * - Configurazione avanzata via Web Interface
 * 
 * HARDWARE:
 * - ESP8266 (Wemos D1 Mini / NodeMCU)
 * - Display OLED SSD1306 (SDA=D2, SCL=D1)
 * - Keypad TTP229 (SDA=D2, SCL=D1) - Condivide bus I2C
 * - Batteria 18650 + TP4056 + Partitore V-Bat (A0)
 * 
 * MAPPATURA TASTI (TTP229 4x4):
 * [1] [2] [3] [4]  -> 1=PREV Node, 2=NEXT Node, 3=Menu/Info, 4=Wake/Home
 * [5] [6] [7] [8]  -> Comandi Ch 1-4 (es. Luce 1, 2, 3, 4 o Tapparella SU/GIU)
 * [9] [10][11][12] -> Comandi Extra / Scene
 * [13][14][15][16] -> Opzioni / Setup
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DomoticaEspNow.h" // Libreria condivisa
#include "version.h"

extern "C" {
  #include "user_interface.h"
  #include "gpio.h"
}

// --- PIN CONFIGURATION (I2C Shared) ---
#define SDA_PIN 4 // D2
#define SCL_PIN 5 // D1
#define BAT_PIN A0

// --- OLED CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- DATA STRUCTURES ---
struct TargetNode {
  char name[16];      // Es. "SALOTTO"
  char type[16];      // Es. "LIGHT_4CH", "SHUTTER_2CH"
  uint8_t mac[6];     // Indirizzo MAC
  uint8_t minKey;     // Tasto minimo attivo (es. 5)
  uint8_t maxKey;     // Tasto massimo attivo (es. 8)
  bool valid;
};

#define MAX_NODES 10
struct Config {
  TargetNode nodes[MAX_NODES];
  int nodeCount;
};

Config config;
int currentNodeIndex = 0; // Indice del nodo attualmente selezionato a display

// --- GLOBALS ---
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;
DomoticaEspNow espNow;

unsigned long lastActivity = 0;
const int SLEEP_TIMEOUT = 15000; // 15 secondi

// --- PROTOTYPES ---
void setupHardware();
void loadConfig();
void saveConfig();
void updateDisplay();
void handleKeypad();
void sendCommand(int key) {
  TargetNode &node = config.nodes[currentNodeIndex];
  
  // 1. Verifica se il tasto è valido per questo dispositivo
  if (key < node.minKey || key > node.maxKey) {
    Serial.printf("Key %d ignored for %s (Range: %d-%d)\n", key, node.name, node.minKey, node.maxKey);
    // Feedback visivo "Divieto" su OLED
    display.fillRect(0, 56, 128, 8, BLACK);
    display.setCursor(0, 56);
    display.print("TASTO NON ATTIVO");
    display.display();
    delay(500);
    updateDisplay(); // Ripristina UI
    return;
  }

  // 2. Calcola indice comando (offset)
  // Se minKey=5, e premo 5 -> cmdIndex=1. Se premo 6 -> cmdIndex=2.
  int cmdIndex = key - node.minKey + 1;
  
  Serial.printf("Sending Command %d to %s...\n", cmdIndex, node.name);
  
  // Feedback Invio
  display.fillRect(0, 56, 128, 8, BLACK);
  display.setCursor(0, 56);
  display.printf("INVIO CMD %d...", cmdIndex);
  display.display();

  // 3. Logica Invio ESP-NOW (da integrare con libreria reale)
  // espNow.send(node.mac, node.name, topic, String(cmdIndex).c_str(), "REQUEST", "COMMAND", "");
  
  delay(200); // Mockup ritardo rete
  
  // Ripristina UI
  updateDisplay();
}

void setup() {
  Serial.println("\n\n=== UNIVERSAL REMOTE STARTUP ===");

  // Init I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Init OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("BOOTING...");
  display.display();

  // Load Config
  if (!LittleFS.begin()) Serial.println("LittleFS Fail");
  bool configLoaded = loadConfig();

  if (!configLoaded) {
    Serial.println("No Config Found -> Entering Config Mode");
    isConfigMode = true;
    // startConfigMode(); // Da implementare
  }

  // Init ESP-NOW
  WiFi.mode(WIFI_STA);
  espNow.begin();
  
  // TODO: Implementare TTP229 Init (se necessario configurare registri)
  
  lastActivity = millis();
  updateDisplay();
}

void loop() {
  // 1. Gestione Input TTP229
  handleKeypad();

  // 2. Check Sleep
  if (!isConfigMode && millis() - lastActivity > SLEEP_TIMEOUT) {
    enterDeepSleep();
  }
  
  // 3. Serial Debug / Config Trigger
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    lastActivity = millis(); // Reset sleep timer

    if (cmd == "help") {
      Serial.println("\n=== COMMANDS ===");
      Serial.println("status   - Show current configuration");
      Serial.println("restart  - Reboot");
      Serial.println("reset    - Factory reset (clears config)");
      Serial.println("config   - Enter AP Config Mode");
      Serial.println("================\n");
    }
    else if (cmd == "reset") {
      Serial.println("Clearing config...");
      if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
        Serial.println("Config deleted.");
      } else {
        Serial.println("Config file not found.");
      }
      Serial.println("Restarting...");
      delay(500);
      ESP.restart();
    }
    else if (cmd == "restart") {
      Serial.println("Restarting...");
      delay(100);
      ESP.restart();
    }
    else if (cmd == "config") {
      // startConfigMode(); // Da implementare
      Serial.println("Config Mode not implemented yet in this prototype.");
    }
  }
  
  delay(50); // Piccolo delay per stabilità
}

// --- LOGIC MOCKUP ---

void handleKeypad() {
  // Qui andrà la logica di lettura I2C del TTP229
  // Mockup: leggiamo dalla seriale per testare la logica UI
  /*
   * TTP229 I2C Read Logic:
   * Wire.requestFrom(0x57, 2);
   * uint16_t keyData = Wire.read() | (Wire.read() << 8);
   * ... process bitmask ...
   */
}

void updateDisplay() {
  display.clearDisplay();
  
  // Header: Device Name e Indice
  display.setTextSize(1);
  display.setCursor(0,0);
  display.printf("< %s > (%d/%d)", config.nodes[currentNodeIndex].name, currentNodeIndex+1, config.nodeCount);
  
  // Body: Tipo e Icona (Testuale per ora)
  display.setCursor(0, 16);
  display.setTextSize(2);
  display.println(config.nodes[currentNodeIndex].type);
  
  // Footer: Batteria (Mockup)
  display.setTextSize(1);
  display.setCursor(0, 56);
  int batLevel = analogRead(BAT_PIN); // Da convertire
  display.printf("BAT: %d", batLevel);
  
  display.display();
}

void enterDeepSleep() {
  display.clearDisplay();
  display.setCursor(20,20);
  display.println("ZZZ...");
  display.display();
  delay(500);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  
  // Configura Wakeup su pin interrupt del TTP229 (es. D3/GPIO0 o altro)
  // ESP.deepSleep(0); 
  // Nota: Il wakeup da DeepSleep richiede RST collegato a D0 per timer, 
  // o un segnale esterno su RST. Per tastiera, meglio Light Sleep con GPIO Wakeup.
  
  Serial.println("Entering Light Sleep...");
  // ... Copiare logica Light Sleep dal progetto precedente ...
}

// --- STORAGE ---
void loadConfig() {
  // Dummy Data per test
  config.nodeCount = 3;
  
  // Nodo 1: LUCI SALOTTO (4 Canali -> Tasti 5,6,7,8)
  strcpy(config.nodes[0].name, "SALOTTO");
  strcpy(config.nodes[0].type, "LIGHT_4CH");
  config.nodes[0].minKey = 5;
  config.nodes[0].maxKey = 8;
  
  // Nodo 2: LUCI CUCINA (4 Canali -> Tasti 5,6,7,8)
  strcpy(config.nodes[1].name, "CUCINA");
  strcpy(config.nodes[1].type, "LIGHT_4CH");
  config.nodes[1].minKey = 5;
  config.nodes[1].maxKey = 8;
  
  // Nodo 3: TAPPARELLA CAMERA (2 Canali -> Tasti 5,6)
  strcpy(config.nodes[2].name, "CAMERA");
  strcpy(config.nodes[2].type, "SHUTTER_2CH");
  config.nodes[2].minKey = 5;
  config.nodes[2].maxKey = 6;
}
