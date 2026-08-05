#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_IRQ_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_IRQ_H

#include "py/obj.h"

enum
{
    MACHINE_PIN_IRQ_FALLING = 1,
    MACHINE_PIN_IRQ_RISING = 2,
    MACHINE_PIN_IRQ_LOW_LEVEL = 4,
    MACHINE_PIN_IRQ_HIGH_LEVEL = 8,
};

extern const mp_obj_fun_builtin_var_t machine_pin_irq_obj;

void machine_pin_irq_deinit(void);

#endif
