#include "EspNowHandler.h"
#include "NodeTypeManager.h"
#include "WebHandler.h"
#include "WebLog.h"

// Queue variables
QueuedMessage messageQueue[MESSAGE_QUEUE_SIZE];
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;
unsigned long lastMessageProcessTime = 0;
const unsigned long MESSAGE_PROCESS_INTERVAL = 50;

// Statistics variables
unsigned long espNowSendSuccess = 0;
unsigned long espNowSendFailures = 0;
unsigned long totalMessagesProcessed = 0;
unsigned long totalMessagesDropped = 0;
unsigned long maxQueueUsage = 0;
unsigned long avgProcessingTime = 0;
unsigned long processStartTime = 0;
bool messageProcessingActive = false;

// External globals from .ino
extern char gateway_id[50];
extern char mqtt_topic_prefix[50];
extern DomoticaEspNow espNow;

// Global variable definition
char receivedMacStr[18];

// Add message to queue
bool addToMessageQueue(const uint8_t* mac, const struct_message& data) {
    if (queueCount >= MESSAGE_QUEUE_SIZE) {
        totalMessagesDropped++;
        return false;
    }
    
    // Update max usage stats
    if (queueCount > maxQueueUsage) {
        maxQueueUsage = queueCount;
    }
    
    QueuedMessage& msg = messageQueue[queueTail];
    memcpy(msg.mac, mac, 6);
    strncpy(msg.node, data.node, sizeof(msg.node) - 1);
    strncpy(msg.topic, data.topic, sizeof(msg.topic) - 1);
    strncpy(msg.command, data.command, sizeof(msg.command) - 1);
    strncpy(msg.status, data.status, sizeof(msg.status) - 1);
    strncpy(msg.type, data.type, sizeof(msg.type) - 1);
    strncpy(msg.gateway_id, data.gateway_id, sizeof(msg.gateway_id) - 1);
    msg.timestamp = millis();
    msg.valid = true;
    
    // Null-terminate strings
    msg.node[sizeof(msg.node) - 1] = '\0';
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    msg.command[sizeof(msg.command) - 1] = '\0';
    msg.status[sizeof(msg.status) - 1] = '\0';
    msg.type[sizeof(msg.type) - 1] = '\0';
    msg.gateway_id[sizeof(msg.gateway_id) - 1] = '\0';
    
    queueTail = (queueTail + 1) % MESSAGE_QUEUE_SIZE;
    queueCount++;
    
    return true;
}

