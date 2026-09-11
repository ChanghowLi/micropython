#include "i2c.h"
#include "bsp_api.h"

#define I2C_TIMEOUT_MS   (100U)

static void i2c_clear_restart(i2c_t *i2c)
{
    i2c->current_address = 0U;
    i2c->restart_pending = false;
}

static fsp_err_t i2c_abort_transfer(i2c_t *i2c)
{
    fsp_err_t err = i2c->instance->p_api->abort(i2c->instance->p_ctrl);
    i2c_clear_restart(i2c);
    return err;
}

static fsp_err_t i2c_prepare_transfer(i2c_t *i2c, uint8_t address)
{
    if (i2c->restart_pending) {
        if (address != i2c->current_address) {
            fsp_err_t err = i2c_abort_transfer(i2c);
            if (err != FSP_SUCCESS) {
                return err;
            }
            return FSP_ERR_IN_USE;
        }
        return FSP_SUCCESS;
    }

    return i2c->instance->p_api->slaveAddressSet(i2c->instance->p_ctrl, address, I2C_MASTER_ADDR_MODE_7BIT);
}

static void i2c_update_restart(i2c_t *i2c, uint8_t address, bool restart)
{
    if (restart) {
        i2c->current_address = address;
        i2c->restart_pending = true;
    } else {
        i2c_clear_restart(i2c);
    }
}

void i2c_callback(i2c_master_callback_args_t *p_args)
{
    if (p_args == NULL || p_args->p_context == NULL) {
        return;
    }

    i2c_t *i2c = (i2c_t *)p_args->p_context;
    i2c->event = p_args->event;

    if (p_args->event == I2C_MASTER_EVENT_TX_COMPLETE || p_args->event == I2C_MASTER_EVENT_RX_COMPLETE || p_args->event == I2C_MASTER_EVENT_ABORTED) {
#if BSP_CFG_RTOS == 2
        if (i2c->completion != NULL) {
            BaseType_t higher_priority_task_woken = pdFALSE;
            xSemaphoreGiveFromISR(i2c->completion, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
#endif
    }
}

fsp_err_t i2c_open(i2c_t *i2c)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL || i2c->instance->p_cfg == NULL) {
        return FSP_ERR_ASSERTION;
    }

#if BSP_CFG_RTOS == 2
    if (i2c->completion == NULL) {
        i2c->completion = xSemaphoreCreateBinaryStatic(&i2c->completion_storage);
        if (i2c->completion == NULL) {
            return FSP_ERR_OUT_OF_MEMORY;
        }
    }
#endif

    i2c->cfg = *i2c->instance->p_cfg;
    i2c->cfg.p_callback = i2c_callback;
    i2c->cfg.p_context = i2c;
    i2c->event = (i2c_master_event_t)0;

    fsp_err_t err = i2c->instance->p_api->open(i2c->instance->p_ctrl, &i2c->cfg);
    if (err != FSP_SUCCESS) {
        return err;
    }

    i2c->opened = true;
    i2c_clear_restart(i2c);
    return FSP_SUCCESS;
}

fsp_err_t i2c_close(i2c_t *i2c)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return FSP_ERR_ASSERTION;
    }

    if (!i2c->opened) {
        return FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = i2c->instance->p_api->close(i2c->instance->p_ctrl);
    if (err != FSP_SUCCESS) {
        return err;
    }

    i2c->event = (i2c_master_event_t)0;
    i2c->opened = false;
    i2c_clear_restart(i2c);
    return FSP_SUCCESS;
}

fsp_err_t i2c_write(i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t length, bool restart)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return FSP_ERR_ASSERTION;
    }

    if (address > 0x7FU || data == NULL || length == 0U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (!i2c->opened) {
        return FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = i2c_prepare_transfer(i2c, address);
    if (err != FSP_SUCCESS) {
        return err;
    }

    bool continuing = i2c->restart_pending;

#if BSP_CFG_RTOS == 2
    (void)xSemaphoreTake(i2c->completion, 0);
#endif

    i2c->event = (i2c_master_event_t)0;

    err = i2c->instance->p_api->write(i2c->instance->p_ctrl, data, length, restart);
    if (err != FSP_SUCCESS) {
        if (continuing) {
            fsp_err_t abort_err = i2c_abort_transfer(i2c);
            if (abort_err != FSP_SUCCESS) {
                return abort_err;
            }
        }
        return err;
    }

#if BSP_CFG_RTOS == 2
    bool timed_out = xSemaphoreTake(i2c->completion, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE;
#else
    uint32_t waited_ms = 0U;
    while (i2c->event == (i2c_master_event_t)0 && waited_ms < I2C_TIMEOUT_MS) {
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        ++waited_ms;
    }
    bool timed_out = i2c->event == (i2c_master_event_t)0;
#endif

    if (timed_out) {
        err = i2c_abort_transfer(i2c);
        if (err != FSP_SUCCESS) {
            return err;
        }
        return FSP_ERR_TIMEOUT;
    }

    if (i2c->event != I2C_MASTER_EVENT_TX_COMPLETE) {
        i2c_clear_restart(i2c);
        return FSP_ERR_ABORTED;
    }

    i2c_update_restart(i2c, address, restart);
    return FSP_SUCCESS;
}

fsp_err_t i2c_read(i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t length, bool restart)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return FSP_ERR_ASSERTION;
    }

    if (address > 0x7FU || data == NULL || length == 0U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (!i2c->opened) {
        return FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = i2c_prepare_transfer(i2c, address);
    if (err != FSP_SUCCESS) {
        return err;
    }

    bool continuing = i2c->restart_pending;

#if BSP_CFG_RTOS == 2
    (void)xSemaphoreTake(i2c->completion, 0);
#endif

    i2c->event = (i2c_master_event_t)0;

    err = i2c->instance->p_api->read(i2c->instance->p_ctrl, data, length, restart);
    if (err != FSP_SUCCESS) {
        if (continuing) {
            fsp_err_t abort_err = i2c_abort_transfer(i2c);
            if (abort_err != FSP_SUCCESS) {
                return abort_err;
            }
        }
        return err;
    }

#if BSP_CFG_RTOS == 2
    bool timed_out = xSemaphoreTake(i2c->completion, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE;
#else
    uint32_t waited_ms = 0U;
    while (i2c->event == (i2c_master_event_t)0 && waited_ms < I2C_TIMEOUT_MS) {
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        ++waited_ms;
    }
    bool timed_out = i2c->event == (i2c_master_event_t)0;
#endif

    if (timed_out) {
        err = i2c_abort_transfer(i2c);
        if (err != FSP_SUCCESS) {
            return err;
        }
        return FSP_ERR_TIMEOUT;
    }

    if (i2c->event != I2C_MASTER_EVENT_RX_COMPLETE) {
        i2c_clear_restart(i2c);
        return FSP_ERR_ABORTED;
    }

    i2c_update_restart(i2c, address, restart);
    return FSP_SUCCESS;
}
