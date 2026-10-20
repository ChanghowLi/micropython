#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "hal_data.h"
#include "pin.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "shared/runtime/mpirq.h"

#ifndef PIN_IRQ_DEBUG
#define PIN_IRQ_DEBUG   1
#endif

#define MACHINE_PIN_IRQ_CHANNEL_COUNT   32
#define MACHINE_PIN_IRQ_PENDING_MAX     UINT32_MAX
#define MACHINE_PIN_IRQ_PRIORITY_MAX    15
#define MACHINE_PIN_IRQ_PRIORITY_MIN    0

#if PIN_IRQ_DEBUG
#include "utils/log.h"
#define PIN_IRQ_LOGD(msg, ...)              LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_IRQ_LOGI(msg, ...)              LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_IRQ_LOGW(msg, ...)              LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define PIN_IRQ_LOGE(msg, ...)              LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define UNLIKE_RAISE_EIO(v, t, msg, ...)    if (v != t) { if (msg != NULL) { PIN_IRQ_LOGE(msg, ##__VA_ARGS__); mp_raise_OSError(MP_EIO); } }
#else
#define PIN_IRQ_LOGD(msg, ...)
#define PIN_IRQ_LOGI(msg, ...)
#define PIN_IRQ_LOGW(msg, ...)
#define PIN_IRQ_LOGE(msg, ...)
#define UNLIKE_RAISE_EIO(v, t, msg, ...)    if (v != t) { mp_raise_OSError(MP_EIO); }
#endif

typedef struct _machine_pin_irq_obj_t {
    mp_irq_obj_t base;
    struct _machine_pin_irq_obj_t *next;
    const machine_pin_obj_t *pin;
    const external_irq_instance_t *instance;
    external_irq_cfg_t cfg;
    mp_uint_t flags;
    mp_uint_t trigger;
    mp_sched_node_t cleanup_node;
    mp_sched_node_t soft_node;
    volatile uint32_t hard_callback_count;
    volatile uint32_t hard_error_count;
    volatile uint32_t pending_count;
    volatile uint32_t raw_isr_count;
    volatile uint32_t overflow_count;
    uint32_t saved_pin_cfg;
    bool open;
    bool deferred_cleanup;
    bool pin_cfg_saved;
} machine_pin_irq_obj_t;

static const external_irq_instance_t *const machine_pin_irq_instances[MACHINE_PIN_IRQ_CHANNEL_COUNT] = {
    [0] = &g_external_irq0,
    [1] = &g_external_irq1,
    [2] = &g_external_irq2,
    [3] = &g_external_irq3,
    [4] = &g_external_irq4,
    [5] = &g_external_irq5,
    [6] = &g_external_irq6,
    [7] = &g_external_irq7,
    [8] = &g_external_irq8,
    [9] = &g_external_irq9,
    [10] = &g_external_irq10,
    [11] = &g_external_irq11,
    [12] = &g_external_irq12,
    [13] = &g_external_irq13,
    [14] = &g_external_irq14,
    [15] = &g_external_irq15,
    [16] = &g_external_irq16,
    [17] = &g_external_irq17,
    [18] = &g_external_irq18,
    [19] = &g_external_irq19,
    [20] = NULL,
    [21] = &g_external_irq21,
    [22] = &g_external_irq22,
    [23] = &g_external_irq23,
    [24] = &g_external_irq24,
    [25] = &g_external_irq25,
    [26] = &g_external_irq26,
    [27] = &g_external_irq27,
    [28] = &g_external_irq28,
    [29] = &g_external_irq29,
    [30] = &g_external_irq30,
    [31] = &g_external_irq31,
};

MP_REGISTER_ROOT_POINTER(void *machine_pin_irq_obj[MACHINE_PIN_IRQ_CHANNEL_COUNT]);
MP_REGISTER_ROOT_POINTER(void *machine_pin_irq_obj_list);

/**
 * @brief   从 machine_pin_irq_obj_list 中查找已存在的 machine_pin_irq_obj_t 对象
 * @param   pin machine_pin_obj_t 对象
 * @return  如果 machine_pin_irq_obj_list 中有元素的 machine_pin_irq_obj_t::pin 指针指向 pin 对象，则返回该元素的指针，否则返回 NULL
 */
