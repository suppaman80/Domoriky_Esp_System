#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "StorageManager.h"
#include "Settings.h"
#include "version.h"

class WebConfig {
public:
    WebConfig(StorageManager* storage);
    void begin(String currentNodeId, String currentGatewayId, int* currentPins);
    void handle();
    bool isSaved();
    
private:
    StorageManager* _storage;
    ESP8266WebServer _server;
    DNSServer _dnsServer;
    
    String _currentNodeId;
    String _currentGatewayId;
    int _currentPins[4];
    bool _configSaved;
    
    // Handlers
    void handleRoot();
    void handleSave();
};

#endif
