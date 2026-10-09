#include <inttypes.h>
#include "pin.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

#ifndef __PIN_DEBUG
#define __PIN_DEBUG 1
#endif

#if __PIN_DEBUG
#include "utils/log.h"
#define PIN_LOGD(msg, ...)  LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_LOGI(msg, ...)  LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_LOGW(msg, ...)  LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_LOGE(msg, ...)  LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define PIN_LOGD(msg, ...)
#define PIN_LOGI(msg, ...)
#define PIN_LOGW(msg, ...)
#define PIN_LOGE(msg, ...)
#endif

#define MACHINE_PIN_PINS_PER_PORT   16
#define MACHINE_PIN_PORT_COUNT      16

/**
 * @brief machine.Pin 支持的 GPIO 输出驱动能力。
 */
enum machine_pin_drive_t {
    MACHINE_PIN_DRIVE_0 = 0,    /* Low Drive */
    MACHINE_PIN_DRIVE_1,        /* Middle Drive */
    MACHINE_PIN_DRIVE_2,        /* High Drive */
    MACHINE_PIN_DRIVE_3,        /* High Speed High Drive */
};

/**
 * @brief machine.Pin 支持的 GPIO 工作模式。
 */
enum machine_pin_mode_t {
    MACHINE_PIN_MODE_IN = 0,
    MACHINE_PIN_MODE_OUT,
    MACHINE_PIN_MODE_OPEN_DRAIN,
    MACHINE_PIN_MODE_ANALOG,
    MACHINE_PIN_MODE_ALT,
    MACHINE_PIN_MODE_ALT_OPEN_DRAIN,
    MACHINE_PIN_MODE_UNSELECT,          /* Python 层不允许传入这个值 */
};

/**
 * @brief machine.Pin 支持的 GPIO 上下拉配置。
 */
enum machine_pin_pull_t {
    MACHINE_PIN_PULL_NONE = 0,
    MACHINE_PIN_PULL_UP,
};

/**
 * @brief       将用户传入的引脚标识转换为 Pin 对象。支持已有的 Pin 对象和复用表中注册的引脚名称。
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

const machine_pin_af_obj_t *machine_pin_find_af(bsp_io_port_pin_t pin, machine_pin_af_peripheral_t peripheral, uint8_t channel, machine_pin_af_signal_t signal)
{
    for (size_t index = 0; index < machine_pin_afs_count; ++index) {
        const machine_pin_af_obj_t *af = &machine_pin_afs[index];

        if (af->pin == pin && af->peripheral == peripheral && af->channel == channel && af->signal == signal) {
            return af;
        }
    }

    return NULL;
}

/**
 * @brief   释放占用的引脚，并恢复 take 前的 pin 配置
 * @note    已放弃 Pin 互斥访问功能，为其它模块暂时保留此函数
 * @param   pin_id 要释放的引脚
 * @retval  true   成功
 * @retval  false  指定的引脚不存在
 */
bool machine_pin_give(bsp_io_port_pin_t pin_id)
{
    (void)pin_id;

    return true;
}

/**
 * @brief   申请占用指定的引脚
 * @note    已放弃 Pin 互斥访问功能，为其它模块暂时保留此函数
 * @param   pin_id 要占用的引脚
 * @retval  true   申请占用成功
 * @retval  false  申请占用失败
 */
bool machine_pin_take(bsp_io_port_pin_t pin_id)
{
    (void)pin_id;
    
    return true;
}

/**
 * @brief       根据 MicroPython 模式配置 GPIO 引脚。
 *
 * @param       pin   要配置的 Pin 对象。
 * @param       mode  输入或输出模式。
 * @param       pull  无上下拉或内部上拉配置，非法值静默忽略
 * @param       value 初始输出值，未提供时为 MP_OBJ_NULL。
 * @param       drive 输出驱动能力，未提供时为 MP_OBJ_NULL，非法值静默忽略
 * @param       alt   复用功能描述，值为 1~31，对应于引脚复用表中的列，当前直接忽略，不配置复用功能
 * @exception   ValueError mode、pull 或 drive 不是当前支持的配置。
 * @exception   OSError FSP 配置引脚失败。
 */