static machine_pin_irq_obj_t *find_exist_irq_obj(const machine_pin_obj_t *pin)
{
    machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj_list);

    while (irq != NULL) {
        if (irq->pin == pin) {
            return irq;
        }

        irq = irq->next;
    }

    return NULL;
}

/**
 * @brief   查询给定的引脚当前是否有激活的中断
 * @param   pin 要查询的引脚对象
 * @retval  true  已有激活的中断
 * @retval  false 没有已激活的中断
 */
bool machine_pin_irq_is_active(const machine_pin_obj_t *pin)
{
    if (pin->irq_channel < 0 || pin->irq_channel >= MACHINE_PIN_IRQ_CHANNEL_COUNT) {
        return false;
    }

    machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]);

    return irq != NULL && irq->pin == pin && irq->open;
}

/**
 * @brief   将 Python 的触发枚举转为 FSP 的触发枚举值
 * @param   trigger Python 的触发枚举
 * @retval  FSP 的触发枚举
 */
static external_irq_trigger_t convert_to_fsp_enum(mp_uint_t trigger)
{
    external_irq_trigger_t t = -1;

    switch (trigger) {
    case MACHINE_PIN_IRQ_FALLING:
        t = EXTERNAL_IRQ_TRIGGER_FALLING;
        break;
    case MACHINE_PIN_IRQ_RISING:
        t = EXTERNAL_IRQ_TRIGGER_RISING;
        break;
    case MACHINE_PIN_IRQ_FALLING | MACHINE_PIN_IRQ_RISING:
        t = EXTERNAL_IRQ_TRIGGER_BOTH_EDGE;
        break;
    case MACHINE_PIN_IRQ_LOW_LEVEL:
        t = EXTERNAL_IRQ_TRIGGER_LEVEL_LOW;
        break;
    default:
        break;
    }

    return t;
}

/**
 * @brief   重配置引脚为 IRQ 功能或恢复引脚原功能
 * @param   irq    machine_pin_irq_obj_t 对象
 * @param   enable 是否配置为 IRQ，为 false 时会恢复先前的引脚配置
 */
static void configure_pin(machine_pin_irq_obj_t *irq, bool enable)
{
    uint32_t cfg, err;

    const machine_pin_obj_t *pin = irq->pin;
    uint32_t port = pin->pin >> 8;
    uint32_t bit = pin->pin & 0xFF;

    if (enable) {
        if (!irq->pin_cfg_saved) {
            irq->saved_pin_cfg = R_PFS->PORT[port].PIN[bit].PmnPFS;
            irq->pin_cfg_saved = true;
        }

        cfg = irq->saved_pin_cfg;
        cfg &= ~((uint32_t)(IOPORT_CFG_ANALOG_ENABLE | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_CFG_PORT_DIRECTION_OUTPUT | R_PFS_PORT_PIN_PmnPFS_PSEL_Msk));
        cfg |= IOPORT_CFG_IRQ_ENABLE | IOPORT_CFG_PORT_DIRECTION_INPUT;

        err = R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, cfg);
        UNLIKE_RAISE_EIO(err, 0, "R_IOPORT_PinCfg() failed: %" PRIu32, err);
    }
    else if (irq->pin_cfg_saved) {
        err = R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, irq->saved_pin_cfg);
        UNLIKE_RAISE_EIO(err, 0, "R_IOPORT_PinCfg() failed: %" PRIu32, err);
        irq->pin_cfg_saved = false;
    }
}

/**
 * @brief   关闭指定的 external_irq 外设，并重配置引脚
 * @param   irq machine_pin_irq_obj_t 对象
 */
static void fsp_irq_close(machine_pin_irq_obj_t *irq)
{
    uint32_t err;

    if (irq->open) {
        err = irq->instance->p_api->disable(irq->instance->p_ctrl);
        UNLIKE_RAISE_EIO(err, 0, "disable() failed: %" PRIu32, err);
        err = irq->instance->p_api->close(irq->instance->p_ctrl);
        UNLIKE_RAISE_EIO(err, 0, "close() failed: %" PRIu32, err);

        irq->open = false;
    }

    configure_pin(irq, false);
}

