#ifndef RENESAS_RA8_I2C_H
#define RENESAS_RA8_I2C_H

#include <stdbool.h>
#include <stdint.h>

#include "r_i2c_master_api.h"

#define I2C_DEFAULT_TIMEOUT_US    (100000U)

#if BSP_CFG_RTOS == 2
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#endif

typedef struct {
    const i2c_master_instance_t *instance;
    i2c_master_cfg_t cfg;
    volatile i2c_master_event_t event;
    bool opened;
    uint32_t timeout_us;
#if BSP_CFG_RTOS == 2
    SemaphoreHandle_t completion;
    StaticSemaphore_t completion_storage;
    /* One mutex per non-board controller, held across repeated START calls. */
    SemaphoreHandle_t transfer_mutex;
    StaticSemaphore_t transfer_mutex_storage;
    TaskHandle_t transfer_owner;
#endif
} i2c_t;

typedef struct {
    uint8_t *data;
    uint32_t length;
    bool read;
} i2c_segment_t;

/* Task-context board clients acquire/release once per device lifetime.
 * IIC0 is exclusive to Python while any non-board owner has claimed it. */
uint32_t IIC_BoardAcquire(i2c_t **bus);
uint32_t IIC_BoardRelease(i2c_t *bus);
/* Explicit claims survive Init failure and DeInit until Unclaim, so pin
 * cleanup finishes before another owner can configure the controller. */
uint32_t IIC_Claim(i2c_t *i2c);
void IIC_Unclaim(i2c_t *i2c);
uint32_t IIC_ReadRegSCCB(i2c_t *i2c, uint32_t slave, uint16_t reg,
    uint8_t address_bytes, uint8_t *value);

uint32_t IIC_DeInit(i2c_t *i2c);
uint32_t IIC_Init(i2c_t *i2c);
uint32_t IIC_ReadMemory(i2c_t *i2c, uint32_t slave, uint16_t mem_addr, uint8_t addr_width, uint8_t *rdata, uint16_t rlen);
uint32_t IIC_ReadReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint8_t *val, uint8_t val_width);
uint32_t IIC_Write(i2c_t *i2c, uint32_t slave, uint8_t *data, uint8_t length);
uint32_t IIC_WriteReg(i2c_t *i2c, uint32_t slave, uint16_t reg_addr, uint8_t addr_width, uint16_t val, uint8_t val_width);

uint32_t IIC_Transfer(i2c_t *i2c, uint32_t slave, i2c_segment_t *segments, uint32_t count, bool stop);

#endif
