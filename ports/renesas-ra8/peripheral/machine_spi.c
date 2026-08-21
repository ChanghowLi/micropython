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
#include "sci.h"
#include "spi.h"

#define DEFAULT_SPI_BAUDRATE     (1000000)
#define DEFAULT_SPI_BITS         (8)
#define DEFAULT_SPI_FIRSTBIT     (MICROPY_PY_MACHINE_SPI_MSB)
#define DEFAULT_SPI_PHASE        (0)
#define DEFAULT_SPI_POLARITY     (0)

#define IS_VALID_BITS(value)     ((value) == 8)
#define IS_VALID_FIRSTBIT(value) (((value) == MICROPY_PY_MACHINE_SPI_MSB) || ((value) == MICROPY_PY_MACHINE_SPI_LSB))
#define IS_VALID_PHASE(value)    (((value) == 0) || ((value) == 1))
#define IS_VALID_POLARITY(value) (((value) == 0) || ((value) == 1))

typedef struct _machine_hard_spi_obj_t
{
    mp_obj_base_t base;
    bsp_io_port_pin_t sck;
    bsp_io_port_pin_t mosi;
    bsp_io_port_pin_t miso;
    bsp_io_port_pin_t default_sck;
    bsp_io_port_pin_t default_mosi;
    bsp_io_port_pin_t default_miso;
    const machine_pin_obj_t *cs;
    ioport_peripheral_t peripheral;
    uint8_t spi_id;
    bool sci_taken;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    uint32_t baudrate;
} machine_hard_spi_obj_t;

static machine_hard_spi_obj_t machine_hard_spi_obj[] = 
{
    #if defined(MICROPY_HW_SPI0_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 0,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .peripheral = IOPORT_PERIPHERAL_SPI,
        .sck = MICROPY_HW_SPI0_SCK,
        .mosi = MICROPY_HW_SPI0_MOSI,
        .miso = MICROPY_HW_SPI0_MISO,
    },
    #endif

    #if defined(MICROPY_HW_SPI1_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 1,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .peripheral = IOPORT_PERIPHERAL_SPI,
        .sck = MICROPY_HW_SPI1_SCK,
        .mosi = MICROPY_HW_SPI1_MOSI,
        .miso = MICROPY_HW_SPI1_MISO,
    },
    #endif

    #if defined(MICROPY_HW_SCI1_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 11,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI1_3_5_7_9,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI1_SCK,
        .default_mosi = MICROPY_HW_SCI1_TXD,
        .default_miso = MICROPY_HW_SCI1_RXD,
        .sck = MICROPY_HW_SCI1_SCK,
        .mosi = MICROPY_HW_SCI1_TXD,
        .miso = MICROPY_HW_SCI1_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI2_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 12,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI2_SCK,
        .default_mosi = MICROPY_HW_SCI2_TXD,
        .default_miso = MICROPY_HW_SCI2_RXD,
        .sck = MICROPY_HW_SCI2_SCK,
        .mosi = MICROPY_HW_SCI2_TXD,
        .miso = MICROPY_HW_SCI2_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI3_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 13,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI1_3_5_7_9,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI3_SCK,
        .default_mosi = MICROPY_HW_SCI3_TXD,
        .default_miso = MICROPY_HW_SCI3_RXD,
        .sck = MICROPY_HW_SCI3_SCK,
        .mosi = MICROPY_HW_SCI3_TXD,
        .miso = MICROPY_HW_SCI3_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI4_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 14,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI4_SCK,
        .default_mosi = MICROPY_HW_SCI4_TXD,
        .default_miso = MICROPY_HW_SCI4_RXD,
        .sck = MICROPY_HW_SCI4_SCK,
        .mosi = MICROPY_HW_SCI4_TXD,
        .miso = MICROPY_HW_SCI4_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI5_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 15,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI1_3_5_7_9,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI5_SCK,
        .default_mosi = MICROPY_HW_SCI5_TXD,
        .default_miso = MICROPY_HW_SCI5_RXD,
        .sck = MICROPY_HW_SCI5_SCK,
        .mosi = MICROPY_HW_SCI5_TXD,
        .miso = MICROPY_HW_SCI5_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI6_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 16,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI6_SCK,
        .default_mosi = MICROPY_HW_SCI6_TXD,
        .default_miso = MICROPY_HW_SCI6_RXD,
        .sck = MICROPY_HW_SCI6_SCK,
        .mosi = MICROPY_HW_SCI6_TXD,
        .miso = MICROPY_HW_SCI6_RXD,
    },
    #endif

    #if defined(MICROPY_HW_SCI8_SCK)
    {
        .base = {&machine_spi_type},
        .spi_id = 18,
        .polarity = DEFAULT_SPI_POLARITY,
        .phase = DEFAULT_SPI_PHASE,
        .bits = DEFAULT_SPI_BITS,
        .firstbit = DEFAULT_SPI_FIRSTBIT,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .baudrate = DEFAULT_SPI_BAUDRATE,
        .default_sck = MICROPY_HW_SCI8_SCK,
        .default_mosi = MICROPY_HW_SCI8_TXD,
        .default_miso = MICROPY_HW_SCI8_RXD,
        .sck = MICROPY_HW_SCI8_SCK,
        .mosi = MICROPY_HW_SCI8_TXD,
        .miso = MICROPY_HW_SCI8_RXD,
    },
    #endif
};

