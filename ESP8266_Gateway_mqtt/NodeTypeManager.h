#ifndef NODE_TYPE_MANAGER_H
#define NODE_TYPE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "WebLog.h"

#define NODETYPES_FILE "/nodetypes.json"

// Struttura dati per le entità (simile a prima, ma popolata dinamicamente)
struct NodeEntity {
    char suffix[32];       // e.g., "relay_1"
    char name[32];         // e.g., "Relay 1"
    char component[16];    // e.g., "switch"
    char deviceClass[32];  // e.g., "outlet"
    char icon[32];         // e.g., "mdi:power-socket-eu"
    int attributeIndex;  // Index in the status string
};

class NodeTypeManager {
public:
    static bool begin() {
        if (!LittleFS.begin()) {
            DevLog.println("LittleFS Mount Failed");
            return false;
        }
        
        // Se il file non esiste, creiamo quello di default
        if (!LittleFS.exists(NODETYPES_FILE)) {
            DevLog.println("nodetypes.json not found, creating default...");
            createDefaultConfig();
        } else {
            // Controlla se mancano definizioni chiave e aggiorna
            checkAndUpdateConfig();
        }
        
        return loadConfig();
    }

    // Restituisce il JSON grezzo per l'API
    static String getJsonConfig() {
        if (LittleFS.exists(NODETYPES_FILE)) {
            File file = LittleFS.open(NODETYPES_FILE, "r");
            if (file) {
                String content = file.readString();
                file.close();
                return content;
            }
        }
        return "{}";
    }

    // Popola l'array di entità per un dato tipo di nodo
    static int getNodeConfig(const char* nodeType, NodeEntity* entities, int maxEntities) {
        // --- 1. Gestione Dinamica per RL_CTRL_ESP8266_XCH ---
        // Verifica se è un tipo dinamico "RL_CTRL_ESP8266_"
        if (strstr(nodeType, "RL_CTRL_ESP8266_") != NULL) {
            // Estrai il numero di canali dalla stringa (es. "..._3CH")
            // Cerca l'ultimo underscore
            const char* lastUnderscore = strrchr(nodeType, '_');
            if (lastUnderscore) {
                int channels = atoi(lastUnderscore + 1); // Parsa il numero dopo '_'
                
                // Limita al numero massimo di entità
                if (channels > maxEntities) channels = maxEntities;
                
                // Genera dinamicamente le entità
                for(int i=0; i<channels; i++) {
                    snprintf(entities[i].suffix, sizeof(entities[i].suffix), "relay_%d", i+1);
                    snprintf(entities[i].name, sizeof(entities[i].name), "Relay %d", i+1);
                    strncpy(entities[i].component, "switch", sizeof(entities[i].component));
                    strncpy(entities[i].deviceClass, "outlet", sizeof(entities[i].deviceClass));
                    strncpy(entities[i].icon, "mdi:power-socket-eu", sizeof(entities[i].icon));
                    entities[i].attributeIndex = i;
                }
                
                DevLog.printf("Dynamic Config: Generated %d entities for %s\n", channels, nodeType);
                return channels;
            }
        }
        
        // --- 2. Gestione Standard da JSON ---
        // Ricarichiamo il config se necessario (per ora lo facciamo ogni volta per semplicità e RAM, 
        // ma in produzione si potrebbe cachare in memoria se c'è spazio)
        
        File file = LittleFS.open(NODETYPES_FILE, "r");
        if (!file) return 0;

        // Buffer per il documento JSON (dimensione da calibrare)
        DynamicJsonDocument doc(2048); 
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            DevLog.println("Failed to parse nodetypes.json");
            return 0;
        }

        // Cerca il tipo specifico
        JsonVariant typeConfig = doc[nodeType];
        
        // Fallback: se non trovato, cerca parziale "RELAY"
        if (!typeConfig) {
            if (strstr(nodeType, "RELAY") != NULL) {
                // Prova a cercare un tipo generico "4_RELAY_CONTROLLER" come fallback
                typeConfig = doc["4_RELAY_CONTROLLER"];
            }
        }

        if (!typeConfig) return 0;

        JsonArray entitiesJson = typeConfig["entities"];
        int count = 0;

