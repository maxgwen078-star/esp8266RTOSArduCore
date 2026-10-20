#include "esp32-hal.h"          // ⚠️ EN PREMIER : charge FreeRTOS + types de base
#include "esp32-hal-uart.h"     // notre header (sans driver/uart.h)
#include "driver/uart.h"        // le driver (dépend de FreeRTOS, donc après)

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "rom/uart.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp8266/uart_struct.h"
#include "esp8266/uart_register.h"
#include "esp8266/pin_mux_register.h"
#include "esp8266/eagle_soc.h"
#include "esp8266/rom_functions.h"

static int s_uart_debug_nr = 0;

struct uart_struct_t {
    uart_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
    int16_t peek_byte;         // -1 = vide
    bool    swapped;           // UART0 sur GPIO15/GPIO13 ?
    size_t  rx_buffer_size;    // pour resize / réinstall
};

#if CONFIG_DISABLE_HAL_LOCKS
#define UART_MUTEX_LOCK()
#define UART_MUTEX_UNLOCK()
#else
#define UART_MUTEX_LOCK()    do {} while (xSemaphoreTake(uart->lock, portMAX_DELAY) != pdPASS)
#define UART_MUTEX_UNLOCK()  xSemaphoreGive(uart->lock)
#endif

static uart_t _uart_bus_array[2] = {
    {(volatile uart_dev_t *)0x60000000, NULL, 0, -1, false, 2048},
    {(volatile uart_dev_t *)0x60000F00, NULL, 1, -1, false, 2048},
};

// ============================================================
// Swap UART0 (ROM : MTCK=GPIO13 RX, MTDO=GPIO15 TX)
// ============================================================
void uartSwapPins(uart_t* uart, bool swap)
{
    if (uart == NULL || uart->num != 0) return;

    if (swap) {
        uart_enable_swap();
        uart->swapped = true;
    } else {
        uart_disable_swap();
        uart->swapped = false;
    }
}

bool uartIsSwapped(uart_t* uart)
{
    if (uart == NULL || uart->num != 0) return false;
    return uart->swapped;
}

// ============================================================
// uartBegin
//   Ordre : param_config -> driver_install -> enable_swap
// ============================================================
uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config,
                  int8_t rxPin, int8_t txPin, uint16_t queueLen, bool inverted)
{
    (void)queueLen;

    if (uart_nr >= UART_NUM_MAX) return NULL;
    if (rxPin == -1 && txPin == -1) return NULL;
    if (config == 0) config = UART_8N1;

    uart_t* uart = &_uart_bus_array[uart_nr];

#if !CONFIG_DISABLE_HAL_LOCKS
    if (uart->lock == NULL) {
        uart->lock = xSemaphoreCreateMutex();
        if (uart->lock == NULL) return NULL;
    }
#endif

    // ─── 1) Décodage config → enums driver ───
    uint8_t data_bits_cfg = (config >> 2) & 0x03;
    uint8_t parity_cfg    =  config       & 0x03;
    uint8_t stop_bits_cfg = (config >> 4) & 0x03;

    uart_word_length_t data_bits;
    switch (data_bits_cfg) {
        case 0x0: data_bits = UART_DATA_5_BITS; break;
        case 0x1: data_bits = UART_DATA_6_BITS; break;
        case 0x2: data_bits = UART_DATA_7_BITS; break;
        default:  data_bits = UART_DATA_8_BITS; break;
    }

    uart_parity_t parity;
    switch (parity_cfg) {
        case 0x2: parity = UART_PARITY_EVEN; break;
        case 0x3: parity = UART_PARITY_ODD;  break;
        default:  parity = UART_PARITY_DISABLE; break;
    }

    uart_stop_bits_t stop_bits;
    switch (stop_bits_cfg) {
        case 0x2: stop_bits = UART_STOP_BITS_1_5; break;
        case 0x3: stop_bits = UART_STOP_BITS_2;   break;
        default:  stop_bits = UART_STOP_BITS_1;   break;
    }

    // ─── 2) Désinstalle l'ancien driver ───
    if (uart_is_driver_installed(uart_nr)) {
        uart_driver_delete(uart_nr);
    }

    // ─── 3) Config hardware (pin mux + baud + format) ───
    uart_config_t uart_config = {
        .baud_rate           = (int)baudrate,
        .data_bits           = data_bits,
        .parity              = parity,
        .stop_bits           = stop_bits,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
    };
    uart_param_config(uart_nr, &uart_config);

    // ─── 4) Install driver (ring buffer + ISR) ───
    size_t rx_buf = uart->rx_buffer_size;
    if (rx_buf < UART_FIFO_LEN) rx_buf = UART_FIFO_LEN;

    esp_err_t err = uart_driver_install(uart_nr, rx_buf, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        log_e("uart_driver_install(%d) failed: %d", uart_nr, err);
        return NULL;
    }

    // ─── 5) Swap UART0 si rxPin=13 (APRÈS param_config !) ───
    if (uart_nr == 0) {
        if (rxPin == 13 || txPin == 15) {
            uartSwapPins(uart, true);
        } else if (uart->swapped) {
            uartSwapPins(uart, false);
        }
    }

    // ─── 6) Inversion RX ───
    uart->dev->conf0.rxd_inv = inverted ? 1 : 0;

    // ─── 7) Reset état ───
    uart->peek_byte = -1;
    uart_flush_input(uart_nr);

    return uart;
}

