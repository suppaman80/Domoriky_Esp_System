#include "WebLog.h"
#include <time.h>

size_t WebLog::write(uint8_t c) {
    // Prepend timestamp if start of new line
    if (currentLine.length() == 0 && c != '\n' && c != '\r') {
        time_t now = time(nullptr);
        if (now > 100000) { // Check if time is set (approx > 1 day after epoch)
            struct tm * timeinfo = localtime(&now);
            char timeBuf[12];
            snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            currentLine += timeBuf;
        }
    }

    Serial.write(c); // Passthrough to hardware serial
    
    if (c == '\n') {
        if (buffer.size() >= maxLines) buffer.pop_front();
        buffer.push_back(currentLine);
        currentLine = "";
    } else if (c >= 32) { // Only printable characters
        currentLine += (char)c;
    }
    return 1;
}

String WebLog::getJSON() {
    String json = "[";
    for (size_t i = 0; i < buffer.size(); i++) {
        String safeLine = buffer[i];
        safeLine.replace("\\", "\\\\");
        safeLine.replace("\"", "\\\"");
        
        json += "\"" + safeLine + "\"";
        if (i < buffer.size() - 1) json += ",";
    }
    json += "]";
    return json;
}

void WebLog::clear() {
    buffer.clear();
}

WebLog DevLog;
