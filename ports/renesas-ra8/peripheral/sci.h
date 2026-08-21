#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_SCI_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_SCI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MACHINE_SCI_OWNER_NONE,
    MACHINE_SCI_OWNER_I2C,
    MACHINE_SCI_OWNER_SPI,
    MACHINE_SCI_OWNER_UART,
} machine_sci_owner_t;

bool machine_sci_take(uint8_t channel, machine_sci_owner_t owner);
void machine_sci_give(uint8_t channel, machine_sci_owner_t owner);

#endif