static void machine_pin_configure(const machine_pin_obj_t *pin, enum machine_pin_mode_t mode, mp_obj_t pull, mp_obj_t value, mp_obj_t drive, mp_obj_t alt)
{
    uint32_t pmnpfs = R_PFS->PORT[pin->pin >> 8].PIN[pin->pin & 0xFF].PmnPFS;
    mp_int_t pull_value = -1;
    mp_int_t value_value = -1;
    mp_int_t drive_value = -1;

    (void)alt;

    if (pull != MP_OBJ_NULL) {
        if (pull == mp_const_none) {
            pull_value = MACHINE_PIN_PULL_NONE;
        }
        else {
            pull_value = mp_obj_get_int(pull);
        }
    }
    if (value != MP_OBJ_NULL && value != mp_const_none) {
        if (mp_obj_is_true(value)) {
            value_value = 1;
        }
        else {
            value_value = 0;
        }
    }
    if (drive != MP_OBJ_NULL && drive != mp_const_none) {
        drive_value = mp_obj_get_int(drive);
    }

    pmnpfs &= ~IOPORT_CFG_IRQ_ENABLE;
    pmnpfs &= ~IOPORT_CFG_ANALOG_ENABLE;
    pmnpfs &= ~IOPORT_CFG_PERIPHERAL_PIN;
    pmnpfs &= ~R_PFS_PORT_PIN_PmnPFS_PSEL_Msk;

    switch (mode) {
    case MACHINE_PIN_MODE_IN:
        pmnpfs &= ~IOPORT_CFG_PORT_DIRECTION_OUTPUT;
        pmnpfs &= ~IOPORT_CFG_NMOS_ENABLE;
        break;
    case MACHINE_PIN_MODE_OUT:
        pmnpfs |= IOPORT_CFG_PORT_DIRECTION_OUTPUT;
        pmnpfs &= ~IOPORT_CFG_NMOS_ENABLE;
        break;
    case MACHINE_PIN_MODE_OPEN_DRAIN:
        pmnpfs |= IOPORT_CFG_PORT_DIRECTION_OUTPUT;
        pmnpfs |= IOPORT_CFG_NMOS_ENABLE;
        break;
    default:
        break;
    }

    switch (pull_value) {
    case MACHINE_PIN_PULL_NONE:
        pmnpfs &= ~IOPORT_CFG_PULLUP_ENABLE;
        break;
    case MACHINE_PIN_PULL_UP:
        pmnpfs |= IOPORT_CFG_PULLUP_ENABLE;
        break;
    default:
        break;
    }

    switch (value_value) {
    case 1:
        pmnpfs |= IOPORT_CFG_PORT_OUTPUT_HIGH;
        break;
    case 0:
        pmnpfs &= ~IOPORT_CFG_PORT_OUTPUT_HIGH;
        break;
    default:
        break;
    }

    switch (drive_value) {
    case MACHINE_PIN_DRIVE_0:
        pmnpfs &= ~IOPORT_CFG_DRIVE_HIGH;
        break;
    case MACHINE_PIN_DRIVE_1:
        pmnpfs &= ~IOPORT_CFG_DRIVE_HIGH;
        pmnpfs |= IOPORT_CFG_DRIVE_MID;
        break;
    case MACHINE_PIN_DRIVE_2:
        pmnpfs |= IOPORT_CFG_DRIVE_HIGH;
        break;
    case MACHINE_PIN_DRIVE_3:
        pmnpfs &= ~IOPORT_CFG_DRIVE_HIGH;
        pmnpfs |= IOPORT_CFG_DRIVE_HS_HIGH;
        break;
    default:
        break;
    }

    PIN_LOGD("Write PmnPFS: 0x%08" PRIX32, pmnpfs);
    R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, pmnpfs);
}

