/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include "hal_data.h"
#include "lcd.h"

#if LCD_EN_EK79001
#include "lcd_ek79001.h"
#endif

#if LCD_EN_EK79007AD2
#include "lcd_ek79007ad2.h"
#endif

#if LCD_EN_ILI9881C
#include "lcd_ili9881c.h"
#endif

#if LCD_EN_ST7796U
#include "lcd_st7796u.h"
#endif

#if LCD_EN_TFP410PAPR
#include "lcd_tfp410.h"
#endif

/*================================== MACROS =======================================================*/

#ifndef LCD_EN_VOLATILE
#define LCD_EN_VOLATILE	0
#endif

#ifndef __LCD_DEBUG
#define __LCD_DEBUG 1
#endif

#if __LCD_DEBUG
#include <utils/log.h>
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define LCD_LOGD(msg, ...)                  LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGI(msg, ...)                  LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGW(msg, ...)                  LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGE(msg, ...)                  LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define LCD_LOGD(msg, ...)
#define LCD_LOGI(msg, ...)
#define LCD_LOGW(msg, ...)
#define LCD_LOGE(msg, ...)
#endif

#if LCD_EN_VOLATILE
#define VOLATILE volatile
#else
#define VOLATILE
#endif

/*================================== TYPES ========================================================*/

struct lcd_model_name_t {
    LCD_ModelEnum model;
    const char *name;
};

/*================================== GLOBAL VARIABLES =============================================*/

/*================================== LOCAL VARIABLES ==============================================*/

static const struct lcd_model_name_t sc_lcd_model_name[] = {
#if LCD_EN_EK79001
    {LCD_MODEL_EK79001, "EK79001"},
#endif
#if LCD_EN_EK79007AD2
    {LCD_MODEL_EK79007AD2, "EK79007AD2"},
#endif
#if LCD_EN_ILI9881C
    {LCD_MODEL_ILI9881C, "ILI9881C"},
#endif
#if LCD_EN_JD9365HD
    {LCD_MODEL_JD9365HD, "JD9365HD"},
#endif
#if LCD_EN_ST7796U
    {LCD_MODEL_ST7796U, "ST7796U"},
#endif
#if LCD_EN_TFP410PAPR
    {LCD_MODEL_TFP410PAPR, "TFP410PAPR"},
#endif
    {LCD_MODEL_UNKNOW, "Unknow"},
};

static const uint32_t sc_lcd_model_name_len = sizeof(sc_lcd_model_name) / sizeof(sc_lcd_model_name[0]);

/*================================== Private Functions Prototypes =================================*/

static void dirtyRectClear(LCD_DirtyRectType *rect);
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width);
static uint32_t prepareWriteBuffer(LCD_DeviceType *lcd, bool overwrite_all);

/*================================== Public Functions =============================================*/

