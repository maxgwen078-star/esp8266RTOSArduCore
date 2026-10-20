#ifndef __SoftwareSerial_h
#define __SoftwareSerial_h

#ifndef ESP8266
#define ESP8266
#endif

#include <circular_queue.h>
#include <Stream.h>
#include <Print.h>
#include <Arduino.h>

#include <cstdint>
#include <memory>

#ifndef ALWAYS_INLINE_ATTR
#define ALWAYS_INLINE_ATTR __attribute__((always_inline))
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR ICACHE_RAM_ATTR
#endif

// ============================================================
// CPU FREQ — adapter selon ta config IDE (Outils > CPU Frequency)
// ============================================================
// 160 MHz
//#define FSS_CPU_FREQ_HZ  160000000UL
// 80 MHz
#define FSS_CPU_FREQ_HZ   80000000UL

// ============================================================
// Tick = ccount (cycles CPU) — IRAM-safe, jamais de flash access
// ============================================================
static inline uint32_t IRAM_ATTR fss_ccount(void) {
    uint32_t cc;
    __asm__ __volatile__("rsr %0, ccount" : "=a"(cc));
    return cc;
}

namespace EspSoftwareSerial {

class IGpioCapabilities {
public:
    static constexpr bool isValidPin(int8_t pin);
    static constexpr bool isValidInputPin(int8_t pin);
    static constexpr bool isValidOutputPin(int8_t pin);
    static constexpr bool hasPullUp(int8_t pin);
};

class GpioCapabilities : private IGpioCapabilities {
public:
    static constexpr bool isValidPin(int8_t pin) {
        return (pin >= 0 && pin <= 16);
    }
    static constexpr bool isValidInputPin(int8_t pin) {
        return isValidPin(pin) && (pin != 16);
    }
    static constexpr bool isValidOutputPin(int8_t pin) {
        return isValidPin(pin);
    }
    static constexpr bool hasPullUp(int8_t pin) {
        return pin >= 0;
    }
};

enum Parity : uint8_t {
    PARITY_NONE  = 000,
    PARITY_EVEN  = 020,
    PARITY_ODD   = 030,
    PARITY_MARK  = 040,
    PARITY_SPACE = 070,
};

enum Config {
    SWSERIAL_5N1 = PARITY_NONE,
    SWSERIAL_6N1,
    SWSERIAL_7N1,
    SWSERIAL_8N1,
    SWSERIAL_5E1 = PARITY_EVEN,
    SWSERIAL_6E1,
    SWSERIAL_7E1,
    SWSERIAL_8E1,
    SWSERIAL_5O1 = PARITY_ODD,
    SWSERIAL_6O1,
    SWSERIAL_7O1,
    SWSERIAL_8O1,
    SWSERIAL_5M1 = PARITY_MARK,
    SWSERIAL_6M1,
    SWSERIAL_7M1,
    SWSERIAL_8M1,
    SWSERIAL_5S1 = PARITY_SPACE,
    SWSERIAL_6S1,
    SWSERIAL_7S1,
    SWSERIAL_8S1,
    SWSERIAL_5N2 = 0200 | PARITY_NONE,
    SWSERIAL_6N2,
    SWSERIAL_7N2,
    SWSERIAL_8N2,
    SWSERIAL_5E2 = 0200 | PARITY_EVEN,
    SWSERIAL_6E2,
    SWSERIAL_7E2,
    SWSERIAL_8E2,
    SWSERIAL_5O2 = 0200 | PARITY_ODD,
    SWSERIAL_6O2,
    SWSERIAL_7O2,
    SWSERIAL_8O2,
    SWSERIAL_5M2 = 0200 | PARITY_MARK,
    SWSERIAL_6M2,
    SWSERIAL_7M2,
    SWSERIAL_8M2,
    SWSERIAL_5S2 = 0200 | PARITY_SPACE,
    SWSERIAL_6S2,
    SWSERIAL_7S2,
    SWSERIAL_8S2,
};

class UARTBase : public Stream {
public:
    UARTBase();
    UARTBase(int8_t rxPin, int8_t txPin = -1, bool invert = false);
    UARTBase(const UARTBase&) = delete;
    UARTBase& operator= (const UARTBase&) = delete;
    virtual ~UARTBase();

    void begin(uint32_t baud, Config config,
        int8_t rxPin, int8_t txPin, bool invert);

    uint32_t baudRate();
    void setTransmitEnablePin(int8_t txEnablePin);
    void enableIntTx(bool on);
    void enableRxGPIOPullUp(bool on);
    void enableTxGPIOOpenDrain(bool on);

    bool overflow();

    int available() override;
    int availableForWrite() override;
    int peek() override;
    int read() override;

    bool readParity() { return m_lastReadParity; }

