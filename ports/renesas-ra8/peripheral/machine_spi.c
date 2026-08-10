#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extmod/modmachine.h"
#include "hal_data.h"
#include "pin.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "spi.h"

#define DEFAULT_SPI_BAUDRATE     (1000000)
#define DEFAULT_SPI_BITS         (8)
#define DEFAULT_SPI_FIRSTBIT     (MICROPY_PY_MACHINE_SPI_MSB)
#define DEFAULT_SPI_PHASE        (0)
#define DEFAULT_SPI_POLARITY     (0)

#define IS_VALID_BITS(value)     ((value) == 8)
#define IS_VALID_FIRSTBIT(value) ((value) == MICROPY_PY_MACHINE_SPI_MSB)
#define IS_VALID_PHASE(value)    (((value) == 0) || ((value) == 1))
#define IS_VALID_POLARITY(value) (((value) == 0) || ((value) == 1))

typedef struct _machine_hard_spi_obj_t {
    mp_obj_base_t base;
    uint8_t spi_id;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    uint32_t baudrate;
    bsp_io_port_pin_t sck;
    bsp_io_port_pin_t mosi;
    bsp_io_port_pin_t miso;
} machine_hard_spi_obj_t;

static machine_hard_spi_obj_t machine_hard_spi_obj[] = {
    {
        .base = {&machine_spi_type},
        .spi_id = 0,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .sck = BSP_IO_PORT_07_PIN_02,
        .mosi = BSP_IO_PORT_07_PIN_01,
        .miso = BSP_IO_PORT_07_PIN_00,
    },
};

static machine_hard_spi_obj_t *machine_hard_spi_find(mp_int_t spi_id) {
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_spi_obj); ++index) {
        if (machine_hard_spi_obj[index].spi_id == spi_id) {
            return &machine_hard_spi_obj[index];
        }
    }

    mp_raise_msg_varg(
        &mp_type_ValueError,
        MP_ERROR_TEXT("SPI(%d) does not exist"),
        spi_id
        );

    return NULL;
}

static bool machine_hard_spi_take_pins(machine_hard_spi_obj_t *self) 
{
    if (spi_deinit(self->spi_id)) 
    {
        machine_pin_give(self->miso);
        machine_pin_give(self->mosi);
        machine_pin_give(self->sck);
    }

    if (!machine_pin_take(self->sck)) 
    {
        return false;
    }

    if (!machine_pin_take(self->mosi)) 
    {
        machine_pin_give(self->sck);
        return false;
    }

    if (!machine_pin_take(self->miso)) 
    {
        machine_pin_give(self->mosi);
        machine_pin_give(self->sck);
        return false;
    }

    return true;
}

static void machine_hard_spi_give_pins(machine_hard_spi_obj_t *self) 
{
    machine_pin_give(self->miso);
    machine_pin_give(self->mosi);
    machine_pin_give(self->sck);
}

// Python SPI object construction and representation.

static void machine_hard_spi_print(
    const mp_print_t *print,
    mp_obj_t self_in,
    mp_print_kind_t kind
    ) {
    machine_hard_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);

    (void)kind;

    mp_printf(
        print,
        "SPI(%u, baudrate=%u, polarity=%u, phase=%u, bits=%u, "
        "firstbit=%u, sck=P%u%02u, mosi=P%u%02u, miso=P%u%02u)",
        self->spi_id,
        self->baudrate,
        self->polarity,
        self->phase,
        self->bits,
        self->firstbit,
        ((uint32_t)self->sck >> 8) & 0xff,
        (uint32_t)self->sck & 0xff,
        ((uint32_t)self->mosi >> 8) & 0xff,
        (uint32_t)self->mosi & 0xff,
        ((uint32_t)self->miso >> 8) & 0xff,
        (uint32_t)self->miso & 0xff
        );
}

