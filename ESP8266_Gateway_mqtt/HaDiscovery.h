#ifndef HA_DISCOVERY_H
#define HA_DISCOVERY_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "PeerHandler.h"
#include "NodeTypeManager.h"

class HaDiscovery {
public:
    static void publishDiscovery(PubSubClient& client, const Peer& peer, const char* topicPrefix);
    static void publishDashboardConfig(PubSubClient& client, const Peer& peer, const char* topicPrefix);
private:
    static void publishGenericDiscovery(PubSubClient& client, const Peer& peer, const char* topicPrefix, NodeEntity* entities, int count);
    static String getCommandTemplate(const String& nodeId, const String& topic, const char* component);
    static String getValueTemplate(const String& nodeId, int attrIndex, const char* component);
};

#endif
