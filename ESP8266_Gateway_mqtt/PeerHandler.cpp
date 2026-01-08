#include "PeerHandler.h"
#include "EspNowHandler.h"
#include "MqttHandler.h"
#include "HaDiscovery.h"
#include "NodeTypeManager.h"
#include "WebLog.h"

// Define global variables
Peer peerList[MAX_PEERS];
int peerCount = 0;

NodeCommand pendingCommands[MAX_PEERS];
int pendingCommandsCount = 0;
const unsigned long NODE_COMMAND_TIMEOUT = 5000;

// Discovery and Ping flags
bool networkDiscoveryActive = false;
unsigned long networkDiscoveryStartTime = 0;
bool pingNetworkActive = false;
unsigned long pingNetworkStartTime = 0;
int pingResponseCount = 0;
uint8_t pingedNodesMac[MAX_PEERS][6];
bool pingResponseReceived[MAX_PEERS];


// External dependencies
// mqttConnected, mqttClient are in MqttHandler.h
extern char gateway_id[50];
extern char mqtt_topic_prefix[50];

// External functions
// publish* functions are in MqttHandler.h



#define PEERS_FILE "/peers.json"

String macToString(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

void savePeer(const uint8_t* mac_addr, const char* nodeId, const char* nodeType, const char* firmwareVersion) {
    bool isNewPeer = true;
    int peerIndex = 0;
    
    // Controlla se il peer è già nella lista
    for (int i = 0; i < peerCount; i++) {
        if (memcmp(peerList[i].mac, mac_addr, 6) == 0) {
            isNewPeer = false;
            peerIndex = i;
            break;
        }
    }
    
    // Se è un nuovo peer, aggiungilo
    if (isNewPeer) {
        if (peerCount < MAX_PEERS) {
            peerIndex = peerCount;
            memcpy(peerList[peerIndex].mac, mac_addr, 6);
            peerCount++;
        } else {
            DevLog.println("Errore: Lista peer piena!");
            return;
        }
    }
    
    // Aggiorna i dati del peer e traccia cambiamenti
    bool dataChanged = false;

    if (strlen(nodeId) > 0 && strcmp(peerList[peerIndex].nodeId, nodeId) != 0) {
        strncpy(peerList[peerIndex].nodeId, nodeId, sizeof(peerList[peerIndex].nodeId) - 1);
        peerList[peerIndex].nodeId[sizeof(peerList[peerIndex].nodeId) - 1] = '\0';
        dataChanged = true;
    }
    
    if (strlen(nodeType) > 0 && strcmp(peerList[peerIndex].nodeType, nodeType) != 0) {
        strncpy(peerList[peerIndex].nodeType, nodeType, sizeof(peerList[peerIndex].nodeType) - 1);
        peerList[peerIndex].nodeType[sizeof(peerList[peerIndex].nodeType) - 1] = '\0';
        dataChanged = true;
    }
    
    if (strlen(firmwareVersion) > 0 && strcmp(peerList[peerIndex].firmwareVersion, firmwareVersion) != 0) {
        strncpy(peerList[peerIndex].firmwareVersion, firmwareVersion, sizeof(peerList[peerIndex].firmwareVersion) - 1);
        peerList[peerIndex].firmwareVersion[sizeof(peerList[peerIndex].firmwareVersion) - 1] = '\0';
        dataChanged = true;
    }
    
    // Aggiorna timestamp
    peerList[peerIndex].isOnline = true;
    peerList[peerIndex].lastSeen = millis();
    
    // Salva su file se è nuovo o se sono cambiati dati importanti
    if (isNewPeer || dataChanged) {
        savePeersToLittleFS();
        
        // Invia aggiornamento MQTT
        if (mqttConnected) {
            if (isNewPeer) {
                publishPeerStatus(peerIndex, "NODE_NEW");
            } else {
                publishPeerStatus(peerIndex, "NODE_UPDATE");
            }
            HaDiscovery::publishDiscovery(mqttClient, peerList[peerIndex], mqtt_topic_prefix);
        }
    }
}

void loadPeersFromLittleFS() {
    if (!LittleFS.exists(PEERS_FILE)) {
        DevLog.println("Nessun file peer trovato.");
        return;
    }

    File configFile = LittleFS.open(PEERS_FILE, "r");
    if (!configFile) {
        DevLog.println("Impossibile aprire file peer.");
        return;
    }

    size_t size = configFile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, buf.get());

    if (error) {
        DevLog.println("Errore parsing JSON peer");
        return;
    }

    peerCount = 0;
    JsonArray peers = doc["peers"];
    for (JsonObject peer : peers) {
        if (peerCount < MAX_PEERS) {
            const char* macStr = peer["mac"];
            
            // Parse MAC address string to bytes
            int values[6];
            if (sscanf(macStr, "%x:%x:%x:%x:%x:%x", 
                       &values[0], &values[1], &values[2], 
                       &values[3], &values[4], &values[5]) == 6) {
                for(int i=0; i<6; i++) {
                    peerList[peerCount].mac[i] = (uint8_t)values[i];
                }
                
                strncpy(peerList[peerCount].nodeId, peer["nodeId"] | "", sizeof(peerList[peerCount].nodeId) - 1);
                strncpy(peerList[peerCount].nodeType, peer["nodeType"] | "", sizeof(peerList[peerCount].nodeType) - 1);
                strncpy(peerList[peerCount].firmwareVersion, peer["firmwareVersion"] | "", sizeof(peerList[peerCount].firmwareVersion) - 1);
                strncpy(peerList[peerCount].attributes, peer["attributes"] | "0000", sizeof(peerList[peerCount].attributes) - 1);
                peerList[peerCount].attributes[sizeof(peerList[peerCount].attributes) - 1] = '\0';
                
                // Ensure attributes are valid
                if (strlen(peerList[peerCount].attributes) < 4) {
                    strcpy(peerList[peerCount].attributes, "0000");
                }
                
                // Validazione base del nodo caricato
                if (strlen(peerList[peerCount].nodeId) > 0 && strcmp(peerList[peerCount].nodeId, "null") != 0) {
                    // Assume online on boot to prevent "Unavailable" in HA
                    peerList[peerCount].isOnline = true; 
                    peerList[peerCount].lastSeen = millis();
                    peerCount++;
                } else {
                    DevLog.printf("⚠️ Ignorato peer non valido caricato da FS (MAC: %s)\n", macStr);
                }
            }
        }
    }
    DevLog.printf("Caricati %d peer da LittleFS\n", peerCount);
}

