#include "WebLog.h"
#include <time.h>

WebLog DevLog;

size_t WebLog::write(uint8_t c) {
    if (_serialEnabled) Serial.write(c);

    // Prepend timestamp if start of new line
    if (_currentLine.length() == 0 && c != '\n' && c != '\r') {
        time_t now = time(nullptr);
        if (now > 100000) { // Check if time is set (approx > 1 day after epoch)
            struct tm * timeinfo = localtime(&now);
            char timeBuf[12];
            snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            _currentLine += timeBuf;
        }
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
    size_t n = 0;
    while (size--) {
        n += write(*buffer++);
    }
    return n;
}

void WebLog::clear() {
    _logs.clear();
    _currentLine = "";
}

String WebLog::getJSON() {
    // Legacy support or fallback
    DynamicJsonDocument doc(4096);
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
    // Create a temporary document just for serialization
    // 4096 is quite large for stack, but standard for this project apparently.
    // Ideally we would manually stream the JSON array to avoid this buffer.
    
    // Manual streaming to save memory and avoid large buffer
    output.print("[");
    for (size_t i = 0; i < _logs.size(); ++i) {
        if (i > 0) output.print(",");
        
        // Serialize each string individually.
        StaticJsonDocument<1024> doc; // Reused buffer
        JsonVariant v = doc.to<JsonVariant>();
        v.set(_logs[i]);
        serializeJson(v, output);
    }
    
    if (_currentLine.length() > 0) {
        if (_logs.size() > 0) output.print(",");
        StaticJsonDocument<1024> doc;
        JsonVariant v = doc.to<JsonVariant>();
        v.set(_currentLine);
        serializeJson(v, output);
    }
    output.print("]");
}
