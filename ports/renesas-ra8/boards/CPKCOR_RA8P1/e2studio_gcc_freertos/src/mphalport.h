#ifndef MICROPY_INCLUDED_RA8P1_MPHALPORT_H
#define MICROPY_INCLUDED_RA8P1_MPHALPORT_H

#include "hal_data.h"
#include "irq.h"

#define MICROPY_BEGIN_ATOMIC_SECTION()     disable_irq()
#define MICROPY_END_ATOMIC_SECTION(state)  enable_irq(state)

#ifdef __cplusplus
extern "C" {
#endif

extern int g_mp_interrupt_char;

void mp_hal_get_random(size_t n, uint8_t *buf);
void mp_hal_set_interrupt_char(char c);

#ifdef __cplusplus
}
#endif

#endif