static machine_hard_spi_obj_t *machine_hard_spi_find(mp_int_t spi_id) 
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_spi_obj); ++index) {
        if (machine_hard_spi_obj[index].spi_id == spi_id) {
            return &machine_hard_spi_obj[index];
        }
    }

    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("SPI(%d) does not exist"), spi_id);

    return NULL;
}

static const machine_pin_af_obj_t *machine_hard_spi_find_sci_af(
    bsp_io_port_pin_t pin,
    uint8_t channel,
    machine_pin_af_signal_t signal)
{
    for (size_t index = 0; index < machine_pin_sci_spi_afs_count; ++index) {
        const machine_pin_af_obj_t *af = &machine_pin_sci_spi_afs[index];

        if (af->pin == pin && af->channel == channel && af->signal == signal) {
            return af;
        }
    }

    return NULL;
}

static void machine_hard_spi_validate_sci_pins(uint8_t channel, bsp_io_port_pin_t sck, bsp_io_port_pin_t mosi, bsp_io_port_pin_t miso)
{
    const machine_pin_af_obj_t *sck_af = machine_hard_spi_find_sci_af(sck, channel, MACHINE_PIN_AF_SCI_SCK);
    if (sck_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad SCK pin"));
    }

    const machine_pin_af_obj_t *mosi_af = machine_hard_spi_find_sci_af(
        mosi, channel, MACHINE_PIN_AF_SCI_MOSI
    );
    if (mosi_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad MOSI pin"));
    }

    const machine_pin_af_obj_t *miso_af = machine_hard_spi_find_sci_af(miso, channel, MACHINE_PIN_AF_SCI_MISO);
    if (miso_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad MISO pin"));
    }

    if (sck_af->group != mosi_af->group || sck_af->group != miso_af->group) {
        mp_raise_ValueError(MP_ERROR_TEXT("SPI pins must use the same group"));
    }
}

static void machine_hard_spi_configure_pins(machine_hard_spi_obj_t *self)
{
    machine_pin_configure_alt(self->sck, self->peripheral);
    machine_pin_configure_alt(self->mosi, self->peripheral);
    machine_pin_configure_alt(self->miso, self->peripheral);
}

static bool machine_hard_spi_take_pins(machine_hard_spi_obj_t *self, const machine_pin_obj_t *new_cs)
{
    if (!machine_pin_take(self->sck)) {
        return false;
    }

    if (!machine_pin_take(self->mosi)) {
        machine_pin_give(self->sck);
        return false;
    }

    if (!machine_pin_take(self->miso)) {
        machine_pin_give(self->mosi);
        machine_pin_give(self->sck);
        return false;
    }

    if (new_cs != NULL) {
        if (!machine_pin_take(new_cs->pin)) {
            machine_pin_give(self->miso);
            machine_pin_give(self->mosi);
            machine_pin_give(self->sck);
            return false;
        }

        machine_pin_configure_output(new_cs, true);
    }

    self->cs = new_cs;
    return true;
}

