#include <string.h>

#include "hal_data.h"
#include "pin.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "shared/runtime/mpirq.h"

#define MACHINE_PIN_IRQ_CHANNEL_COUNT (32)
#define MACHINE_PIN_IRQ_PRIORITY_MAX (15)
#define MACHINE_PIN_IRQ_PRIORITY_MIN (1)

typedef struct _machine_pin_irq_obj_t {
    mp_irq_obj_t base;
    struct _machine_pin_irq_obj_t *next;
    const machine_pin_obj_t *pin;
    const external_irq_instance_t *instance;
    external_irq_cfg_t cfg;
    mp_uint_t flags;
    mp_uint_t trigger;
    uint32_t saved_pin_cfg;
    bool open;
    bool pin_cfg_saved;
    bool pin_taken;
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

static machine_pin_irq_obj_t *machine_pin_irq_find(const machine_pin_obj_t *pin)
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

bool machine_pin_irq_is_active(const machine_pin_obj_t *pin)
{
    if (pin->irq_channel < 0 || (size_t)pin->irq_channel >= MACHINE_PIN_IRQ_CHANNEL_COUNT) {
        return false;
    }

    machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]);

    return irq != NULL && irq->pin == pin && irq->open;
}

static void machine_pin_irq_raise_fsp_error(fsp_err_t err)
{
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
}

static external_irq_trigger_t machine_pin_irq_fsp_trigger(mp_uint_t trigger)
{
    switch (trigger) {
        case MACHINE_PIN_IRQ_FALLING:
            return EXTERNAL_IRQ_TRIGGER_FALLING;

        case MACHINE_PIN_IRQ_RISING:
            return EXTERNAL_IRQ_TRIGGER_RISING;

        case MACHINE_PIN_IRQ_FALLING | MACHINE_PIN_IRQ_RISING:
            return EXTERNAL_IRQ_TRIGGER_BOTH_EDGE;

        case MACHINE_PIN_IRQ_LOW_LEVEL:
            return EXTERNAL_IRQ_TRIGGER_LEVEL_LOW;

        case MACHINE_PIN_IRQ_HIGH_LEVEL:
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%q is not supported"), MP_QSTR_IRQ_HIGH_LEVEL);

        default:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid IRQ trigger"));
    }
}

static void machine_pin_irq_configure_pin(machine_pin_irq_obj_t *irq, bool enable)
{
    const machine_pin_obj_t *pin = irq->pin;
    uint32_t port = (uint32_t)pin->pin >> 8;
    uint32_t bit = (uint32_t)pin->pin & 0xffU;

    if (enable) {
        if (!irq->pin_cfg_saved) {
            irq->saved_pin_cfg = R_PFS->PORT[port].PIN[bit].PmnPFS;
            irq->pin_cfg_saved = true;
        }

        uint32_t cfg = irq->saved_pin_cfg;

        cfg &= ~((uint32_t)(IOPORT_CFG_ANALOG_ENABLE | IOPORT_CFG_PERIPHERAL_PIN |
            IOPORT_CFG_PORT_DIRECTION_OUTPUT | R_PFS_PORT_PIN_PmnPFS_PSEL_Msk));
        cfg |= IOPORT_CFG_IRQ_ENABLE | IOPORT_CFG_PORT_DIRECTION_INPUT;

        machine_pin_irq_raise_fsp_error(R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, cfg));
    } else if (irq->pin_cfg_saved) {
        machine_pin_irq_raise_fsp_error(R_IOPORT_PinCfg(g_ioport.p_ctrl, pin->pin, irq->saved_pin_cfg));
        irq->pin_cfg_saved = false;
    }
}

static void machine_pin_irq_close(machine_pin_irq_obj_t *irq)
{
    if (irq->open) {
        machine_pin_irq_raise_fsp_error(irq->instance->p_api->disable(irq->instance->p_ctrl));
        machine_pin_irq_raise_fsp_error(irq->instance->p_api->close(irq->instance->p_ctrl));
        irq->open = false;
    }

    machine_pin_irq_configure_pin(irq, false);
}

static void machine_pin_irq_release(machine_pin_irq_obj_t *irq)
{
    size_t channel = (size_t)irq->pin->irq_channel;

    if (MP_STATE_PORT(machine_pin_irq_obj[channel]) == irq) {
        machine_pin_irq_close(irq);
        MP_STATE_PORT(machine_pin_irq_obj[channel]) = NULL;
    }

    if (irq->pin_taken) {
        machine_pin_give(irq->pin->pin);
        irq->pin_taken = false;
    }

    irq->flags = 0;
    irq->trigger = 0;
}

