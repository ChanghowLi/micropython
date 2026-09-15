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

static fsp_err_t i2c_encode_address(uint16_t address, uint8_t width, uint8_t *buffer)
{
    if (buffer == NULL) {
        return FSP_ERR_ASSERTION;
    }

    if (width == 1U) {
        if (address > 0xFFU) {
            return FSP_ERR_INVALID_ARGUMENT;
        }
        buffer[0] = (uint8_t)address;
        return FSP_SUCCESS;
    }

    if (width == 2U) {
        buffer[0] = (uint8_t)(address >> 8);
        buffer[1] = (uint8_t)address;
        return FSP_SUCCESS;
    }

    return FSP_ERR_INVALID_ARGUMENT;
}

static fsp_err_t i2c_transfer_segment(i2c_t *i2c, uint8_t *data, uint32_t length, bool read, bool restart, bool *started)
{
    if (started != NULL) {
        *started = false;
    }

#if BSP_CFG_RTOS == 2
    (void)xSemaphoreTake(i2c->completion, 0);
#endif

    i2c->event = (i2c_master_event_t)0;

    fsp_err_t err;
    if (read) {
        err = i2c->instance->p_api->read(i2c->instance->p_ctrl, data, length, restart);
    } else {
        err = i2c->instance->p_api->write(i2c->instance->p_ctrl, data, length, restart);
    }

    if (err != FSP_SUCCESS) {
        return err;
    }

    if (started != NULL) {
        *started = true;
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
        err = i2c->instance->p_api->abort(i2c->instance->p_ctrl);
        if (err != FSP_SUCCESS) {
            return err;
        }
        return FSP_ERR_TIMEOUT;
    }

    i2c_master_event_t expected_event = read ? I2C_MASTER_EVENT_RX_COMPLETE : I2C_MASTER_EVENT_TX_COMPLETE;
    if (i2c->event != expected_event) {
        return FSP_ERR_ABORTED;
    }

    return FSP_SUCCESS;
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

uint32_t IIC_Init(i2c_t *i2c)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL || i2c->instance->p_cfg == NULL) {
        return (uint32_t)FSP_ERR_ASSERTION;
    }

#if BSP_CFG_RTOS == 2
    if (i2c->completion == NULL) {
        i2c->completion = xSemaphoreCreateBinaryStatic(&i2c->completion_storage);
        if (i2c->completion == NULL) {
            return (uint32_t)FSP_ERR_OUT_OF_MEMORY;
        }
    }
#endif

    i2c->cfg = *i2c->instance->p_cfg;
    i2c->cfg.p_callback = i2c_callback;
    i2c->cfg.p_context = i2c;
    i2c->event = (i2c_master_event_t)0;

    fsp_err_t err = i2c->instance->p_api->open(i2c->instance->p_ctrl, &i2c->cfg);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    i2c->opened = true;
    i2c_clear_restart(i2c);
    return (uint32_t)FSP_SUCCESS;
}

uint32_t IIC_DeInit(i2c_t *i2c)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return (uint32_t)FSP_ERR_ASSERTION;
    }

    if (!i2c->opened) {
        return (uint32_t)FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = i2c->instance->p_api->close(i2c->instance->p_ctrl);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    i2c->event = (i2c_master_event_t)0;
    i2c->opened = false;
    i2c_clear_restart(i2c);
    return (uint32_t)FSP_SUCCESS;
}

