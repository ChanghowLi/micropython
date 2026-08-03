#include "py/mperrno.h"
#include "py/runtime.h"

#include "pin.h"

/**
 * @brief machine.Pin 支持的 GPIO 输出驱动能力。
 */
enum {
    MACHINE_PIN_DRIVE_0 = 0,
    MACHINE_PIN_DRIVE_1,
    MACHINE_PIN_DRIVE_2,
    MACHINE_PIN_DRIVE_3,
};

/**
 * @brief machine.Pin 支持的 GPIO 工作模式。
 */
enum {
    MACHINE_PIN_MODE_IN = 0,
    MACHINE_PIN_MODE_OUT,
    MACHINE_PIN_MODE_OPEN_DRAIN,
    MACHINE_PIN_MODE_ANALOG,
    MACHINE_PIN_MODE_ALT,
    MACHINE_PIN_MODE_ALT_OPEN_DRAIN,
};

/**
 * @brief machine.Pin 支持的 GPIO 上下拉配置。
 */
enum {
    MACHINE_PIN_PULL_NONE = 0,
    MACHINE_PIN_PULL_UP,
};

/**
 * @brief       将用户传入的引脚标识转换为 Pin 对象。支持已有的 Pin 对象和 pins.csv 中注册的安全引脚名称。
 * @param       user_obj 用户传入的 Pin 对象或引脚名称。
 * @return      对应的 Pin 对象。
 * @exception   ValueError 引脚名称无效或尚未注册。
 */
