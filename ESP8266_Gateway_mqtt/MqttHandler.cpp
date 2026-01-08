#include "MqttHandler.h"
#include "PeerHandler.h"
#include "WebHandler.h"
#include "EspNowHandler.h"
#include "version.h"
#include "HaDiscovery.h"
#include "WebLog.h"
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>

// Global variables definition
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// External globals
extern const char* BUILD_DATE;
extern const char* BUILD_TIME;
extern DomoticaEspNow espNow;

// Define wifiClient and mqttClient
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// --- GESTIONE MQTT --- //
void onMqttConnect() {
    mqttConnected = true;
    
    DevLog.println("✅ MQTT: Connected Callback Triggered");
    
    // Sottoscrivi ai 4 topic richiesti
    String gatewayCommandTopic = String(mqtt_topic_prefix) + "/gateway/command";
    String nodeCommandTopic = String(mqtt_topic_prefix) + "/nodo/command";
    
    DevLog.printf("📡 Subscribing to: %s\n", gatewayCommandTopic.c_str());
    if (mqttClient.subscribe(gatewayCommandTopic.c_str())) {
        DevLog.println("   -> Subscription SUCCESS");
    } else {
        DevLog.println("   -> Subscription FAILED");
    }
    
    DevLog.printf("📡 Subscribing to: %s\n", nodeCommandTopic.c_str());
    if (mqttClient.subscribe(nodeCommandTopic.c_str())) {
        DevLog.println("   -> Subscription SUCCESS");
    } else {
        DevLog.println("   -> Subscription FAILED");
    }

    // NEW: Subscribe to Dashboard Status for Auto-Discovery
    String dashboardStatusTopic = String(mqtt_topic_prefix) + "/dashboard/status";
    DevLog.printf("📡 Subscribing to: %s\n", dashboardStatusTopic.c_str());
    mqttClient.subscribe(dashboardStatusTopic.c_str());

    // NEW: Send Discovery Request to force Dashboard to announce itself
    String dashboardDiscoveryTopic = String(mqtt_topic_prefix) + "/dashboard/discovery";
    mqttClient.publish(dashboardDiscoveryTopic.c_str(), "{\"command\":\"DISCOVER\"}");
    DevLog.printf("📡 Sent Dashboard Discovery Request to: %s\n", dashboardDiscoveryTopic.c_str());

    delay(200); // Small delay to ensure stability before sending heartbeat

    // NEW: Send Gateway Heartbeat immediately to announce version and status
    sendGatewayHeartbeat();

    // Pubblica discovery per tutti i peer conosciuti
    for (int i = 0; i < peerCount; i++) {
        HaDiscovery::publishDiscovery(mqttClient, peerList[i], mqtt_topic_prefix);
        HaDiscovery::publishDashboardConfig(mqttClient, peerList[i], mqtt_topic_prefix);
        
        // NEW: Publish current state immediately to remove "lightning bolt" icon
        publishPeerStatus(i, "HEARTBEAT"); 
        
        // Piccolo delay per evitare buffer overflow durante l'invio massivo
        delay(50);
    }
}

void onMqttDisconnect() {
    mqttConnected = false;
}

// Callback per messaggi MQTT ricevuti
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Converte payload in stringa
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    String topicStr = String(topic);
    String gatewayCommandTopic = String(mqtt_topic_prefix) + "/gateway/command";
    String nodeCommandTopic = String(mqtt_topic_prefix) + "/nodo/command";
    String dashboardStatusTopic = String(mqtt_topic_prefix) + "/dashboard/status";
    
    if (topicStr == gatewayCommandTopic) {
        processMqttCommand(message);
    } else if (topicStr == nodeCommandTopic) {
        processNodeCommand(message);
    } else if (topicStr == dashboardStatusTopic) {
        // Handle Dashboard Discovery & Status
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            if (doc.containsKey("status") && doc["status"] == "offline") {
                 discoveredDashboardIP = ""; // Clear IP to signal offline
                 DevLog.println("🖥️ Dashboard went OFFLINE (LWT)");
            } else if (doc.containsKey("ip")) {
                discoveredDashboardIP = doc["ip"].as<String>();
                lastDashboardSeen = millis();
                DevLog.printf("🖥️ Dashboard rilevata: %s\n", discoveredDashboardIP.c_str());
            }
        }
    }
}