void savePeersToLittleFS() {
    DynamicJsonDocument doc(4096);
    JsonArray peers = doc.createNestedArray("peers");

    for (int i = 0; i < peerCount; i++) {
        JsonObject peer = peers.createNestedObject();
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2], 
                 peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
        
        peer["mac"] = macStr;
        peer["nodeId"] = peerList[i].nodeId;
        peer["nodeType"] = peerList[i].nodeType;
        peer["firmwareVersion"] = peerList[i].firmwareVersion;
        peer["attributes"] = peerList[i].attributes;
    }

    File configFile = LittleFS.open(PEERS_FILE, "w");
    if (!configFile) {
        DevLog.println("Impossibile aprire file peer per scrittura");
        return;
    }

    serializeJson(doc, configFile);
    configFile.close();
}

void printPeersList() {
    DevLog.println("--- LISTA PEER ---");
    for (int i = 0; i < peerCount; i++) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2], 
                 peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
                 
        DevLog.printf("%d) ID: %s, MAC: %s, Type: %s, Ver: %s, Online: %s\n", 
                      i, peerList[i].nodeId, macStr, peerList[i].nodeType, 
                      peerList[i].firmwareVersion, peerList[i].isOnline ? "YES" : "NO");
    }
    DevLog.println("------------------");
}

// Non-blocking Peer List sending
bool listPeersActive = false;
int listPeersIndex = 0;
unsigned long lastPeerListSendTime = 0;
const unsigned long PEER_LIST_SEND_INTERVAL = 20; // 20ms between messages

void listPeers() {
    if (peerCount == 0) {
        DevLog.println("No peers to list.");
        return;
    }
    DevLog.println("Listing peers (non-blocking)...");
    
    // Start non-blocking process
    listPeersActive = true;
    listPeersIndex = 0;
    lastPeerListSendTime = 0;
}

void processPeerListSending() {
    if (!listPeersActive) return;

    // Send one peer per interval
    if (millis() - lastPeerListSendTime >= PEER_LIST_SEND_INTERVAL) {
        if (listPeersIndex < peerCount) {
            publishPeerStatus(listPeersIndex, "LIST_PEER_ITEM");
            lastPeerListSendTime = millis();
            listPeersIndex++;
        } else {
            // Finished
            listPeersActive = false;
            DevLog.println("Peer listing completed.");
        }
    }
}