/**
 * @brief       将引脚配置为指定的外设复用功能。
 *
 * @param       pin_id     要配置的引脚。
 * @param       peripheral FSP 外设复用功能。
 * @param       options    驱动能力、开漏和上拉等额外配置。
 * @exception   OSError FSP 配置引脚失败。
 */
void machine_pin_configure_alt(bsp_io_port_pin_t pin_id, ioport_peripheral_t peripheral, uint32_t options)
{
    uint32_t cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)peripheral | options;

    fsp_err_t error = R_IOPORT_PinCfg(g_ioport.p_ctrl, pin_id, cfg);

    if (error != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
}

void machine_pin_configure_input(const machine_pin_obj_t *pin)
{
    machine_pin_configure(pin, MACHINE_PIN_MODE_IN, MP_OBJ_NULL, MP_OBJ_NULL, MP_OBJ_NULL, MP_OBJ_NULL);
}

/* 把 CS 配置为 GPIO 输出 */
void machine_pin_configure_output(const machine_pin_obj_t *pin, bool value)
{
    machine_pin_configure(pin, MACHINE_PIN_MODE_OUT, MP_OBJ_NULL, value ? mp_const_true : mp_const_false, MP_OBJ_NULL, MP_OBJ_NULL);
}

bool machine_pin_read(const machine_pin_obj_t *pin)
{
    bsp_io_level_t level;
    fsp_err_t err = R_IOPORT_PinRead(g_ioport.p_ctrl, pin->pin, &level);

    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }

    return level == BSP_IO_LEVEL_HIGH;
}

/* 把 CS 拉高或拉低 */
void machine_pin_write(const machine_pin_obj_t *pin, bool value)
{
    fsp_err_t err = R_IOPORT_PinWrite(g_ioport.p_ctrl, pin->pin, value ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);

    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
}