uint32_t LCD_DrawBitmap(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *bitmap)
{
	uint16_t i;
	uint32_t err;

	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint16_t *p16_bp = NULL;
	VOLATILE uint32_t *p32_bg = NULL;
	VOLATILE uint32_t *p32_bp = NULL;

	if (lcd->drawBitmap != NULL) {
		return lcd->drawBitmap(lcd, x, y, length, width, bitmap);
	}

	if ((lcd->graphic_mem_1 == NULL) && (lcd->graphic_mem_2 == NULL)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}

    if ((length == 0) || (width == 0)) {
        return FSP_SUCCESS;
    }
    if (x >= lcd->x_length) {
        LCD_LOGW("X [%" PRIu16 "] out of range", x);
        return FSP_ERR_ASSERTION;
    }
    if (y >= lcd->y_length) {
        LCD_LOGW("Y [%" PRIu16 "] out of range", y);
        return FSP_ERR_ASSERTION;
    }

    if ((x + length) > lcd->x_length) {
        length = lcd->x_length - x;
    }
    if ((y + width) > lcd->y_length) {
        width = lcd->y_length - y;
    }

    err = prepareWriteBuffer(lcd, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (lcd->color_format) {
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (uint16_t *)lcd->graphic_writeable;
    	p16_bp = (uint16_t *)bitmap;
    	p16_bg = &p16_bg[y * (lcd->x_length + lcd->x_dummy) + x];
    	for (i = 0; i < width; i++) {
    		memcpy((void *)p16_bg, (void *)p16_bp, length * 2);
    		p16_bg = &p16_bg[lcd->x_length + lcd->x_dummy];
    		p16_bp = &p16_bp[length];
    	}
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    	p32_bg = (uint32_t *)lcd->graphic_writeable;
		p32_bp = (uint32_t *)bitmap;
    	p32_bg = &p32_bg[y * (lcd->x_length + lcd->x_dummy) + x];
    	for (i = 0; i < width; i++) {
    		memcpy((void *)p32_bg, (void *)p32_bp, length * 4);
    		p32_bg = &p32_bg[lcd->x_length + lcd->x_dummy];
    		p32_bp = &p32_bp[length];
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&lcd->dirty_current, x, y, length, width);

	return err;
}

uint32_t LCD_DrawPoint(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint32_t color)
{
	uint32_t err;

	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	if (lcd->drawPoint != NULL) {
		return lcd->drawPoint(lcd, x, y, color);
	}

	if ((lcd->graphic_mem_1 == NULL) && (lcd->graphic_mem_2 == NULL)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}
	if ((x >= lcd->x_length) || (y >= lcd->y_length)) {
        return FSP_ERR_ASSERTION;
    }

	err = prepareWriteBuffer(lcd, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (lcd->color_format) {
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)lcd->graphic_writeable;
    	p16_bg[y * (lcd->x_length + lcd->x_dummy) + x] = (uint16_t)color;
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    	p32_bg = (VOLATILE uint32_t *)lcd->graphic_writeable;
    	p32_bg[y * (lcd->x_length + lcd->x_dummy) + x] = color;
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&lcd->dirty_current, x, y, 1, 1);

	return err;
}

uint32_t LCD_Fill(LCD_DeviceType *lcd, uint32_t color)
{
	uint32_t i;
    uint32_t err;

    uint16_t total_length = lcd->x_length + lcd->x_dummy;
    uint16_t total_width = lcd->y_length + lcd->y_dummy;
    uint32_t total = total_length * total_width;
    VOLATILE uint16_t *p16_bg = NULL;
    VOLATILE uint32_t *p32_bg = NULL;

    if (lcd->fill != NULL) {
        return lcd->fill(lcd, color);
    }

    if ((lcd->graphic_mem_1 == NULL) && (lcd->graphic_mem_2 == NULL)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}

	err = prepareWriteBuffer(lcd, true);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (lcd->color_format) {
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)lcd->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p16_bg[i] = (uint16_t)color;
    	}
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    case LCD_COLOR_FORMAT_ARGB8888:
    	p32_bg = (VOLATILE uint32_t *)lcd->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p32_bg[i] = color;
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&lcd->dirty_current, 0, 0, total_length, total_width);

    return err;
}

