#ifndef WEBLOG_H
#define WEBLOG_H

#include <Arduino.h>
#include <deque>

class WebLog : public Print {
public:
    std::deque<String> buffer;
    const size_t maxLines = 100;
    String currentLine;
    
    virtual size_t write(uint8_t c);
    String getJSON();
    void clear();
};

extern WebLog DevLog;

#endif
