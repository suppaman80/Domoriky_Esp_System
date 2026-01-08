#ifndef WEBLOG_H
#define WEBLOG_H

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

// Limit log size for ESP8266 to save RAM
#define MAX_LOG_LINES 60

class WebLog : public Print {
private:
    std::vector<String> _logs;
    String _currentLine;
    bool _serialEnabled;

public:
    WebLog() : _serialEnabled(true) {
        _logs.reserve(MAX_LOG_LINES);
    }

    void begin(unsigned long baud) {
        Serial.begin(baud);
    }

    // Override Print::write to capture output
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    void clear();
    String getJSON();
    void streamJSON(Print& output);
};

extern WebLog DevLog;

#endif
