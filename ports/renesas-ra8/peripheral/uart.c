#include "uart.h"
#include <string.h>
#include "bsp_api.h"

static fsp_err_t uart_validate(uart_t *uart)
{
    if (uart == NULL || uart->instance == NULL || uart->instance->p_api == NULL || uart->instance->p_ctrl == NULL || uart->instance->p_cfg == NULL) {
        return FSP_ERR_ASSERTION;
    }

    return FSP_SUCCESS;
}

static fsp_err_t uart_validate_opened(uart_t *uart)
{
    fsp_err_t err = uart_validate(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (uart->opened) {
        return FSP_SUCCESS;
    }
    return FSP_ERR_NOT_OPEN;
}

#if BSP_CFG_RTOS == 2
static TickType_t uart_timeout_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return 0U;
    }

    TickType_t timeout_ticks = (TickType_t)(((uint64_t)timeout_ms * configTICK_RATE_HZ + 999ULL) / 1000ULL);
    if (timeout_ticks == 0U) {
        return 1U;
    }
    return timeout_ticks;
}
#endif

static uint32_t uart_rx_available(const uart_t *uart)
{
    uint16_t head = uart->rx_head;
    uint16_t tail = uart->rx_tail;

    if (head >= tail) {
        return (uint32_t)(head - tail);
    }

    return UART_RX_BUFFER_SIZE - (uint32_t)tail + (uint32_t)head;
}