static mp_obj_t machine_hard_spi_make_new(
    const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *all_args
    ) {
    enum {
        ARG_id,
        ARG_baudrate,
        ARG_polarity,
        ARG_phase,
        ARG_bits,
        ARG_firstbit,
        ARG_sck,
        ARG_mosi,
        ARG_miso,
    };

    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_baudrate, MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_phase, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_bits, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_sck, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    (void)type;

    mp_arg_parse_all_kw_array(
        n_args,
        n_kw,
        all_args,
        MP_ARRAY_SIZE(allowed_args),
        allowed_args,
        args
        );

    mp_int_t spi_id = mp_obj_get_int(args[ARG_id].u_obj);
    machine_hard_spi_obj_t *self = machine_hard_spi_find(spi_id);

    if (n_args == 1 && n_kw == 0) 
    {
        return MP_OBJ_FROM_PTR(self);
    }

    if (args[ARG_baudrate].u_int != -1) 
    {
        if (args[ARG_baudrate].u_int <= 0) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad baudrate"));
        }
        self->baudrate = args[ARG_baudrate].u_int;
    }

    if (args[ARG_polarity].u_int != -1) 
    {
        if (!IS_VALID_POLARITY(args[ARG_polarity].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad polarity"));
        }
        self->polarity = args[ARG_polarity].u_int;
    }

    if (args[ARG_phase].u_int != -1) 
    {
        if (!IS_VALID_PHASE(args[ARG_phase].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad phase"));
        }
        self->phase = args[ARG_phase].u_int;
    }

    if (args[ARG_bits].u_int != -1) 
    {
        if (!IS_VALID_BITS(args[ARG_bits].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad bits"));
        }
        self->bits = args[ARG_bits].u_int;
    }

    if (args[ARG_firstbit].u_int != -1) 
    {
        if (!IS_VALID_FIRSTBIT(args[ARG_firstbit].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad firstbit"));
        }
        self->firstbit = args[ARG_firstbit].u_int;
    }

    if (args[ARG_sck].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin = machine_pin_find(args[ARG_sck].u_obj);

        if (pin->pin != self->sck) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad SCK pin"));
        }
    }

    if (args[ARG_mosi].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin = machine_pin_find(args[ARG_mosi].u_obj);

        if (pin->pin != self->mosi) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad MOSI pin"));
        }
    }

    if (args[ARG_miso].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin = machine_pin_find(args[ARG_miso].u_obj);

        if (pin->pin != self->miso) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad MISO pin"));
        }
    }

    if (!machine_hard_spi_take_pins(self)) 
    {
        mp_raise_OSError(MP_EBUSY);
    }

    int error = spi_init(
        self->spi_id,
        self->baudrate,
        self->polarity,
        self->phase,
        self->bits,
        self->firstbit
        );

    if (error != 0) 
    {
        machine_hard_spi_give_pins(self);
        mp_raise_OSError(error);
    }

    return MP_OBJ_FROM_PTR(self);
}