// ============================================================
// uartEnd
// ============================================================
void uartEnd(uart_t* uart, uint8_t rxPin, uint8_t txPin)
{
    (void)rxPin;
    (void)txPin;
    if (uart == NULL) return;

    if (uart->num == 0 && uart->swapped) {
        uartSwapPins(uart, false);
    }

    if (uart_is_driver_installed(uart->num)) {
        uart_driver_delete(uart->num);
    }

    uart->peek_byte = -1;
}

// ============================================================
// Resize RX buffer
// ============================================================
size_t uartResizeRxBuffer(uart_t* uart, size_t new_size)
{
    if (uart == NULL) return 0;
    if (new_size < UART_FIFO_LEN) new_size = UART_FIFO_LEN;

    if (!uart_is_driver_installed(uart->num)) {
        uart->rx_buffer_size = new_size;
        return new_size;
    }

    if (new_size <= uart->rx_buffer_size) {
        return uart->rx_buffer_size;
    }

    uart_driver_delete(uart->num);
    esp_err_t err = uart_driver_install(uart->num, new_size, 0, 0, NULL, 0);
    if (err != ESP_OK) return 0;
    uart->rx_buffer_size = new_size;
    return new_size;
}

// ============================================================
// Inversion RX
// ============================================================
void uartSetRxInvert(uart_t* uart, bool invert) {
    if (uart == NULL) return;
    uart->dev->conf0.rxd_inv = invert ? 1 : 0;
}

// ============================================================
// Disponibilité
// ============================================================
uint32_t uartAvailable(uart_t* uart) {
    if (uart == NULL) return 0;
    size_t n = 0;
    uart_get_buffered_data_len(uart->num, &n);
    return (uint32_t)n + (uart->peek_byte >= 0 ? 1u : 0u);
}

uint32_t uartAvailableForWrite(uart_t* uart) {
    if (uart == NULL) return 0;
    return 0x7F - uart->dev->status.txfifo_cnt;
}

// ============================================================
// Lecture
// ============================================================
uint8_t uartRead(uart_t* uart) {
    if (uart == NULL) return 0;
    if (uart->peek_byte >= 0) {
        uint8_t c = (uint8_t)uart->peek_byte;
        uart->peek_byte = -1;
        return c;
    }
    uint8_t c;
    if (uart_read_bytes(uart->num, &c, 1, 0) == 1) return c;
    return 0;
}

uint8_t uartPeek(uart_t* uart) {
    if (uart == NULL) return 0;
    if (uart->peek_byte >= 0) return (uint8_t)uart->peek_byte;
    uint8_t c;
    if (uart_read_bytes(uart->num, &c, 1, 0) == 1) {
        uart->peek_byte = c;
        return c;
    }
    return 0;
}

