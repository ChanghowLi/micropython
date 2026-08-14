#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "hal_data.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "semphr.h"
#include "spi.h"

#define RA8_SCI_SPI_FIRST_ID         (11)
#define RA8_SCI_SPI_LAST_ID          (18)

typedef struct _ra8_spi_state_t 
{
    const spi_instance_t *instance;
    spi_cfg_t cfg;
    spi_b_extended_cfg_t extended_cfg;
    volatile spi_event_t event;
    bool opened;
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
} ra8_spi_state_t;

static ra8_spi_state_t ra8_spi_states[] = {
    {.instance = &g_spi0},
    {.instance = &g_spi1},
};

typedef struct _ra8_sci_spi_state_t
{
    sci_b_spi_instance_ctrl_t ctrl;
    spi_cfg_t cfg;
    sci_b_spi_extended_cfg_t extended_cfg;
    spi_instance_t instance;
    volatile spi_event_t event;
    bool opened;
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
} ra8_sci_spi_state_t;

#define RA8_SPI_COUNT (sizeof(ra8_spi_states) / sizeof(ra8_spi_states[0]))

static ra8_spi_state_t *ra8_spi_get_state(uint32_t id) 
{
    if (id >= RA8_SPI_COUNT) {
        return NULL;
    }

    return &ra8_spi_states[id];
}

static int ra8_spi_fsp_error(fsp_err_t error) 
{
    if (error == FSP_SUCCESS) {
        return 0;
    }

    return MP_EIO;
}