uint32_t LCD_FillRectangle(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, uint32_t color)
{
	uint32_t i, j;
	uint32_t err;

	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	if (lcd->fillRectangle != NULL) {
		return lcd->fillRectangle(lcd, x, y, length, width, color);
	}

	if ((lcd->graphic_mem_1 == NULL) && (lcd->graphic_mem_2 == NULL)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}
	if ((length == 0) || (width == 0)) {
        return FSP_SUCCESS;
    }
    if ((x >= lcd->x_length) || (y >= lcd->y_length)) {
        return FSP_ERR_ASSERTION;
    }

    if ((x + length) > lcd->x_length) {
        length = lcd->x_length - x;
    }
    if ((y + width) > lcd->y_length) {
        width = lcd->y_length - y;
    }

    err = prepareWriteBuffer(lcd, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (lcd->color_format) {
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)lcd->graphic_writeable;
    	p16_bg = &p16_bg[y * (lcd->x_length + lcd->x_dummy)];
    	for (i = 0; i < width; i++) {
    		for (j = x; j < (x + length); j++) {
    			p16_bg[j] = (uint16_t)color;
    		}
    		p16_bg = &p16_bg[lcd->x_length + lcd->x_dummy];
    	}
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    case LCD_COLOR_FORMAT_ARGB8888:
    	p32_bg = (VOLATILE uint32_t *)lcd->graphic_writeable;
    	p32_bg = &p32_bg[y * (lcd->x_length + lcd->x_dummy)];
    	for (i = 0; i < width; i++) {
    		for (j = x; j < (x + length); j++) {
    			p32_bg[j] = color;
    		}
    		p32_bg = &p32_bg[lcd->x_length + lcd->x_dummy];
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&lcd->dirty_current, x, y, length, width);

	return err;
}

uint32_t LCD_Initialize(LCD_DeviceType *lcd, LCD_ConfigType *cfg)
{
    uint32_t i, err;

    if ((lcd == NULL) || (cfg == NULL)) {
    	LCD_LOGE("lcd or cfg is NULL");
    	return FSP_ERR_ASSERTION;
    }
    if ((cfg->peri_1 == NULL) && (cfg->peri_2 == NULL)) {
    	LCD_LOGE("Both peri_1 and peri_2 is NULL");
    	return FSP_ERR_ASSERTION;
    }
    if ((cfg->peri_model_1 == LCD_PERI_UNUSED) && (cfg->peri_model_2 == LCD_PERI_UNUSED)) {
    	LCD_LOGE("At least special a peri");
    	return FSP_ERR_ASSERTION;
    }

    lcd->peri_1 = cfg->peri_1;
    lcd->peri_2 = cfg->peri_2;
    lcd->peri_model_1 = cfg->peri_model_1;
    lcd->peri_model_2 = cfg->peri_model_2;
#if LCD_EN_PERI_I2C
    lcd->iicWrite = cfg->iicWrite;
    lcd->iicRead = cfg->iicRead;
#endif

    lcd->vsync_cnt = 0;
#if LCD_EN_PERI_GLCDC
    dirtyRectClear(&lcd->dirty_current);
	dirtyRectClear(&lcd->dirty_submitted);
	lcd->present_pending = false;

    lcd->gr1_underflow_cnt = 0;
    lcd->gr2_underflow_cnt = 0;
#endif
#if LCD_EN_PERI_MIPI
    lcd->mipi_timing_err_cnt = 0;
    lcd->mipi_underflow_cnt = 0;
    lcd->mipi_overflow_cnt = 0;
#endif

    if (lcd->peri_1 == NULL) {
    	lcd->peri_1 = lcd->peri_2;
    	lcd->peri_model_1 = lcd->peri_model_2;
    }
    if (lcd->peri_2 == NULL) {
    	lcd->peri_2 = lcd->peri_1;
    	lcd->peri_model_2 = lcd->peri_model_1;
    }

    switch (cfg->model) {
#if LCD_EN_EK79001
    case LCD_MODEL_EK79001:
        lcd->model = LCD_MODEL_EK79001;
        lcd->drawBitmap = NULL;
        lcd->drawPoint = NULL;
        lcd->fill = NULL;
        lcd->fillRectangle = NULL;
        lcd->init = LCD_EK79001_Init;
        lcd->isIdle = NULL;
        lcd->present = NULL;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_EK79007AD2
    case LCD_MODEL_EK79007AD2:
        lcd->model = LCD_MODEL_EK79007AD2;
        lcd->drawBitmap = NULL;
        lcd->drawPoint = NULL;
        lcd->fill = NULL;
        lcd->fillRectangle = NULL;
        lcd->init = LCD_EK79007AD2_Init;
        lcd->isIdle = NULL;
        lcd->present = NULL;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_ILI9881C
    case LCD_MODEL_ILI9881C:
        lcd->model = LCD_MODEL_ILI9881C;
        lcd->drawBitmap = NULL;
        lcd->drawPoint = NULL;
        lcd->fill = NULL;
        lcd->fillRectangle = NULL;
        lcd->init = LCD_ILI9881C_Init;
        lcd->isIdle = NULL;
        lcd->present = NULL;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_JD9365HD
    case LCD_MODEL_JD9365HD:
        lcd->model = LCD_MODEL_JD9365HD;
        break;
#endif
#if LCD_EN_ST7796U
    case LCD_MODEL_ST7796U:
        lcd->model = LCD_MODEL_ST7796U;
        lcd->drawBitmap = LCD_ST7796U_DrawBitmap;
        lcd->drawPoint = LCD_ST7796U_DrawPoint;
        lcd->fill = LCD_ST7796U_Fill;
        lcd->fillRectangle = LCD_ST7796U_FillRectangle;
        lcd->init = LCD_ST7796U_Init;
        lcd->isIdle = LCD_ST7796U_IsIdle;
        lcd->present = LCD_ST7796U_Present;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_TFP410PAPR
    case LCD_MODEL_TFP410PAPR:
        lcd->model = LCD_MODEL_TFP410PAPR;
        lcd->drawBitmap = NULL;
        lcd->drawPoint = NULL;
        lcd->fill = NULL;
        lcd->fillRectangle = NULL;
        lcd->init = LCD_TFP410_Init;
        lcd->isIdle = NULL;
        lcd->present = NULL;
        lcd->setBackLight = NULL;
        break;
#endif
    default:
        return FSP_ERR_UNSUPPORTED;
    }

    err = lcd->init(lcd);
    UNLIKE_RETURN(err, 0, "LCD init failed: %" PRIu32, err);

    for (i = 0; i < sc_lcd_model_name_len; i++) {
        if (lcd->model == sc_lcd_model_name[i].model) {
            lcd->name = sc_lcd_model_name[i].name;
            break;
        }
    }

    LCD_LOGD("LCD model   : %s", lcd->name);
    LCD_LOGD("LCD X length: %" PRIu16, lcd->x_length);
    LCD_LOGD("LCD X dummy : %" PRIu16, lcd->x_dummy);
    LCD_LOGD("LCD Y length: %" PRIu16, lcd->y_length);
    LCD_LOGD("LCD Y dummy : %" PRIu16, lcd->y_dummy);

    return err;
}

uint32_t LCD_IsIdle(LCD_DeviceType *lcd, bool *idle)
{
	if (lcd->isIdle != NULL) {
		return lcd->isIdle(lcd, idle);
	}

	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}

	*idle = lcd->present_pending ? false : true;

	return FSP_SUCCESS;
}