const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj)
{
    if (mp_obj_is_type(user_obj, &machine_pin_type)) {
        return MP_OBJ_TO_PTR(user_obj);
    }

    if (mp_obj_is_str(user_obj)) {
        qstr name = mp_obj_str_get_qstr(user_obj);

        for (size_t i = 0; i < machine_pin_generated_pins_count; ++i) {
            const machine_pin_obj_t *pin = machine_pin_generated_pins[i];

            if (pin->name == name) {
                return pin;
            }
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("invalid pin"));
}

/**
 * @brief       根据 MicroPython 模式配置 GPIO 引脚。
 *
 * @param       pin   要配置的 Pin 对象。
 * @param       mode  输入或输出模式。
 * @param       pull  无上下拉或内部上拉配置。
 * @param       value 初始输出值，未提供时为 MP_OBJ_NULL。
 * @param       drive 输出驱动能力，未提供时为 MP_OBJ_NULL。
 * @exception   ValueError mode、pull 或 drive 不是当前支持的配置。
 * @exception   OSError FSP 配置引脚失败。
 */
static void machine_pin_configure(const machine_pin_obj_t *pin, mp_int_t mode, mp_obj_t pull, mp_obj_t value, mp_obj_t drive, mp_obj_t alt)
{
    uint32_t cfg;
    mp_int_t alt_value = -1;

    if (alt != MP_OBJ_NULL && alt != mp_const_none) {
        alt_value = mp_obj_get_int(alt);
    }

    if (mode == MACHINE_PIN_MODE_ALT ||
        mode == MACHINE_PIN_MODE_ALT_OPEN_DRAIN) {
        if (alt_value < 1 || alt_value > 31) {
            mp_raise_ValueError(
                MP_ERROR_TEXT("ALT mode requires alt from 1 to 31"));
        }

        uint32_t alt_bit = (uint32_t) 1U << (uint32_t) alt_value;
        if ((pin->alt_mask & alt_bit) == 0U) {
            mp_raise_ValueError(
                MP_ERROR_TEXT("invalid alternate function for pin"));
        }
    } else if (alt_value != -1) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("alt is only valid for ALT mode"));
    }

    switch (mode) {
        case MACHINE_PIN_MODE_ANALOG:
            if (value != MP_OBJ_NULL && value != mp_const_none) {
                mp_raise_ValueError(
                    MP_ERROR_TEXT("value is not valid for analog mode"));
            }

            cfg = IOPORT_CFG_ANALOG_ENABLE;
            break;

        case MACHINE_PIN_MODE_IN:
            if (value != MP_OBJ_NULL && value != mp_const_none) {
                mp_raise_ValueError(MP_ERROR_TEXT("value is only valid for output mode"));
            }
            cfg = IOPORT_CFG_PORT_DIRECTION_INPUT;
            break;

        case MACHINE_PIN_MODE_ALT:
        case MACHINE_PIN_MODE_ALT_OPEN_DRAIN:
            if (value != MP_OBJ_NULL && value != mp_const_none) {
                mp_raise_ValueError(
                    MP_ERROR_TEXT("value is not valid for ALT mode"));
            }

            cfg = (((uint32_t) alt_value << R_PFS_PORT_PIN_PmnPFS_PSEL_Pos) & R_PFS_PORT_PIN_PmnPFS_PSEL_Msk) | IOPORT_CFG_PERIPHERAL_PIN;

            if (mode == MACHINE_PIN_MODE_ALT_OPEN_DRAIN) {
                cfg |= IOPORT_CFG_NMOS_ENABLE;
            }
            break;

        case MACHINE_PIN_MODE_OPEN_DRAIN:
        case MACHINE_PIN_MODE_OUT:
            cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT;

            if (mode == MACHINE_PIN_MODE_OPEN_DRAIN) {
                cfg |= IOPORT_CFG_NMOS_ENABLE;
            }
            if (value != MP_OBJ_NULL && value != mp_const_none) {
                if (mp_obj_is_true(value)) {
                    cfg |= IOPORT_CFG_PORT_OUTPUT_HIGH;
                } else {
                    cfg |= IOPORT_CFG_PORT_OUTPUT_LOW;
                }
            }
            break;
        default:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid pin mode"));
    }

    if (pull != MP_OBJ_NULL && pull != mp_const_none) {
        switch (mp_obj_get_int(pull)) {
            case MACHINE_PIN_PULL_NONE:
                break;
            case MACHINE_PIN_PULL_UP:
                if (mode == MACHINE_PIN_MODE_ANALOG) {
                    mp_raise_ValueError(
                        MP_ERROR_TEXT("pull is not valid for analog mode"));
                }

                cfg |= IOPORT_CFG_PULLUP_ENABLE;
                break;
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("invalid pin pull"));
        }
    }

    if (drive != MP_OBJ_NULL && drive != mp_const_none) {
        if (mode != MACHINE_PIN_MODE_ALT &&
            mode != MACHINE_PIN_MODE_ALT_OPEN_DRAIN &&
            mode != MACHINE_PIN_MODE_OPEN_DRAIN &&
            mode != MACHINE_PIN_MODE_OUT) {
            mp_raise_ValueError(
                MP_ERROR_TEXT("drive is only valid for output or ALT mode"));
        }

        switch (mp_obj_get_int(drive)) {
            case MACHINE_PIN_DRIVE_0:
                break;
            case MACHINE_PIN_DRIVE_1:
                cfg |= IOPORT_CFG_DRIVE_MID;
                break;
            case MACHINE_PIN_DRIVE_2:
                cfg |= IOPORT_CFG_DRIVE_HS_HIGH;
                break;
            case MACHINE_PIN_DRIVE_3:
                cfg |= IOPORT_CFG_DRIVE_HIGH;
                break;
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("invalid pin drive"));
        }
    }

    fsp_err_t err = R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, cfg);
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
}

/**
 * @brief       创建 machine.Pin 对象。如果提供 mode 参数，则同时配置引脚方向、上下拉和驱动能力。value 仅用于指定输出模式的初始电平。
 * @param       type   正在构造的 MicroPython 类型。
 * @param       n_args 位置参数数量。
 * @param       n_kw   关键字参数数量。
 * @param       args   位置参数和关键字参数值。
 * @return      与引脚标识对应的 MicroPython Pin 对象。
 */
static mp_obj_t machine_pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    (void)type;

    enum {
        ARG_id,
        ARG_mode,
        ARG_pull,
        ARG_value,
        ARG_drive,
        ARG_alt,
    };
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_pull, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_drive, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_alt, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t parsed_args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, parsed_args);

    const machine_pin_obj_t *pin = machine_pin_find(parsed_args[ARG_id].u_obj);

    if (parsed_args[ARG_mode].u_obj != MP_OBJ_NULL) {
        mp_int_t mode = mp_obj_get_int(parsed_args[ARG_mode].u_obj);
        machine_pin_configure(pin, mode, parsed_args[ARG_pull].u_obj, parsed_args[ARG_value].u_obj, parsed_args[ARG_drive].u_obj, parsed_args[ARG_alt].u_obj);
    }
    else if (
        (parsed_args[ARG_pull].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_pull].u_obj != mp_const_none) ||
        (parsed_args[ARG_value].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_value].u_obj != mp_const_none) ||
        (parsed_args[ARG_drive].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_drive].u_obj != mp_const_none) ||
        (parsed_args[ARG_alt].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_alt].u_obj != mp_const_none)) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("pull, value, drive and alt require mode"));
    }

    return MP_OBJ_FROM_PTR(pin);
}

