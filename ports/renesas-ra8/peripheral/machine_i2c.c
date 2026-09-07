#include <stdbool.h>
#include <stdint.h>

#include "extmod/modmachine.h"
#include "pin.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "i2c.h"

#if MICROPY_PY_MACHINE_I2C

#define MACHINE_I2C_DEFAULT_FREQ_HZ      (400000U)
#define MACHINE_I2C_DEFAULT_TIMEOUT_US   (50000U)
#define MACHINE_I2C_PIN_OPTIONS          (IOPORT_CFG_NMOS_ENABLE | IOPORT_CFG_DRIVE_MID)

typedef struct _machine_hard_i2c_obj_t {
    mp_obj_base_t base;
    bsp_io_port_pin_t scl;
    bsp_io_port_pin_t sda;
    bsp_io_port_pin_t default_scl;
    bsp_io_port_pin_t default_sda;
    uint8_t i2c_id;
    bool initialized;
    uint32_t freq;
    uint32_t timeout_us;
} machine_hard_i2c_obj_t;

static machine_hard_i2c_obj_t machine_hard_i2c_obj[] = {
    {
        .base = {&machine_i2c_type},
        .scl = BSP_IO_PORT_05_PIN_12,
        .sda = BSP_IO_PORT_05_PIN_11,
        .default_scl = BSP_IO_PORT_05_PIN_12,
        .default_sda = BSP_IO_PORT_05_PIN_11,
        .i2c_id = 1,
        .initialized = false,
        .freq = MACHINE_I2C_DEFAULT_FREQ_HZ,
        .timeout_us = MACHINE_I2C_DEFAULT_TIMEOUT_US,
    },
    {
        .base = {&machine_i2c_type},
        .scl = BSP_IO_PORT_05_PIN_15,
        .sda = BSP_IO_PORT_05_PIN_14,
        .default_scl = BSP_IO_PORT_05_PIN_15,
        .default_sda = BSP_IO_PORT_05_PIN_14,
        .i2c_id = 2,
        .initialized = false,
        .freq = MACHINE_I2C_DEFAULT_FREQ_HZ,
        .timeout_us = MACHINE_I2C_DEFAULT_TIMEOUT_US,
    },
};

static machine_hard_i2c_obj_t *machine_hard_i2c_find(mp_int_t id)
{
    for (size_t i = 0; i < MP_ARRAY_SIZE(machine_hard_i2c_obj); ++i) {
        if (machine_hard_i2c_obj[i].i2c_id == id) {
            return &machine_hard_i2c_obj[i];
        }
    }

    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("I2C(%d) does not exist"), id);
    return NULL;
}

static void machine_hard_i2c_validate_pins(machine_hard_i2c_obj_t *self, bsp_io_port_pin_t scl, bsp_io_port_pin_t sda)
{
    const machine_pin_af_obj_t *scl_af = machine_pin_find_af(scl, MACHINE_PIN_AF_PERIPHERAL_IIC, self->i2c_id, MACHINE_PIN_AF_I2C_SCL);
    if (scl_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad SCL pin"));
    }

    const machine_pin_af_obj_t *sda_af = machine_pin_find_af(sda, MACHINE_PIN_AF_PERIPHERAL_IIC, self->i2c_id, MACHINE_PIN_AF_I2C_SDA);
    if (sda_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad SDA pin"));
    }

    if (scl_af->group != sda_af->group) {
        mp_raise_ValueError(MP_ERROR_TEXT("SCL and SDA must use the same IIC pin group"));
    }
}

static bool machine_hard_i2c_take_pins(machine_hard_i2c_obj_t *self)
{
    if (!machine_pin_take(self->scl)) {
        return false;
    }

    if (!machine_pin_take(self->sda)) {
        machine_pin_give(self->scl);
        return false;
    }

    return true;
}

static bool machine_hard_i2c_give_pins(machine_hard_i2c_obj_t *self)
{
    bool success = true;
    success = machine_pin_give(self->sda) && success;
    success = machine_pin_give(self->scl) && success;
    return success;
}

static void machine_hard_i2c_configure_pins(machine_hard_i2c_obj_t *self)
{
    machine_pin_configure_alt(self->scl, IOPORT_PERIPHERAL_IIC, MACHINE_I2C_PIN_OPTIONS);
    machine_pin_configure_alt(self->sda, IOPORT_PERIPHERAL_IIC, MACHINE_I2C_PIN_OPTIONS);
}

