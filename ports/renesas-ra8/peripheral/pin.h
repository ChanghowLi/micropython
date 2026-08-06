#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H

#include "hal_data.h"
#include "py/obj.h"

typedef enum _machine_pin_owner_t
{
    MACHINE_PIN_OWNER_FREE = 0,
    MACHINE_PIN_OWNER_GPIO,
    MACHINE_PIN_OWNER_I2C,
    MACHINE_PIN_OWNER_IRQ,
    MACHINE_PIN_OWNER_SPI,
    MACHINE_PIN_OWNER_UART,
} machine_pin_owner_t;

typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    qstr name;
    bsp_io_port_pin_t pin;
    uint32_t alt_mask;
    int8_t irq_channel;
    bool irq_deep_standby;
} machine_pin_obj_t;

#include "genhdr/pins.h"

extern const mp_obj_dict_t machine_pin_board_pins_locals_dict;
extern const mp_obj_dict_t machine_pin_cpu_pins_locals_dict;
extern const mp_obj_type_t machine_pin_board_pins_obj_type;
extern const mp_obj_type_t machine_pin_cpu_pins_obj_type;
extern const mp_obj_type_t machine_pin_type;

const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);
bool machine_pin_take(bsp_io_port_pin_t pin_id, machine_pin_owner_t owner);
void machine_pin_give(bsp_io_port_pin_t pin_id, machine_pin_owner_t owner);

#endif
