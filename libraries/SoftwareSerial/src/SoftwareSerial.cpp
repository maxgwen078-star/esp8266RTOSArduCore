#include "SoftwareSerial.h"
#include <Arduino.h>

extern "C" {
#include "esp8266/eagle_soc.h"
#include "driver/gpio.h"
}

using namespace EspSoftwareSerial;

ALWAYS_INLINE_ATTR inline void IRAM_ATTR UARTBase::disableInterrupts()
{
    taskENTER_CRITICAL();
}
ALWAYS_INLINE_ATTR inline void IRAM_ATTR UARTBase::restoreInterrupts()
{
    taskEXIT_CRITICAL();
}

constexpr uint8_t BYTE_ALL_BITS_SET = ~static_cast<uint8_t>(0);

UARTBase::UARTBase() {}
UARTBase::UARTBase(int8_t rxPin, int8_t txPin, bool invert)
{
    m_rxPin = rxPin;
    m_txPin = txPin;
    m_invert = invert;
}
UARTBase::~UARTBase() { end(); }

void UARTBase::setRxGPIOPinMode() {
    if (m_rxValid) {
        pinMode(m_rxPin, m_rxGPIOHasPullUp && m_rxGPIOPullUpEnabled ? INPUT_PULLUP : INPUT);
    }
}
void UARTBase::setTxGPIOPinMode() {
    if (m_txValid) {
        pinMode(m_txPin, m_txGPIOOpenDrain ? OUTPUT_OPEN_DRAIN : OUTPUT);
    }
}

void UARTBase::begin(uint32_t baud, Config config,
    int8_t rxPin, int8_t txPin, bool invert) {
    if (-1 != rxPin) m_rxPin = rxPin;
    if (-1 != txPin) m_txPin = txPin;
    m_oneWire = (m_rxPin == m_txPin);
    m_invert = invert;
    m_dataBits = 5 + (config & 07);
    m_parityMode = static_cast<Parity>(config & 070);
    m_stopBits = 1 + ((config & 0300) ? 1 : 0);
    m_pduBits = m_dataBits + static_cast<bool>(m_parityMode) + m_stopBits;

    // ============================================================
    // FIX 1 : m_bitTicks = cycles CPU par bit (au lieu de micros)
    // ============================================================
    m_bitTicks = FSS_CPU_FREQ_HZ / baud;

    m_intTxEnabled = true;
}

void UARTBase::beginRx(bool hasPullUp, int bufCapacity, int isrBufCapacity) {
    m_rxGPIOHasPullUp = hasPullUp;
    m_rxReg = nullptr;
    m_rxBitMask = 1u << m_rxPin;

    m_buffer.reset(new circular_queue<uint8_t>((bufCapacity > 0) ? bufCapacity : 64));
    if (m_parityMode) {
        m_parityBuffer.reset(new circular_queue<uint8_t>((m_buffer->capacity() + 7) / 8));
        m_parityInPos = m_parityOutPos = 1;
    }
    m_isrBuffer.reset(new circular_queue<uint32_t, UARTBase*>((isrBufCapacity > 0) ?
        isrBufCapacity : m_buffer->capacity() * (2 + m_dataBits + static_cast<bool>(m_parityMode))));

    if (m_buffer && (!m_parityMode || m_parityBuffer) && m_isrBuffer) {
        m_rxValid = true;
        setRxGPIOPinMode();
    }
}

void UARTBase::beginTx() {
    m_txBitMask = 1u << m_txPin;
    m_txValid = true;
    if (!m_oneWire) {
        setTxGPIOPinMode();
        digitalWrite(m_txPin, !m_invert);
    }
}

void UARTBase::end() {
    enableRx(false);
    m_txValid = false;
    if (m_buffer) m_buffer.reset();
    m_parityBuffer.reset();
    if (m_isrBuffer) m_isrBuffer.reset();
}

uint32_t UARTBase::baudRate() {
    // ============================================================
    // FIX 2 : baudRate = FREQ / m_bitTicks
    // ============================================================
    return FSS_CPU_FREQ_HZ / m_bitTicks;
}