// Process message queue
void processMessageQueue() {
    if (queueCount == 0) return;
    
    // Process up to 10 messages or for max 10ms to drain queue faster
    unsigned long startTime = millis();
    int processedCount = 0;
    
    while (queueCount > 0 && processedCount < 10 && (millis() - startTime) < 10) {
        
        QueuedMessage& msg = messageQueue[queueHead];
        if (!msg.valid) {
            queueHead = (queueHead + 1) % MESSAGE_QUEUE_SIZE;
            queueCount--;
            continue;
        }
        
        // Process the message
        snprintf(receivedMacStr, sizeof(receivedMacStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5]);
        
        DevLog.printf("RICEVUTO - (\"node\":\"%s\")(\"topic\":\"%s\")(\"command\":\"%s\")(\"status\":\"%s\")(\"type\":\"%s\")(\"gateway_id\":\"%s\")\n",
                 msg.node, msg.topic, msg.command, msg.status, msg.type, msg.gateway_id);
    
        // Copy data to receivedData for compatibility
        strncpy(receivedData.node, msg.node, sizeof(receivedData.node));
        strncpy(receivedData.topic, msg.topic, sizeof(receivedData.topic));
        strncpy(receivedData.command, msg.command, sizeof(receivedData.command));
        strncpy(receivedData.status, msg.status, sizeof(receivedData.status));
        strncpy(receivedData.type, msg.type, sizeof(receivedData.type));
        strncpy(receivedData.gateway_id, msg.gateway_id, sizeof(receivedData.gateway_id));
        
        // --- GESTIONE COMANDO RIMOZIONE PEER ---
        if (strcmp(receivedData.command, "REMOVE_PEER") == 0) {
            DevLog.printf("🚫 Ricevuto REMOVE_PEER da %s (%s). Rimozione immediata...\n", receivedData.node, receivedMacStr);
            removePeer(receivedMacStr);
            
            // Rimuovi messaggio dalla coda e continua
            msg.valid = false;
            queueHead = (queueHead + 1) % MESSAGE_QUEUE_SIZE;
            queueCount--;
            processedCount++;
            continue;
        }

        // Process message logic
        // RELAXED CHECK: Allow broadcast/discovery messages even if gateway_id doesn't match exactly
        // or if it's a specific message for this gateway
        bool isForThisGateway = (strcmp(receivedData.gateway_id, gateway_id) == 0);
        bool isDiscovery = (strcmp(receivedData.type, "DISCOVERY") == 0);
        bool isBroadcast = (strcmp(receivedData.gateway_id, "ALL") == 0 || strcmp(receivedData.gateway_id, "") == 0);
    
        if (isForThisGateway || isDiscovery || isBroadcast) {
        // Always update lastSeen for any received message
        bool peerFound = false;
        for (int i = 0; i < peerCount; i++) {
            if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                peerFound = true;
                // Detect state change from OFFLINE to ONLINE
                    bool wasOffline = !peerList[i].isOnline;
                    
                    peerList[i].isOnline = true;
                    peerList[i].lastSeen = millis();
                    
                    // Update attributes dynamically based on Node Type Configuration
                    NodeEntity entities[8];
                    int count = NodeTypeManager::getNodeConfig(peerList[i].nodeType, entities, 8);
                    
                    if (count > 0) {
                        // Ensure attributes string is long enough
                        int minLen = 0;
                        for(int k=0; k<count; k++) {
                            if(entities[k].attributeIndex >= minLen) minLen = entities[k].attributeIndex + 1;
                        }
                        
                        if ((int)strlen(peerList[i].attributes) < minLen) {
                             // Pad with '0'
                             for(int k=strlen(peerList[i].attributes); k<minLen; k++) peerList[i].attributes[k] = '0';
                             peerList[i].attributes[minLen] = '\0';
                        }
    
                        // Find matching entity for this topic
                        for (int k = 0; k < count; k++) {
                            if (String(receivedData.topic) == entities[k].suffix) {
                                int idx = entities[k].attributeIndex;
                                char newState = '0';
                                
                                // Map Status to State Char ('1'/'0')
                                // Adapts to both Switch (ON/OFF) and Cover (UP/DOWN/OPEN/CLOSE)
                                if (strcmp(receivedData.status, "1") == 0 || 
                                    strcmp(receivedData.status, "ON") == 0 || 
                                    strcmp(receivedData.status, "UP") == 0 || 
                                    strcmp(receivedData.status, "OPEN") == 0) {
                                    newState = '1';
                                }
                                // Note: '0' is default for OFF/DOWN/CLOSE
                                
                                peerList[i].attributes[idx] = newState;
                                break; 
                            }
                        }
                    }
                    // Fallback for legacy Relay logic if config fails (shouldn't happen given NodeTypes fallback)
                    else if (strncmp(receivedData.topic, "relay_", 6) == 0) {
                        int relayIdx = receivedData.topic[6] - '1'; // '1' -> 0
                        if (relayIdx >= 0 && relayIdx < 4) {
                            if (strlen(peerList[i].attributes) < 4) strcpy(peerList[i].attributes, "0000");
                            char newState = (strcmp(receivedData.status, "1") == 0 || strcmp(receivedData.status, "ON") == 0) ? '1' : '0';
                            peerList[i].attributes[relayIdx] = newState;
                            peerList[i].attributes[4] = '\0';
                        }
                    }
                    
                    // If node was offline, notify via MQTT
                    if (wasOffline && mqttConnected) {
                        publishPeerStatus(i, "NODE_STATUS_UPDATE");
                        publishNodeAvailability(String(peerList[i].nodeId), "online");
                        DevLog.print("STATUS: Node back ONLINE: ");
                        DevLog.println(peerList[i].nodeId);
                    }
                    break;
                }
            }
    
            // Process command response to clear pending commands
            processCommandResponse(receivedData.node, receivedData.topic, receivedData.status, msg.mac);
            
            // Handle OTA Feedback
            if (strcmp(receivedData.type, "FEEDBACK") == 0 && strcmp(receivedData.command, "OTA_UPDATE") == 0) {
                 DevLog.printf("OTA FEEDBACK Received: Node=%s, Status=%s, TargetGateway=%s\n", receivedData.node, receivedData.status, receivedData.gateway_id);
                 
                 // Update global status regardless of nodeId match if we are in a triggered state
                 // This ensures we catch the feedback even if there are case/format discrepancies
                 if (globalOtaStatus.status == "TRIGGERED" || globalOtaStatus.status == "OTA_STARTING" || globalOtaStatus.status == "OTA_PROGRESS") {
                      globalOtaStatus.status = receivedData.status;
                      globalOtaStatus.lastMessage = "Node Response (" + String(receivedData.node) + "): " + String(receivedData.status);
                      globalOtaStatus.timestamp = millis();
                 }
            }
    
            // Handle REGISTER command - Relaxed check
            if (strcmp(receivedData.command, "REGISTER") == 0) {
                
                if (strlen(receivedData.status) > 0) {
                    // Parse "TYPE|VERSION" if present
                    String statusStr = String(receivedData.status);
                    String typeStr = "";
                    String versionStr = "";
                    int pipeIndex = statusStr.indexOf('|');
                    
                    if (pipeIndex != -1) {
                        typeStr = statusStr.substring(0, pipeIndex);
                        versionStr = statusStr.substring(pipeIndex + 1);
                    } else {
                        typeStr = statusStr;
                    }
    
                    DevLog.printf("📝 REGISTRATION parsed - Type: %s, Version: %s\n", typeStr.c_str(), versionStr.c_str());
    
                    // OTA COMPLETION CHECK
                    // Check if this registration completes a pending OTA for this node
                    if ((globalOtaStatus.status == "TRIGGERED" || globalOtaStatus.status == "OTA_STARTING" || globalOtaStatus.status == "OTA_PROGRESS") && 
                        String(receivedData.node) == globalOtaStatus.nodeId) {
                        
                        globalOtaStatus.status = "SUCCESS";
                        globalOtaStatus.lastMessage = "Aggiornamento completato! Nuova Versione: " + versionStr;
                        globalOtaStatus.timestamp = millis();
                        DevLog.println("✅ OTA SUCCESS confirmed via REGISTRATION");
                    }
    
                    // Check if node is already registered with same type
                    bool alreadyRegistered = false;
                    for (int i = 0; i < peerCount; i++) {
                        if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                            // Found by MAC
                            if (strcmp(peerList[i].nodeType, typeStr.c_str()) == 0) {
                                alreadyRegistered = true;
                            }
                            // Update timestamp and online status regardless of type match
                            peerList[i].lastSeen = millis();
                            peerList[i].isOnline = true;
                            
                            // Se è una risposta al discovery, forziamo il refresh del discovery MQTT
                        // Questo gestisce il caso in cui il nodo è stato cancellato da HA
                        if (!alreadyRegistered || versionStr.length() > 0) {
                             savePeer(msg.mac, receivedData.node, typeStr.c_str(), versionStr.c_str(), true);
                        } else {
                             // Anche se già registrato e nessun cambiamento, forziamo discovery
                             savePeer(msg.mac, receivedData.node, typeStr.c_str(), versionStr.c_str(), true);
                        }
                        
                        alreadyRegistered = true; // Mark as handled
                        break;
                    }
                }
                
                if (!alreadyRegistered) {
                    savePeer(msg.mac, receivedData.node, typeStr.c_str(), versionStr.c_str(), true);
                }
            }
        } else if (strcmp(receivedData.type, "DISCOVERY") == 0 && 
                         strcmp(receivedData.command, "REQUEST") == 0) {
                      // DISCOVERY REQUEST: aggiorna lastSeen e marca come online
                      for (int i = 0; i < peerCount; i++) {
                          if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                              peerList[i].isOnline = true;
                              peerList[i].lastSeen = millis();
                              break;
                          }
                      }
    
                      // RISPOSTA AL DISCOVERY: Fondamentale per completare l'handshake con il nodo
                      // Il nodo si aspetta una risposta per settare gatewayFound = true
                      DevLog.printf("DISCOVERY REQUEST from %s (Target Gateway: %s)\n", receivedMacStr, receivedData.gateway_id);
                      DevLog.printf("Sending DISCOVERY RESPONSE to %s\n", receivedMacStr);
                      espNow.send(msg.mac, "GATEWAY", "DISCOVERY", "RESPONSE", "AVAILABLE", "GATEWAY_INFO", gateway_id);
    
            } else if (((strcmp(receivedData.type, "RESPONSE") == 0 || strcmp(receivedData.type, "FEEDBACK") == 0) && 
                       strcmp(receivedData.topic, "CONTROL") == 0 &&
                       strcmp(receivedData.command, "PONG") == 0) || 
                      (strcmp(receivedData.topic, "STATUS") == 0 && 
                       strcmp(receivedData.command, "HEARTBEAT") == 0)) {
                      
                      // Aggiorna lo stato del nodo
                      for (int i = 0; i < peerCount; i++) {
                          if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                              peerList[i].isOnline = true;
                              peerList[i].lastSeen = millis();
                              
                              // Estrai versione firmware se presente (formato "ALIVE|version|attributes" or "ONLINE|version")
                              String statusStr = String(receivedData.status);
                              String version = "";
                              String attributes = "";
                              
                              if (statusStr.startsWith("ALIVE|")) {
                                  int firstPipe = statusStr.indexOf('|');
                                  int secondPipe = statusStr.indexOf('|', firstPipe + 1);
                                  
                                  if (secondPipe != -1) {
                                      version = statusStr.substring(firstPipe + 1, secondPipe);
                                      attributes = statusStr.substring(secondPipe + 1);
                                  } else {
                                      version = statusStr.substring(firstPipe + 1);
                                  }
                              } else if (statusStr.startsWith("ONLINE|")) {
                                   int firstPipe = statusStr.indexOf('|');
                                   version = statusStr.substring(firstPipe + 1);
                              }
                              
                              // Aggiorna versione se trovata
                              if (version.length() > 0 && strcmp(peerList[i].firmwareVersion, version.c_str()) != 0) {
                                  strncpy(peerList[i].firmwareVersion, version.c_str(), sizeof(peerList[i].firmwareVersion) - 1);
                                  peerList[i].firmwareVersion[sizeof(peerList[i].firmwareVersion) - 1] = '\0';
                                  DevLog.printf("Updated firmware version for node %s: %s\n", peerList[i].nodeId, peerList[i].firmwareVersion);
                                  
                                  // Salva su LittleFS se la versione cambia
                                  savePeersToLittleFS();
                                  
                                  // Pubblica aggiornamento versione via MQTT
                                  if (mqttConnected) {
                                       publishPeerStatus(i, "NODE_VERSION_UPDATE");
                                  }
                              }
                              
                              // Salva attributi se presenti
                              if (attributes.length() > 0) {
                                   strncpy(peerList[i].attributes, attributes.c_str(), sizeof(peerList[i].attributes) - 1);
                                   peerList[i].attributes[sizeof(peerList[i].attributes) - 1] = '\0';
                              }
                              
                              break;
                          }
                      }
                      
                      // Segna la risposta ricevuta nel sistema PING
                      if (pingNetworkActive) {
                          // Cerca il nodo nella lista peerList usando l'indice diretto
                          for (int i = 0; i < peerCount && i < pingResponseCount; i++) {
                              if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                                  pingResponseReceived[i] = true;
                                  break;
                              }
                          }
                      }
            } else if (strcmp(receivedData.type, "DISCOVERY") == 0 && 
                             strcmp(receivedData.command, "DISCOVERY_RESPONSE") == 0) {
                 
                 // Parse "TYPE|VERSION" from status
                 String statusStr = String(receivedData.status);
                 String typeStr = "";
                 String versionStr = "";
                 int pipeIndex = statusStr.indexOf('|');
                 
                 if (pipeIndex != -1) {
                     typeStr = statusStr.substring(0, pipeIndex);
                     versionStr = statusStr.substring(pipeIndex + 1);
                 } else {
                     if (statusStr.length() > 0 && statusStr != "AVAILABLE") {
                         typeStr = statusStr;
                     }
                 }

                 // Handle discovery response
                 bool known = false;
                 for (int i = 0; i < peerCount; i++) {
                     if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                         known = true;
                         peerList[i].lastSeen = millis();
                         peerList[i].isOnline = true;
                         
                         // Use existing nodeType if the received one is empty/generic/unknown
                         const char* targetType = (typeStr.length() > 0 && typeStr != "GENERIC" && typeStr != "UNKNOWN") ? typeStr.c_str() : peerList[i].nodeType;
                         
                         // Force update/discovery for known nodes (re-sends MQTT config)
                         savePeer(msg.mac, receivedData.node, targetType, versionStr.c_str(), true);
                         
                         break;
                     }
                 }
                 
                 if (!known) {
                     DevLog.println("✨ New node discovered via response!");
                     savePeer(msg.mac, receivedData.node, typeStr.c_str(), versionStr.c_str(), true); 
                     
                     if (typeStr == "" || typeStr == "UNKNOWN") {
                         DevLog.println("⚠️ New Node (Unknown Type). Forcing RESTART to register.");
                         espNow.send(msg.mac, gateway_id, "CONTROL", "RESTART", "0", "", "");
                     }
                 }
            } else {
                 // Per tutti gli altri messaggi, usa solo nodeId (senza cambiare nodeType)
                 // Aggiorna anche lo stato online poiché il nodo sta comunicando attivamente
                 bool known = false;
                 for (int i = 0; i < peerCount; i++) {
                     if (memcmp(peerList[i].mac, msg.mac, 6) == 0) {
                         peerList[i].isOnline = true;
                         peerList[i].lastSeen = millis();
                         
                         // Check if known peer has missing type
                         if (strlen(peerList[i].nodeType) == 0 || strcmp(peerList[i].nodeType, "UNKNOWN") == 0) {
                             DevLog.println("⚠️ Known Node with missing Type. Forcing RESTART to re-register.");
                             espNow.send(msg.mac, gateway_id, "CONTROL", "RESTART", "0", "", "");
                         }
                         
                         known = true;
                         break;
                     }
                 }
                 
                 if (!known) {
                      // Just register as unknown, don't try to guess type from status
                      savePeer(msg.mac, receivedData.node, "", "", true); 
                      
                      // Force Restart to ensure proper Registration and Type detection
                      DevLog.println("⚠️ New Node (Unknown Type) detected via generic msg. Forcing RESTART to register.");
                      espNow.send(msg.mac, gateway_id, "CONTROL", "RESTART", "0", "", "");
                 }
            }
            
            // Send to MQTT
            newEspNowData = true;
            processEspNowData(); // Publish immediately to avoid overwriting in batch processing
        }
        
        // Remove message from queue
        msg.valid = false;
        queueHead = (queueHead + 1) % MESSAGE_QUEUE_SIZE;
        queueCount--;
        
        // Update stats
        totalMessagesProcessed++;
        unsigned long processingTime = millis() - processStartTime;
        avgProcessingTime = ((avgProcessingTime * (totalMessagesProcessed - 1)) + processingTime) / totalMessagesProcessed;
        
        processedCount++;
    }
    
    messageProcessingActive = false;
}

