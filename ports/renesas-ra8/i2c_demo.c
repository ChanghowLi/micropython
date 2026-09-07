#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_data.h"

#define I2C_DEMO_TIMEOUT_MS (100U)

static volatile i2c_master_event_t i2c_demo_event = (i2c_master_event_t)0;
static bool i2c_demo_opened = false;

void i2c_master_callback(i2c_master_callback_args_t *p_args)
{
    if (p_args == NULL) {
        return;
    }

    i2c_demo_event = p_args->event;
}

static fsp_err_t i2c_demo_configure_pins(void)
{
    fsp_err_t error;

    error = R_IOPORT_PinCfg(
        g_ioport.p_ctrl,
        BSP_IO_PORT_05_PIN_12,
        IOPORT_CFG_PERIPHERAL_PIN |
        IOPORT_PERIPHERAL_IIC |
        IOPORT_CFG_NMOS_ENABLE |
        IOPORT_CFG_DRIVE_MID
    );
    if (error != FSP_SUCCESS) {
        return error;
    }

    error = R_IOPORT_PinCfg(
        g_ioport.p_ctrl,
        BSP_IO_PORT_05_PIN_11,
        IOPORT_CFG_PERIPHERAL_PIN |
        IOPORT_PERIPHERAL_IIC |
        IOPORT_CFG_NMOS_ENABLE |
        IOPORT_CFG_DRIVE_MID
    );

    return error;
}

static fsp_err_t i2c_demo_wait(i2c_master_event_t expected_event)
{
    for (uint32_t elapsed_ms = 0; elapsed_ms < I2C_DEMO_TIMEOUT_MS; ++elapsed_ms) {
        i2c_master_event_t event = i2c_demo_event;

        if (event == expected_event) {
            return FSP_SUCCESS;
        }

        if (event == I2C_MASTER_EVENT_ABORTED) {
            return FSP_ERR_ABORTED;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    (void)R_IIC_MASTER_Abort(g_i2c_master1.p_ctrl);
    return FSP_ERR_TIMEOUT;
}

fsp_err_t i2c_demo_open(void)
{
    if (i2c_demo_opened) {
        return FSP_SUCCESS;
    }

    fsp_err_t error = i2c_demo_configure_pins();
    if (error != FSP_SUCCESS) {
        return error;
    }

    error = R_IIC_MASTER_Open(g_i2c_master1.p_ctrl, g_i2c_master1.p_cfg);
    if (error == FSP_SUCCESS) {
        i2c_demo_opened = true;
    }

    return error;
}

fsp_err_t i2c_demo_close(void)
{
    if (!i2c_demo_opened) {
        return FSP_SUCCESS;
    }

    fsp_err_t error = R_IIC_MASTER_Close(g_i2c_master1.p_ctrl);
    if (error == FSP_SUCCESS) {
        i2c_demo_opened = false;
        i2c_demo_event = (i2c_master_event_t)0;
    }

    return error;
}

fsp_err_t i2c_demo_write(uint8_t slave_addr, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U || slave_addr > 0x7FU) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    fsp_err_t error = i2c_demo_open();
    if (error != FSP_SUCCESS) {
        return error;
    }

    error = R_IIC_MASTER_SlaveAddressSet(
        g_i2c_master1.p_ctrl,
        slave_addr,
        I2C_MASTER_ADDR_MODE_7BIT
    );
    if (error != FSP_SUCCESS) {
        return error;
    }

    i2c_demo_event = (i2c_master_event_t)0;

    error = R_IIC_MASTER_Write(
        g_i2c_master1.p_ctrl,
        data,
        length,
        false
    );
    if (error != FSP_SUCCESS) {
        return error;
    }

    return i2c_demo_wait(I2C_MASTER_EVENT_TX_COMPLETE);
}

fsp_err_t i2c_demo_read(uint8_t slave_addr, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0U || slave_addr > 0x7FU) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    fsp_err_t error = i2c_demo_open();
    if (error != FSP_SUCCESS) {
        return error;
    }

    error = R_IIC_MASTER_SlaveAddressSet(
        g_i2c_master1.p_ctrl,
        slave_addr,
        I2C_MASTER_ADDR_MODE_7BIT
    );
    if (error != FSP_SUCCESS) {
        return error;
    }

    i2c_demo_event = (i2c_master_event_t)0;

    error = R_IIC_MASTER_Read(
        g_i2c_master1.p_ctrl,
        data,
        length,
        false
    );
    if (error != FSP_SUCCESS) {
        return error;
    }

    return i2c_demo_wait(I2C_MASTER_EVENT_RX_COMPLETE);
}