    static bool parityEven(uint8_t byte) {
        byte ^= byte >> 4;
        byte &= 0xf;
        return (0x6996 >> byte) & 1;
    }
    static bool parityOdd(uint8_t byte) {
        byte ^= byte >> 4;
        byte &= 0xf;
        return (0x9669 >> byte) & 1;
    }

    int read(uint8_t* buffer, size_t size);
    int read(char* buffer, size_t size) {
        return read(reinterpret_cast<uint8_t*>(buffer), size);
    }
    size_t readBytes(uint8_t* buffer, size_t size) override;
    size_t readBytes(char* buffer, size_t size) override {
        return readBytes(reinterpret_cast<uint8_t*>(buffer), size);
    }
    void flush() override;

    size_t write(uint8_t byte) override;
    size_t write(uint8_t byte, Parity parity);
    size_t write(const uint8_t* buffer, size_t size) override;
    size_t write(const char* buffer, size_t size) {
        return write(reinterpret_cast<const uint8_t*>(buffer), size);
    }
    size_t write(const uint8_t* buffer, size_t size, Parity parity);
    size_t write(const char* buffer, size_t size, Parity parity) {
        return write(reinterpret_cast<const uint8_t*>(buffer), size, parity);
    }

    operator bool() const {
        return (-1 == m_rxPin || m_rxValid) && (-1 == m_txPin || m_txValid)
               && !(-1 == m_rxPin && m_oneWire);
    }

    void enableRx(bool on);
    void enableTx(bool on);

    bool listen() { enableRx(true); return true; }
    void end();
    bool isListening() { return m_rxEnabled; }
    bool stopListening() { enableRx(false); return true; }

    void onReceive(const Delegate<void(), void*>& handler);
    void onReceive(Delegate<void(), void*>&& handler);

    [[deprecated("function removed; semantics of onReceive() changed.")]]
    void perform_work();

    static void IRAM_ATTR rxBitISR(UARTBase* self);
    static void IRAM_ATTR rxBitSyncISR(UARTBase* self);

    using Print::write;

protected:
    void beginRx(bool hasPullUp, int bufCapacity, int isrBufCapacity);
    void beginTx();

    int8_t m_rxPin = -1;
    int8_t m_txPin = -1;
    bool   m_invert = false;

private:
    void lazyDelay();
    void preciseDelay();
    void writePeriod(uint32_t dutyCycle, uint32_t offCycle, bool withStopBit);
    void setRxGPIOPinMode();
    void setTxGPIOPinMode();
    void rxBits();
    void rxBits(const uint32_t isrTick);
    static void disableInterrupts();
    static void restoreInterrupts();

    // ============================================================
    // Tick = ccount (cycles CPU)
    // ============================================================
    static inline uint32_t IRAM_ATTR ticks() {
        return fss_ccount();
    }
    static inline uint32_t IRAM_ATTR microsToTicks(uint32_t us) {
        return us * (FSS_CPU_FREQ_HZ / 1000000UL);
    }
    static inline uint32_t IRAM_ATTR ticksToMicros(uint32_t tks) {
        return tks / (FSS_CPU_FREQ_HZ / 1000000UL);
    }

    volatile uint32_t* m_rxReg = nullptr;
    uint32_t m_rxBitMask = 0;
    uint32_t m_txBitMask = 0;
    int8_t   m_txEnablePin = -1;
    uint8_t  m_dataBits = 8;
    bool     m_oneWire = false;
    bool     m_rxValid = false;
    bool     m_rxEnabled = false;
    bool     m_txValid = false;
    bool     m_txEnableValid = false;
    uint8_t  m_pduBits = 10;
    bool     m_intTxEnabled = true;
    bool     m_rxGPIOHasPullUp = false;
    bool     m_rxGPIOPullUpEnabled = true;
    bool     m_txGPIOOpenDrain = false;
    Parity   m_parityMode = PARITY_NONE;
    uint8_t  m_stopBits = 1;
    bool     m_lastReadParity = false;
    bool     m_overflow = false;
    uint32_t m_bitTicks = 0;
    uint8_t  m_parityInPos = 1;
    uint8_t  m_parityOutPos = 1;
    int8_t   m_rxLastBit = 0;
    uint8_t  m_rxCurByte = 0;

    std::unique_ptr<circular_queue<uint8_t> > m_buffer;
    std::unique_ptr<circular_queue<uint8_t> > m_parityBuffer;

    uint32_t m_periodStart = 0;
    uint32_t m_periodDuration = 0;

    volatile bool m_isrOverflow = false;

