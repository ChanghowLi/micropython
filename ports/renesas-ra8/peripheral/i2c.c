#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "hal_data.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "semphr.h"
#include "i2c.h"

#ifndef __I2C_DEBUG
#define __I2C_DEBUG 1
#endif

#if __I2C_DEBUG
#include "utils/log.h"
#define I2C_LOGD(msg, ...)      LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGI(msg, ...)      LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGW(msg, ...)      LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGE(msg, ...)      LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define I2C_LOGD(msg, ...)
#define I2C_LOGI(msg, ...)
#define I2C_LOGW(msg, ...)
#define I2C_LOGE(msg, ...)
#endif

#define I2C_CLOCK_FALL_TIME_NS          (120U)
#define I2C_CLOCK_MAX_BRL_BRH           (31U)
#define I2C_CLOCK_MIN_BRL_BRH           (2U)
#define I2C_CLOCK_NOISE_FILTER_STAGES   (1U)
#define I2C_CLOCK_RISE_TIME_NS          (120U)
#define I2C_MAX_FREQ_HZ                 (1000000U)

typedef struct _ra8_i2c_state_t {
    const i2c_master_instance_t *instance;
    i2c_master_cfg_t cfg;
    iic_master_extended_cfg_t extended_cfg;
    volatile i2c_master_event_t event;
    volatile int transfer_error;
    volatile uint32_t acked;
    volatile bool data_nack;
    bool opened;
    bool restart_pending;
    uint16_t current_addr;
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
} ra8_i2c_state_t;

static ra8_i2c_state_t ra8_i2c_states[] = {
    {.instance = NULL},
    {.instance = &g_i2c_master1},
    {.instance = &g_i2c_master2},
};

static ra8_i2c_state_t *ra8_i2c_get_state(uint32_t id)
{
    if (id >= MP_ARRAY_SIZE(ra8_i2c_states)) {
        return NULL;
    }

    if (ra8_i2c_states[id].instance == NULL) {
        return NULL;
    }

    return &ra8_i2c_states[id];
}

static int ra8_i2c_fsp_error(fsp_err_t error)
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
        default:
            return MP_EIO;
    }
}

static void ra8_i2c_capture_error(ra8_i2c_state_t *state, iic_master_instance_ctrl_t *ctrl)
{
    uint8_t status = ctrl->p_reg->ICSR2;
    if (status & R_IIC0_ICSR2_TMOF_Msk) {
        state->transfer_error = MP_ETIMEDOUT;
        state->data_nack = false;
    } else if (status & R_IIC0_ICSR2_AL_Msk) {
        state->transfer_error = MP_EBUSY;
        state->data_nack = false;
    } else if ((status & R_IIC0_ICSR2_NACKF_Msk) &&
               state->transfer_error == 0 && !state->data_nack) {
        if (ctrl->read || !ctrl->p_reg->ICCR2_b.MST) {
            state->transfer_error = MP_ENODEV;
            return;
        }

        /* With NACKE enabled, TDRE stays clear for a queued, untransmitted
         * byte when NACK suspends transmission. FSP counts bytes loaded, not
         * ACKs. Exclude that queued byte and the byte that received NACK.
         * FSP uses one priority for TXI/ERI; DTC is rejected in i2c_init(). */
        uint32_t transmitted = ctrl->loaded;
        if (!(status & R_IIC0_ICSR2_TDRE_Msk) && transmitted != 0) {
            --transmitted;
        }
        if (transmitted == 0) {
            state->transfer_error = MP_ENODEV;
        } else {
            state->acked = transmitted - 1;
            state->data_nack = true;
        }
    }
}

/* Linker wrapping preserves the FSP ISR while observing error flags before
 * it clears them and resets loaded/remain. Keep both build systems in sync. */
void __real_iic_master_eri_isr(void);
void __wrap_iic_master_eri_isr(void);

void __wrap_iic_master_eri_isr(void)
{
    iic_master_instance_ctrl_t *ctrl = R_FSP_IsrContextGet(R_FSP_CurrentIrqGet());
    for (size_t index = 1; index < MP_ARRAY_SIZE(ra8_i2c_states); ++index) {
        ra8_i2c_state_t *state = &ra8_i2c_states[index];
        if (state->opened && state->instance->p_ctrl == (i2c_master_ctrl_t *)ctrl) {
            ra8_i2c_capture_error(state, ctrl);
            break;
        }
    }
    __real_iic_master_eri_isr();
}

static int ra8_i2c_abort(ra8_i2c_state_t *state)
{
    fsp_err_t error = state->instance->p_api->abort(state->instance->p_ctrl);
    if (error == FSP_SUCCESS) {
        state->restart_pending = false;
    }
    return ra8_i2c_fsp_error(error);
}