uint32_t i2c_transfer(i2c_t *i2c, uint32_t slave, uint8_t *data, uint32_t length, bool read, bool restart)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return (uint32_t)FSP_ERR_ASSERTION;
    }

    if (slave > 0x7FU) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (read && (data == NULL || length == 0U)) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (!read && length > 0U && data == NULL) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (!i2c->opened) {
        return (uint32_t)FSP_ERR_NOT_OPEN;
    }

    uint8_t address = (uint8_t)slave;
    fsp_err_t err = i2c_prepare_transfer(i2c, address);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    bool continuing = i2c->restart_pending;

    uint8_t dummy = 0U;
    uint8_t *transfer_data = data;
    if (!read && length == 0U) {
        transfer_data = &dummy;
    }

    bool started = false;
    err = i2c_transfer_segment(i2c, transfer_data, length, read, restart, &started);
    if (err != FSP_SUCCESS) {
        if (continuing && !started && err != FSP_ERR_TIMEOUT) {
            fsp_err_t abort_err = i2c_abort_transfer(i2c);
            if (abort_err != FSP_SUCCESS) {
                return (uint32_t)abort_err;
            }
        } else {
            i2c_clear_restart(i2c);
        }
        return (uint32_t)err;
    }

    i2c_update_restart(i2c, address, restart);
    return (uint32_t)FSP_SUCCESS;
}

uint32_t IIC_ReadMemory(i2c_t *i2c, uint32_t slave, uint16_t mem_addr, uint8_t addr_width, uint8_t *rdata, uint16_t rlen)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return (uint32_t)FSP_ERR_ASSERTION;
    }

    if (slave > 0x7FU || rdata == NULL || rlen == 0U) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (!i2c->opened) {
        return (uint32_t)FSP_ERR_NOT_OPEN;
    }

    uint8_t address_data[2];
    fsp_err_t err = i2c_encode_address(mem_addr, addr_width, address_data);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    err = i2c->instance->p_api->slaveAddressSet(i2c->instance->p_ctrl, slave, I2C_MASTER_ADDR_MODE_7BIT);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    err = i2c_transfer_segment(i2c, address_data, addr_width, false, true, NULL);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    bool started = false;
    err = i2c_transfer_segment(i2c, rdata, rlen, true, false, &started);
    if (err != FSP_SUCCESS && !started && err != FSP_ERR_TIMEOUT) {
        fsp_err_t abort_err = i2c->instance->p_api->abort(i2c->instance->p_ctrl);
        if (abort_err != FSP_SUCCESS) {
            return (uint32_t)abort_err;
        }
    }

    return (uint32_t)err;
}

uint32_t IIC_ReadReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint8_t *val, uint8_t val_width)
{
    if (val == NULL || (val_width != 1U && val_width != 2U)) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    return IIC_ReadMemory(i2c, slave, reg_addr, addr_width, val, val_width);
}

uint32_t IIC_Write(i2c_t *i2c, uint32_t slave, uint8_t *data, uint8_t length)
{
    if (i2c == NULL || i2c->instance == NULL || i2c->instance->p_api == NULL || i2c->instance->p_ctrl == NULL) {
        return (uint32_t)FSP_ERR_ASSERTION;
    }

    if (slave > 0x7FU || data == NULL || length == 0U) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (!i2c->opened) {
        return (uint32_t)FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = i2c->instance->p_api->slaveAddressSet(i2c->instance->p_ctrl, slave, I2C_MASTER_ADDR_MODE_7BIT);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    return (uint32_t)i2c_transfer_segment(i2c, data, length, false, false, NULL);
}

uint32_t IIC_WriteReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint16_t val, uint8_t val_width)
{
    if (slave > 0x7FU || (val_width != 1U && val_width != 2U)) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    if (val_width == 1U && val > 0xFFU) {
        return (uint32_t)FSP_ERR_INVALID_ARGUMENT;
    }

    uint8_t tx_data[4];
    fsp_err_t err = i2c_encode_address(reg_addr, addr_width, tx_data);
    if (err != FSP_SUCCESS) {
        return (uint32_t)err;
    }

    if (val_width == 1U) {
        tx_data[addr_width] = (uint8_t)val;
    } else {
        tx_data[addr_width] = (uint8_t)(val >> 8);
        tx_data[addr_width + 1U] = (uint8_t)val;
    }

    return IIC_Write(i2c, slave, tx_data, (uint8_t)(addr_width + val_width));
}
