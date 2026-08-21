#ifndef __LCD_H
#define __LCD_H

#ifndef LCD_EN_EK79001
#define LCD_EN_EK79001      0
#endif

#ifndef LCD_EN_EK79007AD2
#define LCD_EN_EK79007AD2   0
#endif

#ifndef LCD_EN_ILI9881C
#define LCD_EN_ILI9881C     0
#endif

#ifndef LCD_EN_JD9365HD
#define LCD_EN_JD9365HD     0
#endif

#ifndef LCD_EN_ST7796U
#define LCD_EN_ST7796U      1
#endif

#ifndef LCD_EN_TFP410PAPR
#define LCD_EN_TFP410PAPR   0
#endif

#include <stdint.h>

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

typedef struct {
    uint16_t x_length;
    uint16_t y_length;
    uint16_t x_dummy;
    uint16_t y_dummy;

    LCD_ColorFormatEnum color_format;
    LCD_ModelEnum model;
    LCD_OrientationEnum orientation;

    const void *peri_inst;
} LCD_DeviceType;

uint32_t LCD_Initialize(LCD_DeviceType *lcd, LCD_ModelEnum model, void *peri_inst);

#ifdef __cplusplus
}
#endif

#endif
