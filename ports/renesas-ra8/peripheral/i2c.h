#ifndef RENESAS_RA8_I2C_H
#define RENESAS_RA8_I2C_H

#include <stdbool.h>
#include <stdint.h>

#include "r_i2c_master_api.h"

#if BSP_CFG_RTOS == 2
#include "FreeRTOS.h"
#include "semphr.h"
#endif

typedef struct {
    const i2c_master_instance_t *instance;
    i2c_master_cfg_t cfg;
    volatile i2c_master_event_t event;
    uint8_t current_address;
    bool opened;
    bool restart_pending;
#if BSP_CFG_RTOS == 2
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
#endif
} i2c_t;

fsp_err_t i2c_open(i2c_t *i2c);
fsp_err_t i2c_close(i2c_t *i2c);
fsp_err_t i2c_write(i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t length, bool restart);
fsp_err_t i2c_read(i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t length, bool restart);

#endif
