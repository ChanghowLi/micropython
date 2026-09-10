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

#ifndef __SPI_DEBUG
#define __SPI_DEBUG 1
#endif

#if __SPI_DEBUG
#include "utils/log.h"
#define SPI_LOGD(msg, ...)      LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define SPI_LOGI(msg, ...)      LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define SPI_LOGW(msg, ...)      LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define SPI_LOGE(msg, ...)      LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define SPI_LOGD(msg, ...)
#define SPI_LOGI(msg, ...)
#define SPI_LOGW(msg, ...)
#define SPI_LOGE(msg, ...)
#endif

#define RA8_SCI_SPI_FIRST_ID (10)
#define RA8_SPI_COUNT MP_ARRAY_SIZE(ra8_spi_states)

typedef struct _ra8_spi_state_t {
    const spi_instance_t *instance;
    spi_cfg_t cfg;
    spi_b_extended_cfg_t spi_extended_cfg;
    sci_b_spi_extended_cfg_t sci_spi_extended_cfg;
    volatile spi_event_t event;
    bool opened;
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
} ra8_spi_state_t;

static ra8_spi_state_t ra8_spi_states[] = {
    {.instance = &g_spi0},
    {.instance = &g_spi1},
};

static ra8_spi_state_t ra8_sci_spi_states[] = {
#if defined(MICROPY_HW_SCI0_SCK)
    {.instance = &g_sci_spi0},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI1_SCK)
    {.instance = &g_sci_spi1},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI2_SCK)
    {.instance = &g_sci_spi2},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI3_SCK)
    {.instance = &g_sci_spi3},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI4_SCK)
    {.instance = &g_sci_spi4},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI5_SCK)
    {.instance = &g_sci_spi5},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI6_SCK)
    {.instance = &g_sci_spi6},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI7_SCK)
    {.instance = &g_sci_spi7},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI8_SCK)
    {.instance = &g_sci_spi8},
#else
    {.instance = NULL},
#endif
#if defined(MICROPY_HW_SCI9_SCK)
    {.instance = &g_sci_spi9},
#else
    {.instance = NULL},
#endif
};

static ra8_spi_state_t *ra8_spi_get_state(uint32_t id)
{
    if (id < RA8_SPI_COUNT) {
        return &ra8_spi_states[id];
    }

    if (id < RA8_SCI_SPI_FIRST_ID) {
        return NULL;
    }

    uint32_t index = id - RA8_SCI_SPI_FIRST_ID;

    if (index >= MP_ARRAY_SIZE(ra8_sci_spi_states) || ra8_sci_spi_states[index].instance == NULL) {
        return NULL;
    }

    return &ra8_sci_spi_states[index];
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

bool spi_deinit(uint32_t id)
{
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL) {
        SPI_LOGE("invalid SPI id: %lu", (unsigned long)id);
        return false;
    }

    SPI_LOGD("id=%lu, opened=%d", (unsigned long)id, state->opened);

    if (!state->opened) {
        return true;
    }

    fsp_err_t error = state->instance->p_api->close(state->instance->p_ctrl);

    if (error != FSP_SUCCESS) {
        SPI_LOGE("failed to close SPI: id=%lu, error=%d", (unsigned long)id, (int)error);
        return false;
    }

    state->event = (spi_event_t)0;
    state->opened = false;
    return true;
}

