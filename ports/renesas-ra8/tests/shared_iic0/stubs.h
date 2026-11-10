/* Host-only FSP/RTOS stand-ins; the production i2c.c is compiled unchanged. */
#ifndef TEST_IIC_STUBS_H
#define TEST_IIC_STUBS_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BSP_CFG_RTOS 2
#define BSP_DELAY_UNITS_MICROSECONDS 0
#define BSP_IO_PORT_04_PIN_09 0x409
#define BSP_IO_PORT_04_PIN_10 0x40a
#define FSP_CRITICAL_SECTION_DEFINE
#define FSP_CRITICAL_SECTION_ENTER
#define FSP_CRITICAL_SECTION_EXIT
#define IOPORT_CFG_DRIVE_MID 4U
#define IOPORT_CFG_NMOS_ENABLE 2U
#define IOPORT_CFG_PERIPHERAL_PIN 1U
#define IOPORT_PERIPHERAL_IIC 8U
#define configTICK_RATE_HZ 1000U
#define pdFALSE 0
#define pdTRUE 1
#define portYIELD_FROM_ISR(x) ((void)(x))
typedef int fsp_err_t;
enum { FSP_SUCCESS, FSP_ERR_ASSERTION, FSP_ERR_INVALID_ARGUMENT, FSP_ERR_OUT_OF_MEMORY, FSP_ERR_TIMEOUT, FSP_ERR_ABORTED, FSP_ERR_NOT_OPEN, FSP_ERR_IN_USE, FSP_ERR_ALREADY_OPEN, FSP_ERR_UNSUPPORTED };
enum { I2C_MASTER_ADDR_MODE_7BIT };
typedef enum { I2C_MASTER_EVENT_TX_COMPLETE=1, I2C_MASTER_EVENT_RX_COMPLETE, I2C_MASTER_EVENT_ABORTED } i2c_master_event_t;
typedef struct { void *p_context; i2c_master_event_t event; } i2c_master_callback_args_t;
typedef struct { void (*p_callback)(i2c_master_callback_args_t *); void *p_context; } i2c_master_cfg_t;
typedef struct { bool restarted; uint32_t slave; } iic_master_instance_ctrl_t;
typedef iic_master_instance_ctrl_t sci_b_i2c_instance_ctrl_t;
typedef struct {
 fsp_err_t (*open)(void *, const i2c_master_cfg_t *);
 fsp_err_t (*close)(void *);
 fsp_err_t (*read)(void *,uint8_t *,uint32_t,bool);
 fsp_err_t (*write)(void *,uint8_t *,uint32_t,bool);
 fsp_err_t (*abort)(void *);
 fsp_err_t (*slaveAddressSet)(void *,uint32_t,int);
} i2c_master_api_t;
typedef struct { void *p_ctrl; const i2c_master_cfg_t *p_cfg; const i2c_master_api_t *p_api; } i2c_master_instance_t;
extern const i2c_master_api_t g_i2c_master_on_iic, g_i2c_master_on_sci_b;
extern const i2c_master_instance_t g_i2c_master0;
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef struct { int available; } StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s) { s->available=0; return s; }
static inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)t; if(!s->available)return 0; s->available=0; return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t s) { s->available=1; return 1; }
static inline void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *w) { (void)w; s->available=1; }
static inline void R_BSP_SoftwareDelay(unsigned a,int b) { (void)a;(void)b; }
typedef struct { void *p_ctrl; } test_ioport_t;
extern test_ioport_t g_ioport;
extern const int g_ceu_ov5640, g_ceu_ov7725;
static inline fsp_err_t R_IOPORT_PinCfg(void *c,int pin,uint32_t cfg) { (void)c;(void)pin;(void)cfg;return 0; }
#endif
