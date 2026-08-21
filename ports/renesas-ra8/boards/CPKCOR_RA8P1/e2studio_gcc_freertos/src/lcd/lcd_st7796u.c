#include "lcd_st7796u.h"

#if LCD_EN_ST7796U

/*================================== INCLUDES =====================================================*/

#if defined(__ARMCOMPILER_VERSION)
#elif defined(__clang_version__)
#include <byteswap.h>
#else
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "hal_data.h"
#include "utils/util.h"

/*================================== MACROS =======================================================*/

#define ST7796U_INTERFACE_3SPI              0x01
#define ST7796U_INTERFACE_MIPI              0x02
#define ST7796U_INTERFACE_RGB               0x03

#ifndef __ST7796U_DEBUG
#define __ST7796U_DEBUG			            1
#endif

#ifndef __ST7796U_INTERFACE
#define __ST7796U_INTERFACE                 ST7796U_INTERFACE_3SPI
#endif

#ifndef __ST7796U_DEFAULT_TIMEOUT
#define __ST7796U_DEFAULT_TIMEOUT           500
#endif

#define ST7796U_PIN_BACKLIGHT	            BSP_IO_PORT_00_PIN_12
#define ST7796U_PIN_RESET		            BSP_IO_PORT_00_PIN_13
#define ST7796U_X_LENGTH                    222
#define ST7796U_X_OFFSET                    49
#define ST7796U_Y_LENGTH                    480
#define ST7796U_Y_OFFSET                    0

#if __ST7796U_DEBUG
#include <utils/log.h>
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define ST7796U_LOGD(msg, ...)              LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGI(msg, ...)              LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGW(msg, ...)              LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGE(msg, ...)              LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define ST7796U_LOGD(msg, ...)
#define ST7796U_LOGI(msg, ...)
#define ST7796U_LOGW(msg, ...)
#define ST7796U_LOGE(msg, ...)
#endif

#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI

#ifndef ST7796U_EN_CACHE
#define ST7796U_EN_CACHE            1
#endif

#if ST7796U_EN_CACHE
#define ST7796U_CACHE_SIZE          (ST7796U_X_LENGTH * ST7796U_Y_LENGTH * 2)
#endif

#define ST7796U_PIN_SPI_CLK		    BSP_IO_PORT_05_PIN_14
#define ST7796U_PIN_SPI_MOSI	    BSP_IO_PORT_07_PIN_14
#define ST7796U_PIN_SPI_CS          BSP_IO_PORT_05_PIN_15
#define ST7796U_PIN_DCX             BSP_IO_PORT_07_PIN_15

#endif /* #if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI */

/*================================== TYPES ========================================================*/

#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI
/* If cmd == 0xFF, that means delay len ms */
struct spi_init_table {
    uint8_t cmd;
    uint8_t len;
    uint8_t val[16];
};
#endif

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI

static const struct spi_init_table sc_init_table[] = {
    {0xFF, 120, {0x00}},
    {0x11, 0  , {0x00}},
    {0xFF, 120, {0x00}},
    {0xF0, 1  , {0xC3}},
    {0xF0, 1  , {0x96}},
    {0x36, 1  , {0x48}},
    {0x3A, 1  , {0x55}},
    {0xB4, 1  , {0x01}},
    {0xB6, 3  , {0x8A, 0x07, 0x3B}},
    {0xB7, 1  , {0xC6}},
    {0xB9, 2  , {0x02, 0xE0}},
    {0xC0, 2  , {0xC0, 0x64}},
    {0xC1, 1  , {0x1D}},
    {0xC2, 1  , {0xA7}},
    {0xC5, 1  , {0x18}},
    {0xE8, 8  , {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}},
    {0xE0, 14 , {0xF0, 0x0B, 0x12, 0x09, 0x0A, 0x26, 0x39, 0x54, 0x4E, 0x38, 0x13, 0x13, 0x2E, 0x34}},
    {0xE1, 14 , {0xF0, 0x10, 0x15, 0x0D, 0x0C, 0x07, 0x38, 0x43, 0x4D, 0x3A, 0x16, 0x15, 0x30, 0x35}},
    {0x35, 1  , {0x00}},
    {0x21, 0  , {0x00}},
    {0x2A, 4  , {(ST7796U_X_OFFSET >> 8) & 0xFF, ST7796U_X_OFFSET & 0xFF, ((ST7796U_X_LENGTH + ST7796U_X_OFFSET - 1) >> 8) & 0xFF, (ST7796U_X_LENGTH + ST7796U_X_OFFSET - 1) & 0xFF}},
    {0x2B, 4  , {(ST7796U_Y_OFFSET >> 8) & 0xFF, ST7796U_Y_OFFSET & 0xFF, ((ST7796U_Y_LENGTH + ST7796U_Y_OFFSET - 1) >> 8) & 0xFF, (ST7796U_Y_LENGTH + ST7796U_Y_OFFSET - 1) & 0xFF}},
	{0xF0, 1  , {0x3C}},
    {0xF0, 1  , {0x69}},
	{0x29, 0  , {0x00}},
};

