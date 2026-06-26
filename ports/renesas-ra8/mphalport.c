// MicroPython HAL port implementation for RA8P1 (FreeRTOS + FSP)
//
// Uses:
//   - console.c's UART ring buffer for RX (interrupt-driven)
//   - Direct SCI UART register write for TX (polling)
//   - FreeRTOS xTaskGetTickCount() for mp_hal_ticks_ms()

#include "py/mpconfig.h"
#include "py/mphal.h"

// FSP / FreeRTOS includes
#include "bsp_api.h"
#include "hal_data.h"
#include "console.h"
#include "FreeRTOS.h"
#include "task.h"

// --- UART RX: read one character from the console ring buffer ---

int mp_hal_stdin_rx_chr(void) {
    unsigned char c;
    // Wait for data in the RX ring buffer (filled by UART RX interrupt)
    while (CONSOLE_HasData() == 0) {
        // Yield to other tasks while waiting; the UART interrupt will fill
        // the buffer regardless
        taskYIELD();
    }
    CONSOLE_Read(&c, 1);
    return c;
}

// --- UART TX: send string via direct register write ---

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    // Direct UART TX register access — same approach as console.c's fputc
    sci_b_uart_instance_ctrl_t *ctrl =
        (sci_b_uart_instance_ctrl_t *)CONSOLE_CFG_UART_INSTANCE.p_ctrl;
    for (mp_uint_t i = 0; i < len; i++) {
        ctrl->p_reg->TDR_BY = (uint8_t)str[i];
        while ((ctrl->p_reg->CSR & R_SCI_B0_CSR_TDRE_Msk) == 0) {}
    }
    return len;
}

// --- Millisecond counter from FreeRTOS tick ---

mp_uint_t mp_hal_ticks_ms(void) {
    // configTICK_RATE_HZ = 1000, so each tick is 1ms
    return (mp_uint_t)xTaskGetTickCount();
}

// --- Interrupt character (Ctrl-C handling) ---
//
// For basic REPL, Ctrl-C (0x03) is handled by the readline code checking
// the received character. The event-driven REPL (MICROPY_REPL_EVENT_DRIVEN)
// uses the interrupt_char mechanism for async KeyboardInterrupt.
// For now, we rely on the polling-based REPL where Ctrl-C is handled inline.

static int mp_interrupt_char = -1;

void mp_hal_set_interrupt_char(char c) {
    mp_interrupt_char = c;
}

// Check if the interrupt character has been received.
// Called by MicroPython VM periodically (e.g., in mp_handle_pending).
// We peek into the UART RX buffer to see if Ctrl-C was pressed.
int mp_hal_is_interrupt_char_received(void) {
    if (mp_interrupt_char < 0) {
        return 0;
    }
    // We can't easily peek the ring buffer without consuming the char,
    // so for now we rely on the polling readline to detect Ctrl-C.
    // For full event-driven REPL support, the UART RX ISR callback
    // (UART9_Callback in console.c) should be extended to set a flag
    // when the interrupt char is received.
    return 0;
}