// --- FUNZIONI MQTT DI BASE --- //
// Funzione per pubblicare stato del gateway: domoriky/gateway/status
void publishGatewayStatus(const String& eventType, const String& message, const String& command, const String& ip) {
    if (!mqttClient.connected()) {
        return;
    }
    
    // Topic unico per status del gateway: domoriky/gateway/status
    String topic = String(mqtt_topic_prefix) + "/gateway/status";
    
    // Crea il payload JSON
    DynamicJsonDocument doc(512);
    doc["eventType"] = eventType;
    doc["gatewayId"] = gateway_id;
    doc["message"] = message;
    doc["timestamp"] = millis();
    doc["MAC"] = WiFi.macAddress(); // Aggiungi MAC address del gateway
    doc["version"] = FIRMWARE_VERSION;
    doc["buildDate"] = String(BUILD_DATE) + " " + String(BUILD_TIME);
    
    // Aggiungi il campo command se specificato
    if (command.length() > 0) {
        doc["command"] = command;
    }
    
    // Aggiungi IP se specificato
    if (ip.length() > 0) {
        doc["ip"] = ip;
    }
    
    // Safety check: ensure gatewayId is never null or empty
    if (!doc["gatewayId"] || String(gateway_id).length() == 0 || String(gateway_id) == "null") {
        doc["gatewayId"] = "GATEWAY_02"; // Fallback to default
    }
    
    String payload;
    serializeJson(doc, payload);
    
    // Pubblica sul topic
    mqttClient.publish(topic.c_str(), payload.c_str());
}

// Funzione per pubblicare dati nodi: domoriky/nodo/status
void publishNodeStatus(const String& nodeId, const String& topic_name, const String& command, const String& status, const String& type) {
    if (!mqttClient.connected()) {
        return;
    }
    
    // Topic unico per status dei nodi: domoriky/nodo/status
    String topic = String(mqtt_topic_prefix) + "/nodo/status";
    
    // Crea il payload JSON
    DynamicJsonDocument doc(512);
    doc["Node"] = nodeId;
    doc["Topic"] = topic_name;
    doc["Command"] = command;
    doc["Status"] = status;
    doc["Type"] = type;
    doc["gatewayId"] = gateway_id;
    doc["timestamp"] = millis();
    
    // Aggiungi MAC address del nodo se disponibile
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == nodeId) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2],
                    peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
            doc["MAC"] = macStr;
            
            // Include current attributes state for value_template
            if (strlen(peerList[i].attributes) > 0) {
                doc["attributes"] = peerList[i].attributes;
            }
            
            break;
        }
    }
    
    String payload;
    serializeJson(doc, payload);
    
    // Pubblica sul topic
    mqttClient.publish(topic.c_str(), payload.c_str());
}

void publishNodeAvailability(const String& nodeId, const char* availability) {
    if (!mqttClient.connected()) {
        return;
    }
    String topic = String(mqtt_topic_prefix) + String("/nodo/") + nodeId + String("/availability");
    mqttClient.publish(topic.c_str(), availability, true);
}

void publishToMQTT(const String& subtopic, const String& eventType, const String& message) {
    // Reindirizza alla nuova funzione gateway
    publishGatewayStatus(eventType, message);
}

bool connectToMQTT() {
    if (mqttConnected) {
        return true;
    }
    
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(5);
    
    String clientId = "ESP8266Gateway_" + String(gateway_id);
    String willTopic = String(mqtt_topic_prefix) + String("/gateway/availability");
    const char* willMsg = "offline";
    
    bool connected = false;
    
    DevLog.print("🔌 Tentativo connessione MQTT a ");
    DevLog.print(mqtt_server);
    DevLog.print(":");
    DevLog.println(mqtt_port);
    
    if (strlen(mqtt_user) > 0 && strlen(mqtt_password) > 0) {
        DevLog.println("   Uso autenticazione user/pass");
        connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password,
                                      willTopic.c_str(), 0, true, willMsg);
    } else {
        DevLog.println("   Connessione anonima");
        connected = mqttClient.connect(clientId.c_str(), willTopic.c_str(), 0, true, willMsg);
    }
    
    if (connected) {
        DevLog.println("✅ MQTT Connesso!");
        // Birth message per availability gateway
        mqttClient.publish(willTopic.c_str(), "online", true);
        onMqttConnect();
        return true;
    } else {
        DevLog.print("❌ MQTT Errore connessione. Stato: ");
        DevLog.println(mqttClient.state());
        // Stati PubSubClient:
        // -4 : MQTT_CONNECTION_TIMEOUT
        // -3 : MQTT_CONNECTION_LOST
        // -2 : MQTT_CONNECT_FAILED
        // -1 : MQTT_DISCONNECTED
        // 1 : MQTT_CONNECTED
        // 2 : MQTT_CONNECT_BAD_PROTOCOL
        // 3 : MQTT_CONNECT_BAD_CLIENT_ID
        // 4 : MQTT_CONNECT_UNAVAILABLE
        // 5 : MQTT_CONNECT_BAD_CREDENTIALS
        // 6 : MQTT_CONNECT_UNAUTHORIZED
        return false;
    }
}

