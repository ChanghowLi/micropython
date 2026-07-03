// MicroPython HAL port declarations for RA8P1
#ifndef MICROPY_INCLUDED_RA8P1_MPHALPORT_H
#define MICROPY_INCLUDED_RA8P1_MPHALPORT_H

// These functions are implemented in mphalport.c
int mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);
mp_uint_t mp_hal_ticks_ms(void);
void mp_hal_set_interrupt_char(char c);
uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags);

#endif // MICROPY_INCLUDED_RA8P1_MPHALPORT_H