/**
 * @brief   释放 irq 资源
 * @param   irq machine_pin_irq_obj_t 对象
 * @details 移除 machine_pin_irq_obj 中的槽位
 *          重置 irq 中的部分成员变量
 */
static void machine_pin_irq_release(machine_pin_irq_obj_t *irq)
{
    size_t channel = (size_t)irq->pin->irq_channel;

    if (MP_STATE_PORT(machine_pin_irq_obj[channel]) == irq) {
        fsp_irq_close(irq);
        MP_STATE_PORT(machine_pin_irq_obj[channel]) = NULL;
    }

    irq->flags = 0;
    irq->trigger = 0;
    irq->pending_count = 0;
    irq->deferred_cleanup = false;
}

/**
 * @brief   强制关闭 irq 和清理 machine_pin_irq_obj_t 对象的内部变量
 * @param   irq machine_pin_irq_obj_t 对象
 */
static void force_close_irq(machine_pin_irq_obj_t *irq)
{
    if (irq->open) {
        irq->instance->p_api->disable(irq->instance->p_ctrl);
        irq->instance->p_api->close(irq->instance->p_ctrl);
    }

    if (irq->pin_cfg_saved) {
        if (R_IOPORT_PinCfg(g_ioport.p_ctrl, irq->pin->pin, irq->saved_pin_cfg) == FSP_SUCCESS) {
            irq->pin_cfg_saved = false;
        }
    }

    irq->open = false;
    irq->trigger = 0;
    irq->pending_count = 0;
    irq->deferred_cleanup = false;
    if (MP_STATE_PORT(machine_pin_irq_obj[irq->pin->irq_channel]) == irq) {
        MP_STATE_PORT(machine_pin_irq_obj[irq->pin->irq_channel]) = NULL;
    }
}

/**
 * @brief   实际配置 irq 的触发和 machine_pin_irq_obj_t 对象内部成员变量维护
 * @param   pin_in  machine_pin_obj_t 对象
 * @param   truffer 触发方式，为 0 时表示关闭触发
 * @return  总是返回 0，遇到错误时已通过异常机制抛出
 */
static mp_uint_t machine_pin_irq_trigger(mp_obj_t pin_in, mp_uint_t trigger)
{
    uint32_t err;

    const machine_pin_obj_t *pin = MP_OBJ_TO_PTR(pin_in);
    machine_pin_irq_obj_t *irq = find_exist_irq_obj(pin);

    if (trigger == 0) {
        machine_pin_irq_release(irq);
        return 0;
    }

    irq->flags = 0;

    machine_pin_irq_obj_t *active_irq = MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]);
    external_irq_trigger_t fsp_trigger = convert_to_fsp_enum(trigger);

    if (active_irq != NULL && active_irq != irq) {
        PIN_IRQ_LOGE("Error occur: irq object in machine_pin_irq_obj is not equal to object in machine_pin_irq_obj_list");
        mp_raise_OSError(MP_EBUSY);
    }

    if (irq->open) {
        fsp_irq_close(irq);
    }

    irq->trigger = trigger;
    irq->cfg.trigger = fsp_trigger;
    irq->hard_callback_count = 0;
    irq->hard_error_count = 0;
    irq->pending_count = 0;
    irq->raw_isr_count = 0;
    irq->overflow_count = 0;

    configure_pin(irq, true);
    err = irq->instance->p_api->open(irq->instance->p_ctrl, &irq->cfg);
    UNLIKE_RAISE_EIO(err, 0, "open() failed: %" PRIu32, err);

    irq->open = true;
    MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]) = irq;
    err = irq->instance->p_api->enable(irq->instance->p_ctrl);
    UNLIKE_RAISE_EIO(err, 0, "enable() failed: %" PRIu32, err);

    return 0;
}

static mp_uint_t machine_pin_irq_info(mp_obj_t pin_in, mp_uint_t info_type)
{
    const machine_pin_obj_t *pin = MP_OBJ_TO_PTR(pin_in);
    machine_pin_irq_obj_t *irq = find_exist_irq_obj(pin);

    if (info_type == MP_IRQ_INFO_FLAGS) {
        return irq->flags;
    }

    if (info_type == MP_IRQ_INFO_TRIGGERS) {
        return irq->trigger;
    }

    return 0;
}

