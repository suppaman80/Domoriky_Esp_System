#include "WebConfig.h"

WebConfig::WebConfig(StorageManager* storage) : _server(80) {
    _storage = storage;
    _configSaved = false;
}

void WebConfig::begin(String currentNodeId, String currentGatewayId, int* currentPins) {
    _currentNodeId = currentNodeId;
    _currentGatewayId = currentGatewayId;
    for(int i=0; i<4; i++) _currentPins[i] = currentPins[i];
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    
    _dnsServer.start(53, "*", WiFi.softAPIP());
    
    _server.on("/", std::bind(&WebConfig::handleRoot, this));
    _server.on("/save", HTTP_POST, std::bind(&WebConfig::handleSave, this));
    _server.onNotFound(std::bind(&WebConfig::handleRoot, this));
    _server.begin();
    
    Serial.println("WebConfig avviato su " + WiFi.softAPIP().toString());
}

void WebConfig::handle() {
    _dnsServer.processNextRequest();
    _server.handleClient();
}

bool WebConfig::isSaved() {
    return _configSaved;
}

static String getPinOptions(int selected) {
    String options = "";
    struct PinDef { int gpio; const char* name; };
    PinDef pins[] = {
        {16, "GPIO 16 (D0)"},
        {5, "GPIO 5 (D1)"},
        {4, "GPIO 4 (D2)"},
        {0, "GPIO 0 (D3)"},
        {2, "GPIO 2 (D4)"},
        {14, "GPIO 14 (D5)"},
        {12, "GPIO 12 (D6)"},
        {13, "GPIO 13 (D7)"},
        {15, "GPIO 15 (D8)"},
        {3, "GPIO 3 (RX)"},
        {1, "GPIO 1 (TX)"}
    };
    
    for(int i=0; i<11; i++) {
        options += "<option value='" + String(pins[i].gpio) + "'";
        if(pins[i].gpio == selected) options += " selected";
        options += ">" + String(pins[i].name) + "</option>";
    }
    return options;
}

void WebConfig::handleRoot() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Configurazione Relay Node</title>";
    html += "<meta charset='UTF-8'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 500px; margin: 0 auto; }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 20px; font-size: 24px; }";
    html += "label { display: block; margin: 10px 0 5px 0; font-weight: bold; color: #555; }";
    html += "input[type='text'], select { width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
    html += "input[type='text']:focus, select:focus { border-color: #4CAF50; outline: none; }";
    html += "button { background: #4CAF50; color: white; padding: 15px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; margin-top: 20px; }";
    html += "button:hover { background: #45a049; }";
    html += ".info { background: #e7f3ff; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3; font-size: 14px; }";
    html += ".pin-group { background: #f9f9f9; padding: 10px; border-radius: 5px; margin-top: 10px; border: 1px solid #eee; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🔧 Configurazione Relay Node</h1>";
    html += "<p style='text-align: center; color: #666; font-size: 12px;'>FW: " + String(FIRMWARE_VERSION) + "</p>";
    html += "<div class='info'>";
    html += "<strong>Istruzioni:</strong><br>";
    html += "• Configura ID Nodo e Gateway<br>";
    html += "• Assegna i GPIO ai Relè<br>";
    html += "• Salva per riavviare";
    html += "</div>";
    html += "<form action='/save' method='POST'>";
    
    html += "<label for='nodeId'>ID Nodo Relay:</label>";
    html += "<input type='text' id='nodeId' name='nodeId' value='" + _currentNodeId + "' required>";
    
    html += "<label for='gatewayId'>ID Gateway Destinazione:</label>";
    html += "<input type='text' id='gatewayId' name='gatewayId' value='" + _currentGatewayId + "' required>";
    
    html += "<div class='pin-group'>";
    html += "<h3 style='margin:5px 0 10px 0;color:#666'>Assegnazione GPIO Relè</h3>";
    for(int i=0; i<4; i++) {
        html += "<label for='pin" + String(i) + "'>Relè " + String(i+1) + ":</label>";
        html += "<select name='pin" + String(i) + "' id='pin" + String(i) + "'>";
        html += getPinOptions(_currentPins[i]);
        html += "</select>";
    }
    html += "</div>";
    
    html += "<button type='submit'>💾 Salva e Riavvia</button>";
    html += "</form>";
    html += "</div></body></html>";
    
    _server.send(200, "text/html", html);
}

void WebConfig::handleSave() {
    if (_server.hasArg("nodeId") && _server.hasArg("gatewayId")) {
        String nodeId = _server.arg("nodeId");
        String gatewayId = _server.arg("gatewayId");
        int pins[4];
        
        nodeId.trim();
        gatewayId.trim();
        
        // Leggi PIN
        for(int i=0; i<4; i++) {
            String argName = "pin" + String(i);
            if(_server.hasArg(argName)) {
                pins[i] = _server.arg(argName).toInt();
            } else {
                pins[i] = _currentPins[i]; // Fallback
            }
        }
        
        if (nodeId.length() > 0 && gatewayId.length() > 0) {
            if (_storage->saveConfig(nodeId, gatewayId, pins)) {
                String html = "<!DOCTYPE html><html><body><h1>✅ Configurazione Salvata!</h1><p>Riavvio in corso...</p></body></html>";
                _server.send(200, "text/html", html);
                _configSaved = true;
                return;
            }
        }
    }
    _server.send(400, "text/plain", "Errore parametri");
}
