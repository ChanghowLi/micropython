#ifndef MICROPY_INCLUDED_RA8P1_MPHALPORT_H
#define MICROPY_INCLUDED_RA8P1_MPHALPORT_H

#include "hal_data.h"
#include "irq.h"
#include "pin.h"

#define MICROPY_BEGIN_ATOMIC_SECTION()     disable_irq()
#define MICROPY_END_ATOMIC_SECTION(state)  enable_irq(state)

#define MP_HAL_PIN_FMT                     "%q"
#define mp_hal_delay_us_fast(us)           mp_hal_delay_us(us)
#define mp_hal_get_pin_obj(o)              machine_pin_find(o)
#define mp_hal_pin_input(p)                machine_pin_configure_input(p)
#define mp_hal_pin_name(p)                 ((p)->name)
#define mp_hal_pin_obj_t                   const machine_pin_obj_t *
#define mp_hal_pin_output(p)               machine_pin_configure_output((p), false)
#define mp_hal_pin_read(p)                 machine_pin_read(p)
#define mp_hal_pin_write(p, v)             machine_pin_write((p), (v))

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
