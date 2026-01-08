#ifndef DomoticaEspNow_h
#define DomoticaEspNow_h

#include "Arduino.h"

#ifdef ESP32
  #include <esp_now.h>
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <espnow.h>
  #include <ESP8266WiFi.h>
#endif

// Struttura unificata per i messaggi
typedef struct struct_message {
  char node[20];
  char topic[20];
  char command[20];
  char status[100]; // Aumentato da 20 a 100 per supportare payload lunghi (es. OTA)
  char type[20];
  char gateway_id[20];  // ID univoco del gateway per auto-discovery
} struct_message;

class DomoticaEspNow
{
  public:
    DomoticaEspNow();
    void begin(bool master = false);
    void send(uint8_t *address, const char* node, const char* topic, const char* command, const char* status, const char* type, const char* gateway_id = "");
    int addPeer(uint8_t *peer_addr);
    int removePeer(uint8_t *peer_addr);
    bool hasPeer(uint8_t *peer_addr);
    void clearPeers();
    static void setDebug(bool debug);
    #ifdef ESP32
    static void onDataReceived(void (*cb)(const uint8_t*, const uint8_t*, int));
#elif defined(ESP8266)
    static void onDataReceived(void (*cb)(uint8_t*, uint8_t*, uint8_t));
#endif

#ifdef ESP32
    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
#elif defined(ESP8266)
    static void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);
    static void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);
#endif

  private:
#ifdef ESP32
    static void (*_onDataReceived)(const uint8_t*, const uint8_t*, int);
#elif defined(ESP8266)
    static void (*_onDataReceived)(uint8_t*, uint8_t*, uint8_t);
#endif
};

#endif