void spi_callback(spi_callback_args_t *p_args) 
{
    if (p_args->p_context == NULL) {
        return;
    }

    ra8_spi_state_t *state = (ra8_spi_state_t *)p_args->p_context;

    state->event = p_args->event;

    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(state->completion, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void ra8_sci_spi_callback(spi_callback_args_t *p_args)
{
    if (p_args->p_context == NULL) {
        return;
    }

    ra8_sci_spi_state_t *state = (ra8_sci_spi_state_t *)p_args->p_context;

    state->event = p_args->event;

    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(state->completion, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

bool spi_deinit(uint32_t id)
{
    if (id >= RA8_SCI_SPI_FIRST_ID && id <= RA8_SCI_SPI_LAST_ID) {
        uint32_t index = id - RA8_SCI_SPI_FIRST_ID;

        ra8_sci_spi_state_t *state = MP_STATE_PORT(ra8_sci_spi_states[index]);

        if (state == NULL) {
            return false;
        }

        // 必须先关闭硬件，停止相关中断。
        if (state->opened) {
            fsp_err_t error = state->instance.p_api->close(state->instance.p_ctrl);

            if (error != FSP_SUCCESS) {
                return false;
            }

            state->opened = false;
        }

        // 销毁使用静态存储创建的信号量对象。
        if (state->completion != NULL) {
            vSemaphoreDelete(state->completion);
            state->completion = NULL;
        }

        state->event = (spi_event_t)0;

        // 先断开 GC 根指针，再释放状态。
        MP_STATE_PORT(ra8_sci_spi_states[index]) = NULL;
        m_del(ra8_sci_spi_state_t, state, 1);

        return true;
    }

    // 专用 SPI 释放逻辑
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL || !state->opened) {
        return false;
    }

    state->instance->p_api->close(state->instance->p_ctrl);

    state->event = (spi_event_t)0;
    state->opened = false;
    return true;
}

int spi_init(uint32_t id, uint32_t baudrate, uint8_t polarity, uint8_t phase, uint8_t bits, uint8_t firstbit) 
{
    if (baudrate == 0 || bits != 8 || polarity > 1 || phase > 1 || firstbit > 1) {
        return MP_EINVAL;
    }

    if (id >= RA8_SCI_SPI_FIRST_ID && id <= RA8_SCI_SPI_LAST_ID) {
        uint32_t index = id - RA8_SCI_SPI_FIRST_ID;
        ra8_sci_spi_state_t *state = MP_STATE_PORT(ra8_sci_spi_states[index]);

        if (state == NULL) {
            state = m_new0(ra8_sci_spi_state_t, 1);

            memcpy(&state->instance, &g_sci_spi1, sizeof(spi_instance_t));
            memcpy(&state->cfg, g_sci_spi1.p_cfg, sizeof(spi_cfg_t));
            memcpy(&state->extended_cfg, g_sci_spi1.p_cfg->p_extend, sizeof(sci_b_spi_extended_cfg_t));

            state->instance.p_ctrl = &state->ctrl;
            state->instance.p_cfg = &state->cfg;
            state->cfg.channel = index + 1;
            state->cfg.p_extend = &state->extended_cfg;

            state->cfg.p_callback = ra8_sci_spi_callback;
            state->cfg.p_context = state;

            MP_STATE_PORT(ra8_sci_spi_states[index]) = state;  //创建完把这个 state 放进对应数组槽位
        }

        if (state->completion == NULL) {
            state->completion = xSemaphoreCreateBinaryStatic(&state->completion_storage);

            if (state->completion == NULL) {
                return MP_ENOMEM;
            }
        }

        if (state->opened) {
            fsp_err_t error = state->instance.p_api->close(state->instance.p_ctrl);

            if (error != FSP_SUCCESS) {
                return ra8_spi_fsp_error(error);
            }

            state->opened = false;
        }

        state->cfg.clk_phase = phase == 0 ? SPI_CLK_PHASE_EDGE_ODD : SPI_CLK_PHASE_EDGE_EVEN;
        state->cfg.clk_polarity = polarity == 0 ? SPI_CLK_POLARITY_LOW : SPI_CLK_POLARITY_HIGH;
        state->cfg.bit_order = firstbit == 0 ? SPI_BIT_ORDER_MSB_FIRST : SPI_BIT_ORDER_LSB_FIRST;

        fsp_err_t error = R_SCI_B_SPI_CalculateBitrate(baudrate, state->extended_cfg.clock_source, &state->extended_cfg.clk_div);

        if (error != FSP_SUCCESS) {
            return ra8_spi_fsp_error(error);
        }

        state->event = (spi_event_t)0;

        error = state->instance.p_api->open(state->instance.p_ctrl, &state->cfg);

        if (error != FSP_SUCCESS) {
            return ra8_spi_fsp_error(error);
        }

        state->opened = true;
        return 0;
    }

    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL) { 
        return MP_ENODEV;
    }

    if (state->completion == NULL) {
        state->completion = xSemaphoreCreateBinaryStatic(&state->completion_storage);

        if (state->completion == NULL) { 
            return MP_ENOMEM;
        }
    }

    if (state->opened) {
        spi_deinit(id);
    }

    state->cfg = *state->instance->p_cfg;
    state->extended_cfg = *(const spi_b_extended_cfg_t *)state->instance->p_cfg->p_extend;

    state->cfg.clk_phase = phase == 0 ? SPI_CLK_PHASE_EDGE_ODD : SPI_CLK_PHASE_EDGE_EVEN;
    state->cfg.clk_polarity = polarity == 0 ? SPI_CLK_POLARITY_LOW : SPI_CLK_POLARITY_HIGH;
    state->cfg.bit_order = firstbit == 0 ? SPI_BIT_ORDER_MSB_FIRST : SPI_BIT_ORDER_LSB_FIRST;

    state->cfg.p_callback = spi_callback;
    state->cfg.p_context = state;
    state->cfg.p_extend = &state->extended_cfg;

    fsp_err_t error = R_SPI_B_CalculateBitrate(baudrate, state->extended_cfg.clock_source, &state->extended_cfg.spck_div);

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    state->event = (spi_event_t)0;

    error = state->instance->p_api->open(state->instance->p_ctrl, &state->cfg);

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    state->opened = true;
    return 0;
}

int spi_transfer(uint32_t id, size_t len, const uint8_t *src, uint8_t *dest, uint32_t timeout_ms) 
{
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL || !state->opened) {
        return MP_ENODEV;
    }

    if (len == 0) {
        return 0;
    }

    if (len > UINT32_MAX) {
        return MP_EINVAL;
    }

    if (src == NULL) {
        return MP_EINVAL;
    }

    state->event = (spi_event_t)0;

    while (xSemaphoreTake(state->completion, 0) == pdTRUE) {
    }

    fsp_err_t error;

    if (dest != NULL) {
        // 需要接收，所以同时发送和接收
        error = state->instance->p_api->writeRead(state->instance->p_ctrl, src, dest, (uint32_t)len, SPI_BIT_WIDTH_8_BITS);
    } else {
        // 不需要接收，只发送
        error = state->instance->p_api->write(state->instance->p_ctrl, src, (uint32_t)len, SPI_BIT_WIDTH_8_BITS);
    }

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    if (xSemaphoreTake(state->completion, timeout_ticks) != pdTRUE) {
        spi_deinit(id);
        return MP_ETIMEDOUT;
    }

    mp_handle_pending(true);

    if (state->event != SPI_EVENT_TRANSFER_COMPLETE) {
        return MP_EIO;
    }

    return 0;
}