static int ra8_i2c_clock_configure(uint32_t freq, i2c_master_cfg_t *cfg, iic_master_extended_cfg_t *extended_cfg)
{
    if (freq == 0 || freq > I2C_MAX_FREQ_HZ) {
        return MP_EINVAL;
    }

    uint32_t pclkb = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKB);
    if (pclkb == 0) {
        return MP_EIO;
    }

    bool found = false;
    uint32_t best_freq = 0;
    uint64_t best_duty_error = UINT64_MAX;
    uint8_t best_cks = 0;
    uint8_t best_brh = 0;
    uint8_t best_brl = 0;

    for (uint8_t cks = 0; cks <= 7; ++cks) {
        uint32_t iic_clock = pclkb >> cks;
        if (iic_clock == 0) {
            continue;
        }

        for (uint8_t brh = I2C_CLOCK_MIN_BRL_BRH; brh <= I2C_CLOCK_MAX_BRL_BRH; ++brh) {
            for (uint8_t brl = I2C_CLOCK_MIN_BRL_BRH; brl <= I2C_CLOCK_MAX_BRL_BRH; ++brl) {
                uint32_t high_cycles = cks == 0 ? brh + 3U + 3U * I2C_CLOCK_NOISE_FILTER_STAGES : brh + 2U + I2C_CLOCK_NOISE_FILTER_STAGES;
                uint32_t low_cycles = cks == 0 ? brl + 3U + I2C_CLOCK_NOISE_FILTER_STAGES : brl + 2U + I2C_CLOCK_NOISE_FILTER_STAGES;
                uint64_t period_num = (uint64_t)(high_cycles + low_cycles) * 1000000000ULL + (uint64_t)(I2C_CLOCK_RISE_TIME_NS + I2C_CLOCK_FALL_TIME_NS) * iic_clock;
                uint64_t freq_num = (uint64_t)iic_clock * 1000000000ULL;
                uint32_t actual_freq = (uint32_t)(freq_num / period_num);

                if (actual_freq == 0 || actual_freq > freq) {
                    continue;
                }

                uint64_t high_num = (uint64_t)high_cycles * 1000000000ULL + (uint64_t)I2C_CLOCK_RISE_TIME_NS * iic_clock;
                uint64_t twice_high = high_num * 2ULL;
                uint64_t duty_error = twice_high > period_num ? twice_high - period_num : period_num - twice_high;

                if (!found || actual_freq > best_freq || (actual_freq == best_freq && duty_error < best_duty_error)) {
                    found = true;
                    best_freq = actual_freq;
                    best_duty_error = duty_error;
                    best_cks = cks;
                    best_brh = brh;
                    best_brl = brl;
                }
            }
        }
    }

    if (!found) {
        return MP_EINVAL;
    }

    extended_cfg->clock_settings.cks_value = best_cks;
    extended_cfg->clock_settings.brh_value = best_brh;
    extended_cfg->clock_settings.brl_value = best_brl;
    cfg->rate = freq <= I2C_MASTER_RATE_STANDARD ? I2C_MASTER_RATE_STANDARD : (freq <= I2C_MASTER_RATE_FAST ? I2C_MASTER_RATE_FAST : I2C_MASTER_RATE_FASTPLUS);
    return 0;
}

int i2c_validate_freq(uint32_t id, uint32_t freq)
{
    ra8_i2c_state_t *state = ra8_i2c_get_state(id);
    if (state == NULL) {
        return MP_ENODEV;
    }

    i2c_master_cfg_t cfg = *state->instance->p_cfg;
    iic_master_extended_cfg_t extended_cfg = *(const iic_master_extended_cfg_t *)cfg.p_extend;
    return ra8_i2c_clock_configure(freq, &cfg, &extended_cfg);
}

