#include "lcd_ek79001.h"

#if LCD_EN_EK79001

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <stdbool.h>
#include "hal_data.h"
#include "utils/util.h"

/*================================== MACROS =======================================================*/

#define EK79001_PIN_BACKLIGHT				BSP_IO_PORT_00_PIN_12
#define EK79001_PIN_RESET					BSP_IO_PORT_00_PIN_13

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#define EK79001_CACHE_NUM					1

#ifndef __EK79001_DEBUG
#define __EK79001_DEBUG	1
#endif

#if __EK79001_DEBUG
#include "perf_counter/perf_counter.h"
#include "utils/log.h"
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define EK79001_LOGD(msg, ...)           	LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGI(msg, ...)           	LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGW(msg, ...)           	LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGE(msg, ...)   			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define EK79001_LOGD(msg, ...)
#define EK79001_LOGI(msg, ...)
#define EK79001_LOGW(msg, ...)
#define EK79001_LOGE(msg, ...)
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[32];

/*================================== Private Functions Prototypes =================================*/

static void pinConfig(void);
static void glcdcCallback(display_callback_args_t *p_args);

/*================================== Public Functions =============================================*/

uint32_t LCD_EK79001_Init(LCD_DeviceType *lcd)
{
	uint32_t err;

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;

	pinConfig();
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79001_PIN_BACKLIGHT, BSP_IO_LEVEL_LOW);
	R_BSP_PinWrite(EK79001_PIN_RESET, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(EK79001_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		EK79001_LOGI("GLCDC already open");
		p_glcdc->p_api->close(p_glcdc_ctrl);
	}
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;

	lcd->x_length = 1024;
	lcd->x_dummy = 0;
	lcd->y_length = 600;
	lcd->y_dummy = 0;
	lcd->color_format = LCD_COLOR_FORMAT_RGB888;
	lcd->orientation = LCD_ORIENTATION_HORIZONTAL;
#if EK79001_CACHE_NUM == 1
	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
	lcd->graphic_mem_2 = NULL;
	lcd->graphic_writeable = lcd->graphic_mem_1;
#elif EK79001_CACHE_NUM == 2
	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
	lcd->graphic_mem_2 = &lcd->graphic_mem_1[lcd->x_length * lcd->y_length * 4];
	lcd->graphic_writeable = lcd->graphic_mem_2;
#endif

	memset(lcd->graphic_mem_1, 0x00, lcd->x_length * lcd->y_length * 4);
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	err = p_glcdc->p_api->start(p_glcdc_ctrl);
	R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);

	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79001_PIN_BACKLIGHT, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();

	return err;
}

/*================================== Private Functions ============================================*/

static void pinConfig(void)
{
	s_pin_cfg_data[0].pin = EK79001_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = EK79001_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[2].pin = BSP_IO_PORT_05_PIN_15;
	s_pin_cfg_data[2].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[3].pin = BSP_IO_PORT_09_PIN_14;
	s_pin_cfg_data[3].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[4].pin = BSP_IO_PORT_09_PIN_15;
	s_pin_cfg_data[4].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[5].pin = BSP_IO_PORT_09_PIN_03;
	s_pin_cfg_data[5].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[6].pin = BSP_IO_PORT_09_PIN_02;
	s_pin_cfg_data[6].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[7].pin = BSP_IO_PORT_09_PIN_10;
	s_pin_cfg_data[7].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[8].pin = BSP_IO_PORT_09_PIN_11;
	s_pin_cfg_data[8].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[9].pin = BSP_IO_PORT_09_PIN_12;
	s_pin_cfg_data[9].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[10].pin = BSP_IO_PORT_09_PIN_13;
	s_pin_cfg_data[10].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[11].pin = BSP_IO_PORT_09_PIN_04;
	s_pin_cfg_data[11].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[12].pin = BSP_IO_PORT_02_PIN_07;
	s_pin_cfg_data[12].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[13].pin = BSP_IO_PORT_11_PIN_07;
	s_pin_cfg_data[13].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[14].pin = BSP_IO_PORT_11_PIN_06;
	s_pin_cfg_data[14].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[15].pin = BSP_IO_PORT_11_PIN_05;
	s_pin_cfg_data[15].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[16].pin = BSP_IO_PORT_11_PIN_01;
	s_pin_cfg_data[16].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[17].pin = BSP_IO_PORT_11_PIN_04;
	s_pin_cfg_data[17].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[18].pin = BSP_IO_PORT_11_PIN_03;
	s_pin_cfg_data[18].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[19].pin = BSP_IO_PORT_11_PIN_02;
	s_pin_cfg_data[19].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[20].pin = BSP_IO_PORT_11_PIN_00;
	s_pin_cfg_data[20].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[21].pin = BSP_IO_PORT_07_PIN_07;
	s_pin_cfg_data[21].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[22].pin = BSP_IO_PORT_07_PIN_11;
	s_pin_cfg_data[22].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[23].pin = BSP_IO_PORT_07_PIN_12;
	s_pin_cfg_data[23].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[24].pin = BSP_IO_PORT_07_PIN_13;
	s_pin_cfg_data[24].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[25].pin = BSP_IO_PORT_07_PIN_14;
	s_pin_cfg_data[25].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[26].pin = BSP_IO_PORT_07_PIN_15;
	s_pin_cfg_data[26].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[27].pin = BSP_IO_PORT_07_PIN_10;
	s_pin_cfg_data[27].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[28].pin = BSP_IO_PORT_08_PIN_06;
	s_pin_cfg_data[28].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[29].pin = BSP_IO_PORT_08_PIN_05;
	s_pin_cfg_data[29].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[30].pin = BSP_IO_PORT_08_PIN_07;
	s_pin_cfg_data[30].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[31].pin = BSP_IO_PORT_05_PIN_13;
	s_pin_cfg_data[31].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;

	s_pin_cfg.number_of_pins = 32;
	s_pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&s_pin_cfg_data;
	s_pin_cfg.p_extend = NULL;

    R_IOPORT_Open(&s_pin_ctrl, &s_pin_cfg);
}

static void glcdcCallback(display_callback_args_t *p_args)
{
	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case DISPLAY_EVENT_GR1_UNDERFLOW:
		lcd->gr1_underflow_cnt++;
		break;
	case DISPLAY_EVENT_GR2_UNDERFLOW:
		lcd->gr2_underflow_cnt++;
		break;
	case DISPLAY_EVENT_LINE_DETECTION:
		lcd->vsync_cnt++;
		if (lcd->present_pending) {
			if (lcd->graphic_mem_2 != NULL) {
				if (lcd->graphic_writeable == lcd->graphic_mem_1) {
					lcd->graphic_writeable = lcd->graphic_mem_2;
				}
				else if (lcd->graphic_writeable == lcd->graphic_mem_2) {
					lcd->graphic_writeable = lcd->graphic_mem_1;
				}
			}

			__DMB();
			lcd->present_pending = false;
		}
		break;
	case DISPLAY_EVENT_FRAME_END:
		break;
	}
}

#endif /* #if LCD_EN_EK79001 */
