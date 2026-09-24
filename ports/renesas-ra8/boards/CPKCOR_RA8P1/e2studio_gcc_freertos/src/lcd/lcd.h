#ifndef __LCD_H
#define __LCD_H

#include <stdbool.h>
#include <stdint.h>

#ifndef LCD_EN_PERI_GLCDC
#define LCD_EN_PERI_GLCDC	1
#endif

#ifndef LCD_EN_PERI_I2C
#define LCD_EN_PERI_I2C		1
#endif

#ifndef LCD_EN_PERI_MIPI
#define LCD_EN_PERI_MIPI	0
#endif

#ifndef LCD_EN_PERI_SCI_SPI
#define LCD_EN_PERI_SCI_SPI	0
#endif

#ifndef LCD_EN_PERI_SPI
#define LCD_EN_PERI_SPI		0
#endif

#if LCD_EN_PERI_GLCDC
#ifndef LCD_EN_EK79001
#define LCD_EN_EK79001      0
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_EK79007AD2
#define LCD_EN_EK79007AD2   1
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_ILI9881C
#define LCD_EN_ILI9881C     1
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_JD9365HD
#define LCD_EN_JD9365HD     0
#endif
#endif

#if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI || LCD_EN_PERI_SCI_SPI
#ifndef LCD_EN_ST7796U
#define LCD_EN_ST7796U      0
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_I2C
#ifndef LCD_EN_TFP410PAPR
#define LCD_EN_TFP410PAPR   1
#endif
#endif

#define LCD_COLOR_ARGB8888_BLACK		0x00000000
#define LCD_COLOR_ARGB8888_BLUE			0xFF0000FF
#define LCD_COLOR_ARGB8888_GREEN		0xFF00FF00
#define LCD_COLOR_ARGB8888_GREY			0xFF808080
#define LCD_COLOR_ARGB8888_LIGHT_BLUE	0xFF00FFFF
#define LCD_COLOR_ARGB8888_ORANGE		0xFFFF6600
#define LCD_COLOR_ARGB8888_PURPLE		0xFF9900FF
#define LCD_COLOR_ARGB8888_RED			0xFFFF0000
#define LCD_COLOR_ARGB8888_WHITE		0xFFFFFFFF
#define LCD_COLOR_ARGB8888_YELLOW		0xFFFFFF00

#define LCD_COLOR_RGB565_BLACK			0x0000
#define LCD_COLOR_RGB565_BLUE			0x001F
#define LCD_COLOR_RGB565_GREEN			0x07E0
#define LCD_COLOR_RGB565_GREY			0x8410
#define LCD_COLOR_RGB565_LIGHT_BLUE		0x07FF
#define LCD_COLOR_RGB565_ORANGE			0xFB20
#define LCD_COLOR_RGB565_PURPLE			0x981F
#define LCD_COLOR_RGB565_RED			0xF800
#define LCD_COLOR_RGB565_WHITE			0xFFFF
#define LCD_COLOR_RGB565_YELLOW			0xFFE0

#define LCD_COLOR_RGB888_BLACK			0x000000
#define LCD_COLOR_RGB888_BLUE			0x0000FF
#define LCD_COLOR_RGB888_GREEN			0x00FF00
#define LCD_COLOR_RGB888_GREY			0x808080
#define LCD_COLOR_RGB888_LIGHT_BLUE		0x00FFFF
#define LCD_COLOR_RGB888_ORANGE			0xFF6600
#define LCD_COLOR_RGB888_PURPLE			0x9900FF
#define LCD_COLOR_RGB888_RED			0xFF0000
#define LCD_COLOR_RGB888_WHITE			0xFFFFFF
#define LCD_COLOR_RGB888_YELLOW			0xFFFF00

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LCD_COLOR_FORMAT_ARGB1555,
    LCD_COLOR_FORMAT_ARGB4444,
    LCD_COLOR_FORMAT_ARGB8888,
    LCD_COLOR_FORMAT_RGB565,
    LCD_COLOR_FORMAT_RGB666,
    LCD_COLOR_FORMAT_RGB888
} LCD_ColorFormatEnum;

typedef enum {
#if LCD_EN_EK79001
    LCD_MODEL_EK79001,
#endif
#if LCD_EN_EK79007AD2
    LCD_MODEL_EK79007AD2,
#endif
#if LCD_EN_ILI9881C
    LCD_MODEL_ILI9881C,
#endif
#if LCD_EN_JD9365HD
    LCD_MODEL_JD9365HD,
#endif
#if LCD_EN_ST7796U
    LCD_MODEL_ST7796U,
#endif
#if LCD_EN_TFP410PAPR
    LCD_MODEL_TFP410PAPR,
#endif
    LCD_MODEL_UNKNOW
} LCD_ModelEnum;