/**
 * @brief       创建 machine.Pin 对象并配置输入输出参数
 * @note        mode 参数只允许设置为 IN 或 OUT 或 OPEN_DRAIN
 *              此函数默认意图为将 Pin 作为 GPIO 使用，因此会额外清理复用功能设置，但 pull、value、drive 保留
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

    enum machine_pin_mode_t mode = MACHINE_PIN_MODE_UNSELECT;

    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, parsed_args);

    const machine_pin_obj_t *pin = machine_pin_find(parsed_args[ARG_id].u_obj);
    PIN_LOGD("id = %s", qstr_str(pin->name));

    if (machine_pin_irq_is_active(pin)) {
        PIN_LOGE("Current pin is used as external irq pin");
        mp_raise_OSError(MP_EBUSY);
    }

    if ((parsed_args[ARG_mode].u_obj != NULL) && (parsed_args[ARG_mode].u_obj != mp_const_none)) {
        mode = mp_obj_get_int(parsed_args[ARG_mode].u_obj);
        if (mode == MACHINE_PIN_MODE_UNSELECT) {
            mp_raise_ValueError(MP_ERROR_TEXT("RA8 pin mode only support IN, OUT and OPEN_DRAIN"));
        }
    }

    switch (mode) {
    case MACHINE_PIN_MODE_IN:
    case MACHINE_PIN_MODE_OUT:
    case MACHINE_PIN_MODE_OPEN_DRAIN:
    case MACHINE_PIN_MODE_UNSELECT:
        machine_pin_configure(pin, mode, parsed_args[ARG_pull].u_obj, parsed_args[ARG_value].u_obj, parsed_args[ARG_drive].u_obj, parsed_args[ARG_alt].u_obj);
        break;
    default:
        mp_raise_ValueError(MP_ERROR_TEXT("RA8 pin mode only support IN, OUT and OPEN_DRAIN"));
        break;
    }

    return MP_OBJ_FROM_PTR(pin);
}

/**
 * @brief       使用指定参数重新配置 Pin 对象。
 * @note        mode 参数只允许设置为 IN 或 OUT
 *              此函数默认意图为将 Pin 作为 GPIO 使用，因此会额外清理复用功能设置，但 pull、value、drive 保留
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

    enum machine_pin_mode_t mode = MACHINE_PIN_MODE_UNSELECT;
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, parsed_args);
    PIN_LOGD("Pin: %s", qstr_str(self->name));

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Current pin is used as external irq pin");
        mp_raise_OSError(MP_EBUSY);
    }

    if ((parsed_args[ARG_mode].u_obj != NULL) && (parsed_args[ARG_mode].u_obj != mp_const_none)) {
        mode = mp_obj_get_int(parsed_args[ARG_mode].u_obj);
        if (mode == MACHINE_PIN_MODE_UNSELECT) {
            mp_raise_ValueError(MP_ERROR_TEXT("RA8 pin mode only support IN, OUT and OPEN_DRAIN"));
        }
    }

    switch (mode) {
    case MACHINE_PIN_MODE_IN:
    case MACHINE_PIN_MODE_OUT:
    case MACHINE_PIN_MODE_OPEN_DRAIN:
    case MACHINE_PIN_MODE_UNSELECT:
        machine_pin_configure(self, mode, parsed_args[ARG_pull].u_obj, parsed_args[ARG_value].u_obj, parsed_args[ARG_drive].u_obj, parsed_args[ARG_alt].u_obj);
        break;
    default:
        mp_raise_ValueError(MP_ERROR_TEXT("RA8 pin mode only support IN, OUT and OPEN_DRAIN"));
        break;
    }

    return mp_const_none;
}

static mp_obj_t machine_pin_deinit(mp_obj_t self_in)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(self_in);

    PIN_LOGD("%s call", qstr_str(self->name));

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Current pin is used as external irq pin");
        mp_raise_OSError(MP_EBUSY);
    }

    return mp_const_none;
}

/**
 * @brief   读取或设置引脚模式，对应于 Pin.mode([mode])
 * @note    mode 参数只允许设置为 IN 或 OUT 或 OPEN_DRAIN
 *          此函数默认意图为将 Pin 作为 GPIO 使用，因此会额外清理复用功能设置，但 pull、value、drive 保留
 * @param   n_args 参数数量
 * @param   args   参数数组
 */
static mp_obj_t machine_pin_mode(size_t n_args, const mp_obj_t *args)
{
    enum machine_pin_mode_t mode = MACHINE_PIN_MODE_UNSELECT;
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = self->pin >> 8;
    uint32_t bit = self->pin & 0xFF;

    if (n_args == 1) {
        uint32_t analog = R_PFS->PORT[port].PIN[bit].PmnPFS_b.ASEL;
        uint32_t peripheral = R_PFS->PORT[port].PIN[bit].PmnPFS_b.PMR;
        uint32_t direction = R_PFS->PORT[port].PIN[bit].PmnPFS_b.PDR;
        uint32_t open_drain = R_PFS->PORT[port].PIN[bit].PmnPFS_b.NCODR;

        if (analog != 0U) {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_MODE_ANALOG);
        }

        if (peripheral != 0U) {
            return MP_OBJ_NEW_SMALL_INT(open_drain == 0U ? MACHINE_PIN_MODE_ALT : MACHINE_PIN_MODE_ALT_OPEN_DRAIN);
        }

        if (direction == 0U) {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_MODE_IN);
        }

        return MP_OBJ_NEW_SMALL_INT(open_drain == 0U ? MACHINE_PIN_MODE_OUT : MACHINE_PIN_MODE_OPEN_DRAIN);
    }

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Current pin is used as external irq pin");
        mp_raise_OSError(MP_EBUSY);
    }

    mode = mp_obj_get_int(args[1]);
    switch (mode) {
    case MACHINE_PIN_MODE_IN:
    case MACHINE_PIN_MODE_OUT:
    case MACHINE_PIN_MODE_OPEN_DRAIN:
        machine_pin_configure(self, mode, MP_OBJ_NULL, MP_OBJ_NULL, MP_OBJ_NULL, MP_OBJ_NULL);
        break;
    default:
        mp_raise_ValueError(MP_ERROR_TEXT("RA8 pin mode only support IN, OUT and OPEN_DRAIN"));
        break;
    }

    return mp_const_none;
}

