#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "Config.h"
#include "GatewayTypes.h"
#include "PeerHandler.h"
#include "MqttHandler.h"
#include "version.h"
#include "WebLog.h"

// External globals
extern ESP8266WebServer configServer;
extern DNSServer dnsServer;
extern DomoticaEspNow espNow;

// Global flags
extern bool otaRunning;
extern bool networkDiscoveryActive;
extern unsigned long networkDiscoveryStartTime;
extern bool pingNetworkActive;
extern unsigned long pingNetworkStartTime;
extern int pingResponseCount;
extern uint8_t pingedNodesMac[MAX_PEERS][6];
extern bool pingResponseReceived[MAX_PEERS];

extern const char* BUILD_DATE;
extern const char* BUILD_TIME;

// Global OTA Status Tracker
struct OtaStatus {
    String nodeId;
    String status; // "IDLE", "TRIGGERED", "STARTING", "PROGRESS", "SUCCESS", "FAILED"
    unsigned long timestamp;
    String lastMessage;
    int progress;
};
extern OtaStatus globalOtaStatus;

// Dashboard Discovery
extern String discoveredDashboardIP;
extern unsigned long lastDashboardSeen;

// External functions
bool shouldStartConfigMode();
void resetWiFiConfig();
void totalReset();

// Helper class for chunked streaming to WebServer
class ChunkedPrint : public Print {
    ESP8266WebServer* _server;
    uint8_t _buffer[1024];
    size_t _pos;

public:
    ChunkedPrint(ESP8266WebServer* server) : _server(server), _pos(0) {}

    void flush() {
        if (_pos > 0) {
            String s;
            s.reserve(_pos);
            for(size_t i=0; i<_pos; i++) s += (char)_buffer[i];
            _server->sendContent(s);
            _pos = 0;
        }
    }

    virtual size_t write(uint8_t c) override {
        _buffer[_pos++] = c;
        if (_pos >= sizeof(_buffer)) {
            flush();
        }
        return 1;
    }
    
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        size_t written = 0;
        while (written < size) {
            size_t space = sizeof(_buffer) - _pos;
            if (space == 0) {
                flush();
                space = sizeof(_buffer);
            }
            size_t toCopy = (size - written) < space ? (size - written) : space;
            memcpy(_buffer + _pos, buffer + written, toCopy);
            _pos += toCopy;
            written += toCopy;
        }
        return written;
    }
};

// Function Prototypes
bool startWebServer();
void startStationWebServer();
void stopWebServer();
void setupWebRoutes(); // Helper to register all routes

// Page Handlers
void handleRoot();
void handleSave();
void handleSettings();
void handleNodes();
void handleOtaManager();
void handleGatewayOTA();
void handleNotFound();

// Action Handlers
void handleResetAp();
void handleFactoryReset();
void handleReboot();
void handleDeleteNode();
void handleTriggerOta();
void handleGatewayUpdate();

// API Handlers
void handleApiNodesList();
void handleApiPingNode();
void handleApiNodeStatus();
void handleApiNodeRestart();
void handleApiNodeReset();
void handleNetworkDiscovery();
void handlePingNetwork();
void handleApiForceHaDiscovery();
void handleApiOtaStatus();
void handleApiDashboardInfo(); // Aggiunto prototipo

// Helpers
void streamNetworksList(ESP8266WebServer& server);

#endif