// ============================================================
// Écriture
// ============================================================
void uartWrite(uart_t* uart, uint8_t c) {
    if (uart == NULL) return;
    uart_write_bytes(uart->num, (const char*)&c, 1);
}

void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len) {
    if (uart == NULL || data == NULL) return;
    uart_write_bytes(uart->num, (const char*)data, len);
}

// ============================================================
// Flush
// ============================================================
void uartFlush(uart_t* uart) { uartFlushTxOnly(uart, true); }

void uartFlushTxOnly(uart_t* uart, bool txOnly) {
    if (uart == NULL) return;
    uart_wait_tx_done(uart->num, 1000);
    if (!txOnly) {
        uart_flush_input(uart->num);
        uart->peek_byte = -1;
    }
}

// ============================================================
// Baudrate
// ============================================================
void uartSetBaudRate(uart_t* uart, uint32_t baud_rate) {
    if (uart == NULL) return;
    uart_set_baudrate(uart->num, baud_rate);
}

uint32_t uartGetBaudRate(uart_t* uart) {
    if (uart == NULL) return 0;
    uint32_t baud = 0;
    uart_get_baudrate(uart->num, &baud);
    return baud;
}

// ============================================================
// Debug / putc
// ============================================================
static void IRAM_ATTR uart0_write_char(char c) {
    char buffer[1];
    buffer[0] = c;
    uart_write_bytes(UART_NUM_0, buffer, 1);
}

static void uart_install_putc(void) {
    os_install_putc1((void (*)(char)) &uart0_write_char);
}

void uartSetDebug(uart_t* uart) {
    if (uart == NULL || uart->num >= UART_NUM_MAX) {
        s_uart_debug_nr = -1;
    } else {
        s_uart_debug_nr = uart->num;
    }
    uart_install_putc();
}

int uartGetDebug(void) { return s_uart_debug_nr; }

int log_printf(const char *format, ...) {
    if (s_uart_debug_nr < 0 || s_uart_debug_nr >= UART_NUM_MAX) return 0;

    static char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg, copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, arg);
    va_end(copy);
    if (len >= (int)sizeof(loc_buf)) {
        temp = (char*)malloc(len + 1);
        if (temp == NULL) {
            va_end(arg);
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);

    ets_printf("%s", temp);

    if (len >= (int)sizeof(loc_buf)) free(temp);
    return len;
}

// ============================================================
// Détection de baudrate
// ============================================================
unsigned long uartBaudrateDetect(uart_t *uart, bool flg) {
    while (uart->dev->rxd_cnt.edge_cnt < 30) {
        if (flg) return 0;
        ets_delay_us(1000);
    }
    unsigned long ret = ((uart->dev->lowpulse.min_cnt +
                          uart->dev->highpulse.min_cnt) >> 1) + 12;
    return ret;
}

void uartStartDetectBaudrate(uart_t *uart) {
    if (!uart) return;
    uart->dev->auto_baud.glitch_filt = 0x08;
    uart->dev->auto_baud.en = 0;
    uart->dev->auto_baud.en = 1;
}

unsigned long uartDetectBaudrate(uart_t *uart) {
    static bool uartStateDetectingBaudrate = false;

    if (!uartStateDetectingBaudrate) {
        uart->dev->auto_baud.glitch_filt = 0x08;
        uart->dev->auto_baud.en = 0;
        uart->dev->auto_baud.en = 1;
        uartStateDetectingBaudrate = true;
    }

    unsigned long divisor = uartBaudrateDetect(uart, true);
    if (!divisor) return 0;

    uart->dev->auto_baud.en = 0;
    uartStateDetectingBaudrate = false;

    unsigned long baudrate = getApbFrequency() / divisor;

    static const unsigned long default_rates[] = {
        300, 600, 1200, 2400, 4800, 9600, 19200, 38400,
        57600, 74880, 115200, 230400, 256000, 460800, 921600, 1843200, 3686400
    };

    size_t i;
    for (i = 1; i < sizeof(default_rates) / sizeof(default_rates[0]) - 1; i++) {
        if (baudrate <= default_rates[i]) {
            if (baudrate - default_rates[i - 1] < default_rates[i] - baudrate) i--;
            break;
        }
    }
    return default_rates[i];
}