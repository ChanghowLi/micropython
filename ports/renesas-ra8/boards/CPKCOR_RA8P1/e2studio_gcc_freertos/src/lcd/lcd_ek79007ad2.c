#include "lcd_ek79007ad2.h"

#if LCD_EN_EK79007AD2

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <stdbool.h>
#include "hal_data.h"
#include "utils/util.h"

/*================================== MACROS =======================================================*/

#define EK79007AD2_PIN_BACKLIGHT			BSP_IO_PORT_00_PIN_12
#define EK79007AD2_PIN_RESET				BSP_IO_PORT_00_PIN_13

#define MIPI_CMD_ID_DELAY					((mipi_cmd_id_t)0xFE)

#ifndef __EK79007AD2_DEBUG
#define __EK79007AD2_DEBUG	1
#endif

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef EK79007AD2_CACHE_NUM
#define EK79007AD2_CACHE_NUM	1
#endif

#if __EK79007AD2_DEBUG
#include "perf_counter/perf_counter.h"
#include "utils/log.h"
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define EK79007AD2_LOGD(msg, ...)           LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGI(msg, ...)           LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGW(msg, ...)           LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGE(msg, ...)   		LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define EK79007AD2_LOGD(msg, ...)
#define EK79007AD2_LOGI(msg, ...)
#define EK79007AD2_LOGW(msg, ...)
#define EK79007AD2_LOGE(msg, ...)
#endif

/*================================== TYPES ========================================================*/

struct mipi_init_table {
	uint8_t size;
	uint8_t buffer[10];
	mipi_cmd_id_t cmd_id;
	mipi_dsi_cmd_flag_t flags;
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[2];

static const struct mipi_init_table sc_mipi_init_table[] = {
	{2, {0xB2, 0x10}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x80, 0xAC}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x81, 0xB8}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x82, 0x09}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x83, 0x78}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x84, 0x7F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x85, 0xBB}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x86, 0x70}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER}
};

static volatile bool s_mipi_cmd_tx_done;

/*================================== Private Functions Prototypes =================================*/

static void pinConfig(void);
static void glcdcCallback(display_callback_args_t *p_args);
static void mipiCallback(mipi_dsi_callback_args_t *p_args);

/*================================== Public Functions =============================================*/

uint32_t LCD_EK79007AD2_Init(LCD_DeviceType *lcd)
{
	uint32_t err;
	size_t i, init_table_len;
	mipi_dsi_cmd_t msg;

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
	const mipi_dsi_instance_t *p_mipi = (const mipi_dsi_instance_t *)lcd->peri_2;
	mipi_dsi_instance_ctrl_t *p_mipi_ctrl = (mipi_dsi_instance_ctrl_t *)p_mipi->p_ctrl;

	pinConfig();
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79007AD2_PIN_RESET, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(EK79007AD2_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinWrite(EK79007AD2_PIN_BACKLIGHT, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		EK79007AD2_LOGI("GLCDC already open");
		p_glcdc->p_api->close(p_glcdc_ctrl);
	}
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;
	p_mipi_ctrl->p_callback = mipiCallback;
	p_mipi_ctrl->p_context = lcd;

	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	msg.channel = 0;
	msg.p_rx_buffer = NULL;
	init_table_len = sizeof(sc_mipi_init_table) / sizeof(sc_mipi_init_table[0]);
	for (i = 0; i < init_table_len; i++) {
		msg.cmd_id = sc_mipi_init_table[i].cmd_id;
		msg.flags = sc_mipi_init_table[i].flags;
		msg.p_tx_buffer = sc_mipi_init_table[i].buffer;
		msg.tx_len = sc_mipi_init_table[i].size;
		s_mipi_cmd_tx_done = false;
		err = p_mipi->p_api->command(p_mipi->p_ctrl, &msg);
		UNLIKE_RETURN(err, 0, "MIPI Command Error: %" PRIu32, err);
		while (s_mipi_cmd_tx_done == false) {
		#ifdef __OPTIMIZE__
			__nop();
		#endif
		}
		R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	}

	lcd->x_length = p_glcdc_ctrl->p_cfg->input[0].hsize;
	lcd->x_dummy = 0;
	lcd->y_length = p_glcdc_ctrl->p_cfg->input[0].vsize;
	lcd->y_dummy = 0;
	lcd->color_format = LCD_COLOR_FORMAT_ARGB8888;
	lcd->orientation = LCD_ORIENTATION_HORIZONTAL;
#if EK79007AD2_CACHE_NUM == 1
	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
	lcd->graphic_mem_2 = NULL;
	lcd->graphic_writeable = lcd->graphic_mem_1;
#else
	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
	lcd->graphic_mem_2 = &lcd->graphic_mem_1[lcd->x_length * lcd->y_length * 2];
	lcd->graphic_writeable = lcd->graphic_mem_2;
#endif

	memset(lcd->graphic_mem_1, 0xFF, lcd->x_length * lcd->y_length * 4);
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	p_glcdc->p_api->start(p_glcdc_ctrl);

	return err;
}

/*================================== Private Functions ============================================*/

static void pinConfig(void)
{
	s_pin_cfg_data[0].pin = EK79007AD2_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = EK79007AD2_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg.number_of_pins = 2;
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

static void mipiCallback(mipi_dsi_callback_args_t *p_args)
{
	uint32_t status;

	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case MIPI_DSI_EVENT_SEQUENCE_0:
		s_mipi_cmd_tx_done = true;
		break;
	case MIPI_DSI_EVENT_SEQUENCE_1:
		break;
	case MIPI_DSI_EVENT_VIDEO:
		status = (uint32_t)p_args->video_status;
		if (status & MIPI_DSI_VIDEO_STATUS_TIMING_ERROR) {
			lcd->mipi_timing_err_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_UNDERFLOW) {
			lcd->mipi_underflow_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_OVERFLOW) {
			lcd->mipi_overflow_cnt++;
		}
		break;
	case MIPI_DSI_EVENT_RECEIVE:
		break;
	case MIPI_DSI_EVENT_FATAL:
		break;
	case MIPI_DSI_EVENT_PHY:
		break;
	case MIPI_DSI_EVENT_POST_OPEN:
		break;
	case MIPI_DSI_EVENT_PRE_START:
		break;
	}
}

#endif /* #if LCD_EN_EK79007AD2 */
