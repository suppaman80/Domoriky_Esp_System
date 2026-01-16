#ifndef PEER_HANDLER_H
#define PEER_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <DomoticaEspNow.h>
#include "GatewayTypes.h"
#include "Config.h"

// Declaration of global variables (defined in PeerHandler.cpp)
extern Peer peerList[MAX_PEERS];
extern int peerCount;

// Pending commands queue
extern NodeCommand pendingCommands[MAX_PEERS];
extern int pendingCommandsCount;
extern const unsigned long NODE_COMMAND_TIMEOUT;

// Discovery and Ping flags
extern bool networkDiscoveryActive;
extern unsigned long networkDiscoveryStartTime;
extern bool pingNetworkActive;
extern unsigned long pingNetworkStartTime;
extern int pingResponseCount;
extern uint8_t pingedNodesMac[MAX_PEERS][6];
extern bool pingResponseReceived[MAX_PEERS];

// Non-blocking Peer List sending
extern bool listPeersActive;
extern int listPeersIndex;
extern unsigned long lastPeerListSendTime;

// Function prototypes
void savePeer(const uint8_t* mac_addr, const char* nodeId = "", const char* nodeType = "", const char* firmwareVersion = "", bool forceDiscovery = false);
void loadPeersFromLittleFS();
void savePeersToLittleFS();
void printPeersList();
String macToString(const uint8_t* mac);
void listPeers();
void processPeerListSending();
void clearAllPeers();
void removePeer(const char* macAddress);
void checkAndSavePeers();
void forceSavePeers();
void processNodeCommand(const String& msgStr);
void processNodeCommandTimeout();
void processOfflineCheck();
void processNetworkDiscovery();
void processCommandResponse(const char* nodeId, const char* topic, const char* status, const uint8_t* mac);

#endif