static void machine_hard_spi_give_pins(machine_hard_spi_obj_t *self) 
{
    if (self->cs != NULL) {
        machine_pin_give(self->cs->pin);
        self->cs = NULL;
    }

    machine_pin_give(self->miso);
    machine_pin_give(self->mosi);
    machine_pin_give(self->sck);
}

static void machine_hard_spi_give_sci(machine_hard_spi_obj_t *self)
{
    if (self->spi_id >= 10 && self->sci_taken) {
        machine_sci_give(self->spi_id - 10, MACHINE_SCI_OWNER_SPI);
        self->sci_taken = false;
    }
}

static void machine_hard_spi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) 
{
    machine_hard_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);

    (void)kind;

    mp_printf(
        print,
        "SPI(%u, baudrate=%u, polarity=%u, phase=%u, bits=%u, "
        "firstbit=%u, sck=P%X%02u, mosi=P%X%02u, miso=P%X%02u",
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
        (uint32_t)self->miso & 0xff);

    if (self->cs == NULL) {
        mp_printf(print, ", cs=None)");
    } else {
        mp_printf(print, ", cs=P%X%02u)", ((uint32_t)self->cs->pin >> 8) & 0xff, (uint32_t)self->cs->pin & 0xff);
    }
}

static mp_obj_t machine_hard_spi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
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
        ARG_cs,
    };

    static const mp_arg_t allowed_args[] = 
    {
        {MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_baudrate, MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_phase, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_bits, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_sck, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_cs, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    (void)type;

    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t spi_id = mp_obj_get_int(args[ARG_id].u_obj);
    machine_hard_spi_obj_t *self = machine_hard_spi_find(spi_id);

    if (args[ARG_baudrate].u_int != -1) 
    {
        if (args[ARG_baudrate].u_int <= 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad baudrate"));
        }
        self->baudrate = args[ARG_baudrate].u_int;
    }

    if (args[ARG_polarity].u_int != -1) 
    {
        if (!IS_VALID_POLARITY(args[ARG_polarity].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad polarity"));
        }
        self->polarity = args[ARG_polarity].u_int;
    }

    if (args[ARG_phase].u_int != -1) 
    {
        if (!IS_VALID_PHASE(args[ARG_phase].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad phase"));
        }
        self->phase = args[ARG_phase].u_int;
    }

    if (args[ARG_bits].u_int != -1) 
    {
        if (!IS_VALID_BITS(args[ARG_bits].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad bits"));
        }
        self->bits = args[ARG_bits].u_int;
    }

    if (args[ARG_firstbit].u_int != -1) 
    {
        if (!IS_VALID_FIRSTBIT(args[ARG_firstbit].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad firstbit"));
        }
        self->firstbit = args[ARG_firstbit].u_int;
    }

    bool has_sck = args[ARG_sck].u_obj != MP_OBJ_NULL;
    bool has_mosi = args[ARG_mosi].u_obj != MP_OBJ_NULL;
    bool has_miso = args[ARG_miso].u_obj != MP_OBJ_NULL;
    bsp_io_port_pin_t new_sck = self->sck;
    bsp_io_port_pin_t new_mosi = self->mosi;
    bsp_io_port_pin_t new_miso = self->miso;

    if (self->spi_id >= 10) {
        if (!has_sck && !has_mosi && !has_miso) {
            new_sck = self->default_sck;
            new_mosi = self->default_mosi;
            new_miso = self->default_miso;
        } else {
            if (!has_sck || !has_mosi || !has_miso) {
                mp_raise_ValueError(MP_ERROR_TEXT("must specify sck, mosi and miso"));
            }

            const machine_pin_obj_t *sck_pin = machine_pin_find(args[ARG_sck].u_obj);
            const machine_pin_obj_t *mosi_pin = machine_pin_find(args[ARG_mosi].u_obj);
            const machine_pin_obj_t *miso_pin = machine_pin_find(args[ARG_miso].u_obj);

            new_sck = sck_pin->pin;
            new_mosi = mosi_pin->pin;
            new_miso = miso_pin->pin;
        }

        machine_hard_spi_validate_sci_pins(self->spi_id - 10, new_sck, new_mosi, new_miso);
    } else {
        if (has_sck) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_sck].u_obj);

            if (pin->pin != self->sck) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad SCK pin"));
            }
        }

        if (has_mosi) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_mosi].u_obj);

            if (pin->pin != self->mosi) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad MOSI pin"));
            }
        }

        if (has_miso) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_miso].u_obj);

            if (pin->pin != self->miso) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad MISO pin"));
            }
        }
    }

    const machine_pin_obj_t *new_cs = self->cs;

    if (args[ARG_cs].u_obj != MP_OBJ_NULL) {
        if (args[ARG_cs].u_obj == mp_const_none) {
            new_cs = NULL;
        } else {
            new_cs = machine_pin_find(args[ARG_cs].u_obj);
        }
    }

    if (spi_deinit(self->spi_id)) {
        machine_hard_spi_give_pins(self);
    }

    self->sck = new_sck;
    self->mosi = new_mosi;
    self->miso = new_miso;

    bool claimed_sci = false;
    if (self->spi_id >= 10 && !self->sci_taken) {
        if (!machine_sci_take(self->spi_id - 10, MACHINE_SCI_OWNER_SPI)) {
            mp_raise_OSError(MP_EBUSY);
        }

        self->sci_taken = true;
        claimed_sci = true;
    }

    if (!machine_hard_spi_take_pins(self, new_cs)) {
        if (claimed_sci) {
            machine_hard_spi_give_sci(self);
        }

        mp_raise_OSError(MP_EBUSY);
    }

    machine_hard_spi_configure_pins(self);

    int error = spi_init(self->spi_id, self->baudrate, self->polarity, self->phase, self->bits, self->firstbit);

    if (error != 0) {
        machine_hard_spi_give_pins(self);

        if (claimed_sci) {
            machine_hard_spi_give_sci(self);
        }

        mp_raise_OSError(error);
    }

    return MP_OBJ_FROM_PTR(self);
}