static const mp_irq_methods_t machine_pin_irq_methods = {
    .trigger = machine_pin_irq_trigger,
    .info = machine_pin_irq_info,
};

/**
 * @brief   irq Python 层回调函数异常处理
 * @param   node 调度节点
 */
static void machine_pin_irq_deferred_cleanup(mp_sched_node_t *node)
{
    machine_pin_irq_obj_t *irq = (machine_pin_irq_obj_t *)((uintptr_t)node - offsetof(machine_pin_irq_obj_t, cleanup_node));

    if (!irq->deferred_cleanup) {
        return;
    }

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        machine_pin_irq_release(irq);
        nlr_pop();
    }
    else {
        force_close_irq(irq);
    }
}

/**
 * @brief   hard=False 时的回调入口
 * @param   node 调度节点
 * @details 该函数不在中断上下文中
 */
static void machine_pin_irq_soft_dispatch(mp_sched_node_t *node)
{
    machine_pin_irq_obj_t *irq = (machine_pin_irq_obj_t *)((uintptr_t)node - offsetof(machine_pin_irq_obj_t, soft_node));
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    uint32_t pending_count = irq->pending_count;
    irq->pending_count = 0;
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    for (uint32_t index = 0; index < pending_count; index++) {
        if (!irq->open || irq->base.ishard || irq->base.handler == mp_const_none) {
            break;
        }

        mp_call_function_1_protected(irq->base.handler, irq->base.parent);
    }
}

void machine_pin_irq_callback(external_irq_callback_args_t *p_args)
{
    if (p_args->channel >= MACHINE_PIN_IRQ_CHANNEL_COUNT) {
        return;
    }

    machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj[p_args->channel]);

    if (irq == NULL || !irq->open) {
        return;
    }

    irq->raw_isr_count++;
    irq->flags = irq->trigger;
    if (!irq->base.ishard && irq->base.handler != mp_const_none) {
        if (irq->pending_count < MACHINE_PIN_IRQ_PENDING_MAX) {
            irq->pending_count++;
        }
        else {
            irq->overflow_count++;
        }
        mp_sched_schedule_node(&irq->soft_node, machine_pin_irq_soft_dispatch);
    }
    else if (irq->base.ishard && irq->base.handler != mp_const_none) {
        if (mp_irq_dispatch(irq->base.handler, irq->base.parent, true) < 0) {
            irq->hard_error_count++;
            irq->base.handler = mp_const_none;
            irq->deferred_cleanup = true;
            mp_sched_schedule_node(&irq->cleanup_node, machine_pin_irq_deferred_cleanup);
        }
        else {
            irq->hard_callback_count++;
        }
    }
}

static mp_obj_t machine_pin_irq_stats(mp_obj_t pin_in)
{
    const machine_pin_obj_t *pin = machine_pin_find(pin_in);
    machine_pin_irq_obj_t *irq = find_exist_irq_obj(pin);

    if (irq == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("IRQ is not configured"));
    }

    mp_obj_t stats[] = {
        mp_obj_new_int_from_uint(irq->raw_isr_count),
        mp_obj_new_int_from_uint(irq->overflow_count),
        mp_obj_new_int_from_uint(irq->hard_callback_count),
        mp_obj_new_int_from_uint(irq->hard_error_count),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(stats), stats);
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_irq_stats_obj, machine_pin_irq_stats);

/**
 * @brief   Pin.irq() 的实现
 * @param   n_args   位置参数数量
 * @param   pos_args 位置参数数组
 * @param   kw_args  关键字参数表
 */
