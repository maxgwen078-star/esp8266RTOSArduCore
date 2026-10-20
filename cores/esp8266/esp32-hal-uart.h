#ifndef MAIN_ESP8266_HAL_UART_H_
#define MAIN_ESP8266_HAL_UART_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// ⚠️ NE PAS inclure "driver/uart.h" ici : ce header dépend de FreeRTOS
//    et doit être inclus APRÈS esp32-hal.h dans les .c/.cpp.
//    On se contente de forward-déclarer uart_t.

#define UART0    0
#define UART1    1
#define UART_NO -1

// --- Data bits (bits 2-3 de `config`) ---
#define UART_NB_BIT_MASK      0x0C
#define UART_NB_BIT_5         0x00
#define UART_NB_BIT_6         0x04
#define UART_NB_BIT_7         0x08
#define UART_NB_BIT_8         0x0C

// --- Parité (bits 0-1) ---
#define UART_PARITY_MASK      0x03
#define UART_PARITY_NONE_CFG  0x00
#define UART_PARITY_EVEN_CFG  0x02
#define UART_PARITY_ODD_CFG   0x03

// --- Stop bits (bits 4-5) ---
#define UART_NB_STOP_BIT_MASK 0x30
#define UART_NB_STOP_BIT_0    0x00
#define UART_NB_STOP_BIT_1    0x10
#define UART_NB_STOP_BIT_15   0x20
#define UART_NB_STOP_BIT_2    0x30

#define UART_5N1 ( UART_NB_BIT_5 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_1 )
#define UART_6N1 ( UART_NB_BIT_6 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_1 )
#define UART_7N1 ( UART_NB_BIT_7 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_1 )
#define UART_8N1 ( UART_NB_BIT_8 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_1 )
#define UART_5N2 ( UART_NB_BIT_5 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_2 )
#define UART_6N2 ( UART_NB_BIT_6 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_2 )
#define UART_7N2 ( UART_NB_BIT_7 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_2 )
#define UART_8N2 ( UART_NB_BIT_8 | UART_PARITY_NONE_CFG | UART_NB_STOP_BIT_2 )
#define UART_5E1 ( UART_NB_BIT_5 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_1 )
#define UART_6E1 ( UART_NB_BIT_6 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_1 )
#define UART_7E1 ( UART_NB_BIT_7 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_1 )
#define UART_8E1 ( UART_NB_BIT_8 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_1 )
#define UART_5E2 ( UART_NB_BIT_5 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_2 )
#define UART_6E2 ( UART_NB_BIT_6 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_2 )
#define UART_7E2 ( UART_NB_BIT_7 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_2 )
#define UART_8E2 ( UART_NB_BIT_8 | UART_PARITY_EVEN_CFG | UART_NB_STOP_BIT_2 )
#define UART_5O1 ( UART_NB_BIT_5 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_1 )
#define UART_6O1 ( UART_NB_BIT_6 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_1 )
#define UART_7O1 ( UART_NB_BIT_7 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_1 )
#define UART_8O1 ( UART_NB_BIT_8 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_1 )
#define UART_5O2 ( UART_NB_BIT_5 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_2 )
#define UART_6O2 ( UART_NB_BIT_6 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_2 )
#define UART_7O2 ( UART_NB_BIT_7 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_2 )
#define UART_8O2 ( UART_NB_BIT_8 | UART_PARITY_ODD_CFG  | UART_NB_STOP_BIT_2 )

#define UART_FULL     0
#define UART_RX_ONLY  1
#define UART_TX_ONLY  2

#define UART_TX_FIFO_SIZE 0x80

struct uart_struct_t;
typedef struct uart_struct_t uart_t;

uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config,
                  int8_t rxPin, int8_t txPin, uint16_t queueLen, bool inverted);
void uartEnd(uart_t* uart, uint8_t rxPin, uint8_t txPin);

uint32_t uartAvailable(uart_t* uart);
uint32_t uartAvailableForWrite(uart_t* uart);
uint8_t uartRead(uart_t* uart);
uint8_t uartPeek(uart_t* uart);

void uartWrite(uart_t* uart, uint8_t c);
void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len);

void uartFlush(uart_t* uart);
void uartFlushTxOnly(uart_t* uart, bool txOnly);

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate);
uint32_t uartGetBaudRate(uart_t* uart);

size_t uartResizeRxBuffer(uart_t* uart, size_t new_size);

void uartSetRxInvert(uart_t* uart, bool invert);

void uartSetDebug(uart_t* uart);
int uartGetDebug();

void uartStartDetectBaudrate(uart_t *uart);
unsigned long uartDetectBaudrate(uart_t *uart);

// Swap UART0 : GPIO1/3 <-> GPIO15/13 (ROM/driver)
void uartSwapPins(uart_t* uart, bool swap);
bool uartIsSwapped(uart_t* uart);

#ifdef __cplusplus
}
#endif

#endif