static void machine_pin_irq_force_inactive(machine_pin_irq_obj_t *irq)
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

    if (irq->pin_taken) {
        machine_pin_give(irq->pin->pin);
        irq->pin_taken = false;
    }

    irq->open = false;
    irq->trigger = 0;
    if (MP_STATE_PORT(machine_pin_irq_obj[irq->pin->irq_channel]) == irq) {
        MP_STATE_PORT(machine_pin_irq_obj[irq->pin->irq_channel]) = NULL;
    }
}

static mp_uint_t machine_pin_irq_trigger(mp_obj_t pin_in, mp_uint_t trigger)
{
    const machine_pin_obj_t *pin = MP_OBJ_TO_PTR(pin_in);
    machine_pin_irq_obj_t *irq = machine_pin_irq_find(pin);

    if (trigger == 0) {
        machine_pin_irq_release(irq);
        return 0;
    }

    irq->flags = 0;

    machine_pin_irq_obj_t *active_irq = MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]);
    external_irq_trigger_t fsp_trigger = machine_pin_irq_fsp_trigger(trigger);

    if (active_irq != NULL && active_irq != irq) {
        mp_raise_OSError(MP_EBUSY);
    }

    if (!irq->pin_taken) {
        if (!machine_pin_take(pin->pin)) {
            mp_raise_OSError(MP_EBUSY);
        }

        irq->pin_taken = true;
    }

    if (irq->open) {
        machine_pin_irq_close(irq);
    }

    irq->trigger = trigger;
    irq->cfg.trigger = fsp_trigger;

    machine_pin_irq_configure_pin(irq, true);

    machine_pin_irq_raise_fsp_error(irq->instance->p_api->open(irq->instance->p_ctrl, &irq->cfg));

    irq->open = true;
    MP_STATE_PORT(machine_pin_irq_obj[pin->irq_channel]) = irq;

    machine_pin_irq_raise_fsp_error(irq->instance->p_api->enable(irq->instance->p_ctrl));

    return 0;
}

static mp_uint_t machine_pin_irq_info(mp_obj_t pin_in, mp_uint_t info_type)
{
    const machine_pin_obj_t *pin = MP_OBJ_TO_PTR(pin_in);
    machine_pin_irq_obj_t *irq = machine_pin_irq_find(pin);

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

void machine_pin_irq_callback(external_irq_callback_args_t *p_args)
{
    if (p_args->channel >= MACHINE_PIN_IRQ_CHANNEL_COUNT) {
        return;
    }

    machine_pin_irq_obj_t *irq = MP_STATE_PORT(machine_pin_irq_obj[p_args->channel]);

    if (irq == NULL || !irq->open) {
        return;
    }

    irq->flags = irq->trigger;
    mp_irq_handler(&irq->base);
}

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

    machine_pin_irq_obj_t *irq = machine_pin_irq_find(pin);

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

        if (args[ARG_priority].u_int < MACHINE_PIN_IRQ_PRIORITY_MIN || args[ARG_priority].u_int > MACHINE_PIN_IRQ_PRIORITY_MAX) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid IRQ priority"));
        }

        if (args[ARG_wake].u_obj != mp_const_none) {
            mp_raise_NotImplementedError(MP_ERROR_TEXT("wake is not supported"));
        }

        //先验证，在旧状态之前
        mp_uint_t trigger = args[ARG_handler].u_obj == mp_const_none ? 0 : args[ARG_trigger].u_int;

        if (trigger != 0) {
            machine_pin_irq_fsp_trigger(trigger);
        }

        mp_obj_t old_handler = irq->base.handler;
        bool old_ishard = irq->base.ishard;
        uint8_t old_ipl = irq->cfg.ipl;
        mp_uint_t old_trigger = irq->trigger;
        bool old_open = irq->open;

        irq->base.handler = args[ARG_handler].u_obj;
        irq->base.ishard = args[ARG_hard].u_bool;
        irq->cfg.ipl = (uint8_t)(16 - args[ARG_priority].u_int);

        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), trigger);
            nlr_pop();
        } else {
            //4异常恢复旧配置
            mp_obj_t configure_exception = MP_OBJ_FROM_PTR(nlr.ret_val);

            irq->base.handler = old_handler;
            irq->base.ishard = old_ishard;
            irq->cfg.ipl = old_ipl;

            if (old_open) {
                nlr_buf_t restore_nlr;
                if (nlr_push(&restore_nlr) == 0) {
                    machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), old_trigger);
                    nlr_pop();
                } else {
                    machine_pin_irq_force_inactive(irq);
                }
            } else {
                nlr_buf_t cleanup_nlr;
                if (nlr_push(&cleanup_nlr) == 0) {
                    machine_pin_irq_trigger(MP_OBJ_FROM_PTR(pin), 0);
                    nlr_pop();
                } else {
                    machine_pin_irq_force_inactive(irq);
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

MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_irq_obj, 1, machine_pin_irq);