/**
 * @brief       使用指定参数重新配置 Pin 对象。
 * @param       n_args   位置参数数量，包括 self。
 * @param       pos_args 位置参数，其中 pos_args[0] 是 Pin 对象。
 * @param       kw_args  关键字参数。
 * @return      None。
 * @exception   V alueError 未提供 mode 但提供了其它配置参数。
 */
static mp_obj_t machine_pin_init(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum {
        ARG_mode,
        ARG_pull,
        ARG_value,
        ARG_drive,
        ARG_alt,
    };
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_pull, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_drive, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_alt, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t parsed_args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(
        n_args - 1,
        pos_args + 1,
        kw_args,
        MP_ARRAY_SIZE(allowed_args),
        allowed_args,
        parsed_args
        );

    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    if (parsed_args[ARG_mode].u_obj != MP_OBJ_NULL) {
        mp_int_t mode = mp_obj_get_int(parsed_args[ARG_mode].u_obj);

        machine_pin_configure(self, mode, parsed_args[ARG_pull].u_obj, parsed_args[ARG_value].u_obj, parsed_args[ARG_drive].u_obj, parsed_args[ARG_alt].u_obj);
    }
    else if (
        (parsed_args[ARG_pull].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_pull].u_obj != mp_const_none) ||
        (parsed_args[ARG_value].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_value].u_obj != mp_const_none) ||
        (parsed_args[ARG_drive].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_drive].u_obj != mp_const_none) ||
        (parsed_args[ARG_alt].u_obj != MP_OBJ_NULL &&
         parsed_args[ARG_alt].u_obj != mp_const_none)) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("pull, value, drive and alt require mode"));
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(
    machine_pin_init_obj,
    1,
    machine_pin_init
    );

/**
 * @brief Read or set the pin direction mode.
 *
 * @param n_args Number of arguments including self.
 * @param args Arguments containing self and an optional mode.
 * @return Current mode when reading, otherwise None.
 * @exception ValueError The requested mode is not supported.
 * @exception OSError FSP failed to change the pin direction.
 */