static void machine_hard_i2c_stop(machine_hard_i2c_obj_t *self)
{
    if (!self->initialized) {
        return;
    }

    if (!i2c_deinit(self->i2c_id)) {
        mp_raise_OSError(MP_EIO);
    }

    self->initialized = false;
    if (!machine_hard_i2c_give_pins(self)) {
        mp_raise_OSError(MP_EIO);
    }
}

static void machine_hard_i2c_start(machine_hard_i2c_obj_t *self)
{
    if (!machine_hard_i2c_take_pins(self)) {
        mp_raise_OSError(MP_EBUSY);
    }

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        machine_hard_i2c_configure_pins(self);
        nlr_pop();
    } else {
        machine_hard_i2c_give_pins(self);
        nlr_jump(nlr.ret_val);
    }

    int error = i2c_init(self->i2c_id, self->freq);
    if (error != 0) {
        machine_hard_i2c_give_pins(self);
        if (error == MP_EINVAL) {
            mp_raise_ValueError(MP_ERROR_TEXT("unsupported freq"));
        }
        mp_raise_OSError(error);
    }

    self->initialized = true;
}

static uint32_t machine_hard_i2c_parse_positive(mp_obj_t value_in, mp_rom_error_text_t error_text)
{
    mp_int_t value = mp_obj_get_int(value_in);
    if (value <= 0) {
        mp_raise_ValueError(error_text);
    }
    return (uint32_t)value;
}

static void machine_hard_i2c_reconfigure(machine_hard_i2c_obj_t *self, bsp_io_port_pin_t scl, bsp_io_port_pin_t sda, uint32_t freq, uint32_t timeout_us)
{
    machine_hard_i2c_validate_pins(self, scl, sda);
    int error = i2c_validate_freq(self->i2c_id, freq);
    if (error == MP_EINVAL) {
        mp_raise_ValueError(MP_ERROR_TEXT("unsupported freq"));
    }
    if (error != 0) {
        mp_raise_OSError(error);
    }
    machine_hard_i2c_stop(self);

    self->scl = scl;
    self->sda = sda;
    self->freq = freq;
    self->timeout_us = timeout_us;
    machine_hard_i2c_start(self);
}

static void machine_hard_i2c_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    (void)kind;
    machine_hard_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(
        print,
        "I2C(%u, initialized=%u, freq=%u, scl=P%X%02u, sda=P%X%02u, timeout=%u)",
        self->i2c_id,
        self->initialized,
        self->freq,
        ((uint32_t)self->scl >> 8) & 0xff,
        (uint32_t)self->scl & 0xff,
        ((uint32_t)self->sda >> 8) & 0xff,
        (uint32_t)self->sda & 0xff,
        self->timeout_us);
}

static void machine_hard_i2c_init(mp_obj_base_t *self_in, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum { ARG_scl, ARG_sda, ARG_freq, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_scl, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_sda, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };
    machine_hard_i2c_obj_t *self = (machine_hard_i2c_obj_t *)self_in;
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    bool has_scl = args[ARG_scl].u_obj != MP_OBJ_NULL;
    bool has_sda = args[ARG_sda].u_obj != MP_OBJ_NULL;
    if (has_scl != has_sda) {
        mp_raise_ValueError(MP_ERROR_TEXT("must specify both scl and sda"));
    }

    bsp_io_port_pin_t scl = self->scl;
    bsp_io_port_pin_t sda = self->sda;
    if (has_scl) {
        scl = machine_pin_find(args[ARG_scl].u_obj)->pin;
        sda = machine_pin_find(args[ARG_sda].u_obj)->pin;
    }

    uint32_t freq = self->freq;
    if (args[ARG_freq].u_obj != MP_OBJ_NULL) {
        freq = machine_hard_i2c_parse_positive(args[ARG_freq].u_obj, MP_ERROR_TEXT("bad freq"));
    }

    uint32_t timeout_us = self->timeout_us;
    if (args[ARG_timeout].u_obj != MP_OBJ_NULL) {
        timeout_us = machine_hard_i2c_parse_positive(args[ARG_timeout].u_obj, MP_ERROR_TEXT("bad timeout"));
    }

    machine_hard_i2c_reconfigure(self, scl, sda, freq, timeout_us);
}

