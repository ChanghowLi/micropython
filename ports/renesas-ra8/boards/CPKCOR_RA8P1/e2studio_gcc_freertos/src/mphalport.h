#ifndef MICROPY_INCLUDED_RA8P1_MPHALPORT_H
#define MICROPY_INCLUDED_RA8P1_MPHALPORT_H

#ifdef __cplusplus
extern "C" {
#endif

extern int g_mp_interrupt_char;

void mp_hal_set_interrupt_char(char c);

#ifdef __cplusplus
}
#endif

#endif
