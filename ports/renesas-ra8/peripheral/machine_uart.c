#include <stdbool.h>
#include <stdint.h>

#include "extmod/modmachine.h"
#include "hal_data.h"
#include "pin.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "sci.h"
#include "uart.h"

#if MICROPY_PY_MACHINE_UART

#define MACHINE_UART_TX_PIN_OPTIONS  IOPORT_CFG_DRIVE_HIGH

typedef struct _machine_hard_uart_obj_t {
    mp_obj_base_t base;
    uart_t *uart;
    bsp_io_port_pin_t tx;
    bsp_io_port_pin_t rx;
    ioport_peripheral_t peripheral;
    uint8_t id;
    uint8_t channel;
    bool initialized;
    bool sci_taken;
    uint32_t baudrate;
    uint32_t timeout;
    uint32_t timeout_char;
} machine_hard_uart_obj_t;

typedef struct {
    uint32_t baudrate;
    uint32_t timeout;
    uint32_t timeout_char;
} machine_hard_uart_config_t;

static uart_t machine_hard_uart_instances[] = {
    {.instance = &g_sci_uart1},
    {.instance = &g_sci_uart2},
    {.instance = &g_sci_uart4},
    {.instance = &g_sci_uart5},
    {.instance = &g_sci_uart6},
    {.instance = &g_sci_uart8},
};

static machine_hard_uart_obj_t machine_hard_uart_obj[] = {
#if defined(MICROPY_HW_SCI1_TXD) && defined(MICROPY_HW_SCI1_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[0],
        .tx = MICROPY_HW_SCI1_TXD,
        .rx = MICROPY_HW_SCI1_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI1_3_5_7_9,
        .id = 1,
        .channel = 1,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
#if defined(MICROPY_HW_SCI2_TXD) && defined(MICROPY_HW_SCI2_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[1],
        .tx = MICROPY_HW_SCI2_TXD,
        .rx = MICROPY_HW_SCI2_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .id = 2,
        .channel = 2,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
#if defined(MICROPY_HW_SCI4_TXD) && defined(MICROPY_HW_SCI4_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[2],
        .tx = MICROPY_HW_SCI4_TXD,
        .rx = MICROPY_HW_SCI4_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .id = 4,
        .channel = 4,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
#if defined(MICROPY_HW_SCI5_TXD) && defined(MICROPY_HW_SCI5_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[3],
        .tx = MICROPY_HW_SCI5_TXD,
        .rx = MICROPY_HW_SCI5_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI1_3_5_7_9,
        .id = 5,
        .channel = 5,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
#if defined(MICROPY_HW_SCI6_TXD) && defined(MICROPY_HW_SCI6_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[4],
        .tx = MICROPY_HW_SCI6_TXD,
        .rx = MICROPY_HW_SCI6_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .id = 6,
        .channel = 6,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
#if defined(MICROPY_HW_SCI8_TXD) && defined(MICROPY_HW_SCI8_RXD)
    {
        .base = {&machine_uart_type},
        .uart = &machine_hard_uart_instances[5],
        .tx = MICROPY_HW_SCI8_TXD,
        .rx = MICROPY_HW_SCI8_RXD,
        .peripheral = IOPORT_PERIPHERAL_SCI0_2_4_6_8,
        .id = 8,
        .channel = 8,
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    },
#endif
};

static machine_hard_uart_obj_t *machine_hard_uart_find(mp_int_t id)
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_uart_obj); ++index) {
        if (machine_hard_uart_obj[index].id == id) {
            return &machine_hard_uart_obj[index];
        }
    }

    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("UART(%d) does not exist"), id);
    return NULL;
}

static int machine_hard_uart_fsp_error(fsp_err_t error)
{
    switch (error) {
        case FSP_SUCCESS:
            return 0;
        case FSP_ERR_IN_USE:
        case FSP_ERR_ALREADY_OPEN:
            return MP_EBUSY;
        case FSP_ERR_TIMEOUT:
            return MP_ETIMEDOUT;
        case FSP_ERR_NOT_OPEN:
            return MP_ENODEV;
        case FSP_ERR_ASSERTION:
        case FSP_ERR_INVALID_ARGUMENT:
        case FSP_ERR_INVALID_SIZE:
            return MP_EINVAL;
        case FSP_ERR_ABORTED:
            return MP_ECANCELED;
        default:
            return MP_EIO;
    }
}

