// ESP8266 Gateway
// Sketch per un gateway ESP8266 che gestisce ESP-NOW e MQTT.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DomoticaEspNow.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <time.h> // NTP support

#include "Config.h"
#include "PeerHandler.h"
#include "MqttHandler.h"
#include "EspNowHandler.h"
#include "WebHandler.h"
#include "version.h"
#include "HaDiscovery.h"
#include "NodeTypeManager.h"
#include "WebLog.h"

const char* BUILD_DATE = __DATE__;
const char* BUILD_TIME = __TIME__;

// --- CONFIGURAZIONE --- //


// --- OGGETTI GLOBALI --- //
// wifiClient is defined in MqttHandler.cpp
// mqttClient definition is in MqttHandler.cpp
ESP8266WebServer configServer(80);
DNSServer dnsServer;

struct_message myData; // For sending
struct_message receivedData; // For receiving
DomoticaEspNow espNow;

// --- FLAGS --- //
volatile bool newEspNowData = false;
volatile bool sendPeerListFlag = false;  // Flag per inviare lista peer nel loop principale
bool configExists = false;  // Flag per indicare se esiste configurazione in LittleFS
bool otaRunning = false;    // Flag per indicare se è in corso un aggiornamento OTA
bool stationWebServerActive = false; // Flag per indicare se il web server station è attivo

unsigned long totalMessagesTimeout = 0;
unsigned long lastStatsReset = 0;

// --- RESET HARDWARE --- //
#define RESET_BUTTON_PIN 0 // GPIO0 - Pulsante reset hardware (FLASH button)
bool resetButtonPressed = false;
unsigned long resetButtonPressTime = 0;
const unsigned long RESET_HOLD_TIME = 5000; // 5 secondi per reset WiFi
const unsigned long FACTORY_RESET_TIME = 10000; // 10 secondi per factory reset

// --- LED FEEDBACK --- //
unsigned long lastLedBlink = 0;
bool ledState = false;

// Forward Declarations
void printGatewayStatus();
void handleSerialCommands();
void handleResetButton();

void debugLittleFSData() {
    // Funzione vuota - debug rimosso
}