uint32_t LCD_Present(LCD_DeviceType *lcd)
{
	uint32_t err;

#if LCD_EN_PERI_GLCDC
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}

	uint8_t *submitted = lcd->graphic_writeable;
	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;

	if (lcd->present_pending) {
		return FSP_ERR_IN_USE;
	}

	if (lcd->graphic_mem_2 == NULL) {
		dirtyRectClear(&lcd->dirty_current);
		dirtyRectClear(&lcd->dirty_submitted);
		__DSB();
		lcd->present_pending = true;

		return FSP_SUCCESS;
	}

	if ((submitted != lcd->graphic_mem_1) && (submitted != lcd->graphic_mem_2)) {
		LCD_LOGE("graphic_writeable is not equal to mem_1 or mem_2 !");
		return FSP_ERR_ASSERTION;
	}
	if (lcd->dirty_current.valid == false) {
		/* 避免在没有新画面时切换到尚未同步的后台缓冲。 */
		return FSP_SUCCESS;
	}

	__DSB();
	err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, submitted, DISPLAY_FRAME_LAYER_1);
	if (err != FSP_SUCCESS) {
		if (err == FSP_ERR_INVALID_UPDATE_TIMING) {
			return FSP_ERR_IN_USE;
		}
		else {
			LCD_LOGE("Buffer change failed: %" PRIu32, err);
			return err;
		}
	}

	lcd->dirty_submitted = lcd->dirty_current;
	dirtyRectClear(&lcd->dirty_current);
	__DMB();
	lcd->present_pending = true;

	return FSP_SUCCESS;
#else
	if (lcd->present == NULL) {
		return FSP_ERR_UNSUPPORTED;
	}
	err = lcd->present(lcd);

	return err;
#endif
}

uint32_t LCD_SetBackLight(LCD_DeviceType *lcd, uint8_t percent)
{
	uint32_t err;

	if (lcd->setBackLight == NULL) {
		return FSP_ERR_UNSUPPORTED;
	}
	err = lcd->setBackLight(lcd, percent);

	return err;
}

/*================================== Private Functions ============================================*/

/**
 * @brief 清除脏矩形
 * @param[in,out] rect 脏矩形
 */
