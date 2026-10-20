#include "HardwareSerial.h"     // inclut déjà esp32-hal.h (FreeRTOS OK)
#include "esp32-hal.h"
#include "esp32-hal-log.h"
#include "driver/uart.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "pins_arduino.h"

// ESP8266 : TX0 = GPIO1, RX0 = GPIO3
#ifndef RX0
#define RX0 3
#endif
#ifndef TX0
#define TX0 1
#endif
// ESP8266 : UART1 = TX-only sur GPIO2
#ifndef TX1
#define TX1 2
#endif

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
HardwareSerial Serial(0);
HardwareSerial Serial1(1);
#endif

HardwareSerial::HardwareSerial(int uart_nr)
    : _uart_nr(uart_nr), _uart(NULL), _tx_pin(-1), _rx_pin(-1), _rx_buffer_size(0)
{}

// ============================================================
// begin() — API ESP32-like
// ============================================================
void HardwareSerial::begin(unsigned long baud, uint32_t config,
                           int8_t rxPin, int8_t txPin,
                           bool invert, unsigned long timeout_ms)
{
    if(0 > _uart_nr || _uart_nr > (UART_NUM_MAX - 1)) {
        log_e("Serial number is invalid, please use 0 or 1");
        return;
    }
    if(_uart) {
        end();
    }

    // UART0 : défauts GPIO3 (RX) / GPIO1 (TX)
    if(_uart_nr == 0 && rxPin < 0 && txPin < 0) {
        rxPin = RX0;
        txPin = TX0;
    }
    // UART1 : TX-only, on ne touche pas à rxPin
    if(_uart_nr == 1 && txPin < 0) {
        txPin = TX1;
    }

    _uart = uartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, 256, invert);
    _tx_pin = txPin;
    _rx_pin = rxPin;

    if(!baud) {
        uartStartDetectBaudrate(_uart);
        time_t startMillis = millis();
        unsigned long detectedBaudRate = 0;
        while(millis() - startMillis < timeout_ms &&
              !(detectedBaudRate = uartDetectBaudrate(_uart))) {
            yield();
        }

        end();

        if(detectedBaudRate) {
            delay(100);
            _uart = uartBegin(_uart_nr, detectedBaudRate, config, rxPin, txPin, 256, invert);
        } else {
            log_e("Could not detect baudrate");
            _uart = NULL;
            _tx_pin = -1;
            _rx_pin = -1;
        }
    }
}

// ============================================================
// begin() — API ESP8266 Arduino core compatible
// ============================================================
void HardwareSerial::begin(unsigned long baud, SerialConfig config, SerialMode mode,
                           int8_t rxPin, int8_t txPin,
                           bool invert, unsigned long timeout_ms)
{
    if(mode == SERIAL_RX_ONLY) {
        txPin = -1;
    } else if(mode == SERIAL_TX_ONLY) {
        rxPin = -1;
    }
    begin(baud, config, rxPin, txPin, invert, timeout_ms);
}

void HardwareSerial::updateBaudRate(unsigned long baud)
{
    uartSetBaudRate(_uart, baud);
}

void HardwareSerial::end()
{
    if(uartGetDebug() == _uart_nr) {
        uartSetDebug(0);
    }
    log_v("pins %d %d", _tx_pin, _rx_pin);
    uartEnd(_uart, _rx_pin, _tx_pin);
    _uart = 0;
}

size_t HardwareSerial::setRxBufferSize(size_t new_size)
{
    if(_uart == NULL) {
        _rx_buffer_size = new_size;
        return new_size;
    }
    return uartResizeRxBuffer(_uart, new_size);
}

void HardwareSerial::setDebugOutput(bool en)
{
    if(_uart == 0) return;
    if(en) {
        uartSetDebug(_uart);
    } else {
        if(uartGetDebug() == _uart_nr) {
            uartSetDebug(0);
        }
    }
}

bool HardwareSerial::swap(bool swap)
{
    if(_uart == NULL || _uart_nr != 0) return false;
    uartSwapPins(_uart, swap);
    return true;
}

bool HardwareSerial::isSwapped() const
{
    if(_uart == NULL) return false;
    return uartIsSwapped(_uart);
}

int HardwareSerial::available(void)        { return uartAvailable(_uart); }
int HardwareSerial::availableForWrite(void) { return uartAvailableForWrite(_uart); }

int HardwareSerial::peek(void)
{
    if (available()) return uartPeek(_uart);
    return -1;
}

int HardwareSerial::read(void)
{
    if(available()) return uartRead(_uart);
    return -1;
}

size_t HardwareSerial::read(uint8_t *buffer, size_t size)
{
    size_t avail = available();
    if (size < avail) avail = size;
    size_t count = 0;
    while(count < avail) {
        *buffer++ = uartRead(_uart);
        count++;
    }
    return count;
}

void HardwareSerial::flush(void)        { uartFlush(_uart); }
void HardwareSerial::flush(bool txOnly) { uartFlushTxOnly(_uart, txOnly); }

size_t HardwareSerial::write(uint8_t c)
{
    uartWrite(_uart, c);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    uartWriteBuf(_uart, buffer, size);
    return size;
}

uint32_t HardwareSerial::baudRate() { return uartGetBaudRate(_uart); }

HardwareSerial::operator bool() const { return true; }

void HardwareSerial::setRxInvert(bool invert) { uartSetRxInvert(_uart, invert); }