void reconnectMQTT() {
    unsigned long currentTime = millis();
    
    if (mqttConnected || (currentTime - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL)) {
        return;
    }
    
    lastMqttReconnectAttempt = currentTime;
    // Ri-crea il client WiFi per resilienza
    wifiClient.stop();
    delay(50);
    connectToMQTT();
}

void setupMQTT() {
    // IMPORTANTE: setBufferSize DEVE essere chiamato PRIMA di setServer
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqttClient.setKeepAlive(30); // seconds
    mqttClient.setSocketTimeout(5); // seconds
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(onMqttMessage);
}

void publishPeerStatus(int i, const char* command) {
    if (!mqttClient.connected()) return;
    
    unsigned long currentTime = millis();
    DynamicJsonDocument peerDoc(512);
    peerDoc["command"] = command;
    
    // Safety check for gatewayId
    if (String(gateway_id).length() == 0 || String(gateway_id) == "null") {
        peerDoc["gatewayId"] = "GATEWAY_02"; // Fallback
    } else {
        peerDoc["gatewayId"] = gateway_id;
    }
    
    peerDoc["timestamp"] = currentTime;
    peerDoc["index"] = i;
    peerDoc["totalNodes"] = peerCount;
    
    // Calcola nodi online per allineamento con PING_NETWORK_REPORT
    int onlineNodes = 0;
    for(int j=0; j<peerCount; j++) {
        if(peerList[j].isOnline) onlineNodes++;
    }
    peerDoc["onlineNodes"] = onlineNodes;
    
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2], 
             peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
    
    peerDoc["mac"] = macStr;
    peerDoc["nodeId"] = strlen(peerList[i].nodeId) > 0 ? peerList[i].nodeId : macStr;
    peerDoc["nodeType"] = strlen(peerList[i].nodeType) > 0 ? peerList[i].nodeType : "UNKNOWN";
    peerDoc["firmwareVersion"] = strlen(peerList[i].firmwareVersion) > 0 ? peerList[i].firmwareVersion : "UNKNOWN";
    peerDoc["attributes"] = strlen(peerList[i].attributes) > 0 ? peerList[i].attributes : "";
    peerDoc["status"] = peerList[i].isOnline ? "online" : "offline";
    
    // Aggiungi informazioni aggiuntive se disponibili
    if (peerList[i].lastSeen > 0) {
        unsigned long lastSeenAgo = currentTime - peerList[i].lastSeen;
        peerDoc["lastSeenMs"] = lastSeenAgo;
        peerDoc["lastSeen"] = String(lastSeenAgo / 1000) + "s fa";
    } else {
         peerDoc["lastSeenMs"] = -1;
         peerDoc["lastSeen"] = "MAI";
    }

    String peerResponse;
    serializeJson(peerDoc, peerResponse);
    
    // Topic unico per invio lista nodi e status updates
    String topic = String(mqtt_topic_prefix) + "/nodo/status";
    mqttClient.publish(topic.c_str(), peerResponse.c_str());
}

