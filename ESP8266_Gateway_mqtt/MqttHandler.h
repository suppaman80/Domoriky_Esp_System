#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

// Configura buffer MQTT per messaggi più grandi
#undef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 4096

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "GatewayTypes.h"

// External globals
extern PubSubClient mqttClient;
extern bool mqttConnected;
extern unsigned long lastMqttReconnectAttempt;
extern const unsigned long MQTT_RECONNECT_INTERVAL;

// Function prototypes
void setupMQTT();
bool connectToMQTT();
void reconnectMQTT();
void onMqttConnect();
void onMqttDisconnect();
void onMqttMessage(char* topic, byte* payload, unsigned int length);
void publishGatewayStatus(const String& eventType, const String& message, const String& command = "", const String& ip = "");
void publishNodeStatus(const String& nodeId, const String& topic_name, const String& command, const String& status, const String& type);
void publishPeerStatus(int i, const char* command);
void publishNodeAvailability(const String& nodeId, const char* availability);
void publishToMQTT(const String& subtopic, const String& eventType, const String& message);
void sendGatewayHeartbeat();
void sendDashboardDiscovery();
void triggerGlobalDiscovery();

// External command processor
void processMqttCommand(const String& msgStr);
void performOTA(String url);

#endif
