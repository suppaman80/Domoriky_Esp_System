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
    bool _relayStates[4];
    int _relayPins[4] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
    
    // Variabili LED
    unsigned long _ledOnTime;
    unsigned long _lastLedToggle;
    bool _ledBlinkState;
};

#endif