void sendGatewayHeartbeat() {
    unsigned long currentTime = millis();
    
    // Crea JSON per il heartbeat del gateway (aumentato buffer a 2048 bytes per evitare troncamenti)
    DynamicJsonDocument gatewayHeartbeatDoc(2048);
    gatewayHeartbeatDoc["command"] = "GATEWAY_HEARTBEAT";
    gatewayHeartbeatDoc["gatewayId"] = gateway_id;
    gatewayHeartbeatDoc["mac"] = WiFi.macAddress();
    gatewayHeartbeatDoc["status"] = "ALIVE";
    gatewayHeartbeatDoc["timestamp"] = currentTime;
    gatewayHeartbeatDoc["uptime"] = currentTime;
    gatewayHeartbeatDoc["freeHeap"] = ESP.getFreeHeap();
    gatewayHeartbeatDoc["wifiRSSI"] = WiFi.RSSI();
    gatewayHeartbeatDoc["peerCount"] = peerCount;
    
    // Aggiungi Versione Firmware
    gatewayHeartbeatDoc["version"] = FIRMWARE_VERSION;
    gatewayHeartbeatDoc["buildDate"] = String(BUILD_DATE) + " " + String(BUILD_TIME);
    
    // Aggiungi IP del gateway
    gatewayHeartbeatDoc["ip"] = WiFi.localIP().toString();
    
    // Aggiungi informazioni rete
    gatewayHeartbeatDoc["modalita_rete"] = network_mode;
    
    // Aggiungi informazioni peer (semplificato)
    JsonObject peerInfo = gatewayHeartbeatDoc.createNestedObject("peer");
    peerInfo["registrati"] = peerCount;
    peerInfo["massimo"] = MAX_PEERS;
    
    // Conta nodi online
    int onlineCount = 0;
    for (int i = 0; i < peerCount; i++) {
        if (peerList[i].isOnline) {
            onlineCount++;
        }
    }
    peerInfo["online"] = onlineCount;
    
    // Aggiungi informazioni MQTT
    JsonObject mqttInfo = gatewayHeartbeatDoc.createNestedObject("mqtt");
    mqttInfo["server"] = String(mqtt_server) + ":" + String(mqtt_port);
    mqttInfo["connected"] = mqttClient.connected();
    
    String heartbeatOutput;
    serializeJson(gatewayHeartbeatDoc, heartbeatOutput);
    
    // Topic unificato per status del gateway
    String topic = String(mqtt_topic_prefix) + "/gateway/status";
    
    // Pubblica sul topic unificato del gateway (RETAINED = true per persistenza stato)
    if (mqttClient.connected() && mqttClient.publish(topic.c_str(), heartbeatOutput.c_str(), true)) {
        DevLog.printf("Heartbeat gateway inviato al topic: %s\n", topic.c_str());
        // DevLog.println(heartbeatOutput); // Risparmia log
    } else {
        DevLog.println("Errore invio heartbeat gateway");
    }
}

void sendDashboardDiscovery() {
    if (!mqttClient.connected()) {
        DevLog.println("❌ Cannot send discovery: MQTT not connected");
        return;
    }
    String dashboardDiscoveryTopic = String(mqtt_topic_prefix) + "/dashboard/discovery";
    mqttClient.publish(dashboardDiscoveryTopic.c_str(), "{\"command\":\"DISCOVER\"}");
    DevLog.printf("📡 Manual Dashboard Discovery sent to: %s\n", dashboardDiscoveryTopic.c_str());
}

void performOTA(String url) {
    DevLog.println("Avvio OTA Update da: " + url);
    publishGatewayStatus("ota_start", "Starting OTA update from " + url, "OTA_UPDATE");
    
    // Assicura che il messaggio MQTT venga inviato prima di bloccare tutto
    mqttClient.loop();
    delay(200);
    mqttClient.loop();
    
    // Usa un client WiFi dedicato per l'aggiornamento per non interferire con MQTT
    WiFiClient updateClient;

    // Disabilita interrupts/watchdogs se necessario, ma ESPhttpUpdate gestisce molto da solo.
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW); 
    
    // Importante: usare updateClient separato
    t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            DevLog.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            publishGatewayStatus("ota_failed", "Error: " + ESPhttpUpdate.getLastErrorString(), "OTA_UPDATE");
            break;

        case HTTP_UPDATE_NO_UPDATES:
            DevLog.println("HTTP_UPDATE_NO_UPDATES");
            publishGatewayStatus("ota_no_updates", "No updates found", "OTA_UPDATE");
            break;

        case HTTP_UPDATE_OK:
            DevLog.println("HTTP_UPDATE_OK");
            publishGatewayStatus("ota_success", "Update successful, rebooting...", "OTA_UPDATE");
            break;
    }
}