static void machine_hard_spi_init(mp_obj_base_t *self_in, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) 
{ 
    machine_hard_spi_obj_t *self = (machine_hard_spi_obj_t *)self_in;

    enum {
        ARG_baudrate,
        ARG_polarity,
        ARG_phase,
        ARG_bits,
        ARG_firstbit,
        ARG_sck,
        ARG_mosi,
        ARG_miso,
        ARG_cs,
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
        {MP_QSTR_cs, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_baudrate].u_int != -1) {
        if (args[ARG_baudrate].u_int <= 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad baudrate"));
        }
        self->baudrate = args[ARG_baudrate].u_int;
    }

    if (args[ARG_polarity].u_int != -1) {
        if (!IS_VALID_POLARITY(args[ARG_polarity].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad polarity"));
        }
        self->polarity = args[ARG_polarity].u_int;
    }

    if (args[ARG_phase].u_int != -1) {
        if (!IS_VALID_PHASE(args[ARG_phase].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad phase"));
        }
        self->phase = args[ARG_phase].u_int;
    }

    if (args[ARG_bits].u_int != -1) {
        if (!IS_VALID_BITS(args[ARG_bits].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad bits"));
        }
        self->bits = args[ARG_bits].u_int;
    }

    if (args[ARG_firstbit].u_int != -1) {
        if (!IS_VALID_FIRSTBIT(args[ARG_firstbit].u_int)) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad firstbit"));
        }
        self->firstbit = args[ARG_firstbit].u_int;
    }

    bool has_sck = args[ARG_sck].u_obj != MP_OBJ_NULL;
    bool has_mosi = args[ARG_mosi].u_obj != MP_OBJ_NULL;
    bool has_miso = args[ARG_miso].u_obj != MP_OBJ_NULL;
    bsp_io_port_pin_t new_sck = self->sck;
    bsp_io_port_pin_t new_mosi = self->mosi;
    bsp_io_port_pin_t new_miso = self->miso;

    if (self->spi_id >= 10) {
        if (has_sck || has_mosi || has_miso) {
            if (!has_sck || !has_mosi || !has_miso) {
                mp_raise_ValueError(MP_ERROR_TEXT("must specify sck, mosi and miso"));
            }

            const machine_pin_obj_t *sck_pin = machine_pin_find(args[ARG_sck].u_obj);
            const machine_pin_obj_t *mosi_pin = machine_pin_find(args[ARG_mosi].u_obj);
            const machine_pin_obj_t *miso_pin = machine_pin_find(args[ARG_miso].u_obj);

            new_sck = sck_pin->pin;
            new_mosi = mosi_pin->pin;
            new_miso = miso_pin->pin;
            machine_hard_spi_validate_sci_pins(self->spi_id - 10, new_sck, new_mosi, new_miso);
        }
    } else {
        if (has_sck) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_sck].u_obj);
            if (pin->pin != self->sck) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad SCK pin"));
            }
        }

        if (has_mosi) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_mosi].u_obj);
            if (pin->pin != self->mosi) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad MOSI pin"));
            }
        }

        if (has_miso) {
            const machine_pin_obj_t *pin = machine_pin_find(args[ARG_miso].u_obj);
            if (pin->pin != self->miso) {
                mp_raise_ValueError(MP_ERROR_TEXT("bad MISO pin"));
            }
        }
    }

    const machine_pin_obj_t *new_cs = self->cs;

    if (args[ARG_cs].u_obj != MP_OBJ_NULL) {
        if (args[ARG_cs].u_obj == mp_const_none) {
            new_cs = NULL;
        } else {
            new_cs = machine_pin_find(args[ARG_cs].u_obj);
        }
    }

    if (spi_deinit(self->spi_id)) {
        machine_hard_spi_give_pins(self);
    }

    self->sck = new_sck;
    self->mosi = new_mosi;
    self->miso = new_miso;

    bool claimed_sci = false;
    if (self->spi_id >= 10 && !self->sci_taken) {
        if (!machine_sci_take(self->spi_id - 10, MACHINE_SCI_OWNER_SPI)) {
            mp_raise_OSError(MP_EBUSY);
        }

        self->sci_taken = true;
        claimed_sci = true;
    }

    if (!machine_hard_spi_take_pins(self, new_cs)) {
        if (claimed_sci) {
            machine_hard_spi_give_sci(self);
        }

        mp_raise_OSError(MP_EBUSY);
    }

    machine_hard_spi_configure_pins(self);

    int error = spi_init(self->spi_id, self->baudrate, self->polarity, self->phase, self->bits, self->firstbit);

    if (error != 0) {
        machine_hard_spi_give_pins(self);

        if (claimed_sci) {
            machine_hard_spi_give_sci(self);
        }

        mp_raise_OSError(error);
    }
}