static mp_obj_t machine_i2c_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_id, ARG_scl, ARG_sda, ARG_freq, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_scl, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_sda, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    (void)type;
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t id = mp_obj_get_int(args[ARG_id].u_obj);
    machine_hard_i2c_obj_t *self = machine_hard_i2c_find(id);

    bool has_scl = args[ARG_scl].u_obj != MP_OBJ_NULL;
    bool has_sda = args[ARG_sda].u_obj != MP_OBJ_NULL;
    if (has_scl != has_sda) {
        mp_raise_ValueError(MP_ERROR_TEXT("must specify both scl and sda"));
    }

    bsp_io_port_pin_t scl = self->default_scl;
    bsp_io_port_pin_t sda = self->default_sda;
    if (has_scl) {
        scl = machine_pin_find(args[ARG_scl].u_obj)->pin;
        sda = machine_pin_find(args[ARG_sda].u_obj)->pin;
    }

    uint32_t freq = MACHINE_I2C_DEFAULT_FREQ_HZ;
    if (args[ARG_freq].u_obj != MP_OBJ_NULL) {
        freq = machine_hard_i2c_parse_positive(args[ARG_freq].u_obj, MP_ERROR_TEXT("bad freq"));
    }

    uint32_t timeout_us = MACHINE_I2C_DEFAULT_TIMEOUT_US;
    if (args[ARG_timeout].u_obj != MP_OBJ_NULL) {
        timeout_us = machine_hard_i2c_parse_positive(args[ARG_timeout].u_obj, MP_ERROR_TEXT("bad timeout"));
    }

    machine_hard_i2c_reconfigure(self, scl, sda, freq, timeout_us);
    return MP_OBJ_FROM_PTR(self);
}

static int machine_hard_i2c_transfer_single(mp_obj_base_t *self_in, uint16_t addr, size_t len, uint8_t *buf, unsigned int flags)
{
    machine_hard_i2c_obj_t *self = (machine_hard_i2c_obj_t *)self_in;
    if (!self->initialized) {
        return -MP_ENODEV;
    }

    bool read = (flags & MP_MACHINE_I2C_FLAG_READ) != 0;
    bool stop = (flags & MP_MACHINE_I2C_FLAG_STOP) != 0;
    uint32_t timeout_ms = self->timeout_us / 1000U;
    if ((self->timeout_us % 1000U) != 0U) {
        ++timeout_ms;
    }
    size_t transferred;
    int error = i2c_transfer(self->i2c_id, addr, len, buf, read, stop, timeout_ms, &transferred);

    if (error != 0) {
        return -error;
    }

    return (int)transferred;
}

static mp_obj_t machine_hard_i2c_deinit(mp_obj_t self_in)
{
    machine_hard_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    machine_hard_i2c_stop(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_i2c_deinit_obj, machine_hard_i2c_deinit);

static void machine_hard_i2c_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest)
{
    if (dest[0] == MP_OBJ_NULL && attr == MP_QSTR_deinit) {
        dest[0] = MP_OBJ_FROM_PTR(&machine_hard_i2c_deinit_obj);
        dest[1] = self_in;
        return;
    }

    if (dest[0] == MP_OBJ_NULL) {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

void machine_i2c_deinit_all(void)
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_i2c_obj); ++index) {
        machine_hard_i2c_obj_t *self = &machine_hard_i2c_obj[index];
        if (!self->initialized) {
            continue;
        }
        if (i2c_deinit(self->i2c_id)) {
            self->initialized = false;
            machine_hard_i2c_give_pins(self);
        }
    }
}

static const mp_machine_i2c_p_t machine_hard_i2c_p = {
    .init = machine_hard_i2c_init,
    .transfer = mp_machine_i2c_transfer_adaptor,
    .transfer_single = machine_hard_i2c_transfer_single,
};

MP_DEFINE_CONST_OBJ_TYPE(machine_i2c_type, MP_QSTR_I2C, MP_TYPE_FLAG_NONE, make_new, machine_i2c_make_new, print, machine_hard_i2c_print, attr, machine_hard_i2c_attr, protocol, &machine_hard_i2c_p, locals_dict, &mp_machine_i2c_locals_dict);

#endif // MICROPY_PY_MACHINE_I2C
