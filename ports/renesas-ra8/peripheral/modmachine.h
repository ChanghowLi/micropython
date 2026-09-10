#ifndef MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H
#define MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H

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
void machine_i2c_deinit_all(void);

#endif // MICROPY_INCLUDED_RENESAS_RA8_MODMACHINE_H