static void uart_rx_push(uart_t *uart, uint8_t data)
{
    uint16_t head = uart->rx_head;
    uint16_t next = (uint16_t)(head + 1U);
    if (next >= UART_RX_BUFFER_SIZE) {
        next = 0U;
    }

    if (next == uart->rx_tail) {
        uart->rx_error = UART_EVENT_ERR_OVERFLOW;
        return;
    }

    uart->rx_buffer[head] = data;
    uart->rx_head = next;

#if BSP_CFG_RTOS == 2
    if (uart->rx_ready != NULL) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xSemaphoreGiveFromISR(uart->rx_ready, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
#endif
}

static void uart_rx_pop(uart_t *uart, uint8_t *data, uint32_t length)
{
    uint16_t tail = uart->rx_tail;

    for (uint32_t index = 0U; index < length; ++index) {
        data[index] = uart->rx_buffer[tail];
        ++tail;
        if (tail >= UART_RX_BUFFER_SIZE) {
            tail = 0U;
        }
    }

    uart->rx_tail = tail;
}

void uart_callback(uart_callback_args_t *p_args)
{
    if (p_args == NULL || p_args->p_context == NULL) {
        return;
    }

    uart_t *uart = (uart_t *)p_args->p_context;
    if (p_args->event == UART_EVENT_RX_CHAR) {
        uart_rx_push(uart, (uint8_t)p_args->data);
        return;
    }

    if (p_args->event == UART_EVENT_TX_COMPLETE) {
        uart->tx_busy = false;
        return;
    }

    if (p_args->event == UART_EVENT_ERR_PARITY ||
        p_args->event == UART_EVENT_ERR_FRAMING ||
        p_args->event == UART_EVENT_ERR_OVERFLOW ||
        p_args->event == UART_EVENT_BREAK_DETECT) {
        uart->rx_error = p_args->event;

#if BSP_CFG_RTOS == 2
        if (uart->rx_ready != NULL) {
            BaseType_t higher_priority_task_woken = pdFALSE;
            xSemaphoreGiveFromISR(uart->rx_ready, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
#endif
    }
}

uint32_t UART_Init(uart_t *uart)
{
    fsp_err_t err = uart_validate(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (uart->opened) {
        return FSP_ERR_ALREADY_OPEN;
    }

#if BSP_CFG_RTOS == 2
    if (uart->rx_ready == NULL) {
        uart->rx_ready = xSemaphoreCreateBinaryStatic(&uart->rx_ready_storage);
        if (uart->rx_ready == NULL) {
            return FSP_ERR_OUT_OF_MEMORY;
        }
    }
#endif

    uart->cfg = *uart->instance->p_cfg;
    uart->cfg.p_callback = uart_callback;
    uart->cfg.p_context = uart;
    uart->rx_head = 0U;
    uart->rx_tail = 0U;
    uart->rx_error = (uart_event_t)0;
    uart->tx_busy = false;
    err = uart->instance->p_api->open(uart->instance->p_ctrl, &uart->cfg);
    if (err != FSP_SUCCESS) {
        return err;
    }

    uart->opened = true;

    return FSP_SUCCESS;
}

uint32_t UART_DeInit(uart_t *uart)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    err = uart->instance->p_api->close(uart->instance->p_ctrl);
    if (err != FSP_SUCCESS) {
        return err;
    }

    uart->rx_head = 0U;
    uart->rx_tail = 0U;
    uart->rx_error = (uart_event_t)0;
    uart->tx_busy = false;
    uart->opened = false;
    return FSP_SUCCESS;
}

uint32_t UART_Flush(uart_t *uart)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

#if BSP_CFG_RTOS == 2
    TickType_t timeout_ticks = uart_timeout_ticks(UART_DEFAULT_TIMEOUT_MS);
    TickType_t start_ticks = xTaskGetTickCount();

    while (uart->tx_busy) {
        if ((xTaskGetTickCount() - start_ticks) >= timeout_ticks) {
            return FSP_ERR_TIMEOUT;
        }
        vTaskDelay(1U);
    }
#else
    uint32_t waited_ms = 0U;
    while (uart->tx_busy) {
        if (waited_ms >= UART_DEFAULT_TIMEOUT_MS) {
            return FSP_ERR_TIMEOUT;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        ++waited_ms;
    }
#endif

    return FSP_SUCCESS;
}

uint32_t UART_SetBaudrate(uart_t *uart, uint32_t baudrate)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (baudrate == 0U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    sci_b_baud_setting_t baud_setting;
    err = R_SCI_B_UART_BaudCalculate(baudrate, false, UART_MAX_BAUD_ERROR_X_1000, &baud_setting);
    if (err != FSP_SUCCESS) {
        return err;
    }

    err = uart->instance->p_api->baudSet(uart->instance->p_ctrl, &baud_setting);
    if (err != FSP_SUCCESS) {
        return err;
    }

    return FSP_SUCCESS;
}

uint32_t UART_Any(uart_t *uart, uint32_t *available)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (available == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *available = uart_rx_available(uart);
    return FSP_SUCCESS;
}

uint32_t UART_TxDone(uart_t *uart, bool *done)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (done == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *done = !uart->tx_busy;
    return FSP_SUCCESS;
}

// 等待接收缓冲区中至少有一个字节；接收错误和超时通过返回值报告。
static fsp_err_t uart_rx_wait(uart_t *uart, uint32_t timeout_ms)
{
#if BSP_CFG_RTOS == 2
    TickType_t timeout_ticks = uart_timeout_ticks(timeout_ms);
    TickType_t start_ticks = xTaskGetTickCount();
#else
    uint32_t waited_ms = 0U;
#endif

    for (;;) {
        if (uart->rx_error != (uart_event_t)0) {
            uart->rx_error = (uart_event_t)0;
            return FSP_ERR_ABORTED;
        }

        if (uart_rx_available(uart) > 0U) {
            return FSP_SUCCESS;
        }

#if BSP_CFG_RTOS == 2
        TickType_t elapsed_ticks = xTaskGetTickCount() - start_ticks;
        if (elapsed_ticks >= timeout_ticks) {
            return FSP_ERR_TIMEOUT;
        }
        (void)xSemaphoreTake(uart->rx_ready, timeout_ticks - elapsed_ticks);
#else
        if (waited_ms >= timeout_ms) {
            return FSP_ERR_TIMEOUT;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        ++waited_ms;
#endif
    }
}

uint32_t UART_Read(uart_t *uart, uint8_t *data, uint32_t length, uint32_t *read_length, uint32_t timeout_ms, uint32_t timeout_char_ms)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (data == NULL || length == 0U || read_length == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *read_length = 0U;
    uint32_t wait_ms = timeout_ms;

    while (*read_length < length) {
        // 首次等待用 timeout_ms，读到数据后用 timeout_char_ms。
        err = uart_rx_wait(uart, wait_ms);
        if (err != FSP_SUCCESS) {
            // 已经读到部分数据时，优先把已有数据返回给上层。
            if ((err == FSP_ERR_TIMEOUT || err == FSP_ERR_ABORTED) && *read_length > 0U) {
                return FSP_SUCCESS;
            }
            return err;
        }

        // 有多少取多少，但不超过还需要的数量。
        uint32_t available = uart_rx_available(uart);
        uint32_t remaining = length - *read_length;
        if (available > remaining) {
            available = remaining;
        }

        // 将数据接在输出数组已有的数据后面。
        uint32_t bytes_read = *read_length;
        uint8_t *destination = &data[bytes_read];
        uart_rx_pop(uart, destination, available);
        *read_length = bytes_read + available;

        wait_ms = timeout_char_ms;
    }

    return FSP_SUCCESS;
}

uint32_t UART_Write(uart_t *uart, const uint8_t *data, uint32_t length)
{
    fsp_err_t err = uart_validate_opened(uart);
    if (err != FSP_SUCCESS) {
        return err;
    }

    if (data == NULL || length == 0U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (uart->tx_busy) {
        return FSP_ERR_IN_USE;
    }

    if (length > UART_TX_BUFFER_SIZE) {
        return FSP_ERR_INVALID_SIZE;
    }

    memcpy(uart->tx_buffer, data, length);  // 复制待发送数据
    uart->tx_busy = true;                   // 标记正在发送
    err = uart->instance->p_api->write(uart->instance->p_ctrl, uart->tx_buffer, length);
    if (err != FSP_SUCCESS) {
        uart->tx_busy = false;
        return err;
    }

    return FSP_SUCCESS;
}