void i2c_master_callback(i2c_master_callback_args_t *p_args)
{
    if (p_args == NULL || p_args->p_context == NULL) {
        return;
    }

    if (p_args->event != I2C_MASTER_EVENT_TX_COMPLETE &&
        p_args->event != I2C_MASTER_EVENT_RX_COMPLETE &&
        p_args->event != I2C_MASTER_EVENT_ABORTED) {
        return;
    }

    ra8_i2c_state_t *state = (ra8_i2c_state_t *)p_args->p_context;
    state->event = p_args->event;

    if (state->completion == NULL) {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(state->completion, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

bool i2c_deinit(uint32_t id)
{
    ra8_i2c_state_t *state = ra8_i2c_get_state(id);

    if (state == NULL) {
        I2C_LOGE("invalid I2C id: %lu", (unsigned long)id);
        return false;
    }

    I2C_LOGD("id=%lu, opened=%d", (unsigned long)id, state->opened);

    if (!state->opened) {
        return true;
    }

    fsp_err_t error = state->instance->p_api->close(state->instance->p_ctrl);
    if (error != FSP_SUCCESS) {
        I2C_LOGE("failed to close I2C: id=%lu, error=%d", (unsigned long)id, (int)error);
        return false;
    }

    state->event = (i2c_master_event_t)0;
    state->restart_pending = false;
    state->current_addr = 0;
    state->opened = false;
    return true;
}

int i2c_init(uint32_t id, uint32_t freq)
{
    ra8_i2c_state_t *state = ra8_i2c_get_state(id);

    if (state == NULL) {
        return MP_ENODEV;
    }

    i2c_master_cfg_t new_cfg = *state->instance->p_cfg;
    /* ACK accounting requires the interrupt-driven byte counters. */
    if (new_cfg.p_transfer_tx != NULL || new_cfg.p_transfer_rx != NULL) {
        return MP_EINVAL;
    }
    iic_master_extended_cfg_t new_extended_cfg = *(const iic_master_extended_cfg_t *)state->instance->p_cfg->p_extend;
    new_cfg.p_extend = &new_extended_cfg;
    new_cfg.p_callback = i2c_master_callback;
    new_cfg.p_context = state;

    int clock_error = ra8_i2c_clock_configure(freq, &new_cfg, &new_extended_cfg);
    if (clock_error != 0) {
        return clock_error;
    }

    if (!i2c_deinit(id)) {
        return MP_EIO;
    }

    state->cfg = new_cfg;
    state->extended_cfg = new_extended_cfg;
    state->cfg.p_extend = &state->extended_cfg;

    if (state->completion == NULL) {
        state->completion = xSemaphoreCreateBinaryStatic(&state->completion_storage);
        if (state->completion == NULL) {
            return MP_ENOMEM;
        }
    }

    while (xSemaphoreTake(state->completion, 0) == pdTRUE) {
    }
    state->event = (i2c_master_event_t)0;
    state->restart_pending = false;
    state->current_addr = 0;

    fsp_err_t error = state->instance->p_api->open(state->instance->p_ctrl, &state->cfg);
    if (error != FSP_SUCCESS) {
        return ra8_i2c_fsp_error(error);
    }

    state->opened = true;
    return 0;
}

int i2c_transfer(uint32_t id, uint16_t addr, size_t len, uint8_t *buf, bool read, bool stop, uint32_t timeout_ms, size_t *transferred)
{
    *transferred = 0;
    ra8_i2c_state_t *state = ra8_i2c_get_state(id);

    if (state == NULL || !state->opened) {
        return MP_ENODEV;
    }

    if (addr > 0x7fU || len > UINT32_MAX) {
        return MP_EINVAL;
    }

    if (read && len == 0) {
        /* A register read may already have sent its address without STOP. */
        if (state->restart_pending) {
            int error = ra8_i2c_abort(state);
            if (error != 0) {
                return error;
            }
        }
        return MP_EINVAL;
    }

    if (len > 0 && buf == NULL) {
        return MP_EINVAL;
    }

    /* Address-only writes are used by scan(); FSP requires a non-NULL buffer. */
    uint8_t dummy = 0;
    uint8_t *transfer_buf = len == 0 ? &dummy : buf;

    if (state->restart_pending) {
        if (addr != state->current_addr) {
            int error = ra8_i2c_abort(state);
            return error != 0 ? error : MP_EBUSY;
        }
    } else {
        fsp_err_t error = state->instance->p_api->slaveAddressSet(state->instance->p_ctrl, addr, I2C_MASTER_ADDR_MODE_7BIT);
        if (error != FSP_SUCCESS) {
            return ra8_i2c_fsp_error(error);
        }
        state->current_addr = addr;
    }

    while (xSemaphoreTake(state->completion, 0) == pdTRUE) {
    }
    state->event = (i2c_master_event_t)0;

    state->transfer_error = 0;
    state->acked = 0;
    state->data_nack = false;

    bool restart = !stop;
    fsp_err_t error;

    if (read) {
        error = state->instance->p_api->read(state->instance->p_ctrl, transfer_buf, (uint32_t)len, restart);
    } else {
        error = state->instance->p_api->write(state->instance->p_ctrl, transfer_buf, (uint32_t)len, restart);
    }

    if (error != FSP_SUCCESS) {
        int abort_error = ra8_i2c_abort(state);
        return abort_error != 0 ? abort_error : ra8_i2c_fsp_error(error);
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ticks == 0 && timeout_ms > 0) {
        timeout_ticks = 1;
    }

    if (xSemaphoreTake(state->completion, timeout_ticks) != pdTRUE) {
        I2C_LOGE("I2C transfer timeout: id=%lu", (unsigned long)id);
        int abort_error = ra8_i2c_abort(state);
        return abort_error != 0 ? abort_error : MP_ETIMEDOUT;
    }

    switch (state->event) {
        case I2C_MASTER_EVENT_TX_COMPLETE:
        case I2C_MASTER_EVENT_RX_COMPLETE:
            state->restart_pending = restart;
            *transferred = len;
            return 0;

        case I2C_MASTER_EVENT_ABORTED:
            state->restart_pending = false;
            if (state->transfer_error != 0) {
                return state->transfer_error;
            }
            if (!read && state->data_nack) {
                *transferred = state->acked;
                return 0;
            }
            return MP_EIO;

        default:
            state->restart_pending = false;
            return MP_EIO;
    }
}