static void machine_hard_spi_deinit(mp_obj_base_t *self_in) 
{
    machine_hard_spi_obj_t *self = (machine_hard_spi_obj_t *)self_in;

    if (spi_deinit(self->spi_id)) {
        machine_hard_spi_give_pins(self);
        machine_hard_spi_give_sci(self);
    }
}

static void machine_hard_spi_transfer(mp_obj_base_t *self_in, size_t len, const uint8_t *src, uint8_t *dest) 
{
    machine_hard_spi_obj_t *self = (machine_hard_spi_obj_t *)self_in;

    if (len == 0) {
        return;
    }

    uint32_t transfer_ms = (uint32_t)(((uint64_t)len * self->bits * 1000U + self->baudrate - 1U) / self->baudrate);
    uint32_t timeout_ms = transfer_ms * 2U + 100U;

    /* 传输前拉低CS，选中从设备 */
    if (self->cs != NULL) {
        machine_pin_write(self->cs, false);
    }

    int error = spi_transfer(self->spi_id, len, src, dest, timeout_ms);

    /* 传输返回后拉高CS，释放从设备 */
    if (self->cs != NULL) {
        machine_pin_write(self->cs, true);
    }

    if (error != 0) {
        if (error == MP_ETIMEDOUT) {
            machine_hard_spi_give_pins(self);
            machine_hard_spi_give_sci(self);
        }
        mp_raise_OSError(error);
    }
}