void printGatewayStatus() {
    DevLog.println("\n🌐 === STATO GATEWAY ESP8266 ====");
    
    // Informazioni sistema
    DevLog.printf("📱 Gateway ID: %s\n", gateway_id);
    DevLog.printf("🔧 Versione Firmware: %s\n", FIRMWARE_VERSION);
    DevLog.printf("📅 Build: %s %s\n", BUILD_DATE, BUILD_TIME);
    DevLog.printf("⏱️ Uptime: %lu minuti\n", millis() / 60000);
    DevLog.printf("💾 Heap libero: %d bytes\n", ESP.getFreeHeap());
    DevLog.printf("🧩 Frammentazione Heap: %u%%\n", ESP.getHeapFragmentation());
    DevLog.printf("📦 Max Blocco Heap: %u bytes\n", ESP.getMaxFreeBlockSize());
    
    // Informazioni WiFi
    DevLog.printf("\n📶 CONNESSIONE WIFI:\n");
    if (WiFi.status() == WL_CONNECTED) {
        DevLog.printf("   Stato: Connesso ✅\n");
        DevLog.printf("   SSID: %s\n", WiFi.SSID().c_str());
        DevLog.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
        DevLog.printf("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        DevLog.printf("   DNS: %s\n", WiFi.dnsIP().toString().c_str());
        DevLog.printf("   RSSI: %d dBm\n", WiFi.RSSI());
        DevLog.printf("   MAC: %s\n", WiFi.macAddress().c_str());
    } else {
        DevLog.printf("   Stato: Disconnesso ❌\n");
        DevLog.printf("   MAC: %s\n", WiFi.macAddress().c_str());
    }
    
    // Informazioni MQTT
    DevLog.printf("\n🔌 MQTT:\n");
    DevLog.printf("   Server: %s:%d\n", mqtt_server, mqtt_port);
    DevLog.printf("   Stato: %s\n", mqttClient.connected() ? "Connesso ✅" : "Disconnesso ❌");
    DevLog.printf("   Topic base: %s\n", mqtt_topic_prefix);
    
    // Informazioni ESP-NOW
    DevLog.printf("\n📡 ESP-NOW:\n");
    DevLog.printf("   Peer registrati: %d/%d\n", peerCount, MAX_PEERS);
    DevLog.printf("   Modalità rete: %s\n", network_mode);
    
    // Statistiche coda messaggi
    DevLog.printf("\n📊 CODA MESSAGGI:\n");
    printQueueStatus();
    
    DevLog.println("=================================");
}

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "status") {
            printGatewayStatus();
        } else if (command == "reset") {
            DevLog.println("🔄 Reset WiFi richiesto via seriale...");
            resetWiFiConfig();
        } else if (command == "restart") {
            DevLog.println("🔄 Riavvio gateway richiesto via seriale...");
            delay(1000);
            ESP.restart();
        } else if (command == "totalreset") {
            DevLog.println("🚨 Total reset richiesto via seriale...");
            totalReset();
        } else if (command == "peers") {
            printPeersList();
        } else if (command == "queue") {
            printQueueStatus();
        } else if (command == "config") {
            debugLittleFSData();
        } else if (command == "help") {
            DevLog.println("\n=== COMANDI SERIALI GATEWAY ====");
            DevLog.println("status     - Mostra stato completo del gateway");
            DevLog.println("reset      - Reset WiFi e riavvio in modalità AP");
            DevLog.println("restart    - Riavvia il gateway");
            DevLog.println("totalreset - Reset completo (cancella tutto)");
            DevLog.println("peers      - Mostra lista peer ESP-NOW");
            DevLog.println("queue      - Mostra stato coda messaggi");
            DevLog.println("config     - Mostra configurazione LittleFS");
            DevLog.println("help       - Mostra questo elenco comandi");
            DevLog.println("=================================");
        } else {
            DevLog.println("Comando non riconosciuto. Usa 'help' per lista comandi.");
        }
    }
}

void handleResetButton() {
    bool currentState = digitalRead(RESET_BUTTON_PIN) == LOW;
    
    if (currentState && !resetButtonPressed) {
        // Pulsante appena premuto
        resetButtonPressed = true;
        resetButtonPressTime = millis();
        DevLog.println("🔘 Pulsante reset premuto...");
    }
    else if (!currentState && resetButtonPressed) {
        // Pulsante rilasciato
        resetButtonPressed = false;
        digitalWrite(LED_BUILTIN, HIGH); // Spegni LED (logica invertita)
        ledState = false; // Reset stato LED
        unsigned long pressDuration = millis() - resetButtonPressTime;
        
        if (pressDuration >= FACTORY_RESET_TIME) {
            // Factory reset: cancella tutto
            DevLog.println("🚨 FACTORY RESET TRIGGERED BY BUTTON");
            totalReset();
        }
        else if (pressDuration >= RESET_HOLD_TIME) {
            // Reset solo WiFi, mantieni peers
            DevLog.println("🔄 WIFI RESET TRIGGERED BY BUTTON");
            forceSavePeers(); // Salva peers prima del reset
            resetWiFiConfig();
        }
        else {
            // Pressione troppo breve (< 5 sec) -> GLOBAL DISCOVERY / REFRESH
            DevLog.println("🔘 Short Press: Triggering Global Discovery...");
            triggerGlobalDiscovery();
            
            // Feedback LED rapido
            for(int i=0; i<3; i++) {
                digitalWrite(LED_BUILTIN, !ledState);
                delay(100);
                digitalWrite(LED_BUILTIN, ledState);
                delay(100);
            }
        }
    }
    else if (currentState && resetButtonPressed) {
            // Pulsante tenuto premuto - mostra feedback
            unsigned long currentDuration = millis() - resetButtonPressTime;
            
            if (currentDuration >= FACTORY_RESET_TIME) {
                // Oltre 10 secondi: LED lampeggia velocissimo a 100ms
                if (millis() - lastLedBlink >= 100) {
                    ledState = !ledState;
                    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH); 
                    lastLedBlink = millis();
                }
            } else if (currentDuration >= RESET_HOLD_TIME) {
                 // Da 5 a 10 secondi: LED fisso acceso
                 digitalWrite(LED_BUILTIN, LOW); // LED acceso fisso
            } else {
                // Primi 5 secondi: lampeggio normale a 300ms
                if (millis() - lastLedBlink >= 300) {
                    ledState = !ledState;
                    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
                    lastLedBlink = millis();
                }
            }
    }
}