// Process ESP-NOW data for MQTT publishing
void processEspNowData() {
    if (newEspNowData) {
        newEspNowData = false;

        // Check if config exists in LittleFS
        bool configExists = LittleFS.exists("/config.json");
        
        // Filtra messaggi di heartbeat per evitare traffico inutile su MQTT
        // Il gateway aggiorna comunque il timestamp lastSeen internamente (in processMessageQueue)
        // Invia a MQTT solo se NON è un heartbeat periodico
        if (strcmp(receivedData.command, "HEARTBEAT") != 0) {
            if (configExists) {
                // Normal behavior
                if (mqttClient.connected()) {
                    publishNodeStatus(receivedData.node, receivedData.topic, receivedData.command, receivedData.status, receivedData.type);
                }
            } else {
                // No config, try to publish if connected (fallback)
                if (mqttClient.connected()) {
                    publishNodeStatus(receivedData.node, receivedData.topic, receivedData.command, receivedData.status, receivedData.type);
                }
            }
        }
    }
}

// Track ESP-NOW send result
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
    if (status == 0) {
        espNowSendSuccess++;
    } else {
        espNowSendFailures++;
        DevLog.print("❌ Errore invio ESP-NOW a: ");
        DevLog.println(macToString(mac_addr));
    }
}

// Print queue status (placeholder)
void printQueueStatus() {
    // Empty function - debug removed
}

