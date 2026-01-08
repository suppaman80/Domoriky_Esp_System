#include "DomoticaEspNow.h"

#ifdef ESP32
void (*DomoticaEspNow::_onDataReceived)(const uint8_t*, const uint8_t*, int) = nullptr;
#elif defined(ESP8266)
void (*DomoticaEspNow::_onDataReceived)(uint8_t*, uint8_t*, uint8_t) = nullptr;
#endif

// Variabile debug statica
static bool _debugEnabled = false;

DomoticaEspNow::DomoticaEspNow() {
}

void DomoticaEspNow::setDebug(bool debug) {
    _debugEnabled = debug;
}

void DomoticaEspNow::begin(bool master) {
  #ifdef ESP32
    // WiFi.mode(WIFI_STA); // Rimosso per evitare reset della modalità
    if (esp_now_init() != ESP_OK) {
      if(_debugEnabled) Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
  #elif defined(ESP8266)
    if (esp_now_init() != 0) {
      if(_debugEnabled) Serial.println("Errore durante l'inizializzazione di ESP-NOW");
      return;
    }
    if(master) {
        esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    } else {
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
  #endif
}

int DomoticaEspNow::addPeer(uint8_t *peer_addr) {
  #ifdef ESP32
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peer_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      if(_debugEnabled) Serial.println("Failed to add peer");
      return -1;
    }
  #elif defined(ESP8266)
    // Per ESP8266, usa ESP_NOW_ROLE_COMBO per permettere comunicazione bidirezionale
    if (esp_now_add_peer(peer_addr, ESP_NOW_ROLE_COMBO, 0, NULL, 0) != 0){
        if(_debugEnabled) Serial.println("Failed to add peer");
        return -1;
    }
  #endif
  return 0;
}

int DomoticaEspNow::removePeer(uint8_t *peer_addr) {
  #ifdef ESP32
    if (esp_now_del_peer(peer_addr) != ESP_OK){
      if(_debugEnabled) Serial.println("Failed to remove peer");
      return -1;
    }
  #elif defined(ESP8266)
    if (esp_now_del_peer(peer_addr) != 0){
      if(_debugEnabled) Serial.println("Failed to remove peer");
      return -1;
    }
  #endif
  return 0;
}

bool DomoticaEspNow::hasPeer(uint8_t *peer_addr) {
  #ifdef ESP32
    return esp_now_is_peer_exist(peer_addr);
  #elif defined(ESP8266)
    return esp_now_is_peer_exist(peer_addr) > 0;
  #endif
}

void DomoticaEspNow::clearPeers() {
    // ESP8266/ESP32 non hanno una funzione "clear all" nativa
    // Bisognerebbe tenere traccia dei peer internamente per rimuoverli tutti
    // Per ora lasciamo vuoto per evitare errori di compilazione, 
    // ma il chiamante dovrebbe gestire la rimozione individuale se necessario.
}

void DomoticaEspNow::send(uint8_t *address, const char* node, const char* topic, const char* command, const char* status, const char* type, const char* gateway_id) {
  struct_message message;
  strncpy(message.node, node, sizeof(message.node) - 1);
  strncpy(message.topic, topic, sizeof(message.topic) - 1);
  strncpy(message.command, command, sizeof(message.command) - 1);
  strncpy(message.status, status, sizeof(message.status) - 1);
  strncpy(message.type, type, sizeof(message.type) - 1);
  strncpy(message.gateway_id, gateway_id, sizeof(message.gateway_id) - 1);

  message.node[sizeof(message.node) - 1] = '\0';
  message.topic[sizeof(message.topic) - 1] = '\0';
  message.command[sizeof(message.command) - 1] = '\0';
  message.status[sizeof(message.status) - 1] = '\0';
  message.type[sizeof(message.type) - 1] = '\0';
  message.gateway_id[sizeof(message.gateway_id) - 1] = '\0';

  #ifdef ESP32
    esp_now_send(address, (uint8_t *) &message, sizeof(message));
  #elif defined(ESP8266)
    esp_now_send(address, (uint8_t *) &message, sizeof(message));
  #endif
}

#ifdef ESP32
void DomoticaEspNow::onDataReceived(void (*cb)(const uint8_t*, const uint8_t*, int)) {
    DomoticaEspNow::_onDataReceived = cb;
}
#elif defined(ESP8266)
void DomoticaEspNow::onDataReceived(void (*cb)(uint8_t*, uint8_t*, uint8_t)) {
    DomoticaEspNow::_onDataReceived = cb;
}
#endif

#ifdef ESP32
void DomoticaEspNow::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW send -> ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" : ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
void DomoticaEspNow::OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (_onDataReceived) {
        _onDataReceived(info->src_addr, incomingData, len);
    }
}
#elif defined(ESP8266)
void DomoticaEspNow::OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("[ESP8266 SEND DEBUG] Target: ");
  for(int i=0; i<6; i++) {
      Serial.print(mac_addr[i], HEX);
      if(i<5) Serial.print(":");
  }
  Serial.print(" Status: ");
  if (sendStatus == 0){
    Serial.println("SUCCESS (Delivery Confirmed)");
  } else{
    Serial.println("FAIL (No ACK)");
  }
}

void DomoticaEspNow::OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    if (_onDataReceived) {
        _onDataReceived(mac, incomingData, len);
    }
}
#endif
