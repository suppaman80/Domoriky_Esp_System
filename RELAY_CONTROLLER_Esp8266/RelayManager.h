#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include "Settings.h"

class RelayManager {
public:
    RelayManager();
    void begin();
    void configurePins(int p1, int p2, int p3, int p4);
    void setRelay(int index, bool state);
    void toggleRelay(int index);
    bool getRelayState(int index);
    void allOff();
    
    // Gestione LED
    void setLed(bool on);
    void triggerLedFeedback();
    void updateLed(bool configMode, bool gatewayFound, bool lightSleepMode);

private:
    bool _relayStates[MAX_RELAYS];
    int _relayPins[MAX_RELAYS] = {DEFAULT_RELAY1_PIN, DEFAULT_RELAY2_PIN, DEFAULT_RELAY3_PIN, DEFAULT_RELAY4_PIN, DEFAULT_RELAY5_PIN, DEFAULT_RELAY6_PIN};
    
    // Variabili LED
    unsigned long _ledOnTime;
    unsigned long _lastLedToggle;
    bool _ledBlinkState;
};

#endif