void processMqttCommand(const String& msgStr) {
    DevLog.print("RICEVUTO - MQTT Command: ");
    DevLog.println(msgStr);

    // Variabile per tracciare se il comando è stato gestito
    bool commandHandled = false;

    // Parsing JSON per la nuova struttura semplificata
    DynamicJsonDocument doc(512);
    DeserializationError parseError = deserializeJson(doc, msgStr);
    
    if (parseError) {
        DevLog.println("Errore parsing JSON");
        publishGatewayStatus("error", "Invalid JSON format");
        return;
    }
    
    // Comandi per il GATEWAY - Struttura: {"command": "PING_NETWORK"}
    String command = "";
    if (doc.containsKey("command")) {
        command = doc["command"].as<String>();
    } else if (doc.containsKey("Command")) {
        command = doc["Command"].as<String>();
    }
    
    if (command.length() > 0) {
        
        DevLog.printf("Comando per GATEWAY - Command: %s\n", command.c_str());

        // Comando OTA_UPDATE
        if (command == "OTA_UPDATE") {
            DevLog.println("--- COMMAND: OTA_UPDATE RECEIVED ---");
            
            // Verifica che il comando sia per questo gateway specifico
            if (doc.containsKey("gatewayId")) {
                String targetId = doc["gatewayId"].as<String>();
                DevLog.printf("Target Gateway ID: %s (My ID: %s)\n", targetId.c_str(), gateway_id);
                
                if (targetId != String(gateway_id)) {
                    DevLog.println("Ignored: Command not for me.");
                    return; 
                }
            } else {
                DevLog.println("Warning: No gatewayId in command, proceeding anyway (broadcast?)");
            }

            if (doc.containsKey("url")) {
                String url = doc["url"].as<String>();
                performOTA(url);
            } else {
                DevLog.println("Error: Missing URL");
                publishGatewayStatus("error", "Missing URL for OTA_UPDATE", "OTA_UPDATE");
            }
            return;
        }

        // Comando GET_VERSION
        if (command == "GET_VERSION") {
            String message = "Version: " + String(FIRMWARE_VERSION) + ", Build: " + String(BUILD_DATE) + " " + String(BUILD_TIME);
            publishGatewayStatus("version_info", message, "GET_VERSION");
            return;
        }
        
        // Comando LIST_PEERS
        if (command == "LIST_PEERS") {
            listPeers();
            return;
        }

        // Comando RESET_WIFI_CONFIG
        if (command == "RESET_WIFI_CONFIG") {
            DevLog.println("Comando RESET_WIFI_CONFIG ricevuto via MQTT. Eseguo reset WiFi...");
            publishGatewayStatus("wifi_reset", "Resetting WiFi configuration and rebooting...", "RESET_WIFI_CONFIG");
            // Piccolo delay per permettere l'invio del messaggio MQTT
            delay(1000);
            resetWiFiConfig();
            return;
        }
        

        // Comando GATEWAY_HEARTBEAT
        if (command == "GATEWAY_HEARTBEAT") {
            sendGatewayHeartbeat();
            return;
        }
        
        // Comando NETWORK_DISCOVERY
        if (command == "NETWORK_DISCOVERY") {
            // Invia discovery broadcast
            uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            espNow.send(broadcastAddress, "GATEWAY", "DISCOVERY", "DISCOVERY", "REQUEST", "DISCOVERY", gateway_id);
            
            return;
        }
        
        // NOTA: Il comando NODE_REBOOT è ora gestito nella funzione processNodeCommand
        // per essere inviato al topic corretto: domoriky/nodo/command
        
        // Comando NODE_FACTORY_RESET - Factory reset di un nodo specifico
        if (command == "NODE_FACTORY_RESET") {
            if (doc.containsKey("nodeId")) {
                String targetNodeId = doc["nodeId"].as<String>();
                
                // Cerca il nodo nella lista dei peer
                bool nodeFound = false;
                for (int i = 0; i < peerCount; i++) {
                    if (String(peerList[i].nodeId) == targetNodeId) {
                        // Invia comando FACTORY_RESET al nodo specifico
                        espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "FACTORY_RESET", "", "COMMAND", gateway_id);
                        
                        // Invia conferma via WebSocket (o MQTT se necessario)
                        // In questo caso loggiamo solo
                        DevLog.printf("Factory reset sent to node %s\n", targetNodeId.c_str());
                        
                        nodeFound = true;
                        break;
                    }
                }
                
                if (!nodeFound) {
                    // Nodo non trovato nella lista peer - nessun messaggio MQTT
                }
            } else {
                // Parametro nodeId mancante - nessun messaggio MQTT
            }
            commandHandled = true;
            return;
        }
        
        // Comando NETWORK_REBOOT - Riavvia tutti i nodi nella rete
        if (command == "NETWORK_REBOOT") {
            DynamicJsonDocument responseDoc(512);
            responseDoc["command"] = "NETWORK_REBOOT";
            responseDoc["timestamp"] = millis();
            responseDoc["total_nodes"] = peerCount;
            
            if (peerCount > 0) {
                JsonArray rebootedNodes = responseDoc.createNestedArray("rebooted_nodes");
                int nodesSent = 0;
                
                // Invia comando RESTART individualmente a ciascun nodo (come sequenza di NODE_REBOOT)
                for (int i = 0; i < peerCount; i++) {
                    if (peerList[i].isOnline) {
                        // Invia comando RESTART al nodo specifico (come NODE_REBOOT)
                        espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "RESTART", "REQUEST", "COMMAND", gateway_id);
                        
                        // Aggiungi il nodo alla lista dei nodi riavviati
                        rebootedNodes.add(peerList[i].nodeId);
                        nodesSent++;
                        
                        // Piccolo delay tra un invio e l'altro
                        delay(50);
                    }
                }
                
                responseDoc["nodes_sent"] = nodesSent;
                
                DevLog.printf("Network Reboot initiated: %d nodes targeted\n", nodesSent);
            } else {
                responseDoc["error"] = "No peers found";
                DevLog.println("Network Reboot failed: No peers");
            }
            
            commandHandled = true;
            return;
        }
        
        // Comando REMOVE_PEER con formato JSON
        if (command == "REMOVE_PEER") {
            if (doc.containsKey("gatewayId")) {
                String requestedGatewayId = doc["gatewayId"];
                if (requestedGatewayId == gateway_id) {
                    if (doc.containsKey("mac")) {
                        String macAddress = doc["mac"];
                        removePeer(macAddress.c_str());
                    } else {
                        publishGatewayStatus("missing_parameter", "MAC address required for REMOVE_PEER command", "REMOVE_PEER");
                    }
                }
            }
            commandHandled = true;
            return;
        }
        
        // Comando CLEAR_PEERS con formato JSON
        if (command == "CLEAR_PEERS") {
            if (doc.containsKey("gatewayId")) {
                String requestedGatewayId = doc["gatewayId"];
                if (requestedGatewayId == gateway_id) {
                    clearAllPeers();
                }
                // Se gatewayId diverso, ignora silenziosamente
            } else {
                // Parametro gatewayId mancante - nessun messaggio MQTT
            }
            commandHandled = true;
            return;
        }
        
        // Comando RESET_TO_AP con formato JSON
        if (command == "RESET_TO_AP") {
            resetWiFiConfig();
            commandHandled = true;
            return;
        }
        
        // Comando CLEANUP_OFFLINE_PEERS - Rimuove tutti i peer offline
        if (command == "CLEANUP_OFFLINE_PEERS") {
            DevLog.println("RICEVUTO - Comando: CLEANUP_OFFLINE_PEERS");
            
            int offlineCount = 0;
            int totalCount = peerCount;
            
            // Conta i peer offline e crea lista temporanea
            for (int i = 0; i < peerCount; i++) {
                if (!peerList[i].isOnline) {
                    offlineCount++;
                }
            }
            
            if (offlineCount > 0) {
                DevLog.print("Trovati ");
                DevLog.print(offlineCount);
                DevLog.println(" peer offline da rimuovere");
                
                // Rimuove i peer offline spostando i peer online all'inizio
                int newPeerCount = 0;
                for (int i = 0; i < peerCount; i++) {
                    if (peerList[i].isOnline) {
                        // Copia peer online nella nuova posizione
                        if (i != newPeerCount) {
                            memcpy(&peerList[newPeerCount], &peerList[i], sizeof(Peer));
                        }
                        newPeerCount++;
                    } else {
                        // Log rimozione peer offline
                        DevLog.print("Rimosso peer offline: ");
                        DevLog.println(peerList[i].nodeId);
                    }
                }
                
                peerCount = newPeerCount;
                
                // Salva la lista aggiornata
                savePeersToLittleFS();
                
                DevLog.print("Cleanup completato: rimossi ");
                DevLog.print(offlineCount);
                DevLog.print(" peer, rimasti ");
                DevLog.println(peerCount);
                
            } else {
                // Nessun peer offline da rimuovere
                DevLog.println("Nessun peer offline da rimuovere");
            }
            
            commandHandled = true;
            return;
        }
        
        // Comando RESTART con formato JSON
        if (command == "RESTART") {
            DevLog.println("RICEVUTO - Comando: RESTART");
            
            // Verifica gatewayId se specificato
            if (doc.containsKey("gatewayId")) {
                String targetGatewayId = doc["gatewayId"].as<String>();
                if (targetGatewayId != String(gateway_id)) {
                    return; // Ignora silenziosamente se gatewayId diverso
                }
            }
            
            // Disconnetti MQTT
            mqttClient.disconnect();
            mqttConnected = false;
            
            DevLog.println("Restarting ESP8266...");
            delay(1000);
            
            // Riavvia l'ESP8266 senza resettare la configurazione
            ESP.restart();
            commandHandled = true;
            return;
        }
        
        // Comando NETWORK_DISCOVERY con formato JSON
        if (command == "NETWORK_DISCOVERY") {
            DevLog.println("RICEVUTO - Comando: NETWORK_DISCOVERY");
            
            // Network discovery avviato - nessun messaggio MQTT di conferma
            
            // NON resettare lo stato online - lasciare che i nodi rispondano e rimangano online
            // Solo i nodi che non rispondono verranno marcati offline dal timeout naturale
            
            // Invia SOLO richiesta di discovery broadcast per trovare nuovi nodi
            // NON inviare discovery individuali ai peer esistenti per evitare duplicazioni
            uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            espNow.send(broadcastAddress, "GATEWAY", "DISCOVERY", "DISCOVERY", "REQUEST", "DISCOVERY", gateway_id);
            
            // Imposta flag per inviare lista aggiornata dopo timeout
            networkDiscoveryActive = true;
            networkDiscoveryStartTime = millis();
            
            commandHandled = true;
            return;
        }
        
        // Comando PING_NETWORK - Invia PING individualmente a tutti i nodi online
        if (command == "PING_NETWORK") {
            DevLog.println("RICEVUTO - Comando: PING_NETWORK");
            
            int nodesSent = 0;
            String pingedNodesStr = "[";
            
            if (peerCount > 0) {
                // Resetta lo stato delle risposte
                memset(pingResponseReceived, 0, sizeof(pingResponseReceived));
                
                // Invia comando PING individualmente a TUTTI i nodi nella lista
                for (int i = 0; i < peerCount; i++) {
                    // Salva il MAC del nodo pingato usando l'indice corretto
                    memcpy(pingedNodesMac[i], peerList[i].mac, 6);
                    pingResponseReceived[i] = false;
                    
                    // Invia comando PING al nodo specifico
                    espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "PING", "REQUEST", "COMMAND", gateway_id);
                    
                    // Aggiungi ID nodo alla lista stringa (più leggero di JSON object completo)
                    if (nodesSent > 0) pingedNodesStr += ",";
                    pingedNodesStr += "\"" + String(peerList[i].nodeId) + "\"";
                    
                    nodesSent++;
                    
                    // Piccolo delay tra un invio e l'altro per evitare sovraccarico
                    delay(10);
                }
                
                // Avvia il processo di attesa risposte
                pingNetworkActive = true;
                pingNetworkStartTime = millis();
                pingResponseCount = peerCount; 
            }
            pingedNodesStr += "]";
            
            // Costruisci JSON manualmente per risparmiare RAM (evita DynamicJsonDocument 2KB)
            String responseOutput = "{";
            responseOutput += "\"command\":\"PING_NETWORK\",";
            responseOutput += "\"timestamp\":" + String(millis()) + ",";
            responseOutput += "\"total_nodes\":" + String(peerCount) + ",";
            responseOutput += "\"nodes_sent\":" + String(nodesSent) + ",";
            responseOutput += "\"result\":\"" + String(nodesSent > 0 ? "success" : "no_nodes") + "\",";
            responseOutput += "\"pinged_nodes\":" + pingedNodesStr;
            responseOutput += "}";
            
            // Pubblica report iniziale
            String topic = String(mqtt_topic_prefix) + "/gateway/status";
            mqttClient.publish(topic.c_str(), responseOutput.c_str());
            
            commandHandled = true;
            return;
        }
    }
}