typedef enum {
    LCD_ORIENTATION_HORIZONTAL,
    LCD_ORIENTATION_HORIZONTAL_FLIP,
    LCD_ORIENTATION_VERTICAL,
    LCD_ORIENTATION_VERTICAL_FLIP
} LCD_OrientationEnum;

typedef enum {
	LCD_PERI_UNUSED = 0,
    LCD_PERI_GLCDC,
    LCD_PERI_I2C,
    LCD_PERI_MIPI,
    LCD_PERI_SCI_SPI,
    LCD_PERI_SPI
} LCD_PeriModelEnum;

typedef struct {
    LCD_ModelEnum model;
    const void *peri_1;
    const void *peri_2;
    LCD_PeriModelEnum peri_model_1;
    LCD_PeriModelEnum peri_model_2;
#if LCD_EN_PERI_I2C
    /* 同步 IIC 回调：封装从机地址，返回 FSP 错误码；iicRead 先写寄存器地址再读取。 */
    uint32_t (*iicWrite)(uint8_t *data, uint8_t len);
    uint32_t (*iicRead)(uint8_t *data_w, uint8_t len_w, uint8_t *data_r, uint8_t len_r);
#endif
} LCD_ConfigType;

#if LCD_EN_PERI_GLCDC
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t length;
    uint16_t width;
    bool valid;
} LCD_DirtyRectType;
#endif

typedef struct LCD_DeviceType{
    uint16_t x_length;
    uint16_t y_length;
    uint16_t x_dummy;
    uint16_t y_dummy;

    LCD_ColorFormatEnum color_format;
    LCD_ModelEnum model;
    LCD_OrientationEnum orientation;

    bool acquired;
    uint8_t *graphic_mem_1;
    uint8_t *graphic_mem_2;			/* GLCDC/MIPI 模式下，如果只有一个帧缓冲，则此项设为 NULL */
    uint8_t *graphic_writeable;		/* GLCDC/MIPI 模式下，如果只有一个帧缓冲，则指向 mem_1；否则指向不被 GLCDC 读取的 mem */
    volatile uint32_t vsync_cnt;
#if LCD_EN_PERI_GLCDC
    LCD_DirtyRectType dirty_current;	/* 当前可写帧缓冲已修改区域的包围矩形 */
    LCD_DirtyRectType dirty_submitted;	/* 上一已提交帧需要同步到新可写帧缓冲的包围矩形 */
    volatile bool present_pending;
    volatile uint32_t gr1_underflow_cnt;
    volatile uint32_t gr2_underflow_cnt;
#endif

#if LCD_EN_PERI_MIPI
    volatile uint32_t mipi_timing_err_cnt;
    volatile uint32_t mipi_underflow_cnt;
    volatile uint32_t mipi_overflow_cnt;
#endif

    const void *peri_1;
    const void *peri_2;
    LCD_PeriModelEnum peri_model_1;
    LCD_PeriModelEnum peri_model_2;

    const char *name;

    uint32_t (*drawBitmap)(struct LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *bitmap);
    uint32_t (*drawPoint)(struct LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint32_t color);
    uint32_t (*fill)(struct LCD_DeviceType *lcd, uint32_t color);
    uint32_t (*fillRectangle)(struct LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, uint32_t color);
    uint32_t (*init)(struct LCD_DeviceType *lcd);
    uint32_t (*isIdle)(struct LCD_DeviceType *lcd, bool *idle);
    uint32_t (*present)(struct LCD_DeviceType *lcd);
    uint32_t (*setBackLight)(struct LCD_DeviceType *lcd, uint8_t percent);

#if LCD_EN_PERI_I2C
    uint32_t (*iicWrite)(uint8_t *data, uint8_t len);
    uint32_t (*iicRead)(uint8_t *data_w, uint8_t len_w, uint8_t *data_r, uint8_t len_r);
#endif
} LCD_DeviceType;

uint32_t LCD_DrawBitmap(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *bitmap);
uint32_t LCD_DrawPoint(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint32_t color);
uint32_t LCD_Fill(LCD_DeviceType *lcd, uint32_t color);
uint32_t LCD_FillRectangle(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, uint32_t color);
uint32_t LCD_Initialize(LCD_DeviceType *lcd, LCD_ConfigType *cfg);
uint32_t LCD_IsIdle(LCD_DeviceType *lcd, bool *idle);
uint32_t LCD_Present(LCD_DeviceType *lcd);
uint32_t LCD_SetBackLight(LCD_DeviceType *lcd, uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
