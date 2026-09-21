#ifndef RENESAS_RA8_UART_H
#define RENESAS_RA8_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "r_sci_b_uart.h"
#include "r_uart_api.h"

#define UART_DEFAULT_BAUDRATE_HZ    (115200U)
#define UART_DEFAULT_TIMEOUT_CHAR_MS (0U)
#define UART_DEFAULT_TIMEOUT_MS     (1000U)
#define UART_MAX_BAUD_ERROR_X_1000  (5000U)
#define UART_RX_BUFFER_SIZE          (512U)
#define UART_TX_BUFFER_SIZE          (512U)

#if BSP_CFG_RTOS == 2
#include "FreeRTOS.h"
#include "semphr.h"
#endif

typedef struct {
    const uart_instance_t *instance;
    uart_cfg_t cfg;
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;
    volatile uart_event_t rx_error;
    volatile bool tx_busy;
    bool opened;
#if BSP_CFG_RTOS == 2
    SemaphoreHandle_t rx_ready;
    StaticSemaphore_t rx_ready_storage;
#endif
} uart_t;

uint32_t UART_DeInit(uart_t *uart);
uint32_t UART_Flush(uart_t *uart);
uint32_t UART_Init(uart_t *uart);
uint32_t UART_Any(uart_t *uart, uint32_t *available);
uint32_t UART_Read(
    uart_t *uart,
    uint8_t *data,
    uint32_t length,
    uint32_t *read_length,
    uint32_t timeout_ms,
    uint32_t timeout_char_ms);
uint32_t UART_SetBaudrate(uart_t *uart, uint32_t baudrate);
uint32_t UART_TxDone(uart_t *uart, bool *done);
uint32_t UART_Write(uart_t *uart, const uint8_t *data, uint32_t length);

#endif
