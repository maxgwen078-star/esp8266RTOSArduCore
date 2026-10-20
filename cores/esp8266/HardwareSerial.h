#ifndef HardwareSerial_h
#define HardwareSerial_h

#include <inttypes.h>
#include "Stream.h"
#include "esp32-hal.h"
#include "esp32-hal-uart.h"

// ---- Compat avec le core ESP8266 Arduino ----
typedef uint32_t SerialConfig;
typedef enum {
    SERIAL_FULL    = 0,
    SERIAL_RX_ONLY = 1,
    SERIAL_TX_ONLY = 2
} SerialMode;

#define SERIAL_5N1 UART_5N1
#define SERIAL_6N1 UART_6N1
#define SERIAL_7N1 UART_7N1
#define SERIAL_8N1 UART_8N1
#define SERIAL_5N2 UART_5N2
#define SERIAL_6N2 UART_6N2
#define SERIAL_7N2 UART_7N2
#define SERIAL_8N2 UART_8N2
#define SERIAL_5E1 UART_5E1
#define SERIAL_6E1 UART_6E1
#define SERIAL_7E1 UART_7E1
#define SERIAL_8E1 UART_8E1
#define SERIAL_5E2 UART_5E2
#define SERIAL_6E2 UART_6E2
#define SERIAL_7E2 UART_7E2
#define SERIAL_8E2 UART_8E2
#define SERIAL_5O1 UART_5O1
#define SERIAL_6O1 UART_6O1
#define SERIAL_7O1 UART_7O1
#define SERIAL_8O1 UART_8O1
#define SERIAL_5O2 UART_5O2
#define SERIAL_6O2 UART_6O2
#define SERIAL_7O2 UART_7O2
#define SERIAL_8O2 UART_8O2

class HardwareSerial: public Stream
{
public:
    HardwareSerial(int uart_nr);

    // API ESP32-like
    void begin(unsigned long baud, uint32_t config=UART_8N1,
               int8_t rxPin=-1, int8_t txPin=-1,
               bool invert=false, unsigned long timeout_ms=20000UL);

    // API ESP8266 Arduino core compatible
    void begin(unsigned long baud, SerialConfig config, SerialMode mode,
               int8_t rxPin=-1, int8_t txPin=-1,
               bool invert=false, unsigned long timeout_ms=20000UL);

    void end();
    void updateBaudRate(unsigned long baud);
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    inline size_t read(char * buffer, size_t size) { return read((uint8_t*) buffer, size); }
    void flush(void);
    void flush(bool txOnly);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    inline size_t write(const char * buffer, size_t size) { return write((uint8_t*) buffer, size); }
    inline size_t write(const char * s) { return write((uint8_t*) s, strlen(s)); }
    inline size_t write(unsigned long n) { return write((uint8_t) n); }
    inline size_t write(long n)          { return write((uint8_t) n); }
    inline size_t write(unsigned int n)  { return write((uint8_t) n); }
    inline size_t write(int n)           { return write((uint8_t) n); }
    uint32_t baudRate();
    operator bool() const;

    size_t setRxBufferSize(size_t);
    void setDebugOutput(bool);
    void setRxInvert(bool);

    // ---- Compat ESP8266 Arduino core : swap UART0 ----
    bool swap(bool swap = true);
    bool isSwapped() const;

protected:
    int _uart_nr;
    uart_t* _uart;
    int8_t _tx_pin;
    int8_t _rx_pin;
    size_t _rx_buffer_size;
};

extern void serialEventRun(void) __attribute__((weak));

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
extern HardwareSerial Serial;
extern HardwareSerial Serial1;
#endif

#endif // HardwareSerial_h