static mp_obj_t machine_pin_pull(size_t n_args, const mp_obj_t *args)
{
    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = self->pin >> 8;
    uint32_t bit = self->pin & 0xFF;
    uint32_t pmnpfs = R_PFS->PORT[port].PIN[bit].PmnPFS;

    if (n_args == 1) {
        return MP_OBJ_NEW_SMALL_INT((pmnpfs & IOPORT_CFG_PULLUP_ENABLE) != 0U ? MACHINE_PIN_PULL_UP : MACHINE_PIN_PULL_NONE);
    }

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Current pin is used as external irq pin");
        mp_raise_OSError(MP_EBUSY);
    }

    if (args[1] == mp_const_none) {
        pmnpfs &= ~IOPORT_CFG_PULLUP_ENABLE;
    }
    else {
        mp_int_t pull = mp_obj_get_int(args[1]);

        switch (pull) {
        case MACHINE_PIN_PULL_NONE:
            pmnpfs &= ~IOPORT_CFG_PULLUP_ENABLE;
            break;
        case MACHINE_PIN_PULL_UP:
            pmnpfs |= IOPORT_CFG_PULLUP_ENABLE;
            break;
        default:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid pin pull"));
        }
    }

    R_IOPORT_PinCfg(g_ioport.p_ctrl, self->pin, pmnpfs);

    return mp_const_none;
}

/**
 * @brief   读取或设置 Pin 的驱动能力，对应于 Pin.drive([drive])
 * @param   n_args 参数数量
 * @param   args   参数数组
 */
static mp_obj_t machine_pin_drive(size_t n_args, const mp_obj_t *args)
{
    uint32_t drive;
    uint32_t pmnpfs;
    mp_int_t drive_value;

    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t port = self->pin >> 8;
    uint32_t bit = self->pin & 0xFF;

    if (n_args == 1) {
        drive = R_PFS->PORT[port].PIN[bit].PmnPFS_b.DSCR;
        /* DSCR = 2 时对应 HH */
        if ((drive == 0) || (drive == 1)) {
            return MP_OBJ_NEW_SMALL_INT(drive);
        }
        else if (drive == 2) {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_DRIVE_3);
        }
        else {
            return MP_OBJ_NEW_SMALL_INT(MACHINE_PIN_DRIVE_2);
        }
    }

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Pin[%s] current is used as external irq", qstr_str(self->name));
        mp_raise_OSError(MP_EBUSY);
    }

    drive_value = mp_obj_get_int(args[1]);
    pmnpfs = R_PFS->PORT[port].PIN[bit].PmnPFS;
    pmnpfs &= ~R_PFS_PORT_PIN_PmnPFS_DSCR_Msk;
    switch (drive_value) {
    case MACHINE_PIN_DRIVE_0:
        break;
    case MACHINE_PIN_DRIVE_1:
        pmnpfs |= IOPORT_CFG_DRIVE_MID;
        break;
    case MACHINE_PIN_DRIVE_2:
        pmnpfs |= IOPORT_CFG_DRIVE_HIGH;
        break;
    case MACHINE_PIN_DRIVE_3:
        pmnpfs |= IOPORT_CFG_DRIVE_HS_HIGH;
        break;
    default:
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pin drive"));
    }
    R_IOPORT_PinCfg(g_ioport.p_ctrl, self->pin, pmnpfs);

    return mp_const_none;
}