void UARTBase::setTransmitEnablePin(int8_t txEnablePin) {
    if (-1 != txEnablePin) {
        m_txEnableValid = true;
        m_txEnablePin = txEnablePin;
        pinMode(m_txEnablePin, OUTPUT);
        digitalWrite(m_txEnablePin, LOW);
    } else {
        m_txEnableValid = false;
    }
}
void UARTBase::enableIntTx(bool on) { m_intTxEnabled = on; }
void UARTBase::enableRxGPIOPullUp(bool on) { m_rxGPIOPullUpEnabled = on; setRxGPIOPinMode(); }
void UARTBase::enableTxGPIOOpenDrain(bool on) { m_txGPIOOpenDrain = on; setTxGPIOPinMode(); }

void UARTBase::enableTx(bool on) {
    if (m_txValid && m_oneWire) {
        if (on) {
            enableRx(false);
            setTxGPIOPinMode();
            digitalWrite(m_txPin, !m_invert);
        } else {
            setRxGPIOPinMode();
            enableRx(true);
        }
    }
}

void UARTBase::enableRx(bool on) {
    if (m_rxValid && on != m_rxEnabled) {
        if (on) {
            m_rxLastBit = m_pduBits - 1;
            m_isrLastTick = (ticks() | 1) ^ m_invert;
            // ============================================================
            // FIX 3 : seuil baud = FREQ / 74880
            // ============================================================
            if (m_bitTicks >= FSS_CPU_FREQ_HZ / 74880UL) {
                attachInterruptArg(digitalPinToInterrupt(m_rxPin),
                    reinterpret_cast<void (*)(void*)>(rxBitISR), this, CHANGE);
            } else {
                attachInterruptArg(digitalPinToInterrupt(m_rxPin),
                    reinterpret_cast<void (*)(void*)>(rxBitSyncISR), this,
                    m_invert ? RISING : FALLING);
            }
        } else {
            detachInterrupt(digitalPinToInterrupt(m_rxPin));
        }
        m_rxEnabled = on;
    }
}

int UARTBase::read() {
    if (!m_rxValid) return -1;
    if (!m_buffer->available()) {
        rxBits();
        if (!m_buffer->available()) return -1;
    }
    auto val = m_buffer->pop();
    if (m_parityBuffer) {
        m_lastReadParity = m_parityBuffer->peek() & m_parityOutPos;
        m_parityOutPos <<= 1;
        if (!m_parityOutPos) {
            m_parityOutPos = 1;
            m_parityBuffer->pop();
        }
    }
    return val;
}

int UARTBase::read(uint8_t* buffer, size_t size) {
    if (!m_rxValid) return 0;
    int avail;
    if (0 == (avail = m_buffer->pop_n(buffer, size))) {
        rxBits();
        avail = m_buffer->pop_n(buffer, size);
    }
    if (!avail) return 0;
    if (m_parityBuffer) {
        uint32_t parityBits = avail;
        while (m_parityOutPos >>= 1) ++parityBits;
        m_parityOutPos = (1 << (parityBits % 8));
        m_parityBuffer->pop_n(nullptr, parityBits / 8);
    }
    return avail;
}

size_t UARTBase::readBytes(uint8_t* buffer, size_t size) {
    if (!m_rxValid || !size) return 0;
    size_t count = 0;
    auto start = millis();
    do {
        auto readCnt = read(&buffer[count], size - count);
        count += readCnt;
        if (count >= size) break;
        if (readCnt) {
            start = millis();
        } else {
            // Yield léger au lieu d'optimistic_yield qui pourrait bloquer
            taskYIELD();
        }
    } while (millis() - start < _timeout);
    return count;
}

int UARTBase::available() {
    if (!m_rxValid) return 0;
    rxBits();
    int avail = m_buffer->available();
    if (!avail) {
        // Yield léger
        taskYIELD();
    }
    return avail;
}

int UARTBase::availableForWrite() {
    if (!m_txValid) return 0;
    return 1;
}

void UARTBase::lazyDelay() {
    if (!m_intTxEnabled) restoreInterrupts();
    const auto expired = ticks() - m_periodStart;
    const int32_t remaining = m_periodDuration - expired;
    const uint32_t ms = remaining > 0 ? ticksToMicros(remaining) / 1000UL : 0;
    if (ms > 0) {
        delay(ms);
    } else {
        // Yield léger
        taskYIELD();
    }
    preciseDelay();
    if (!m_intTxEnabled) disableInterrupts();
}