static void machine_hard_uart_validate_pins(machine_hard_uart_obj_t *self)
{
    const machine_pin_af_obj_t *tx_af = machine_pin_find_af(
        self->tx, MACHINE_PIN_AF_PERIPHERAL_SCI, self->channel, MACHINE_PIN_AF_UART_TXD);
    if (tx_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad TX pin"));
    }

    const machine_pin_af_obj_t *rx_af = machine_pin_find_af(
        self->rx, MACHINE_PIN_AF_PERIPHERAL_SCI, self->channel, MACHINE_PIN_AF_UART_RXD);
    if (rx_af == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("bad RX pin"));
    }

    if (tx_af->group != rx_af->group) {
        mp_raise_ValueError(MP_ERROR_TEXT("TX and RX must use the same UART pin group"));
    }
}

static bool machine_hard_uart_take_pins(machine_hard_uart_obj_t *self)
{
    if (!machine_pin_take(self->tx)) {
        return false;
    }

    if (!machine_pin_take(self->rx)) {
        machine_pin_give(self->tx);
        return false;
    }

    return true;
}

static bool machine_hard_uart_give_pins(machine_hard_uart_obj_t *self)
{
    bool success = true;
    success = machine_pin_give(self->rx) && success;
    success = machine_pin_give(self->tx) && success;
    return success;
}

static void machine_hard_uart_give_sci(machine_hard_uart_obj_t *self)
{
    if (self->sci_taken) {
        machine_sci_give(self->channel, MACHINE_SCI_OWNER_UART);
        self->sci_taken = false;
    }
}

static void machine_hard_uart_stop(machine_hard_uart_obj_t *self)
{
    if (!self->initialized) {
        return;
    }

    fsp_err_t error = (fsp_err_t)UART_DeInit(self->uart);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    self->initialized = false;
    bool pins_released = machine_hard_uart_give_pins(self);
    machine_hard_uart_give_sci(self);
    if (!pins_released) {
        mp_raise_OSError(MP_EIO);
    }
}

static void machine_hard_uart_start(machine_hard_uart_obj_t *self)
{
    machine_hard_uart_validate_pins(self);

    if (!machine_hard_uart_take_pins(self)) {
        mp_raise_OSError(MP_EBUSY);
    }

    if (!machine_sci_take(self->channel, MACHINE_SCI_OWNER_UART)) {
        machine_hard_uart_give_pins(self);
        mp_raise_OSError(MP_EBUSY);
    }
    self->sci_taken = true;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        machine_pin_configure_alt(self->tx, self->peripheral, MACHINE_UART_TX_PIN_OPTIONS);
        machine_pin_configure_alt(self->rx, self->peripheral, 0U);
        nlr_pop();
    } else {
        machine_hard_uart_give_sci(self);
        machine_hard_uart_give_pins(self);
        nlr_jump(nlr.ret_val);
    }

    fsp_err_t error = (fsp_err_t)UART_Init(self->uart);
    if (error == FSP_SUCCESS) {
        error = (fsp_err_t)UART_SetBaudrate(self->uart, self->baudrate);
        if (error != FSP_SUCCESS) {
            (void)UART_DeInit(self->uart);
        }
    }
    if (error != FSP_SUCCESS) {
        machine_hard_uart_give_sci(self);
        machine_hard_uart_give_pins(self);
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    self->initialized = true;
}

static void machine_hard_uart_change_baudrate(machine_hard_uart_obj_t *self, uint32_t baudrate)
{
    fsp_err_t error = (fsp_err_t)UART_SetBaudrate(self->uart, baudrate);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    self->baudrate = baudrate;
}

static void machine_hard_uart_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    (void)kind;
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(
        print,
        "UART(%u, initialized=%u, baudrate=%u, timeout=%u, timeout_char=%u, tx=P%X%02u, rx=P%X%02u)",
        self->id,
        self->initialized,
        self->baudrate,
        self->timeout,
        self->timeout_char,
        ((uint32_t)self->tx >> 8) & 0xff,
        (uint32_t)self->tx & 0xff,
        ((uint32_t)self->rx >> 8) & 0xff,
        (uint32_t)self->rx & 0xff);
}

static void machine_hard_uart_parse_config(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args, machine_hard_uart_config_t *config)
{
    enum { ARG_baudrate, ARG_timeout, ARG_timeout_char };
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_baudrate, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_timeout_char, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_baudrate].u_obj != MP_OBJ_NULL) {
        mp_int_t value = mp_obj_get_int(args[ARG_baudrate].u_obj);
        if (value <= 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("bad baudrate"));
        }
        config->baudrate = (uint32_t)value;
    }

    if (args[ARG_timeout].u_obj != MP_OBJ_NULL) {
        mp_int_t value = mp_obj_get_int(args[ARG_timeout].u_obj);
        if (value < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("timeout must not be negative"));
        }
        config->timeout = (uint32_t)value;
    }

    if (args[ARG_timeout_char].u_obj != MP_OBJ_NULL) {
        mp_int_t value = mp_obj_get_int(args[ARG_timeout_char].u_obj);
        if (value < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("timeout_char must not be negative"));
        }
        config->timeout_char = (uint32_t)value;
    }

    // timeout_char 至少要能覆盖一个完整 UART 字符的传输时间。
    uint32_t min_timeout_char = 13000U / config->baudrate + 1U;
    if (config->timeout_char < min_timeout_char) {
        config->timeout_char = min_timeout_char;
    }
}