// --- ESP-NOW CALLBACK --- //
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
    if (len == sizeof(receivedData)) {
        struct_message tempData;
        memcpy(&tempData, incomingData, sizeof(tempData));
        
        // Gestione messaggi di discovery - risposta IMMEDIATA (non in coda)
        if (strcmp(tempData.type, "DISCOVERY") == 0 && 
            strcmp(tempData.topic, "DISCOVERY") == 0) {
            // Verifica se il discovery è per questo gateway
            if (strcmp(tempData.status, gateway_id) == 0) {
                snprintf(receivedMacStr, sizeof(receivedMacStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                
                char debugBuffer[256];
                snprintf(debugBuffer, sizeof(debugBuffer), "RICEVUTO - (\"node\":\"%s\")(\"topic\":\"%s\")(\"command\":\"%s\")(\"status\":\"%s\")(\"type\":\"%s\")(\"gateway_id\":\"%s\")",
                         tempData.node, tempData.topic, tempData.command, tempData.status, tempData.type, tempData.gateway_id);
                DevLog.println(debugBuffer);
                
                // Controlla se il nodo è già registrato
                bool isRegistered = false;
                int registeredIndex = -1;
                for (int i = 0; i < peerCount; i++) {
                    if (memcmp(peerList[i].mac, mac, 6) == 0) {
                        isRegistered = true;
                        registeredIndex = i;
                        break;
                    }
                }
                
                // Risponde sempre per permettere ai nodi di ristabilire la connessione dopo riavvio
                if (!isRegistered) {
                    DevLog.println("Nuovo nodo rilevato - Invio risposta discovery diretta...");
                    espNow.send(mac, "GATEWAY", "DISCOVERY", "RESPONSE", "AVAILABLE", "GATEWAY_INFO", gateway_id);
                    DevLog.println("INVIATO - (\"node\":\"GATEWAY\")(\"topic\":\"DISCOVERY\")(\"command\":\"RESPONSE\")(\"status\":\"AVAILABLE\")(\"type\":\"GATEWAY_INFO\")(\"gateway_id\":\"" + String(gateway_id) + "\")");
                } else {
                    // Aggiorna lo stato del nodo registrato e risponde comunque
                    peerList[registeredIndex].isOnline = true;
                    peerList[registeredIndex].lastSeen = millis();
                    DevLog.printf("✅ Nodo %s già registrato - Riconnessione dopo riavvio\n", peerList[registeredIndex].nodeId);
                    
                    espNow.send(mac, "GATEWAY", "DISCOVERY", "RESPONSE", "AVAILABLE", "GATEWAY_INFO", gateway_id);
                    DevLog.println("INVIATO - (\"node\":\"GATEWAY\")(\"topic\":\"DISCOVERY\")(\"command\":\"RESPONSE\")(\"status\":\"AVAILABLE\")(\"type\":\"GATEWAY_INFO\")(\"gateway_id\":\"" + String(gateway_id) + "\")");
                }
            }
            return;
        }
        
        // Gestione risposte DISCOVERY dai nodi - AUTO-REGISTRAZIONE IMMEDIATA
        if (strcmp(tempData.type, "DISCOVERY") == 0 && 
            strcmp(tempData.topic, "DISCOVERY") == 0 &&
            strcmp(tempData.command, "RESPONSE") == 0) {
            
            // Verifica se il nodo ha configurato questo gateway
            if (strcmp(tempData.gateway_id, gateway_id) == 0) {
                // Validazione ID nodo
                if (strlen(tempData.node) == 0 || strcmp(tempData.node, "null") == 0) {
                     DevLog.println("⚠️ Ignorata DISCOVERY RESPONSE da nodo con ID non valido");
                     return;
                }

                snprintf(receivedMacStr, sizeof(receivedMacStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                
                // Parse "TYPE|VERSION" from status
                String statusStr = String(tempData.status);
                String typeStr = "GENERIC";
                String versionStr = "";
                int pipeIndex = statusStr.indexOf('|');
                
                if (pipeIndex != -1) {
                    typeStr = statusStr.substring(0, pipeIndex);
                    versionStr = statusStr.substring(pipeIndex + 1);
                } else {
                    // If status is not empty and not just "AVAILABLE", use it as type
                    if (statusStr.length() > 0 && statusStr != "AVAILABLE") {
                        typeStr = statusStr;
                    }
                }
                
                // If type is generic or status-like, treat as empty to avoid overwriting specific types
                // This prevents downgrading a known "4_RELAY_CONTROLLER" to "GENERIC" just because it replied "AVAILABLE"
                if (typeStr == "GENERIC" || typeStr == "AVAILABLE" || typeStr == "UNKNOWN") {
                    typeStr = "";
                }

                DevLog.printf("✨ DISCOVERY RESPONSE from %s (MAC: %s) - Type: %s, Ver: %s\n", 
                              tempData.node, receivedMacStr, typeStr.c_str(), versionStr.c_str());
                              
                // Use savePeer to handle registration/update and MQTT discovery consistently
                savePeer(mac, tempData.node, typeStr.c_str(), versionStr.c_str());

                // Update ping response tracking if active
                if (pingNetworkActive) {
                    for (int k = 0; k < pingResponseCount; k++) {
                        if (memcmp(pingedNodesMac[k], mac, 6) == 0) {
                            pingResponseReceived[k] = true;
                            break;
                        }
                    }
                }
            } else {
                DevLog.printf("ℹ️ Nodo %s ignorato - configurato per gateway %s (non %s)\n", 
                              tempData.node, tempData.gateway_id, gateway_id);
            }
            return;
        }
        
        // TUTTI GLI ALTRI MESSAGGI vanno in coda per processamento sequenziale
        if (strcmp(tempData.gateway_id, gateway_id) == 0) {
            if (!addToMessageQueue(mac, tempData)) {
                DevLog.println("❌ ERRORE: Impossibile aggiungere messaggio alla coda - MESSAGGIO PERSO!");
            }
        }
    } else {
        DevLog.print("❌ Ricevuti dati dimensione errata: ");
        DevLog.println(len);
    }
}

// Process network ping logic
void processPingLogic() {
    if (pingNetworkActive) {
        unsigned long currentTime = millis();
        bool allResponsesReceived = true;
        
        for (int i = 0; i < pingResponseCount; i++) {
            if (!pingResponseReceived[i]) {
                allResponsesReceived = false;
                break;
            }
        }
        
        if (allResponsesReceived || currentTime - pingNetworkStartTime >= PING_RESPONSE_TIMEOUT) {
            int onlineCount = 0;
            int nodesMarkedOffline = 0;
            int nodesMarkedOnline = 0;
            
            for (int i = 0; i < pingResponseCount; i++) {
                if (pingResponseReceived[i]) {
                    onlineCount++;
                    // Aggiorna stato online per i nodi che hanno risposto
                    for (int j = 0; j < peerCount; j++) {
                        if (memcmp(peerList[j].mac, pingedNodesMac[i], 6) == 0) {
                            if (!peerList[j].isOnline) {
                                peerList[j].isOnline = true;
                                peerList[j].lastSeen = millis();
                                nodesMarkedOnline++;
                                if (mqttConnected) publishPeerStatus(j, "NODE_STATUS_UPDATE");
                            }
                            break;
                        }
                    }
                } else {
                    for (int j = 0; j < peerCount; j++) {
                        if (memcmp(peerList[j].mac, pingedNodesMac[i], 6) == 0) {
                            // Se il nodo non risponde al PING, marcalo offline IMMEDIATAMENTE
                            if (peerList[j].isOnline) {
                                peerList[j].isOnline = false;
                                nodesMarkedOffline++;
                                if (mqttConnected) {
                                    publishPeerStatus(j, "NODE_STATUS_UPDATE");
                                    // Pubblica anche availability offline specifica
                                    publishNodeAvailability(peerList[j].nodeId, "offline");
                                }
                            }
                            break;
                        }
                    }
                }
            }
            
            // Salva i cambiamenti se ci sono stati aggiornamenti di stato
            if (nodesMarkedOffline > 0 || nodesMarkedOnline > 0) {
                savePeersToLittleFS();
            }
            
            // Conta i nodi attualmente online
            int totalOnlineNodes = 0;
            for (int i = 0; i < peerCount; i++) {
                if (peerList[i].isOnline) {
                    totalOnlineNodes++;
                }
            }
            
            // Ping completato - Invia report finale via MQTT
            if (mqttConnected) {
                 DynamicJsonDocument pingReportDoc(512);
                 pingReportDoc["command"] = "PING_NETWORK_REPORT";
                 pingReportDoc["gatewayId"] = gateway_id;
                 pingReportDoc["timestamp"] = currentTime;
                 pingReportDoc["totalNodes"] = peerCount;
                 pingReportDoc["onlineNodes"] = totalOnlineNodes;
                 pingReportDoc["nodesMarkedOnline"] = nodesMarkedOnline;
                 pingReportDoc["nodesMarkedOffline"] = nodesMarkedOffline;
                 
                 String pingReportOutput;
                 serializeJson(pingReportDoc, pingReportOutput);
                 
                 // Usa topic status per coerenza
                 String topic = String(mqtt_topic_prefix) + "/gateway/status";
                 mqttClient.publish(topic.c_str(), pingReportOutput.c_str());
            }
            
            pingNetworkActive = false;
            pingResponseCount = 0;
        }
    }
}

