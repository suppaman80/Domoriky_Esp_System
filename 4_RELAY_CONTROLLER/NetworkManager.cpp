#include "NetworkManager.h"

NetworkManager::NetworkManager(StorageManager* storage) {
    _storage = storage;
    _gatewayFound = false;
    memset(_gatewayMac, 0, 6);
    _lastActivity = 0;
}

void NetworkManager::begin(String nodeId, String targetGatewayId) {
    _nodeId = nodeId;
    _targetGatewayId = targetGatewayId;
    
    WiFi.mode(WIFI_STA);
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    _espNow.begin(false);
    
    // Add broadcast peer
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    _espNow.addPeer(broadcastAddress);
    
    // Try loading gateway MAC
    if (_storage->loadGatewayMac(_gatewayMac)) {
        _gatewayFound = true;
        _espNow.addPeer(_gatewayMac);
        Serial.println("Gateway MAC caricato da Storage");
    } else {
        Serial.println("Gateway MAC non trovato - Discovery necessario");
    }
}

void NetworkManager::setOnDataReceived(void (*callback)(uint8_t*, uint8_t*, uint8_t)) {
    DomoticaEspNow::onDataReceived(callback);
}

void NetworkManager::sendDiscoveryRequest() {
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    _espNow.send(broadcastAddress, _nodeId.c_str(), "DISCOVERY", "REQUEST", _targetGatewayId.c_str(), "DISCOVERY", _targetGatewayId.c_str());
}

void NetworkManager::sendGatewayRegistration() {
    if (_gatewayFound) {
        if (!_espNow.hasPeer(_gatewayMac)) { _espNow.addPeer(_gatewayMac); }
        _espNow.send(_gatewayMac, _nodeId.c_str(), "CONTROL", "REGISTER", NODE_TYPE, "REGISTRATION", _targetGatewayId.c_str());
    }
}

void NetworkManager::sendFeedback(const char* topic, const char* command, const char* status) {
    if (_gatewayFound) {
        if (!_espNow.hasPeer(_gatewayMac)) { _espNow.addPeer(_gatewayMac); }
        _espNow.send(_gatewayMac, _nodeId.c_str(), topic, command, status, "FEEDBACK", _targetGatewayId.c_str());
    }
}

void NetworkManager::sendHeartbeat() {
    if (_gatewayFound) {
        sendFeedback("STATUS", "HEARTBEAT", "ONLINE");
    }
}

bool NetworkManager::isGatewayFound() {
    return _gatewayFound;
}

void NetworkManager::resetGateway() {
    _gatewayFound = false;
    memset(_gatewayMac, 0, 6);
    _storage->deleteGatewayMac();
}

void NetworkManager::setGatewayFound(const uint8_t* mac) {
    memcpy(_gatewayMac, mac, 6);
    _gatewayFound = true;
    _storage->saveGatewayMac(_gatewayMac);
    if (!_espNow.hasPeer(_gatewayMac)) { _espNow.addPeer(_gatewayMac); }
}

uint8_t* NetworkManager::getGatewayMac() {
    return _gatewayMac;
}

DomoticaEspNow& NetworkManager::getEspNow() {
    return _espNow;
}

void NetworkManager::update() {
    // Gestione interna se necessaria
}