void clearAllPeers() {
    DevLog.println("Cancellazione di TUTTI i peer...");
    
    // 1. Rimuovi peer da ESP-NOW
    for (int i = 0; i < peerCount; i++) {
        esp_now_del_peer(peerList[i].mac);
    }
    
    // 2. Resetta contatore
    peerCount = 0;
    
    // 3. Cancella file su LittleFS
    if (LittleFS.exists(PEERS_FILE)) {
        LittleFS.remove(PEERS_FILE);
        DevLog.println("File peers.json rimosso");
    }
    
    // 4. Notifica via MQTT
    publishGatewayStatus("peers_cleared", "All peers have been removed", "CLEAR_PEERS");
}

void removePeer(const char* macAddress) {
    DevLog.printf("Rimozione peer con MAC: %s\n", macAddress);
    
    int indexToRemove = -1;
    uint8_t macToRemove[6];
    
    // Parse MAC address
    int values[6];
    if (sscanf(macAddress, "%x:%x:%x:%x:%x:%x", 
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
        for(int i=0; i<6; i++) {
            macToRemove[i] = (uint8_t)values[i];
        }
        
        // Trova indice
        for (int i = 0; i < peerCount; i++) {
            if (memcmp(peerList[i].mac, macToRemove, 6) == 0) {
                indexToRemove = i;
                break;
            }
        }
        
        if (indexToRemove != -1) {
            // Rimuovi da ESP-NOW
            esp_now_del_peer(peerList[indexToRemove].mac);
            
            // Notifica rimozione
            publishGatewayStatus("peer_removed", String("Peer removed: ") + peerList[indexToRemove].nodeId, "REMOVE_PEER");
            
            // Sposta gli altri elementi
            for (int i = indexToRemove; i < peerCount - 1; i++) {
                peerList[i] = peerList[i+1];
            }
            peerCount--;
            
            // Salva modifiche
            savePeersToLittleFS();
            DevLog.println("Peer rimosso con successo");
        } else {
            DevLog.println("Peer non trovato");
            publishGatewayStatus("error", "Peer not found", "REMOVE_PEER");
        }
    } else {
        DevLog.println("Formato MAC non valido");
        publishGatewayStatus("error", "Invalid MAC format", "REMOVE_PEER");
    }
}

void checkAndSavePeers() {
    // Logica per salvataggio periodico se necessario
    // Al momento salviamo su ogni modifica importante
}

void forceSavePeers() {
    savePeersToLittleFS();
}

void processNodeCommand(const String& msgStr) {
    DevLog.print("RICEVUTO - Node Command: ");
    DevLog.println(msgStr);
    
    DynamicJsonDocument doc(512);
    DeserializationError parseError = deserializeJson(doc, msgStr);
    
    if (parseError) {
        DevLog.println("Errore parsing JSON per comando nodo");
        return;
    }
    
    // Caso 1: Comando speciale semplificato {"command":"NODE_REBOOT", "nodeId":"..."}
    if (doc.containsKey("command") && doc.containsKey("nodeId")) {
        String command = doc["command"].as<String>();
        String nodeId = doc["nodeId"].as<String>();
        
        DevLog.printf("Comando speciale per NODO - Command: %s, NodeId: %s\n", command.c_str(), nodeId.c_str());
        
        if (command == "NODE_REBOOT") {
            // Cerca il nodo
            bool nodeFound = false;
            for (int i = 0; i < peerCount; i++) {
                if (String(peerList[i].nodeId) == nodeId) {
                    // Invia comando RESTART
                    espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "RESTART", "", "COMMAND", gateway_id);
                    DevLog.printf("Comando RESTART inviato al nodo %s via ESP-NOW\n", nodeId.c_str());
                    nodeFound = true;
                    break;
                }
            }
            
            if (!nodeFound) {
                DevLog.printf("Nodo %s non trovato nella lista peer per REBOOT\n", nodeId.c_str());
            }
            return;
        }
        
        DevLog.printf("Comando speciale %s non riconosciuto\n", command.c_str());
        return;
    }
    
    // Caso 2: Comando standard {"Node":"...", "Topic":"...", "Command":"...", "Type":"..."}
    if (doc.containsKey("Node") && doc.containsKey("Topic") && doc.containsKey("Command") && doc.containsKey("Type")) {
        String nodeId = doc["Node"].as<String>();
        String topic = doc["Topic"].as<String>();
        String command = doc["Command"].as<String>();
        String status = doc["Status"].as<String>();
        String type = doc["Type"].as<String>();
        
        DevLog.printf("Comando per NODO - Node: %s, Topic: %s, Command: %s, Type: %s\n", 
                      nodeId.c_str(), topic.c_str(), command.c_str(), type.c_str());
        
        bool nodeFound = false;
        for (int i = 0; i < peerCount; i++) {
            if (String(peerList[i].nodeId) == nodeId) {
                
                // Controlla se il nodo è offline
                if (!peerList[i].isOnline) {
                    if (mqttConnected) {
                        publishNodeAvailability(nodeId, "offline");
                        publishPeerStatus(i, "NODE_STATUS_UPDATE");
                    }
                    DevLog.printf("Nodo %s offline: comando non inviato\n", nodeId.c_str());
                    nodeFound = true;
                    break;
                }

                // --- GENERIC CONTROL LOGIC ---
                // Handle "ALL_ON" / "ALL_OFF" or group commands based on Node Configuration
                if (topic == "CONTROL") {
                    String cmdNorm = command; cmdNorm.trim(); cmdNorm.toUpperCase();
                    
                    // Check for group commands
                    bool isGroupCommand = (cmdNorm == "ALL_ON" || cmdNorm == "ALL_OFF" || cmdNorm == "ALL_SWITCH");
                    
                    // Legacy compatibility for "ON"/"OFF" sent to CONTROL topic (treated as ALL)
                    // Only applies if the node has multiple switch entities
                    if (cmdNorm == "ON" || cmdNorm == "OFF" || cmdNorm == "TRUE" || cmdNorm == "FALSE") {
                        // Check if it's a multi-relay node
                        NodeEntity entities[8];
                        int count = NodeTypeManager::getNodeConfig(peerList[i].nodeType, entities, 8);
                        if (count > 1) isGroupCommand = true;
                    }
                    
                    if (isGroupCommand) {
                         int mapCmd = -1;
                         if (cmdNorm == "1" || cmdNorm == "ON" || cmdNorm == "TRUE" || cmdNorm == "ALL_ON") mapCmd = 1;
                         else if (cmdNorm == "0" || cmdNorm == "OFF" || cmdNorm == "FALSE" || cmdNorm == "ALL_OFF") mapCmd = 0;
                         else if (cmdNorm == "2" || cmdNorm == "SWITCH" || cmdNorm == "ALL_SWITCH") mapCmd = 2;
                         
                         if (mapCmd != -1) {
                            NodeEntity entities[8];
                            int count = NodeTypeManager::getNodeConfig(peerList[i].nodeType, entities, 8);
                            
                            for(int k=0; k<count; k++) {
                                // Apply only to 'switch' components
                                if(strcmp(entities[k].component, "switch") == 0) {
                                    String cmdStr = String(mapCmd);
                                    espNow.send(peerList[i].mac, nodeId.c_str(), entities[k].suffix, cmdStr.c_str(), status.c_str(), type.c_str(), gateway_id);
                                    
                                    // Add to pending
                                    if (pendingCommandsCount < MAX_PEERS) {
                                        pendingCommands[pendingCommandsCount].nodeId = nodeId;
                                        pendingCommands[pendingCommandsCount].topic = String(entities[k].suffix);
                                        pendingCommands[pendingCommandsCount].command = cmdStr;
                                        pendingCommands[pendingCommandsCount].sentTime = millis();
                                        pendingCommands[pendingCommandsCount].waitingResponse = true;
                                        pendingCommandsCount++;
                                    }
                                }
                            }
                            nodeFound = true;
                            break;
                         }
                    }
                }
                
                // Normalizzazione comandi ON/OFF per switch
                String cmdNorm = command; cmdNorm.trim(); cmdNorm.toUpperCase();
                
                // Check if topic matches a known entity
                NodeEntity entities[8];
                int count = NodeTypeManager::getNodeConfig(peerList[i].nodeType, entities, 8);
                bool entityFound = false;
                
                for(int k=0; k<count; k++) {
                    if (topic == entities[k].suffix) {
                        entityFound = true;
                        // Normalize specific to component type
                        if (strcmp(entities[k].component, "switch") == 0) {
                             if (cmdNorm == "ON" || cmdNorm == "TRUE") command = "1";
                             else if (cmdNorm == "OFF" || cmdNorm == "FALSE") command = "0";
                             else if (cmdNorm == "SWITCH" || cmdNorm == "TOGGLE") command = "2";
                        }
                        // Add other component normalizations here if needed (e.g. cover)
                        break;
                    }
                }
                
                // Fallback for legacy "relay_" prefix if not found in config (safety net)
                if (!entityFound && topic.startsWith("relay_")) {
                    if (cmdNorm == "ON" || cmdNorm == "TRUE") command = "1";
                    else if (cmdNorm == "OFF" || cmdNorm == "FALSE") command = "0";
                    else if (cmdNorm == "SWITCH" || cmdNorm == "TOGGLE") command = "2";
                }
                
                // Invia comando standard via ESP-NOW
                espNow.send(peerList[i].mac, nodeId.c_str(), topic.c_str(), command.c_str(), status.c_str(), type.c_str(), gateway_id);
                DevLog.printf("Comando inviato al nodo %s via ESP-NOW\n", nodeId.c_str());
                
                // Aggiungi alla coda comandi in attesa
                if (pendingCommandsCount < MAX_PEERS) {
                    pendingCommands[pendingCommandsCount].nodeId = nodeId;
                    pendingCommands[pendingCommandsCount].topic = topic;
                    pendingCommands[pendingCommandsCount].command = command;
                    pendingCommands[pendingCommandsCount].sentTime = millis();
                    pendingCommands[pendingCommandsCount].waitingResponse = true;
                    pendingCommandsCount++;
                }
                
                nodeFound = true;
                break;
            }
        }
        
        if (!nodeFound) {
            DevLog.printf("Nodo %s non trovato nella lista peer\n", nodeId.c_str());
        }
    } else {
        DevLog.println("Formato comando nodo non valido");
    }
}