void setup() {
    // Inizializzazione Watchdog Timer (8 secondi)
    ESP.wdtEnable(8000);

    // Init Serial via WebLog
    DevLog.begin(115200);
    delay(100);
    DevLog.println("\n\n==================================");
    DevLog.println("Booting ESP8266 Gateway");
    DevLog.printf("Reset Reason: %s\n", ESP.getResetReason().c_str());
    DevLog.printf("Reset Info: %s\n", ESP.getResetInfo().c_str());
    DevLog.printf("Versione Firmware: %s\n", FIRMWARE_VERSION);
    DevLog.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);

    // CANCELLA COMPLETAMENTE la flash WiFi dell'ESP8266 per evitare credenziali residue
    WiFi.persistent(false);  // Disabilita la persistenza automatica
    WiFi.disconnect(true);   // Disconnetti e cancella credenziali dalla flash
    WiFi.mode(WIFI_OFF);     // Spegni completamente il WiFi
    delay(100);
    
    // Reset completo configurazione IP statica
    WiFi.mode(WIFI_STA);
    WiFi.config(0U, 0U, 0U);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    DevLog.println("🧹 Flash WiFi ESP8266 e configurazione IP cancellate - Nessuna credenziale residua");

    // Configurazione pulsante reset hardware e LED
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Spegni LED (logica invertita)
    DevLog.println("🔘 Pulsante reset hardware configurato su GPIO0");
    DevLog.println("💡 LED integrato configurato");

    DevLog.print("Gateway MAC Address: ");
    DevLog.println(WiFi.macAddress());

    // Inizializza LittleFS
    if (!LittleFS.begin()) {
        DevLog.println("❌ Errore inizializzazione LittleFS");
        delay(1000);
        ESP.restart();
    }
    DevLog.println("✅ LittleFS inizializzato");

    // Inizializza NodeTypeManager
    if (!NodeTypeManager::begin()) {
        DevLog.println("⚠️ Warning: Failed to initialize Node Types configuration");
    } else {
        DevLog.println("✅ Node Types configuration loaded");
    }

    // Configura le rotte web
    setupWebRoutes();

    // Controlla se esiste configurazione in LittleFS
    configExists = LittleFS.exists("/config.json");
    
    // Carica configurazione salvata solo se non in modalità FORCE_HARDCODED_CONFIG
#if !FORCE_HARDCODED_CONFIG
    if (configExists) {
        loadConfigFromLittleFS();
        DevLog.println("📄 Configurazione caricata da LittleFS");
        
        // Init NTP (Fallback to pool.ntp.org if empty)
        if (strlen(ntp_server) == 0) strcpy(ntp_server, "pool.ntp.org");
        if (gmt_offset_sec == 0) gmt_offset_sec = 3600; // Safe default
        
        configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
        DevLog.printf("🕒 NTP Configured: %s, GMT: %ld, DST: %d\n", ntp_server, gmt_offset_sec, daylight_offset_sec);
        
    } else {
        DevLog.println("📄 Nessuna configurazione trovata - Uso valori di default");
        
        // Init NTP with Defaults
        configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
        DevLog.printf("🕒 NTP Default Config: %s, GMT: %ld, DST: %d\n", ntp_server, gmt_offset_sec, daylight_offset_sec);
    }
#else
    saveConfigToLittleFS();