int spi_init(uint32_t id, uint32_t baudrate, uint8_t polarity, uint8_t phase, uint8_t bits, uint8_t firstbit)
{
    bool valid_bits = id >= RA8_SCI_SPI_FIRST_ID ? bits == 8 : bits == 8 || bits == 16 || bits == 32;
    if (baudrate == 0 || !valid_bits || polarity > 1 || phase > 1 || firstbit > 1) {
        return MP_EINVAL;
    }

    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL) {
        return MP_ENODEV;
    }

    if (!spi_deinit(id)) {
        return MP_EIO;
    }

    state->cfg = *state->instance->p_cfg;
    state->cfg.clk_phase = phase == 0 ? SPI_CLK_PHASE_EDGE_ODD : SPI_CLK_PHASE_EDGE_EVEN;
    state->cfg.clk_polarity = polarity == 0 ? SPI_CLK_POLARITY_LOW : SPI_CLK_POLARITY_HIGH;
    state->cfg.bit_order = firstbit == 0 ? SPI_BIT_ORDER_MSB_FIRST : SPI_BIT_ORDER_LSB_FIRST;
    state->cfg.p_callback = spi_callback;
    state->cfg.p_context = state;

    if (state->completion == NULL) {
        state->completion = xSemaphoreCreateBinaryStatic(&state->completion_storage);

        if (state->completion == NULL) {
            return MP_ENOMEM;
        }
    }

    fsp_err_t error;

    if (id >= RA8_SCI_SPI_FIRST_ID) {
        state->sci_spi_extended_cfg = *(const sci_b_spi_extended_cfg_t *)state->instance->p_cfg->p_extend;
        state->cfg.p_extend = &state->sci_spi_extended_cfg;
        error = R_SCI_B_SPI_CalculateBitrate(baudrate, state->sci_spi_extended_cfg.clock_source, &state->sci_spi_extended_cfg.clk_div);
    }
    else {
        state->spi_extended_cfg = *(const spi_b_extended_cfg_t *)state->instance->p_cfg->p_extend;
        state->cfg.p_extend = &state->spi_extended_cfg;
        error = R_SPI_B_CalculateBitrate(baudrate, state->spi_extended_cfg.clock_source, &state->spi_extended_cfg.spck_div);
    }

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

int spi_transfer(uint32_t id, size_t len, const uint8_t *src, uint8_t *dest, uint8_t bits, uint32_t timeout_ms)
{
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL || !state->opened) {
        return MP_ENODEV;
    }

    if (len == 0) {
        return 0;
    }

    if (src == NULL) {
        return MP_EINVAL;
    }

    uint32_t frame_size;
    spi_bit_width_t bit_width;

    switch (bits) {
    case 8:
        bit_width = SPI_BIT_WIDTH_8_BITS;
        frame_size = 1;
        break;

    case 16:
        bit_width = SPI_BIT_WIDTH_16_BITS;
        frame_size = 2;
        break;

    case 32:
        bit_width = SPI_BIT_WIDTH_32_BITS;
        frame_size = 4;
        break;

    default:
        return MP_EINVAL;
    }

    if ((id >= RA8_SCI_SPI_FIRST_ID && bits != 8) || len % frame_size != 0) {
        return MP_EINVAL;
    }

    if ((uintptr_t)src % frame_size != 0 || (dest != NULL && (uintptr_t)dest % frame_size != 0)) {
        return MP_EINVAL;
    }

    uint32_t frame_count = (uint32_t)(len / frame_size);

    (void)xSemaphoreTake(state->completion, 0);
    state->event = (spi_event_t)0;

    fsp_err_t error;

    if (dest != NULL) {
        /*需要接收，所以同时发送和接收*/
        error = state->instance->p_api->writeRead(state->instance->p_ctrl, src, dest, frame_count, bit_width);
    }
    else {
        /*不需要接收，只发送*/
        error = state->instance->p_api->write(state->instance->p_ctrl, src, frame_count, bit_width);
    }

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (xSemaphoreTake(state->completion, timeout_ticks) != pdTRUE) {
        SPI_LOGE("SPI transfer timeout: id=%lu", (unsigned long)id);

        if (!spi_deinit(id)) {
            SPI_LOGE("failed to close SPI after timeout: id=%lu", (unsigned long)id);
            return MP_EIO;
        }

        return MP_ETIMEDOUT;
    }

    int result;

    switch (state->event) {
    case SPI_EVENT_TRANSFER_COMPLETE:
        return 0;

    case SPI_EVENT_TRANSFER_ABORTED:
        result = MP_ECANCELED;
        break;

    case SPI_EVENT_ERR_MODE_FAULT:
        result = MP_EBUSY;
        break;

    case SPI_EVENT_ERR_READ_OVERFLOW:
    case SPI_EVENT_ERR_OVERRUN:
        result = MP_ENOBUFS;
        break;

    case SPI_EVENT_ERR_PARITY:
    case SPI_EVENT_ERR_FRAMING:
        result = MP_EIO;
        break;

    case SPI_EVENT_ERR_MODE_UNDERRUN:
        result = MP_EPIPE;
        break;

    default:
        result = MP_EIO;
        break;
    }

    SPI_LOGE("SPI transfer failed: id=%lu, event=%d, errno=%d", (unsigned long)id, (int)state->event, result);
    return result;
}