    std::unique_ptr<circular_queue<uint32_t, UARTBase*> > m_isrBuffer;
    const Delegate<void(uint32_t&&), UARTBase*> m_isrBufferForEachDel = {
        [](UARTBase* self, uint32_t&& isrTick) { self->rxBits(isrTick); }, this
    };
    uint32_t m_isrLastTick = 0;
    bool     m_rxCurParity = false;
    Delegate<void(), void*> m_rxHandler;
};

template< class GpioCapabilities > class BasicUART : public UARTBase {
    static_assert(std::is_base_of<IGpioCapabilities, GpioCapabilities>::value,
        "template argument is not derived from IGpioCapabilities");
public:
    BasicUART() : UARTBase() {}
    BasicUART(int8_t rxPin, int8_t txPin = -1, bool invert = false) :
        UARTBase(rxPin, txPin, invert) {}

    void begin(uint32_t baud, Config config,
        int8_t rxPin, int8_t txPin, bool invert,
        int bufCapacity = 64, int isrBufCapacity = 0) {
        UARTBase::begin(baud, config, rxPin, txPin, invert);
        if (GpioCapabilities::isValidInputPin(rxPin)) {
            beginRx(GpioCapabilities::hasPullUp(rxPin), bufCapacity, isrBufCapacity);
        }
        if (GpioCapabilities::isValidOutputPin(txPin)) {
            beginTx();
        }
        enableRx(true);
    }
    void begin(uint32_t baud, Config config, int8_t rxPin, int8_t txPin) {
        begin(baud, config, rxPin, txPin, m_invert);
    }
    void begin(uint32_t baud, Config config, int8_t rxPin) {
        begin(baud, config, rxPin, m_txPin, m_invert);
    }
    void begin(uint32_t baud, Config config = SWSERIAL_8N1) {
        begin(baud, config, m_rxPin, m_txPin, m_invert);
    }
    void setTransmitEnablePin(int8_t txEnablePin) {
        UARTBase::setTransmitEnablePin(
            GpioCapabilities::isValidOutputPin(txEnablePin) ? txEnablePin : -1);
    }
};

using UART = BasicUART< GpioCapabilities >;

}; // namespace EspSoftwareSerial

using SoftwareSerial       = EspSoftwareSerial::UART;
using SoftwareSerialParity = EspSoftwareSerial::Parity;
using SoftwareSerialConfig = EspSoftwareSerial::Config;

using EspSoftwareSerial::SWSERIAL_5N1;
using EspSoftwareSerial::SWSERIAL_6N1;
using EspSoftwareSerial::SWSERIAL_7N1;
using EspSoftwareSerial::SWSERIAL_8N1;
using EspSoftwareSerial::SWSERIAL_5E1;
using EspSoftwareSerial::SWSERIAL_6E1;
using EspSoftwareSerial::SWSERIAL_7E1;
using EspSoftwareSerial::SWSERIAL_8E1;
using EspSoftwareSerial::SWSERIAL_5O1;
using EspSoftwareSerial::SWSERIAL_6O1;
using EspSoftwareSerial::SWSERIAL_7O1;
using EspSoftwareSerial::SWSERIAL_8O1;
using EspSoftwareSerial::SWSERIAL_5M1;
using EspSoftwareSerial::SWSERIAL_6M1;
using EspSoftwareSerial::SWSERIAL_7M1;
using EspSoftwareSerial::SWSERIAL_8M1;
using EspSoftwareSerial::SWSERIAL_5S1;
using EspSoftwareSerial::SWSERIAL_6S1;
using EspSoftwareSerial::SWSERIAL_7S1;
using EspSoftwareSerial::SWSERIAL_8S1;
using EspSoftwareSerial::SWSERIAL_5N2;
using EspSoftwareSerial::SWSERIAL_6N2;
using EspSoftwareSerial::SWSERIAL_7N2;
using EspSoftwareSerial::SWSERIAL_8N2;
using EspSoftwareSerial::SWSERIAL_5E2;
using EspSoftwareSerial::SWSERIAL_6E2;
using EspSoftwareSerial::SWSERIAL_7E2;
using EspSoftwareSerial::SWSERIAL_8E2;
using EspSoftwareSerial::SWSERIAL_5O2;
using EspSoftwareSerial::SWSERIAL_6O2;
using EspSoftwareSerial::SWSERIAL_7O2;
using EspSoftwareSerial::SWSERIAL_8O2;
using EspSoftwareSerial::SWSERIAL_5M2;
using EspSoftwareSerial::SWSERIAL_6M2;
using EspSoftwareSerial::SWSERIAL_7M2;
using EspSoftwareSerial::SWSERIAL_8M2;
using EspSoftwareSerial::SWSERIAL_5S2;
using EspSoftwareSerial::SWSERIAL_6S2;
using EspSoftwareSerial::SWSERIAL_7S2;
using EspSoftwareSerial::SWSERIAL_8S2;

#endif // __SoftwareSerial_h