static mp_obj_t machine_pin_mode(size_t n_args, const mp_obj_t *args)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = (uint32_t) self->pin >> 8;
    uint32_t bit = (uint32_t) self->pin & 0xffU;

    if (n_args == 1) {
        uint32_t analog =
            R_PFS->PORT[port].PIN[bit].PmnPFS_b.ASEL;
        uint32_t peripheral =
            R_PFS->PORT[port].PIN[bit].PmnPFS_b.PMR;
        uint32_t direction =
            R_PFS->PORT[port].PIN[bit].PmnPFS_b.PDR;
        uint32_t open_drain =
            R_PFS->PORT[port].PIN[bit].PmnPFS_b.NCODR;

        if (analog != 0U) {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_MODE_ANALOG);
        }

        if (peripheral != 0U) {
            return MP_OBJ_NEW_SMALL_INT(
                open_drain == 0U
                    ? MACHINE_PIN_MODE_ALT
                    : MACHINE_PIN_MODE_ALT_OPEN_DRAIN
            );
        }

        if (direction == 0U) {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_MODE_IN);
        }

        return MP_OBJ_NEW_SMALL_INT(
            open_drain == 0U
                ? MACHINE_PIN_MODE_OUT
                : MACHINE_PIN_MODE_OPEN_DRAIN
        );
    }

    mp_int_t mode = mp_obj_get_int(args[1]);

    if (mode == MACHINE_PIN_MODE_ALT ||
        mode == MACHINE_PIN_MODE_ALT_OPEN_DRAIN) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("use init(..., alt=...) for ALT mode"));
    }

    if (mode != MACHINE_PIN_MODE_ANALOG &&
        mode != MACHINE_PIN_MODE_IN &&
        mode != MACHINE_PIN_MODE_OPEN_DRAIN &&
        mode != MACHINE_PIN_MODE_OUT) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pin mode"));
    }

    uint32_t cfg = R_PFS->PORT[port].PIN[bit].PmnPFS;

    cfg &= ~((uint32_t) (
        IOPORT_CFG_ANALOG_ENABLE |
        IOPORT_CFG_NMOS_ENABLE |
        IOPORT_CFG_PERIPHERAL_PIN |
        IOPORT_CFG_PMOS_ENABLE |
        IOPORT_CFG_PORT_DIRECTION_OUTPUT |
        R_PFS_PORT_PIN_PmnPFS_PSEL_Msk
    ));

    switch (mode) {
        case MACHINE_PIN_MODE_ANALOG:
            cfg |= IOPORT_CFG_ANALOG_ENABLE;
            break;

        case MACHINE_PIN_MODE_IN:
            break;

        case MACHINE_PIN_MODE_OPEN_DRAIN:
            cfg |= IOPORT_CFG_PORT_DIRECTION_OUTPUT;
            cfg |= IOPORT_CFG_NMOS_ENABLE;
            break;

        case MACHINE_PIN_MODE_OUT:
            cfg |= IOPORT_CFG_PORT_DIRECTION_OUTPUT;
            break;
    }

    fsp_err_t err = R_IOPORT_PinCfg(
        g_ioport.p_ctrl,
        self->pin,
        cfg
        );
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    machine_pin_mode_obj,
    1,
    2,
    machine_pin_mode
    );

/**
 * @brief Read or set the pin pull-up configuration.
 *
 * @param n_args Number of arguments including self.
 * @param args Arguments containing self and an optional pull setting.
 * @return Current pull setting when reading, otherwise None.
 * @exception ValueError The requested pull setting is not supported.
 * @exception OSError FSP failed to configure the pin.
 */
static mp_obj_t machine_pin_pull(size_t n_args, const mp_obj_t *args)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = (uint32_t) self->pin >> 8;
    uint32_t bit = (uint32_t) self->pin & 0xffU;

    uint32_t cfg = R_PFS->PORT[port].PIN[bit].PmnPFS;

    if (n_args == 1) {
        return MP_OBJ_NEW_SMALL_INT(
            (cfg & IOPORT_CFG_PULLUP_ENABLE) != 0U
                ? MACHINE_PIN_PULL_UP
                : MACHINE_PIN_PULL_NONE
            );
    }

    if (args[1] == mp_const_none) {
        cfg &= ~((uint32_t) IOPORT_CFG_PULLUP_ENABLE);
    } else {
        mp_int_t pull = mp_obj_get_int(args[1]);

        switch (pull) {
            case MACHINE_PIN_PULL_NONE:
                cfg &= ~((uint32_t) IOPORT_CFG_PULLUP_ENABLE);
                break;

            case MACHINE_PIN_PULL_UP:
                cfg |= IOPORT_CFG_PULLUP_ENABLE;
                break;

            default:
                mp_raise_ValueError(MP_ERROR_TEXT("invalid pin pull"));
        }
    }

    fsp_err_t err = R_IOPORT_PinCfg(
        g_ioport.p_ctrl,
        self->pin,
        cfg
        );
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    machine_pin_pull_obj,
    1,
    2,
    machine_pin_pull
    );

/**
 * @brief Read or set the pin output drive capability.
 *
 * @param n_args Number of arguments including self.
 * @param args Arguments containing self and an optional drive setting.
 * @return Current drive setting when reading, otherwise None.
 * @exception ValueError The pin is not an output or drive is invalid.
 * @exception OSError FSP failed to configure the pin.
 */
