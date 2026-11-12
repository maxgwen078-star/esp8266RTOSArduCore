#if defined(ESP8266_RTOS)
#ifndef _WIRE_H_INCLUDED
#define _WIRE_H_INCLUDED
#include <Arduino.h>

class TwoWire {
public:
    TwoWire() {}
    void begin() {}
    void begin(int, int) {}
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission() { return 0; }
    uint8_t endTransmission(bool) { return 0; }
    uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
    uint8_t requestFrom(uint8_t, uint8_t, bool) { return 0; }
    uint8_t requestFrom(uint8_t, uint8_t, uint8_t) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t* data, size_t size) { return size; }
    int read() { return -1; }
    int available() { return 0; }
    int peek() { return -1; }
    void flush() {}
};
extern TwoWire Wire;
#endif
#endif