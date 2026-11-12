#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED
#if defined(ESP8266_RTOS)

#include <Arduino.h>
#include <pgmspace.h>

#define SPI_HAS_TRANSACTION 1
#define SPI_HAS_NOTUSINGINTERRUPT 1

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define MSBFIRST 1
#define LSBFIRST 0

class SPISettings {
public:
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode){
    _clock = clock;
    _bitOrder = bitOrder;
    _dataMode = dataMode;
  }
  SPISettings(){ _clock=4000000; _bitOrder=MSBFIRST; _dataMode=SPI_MODE0; }
  uint32_t _clock;
  uint8_t _bitOrder;
  uint8_t _dataMode;
};

class SPIClass {
public:
  SPIClass();
  void begin();
  void end();
  void setDataBits(uint16_t bits);
  void setFrequency(uint32_t freq);
  void beginTransaction(SPISettings settings);
  void endTransaction();
  void setBitOrder(uint8_t order);
  void setDataMode(uint8_t mode);
  void setClockDivider(uint32_t div);
  void writeBytes(const uint8_t *data, size_t size);
  uint8_t transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);
  void transfer(void *data, uint32_t size);
  size_t write(uint8_t data);
  size_t write(const uint8_t *data, size_t size);
  size_t write16(uint16_t data);
  size_t write16(uint16_t data, bool msb);
  size_t write32(uint32_t data);
  size_t write32(uint32_t data, bool msb);
  void writePattern(uint16_t data, size_t size);
private:
  uint32_t _frequency;
  uint8_t _dataMode;
  uint8_t _bitOrder;
  bool _initialized;
};

extern SPIClass SPI;
#else
#include_next <SPI.h>
#endif
#endif