void processNodeCommandTimeout() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < pendingCommandsCount; i++) {
        if (pendingCommands[i].waitingResponse && 
            (currentTime - pendingCommands[i].sentTime > NODE_COMMAND_TIMEOUT)) {
            
            DevLog.printf("TIMEOUT comando per nodo %s (Topic: %s)\n", 
                          pendingCommands[i].nodeId.c_str(), pendingCommands[i].topic.c_str());
            
            // Marca nodo come offline o invia notifica errore
            // Rimuovi comando dalla lista
            // FIX: Non marcare offline immediatamente su timeout comando singolo.
            // Lascia che sia il heartbeat o il ping a decidere se è offline.
            DevLog.printf("Command timeout for node %s - Command discarded (Node status preserved)\n", pendingCommands[i].nodeId.c_str());
            
            /* 
            // Marca il nodo come offline e aggiorna availability immediatamente
            for (int p = 0; p < peerCount; p++) {
                if (String(peerList[p].nodeId) == pendingCommands[i].nodeId) {
                    bool wasOnline = peerList[p].isOnline;
                    peerList[p].isOnline = false;
                    if (mqttConnected) {
                        if (wasOnline) {
                            publishPeerStatus(p, "NODE_STATUS_UPDATE");
                        }
                        publishNodeAvailability(pendingCommands[i].nodeId, "offline");
                    }
                    break;
                }
            }
            */
            
            // Rimuovi dalla coda
            for (int j = i; j < pendingCommandsCount - 1; j++) {
                pendingCommands[j] = pendingCommands[j + 1];
            }
            pendingCommandsCount--;
            i--; // Riprocessa questo indice
        }
    }
}

