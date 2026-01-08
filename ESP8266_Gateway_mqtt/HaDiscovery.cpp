#include "HaDiscovery.h"
#include "WebLog.h"

void HaDiscovery::publishDiscovery(PubSubClient& client, const Peer& peer, const char* topicPrefix) {
    if (strlen(peer.nodeId) == 0 || strcmp(peer.nodeId, "null") == 0) {
        DevLog.println("⚠️ HaDiscovery: Ignorato peer con ID vuoto o nullo");
        return;
    }

    NodeEntity entities[8]; // Buffer for entities
    int count = NodeTypeManager::getNodeConfig(peer.nodeType, entities, 8);

    if (count > 0) {
        publishGenericDiscovery(client, peer, topicPrefix, entities, count);
    } else {
        DevLog.printf("⚠️ HaDiscovery: Tipo nodo non supportato per discovery: %s (ID: %s)\n", peer.nodeType, peer.nodeId);
    }
}

void HaDiscovery::publishDashboardConfig(PubSubClient& client, const Peer& peer, const char* topicPrefix) {
    if (strlen(peer.nodeId) == 0 || strcmp(peer.nodeId, "null") == 0) {
        return;
    }

    NodeEntity entities[8];
    int count = NodeTypeManager::getNodeConfig(peer.nodeType, entities, 8);

    if (count > 0) {
        String nodeIdStr = String(peer.nodeId);
        String topic = String(topicPrefix) + String("/dashboard/") + nodeIdStr + String("/config");
        
        DynamicJsonDocument doc(1024);
        doc["nodeId"] = nodeIdStr;
        doc["type"] = peer.nodeType;
        doc["name"] = nodeIdStr; 
        
        JsonArray entitiesJson = doc.createNestedArray("entities");
        for(int i=0; i<count; i++) {
            JsonObject entity = entitiesJson.createNestedObject();
            entity["id"] = entities[i].suffix;
            entity["name"] = entities[i].name;
            entity["type"] = entities[i].component;
            entity["idx"] = entities[i].attributeIndex;
        }
        
        String payload;
        serializeJson(doc, payload);
        client.publish(topic.c_str(), payload.c_str(), true);
    }
}

void HaDiscovery::publishGenericDiscovery(PubSubClient& client, const Peer& peer, const char* topicPrefix, NodeEntity* entities, int count) {
    String nodeIdStr = String(peer.nodeId);
    
    // Configurazione comune
    DynamicJsonDocument doc(1024);
    doc["device"]["identifiers"][0] = String("domoriky_") + nodeIdStr;
    doc["device"]["name"] = nodeIdStr;
    doc["device"]["model"] = String(peer.nodeType);
    doc["device"]["manufacturer"] = String("Domoriky");
    doc["state_topic"] = String(topicPrefix) + String("/nodo/status");
    
    // Availability configuration
    JsonArray avail = doc.createNestedArray("availability");
    JsonObject a1 = avail.createNestedObject();
    a1["topic"] = String(topicPrefix) + String("/nodo/") + nodeIdStr + String("/availability");
    a1["payload_available"] = String("online");
    a1["payload_not_available"] = String("offline");
    
    JsonObject a2 = avail.createNestedObject();
    a2["topic"] = String(topicPrefix) + String("/gateway/availability");
    a2["payload_available"] = String("online");
    a2["payload_not_available"] = String("offline");
    
    doc["command_topic"] = String(topicPrefix) + String("/nodo/command");
    
    for (int i = 0; i < count; i++) {
        DynamicJsonDocument c(1024);
        // Copia configurazione base
        for (JsonPair kv : doc.as<JsonObject>()) { c[kv.key()] = kv.value(); }
        
        c["name"] = nodeIdStr + String(" ") + entities[i].name;
        c["unique_id"] = String("domoriky_") + nodeIdStr + String("_") + entities[i].suffix;
        c["icon"] = entities[i].icon;
        c["device_class"] = entities[i].deviceClass;
        
        // Component specific config
        if (strcmp(entities[i].component, "switch") == 0) {
            c["payload_on"] = "ON";
            c["payload_off"] = "OFF";
            c["state_on"] = "1";
            c["state_off"] = "0";
        } else if (strcmp(entities[i].component, "cover") == 0) {
            c["payload_open"] = "OPEN";
            c["payload_close"] = "CLOSE";
            c["payload_stop"] = "STOP";
            c["state_open"] = "1";
            c["state_closed"] = "0";
            c["optimistic"] = false;
        }

        c["command_template"] = getCommandTemplate(nodeIdStr, entities[i].suffix, entities[i].component);
        c["value_template"] = getValueTemplate(nodeIdStr, entities[i].attributeIndex, entities[i].component);
        
        String cfgTopic = String("homeassistant/") + entities[i].component + "/" + nodeIdStr + "_" + entities[i].suffix + "/config";
        String payload;
        serializeJson(c, payload);
        client.publish(cfgTopic.c_str(), payload.c_str(), true);
    }
    
    // Pubblica availability corrente del nodo (retained) sul topic dedicato
    if (client.connected()) {
        String availTopic = String(topicPrefix) + String("/nodo/") + nodeIdStr + String("/availability");
        const char* availPayload = peer.isOnline ? "online" : "offline";
        client.publish(availTopic.c_str(), availPayload, true);
    }
}

String HaDiscovery::getCommandTemplate(const String& nodeId, const String& topic, const char* component) {
    if (strcmp(component, "cover") == 0) {
         // Cover Template: Maps HA OPEN/CLOSE/STOP to Firmware UP/DOWN/STOP
         String json = "{\"Node\":\"" + nodeId + "\",\"Topic\":\"" + topic;
         json += "\",\"Command\":\"{{ 'UP' if value == 'OPEN' else 'DOWN' if value == 'CLOSE' else 'STOP' }}\",\"Status\":\"\",\"Type\":\"COMMAND\"}";
         return json;
    }
    // Default to Switch: Maps HA ON/OFF to Firmware 1/0
    String json = "{\"Node\":\"" + nodeId + "\",\"Topic\":\"" + topic;
    json += "\",\"Command\":\"{{ '1' if value in ['ON','1', true] else '0' }}\",\"Status\":\"\",\"Type\":\"COMMAND\"}";
    return json;
}

String HaDiscovery::getValueTemplate(const String& nodeId, int attrIndex, const char* component) {
    return String("{% if value_json.Node == '") + nodeId + String("' and value_json.attributes is defined %}{{ value_json.attributes[") + String(attrIndex) + String("] }}{% endif %}");
}