static mp_obj_t machine_pin_drive(size_t n_args, const mp_obj_t *args)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = (uint32_t) self->pin >> 8;
    uint32_t bit = (uint32_t) self->pin & 0xffU;

    if (n_args == 1) {
        uint32_t drive =
            R_PFS->PORT[port].PIN[bit].PmnPFS_b.DSCR;

        return MP_OBJ_NEW_SMALL_INT(drive);
    }

    if (R_PFS->PORT[port].PIN[bit].PmnPFS_b.PDR == 0U) {
        mp_raise_ValueError(
            MP_ERROR_TEXT("drive is only valid for output mode"));
    }

    mp_int_t drive = mp_obj_get_int(args[1]);
    uint32_t drive_cfg;

    switch (drive) {
        case MACHINE_PIN_DRIVE_0:
            drive_cfg = 0U;
            break;

        case MACHINE_PIN_DRIVE_1:
            drive_cfg = IOPORT_CFG_DRIVE_MID;
            break;

        case MACHINE_PIN_DRIVE_2:
            drive_cfg = IOPORT_CFG_DRIVE_HS_HIGH;
            break;

        case MACHINE_PIN_DRIVE_3:
            drive_cfg = IOPORT_CFG_DRIVE_HIGH;
            break;

        default:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid pin drive"));
    }

    uint32_t cfg = R_PFS->PORT[port].PIN[bit].PmnPFS;

    cfg &= ~((uint32_t) R_PFS_PORT_PIN_PmnPFS_DSCR_Msk);
    cfg |= drive_cfg;

    fsp_err_t err = R_IOPORT_PinCfg(
        g_ioport.p_ctrl,
        self->pin,
        cfg
        );
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    machine_pin_drive_obj,
    1,
    2,
    machine_pin_drive
    );



/**
 * @brief 读取或设置 Pin 对象的数字电平。
 *
 * 不传入 value 时读取当前电平；传入 value 时根据其 Python
 * 真值输出高电平或低电平。
 *
 * @param n_args 参数数量，包括 self。
 * @param args 参数数组，其中 args[0] 是 Pin 对象。
 * @return 读取时返回 0 或 1，写入时返回 None。
 * @exception OSError FSP 读取或写入引脚失败。
 */
static mp_obj_t machine_pin_value(size_t n_args, const mp_obj_t *args)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    fsp_err_t err;

    if (n_args == 1) {
        bsp_io_level_t level;

        err = R_IOPORT_PinRead(g_ioport.p_ctrl, self->pin, &level);
        if (err != FSP_SUCCESS) {
            mp_raise_OSError(MP_EIO);
        }

        return MP_OBJ_NEW_SMALL_INT(level == BSP_IO_LEVEL_HIGH);
    }

    bsp_io_level_t level = mp_obj_is_true(args[1]) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;

    err = R_IOPORT_PinWrite(g_ioport.p_ctrl, self->pin, level);
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_value_obj, 1, 2, machine_pin_value);

/**
 * @brief       直接调用 Pin 对象以读取或设置数字电平。
 * @param       self_in 当前 Pin 对象。
 * @param       n_args  用户传入的位置参数数量，不包括 self。
 * @param       n_kw    用户传入的关键字参数数量。
 * @param       args    用户传入的位置参数。
 * @return      未传值时返回 0 或 1，传入值时返回 None。
 */
static mp_obj_t machine_pin_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    mp_arg_check_num(n_args, n_kw, 0, 1, false);

    mp_obj_t value_args[2] = {
        self_in,
        MP_OBJ_NULL,
    };

    if (n_args == 1) {
        value_args[1] = args[0];
    }

    return machine_pin_value(n_args + 1, value_args);
}

/**
 * @brief       将 Pin 对象的输出电平设置为高。
 * @param       self_in 当前 Pin 对象。
 * @return      None。
 */
static mp_obj_t machine_pin_on(mp_obj_t self_in)
{
    mp_obj_t value_args[2] = {
        self_in,
        mp_const_true,
    };

    return machine_pin_value(2, value_args);
}

static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_on_obj, machine_pin_on);

/**
 * @brief       将 Pin 对象的输出电平设置为低。
 *
 * @param       self_in 当前 Pin 对象。
 * @return      None。
 */
static mp_obj_t machine_pin_off(mp_obj_t self_in)
{
    mp_obj_t value_args[2] = {
        self_in,
        mp_const_false,
    };

    return machine_pin_value(2, value_args);
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    machine_pin_off_obj,
    machine_pin_off
    );
    
/**
 * @brief       Toggle the current pin output level.
 *
 * @param       self_in Pin object.
 * @return      None.
 */