static spi_event_t s_spi_event;

#endif /* #if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI */

/*================================== Private Functions Prototypes =================================*/

static void hardReset(void);
static void pinConfig(void);
#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI
static void spiCallback(spi_callback_args_t *p_args);
#endif

/*================================== Public Functions =============================================*/

uint32_t LCD_ST7796U_Init(LCD_DeviceType *lcd)
{
    size_t i, init_table_len;

    uint32_t err = FSP_SUCCESS;
    const spi_instance_t *p_inst = (const spi_instance_t *)lcd->peri_inst;
    sci_b_spi_instance_ctrl_t *p_ctrl = (sci_b_spi_instance_ctrl_t *)p_inst->p_ctrl;

    pinConfig();
    hardReset();
    if (p_ctrl->open == 0) {
        err = R_SCI_B_SPI_Open(p_inst->p_ctrl, p_inst->p_cfg);
        UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
    }
    else {
        ST7796U_LOGI("peri_inst already open");
    }
    R_SCI_B_SPI_CallbackSet(p_ctrl, spiCallback, NULL, NULL);

    return err;
}

/*================================== Private Functions ============================================*/

static void hardReset(void)
{
    R_BSP_PinAccessEnable();
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(4, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
}

static void pinConfig(void)
{
    ioport_instance_ctrl_t pin_ctrl;
    ioport_cfg_t pin_cfg;
#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI
    ioport_pin_cfg_t pin_cfg_data[6];
#endif

    pin_cfg_data[0].pin = ST7796U_PIN_BACKLIGHT;
    pin_cfg_data[0].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
    pin_cfg_data[1].pin = ST7796U_PIN_RESET;
    pin_cfg_data[1].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI
    pin_cfg_data[2].pin = ST7796U_PIN_DCX;
    pin_cfg_data[2].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH | IOPORT_CFG_DRIVE_HIGH;
    pin_cfg_data[3].pin = ST7796U_PIN_SPI_CLK;
    pin_cfg_data[3].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_SCI0_2_4_6_8 | IOPORT_CFG_DRIVE_HIGH;
    pin_cfg_data[4].pin = ST7796U_PIN_SPI_MOSI;
    pin_cfg_data[4].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_SCI0_2_4_6_8 | IOPORT_CFG_DRIVE_HIGH;
    pin_cfg_data[5].pin = ST7796U_PIN_SPI_CS;
    pin_cfg_data[5].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_SCI0_2_4_6_8 | IOPORT_CFG_DRIVE_HIGH;
    pin_cfg.number_of_pins = 5;
#endif
    pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&pin_cfg_data;
    pin_cfg.p_extend = NULL;
    R_IOPORT_Open(&pin_ctrl, &pin_cfg);
}

#if __ST7796U_INTERFACE == ST7796U_INTERFACE_3SPI
static void spiCallback(spi_callback_args_t *p_args)
{
    s_spi_event = p_args->event;
}
#endif

#endif /* #if LCD_EN_ST7796U */
