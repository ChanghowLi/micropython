#ifndef __LCD_ST7796U_H
#define __LCD_ST7796U_H

#include "lcd.h"

#if LCD_EN_ST7796U

#ifdef __cplusplus
extern "C" {
#endif

uint32_t LCD_ST7796U_Init(LCD_DeviceType *lcd);

#ifdef __cplusplus
}
#endif

#endif /* #if LCD_EN_ST7796U */

#endif