static mp_obj_t machine_hard_uart_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);
    (void)type;

    mp_int_t id = mp_obj_get_int(all_args[0]);
    machine_hard_uart_obj_t *self = machine_hard_uart_find(id);

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    machine_hard_uart_config_t config = {
        .baudrate = UART_DEFAULT_BAUDRATE_HZ,
        .timeout = UART_DEFAULT_TIMEOUT_MS,
        .timeout_char = UART_DEFAULT_TIMEOUT_CHAR_MS,
    };
    machine_hard_uart_parse_config(n_args - 1, all_args + 1, &kw_args, &config);

    if (self->initialized) {
        machine_hard_uart_change_baudrate(self, config.baudrate);
    } else {
        self->baudrate = config.baudrate;
        machine_hard_uart_start(self);
    }
    self->timeout = config.timeout;
    self->timeout_char = config.timeout_char;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t machine_hard_uart_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    machine_hard_uart_config_t config = {
        .baudrate = self->baudrate,
        .timeout = self->timeout,
        .timeout_char = self->timeout_char,
    };
    machine_hard_uart_parse_config(n_args - 1, args + 1, kw_args, &config);

    if (self->initialized) {
        machine_hard_uart_change_baudrate(self, config.baudrate);
    } else {
        self->baudrate = config.baudrate;
        machine_hard_uart_start(self);
    }
    self->timeout = config.timeout;
    self->timeout_char = config.timeout_char;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(machine_hard_uart_init_obj, 1, machine_hard_uart_init);

static mp_obj_t machine_hard_uart_deinit(mp_obj_t self_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    machine_hard_uart_stop(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_uart_deinit_obj, machine_hard_uart_deinit);

static mp_obj_t machine_hard_uart_any(mp_obj_t self_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    uint32_t available = 0U;
    fsp_err_t error = (fsp_err_t)UART_Any(self->uart, &available);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    return mp_obj_new_int_from_uint(available);
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_uart_any_obj, machine_hard_uart_any);

static mp_obj_t machine_hard_uart_read(size_t n_args, const mp_obj_t *args)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    if (n_args == 1) {
        vstr_t vstr;
        vstr_init(&vstr, 16);
        uint32_t wait_ms = self->timeout;

        for (;;) {
            uint8_t data;
            uint32_t read_length = 0U;
            fsp_err_t error = (fsp_err_t)UART_Read(
                self->uart, &data, 1U, &read_length, wait_ms, self->timeout_char);

            if (error == FSP_ERR_TIMEOUT) {
                if (vstr.len == 0U) {
                    vstr_clear(&vstr);
                    return mp_const_none;
                }
                break;
            }

            if (error != FSP_SUCCESS) {
                vstr_clear(&vstr);
                mp_raise_OSError(machine_hard_uart_fsp_error(error));
            }

            char *p = vstr_add_len(&vstr, 1U);
            *p = (char)data;
            wait_ms = self->timeout_char;
        }

        return mp_obj_new_bytes_from_vstr(&vstr);
    }

    uint32_t length;
    {
        mp_int_t requested = mp_obj_get_int(args[1]);
        if (requested < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("length must not be negative"));
        }
        if (requested == 0) {
            return mp_obj_new_bytes(NULL, 0);
        }
        if ((uint64_t)requested > UINT32_MAX) {
            mp_raise_ValueError(MP_ERROR_TEXT("length too large"));
        }
        length = (uint32_t)requested;
    }

    vstr_t vstr;
    vstr_init_len(&vstr, (size_t)length);
    uint32_t read_length = 0U;
    fsp_err_t error = (fsp_err_t)UART_Read(
        self->uart,
        (uint8_t *)vstr.buf,
        length,
        &read_length,
        self->timeout,
        self->timeout_char);
    if (error == FSP_ERR_TIMEOUT) {
        vstr_clear(&vstr);
        return mp_const_none;
    }
    if (error != FSP_SUCCESS) {
        vstr_clear(&vstr);
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    vstr.len = read_length;
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_hard_uart_read_obj, 1, 2, machine_hard_uart_read);

