#include "peripheral/pin.h"

static const machine_pin_obj_t machine_pin_P000_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P000,
    .pin = BSP_IO_PORT_00_PIN_00,
};

static const machine_pin_obj_t machine_pin_P001_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P001,
    .pin = BSP_IO_PORT_00_PIN_01,
};

static const machine_pin_obj_t machine_pin_P002_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P002,
    .pin = BSP_IO_PORT_00_PIN_02,
};

static const machine_pin_obj_t machine_pin_P003_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P003,
    .pin = BSP_IO_PORT_00_PIN_03,
};

static const machine_pin_obj_t machine_pin_P007_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P007,
    .pin = BSP_IO_PORT_00_PIN_07,
};

static const machine_pin_obj_t machine_pin_P010_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P010,
    .pin = BSP_IO_PORT_00_PIN_10,
};

static const machine_pin_obj_t machine_pin_P011_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P011,
    .pin = BSP_IO_PORT_00_PIN_11,
};

static const machine_pin_obj_t machine_pin_P014_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P014,
    .pin = BSP_IO_PORT_00_PIN_14,
};

static const machine_pin_obj_t machine_pin_PC09_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC09,
    .pin = BSP_IO_PORT_12_PIN_09,
};

static const machine_pin_obj_t machine_pin_PC15_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC15,
    .pin = BSP_IO_PORT_12_PIN_15,
};

const machine_pin_obj_t *const machine_pin_generated_pins[] = {
    &machine_pin_P000_obj,
    &machine_pin_P001_obj,
    &machine_pin_P002_obj,
    &machine_pin_P003_obj,
    &machine_pin_P007_obj,
    &machine_pin_P010_obj,
    &machine_pin_P011_obj,
    &machine_pin_P014_obj,
    &machine_pin_PC09_obj,
    &machine_pin_PC15_obj,
};

const size_t machine_pin_generated_pins_count =
    MP_ARRAY_SIZE(machine_pin_generated_pins);
