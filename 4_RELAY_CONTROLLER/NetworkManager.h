#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "DomoticaEspNow.h"
#include <ESP8266WiFi.h>
#include "Settings.h"
#include "StorageManager.h"

class NetworkManager {
public:
    NetworkManager(StorageManager* storage);
    
    void begin(String nodeId, String targetGatewayId);
    void update();
    
    // Funzioni di invio
    void sendDiscoveryRequest();
    void sendGatewayRegistration();
    void sendFeedback(const char* topic, const char* command, const char* status);
    void sendHeartbeat();
    
    // Stato
    bool isGatewayFound();
    void resetGateway();
    void setGatewayFound(const uint8_t* mac);
    uint8_t* getGatewayMac();
    
    // Callbacks
    void setOnDataReceived(void (*callback)(uint8_t*, uint8_t*, uint8_t));

    // EspNow Object Access
    DomoticaEspNow& getEspNow();

private:
    StorageManager* _storage;
    DomoticaEspNow _espNow;
    
    String _nodeId;
    String _targetGatewayId;
    
    uint8_t _gatewayMac[6];
    bool _gatewayFound;
    unsigned long _lastActivity;
};

#endif