static mp_obj_t machine_hard_uart_readline(mp_obj_t self_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    vstr_t vstr;
    vstr_init(&vstr, 16);

    for (;;) {
        uint8_t data;
        uint32_t read_length = 0U;
        uint32_t timeout = self->timeout_char;
        if (vstr.len == 0U) {
            timeout = self->timeout;
        }
        fsp_err_t error = (fsp_err_t)UART_Read(
            self->uart, &data, 1U, &read_length, timeout, self->timeout_char);
        if (error == FSP_ERR_TIMEOUT) {
            if (vstr.len == 0U) {
                vstr_clear(&vstr);
                return mp_const_none;
            }
            break;
        }
        if (error != FSP_SUCCESS) {
            vstr_clear(&vstr);
            mp_raise_OSError(machine_hard_uart_fsp_error(error));
        }

        char *p = vstr_add_len(&vstr, 1U);
        *p = (char)data;
        if (data == '\n') {
            break;
        }
    }

    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_uart_readline_obj, machine_hard_uart_readline);

static mp_obj_t machine_hard_uart_readinto(mp_obj_t self_in, mp_obj_t buffer_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buffer_in, &bufinfo, MP_BUFFER_WRITE);
    if (bufinfo.len == 0) {
        return MP_OBJ_NEW_SMALL_INT(0);
    }
    if (bufinfo.len > UINT32_MAX) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too large"));
    }

    uint32_t read_length = 0U;
    fsp_err_t error = (fsp_err_t)UART_Read(
        self->uart,
        bufinfo.buf,
        (uint32_t)bufinfo.len,
        &read_length,
        self->timeout,
        self->timeout_char);
    if (error == FSP_ERR_TIMEOUT) {
        return mp_const_none;
    }
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    return mp_obj_new_int_from_uint(read_length);
}
static MP_DEFINE_CONST_FUN_OBJ_2(machine_hard_uart_readinto_obj, machine_hard_uart_readinto);

static mp_obj_t machine_hard_uart_write(mp_obj_t self_in, mp_obj_t data_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len == 0) {
        return MP_OBJ_NEW_SMALL_INT(0);
    }
    if (bufinfo.len > UINT32_MAX) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too large"));
    }

    fsp_err_t error = (fsp_err_t)UART_Write(self->uart, bufinfo.buf, (uint32_t)bufinfo.len);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    return mp_obj_new_int_from_uint(bufinfo.len);
}
static MP_DEFINE_CONST_FUN_OBJ_2(machine_hard_uart_write_obj, machine_hard_uart_write);

static mp_obj_t machine_hard_uart_flush(mp_obj_t self_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    fsp_err_t error = (fsp_err_t)UART_Flush(self->uart);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_uart_flush_obj, machine_hard_uart_flush);

static mp_obj_t machine_hard_uart_txdone(mp_obj_t self_in)
{
    machine_hard_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->initialized) {
        mp_raise_OSError(MP_ENODEV);
    }

    bool done = false;
    fsp_err_t error = (fsp_err_t)UART_TxDone(self->uart, &done);
    if (error != FSP_SUCCESS) {
        mp_raise_OSError(machine_hard_uart_fsp_error(error));
    }

    return mp_obj_new_bool(done);
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_hard_uart_txdone_obj, machine_hard_uart_txdone);

static const mp_rom_map_elem_t machine_hard_uart_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_hard_uart_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_hard_uart_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_any), MP_ROM_PTR(&machine_hard_uart_any_obj)},
    {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&machine_hard_uart_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&machine_hard_uart_readline_obj)},
    {MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&machine_hard_uart_readinto_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&machine_hard_uart_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&machine_hard_uart_flush_obj)},
    {MP_ROM_QSTR(MP_QSTR_txdone), MP_ROM_PTR(&machine_hard_uart_txdone_obj)},
};
static MP_DEFINE_CONST_DICT(machine_hard_uart_locals_dict, machine_hard_uart_locals_dict_table);

void machine_uart_deinit_all(void)
{
    for (size_t index = 0; index < MP_ARRAY_SIZE(machine_hard_uart_obj); ++index) {
        machine_hard_uart_obj_t *self = &machine_hard_uart_obj[index];
        if (!self->initialized) {
            continue;
        }

        if ((fsp_err_t)UART_DeInit(self->uart) == FSP_SUCCESS) {
            self->initialized = false;
            machine_hard_uart_give_pins(self);
            machine_hard_uart_give_sci(self);
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    machine_uart_type,
    MP_QSTR_UART,
    MP_TYPE_FLAG_NONE,
    make_new, machine_hard_uart_make_new,
    print, machine_hard_uart_print,
    locals_dict, &machine_hard_uart_locals_dict
);

#endif // MICROPY_PY_MACHINE_UART
