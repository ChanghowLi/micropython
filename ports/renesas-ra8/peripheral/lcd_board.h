#ifndef LCD_BOARD_H
#define LCD_BOARD_H

#include "lcd/lcd.h"

typedef enum {
    LCD_STARTUP_TFP410 = 1,
    LCD_STARTUP_ST7796U_MIPI,
    LCD_STARTUP_ST7796U_SPI,
} LCD_BoardMode;

/* Initializes one display and returns its device, or NULL on failure. */
LCD_DeviceType *LCD_BoardInitialize(LCD_BoardMode mode);

/* Stop GLCDC before releasing HDMI or MIPI dynamically allocated framebuffer. */
uint32_t LCD_BoardDeinitialize(LCD_DeviceType *lcd);

#endif