/**
 * @brief       读取或设置 Pin 对象的数字电平。
 * @param       n_args 参数数量，包括 self。
 * @param       args   参数数组，其中 args[0] 是 Pin 对象。
 * @return      读取时返回 0 或 1，写入时返回 None。
 * @exception   OSError FSP 读取或写入引脚失败。
 */
static mp_obj_t machine_pin_value(size_t n_args, const mp_obj_t *args)
{
    bsp_io_level_t level;

    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if (n_args == 1) {
        R_IOPORT_PinRead(g_ioport.p_ctrl, self->pin, &level);

        return MP_OBJ_NEW_SMALL_INT(level);
    }

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Pin[%s] current is used as external irq", qstr_str(self->name));
        mp_raise_OSError(MP_EBUSY);
    }

    level = mp_obj_is_true(args[1]) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;
    R_IOPORT_PinWrite(g_ioport.p_ctrl, self->pin, level);

    return mp_const_none;
}

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

static mp_obj_t machine_pin_toggle(mp_obj_t self_in)
{
    bsp_io_level_t level;

    const machine_pin_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (machine_pin_irq_is_active(self)) {
        PIN_LOGE("Pin[%s] current is used as external irq", qstr_str(self->name));
        mp_raise_OSError(MP_EBUSY);
    }

    R_IOPORT_PinRead(g_ioport.p_ctrl, self->pin, &level);
    if (level == BSP_IO_LEVEL_HIGH) {
        R_IOPORT_PinWrite(g_ioport.p_ctrl, self->pin, BSP_IO_LEVEL_LOW);
    }
    else {
        R_IOPORT_PinWrite(g_ioport.p_ctrl, self->pin, BSP_IO_LEVEL_HIGH);
    }

    return mp_const_none;
}

void machine_pin_deinit_all(void)
{
}

static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_deinit_obj, machine_pin_deinit);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_drive_obj, 1, 2, machine_pin_drive);
static MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_init_obj, 1, machine_pin_init);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_mode_obj, 1, 2, machine_pin_mode);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_off_obj, machine_pin_off);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_on_obj, machine_pin_on);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_pull_obj, 1, 2, machine_pin_pull);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_toggle_obj, machine_pin_toggle);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_value_obj, 1, 2, machine_pin_value);

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
    {MP_ROM_QSTR(MP_QSTR_IRQ_FALLING), MP_ROM_INT(MACHINE_PIN_IRQ_FALLING)},
    {MP_ROM_QSTR(MP_QSTR_IRQ_HIGH_LEVEL), MP_ROM_INT(MACHINE_PIN_IRQ_HIGH_LEVEL)},
    {MP_ROM_QSTR(MP_QSTR_IRQ_LOW_LEVEL), MP_ROM_INT(MACHINE_PIN_IRQ_LOW_LEVEL)},
    {MP_ROM_QSTR(MP_QSTR_IRQ_RISING), MP_ROM_INT(MACHINE_PIN_IRQ_RISING)},
    {MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(MACHINE_PIN_MODE_OPEN_DRAIN)},
    {MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(MACHINE_PIN_MODE_OUT)},
    {MP_ROM_QSTR(MP_QSTR_PULL_NONE), MP_ROM_INT(MACHINE_PIN_PULL_NONE)},
    {MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(MACHINE_PIN_PULL_UP)},
    {MP_ROM_QSTR(MP_QSTR_board),MP_ROM_PTR(&machine_pin_board_pins_obj_type)},
    {MP_ROM_QSTR(MP_QSTR_cpu),MP_ROM_PTR(&machine_pin_cpu_pins_obj_type)},
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_pin_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_drive), MP_ROM_PTR(&machine_pin_drive_obj)},
    {MP_ROM_QSTR(MP_QSTR_high), MP_ROM_PTR(&machine_pin_on_obj) },
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_pin_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_irq), MP_ROM_PTR(&machine_pin_irq_fun_obj)},
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