static void dirtyRectClear(LCD_DirtyRectType *rect)
{
	rect->valid = false;
}

/**
 * @brief 将给定区域合并到单个包围脏矩形中
 * @param[in,out] rect   脏矩形
 * @param[in]     x      区域左上角 X 坐标
 * @param[in]     y      区域左上角 Y 坐标
 * @param[in]     length 区域水平像素数
 * @param[in]     width  区域垂直像素数
 */
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width)
{
	uint32_t bottom;
	uint32_t right;
	uint16_t left;
	uint16_t top;

	if ((length == 0) || (width == 0)) {
		return;
	}
	if (rect->valid == false) {
		rect->x = x;
		rect->y = y;
		rect->length = length;
		rect->width = width;
		rect->valid = true;

		return;
	}

	right = ((uint32_t)rect->x + rect->length) > ((uint32_t)x + length) ?
			(uint32_t)rect->x + rect->length : (uint32_t)x + length;
	bottom = ((uint32_t)rect->y + rect->width) > ((uint32_t)y + width) ?
			 (uint32_t)rect->y + rect->width : (uint32_t)y + width;
	left = rect->x < x ? rect->x : x;
	top = rect->y < y ? rect->y : y;
	rect->x = left;
	rect->y = top;
	rect->length = (uint16_t)(right - rect->x);
	rect->width = (uint16_t)(bottom - rect->y);
}

/**
 * @brief 在绘制前准备可写帧缓冲
 * @details 局部绘制时同步上一已提交帧的脏矩形；整帧覆盖时直接丢弃待同步脏矩形。
 * @param[in,out] lcd LCD 设备实例
 * @param[in] overwrite_all 本次绘制将覆盖整个可写帧缓冲时设为 true
 * @retval FSP_SUCCESS       可写帧缓冲已准备完成
 * @retval FSP_ERR_ASSERTION 可写帧缓冲指针无效
 * @retval FSP_ERR_IN_USE    上一次 Present 尚未在帧边界完成锁存
 */
static uint32_t prepareWriteBuffer(LCD_DeviceType *lcd, bool overwrite_all)
{
	uint16_t row;
	uint16_t stride;

	uint8_t *source = NULL;
	uint16_t *p16_dst = NULL;
	uint16_t *p16_src = NULL;
	uint32_t *p32_dst = NULL;
	uint32_t *p32_src = NULL;

	LCD_DirtyRectType *rect = &lcd->dirty_submitted;

	if (lcd->present_pending) {
		return FSP_ERR_IN_USE;
	}
	if ((lcd->graphic_mem_2 == NULL) || (rect->valid == false)) {
		return FSP_SUCCESS;
	}
	if (overwrite_all) {
		dirtyRectClear(rect);
		return FSP_SUCCESS;
	}

	if (lcd->graphic_writeable == lcd->graphic_mem_1) {
		source = lcd->graphic_mem_2;
	}
	else if (lcd->graphic_writeable == lcd->graphic_mem_2) {
		source = lcd->graphic_mem_1;
	}
	else {
		LCD_LOGE("graphic_writeable is not equal to mem_1 or mem_2 !");
		return FSP_ERR_ASSERTION;
	}

	stride = lcd->x_length + lcd->x_dummy;
	switch (lcd->color_format) {
	case LCD_COLOR_FORMAT_RGB565:
		p16_src = (uint16_t *)source + rect->y * stride + rect->x;
		p16_dst = (uint16_t *)lcd->graphic_writeable + rect->y * stride + rect->x;
		for (row = 0; row < rect->width; row++) {
			memcpy(p16_dst, p16_src, rect->length * 2);
			p16_src = &p16_src[stride];
			p16_dst = &p16_dst[stride];
		}
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_src = (uint32_t *)source + rect->y * stride + rect->x;
		p32_dst = (uint32_t *)lcd->graphic_writeable + rect->y * stride + rect->x;
		for (row = 0; row < rect->width; row++) {
			memcpy(p32_dst, p32_src, rect->length * 4);
			p32_src = &p32_src[stride];
			p32_dst = &p32_dst[stride];
		}
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	dirtyRectClear(rect);

	return FSP_SUCCESS;
}