#endif

    // VALIDAZIONE CRITICA GATEWAY ID
    // Se l'ID è vuoto o nullo, imposta il default.
    // NOTA: Se loadConfigFromLittleFS ha fallito, gateway_id sarà vuoto (se inizializzato a "").
    if (strlen(gateway_id) == 0 || strcmp(gateway_id, "null") == 0) {
        DevLog.println("⚠️ Gateway ID non valido o nullo! Imposto: GATEWAY_MAIN");
        strcpy(gateway_id, "GATEWAY_MAIN");
        // Salva il fix per rendere la modifica persistente
        saveConfigToLittleFS();
    }
    
    if (strlen(mqtt_topic_prefix) == 0) {
        DevLog.println("⚠️ Topic Prefix vuoto! Imposto default: domoriky");
        strcpy(mqtt_topic_prefix, "domoriky");
    }
    
    DevLog.printf("🔧 ID Attivo: %s\n", gateway_id);
    DevLog.printf("📡 Topic Prefix: %s\n", mqtt_topic_prefix);

    // Controllo pulsante reset all'avvio (richiede pressione prolungata di 5 secondi)
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        // Verifica che il pulsante rimanga premuto per 5 secondi
        bool resetConfirmed = true;
        for (int i = 0; i < 50; i++) { // 50 x 100ms = 5 secondi
            delay(100);
            if (digitalRead(RESET_BUTTON_PIN) == HIGH) {
                resetConfirmed = false;
                break;
            }
        }
        
        if (resetConfirmed && digitalRead(RESET_BUTTON_PIN) == LOW) {
            resetWiFiConfig();
        }
    }

#if !FORCE_HARDCODED_CONFIG
    // MODALITÀ INTELLIGENTE - Controlla se esiste configurazione salvata
    
    // Controlla se esiste configurazione in LittleFS E se contiene credenziali WiFi valide
    if (LittleFS.exists("/config.json") && wifi_credentials_loaded && strlen(saved_wifi_ssid) > 0) {
        
        // CONNESSIONE WIFI DIRETTA con dati salvati
        WiFi.mode(WIFI_STA);
        
        int attempts = 0;
        
        // Tentativo connessione WiFi diretta SOLO con credenziali da config.json
         if (wifi_credentials_loaded && strlen(saved_wifi_ssid) > 0) {
             // Configura IP solo se modalità statica e valori validi
             if (strcmp(network_mode, "static") == 0 && strlen(static_ip) > 0) {
                 IPAddress ip, gateway, subnet, dns;
                 if (ip.fromString(static_ip) && gateway.fromString(static_gateway) && subnet.fromString(static_subnet)) {
                     if (strlen(static_dns) > 0 && dns.fromString(static_dns)) {
                         WiFi.config(ip, gateway, subnet, dns);
                     } else {
                         WiFi.config(ip, gateway, subnet);
                     }
                 }
             }
             
             WiFi.begin(saved_wifi_ssid, saved_wifi_password);
         } else {
            // Avvia modalità AP per configurazione
            if (!startWebServer()) {
                DevLog.println("❌ Web server fallito - Riavvio");
                ESP.restart();
                delay(5000);
            }
            
            // Attendi che l'utente completi la configurazione
            DevLog.println("⏳ Attendi configurazione via web...");
            while (WiFi.status() != WL_CONNECTED) {
                configServer.handleClient();
                dnsServer.processNextRequest();
                delay(100);
            }
            
            DevLog.println("\n✅ WiFi configurato con successo via web");
            DevLog.println(WiFi.localIP());
            
            // Salva la configurazione aggiornata
            saveConfigToLittleFS();
            
            // Ferma il web server (AP) e avvia in modalità Station
            stopWebServer();
            startStationWebServer();
            stationWebServerActive = true;
        }
        
        // Attendi connessione (max 20 secondi)
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            DevLog.print(".");
            attempts++;
        }
        DevLog.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            // CONNESSIONE RIUSCITA - Modalità WebSocket attiva
            startStationWebServer();
            stationWebServerActive = true;
        } else {
            // CONNESSIONE FALLITA - Avvia web server per riconfigurazione
            
            // Avvia il web server di configurazione
            if (!startWebServer()) {
                ESP.restart();
                delay(5000);
            }
            
            // Attendi che l'utente completi la configurazione
            while (WiFi.status() != WL_CONNECTED) {
                configServer.handleClient();
                dnsServer.processNextRequest();
                delay(100);
            }
            
            // Salva la configurazione aggiornata
            saveConfigToLittleFS();
            
            // Ferma il web server (AP) e avvia in modalità Station
            stopWebServer();
            startStationWebServer();
        }
        
    } else {
        // Avvia il web server di configurazione
        if (!startWebServer()) {
            delay(3000);
            ESP.restart();
            delay(5000);
        }
        
        // Attendi che l'utente completi la configurazione
        while (WiFi.status() != WL_CONNECTED) {
            configServer.handleClient();
            dnsServer.processNextRequest();
            delay(100);
        }
        
        DevLog.println("\n✅ WiFi configurato con successo");
        
        // Salva la configurazione
        saveConfigToLittleFS();
        
        // Ferma AP e passa a Station
        stopWebServer();
        startStationWebServer();
        stationWebServerActive = true;
    }
