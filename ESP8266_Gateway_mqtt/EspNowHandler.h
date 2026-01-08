#ifndef ESP_NOW_HANDLER_H
#define ESP_NOW_HANDLER_H

#include <Arduino.h>
#include <DomoticaEspNow.h>
#include "GatewayTypes.h"
#include "Config.h"
#include "PeerHandler.h"
#include "MqttHandler.h"

// Constants
#define MESSAGE_QUEUE_SIZE 20
extern const unsigned long MESSAGE_PROCESS_INTERVAL;

// Structure for queued messages
struct QueuedMessage {
    uint8_t mac[6];
    char node[20];
    char topic[20];
    char command[20];
    char status[100];
    char type[20];
    char gateway_id[20];
    unsigned long timestamp;
    bool valid;
};

// Global variables (extern)
extern volatile bool newEspNowData;
extern struct_message receivedData;
extern DomoticaEspNow espNow;
extern char receivedMacStr[18];

// Function prototypes
bool addToMessageQueue(const uint8_t* mac, const struct_message& data);
void processMessageQueue();
void processEspNowData();
void trackEspNowSendResult(uint8_t *mac_addr, uint8_t status);
void printQueueStatus();
void processPingLogic();
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len);

// Statistics (extern)
extern unsigned long espNowSendSuccess;
extern unsigned long espNowSendFailures;
extern unsigned long totalMessagesProcessed;
extern unsigned long totalMessagesDropped;
extern unsigned long maxQueueUsage;
extern unsigned long avgProcessingTime;

extern int queueCount;
extern int queueHead;
extern int queueTail;
extern unsigned long lastMessageProcessTime;
extern bool messageProcessingActive;

// Network Discovery & Management
extern bool networkDiscoveryActive;
extern unsigned long networkDiscoveryStartTime;
extern bool pingNetworkActive;
extern unsigned long pingNetworkStartTime;
extern uint8_t pingedNodesMac[MAX_PEERS][6];
extern bool pingResponseReceived[MAX_PEERS];
extern int pingResponseCount;

extern const unsigned long NETWORK_DISCOVERY_TIMEOUT;
extern const unsigned long PING_RESPONSE_TIMEOUT;
extern const unsigned long NODE_OFFLINE_TIMEOUT;

#endif