void IRAM_ATTR UARTBase::preciseDelay() {
    uint32_t now;
    do {
        now = ticks();
    } while ((now - m_periodStart) < m_periodDuration);
    m_periodDuration = 0;
    m_periodStart = now;
}

void IRAM_ATTR UARTBase::writePeriod(
    uint32_t dutyCycle, uint32_t offCycle, bool withStopBit) {
    preciseDelay();
    if (dutyCycle) {
        if (16 == m_txPin) {
            WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & ~1) | 1);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, m_txBitMask);
        }
        m_periodDuration += dutyCycle;
        if (offCycle || (withStopBit && !m_invert)) {
            if (!withStopBit || m_invert) preciseDelay();
            else lazyDelay();
        }
    }
    if (offCycle) {
        if (16 == m_txPin) {
            WRITE_PERI_REG(RTC_GPIO_OUT, READ_PERI_REG(RTC_GPIO_OUT) & ~1);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, m_txBitMask);
        }
        m_periodDuration += offCycle;
        if (withStopBit && m_invert) lazyDelay();
    }
}

size_t UARTBase::write(uint8_t byte) { return write(&byte, 1); }
size_t UARTBase::write(uint8_t byte, Parity parity) { return write(&byte, 1, parity); }
size_t UARTBase::write(const uint8_t* buffer, size_t size) { return write(buffer, size, m_parityMode); }

size_t IRAM_ATTR UARTBase::write(const uint8_t* buffer, size_t size, Parity parity) {
    if (m_rxValid) rxBits();
    if (!m_txValid) return -1;

    if (m_txEnableValid) digitalWrite(m_txEnablePin, HIGH);

    bool b = !m_invert;
    uint32_t dutyCycle = 0;
    uint32_t offCycle = 0;
    if (!m_intTxEnabled) disableInterrupts();

    const uint32_t dataMask = ((1UL << m_dataBits) - 1);
    bool withStopBit = true;
    m_periodDuration = 0;
    m_periodStart = ticks();

    for (size_t cnt = 0; cnt < size; ++cnt) {
        uint8_t byte = pgm_read_byte(buffer + cnt) & dataMask;

        uint32_t word = ~0UL;
        if (parity && m_parityMode) {
            uint32_t parityBit;
            switch (parity) {
            case PARITY_EVEN:
                parityBit = byte;
                parityBit ^= parityBit >> 4;
                parityBit &= 0xf;
                parityBit = (0x9669 >> parityBit) & 1;
                break;
            case PARITY_ODD:
                parityBit = byte;
                parityBit ^= parityBit >> 4;
                parityBit &= 0xf;
                parityBit = (0x6996 >> parityBit) & 1;
                break;
            case PARITY_MARK: parityBit = 0; break;
            case PARITY_SPACE:
            default: parityBit = 1; break;
            }
            word ^= parityBit;
        }
        word <<= m_dataBits;
        word |= byte;
        word <<= 1;
        if (m_invert) word = ~word;

        for (int i = 0; i <= m_pduBits; ++i) {
            bool pb = b;
            b = word & (1UL << i);
            if (!pb && b) {
                writePeriod(dutyCycle, offCycle, withStopBit);
                withStopBit = false;
                dutyCycle = offCycle = 0;
            }
            if (b) dutyCycle += m_bitTicks;
            else   offCycle  += m_bitTicks;
        }
        withStopBit = true;
    }

    writePeriod(dutyCycle, offCycle, true);

    if (!m_intTxEnabled) restoreInterrupts();
    if (m_txEnableValid) digitalWrite(m_txEnablePin, LOW);
    return size;
}

void UARTBase::flush() {
    if (!m_rxValid) return;
    m_buffer->flush();
    if (m_parityBuffer) {
        m_parityInPos = m_parityOutPos = 1;
        m_parityBuffer->flush();
    }
}
bool UARTBase::overflow() {
    bool res = m_overflow;
    m_overflow = false;
    return res;
}
int UARTBase::peek() {
    if (!m_rxValid) return -1;
    if (!m_buffer->available()) {
        rxBits();
        if (!m_buffer->available()) return -1;
    }
    auto val = m_buffer->peek();
    if (m_parityBuffer) m_lastReadParity = m_parityBuffer->peek() & m_parityOutPos;
    return val;
}