#else
    // MODALITÀ HARDCODED
    WiFi.mode(WIFI_STA);
    if (strcmp(network_mode, "static") == 0) {
        IPAddress ip, gateway, subnet, dns;
        ip.fromString(static_ip);
        gateway.fromString(static_gateway);
        subnet.fromString(static_subnet);
        if (strlen(static_dns) > 0 && dns.fromString(static_dns)) {
             WiFi.config(ip, gateway, subnet, dns);
        } else {
             WiFi.config(ip, gateway, subnet);
        }
    }
    WiFi.begin(wifi_ssid, wifi_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        DevLog.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        startStationWebServer();
    }
#endif

    // Setup MQTT
    setupMQTT();
    
    // Tentativo connessione immediata se WiFi è connesso
    if (WiFi.status() == WL_CONNECTED) {
        connectToMQTT();
    }
    
    // Setup ESP-NOW
    espNow.begin(true); // Initialize as Master/Controller
    // Force COMBO role to ensure we can both SEND and RECEIVE
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    
    // ADD BROADCAST PEER (Critical for Discovery)
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espNow.addPeer(broadcastAddress);
    
    DevLog.println("✅ ESP-NOW Initialized");
    
    // Carica peer salvati
    loadPeersFromLittleFS();
    
    // Registra callback ESP-NOW
    espNow.onDataReceived(OnDataRecv);
    
    // Configura OTA
    ArduinoOTA.setHostname(gateway_id);
    ArduinoOTA.onStart([]() {
        String type = "";
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        DevLog.println("Start updating " + type);
        otaRunning = true;
    });
    ArduinoOTA.onEnd([]() {
        DevLog.println("\nEnd");
        otaRunning = false;
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        DevLog.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DevLog.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            DevLog.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            DevLog.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            DevLog.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            DevLog.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            DevLog.println("End Failed");
        }
        otaRunning = false;
    });
    ArduinoOTA.begin();
    
    DevLog.println("✅ Gateway Pronto");
    printGatewayStatus();
}

