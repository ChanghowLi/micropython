#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H

#include "hal_data.h"
#include "py/obj.h"

enum {
    MACHINE_PIN_IRQ_FALLING = 1,
    MACHINE_PIN_IRQ_RISING = 2,
    MACHINE_PIN_IRQ_LOW_LEVEL = 4,
    MACHINE_PIN_IRQ_HIGH_LEVEL = 8,
};

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
extern const mp_obj_fun_builtin_var_t machine_pin_irq_obj;
extern const mp_obj_type_t machine_pin_board_pins_obj_type;
extern const mp_obj_type_t machine_pin_cpu_pins_obj_type;
extern const mp_obj_type_t machine_pin_type;

const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);
void machine_pin_deinit_all(void);
void machine_pin_give(bsp_io_port_pin_t pin_id);
void machine_pin_irq_deinit(void);
bool machine_pin_irq_is_active(const machine_pin_obj_t *pin);
bool machine_pin_take(bsp_io_port_pin_t pin_id);

#endif