static void machine_hard_spi_init(
    mp_obj_base_t *self_in,
    size_t n_args,
    const mp_obj_t *pos_args,
    mp_map_t *kw_args
    ) {
    machine_hard_spi_obj_t *self =
        (machine_hard_spi_obj_t *)self_in;

    enum {
        ARG_baudrate,
        ARG_polarity,
        ARG_phase,
        ARG_bits,
        ARG_firstbit,
        ARG_sck,
        ARG_mosi,
        ARG_miso,
    };

    static const mp_arg_t allowed_args[] = 
    {
        {MP_QSTR_baudrate, MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_phase, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_bits, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_sck, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    mp_arg_parse_all(
        n_args,
        pos_args,
        kw_args,
        MP_ARRAY_SIZE(allowed_args),
        allowed_args,
        args
        );

    if (args[ARG_baudrate].u_int != -1) 
    {
        if (args[ARG_baudrate].u_int <= 0) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad baudrate"));
        }
        self->baudrate = args[ARG_baudrate].u_int;
    }

    if (args[ARG_polarity].u_int != -1) 
    {
        if (!IS_VALID_POLARITY(args[ARG_polarity].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad polarity"));
        }
        self->polarity = args[ARG_polarity].u_int;
    }

    if (args[ARG_phase].u_int != -1) 
    {
        if (!IS_VALID_PHASE(args[ARG_phase].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad phase"));
        }
        self->phase = args[ARG_phase].u_int;
    }

    if (args[ARG_bits].u_int != -1) 
    {
        if (!IS_VALID_BITS(args[ARG_bits].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad bits"));
        }
        self->bits = args[ARG_bits].u_int;
    }

    if (args[ARG_firstbit].u_int != -1) 
    {
        if (!IS_VALID_FIRSTBIT(args[ARG_firstbit].u_int)) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad firstbit"));
        }
        self->firstbit = args[ARG_firstbit].u_int;
    }

    if (args[ARG_sck].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin =
            machine_pin_find(args[ARG_sck].u_obj);

        if (pin->pin != self->sck) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad SCK pin"));
        }
    }

    if (args[ARG_mosi].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin =
            machine_pin_find(args[ARG_mosi].u_obj);

        if (pin->pin != self->mosi) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad MOSI pin"));
        }
    }

    if (args[ARG_miso].u_obj != MP_OBJ_NULL) 
    {
        const machine_pin_obj_t *pin =
            machine_pin_find(args[ARG_miso].u_obj);

        if (pin->pin != self->miso) 
        {
            mp_raise_ValueError(MP_ERROR_TEXT("bad MISO pin"));
        }
    }

    if (!machine_hard_spi_take_pins(self)) 
    {
        mp_raise_OSError(MP_EBUSY);
    }

    int error = spi_init(
        self->spi_id,
        self->baudrate,
        self->polarity,
        self->phase,
        self->bits,
        self->firstbit
        );

    if (error != 0) 
    {
        machine_hard_spi_give_pins(self);
        mp_raise_OSError(error);
    }
}

static void machine_hard_spi_deinit(mp_obj_base_t *self_in) 
{
    machine_hard_spi_obj_t *self =
        (machine_hard_spi_obj_t *)self_in;

    if (spi_deinit(self->spi_id)) 
    {
        machine_hard_spi_give_pins(self);
    }
}

static void machine_hard_spi_transfer(
    mp_obj_base_t *self_in,
    size_t len,
    const uint8_t *src,
    uint8_t *dest
    ) {
    machine_hard_spi_obj_t *self = (machine_hard_spi_obj_t *)self_in;

    if (len == 0) 
    {
        return;
    }

    uint32_t timeout_ms =
        (uint32_t)(
            ((uint64_t)len * self->bits * 1000U +
             self->baudrate - 1U) /
            self->baudrate
            ) +
        100U;

    int error = spi_transfer(
        self->spi_id,
        len,
        src,
        dest,
        timeout_ms
        );

    if (error != 0) 
    {
        if (error == MP_ETIMEDOUT) 
        {
            machine_hard_spi_give_pins(self);
        }
        mp_raise_OSError(error);
    }
}

static mp_obj_t machine_hard_spi_deinit_method(mp_obj_t self_in) 
{
    machine_hard_spi_deinit(MP_OBJ_TO_PTR(self_in));
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_init_method(
    size_t n_args,
    const mp_obj_t *pos_args,
    mp_map_t *kw_args
    ) {
    machine_hard_spi_init(
        MP_OBJ_TO_PTR(pos_args[0]),
        n_args - 1,
        pos_args + 1,
        kw_args
        );
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_write(mp_obj_t self_in, mp_obj_t buffer_in) 
{
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(buffer_in, &buffer, MP_BUFFER_READ);

    machine_hard_spi_transfer(
        MP_OBJ_TO_PTR(self_in),
        buffer.len,
        buffer.buf,
        NULL
        );
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_write_readinto(
    mp_obj_t self_in,
    mp_obj_t write_buffer_in,
    mp_obj_t read_buffer_in
    ) {
    mp_buffer_info_t write_buffer;
    mp_buffer_info_t read_buffer;

    mp_get_buffer_raise(write_buffer_in, &write_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(read_buffer_in, &read_buffer, MP_BUFFER_WRITE);

    if (write_buffer.len != read_buffer.len) 
    {
        mp_raise_ValueError(MP_ERROR_TEXT("buffers must be the same length"));
    }

    machine_hard_spi_transfer(
        MP_OBJ_TO_PTR(self_in),
        write_buffer.len,
        write_buffer.buf,
        read_buffer.buf
        );
    return mp_const_none;
}

void machine_spi_deinit_all(void) 
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_spi_obj); ++index) 
    {
        machine_hard_spi_obj_t *self = &machine_hard_spi_obj[index];
        if (spi_deinit(self->spi_id)) 
        {
            machine_hard_spi_give_pins(self);
        }
    }
}

static const mp_machine_spi_p_t machine_hard_spi_p = 
{
    .init = machine_hard_spi_init,
    .deinit = machine_hard_spi_deinit,
    .transfer = machine_hard_spi_transfer,
};

static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_spi_deinit_obj, machine_hard_spi_deinit_method);
static MP_DEFINE_CONST_FUN_OBJ_KW(machine_hard_spi_init_obj, 1, machine_hard_spi_init_method);
static MP_DEFINE_CONST_FUN_OBJ_2(machine_hard_spi_write_obj, machine_hard_spi_write);
static MP_DEFINE_CONST_FUN_OBJ_3(machine_hard_spi_write_readinto_obj, machine_hard_spi_write_readinto);

static const mp_rom_map_elem_t machine_hard_spi_locals_dict_table[] = 
{
    {MP_ROM_QSTR(MP_QSTR_MSB), MP_ROM_INT(MICROPY_PY_MACHINE_SPI_MSB)},
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_hard_spi_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_hard_spi_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&machine_hard_spi_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_write_readinto), MP_ROM_PTR(&machine_hard_spi_write_readinto_obj)},
};

static MP_DEFINE_CONST_DICT(
    machine_hard_spi_locals_dict,
    machine_hard_spi_locals_dict_table
    );

MP_DEFINE_CONST_OBJ_TYPE(
    machine_spi_type,
    MP_QSTR_SPI,
    MP_TYPE_FLAG_NONE,
    make_new, machine_hard_spi_make_new,
    print, machine_hard_spi_print,
    protocol, &machine_hard_spi_p,
    locals_dict, &machine_hard_spi_locals_dict
    );
