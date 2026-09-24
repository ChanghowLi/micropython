#include "lcd_tfp410.h"

#if LCD_EN_TFP410PAPR

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <string.h>
#include "hal_data.h"

/*================================== MACROS =======================================================*/

/* For RGB interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef TFP410_CACHE_NUM
#define TFP410_CACHE_NUM                    2
#endif

#define TFP410_CTL1_MODE                    0x08
#define TFP410_CTL3_MODE                    0x0A
#define TFP410_DEV_ID                       0x0410
#define TFP410_DEV_ID_L                     0x02

#ifndef __TFP410_DEBUG
#define __TFP410_DEBUG                 		1
#endif

#if (TFP410_CACHE_NUM != 1) && (TFP410_CACHE_NUM != 2)
#error "TFP410_CACHE_NUM must be 1 or 2"
#endif

#if __TFP410_DEBUG
#include "utils/log.h"
#define TFP410_LOGE(msg, ...)               LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define TFP410_LOGI(msg, ...)               LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#else
#define TFP410_LOGE(msg, ...)
#define TFP410_LOGI(msg, ...)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;

/* 与 ep_lcd_hdmi 的 RGB 引脚配置一致，不操作 LCD 背光或复位引脚。 */
static const ioport_pin_cfg_t sc_pin_cfg_data[] = {
	{ .pin = BSP_IO_PORT_02_PIN_07, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_05_PIN_13, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_05_PIN_14, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_05_PIN_15, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_07, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_11, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_12, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_13, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_14, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_07_PIN_15, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_08_PIN_05, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_08_PIN_06, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_08_PIN_07, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_02, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_03, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_04, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_10, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_11, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_12, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_13, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_14, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_09_PIN_15, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_00, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_01, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_02, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_03, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_04, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_05, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_06, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
	{ .pin = BSP_IO_PORT_11_PIN_07, .pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS },
};

static const ioport_cfg_t sc_pin_cfg = {
	.number_of_pins = sizeof(sc_pin_cfg_data) / sizeof(sc_pin_cfg_data[0]),
	.p_pin_cfg_data = sc_pin_cfg_data,
	.p_extend = NULL,
};

/*================================== Private Functions Prototypes =================================*/

static void glcdcCallback(display_callback_args_t *p_args);

/*================================== Public Functions =============================================*/

/**
 * @brief 初始化 TFP410 和 GLCDC，绘图及提交使用 lcd.c 的默认实现。
 * @param lcd peri_1 为 GLCDC，iicWrite/iicRead 为已绑定从机地址的同步 IIC 回调。
 * @return 成功返回 FSP_SUCCESS，失败返回参数、器件 ID 或底层驱动错误码。
 * @note IIC 总线应由调用方提前初始化；本驱动不使用 peri_2。
 */
uint32_t LCD_TFP410_Init(LCD_DeviceType *lcd)
{
	uint32_t err;
	uint32_t close_err;
	uint32_t bytes_per_pixel;
	uint32_t frame_bytes;
	uint16_t dev_id;
	uint8_t data_r[2];
	uint8_t data_w[3];

	if (lcd == NULL) {
		return FSP_ERR_ASSERTION;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}
	if ((lcd->peri_1 == NULL) || (lcd->iicWrite == NULL) || (lcd->iicRead == NULL)) {
		TFP410_LOGE("GLCDC or IIC callback is NULL");
		return FSP_ERR_ASSERTION;
	}

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	if ((p_glcdc->p_ctrl == NULL) || (p_glcdc->p_cfg == NULL) || (p_glcdc->p_api == NULL)) {
		return FSP_ERR_ASSERTION;
	}
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
	const display_input_cfg_t *p_input = &p_glcdc->p_cfg->input[0];

	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		return FSP_ERR_ASSERTION;
	}

	switch (p_input->format) {
	case DISPLAY_IN_FORMAT_16BITS_RGB565:
		lcd->color_format = LCD_COLOR_FORMAT_RGB565;
		bytes_per_pixel = 2;
		break;
	case DISPLAY_IN_FORMAT_32BITS_RGB888:
		lcd->color_format = LCD_COLOR_FORMAT_RGB888;
		bytes_per_pixel = 4;
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	data_w[0] = TFP410_DEV_ID_L;
	err = lcd->iicRead(data_w, 1, data_r, 2);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Read DEV_ID failed: %" PRIu32, err);
	dev_id = (uint16_t)((uint16_t)data_r[0] | ((uint16_t)data_r[1] << 8));
	if (dev_id != TFP410_DEV_ID) {
		TFP410_LOGE("Check DEV_ID failed, expect: 0x%04X, read: 0x%04X",
			(unsigned int)TFP410_DEV_ID, (unsigned int)dev_id);
		return FSP_ERR_UNSUPPORTED;
	}

	/* 沿用 ep_lcd_hdmi 的配置：0x08 = 0xBF、0x09 = 0x70、0x0A = 0x90。 */
	data_w[0] = TFP410_CTL1_MODE;
	data_w[1] = 0xBF;
	data_w[2] = 0x70;
	err = lcd->iicWrite(data_w, 3);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Write CTL1/CTL2 failed: %" PRIu32, err);
	data_w[0] = TFP410_CTL3_MODE;
	data_w[1] = 0x90;
	err = lcd->iicWrite(data_w, 2);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Write CTL3 failed: %" PRIu32, err);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		TFP410_LOGI("GLCDC already open");
		err = p_glcdc->p_api->close(p_glcdc_ctrl);
		UNLIKE_RETURN(err, FSP_SUCCESS, "Close failed: %" PRIu32, err);
	}

	err = R_IOPORT_Open(&s_pin_ctrl, &sc_pin_cfg);
	if (err == FSP_ERR_ALREADY_OPEN) {
		err = R_IOPORT_PinsCfg(&s_pin_ctrl, &sc_pin_cfg);
	}
	UNLIKE_RETURN(err, FSP_SUCCESS, "Pin config failed: %" PRIu32, err);
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;

	lcd->x_length = p_input->hsize;
	lcd->x_dummy = (uint16_t)(p_input->hstride - p_input->hsize);
	lcd->y_length = p_input->vsize;
	lcd->y_dummy = 0;
	lcd->orientation = LCD_ORIENTATION_HORIZONTAL;
	frame_bytes = p_input->hstride * p_input->vsize * bytes_per_pixel;

	lcd->graphic_mem_1 = (uint8_t *)p_input->p_base;
#if TFP410_CACHE_NUM == 1
	lcd->graphic_mem_2 = NULL;
	lcd->graphic_writeable = lcd->graphic_mem_1;
#else
	lcd->graphic_mem_2 = &lcd->graphic_mem_1[frame_bytes];
	lcd->graphic_writeable = lcd->graphic_mem_2;
#endif

	lcd->dirty_current.valid = false;
	lcd->dirty_submitted.valid = false;
	lcd->present_pending = false;
	lcd->vsync_cnt = 0;
	lcd->gr1_underflow_cnt = 0;
	lcd->gr2_underflow_cnt = 0;

	memset(lcd->graphic_mem_1, 0x00, frame_bytes);
#if TFP410_CACHE_NUM == 2
	memset(lcd->graphic_mem_2, 0x00, frame_bytes);
#endif
	__DSB();
	err = p_glcdc->p_api->start(p_glcdc_ctrl);
	if (err != FSP_SUCCESS) {
		TFP410_LOGE("Start failed: %" PRIu32, err);
		close_err = p_glcdc->p_api->close(p_glcdc_ctrl);
		if (close_err != FSP_SUCCESS) {
			TFP410_LOGE("Close after start failure failed: %" PRIu32, close_err);
		}
	}

	return err;
}

/*================================== Private Functions ============================================*/

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

#endif /* #if LCD_EN_TFP410PAPR */