void UARTBase::rxBits() {
    if (m_isrOverflow) {
        m_isrOverflow = false;
        m_overflow = true;
    }
    m_isrBuffer->for_each(m_isrBufferForEachDel);

    if (m_rxLastBit < m_pduBits - 1) {
        const uint32_t detectionTicks = (m_pduBits - 1 - m_rxLastBit) * m_bitTicks;
        if (!m_isrBuffer->available() && ticks() - m_isrLastTick > detectionTicks) {
            rxBits(((m_isrLastTick + detectionTicks) | 1) ^ m_invert);
        }
    }
}

void UARTBase::rxBits(const uint32_t isrTick) {
    const bool level = (m_isrLastTick & 1) ^ m_invert;
    uint32_t ticksDiff = isrTick - m_isrLastTick;
    m_isrLastTick = isrTick;

    uint32_t bits = ticksDiff / m_bitTicks;
    if (ticksDiff % m_bitTicks > (m_bitTicks >> 1)) ++bits;

    while (bits > 0) {
        if (m_rxLastBit >= (m_pduBits - 1)) {
            if (level) break;
            m_rxLastBit = -1;
            --bits;
            continue;
        }
        if (m_rxLastBit < (m_dataBits - 1)) {
            uint8_t dataBits = min(bits, static_cast<uint32_t>(m_dataBits - 1 - m_rxLastBit));
            m_rxLastBit += dataBits;
            bits -= dataBits;
            m_rxCurByte >>= dataBits;
            if (level) { m_rxCurByte |= (BYTE_ALL_BITS_SET << (8 - dataBits)); }
            continue;
        }
        if (m_parityMode && m_rxLastBit == (m_dataBits - 1)) {
            ++m_rxLastBit;
            --bits;
            m_rxCurParity = level;
            continue;
        }
        if (bits >= static_cast<uint32_t>(m_pduBits - 1 - m_rxLastBit) && level) {
            m_rxCurByte >>= (sizeof(uint8_t) * 8 - m_dataBits);
            if (!m_buffer->push(m_rxCurByte)) {
                m_overflow = true;
            } else if (m_parityBuffer) {
                if (m_rxCurParity) m_parityBuffer->pushpeek() |= m_parityInPos;
                else               m_parityBuffer->pushpeek() &= ~m_parityInPos;
                m_parityInPos <<= 1;
                if (!m_parityInPos) {
                    m_parityBuffer->push();
                    m_parityInPos = 1;
                }
            }
        }
        m_rxLastBit = m_pduBits - 1;
        m_rxCurByte = 0;
        m_rxCurParity = false;
        break;
    }
}

void IRAM_ATTR UARTBase::rxBitISR(UARTBase* self) {
    const bool level = gpio_get_level((gpio_num_t)self->m_rxPin);
    const uint32_t curTick = ticks();
    const bool empty = !self->m_isrBuffer->available();

    if (!self->m_isrBuffer->push((curTick | 1U) ^ !level)) {
        self->m_isrOverflow = true;
    }
    if (empty) self->m_rxHandler();
}

void IRAM_ATTR UARTBase::rxBitSyncISR(UARTBase* self) {
    bool level = self->m_invert;
    const uint32_t start = ticks();
    uint32_t wait = self->m_bitTicks;
    const bool empty = !self->m_isrBuffer->available();

    if (!self->m_isrBuffer->push(((start + wait) | 1U) ^ !level)) {
        self->m_isrOverflow = true;
    }

    for (uint32_t i = 0; i < self->m_pduBits; ++i) {
        const uint32_t timeoutTicks = self->m_bitTicks * 4;
        const uint32_t t0 = ticks();
        while (ticks() - start < wait) {
            if (ticks() - t0 > timeoutTicks) break;
        }
        wait += self->m_bitTicks;

        bool pinLevel = gpio_get_level((gpio_num_t)self->m_rxPin);
        if (pinLevel != level) {
            if (!self->m_isrBuffer->push(((start + wait) | 1U) ^ level)) {
                self->m_isrOverflow = true;
            }
            level = !level;
        }
    }
    if (empty) self->m_rxHandler();
}

void UARTBase::onReceive(const Delegate<void(), void*>& handler) {
    disableInterrupts();
    m_rxHandler = handler;
    restoreInterrupts();
}
void UARTBase::onReceive(Delegate<void(), void*>&& handler) {
    disableInterrupts();
    m_rxHandler = std::move(handler);
    restoreInterrupts();
}