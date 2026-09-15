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

uint32_t IIC_DeInit(i2c_t *i2c);
uint32_t IIC_Init(i2c_t *i2c);
uint32_t IIC_ReadMemory(i2c_t *i2c, uint32_t slave, uint16_t mem_addr, uint8_t addr_width, uint8_t *rdata, uint16_t rlen);
uint32_t IIC_ReadReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint8_t *val, uint8_t val_width);
uint32_t IIC_Write(i2c_t *i2c, uint32_t slave, uint8_t *data, uint8_t length);
uint32_t IIC_WriteReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint16_t val, uint8_t val_width);

/* Internal primitive used by the MicroPython machine.I2C bridge. */
uint32_t i2c_transfer(i2c_t *i2c, uint32_t slave, uint8_t *data, uint32_t length, bool read, bool restart);

#endif
