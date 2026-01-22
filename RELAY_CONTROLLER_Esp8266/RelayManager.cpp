#include "RelayManager.h"

RelayManager::RelayManager() {
    for(int i=0; i<MAX_RELAYS; i++) _relayStates[i] = false;
    _ledOnTime = 0;
    _lastLedToggle = 0;
    _ledBlinkState = false;
}

void RelayManager::begin() {
    for(int i=0; i<MAX_RELAYS; i++) {
        if (_relayPins[i] != PIN_DISABLED) {
            pinMode(_relayPins[i], OUTPUT);
            digitalWrite(_relayPins[i], LOW);
        }
    }
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, HIGH); // LED spento (Active LOW)
}

void RelayManager::configurePins(int p1, int p2, int p3, int p4) {
    // Legacy support (4 pins)
    _relayPins[0] = p1;
    _relayPins[1] = p2;
    _relayPins[2] = p3;
    _relayPins[3] = p4;
    _relayPins[4] = PIN_DISABLED;
    _relayPins[5] = PIN_DISABLED;
}

void RelayManager::setRelay(int index, bool state) {
    if(index >= 0 && index < MAX_RELAYS) {
        if (_relayPins[index] != PIN_DISABLED) {
            _relayStates[index] = state;
            digitalWrite(_relayPins[index], state ? HIGH : LOW);
        }
    }
}

void RelayManager::toggleRelay(int index) {
    if(index >= 0 && index < MAX_RELAYS) {
        if (_relayPins[index] != PIN_DISABLED) {
            setRelay(index, !_relayStates[index]);
        }
    }
}

bool RelayManager::getRelayState(int index) {
    if(index >= 0 && index < MAX_RELAYS) {
        return _relayStates[index];
    }
    return false;
}

void RelayManager::allOff() {
    for(int i=0; i<MAX_RELAYS; i++) {
        setRelay(i, false);
    }
}

void RelayManager::setLed(bool on) {
    digitalWrite(LED_STATUS, on ? LOW : HIGH); // Active LOW
}

void RelayManager::triggerLedFeedback() {
    setLed(true);
    _ledOnTime = millis();
}

void RelayManager::updateLed(bool configMode, bool gatewayFound, bool lightSleepMode) {
    unsigned long currentTime = millis();
    
    // Spegnimento automatico dopo feedback comando
    if (_ledOnTime > 0 && currentTime - _ledOnTime >= LED_FEEDBACK_DURATION) {
        setLed(false);
        _ledOnTime = 0;
    }
    
    // Se c'è un feedback attivo, non fare lampeggi
    if (_ledOnTime > 0) return;

    if (configMode) {
        // Lampeggio veloce in config mode
        if (currentTime - _lastLedToggle >= 100) {
            _ledBlinkState = !_ledBlinkState;
            setLed(_ledBlinkState);
            _lastLedToggle = currentTime;
        }
    } else {
        if (!gatewayFound) {
            // Lampeggio rapido (200ms) quando gateway non trovato
            if (currentTime - _lastLedToggle >= 200) {
                _ledBlinkState = !_ledBlinkState;
                setLed(_ledBlinkState);
                _lastLedToggle = currentTime;
            }
        } else {
            // Lampeggio lento (1s) heartbeat visivo
            if (currentTime - _lastLedToggle >= 1000) {
                _ledBlinkState = !_ledBlinkState;
                setLed(_ledBlinkState);
                _lastLedToggle = currentTime;
            }
        }
        
        // Se in light sleep profondo, spegni LED per risparmiare
        if (lightSleepMode && _ledOnTime == 0) {
            setLed(false);
        }
    }
}