static mp_obj_t machine_hard_spi_deinit_method(mp_obj_t self_in) 
{
    machine_hard_spi_deinit(MP_OBJ_TO_PTR(self_in));
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_init_method(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) 
{
    machine_hard_spi_init(MP_OBJ_TO_PTR(pos_args[0]), n_args - 1, pos_args + 1, kw_args);
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_write(mp_obj_t self_in, mp_obj_t buffer_in) 
{
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(buffer_in, &buffer, MP_BUFFER_READ);

    machine_hard_spi_transfer(MP_OBJ_TO_PTR(self_in), buffer.len, buffer.buf, NULL);
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_write_readinto(mp_obj_t self_in, mp_obj_t write_buffer_in, mp_obj_t read_buffer_in) 
{
    mp_buffer_info_t write_buffer;
    mp_buffer_info_t read_buffer;

    mp_get_buffer_raise(write_buffer_in, &write_buffer, MP_BUFFER_READ);
    mp_get_buffer_raise(read_buffer_in, &read_buffer, MP_BUFFER_WRITE);

    if (write_buffer.len != read_buffer.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffers must be the same length"));
    }

    machine_hard_spi_transfer(MP_OBJ_TO_PTR(self_in), write_buffer.len, write_buffer.buf, read_buffer.buf);
    return mp_const_none;
}

static mp_obj_t machine_hard_spi_read(size_t n_args, const mp_obj_t *args)
{
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(args[1]));
    memset(vstr.buf, n_args == 3 ? mp_obj_get_int(args[2]) : 0, vstr.len);
    machine_hard_spi_transfer(MP_OBJ_TO_PTR(args[0]), vstr.len, (uint8_t *)vstr.buf, (uint8_t *)vstr.buf);
    return mp_obj_new_bytes_from_vstr(&vstr);
}

static mp_obj_t machine_hard_spi_readinto(size_t n_args, const mp_obj_t *args)
{
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(args[1], &buffer, MP_BUFFER_WRITE);
    memset(buffer.buf, n_args == 3 ? mp_obj_get_int(args[2]) : 0, buffer.len);
    machine_hard_spi_transfer(MP_OBJ_TO_PTR(args[0]), buffer.len, buffer.buf, buffer.buf);
    return mp_const_none;
}

void machine_spi_deinit_all(void) 
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_spi_obj); ++index) {
        machine_hard_spi_obj_t *self = &machine_hard_spi_obj[index];
        if (spi_deinit(self->spi_id)) {
            machine_hard_spi_give_pins(self);
            machine_hard_spi_give_sci(self);
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
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_hard_spi_read_obj, 2, 3, machine_hard_spi_read);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_hard_spi_readinto_obj, 2, 3, machine_hard_spi_readinto);
static MP_DEFINE_CONST_FUN_OBJ_2(machine_hard_spi_write_obj, machine_hard_spi_write);
static MP_DEFINE_CONST_FUN_OBJ_3(machine_hard_spi_write_readinto_obj, machine_hard_spi_write_readinto);

static const mp_rom_map_elem_t machine_hard_spi_locals_dict_table[] = 
{
    {MP_ROM_QSTR(MP_QSTR_CONTROLLER), MP_ROM_INT(0)},
    {MP_ROM_QSTR(MP_QSTR_LSB), MP_ROM_INT(MICROPY_PY_MACHINE_SPI_LSB)},
    {MP_ROM_QSTR(MP_QSTR_MSB), MP_ROM_INT(MICROPY_PY_MACHINE_SPI_MSB)},
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_hard_spi_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_hard_spi_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&machine_hard_spi_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&machine_hard_spi_readinto_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&machine_hard_spi_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_write_readinto), MP_ROM_PTR(&machine_hard_spi_write_readinto_obj)},
};

static MP_DEFINE_CONST_DICT(machine_hard_spi_locals_dict, machine_hard_spi_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    machine_spi_type,
    MP_QSTR_SPI,
    MP_TYPE_FLAG_NONE,
    make_new, machine_hard_spi_make_new,
    print, machine_hard_spi_print,
    protocol, &machine_hard_spi_p,
    locals_dict, &machine_hard_spi_locals_dict
    );