void loop() {
    // Gestione OTA
    ArduinoOTA.handle();

    // Se l'OTA è in corso, SALTA tutto il resto per dare priorità all'upload
    if (otaRunning) {
        return;
    }

    // Reset del Watchdog Timer ad ogni ciclo
    ESP.wdtFeed();

    // SEMPRE gestire le richieste web (per OTA manager e config)
    configServer.handleClient();

    // Gestione web server in modalità configurazione
    if (!wifi_credentials_loaded || WiFi.status() != WL_CONNECTED) {
        stationWebServerActive = false; // Reset flag se disconnesso
        dnsServer.processNextRequest();
        
        // LED lampeggio rapido in modalità configurazione
        if (millis() - lastLedBlink > 200) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            lastLedBlink = millis();
        }
        
        delay(100);
    } else {
        // Assicura che il web server station sia attivo se connesso
        if (!stationWebServerActive) {
            startStationWebServer();
            stationWebServerActive = true;
        }

        // Solo se le credenziali sono caricate e WiFi connesso, procedi con MQTT
        // Gestione MQTT
        if (!mqttConnected) {
            unsigned long currentTime = millis();
            if (currentTime - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
                lastMqttReconnectAttempt = currentTime;
                connectToMQTT();
            }
        } else {
            // Esegui il loop MQTT e verifica lo stato della connessione
            if (!mqttClient.loop()) {
                DevLog.println("❌ Connessione MQTT persa");
                mqttConnected = false;
            } else {
                // Se MQTT risulta connesso ma il client non riceve traffico da tempo, forza ping
                static unsigned long lastMqttPing = 0;
                const unsigned long MQTT_PING_INTERVAL = 60000; // 60s
                unsigned long now = millis();
                if (now - lastMqttPing >= MQTT_PING_INTERVAL) {
                    // Pubblica heartbeat availability del gateway per tenere viva la sessione
                    mqttClient.publish((String(mqtt_topic_prefix)+"/gateway/availability").c_str(), "online", true);
                    lastMqttPing = now;
                }
            }
        }
        
        // Processa coda messaggi ESP-NOW
        processMessageQueue();

        // Processa dati ESP-NOW pronti per invio MQTT
        processEspNowData();
        
        // Gestione ping network
        processPingLogic();
        
        // Gestione timeout comandi
        processNodeCommandTimeout();
        
        // Gestione nodi offline - Controllo Heartbeat
        processOfflineCheck();
        
        // Gestione network discovery
        processNetworkDiscovery();
        processPeerListSending(); // Non-blocking peer listing
        
        // LED feedback - Pattern Intelligente
        if (!resetButtonPressed) {
            if (led_enabled) {
                unsigned long currentMillis = millis();
                if (mqttConnected) {
                    // Status: OPERATIONAL (Heartbeat Pulse)
                    // Breve flash ogni 3 secondi per indicare "Tutto OK"
                    static unsigned long lastPulseTime = 0;
                    const unsigned long PULSE_INTERVAL = 3000;
                    const unsigned long PULSE_DURATION = 50; // Molto breve (50ms)
                    
                    if (currentMillis - lastPulseTime >= PULSE_INTERVAL) {
                        digitalWrite(LED_BUILTIN, LOW); // Accendi (Active Low)
                        lastPulseTime = currentMillis;
                    } else if (currentMillis - lastPulseTime >= PULSE_DURATION) {
                        // Spegni il LED dopo la durata del pulse
                        if (digitalRead(LED_BUILTIN) == LOW) { 
                            digitalWrite(LED_BUILTIN, HIGH); // Spegni
                        }
                    }
                } else {
                    // Status: DISCONNECTED / ERROR
                    // Lampeggio costante 500ms
                    if (currentMillis - lastLedBlink > 500) {
                        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                        lastLedBlink = currentMillis;
                    }
                }
            } else {
                // LED Disabilitato dall'utente - Assicura che sia SPENTO
                digitalWrite(LED_BUILTIN, HIGH); // HIGH = SPENTO (Active Low)
            }
        }
        
        // Invio lista peer su richiesta
        if (sendPeerListFlag) {
            sendPeerListFlag = false;
            listPeers(); // This now just triggers the non-blocking process
        }
    }
    
    // Gestione pulsante reset (centralizzata)
    handleResetButton();
    
    // Gestione comandi seriali (sempre attivo)
    handleSerialCommands();
    
    // Gestione Riavvio Automatico
    if (auto_reboot_enabled) {
        static unsigned long lastRebootCheck = 0;
        if (millis() - lastRebootCheck > 10000) { // Check ogni 10 secondi
            lastRebootCheck = millis();
            
            time_t now = time(nullptr);
            if (now > 100000) { // Data valida
                struct tm * timeinfo = localtime(&now);
                if (timeinfo->tm_hour == auto_reboot_hour && timeinfo->tm_min == auto_reboot_minute) {
                    // Evita riavvio immediato dopo boot (uptime < 2 min)
                    if (millis() > 120000) {
                        DevLog.printf("🔄 AUTO REBOOT TRIGGERED at %02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min);
                        publishGatewayStatus("auto_reboot", "Scheduled daily reboot triggered", "REBOOT");
                        delay(1000);
                        ESP.restart();
                    }
                }
            }
        }
    }
    
    // Removed delay(10) to maximize loop speed
}