static mp_obj_t machine_pin_toggle(mp_obj_t self_in)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bsp_io_level_t level;

    fsp_err_t err = R_IOPORT_PinRead(g_ioport.p_ctrl, self->pin, &level);
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    level = level == BSP_IO_LEVEL_HIGH
        ? BSP_IO_LEVEL_LOW
        : BSP_IO_LEVEL_HIGH;

    err = R_IOPORT_PinWrite(g_ioport.p_ctrl, self->pin, level);
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(
    machine_pin_toggle_obj,
    machine_pin_toggle
    );

MP_DEFINE_CONST_OBJ_TYPE(
    machine_pin_board_pins_obj_type,
    MP_QSTR_board,
    MP_TYPE_FLAG_NONE,
    locals_dict, &machine_pin_board_pins_locals_dict
    );

MP_DEFINE_CONST_OBJ_TYPE(
    machine_pin_cpu_pins_obj_type,
    MP_QSTR_cpu,
    MP_TYPE_FLAG_NONE,
    locals_dict, &machine_pin_cpu_pins_locals_dict
    );

/** machine.Pin 类的常量和方法。 */
static const mp_rom_map_elem_t machine_pin_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_ALT), MP_ROM_INT(MACHINE_PIN_MODE_ALT)},
    {MP_ROM_QSTR(MP_QSTR_ALT_OPEN_DRAIN), MP_ROM_INT(MACHINE_PIN_MODE_ALT_OPEN_DRAIN)},
    {MP_ROM_QSTR(MP_QSTR_ANALOG), MP_ROM_INT(MACHINE_PIN_MODE_ANALOG)},
    {MP_ROM_QSTR(MP_QSTR_DRIVE_0), MP_ROM_INT(MACHINE_PIN_DRIVE_0)},
    {MP_ROM_QSTR(MP_QSTR_DRIVE_1), MP_ROM_INT(MACHINE_PIN_DRIVE_1)},
    {MP_ROM_QSTR(MP_QSTR_DRIVE_2), MP_ROM_INT(MACHINE_PIN_DRIVE_2)},
    {MP_ROM_QSTR(MP_QSTR_DRIVE_3), MP_ROM_INT(MACHINE_PIN_DRIVE_3)},
    {MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(MACHINE_PIN_MODE_IN)},
    {MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(MACHINE_PIN_MODE_OPEN_DRAIN)},
    {MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(MACHINE_PIN_MODE_OUT)},
    {MP_ROM_QSTR(MP_QSTR_PULL_NONE), MP_ROM_INT(MACHINE_PIN_PULL_NONE)},
    {MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(MACHINE_PIN_PULL_UP)},
    {MP_ROM_QSTR(MP_QSTR_board),MP_ROM_PTR(&machine_pin_board_pins_obj_type)},
    {MP_ROM_QSTR(MP_QSTR_cpu),MP_ROM_PTR(&machine_pin_cpu_pins_obj_type)},
    {MP_ROM_QSTR(MP_QSTR_drive), MP_ROM_PTR(&machine_pin_drive_obj)},
    {MP_ROM_QSTR(MP_QSTR_high), MP_ROM_PTR(&machine_pin_on_obj) },
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_pin_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_low), MP_ROM_PTR(&machine_pin_off_obj) },
    {MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&machine_pin_mode_obj) },
    {MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&machine_pin_off_obj)},
    {MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&machine_pin_on_obj)},
    {MP_ROM_QSTR(MP_QSTR_pull), MP_ROM_PTR(&machine_pin_pull_obj)},
    {MP_ROM_QSTR(MP_QSTR_toggle), MP_ROM_PTR(&machine_pin_toggle_obj)},
    {MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&machine_pin_value_obj)},
};

static MP_DEFINE_CONST_DICT(
    machine_pin_locals_dict,
    machine_pin_locals_dict_table
    );

/** Python 层 machine.Pin 对应的 MicroPython 类型。 */
MP_DEFINE_CONST_OBJ_TYPE(
    machine_pin_type,
    MP_QSTR_Pin,
    MP_TYPE_FLAG_NONE,
    make_new, machine_pin_make_new,
    call, machine_pin_call,
    locals_dict, &machine_pin_locals_dict
    );