static mp_obj_t machine_pin_irq(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum {
        ARG_handler,
        ARG_trigger,
        ARG_priority,
        ARG_wake,
        ARG_hard,
    };

    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_handler, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
        {MP_QSTR_trigger, MP_ARG_INT, {.u_int = MACHINE_PIN_IRQ_FALLING | MACHINE_PIN_IRQ_RISING}},
        {MP_QSTR_priority, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 1}},
        {MP_QSTR_wake, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
        {MP_QSTR_hard, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false}},
    };

    const machine_pin_obj_t *pin = MP_OBJ_TO_PTR(pos_args[0]);

    if (pin->irq_channel < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pin does not support IRQ"));
    }

    if ((size_t)pin->irq_channel >= MACHINE_PIN_IRQ_CHANNEL_COUNT || machine_pin_irq_instances[pin->irq_channel] == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("IRQ channel is not configured"));
    }

    machine_pin_irq_obj_t *irq = find_exist_irq_obj(pin);

    if (irq == NULL) {
        irq = m_new_obj(machine_pin_irq_obj_t);
        memset(irq, 0, sizeof(*irq));
        mp_irq_init(&irq->base, &machine_pin_irq_methods, MP_OBJ_FROM_PTR(pin));

        irq->next = MP_STATE_PORT(machine_pin_irq_obj_list);
        irq->pin = pin;
        irq->instance = machine_pin_irq_instances[pin->irq_channel];
        irq->cfg = *irq->instance->p_cfg;

        MP_STATE_PORT(machine_pin_irq_obj_list) = irq;
    }

    if (n_args > 1 || kw_args->used != 0) {
        mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

        mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

        if (args[ARG_handler].u_obj != mp_const_none && !mp_obj_is_callable(args[ARG_handler].u_obj)) {
            mp_raise_ValueError(MP_ERROR_TEXT("handler must be None or callable"));
        }

        if (args[ARG_trigger].u_int & MACHINE_PIN_IRQ_HIGH_LEVEL) {
            mp_raise_ValueError(MP_ERROR_TEXT("ra8 not support HIGH_LEVEL trigger"));
        }

        if (args[ARG_priority].u_int < MACHINE_PIN_IRQ_PRIORITY_MIN || args[ARG_priority].u_int > MACHINE_PIN_IRQ_PRIORITY_MAX) {
            mp_raise_ValueError(MP_ERROR_TEXT("Invalid IRQ priority, it should be 0-15"));
        }

        if (args[ARG_wake].u_obj != mp_const_none) {
            mp_raise_NotImplementedError(MP_ERROR_TEXT("wake is not supported"));
        }

        //先验证，在旧状态之前
        mp_uint_t trigger = args[ARG_handler].u_obj == mp_const_none ? 0 : args[ARG_trigger].u_int;

        mp_obj_t old_handler = irq->base.handler;
        bool old_ishard = irq->base.ishard;
        uint8_t old_ipl = irq->cfg.ipl;
        mp_uint_t old_trigger = irq->trigger;
        bool old_open = irq->open;

        irq->base.handler = args[ARG_handler].u_obj;
        irq->base.ishard = args[ARG_hard].u_bool;
        irq->cfg.ipl = (uint8_t)args[ARG_priority].u_int;

        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), trigger);
            nlr_pop();
        }
        else {
            //4异常恢复旧配置
            mp_obj_t configure_exception = MP_OBJ_FROM_PTR(nlr.ret_val);

            irq->base.handler = old_handler;
            irq->base.ishard = old_ishard;
            irq->cfg.ipl = old_ipl;

            PIN_IRQ_LOGW("Restore irq config due to machine_pin_irq_trigger() error");
            if (old_open) {
                nlr_buf_t restore_nlr;
                if (nlr_push(&restore_nlr) == 0) {
                    machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), old_trigger);
                    nlr_pop();
                }
                else {
                    PIN_IRQ_LOGE("Force close IRQ due to restore irq config error");
                    force_close_irq(irq);
                }
            }
            else {
                nlr_buf_t cleanup_nlr;
                if (nlr_push(&cleanup_nlr) == 0) {
                    machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), 0);
                    nlr_pop();
                }
                else {
                    PIN_IRQ_LOGE("Force close IRQ due to restore irq config error");
                    force_close_irq(irq);
                }
            }

            nlr_raise(configure_exception);
        }
    }

    return MP_OBJ_FROM_PTR(irq);
}

void machine_pin_irq_deinit(void)
{
    for (size_t channel = 0; channel < MACHINE_PIN_IRQ_CHANNEL_COUNT; ++channel) {
        machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj[channel]);

        if (irq == NULL) {
            continue;
        }

        machine_pin_irq_release(irq);

        irq->base.handler = mp_const_none;
        irq->base.ishard = false;
    }

    MP_STATE_PORT(machine_pin_irq_obj_list) = NULL;
}

MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_irq_fun_obj, 1, machine_pin_irq);
