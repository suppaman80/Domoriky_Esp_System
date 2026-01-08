#ifndef GATEWAY_TYPES_H
#define GATEWAY_TYPES_H

#include <Arduino.h>

#define MAX_PEERS 20

struct Peer {
    uint8_t mac[6];
    char nodeId[20]; // ID del nodo associato al MAC
    char nodeType[20]; // Tipo di nodo (RELAY, SENSOR, etc.)
    char firmwareVersion[20]; // Versione firmware del nodo
    char attributes[50]; // Attributi del nodo (stato relè, sensori, etc.)
    unsigned long lastSeen; // Timestamp ultimo contatto
    bool isOnline; // Stato online/offline
};

struct NodeCommand {
    String nodeId;
    String topic;
    String command;
    unsigned long sentTime;
    bool waitingResponse;
};

#endif
