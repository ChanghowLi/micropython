#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_SCI_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_SCI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MACHINE_SCI_OWNER_NONE = 0,
    MACHINE_SCI_OWNER_I2C,
    MACHINE_SCI_OWNER_SPI,
} machine_sci_owner_t;

machine_sci_owner_t machine_sci_get_owner(uint8_t channel);
void machine_sci_give(uint8_t channel, machine_sci_owner_t owner);
bool machine_sci_take(uint8_t channel, machine_sci_owner_t owner);

#endif
