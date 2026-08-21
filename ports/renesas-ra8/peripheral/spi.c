#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "hal_data.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "semphr.h"
#include "spi.h"

#define RA8_SCI_SPI_FIRST_ID (11)
#define RA8_SPI_COUNT MP_ARRAY_SIZE(ra8_spi_states)

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
    const spi_instance_t *instance;
    spi_cfg_t cfg;
    sci_b_spi_extended_cfg_t extended_cfg;
    volatile spi_event_t event;
    bool opened;
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
} ra8_sci_spi_state_t;

static ra8_sci_spi_state_t ra8_sci_spi_states[] = {
    {.instance = &g_sci_spi1},
    {.instance = &g_sci_spi2},
    {.instance = NULL},
    {.instance = &g_sci_spi4},
    {.instance = &g_sci_spi5},
    {.instance = &g_sci_spi6},
    {.instance = NULL},
    {.instance = &g_sci_spi8},
};

static ra8_sci_spi_state_t *ra8_sci_spi_get_state(uint32_t id)
{
    if (id < RA8_SCI_SPI_FIRST_ID) {
        return NULL;
    }

    uint32_t index = id - RA8_SCI_SPI_FIRST_ID;

    if (index >= MP_ARRAY_SIZE(ra8_sci_spi_states)) {
        return NULL;
    }

    return &ra8_sci_spi_states[index];
}

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

void sci_spi_callback(spi_callback_args_t *p_args)
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
    ra8_sci_spi_state_t *sci_state = ra8_sci_spi_get_state(id);

    if (sci_state != NULL) {
        if (!sci_state->opened) {
            return false;
        }

        fsp_err_t error = sci_state->instance->p_api->close(sci_state->instance->p_ctrl);

        if (error != FSP_SUCCESS) {
            return false;
        }

        sci_state->event = (spi_event_t)0;
        sci_state->opened = false;
        return true;
    }

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

    ra8_sci_spi_state_t *sci_state = ra8_sci_spi_get_state(id);

    if (sci_state != NULL) {
        if (sci_state->opened) {
            fsp_err_t error = sci_state->instance->p_api->close(sci_state->instance->p_ctrl);

            if (error != FSP_SUCCESS) {
                return ra8_spi_fsp_error(error);
            }

            sci_state->opened = false;
        }

        sci_state->cfg = *sci_state->instance->p_cfg;
        sci_state->extended_cfg = *(const sci_b_spi_extended_cfg_t *)sci_state->instance->p_cfg->p_extend;

        sci_state->cfg.clk_phase = phase == 0 ? SPI_CLK_PHASE_EDGE_ODD : SPI_CLK_PHASE_EDGE_EVEN;
        sci_state->cfg.clk_polarity = polarity == 0 ? SPI_CLK_POLARITY_LOW : SPI_CLK_POLARITY_HIGH;
        sci_state->cfg.bit_order = firstbit == 0 ? SPI_BIT_ORDER_MSB_FIRST : SPI_BIT_ORDER_LSB_FIRST;
        sci_state->cfg.p_callback = sci_spi_callback;
        sci_state->cfg.p_context = sci_state;
        sci_state->cfg.p_extend = &sci_state->extended_cfg;

        if (sci_state->completion == NULL) {
            sci_state->completion = xSemaphoreCreateBinaryStatic(&sci_state->completion_storage);

            if (sci_state->completion == NULL) {
                return MP_ENOMEM;
            }
        }

        fsp_err_t error = R_SCI_B_SPI_CalculateBitrate(baudrate, sci_state->extended_cfg.clock_source, &sci_state->extended_cfg.clk_div);

        if (error != FSP_SUCCESS) {
            return ra8_spi_fsp_error(error);
        }

        sci_state->event = (spi_event_t)0;

        error = sci_state->instance->p_api->open(sci_state->instance->p_ctrl, &sci_state->cfg);

        if (error != FSP_SUCCESS) {
            return ra8_spi_fsp_error(error);
        }

        sci_state->opened = true;
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

    //设置CPHA 时钟相位，决定在哪个时钟边沿采样数据
    state->cfg.clk_phase = phase == 0 ? SPI_CLK_PHASE_EDGE_ODD : SPI_CLK_PHASE_EDGE_EVEN;
    //设置CPOL 时钟极性，决定 SCK 空闲时是低电平还是高电平
    state->cfg.clk_polarity = polarity == 0 ? SPI_CLK_POLARITY_LOW : SPI_CLK_POLARITY_HIGH;
    //设置数据位顺序，决定先发送最高位 MSB 还是最低位 LSB
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
    const spi_instance_t *instance;
    volatile spi_event_t *event;
    SemaphoreHandle_t completion;
    ra8_sci_spi_state_t *sci_state = ra8_sci_spi_get_state(id);

    //SPI id 是否有效，以及对应的 FSP SPI 实例是否已经打开、可以进行传输
    if (sci_state != NULL) {
        if (!sci_state->opened) {
            return MP_ENODEV;
        }

        instance = sci_state->instance;
        event = &sci_state->event;
        completion = sci_state->completion;
    } else {
        ra8_spi_state_t *state = ra8_spi_get_state(id);

        if (state == NULL || !state->opened) {
            return MP_ENODEV;
        }

        instance = state->instance;
        event = &state->event;
        completion = state->completion;
    }

    if (len == 0) {
        return 0;
    }

    if (src == NULL) {
        return MP_EINVAL;
    }

    fsp_err_t error;

    if (dest != NULL) {
        // 需要接收，所以同时发送和接收
        error = instance->p_api->writeRead(instance->p_ctrl, src, dest, (uint32_t)len, SPI_BIT_WIDTH_8_BITS);
    } else {
        // 不需要接收，只发送
        error = instance->p_api->write(instance->p_ctrl, src, (uint32_t)len, SPI_BIT_WIDTH_8_BITS);
    }

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (xSemaphoreTake(completion, timeout_ticks) != pdTRUE) {
        spi_deinit(id);
        return MP_ETIMEDOUT;
    }

    mp_handle_pending(true);   //等待 SPI 期间产生的键盘中断或异常

    switch (*event) {
        case SPI_EVENT_TRANSFER_COMPLETE:
            return 0;

        case SPI_EVENT_TRANSFER_ABORTED:
            return MP_ECANCELED;

        case SPI_EVENT_ERR_MODE_FAULT:
            return MP_EBUSY;

        case SPI_EVENT_ERR_READ_OVERFLOW:
        case SPI_EVENT_ERR_OVERRUN:
            return MP_ENOBUFS;

        case SPI_EVENT_ERR_PARITY:
        case SPI_EVENT_ERR_FRAMING:
            return MP_EIO;

        case SPI_EVENT_ERR_MODE_UNDERRUN:
            return MP_EPIPE;

        default:
            return MP_EIO;
    }
}
