#include "WebLog.h"
#include <time.h>

WebLog DevLog;

// Helper to add timestamp
static void addTimestamp(String& line) {
    time_t now = time(nullptr);
    if (now > 100000) { // Check if time is set
        struct tm * timeinfo = localtime(&now);
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        line += timeBuf;
    }
}

size_t WebLog::write(uint8_t c) {
    if (_serialEnabled) Serial.write(c);

    // Prepend timestamp if start of new line
    if (_currentLine.length() == 0 && c != '\n' && c != '\r') {
        _currentLine.reserve(64); // Reserve memory to reduce fragmentation
        addTimestamp(_currentLine);
    }

    if (c == '\n') {
        if (_logs.size() >= MAX_LOG_LINES) {
            _logs.erase(_logs.begin());
        }
        _logs.push_back(_currentLine);
        _currentLine = "";
    } else if (c != '\r') {
        _currentLine += (char)c;
    }
    return 1;
}

size_t WebLog::write(const uint8_t *buffer, size_t size) {
    if (_serialEnabled) Serial.write(buffer, size);
    
    size_t start = 0;
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] == '\n') {
             size_t len = i - start;
             
             // Check if we need to initialize a new line with timestamp
             if (_currentLine.length() == 0 && len > 0) {
                 _currentLine.reserve(64 + len);
                 addTimestamp(_currentLine);
             }
             
             // Append the content of this line
             if (len > 0) {
                 char temp[len + 1];
                 memcpy(temp, &buffer[start], len);
                 temp[len] = '\0';
                 _currentLine += (const char*)temp;
             }
             
             // Commit the line to logs
             if (_logs.size() >= MAX_LOG_LINES) {
                _logs.erase(_logs.begin());
             }
             _logs.push_back(_currentLine);
             _currentLine = "";
             
             start = i + 1; // Skip the newline char
        } else if (buffer[i] == '\r') {
             // Handle CR: append what we have so far, ignoring the CR itself
             size_t len = i - start;
             if (len > 0) {
                 if (_currentLine.length() == 0) {
                     _currentLine.reserve(64 + len);
                     addTimestamp(_currentLine);
                 }
                 char temp[len + 1];
                 memcpy(temp, &buffer[start], len);
                 temp[len] = '\0';
                 _currentLine += (const char*)temp;
             }
             start = i + 1; // Skip the CR char
        }
    }
    
    // Append any remaining text after the last newline
    if (start < size) {
        size_t len = size - start;
        if (len > 0) {
             if (_currentLine.length() == 0) {
                 _currentLine.reserve(64 + len);
                 addTimestamp(_currentLine);
             }
             char temp[len + 1];
             memcpy(temp, &buffer[start], len);
             temp[len] = '\0';
             _currentLine += (const char*)temp;
        }
    }
    
    return size;
}

void WebLog::clear() {
    _logs.clear();
    _currentLine = "";
}

String WebLog::getJSON() {
    // Legacy support or fallback
    DynamicJsonDocument doc(2048); // Reduced from 4096
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& line : _logs) {
        arr.add(line);
    }

    if (_currentLine.length() > 0) {
        arr.add(_currentLine);
    }

    String output;
    serializeJson(arr, output);
    return output;
}

void WebLog::streamJSON(Print& output) {
    // Manual streaming to save memory and avoid large buffer
    output.print("[");
    for (size_t i = 0; i < _logs.size(); ++i) {
        if (i > 0) output.print(",");
        
        // Serialize each string individually.
        StaticJsonDocument<512> doc; // Reduced from 1024 to save stack
        JsonVariant v = doc.to<JsonVariant>();
        v.set(_logs[i]);
        serializeJson(v, output);
    }
    
    if (_currentLine.length() > 0) {
        if (_logs.size() > 0) output.print(",");
        StaticJsonDocument<512> doc; // Reduced from 1024
        JsonVariant v = doc.to<JsonVariant>();
        v.set(_currentLine);
        serializeJson(v, output);
    }
    output.print("]");
}