        for (JsonObject entity : entitiesJson) {
            if (count >= maxEntities) break;

            strncpy(entities[count].suffix, entity["suffix"] | "", sizeof(entities[count].suffix) - 1);
            entities[count].suffix[sizeof(entities[count].suffix) - 1] = '\0';
            
            strncpy(entities[count].name, entity["name"] | "", sizeof(entities[count].name) - 1);
            entities[count].name[sizeof(entities[count].name) - 1] = '\0';
            
            strncpy(entities[count].component, entity["type"] | "switch", sizeof(entities[count].component) - 1);
            entities[count].component[sizeof(entities[count].component) - 1] = '\0';
            
            strncpy(entities[count].deviceClass, entity["device_class"] | "", sizeof(entities[count].deviceClass) - 1);
            entities[count].deviceClass[sizeof(entities[count].deviceClass) - 1] = '\0';
            
            strncpy(entities[count].icon, entity["icon"] | "", sizeof(entities[count].icon) - 1);
            entities[count].icon[sizeof(entities[count].icon) - 1] = '\0';

            entities[count].attributeIndex = entity["idx"] | count;

            count++;
        }

        return count;
    }

private:
    static bool loadConfig() {
        // Verifica validità JSON
        File file = LittleFS.open(NODETYPES_FILE, "r");
        if (!file) return false;
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (error) {
            DevLog.println("Invalid nodetypes.json format");
            return false;
        }
        return true;
    }

    static void createDefaultConfig() {
        DynamicJsonDocument doc(2048);

        addDefaultTypes(doc);

        File file = LittleFS.open(NODETYPES_FILE, "w");
        if (file) {
            serializeJsonPretty(doc, file);
            file.close();
            DevLog.println("Default nodetypes.json created");
        } else {
            DevLog.println("Failed to create nodetypes.json");
        }
    }

    static void checkAndUpdateConfig() {
        File file = LittleFS.open(NODETYPES_FILE, "r");
        if (!file) return;

        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            DevLog.println("Invalid nodetypes.json, recreating...");
            createDefaultConfig();
            return;
        }

        bool changed = false;
        
        // Verifica se mancano i tipi standard
        if (!doc.containsKey("4_RELAY_CONTROLLER")) {
            addDefault4Relay(doc);
            changed = true;
        }
        if (!doc.containsKey("2_RELAY_CONTROLLER")) {
            addDefault2Relay(doc);
            changed = true;
        }
        if (!doc.containsKey("SHUTTER_CONTROLLER")) {
            addDefaultShutter(doc);
            changed = true;
        }

        if (changed) {
            File outFile = LittleFS.open(NODETYPES_FILE, "w");
            if (outFile) {
                serializeJsonPretty(doc, outFile);
                outFile.close();
                DevLog.println("nodetypes.json updated with missing keys");
            }
        }
    }

    static void addDefaultTypes(DynamicJsonDocument& doc) {
        addDefault4Relay(doc);
        addDefault2Relay(doc);
        addDefaultShutter(doc);
    }

    static void addDefault4Relay(DynamicJsonDocument& doc) {
        JsonObject relay4 = doc.createNestedObject("4_RELAY_CONTROLLER");
        relay4["description"] = "Standard 4-Channel Relay";
        JsonArray r4Entities = relay4.createNestedArray("entities");

        const char* r4Names[] = {"Relay 1", "Relay 2", "Relay 3", "Relay 4"};
        for(int i=0; i<4; i++) {
            JsonObject e = r4Entities.createNestedObject();
            e["suffix"] = String("relay_") + (i+1);
            e["name"] = r4Names[i];
            e["type"] = "switch";
            e["device_class"] = "outlet";
            e["icon"] = "mdi:power-socket-eu";
            e["idx"] = i;
        }
    }

    static void addDefault2Relay(DynamicJsonDocument& doc) {
        JsonObject relay2 = doc.createNestedObject("2_RELAY_CONTROLLER");
        relay2["description"] = "2-Channel Relay";
        JsonArray r2Entities = relay2.createNestedArray("entities");

        const char* r2Names[] = {"Luce 1", "Luce 2"};
        for(int i=0; i<2; i++) {
            JsonObject e = r2Entities.createNestedObject();
            e["suffix"] = String("relay_") + (i+1);
            e["name"] = r2Names[i];
            e["type"] = "switch";
            e["device_class"] = "light";
            e["icon"] = "mdi:lightbulb";
            e["idx"] = i;
        }
    }

    static void addDefaultShutter(DynamicJsonDocument& doc) {
        JsonObject shutter = doc.createNestedObject("SHUTTER_CONTROLLER");
        shutter["description"] = "Window Shutter Controller";
        JsonArray sEntities = shutter.createNestedArray("entities");
        
        JsonObject e = sEntities.createNestedObject();
        e["suffix"] = "shutter";
        e["name"] = "Tapparella";
        e["type"] = "cover";
        e["device_class"] = "shutter";
        e["icon"] = "mdi:window-shutter";
        e["idx"] = 0;
    }
};

#endif
