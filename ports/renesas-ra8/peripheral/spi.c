#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_data.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "spi.h"

typedef struct _ra8_spi_state_t {
    const spi_instance_t *instance;
    spi_cfg_t cfg;
    spi_b_extended_cfg_t extended_cfg;
    volatile spi_event_t event;
    bool opened;
    volatile bool transfer_done;
} ra8_spi_state_t;

static ra8_spi_state_t ra8_spi_states[] = {
    {
        .instance = &g_spi0,
        .event = (spi_event_t)0,
        .opened = false,
        .transfer_done = false,
    },
};

#define RA8_SPI_COUNT \
    (sizeof(ra8_spi_states) / sizeof(ra8_spi_states[0]))

static ra8_spi_state_t *ra8_spi_get_state(uint32_t id) {
    if (id >= RA8_SPI_COUNT) {
        return NULL;
    }

    return &ra8_spi_states[id];
}

static int ra8_spi_fsp_error(fsp_err_t error) {
    if (error == FSP_SUCCESS) {
        return 0;
    }

    return MP_EIO;
}

void spi_callback(spi_callback_args_t *p_args) {
    if (p_args == NULL || p_args->p_context == NULL) {
        return;
    }

    ra8_spi_state_t *state = (ra8_spi_state_t *)p_args->p_context;

    state->event = p_args->event;
    state->transfer_done = true;
}

void spi_init0(void) {
    for (size_t id = 0; id < RA8_SPI_COUNT; ++id) {
        ra8_spi_state_t *state = &ra8_spi_states[id];

        state->event = (spi_event_t)0;
        state->opened = false;
        state->transfer_done = false;
    }
}

bool spi_deinit(uint32_t id) {
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL || !state->opened) {
        return false;
    }

    state->instance->p_api->close(state->instance->p_ctrl);

    state->event = (spi_event_t)0;
    state->opened = false;
    state->transfer_done = false;
    return true;
}

int spi_init(
    uint32_t id,
    uint32_t baudrate,
    uint8_t polarity,
    uint8_t phase,
    uint8_t bits,
    uint8_t firstbit
    ) {
    ra8_spi_state_t *state = ra8_spi_get_state(id);

    if (state == NULL) {
        return MP_ENODEV;
    }

    if (baudrate == 0 ||
        bits != 8 ||
        polarity > 1 ||
        phase > 1 ||
        firstbit > 1) {
        return MP_EINVAL;
    }

    if (state->opened) {
        spi_deinit(id);
    }

    state->cfg = *state->instance->p_cfg;

    state->extended_cfg =
        *(const spi_b_extended_cfg_t *)
        state->instance->p_cfg->p_extend;

    state->cfg.clk_phase =
        phase == 0 ?
        SPI_CLK_PHASE_EDGE_ODD :
        SPI_CLK_PHASE_EDGE_EVEN;

    state->cfg.clk_polarity =
        polarity == 0 ?
        SPI_CLK_POLARITY_LOW :
        SPI_CLK_POLARITY_HIGH;

    state->cfg.bit_order =
        firstbit == 0 ?
        SPI_BIT_ORDER_MSB_FIRST :
        SPI_BIT_ORDER_LSB_FIRST;

    state->cfg.p_callback = spi_callback;
    state->cfg.p_context = state;
    state->cfg.p_extend = &state->extended_cfg;

    fsp_err_t error = R_SPI_B_CalculateBitrate(
        baudrate,
        state->extended_cfg.clock_source,
        &state->extended_cfg.spck_div
        );

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    state->event = (spi_event_t)0;
    state->transfer_done = false;

    error = state->instance->p_api->open(
        state->instance->p_ctrl,
        &state->cfg
        );

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    state->opened = true;
    return 0;
}

int spi_transfer(
    uint32_t id,
    size_t len,
    const uint8_t *src,
    uint8_t *dest,
    uint32_t timeout_ms
    ) {
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
    state->transfer_done = false;

    fsp_err_t error;

    if (dest != NULL) {
        error = state->instance->p_api->writeRead(
            state->instance->p_ctrl,
            src,
            dest,
            (uint32_t)len,
            SPI_BIT_WIDTH_8_BITS
            );
    } else {
        error = state->instance->p_api->write(
            state->instance->p_ctrl,
            src,
            (uint32_t)len,
            SPI_BIT_WIDTH_8_BITS
            );
    }

    if (error != FSP_SUCCESS) {
        return ra8_spi_fsp_error(error);
    }

    uint32_t start_ms = mp_hal_ticks_ms();

    while (!state->transfer_done) {
        mp_handle_pending(true);

        if ((uint32_t)(mp_hal_ticks_ms() - start_ms) >=
            timeout_ms) {
            spi_deinit(id);
            return MP_ETIMEDOUT;
        }
    }

    if (state->event != SPI_EVENT_TRANSFER_COMPLETE) {
        return MP_EIO;
    }

    return 0;
}
