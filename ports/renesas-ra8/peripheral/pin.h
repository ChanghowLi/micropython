#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H

#include "hal_data.h"
#include "py/obj.h"

typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    qstr name;
    bsp_io_port_pin_t pin;
} machine_pin_obj_t;

#include "genhdr/pins.h"

extern const mp_obj_type_t machine_pin_type;

const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);

#endif