void processOfflineCheck() {
    // Implementazione controllo offline (es. heartbeat scaduto)
    // Controlla periodicamente se i nodi hanno superato il timeout di silenzio
    static unsigned long lastOfflineCheck = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastOfflineCheck >= 10000) { // Controlla ogni 10 secondi
        lastOfflineCheck = currentTime;
        int nodesTimedOut = 0;
        
        for (int i = 0; i < peerCount; i++) {
            // Se il nodo è marcato ONLINE e ha superato il timeout
            if (peerList[i].isOnline && (currentTime - peerList[i].lastSeen > NODE_OFFLINE_TIMEOUT)) {
                DevLog.print("TIMEOUT NODO: ");
                DevLog.print(peerList[i].nodeId);
                DevLog.println(" - Marcato OFFLINE");
                
                peerList[i].isOnline = false;
                nodesTimedOut++;
                
                // Notifica MQTT che il nodo è offline
                if (mqttConnected) {
                    publishPeerStatus(i, "NODE_STATUS_UPDATE");
                    publishNodeAvailability(String(peerList[i].nodeId), "offline");
                }
            }
        }
        
        if (nodesTimedOut > 0) {
            savePeersToLittleFS();
        }
    }
}

void processNetworkDiscovery() {
    if (networkDiscoveryActive) {
        unsigned long currentTime = millis();
        if (currentTime - networkDiscoveryStartTime >= 3000) { // NETWORK_DISCOVERY_TIMEOUT hardcoded or from const
             // Use external const if available or define it
             // Using literal 3000 for now to match logic
             
            networkDiscoveryActive = false;
            
            // Logica di fine discovery: marca offline chi non ha risposto? 
            // In realtà il discovery broadcast serve per trovare chi c'è.
            // La logica di marcare offline è più legata al PING_NETWORK o al timeout heartbeat.
            // Qui inviamo solo il report di chi è stato trovato/confermato.
            
            int nodesMarkedOffline = 0;
            // Verifica chi non ha aggiornato lastSeen di recente?
            // Se networkDiscovery aggiorna lastSeen, allora chi ha lastSeen < startTime è offline.
            
            // FIX: Non marcare offline chi non risponde al discovery.
            // Il discovery serve per trovare nodi, non per controllarne la presenza.
            // Lascia che sia il timeout naturale (processOfflineCheck) a gestire lo stato offline.
            /*
            for (int i = 0; i < peerCount; i++) {
                if (peerList[i].isOnline && peerList[i].lastSeen < networkDiscoveryStartTime) {
                    peerList[i].isOnline = false;
                    nodesMarkedOffline++;
                    if (mqttConnected) {
                        publishPeerStatus(i, "NODE_STATUS_UPDATE");
                        publishNodeAvailability(peerList[i].nodeId, "offline");
                    }
                }
            }
            */
            
            if (nodesMarkedOffline > 0) {
                savePeersToLittleFS();
            }
            
            // Invia report discovery
            if (mqttConnected) {
                DynamicJsonDocument discoveryDoc(1024);
                discoveryDoc["eventType"] = "network_discovery_complete";
                discoveryDoc["gatewayId"] = gateway_id;
                discoveryDoc["timestamp"] = currentTime;
                
                JsonArray peers = discoveryDoc.createNestedArray("peers");
                int onlineCount = 0;
                
                for (int i = 0; i < peerCount; i++) {
                    JsonObject peer = peers.createNestedObject();
                    peer["id"] = peerList[i].nodeId;
                    peer["online"] = peerList[i].isOnline;
                    if (peerList[i].isOnline) {
                        onlineCount++;
                    }
                }
                
                discoveryDoc["totalPeers"] = peerCount;
                discoveryDoc["onlinePeers"] = onlineCount;
                discoveryDoc["nodesMarkedOffline"] = nodesMarkedOffline;
                
                String jsonString;
                serializeJson(discoveryDoc, jsonString);
                
                String topic = String(mqtt_topic_prefix) + "/gateway/discovery";
                mqttClient.publish(topic.c_str(), jsonString.c_str());
                DevLog.println("Discovery report inviato");
            }
        }
    }
}

void processCommandResponse(const char* nodeId, const char* topic, const char* status, const uint8_t* mac) {
    // Aggiorna attributi se il messaggio è di feedback relè e contiene lo stato completo
    if (strncmp(topic, "relay_", 6) == 0 && strlen(status) >= 4) {
         for (int i = 0; i < peerCount; i++) {
            if (memcmp(peerList[i].mac, mac, 6) == 0) {
                strncpy(peerList[i].attributes, status, sizeof(peerList[i].attributes) - 1);
                peerList[i].attributes[sizeof(peerList[i].attributes) - 1] = '\0';
                break;
            }
         }
    }

    // Rimuovi comando dalla lista pending
    for (int i = 0; i < pendingCommandsCount; i++) {
        if (pendingCommands[i].waitingResponse && 
            pendingCommands[i].nodeId == String(nodeId) &&
            pendingCommands[i].topic == String(topic)) {
            
            // Rimuovi comando spostando gli elementi successivi
            for (int j = i; j < pendingCommandsCount - 1; j++) {
                pendingCommands[j] = pendingCommands[j + 1];
            }
            pendingCommandsCount--;
            break;
        }
    }
}
