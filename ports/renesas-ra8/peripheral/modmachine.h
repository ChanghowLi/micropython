#ifndef MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H
#define MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _machine_reset_flags_t {
    uint8_t rstsr0;
    uint32_t rstsr1;
    uint8_t rstsr2;
    uint8_t rstsr3;
} machine_reset_flags_t;

extern machine_reset_flags_t machine_reset_flags;

void machine_init(void);
void machine_deinit(void);
void machine_camera_deinit_all(void);
uint32_t machine_camera_stop_all(void);
void machine_i2c_deinit_all(void);
/* Whether Python machine.LCD owns a display instance. */
bool machine_lcd_is_initialized(void);
void machine_uart_deinit_all(void);

#endif // MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H
