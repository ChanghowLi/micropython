#ifndef MICROPY_INCLUDED_RENESAS_RA8_IRQ_H
#define MICROPY_INCLUDED_RENESAS_RA8_IRQ_H

#include <stdint.h>

uint32_t query_irq(void);

uint32_t disable_irq(void);

void enable_irq(uint32_t state);

#endif
