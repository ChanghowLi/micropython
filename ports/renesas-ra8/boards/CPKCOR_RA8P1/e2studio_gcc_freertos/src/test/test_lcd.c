#include "test.h"

#if TEST_EN_LCD

/*================================== INCLUDES =====================================================*/

#include <stdlib.h>
#include "hal_data.h"

#include "lcd/lcd.h"
#include "lcd/picture.h"
#include "perf_counter/perf_counter.h"
#include "utils/log.h"

/*================================== MACROS =======================================================*/

#ifndef TEST_LCD_EN_BACKLIGHT
#define TEST_LCD_EN_BACKLIGHT			0
#endif

#ifndef TEST_LCD_EN_BITMAP_BLOCK
#define TEST_LCD_EN_BITMAP_BLOCK		0
#endif

#ifndef TEST_LCD_EN_COLOR_BAND
#define TEST_LCD_EN_COLOR_BAND          0
#endif

#ifndef TEST_LCD_EN_COLOR_BLOCK
#define TEST_LCD_EN_COLOR_BLOCK			0
#endif

#ifndef TEST_LCD_EN_DRAW_POINT
#define TEST_LCD_EN_DRAW_POINT			0
#endif

#ifndef TEST_LCD_EN_FILL_SCREEN
#define TEST_LCD_EN_FILL_SCREEN			0
#endif

#ifndef TEST_LCD_EN_PICTURE
#define TEST_LCD_EN_PICTURE				0
#endif

#ifndef TEST_LCD_EN_PERFORMANCE
#define TEST_LCD_EN_PERFORMANCE			1
#endif

#ifndef TEST_LCD_EN_PERF_ONLY_FRESH
#define TEST_LCD_EN_PERF_ONLY_FRESH		0
#endif

#if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI

#ifndef TEST_LCD_EN_PRESURE
#define TEST_LCD_EN_PRESURE				0
#endif

#ifndef TEST_LCD_EN_PRESURE_REFINED
#define TEST_LCD_EN_PRESURE_REFINED		0
#endif

#endif /* #if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI */

#ifndef TEST_LCD_EN_USER_MALLOC
#define TEST_LCD_EN_USER_MALLOC			0
#endif

#if TEST_LCD_EN_PERFORMANCE || TEST_LCD_EN_PERF_ONLY_FRESH

#ifndef LCD_RUNNING_TIME_S
#define LCD_RUNNING_TIME_S				20
#endif

#endif

#if TEST_LCD_EN_PRESURE

#ifndef LCD_PRESURE_TIME_S
#define LCD_PRESURE_TIME_S				20
#endif

#endif

#if TEST_LCD_EN_PRESURE_REFINED

#ifndef LCD_PRESURE_REFINED_SIZE
#define LCD_PRESURE_REFINED_SIZE		512
#endif

#endif

#if TEST_LCD_EN_USER_MALLOC

#include TEST_LCD_USER_MALLOC_H

#ifndef TEST_LCD_MALLOC
#define TEST_LCD_MALLOC(x)				NULL
#endif

#ifndef TEST_LCD_FREE
#define TEST_LCD_FREE(x)
#endif

#endif /* #if TEST_LCD_EN_USER_MALLOC */

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

#if TEST_LCD_EN_PRESURE_REFINED
static float s_speed_sum_r[LCD_PRESURE_REFINED_SIZE];
static float s_speed_sum_w[LCD_PRESURE_REFINED_SIZE];
#endif

/*================================== Private Functions Prototypes =================================*/

#if TEST_LCD_EN_BACKLIGHT
static uint32_t testLCDBacklight(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_BITMAP_BLOCK
static uint32_t testLCDBitmapBlock(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_COLOR_BAND
static uint32_t testLCDColorBand(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_COLOR_BLOCK
static uint32_t testLCDColorBlock(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_DRAW_POINT
static uint32_t testLCDDrawPoint(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_FILL_SCREEN
static uint32_t testLCDFillScreen(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PICTURE
static uint32_t testLCDPicture(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PERFORMANCE
static uint32_t testLCDPerformance(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PERF_ONLY_FRESH
static uint32_t testLCDPerformanceOnlyFresh(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PRESURE || TEST_LCD_EN_PRESURE_REFINED

#include "sdram.h"

#if TEST_LCD_EN_PRESURE
static uint32_t testLCDPresure(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PRESURE_REFINED
static uint32_t testLCDPresureRefined(LCD_DeviceType *lcd);
#endif

#endif

/*================================== Public Functions =============================================*/

uint32_t TestLCD(void *lcd_device)
{
	LCD_DeviceType *lcd = (LCD_DeviceType *)lcd_device;

	(void)lcd;

#if TEST_LCD_EN_BACKLIGHT
	testLCDBacklight(lcd);
#endif

#if TEST_LCD_EN_BITMAP_BLOCK
	testLCDBitmapBlock(lcd);
#endif

#if TEST_LCD_EN_COLOR_BAND
	testLCDColorBand(lcd);
#endif

#if TEST_LCD_EN_COLOR_BLOCK
	testLCDColorBlock(lcd);
#endif

#if TEST_LCD_EN_DRAW_POINT
	testLCDDrawPoint(lcd);
#endif

#if TEST_LCD_EN_FILL_SCREEN
	testLCDFillScreen(lcd);
#endif

#if TEST_LCD_EN_PICTURE
	testLCDPicture(lcd);
#endif

#if TEST_LCD_EN_PERFORMANCE
	testLCDPerformance(lcd);
#endif

#if TEST_LCD_EN_PERF_ONLY_FRESH
	testLCDPerformanceOnlyFresh(lcd);
#endif

#if TEST_LCD_EN_PRESURE
	testLCDPresure(lcd);
#endif

#if TEST_LCD_EN_PRESURE_REFINED
	testLCDPresureRefined(lcd);
#endif

	return 0;
}

/*================================== Private Functions ============================================*/

#if TEST_LCD_EN_BACKLIGHT
static uint32_t testLCDBacklight(LCD_DeviceType *lcd)
{
	uint8_t i;
	uint32_t err;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_BACKLIGHT");
#if LCD_EN_ST7796U
	if (lcd->model == LCD_MODEL_ST7796U) {
		for (i = 0; i < 17; i++) {
			err = LCD_SetBackLight(lcd, i);
			if (err) {
				LOG_W(__FUNCTION__, "%s not support set backlight", lcd->name);
				break;
			}
			R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		}
	}
#endif

	return err;
}
#endif /* #if TEST_LCD_EN_BACKLIGHT */

#if TEST_LCD_EN_BITMAP_BLOCK
static uint32_t testLCDBitmapBlock(LCD_DeviceType *lcd)
{
	uint32_t i;
	uint32_t need_size;

	uint8_t *p8_bitmap = NULL;
	uint16_t *p16_bitmap = NULL;
	uint32_t *p32_bitmap = NULL;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_BITMAP_BLOCK");
	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		need_size = (lcd->x_length / 2) * (lcd->y_length / 2) * 2;
	}
	else {
		need_size = (lcd->x_length / 2) * (lcd->y_length / 2) * 4;
	}
#if TEST_LCD_EN_USER_MALLOC
	p8_bitmap = (uint8_t *)TEST_LCD_MALLOC(need_size);
#elif BSP_CFG_RTOS == 1
#elif BSP_CFG_RTOS == 2
#else
	p8_bitmap = (uint8_t *)malloc(need_size);
#endif
	if (p8_bitmap != NULL) {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
			p16_bitmap = (uint16_t *)p8_bitmap;
			for (i = 0; i < (need_size / 2); i++) {
				p16_bitmap[i] = LCD_COLOR_RGB565_RED;
			}
		}
		else {
			p32_bitmap = (uint32_t *)p32_bitmap;
			for (i = 0; i < (need_size / 4); i++) {
				p32_bitmap[i] = LCD_COLOR_RGB888_RED;
			}
		}
		LCD_DrawBitmap(lcd, 0, 0, lcd->x_length / 2, lcd->y_length / 2, p8_bitmap);
		LCD_Present(lcd);

		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
			for (i = 0; i < (need_size / 2); i++) {
				p16_bitmap[i] = LCD_COLOR_RGB565_GREEN;
			}
		}
		else {
			for (i = 0; i < (need_size / 4); i++) {
				p32_bitmap[i] = LCD_COLOR_RGB888_GREEN;
			}
		}
		LCD_DrawBitmap(lcd, lcd->x_length / 2, 0, lcd->x_length / 2, lcd->y_length / 2, p8_bitmap);
		LCD_Present(lcd);

		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
			for (i = 0; i < (need_size / 2); i++) {
				p16_bitmap[i] = LCD_COLOR_RGB565_BLUE;
			}
		}
		else {
			for (i = 0; i < (need_size / 4); i++) {
				p32_bitmap[i] = LCD_COLOR_RGB888_BLUE;
			}
		}
		LCD_DrawBitmap(lcd, 0, lcd->y_length / 2, lcd->x_length / 2, lcd->y_length / 2, p8_bitmap);
		LCD_Present(lcd);

		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
			for (i = 0; i < (need_size / 2); i++) {
				p16_bitmap[i] = LCD_COLOR_RGB565_ORANGE;
			}
		}
		else {
			for (i = 0; i < (need_size / 4); i++) {
				p32_bitmap[i] = LCD_COLOR_RGB888_ORANGE;
			}
		}
		LCD_DrawBitmap(lcd, lcd->x_length / 2, lcd->y_length / 2, lcd->x_length / 2, lcd->y_length / 2, p8_bitmap);
		LCD_Present(lcd);
	#if TEST_LCD_EN_USER_MALLOC
		TEST_LCD_FREE(p_bitmap);
	#elif BSP_CFG_RTOS == 1
	#elif BSP_CFG_RTOS == 2
	#else
		free(p8_bitmap);
#endif
	}
	else {
		LOG_W(__FUNCTION__, "Can't malloc %u bytes, ignore this case", need_size);
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_BITMAP_BLOCK */

#if TEST_LCD_EN_COLOR_BAND
static uint32_t testLCDColorBand(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t color[8];

	LOG_I(__FUNCTION__, "TEST_LCD_EN_COLOR_BAND");
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		color[0] = LCD_COLOR_RGB565_RED;
		color[1] = LCD_COLOR_RGB565_GREEN;
		color[2] = LCD_COLOR_RGB565_BLUE;
		color[3] = LCD_COLOR_RGB565_BLACK;
		color[4] = LCD_COLOR_RGB565_WHITE;
		color[5] = LCD_COLOR_RGB565_YELLOW;
		color[6] = LCD_COLOR_RGB565_PURPLE;
		color[7] = LCD_COLOR_RGB565_LIGHT_BLUE;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		color[0] = LCD_COLOR_RGB888_RED;
		color[1] = LCD_COLOR_RGB888_GREEN;
		color[2] = LCD_COLOR_RGB888_BLUE;
		color[3] = LCD_COLOR_RGB888_BLACK;
		color[4] = LCD_COLOR_RGB888_WHITE;
		color[5] = LCD_COLOR_RGB888_YELLOW;
		color[6] = LCD_COLOR_RGB888_PURPLE;
		color[7] = LCD_COLOR_RGB888_LIGHT_BLUE;
	}
	else {
		color[0] = LCD_COLOR_ARGB8888_RED;
		color[1] = LCD_COLOR_ARGB8888_GREEN;
		color[2] = LCD_COLOR_ARGB8888_BLUE;
		color[3] = LCD_COLOR_ARGB8888_BLACK;
		color[4] = LCD_COLOR_ARGB8888_WHITE;
		color[5] = LCD_COLOR_ARGB8888_YELLOW;
		color[6] = LCD_COLOR_ARGB8888_PURPLE;
		color[7] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
	}

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, &idle);
	}

    uint16_t step = (uint16_t)(lcd->y_length / (sizeof(color) / sizeof(color[0])));
    for (uint16_t i = 0; i < (sizeof(color) / sizeof(color[0])); i++) {
        LCD_FillRectangle(lcd, 0, i * step, lcd->x_length, step, color[i]);
    }

    LCD_Present(lcd);
    LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_COLOR_BAND */

#if TEST_LCD_EN_COLOR_BLOCK
static uint32_t testLCDColorBlock(LCD_DeviceType *lcd)
{
	uint32_t color[4];

	LOG_I(__FUNCTION__, "TEST_LCD_EN_COLOR_BLOCK");
	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		color[0] = LCD_COLOR_RGB565_RED;
		color[1] = LCD_COLOR_RGB565_GREEN;
		color[2] = LCD_COLOR_RGB565_BLUE;
		color[3] = LCD_COLOR_RGB565_ORANGE;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		color[0] = LCD_COLOR_RGB888_RED;
		color[1] = LCD_COLOR_RGB888_GREEN;
		color[2] = LCD_COLOR_RGB888_BLUE;
		color[3] = LCD_COLOR_RGB888_ORANGE;
	}
	else {
		color[0] = LCD_COLOR_ARGB8888_RED;
		color[1] = LCD_COLOR_ARGB8888_GREEN;
		color[2] = LCD_COLOR_ARGB8888_BLUE;
		color[3] = LCD_COLOR_ARGB8888_ORANGE;
	}
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 0, 0, lcd->x_length / 2, lcd->y_length / 2, color[0]);
	LCD_Present(lcd);
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, lcd->x_length / 2, 0, lcd->x_length / 2, lcd->y_length / 2, color[1]);
	LCD_Present(lcd);
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 0, lcd->y_length / 2, lcd->x_length / 2, lcd->y_length / 2, color[2]);
	LCD_Present(lcd);
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, lcd->x_length / 2, lcd->y_length / 2, lcd->x_length / 2, lcd->y_length / 2, color[3]);
	LCD_Present(lcd);

	return 0;
}
#endif /* #if TEST_LCD_EN_COLOR_BLOCK */

#if TEST_LCD_EN_DRAW_POINT
static uint32_t testLCDDrawPoint(LCD_DeviceType *lcd)
{
	bool idle;
	uint16_t x, y;
	uint32_t color;
	uint32_t err;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_DRAW_POINT");

	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		color = LCD_COLOR_RGB565_LIGHT_BLUE;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		color = LCD_COLOR_RGB888_LIGHT_BLUE;
	}
	else {
		color = LCD_COLOR_ARGB8888_LIGHT_BLUE;
	}

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	for (y = 0; y < lcd->y_length; y++) {
		for (x = 0; x < lcd->x_length; x++) {
			LCD_DrawPoint(lcd, x, y, color);

			err = LCD_Present(lcd);
			while (err) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				err = LCD_Present(lcd);
			}

			LCD_IsIdle(lcd, &idle);
			while (idle == false) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				LCD_IsIdle(lcd, &idle);
			}
		}
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_DRAW_POINT */

#if TEST_LCD_EN_FILL_SCREEN
static uint32_t testLCDFillScreen(LCD_DeviceType *lcd)
{
	LOG_I(__FUNCTION__, "TEST_LCD_EN_FILL_SCREEN");
	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_BLACK);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_BLUE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_GREEN);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_GREY);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_ORANGE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_PURPLE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_RED);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_WHITE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB565_YELLOW);
		LCD_Present(lcd);
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_BLACK);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_BLUE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_GREEN);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_GREY);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_ORANGE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_PURPLE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_RED);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_WHITE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_RGB888_YELLOW);
		LCD_Present(lcd);
	}
	else {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_BLACK);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_BLUE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_GREEN);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_GREY);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_ORANGE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_PURPLE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_RED);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_WHITE);
		LCD_Present(lcd);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, LCD_COLOR_ARGB8888_YELLOW);
		LCD_Present(lcd);
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_FILL_SCREEN */

#if TEST_LCD_EN_PICTURE
static uint32_t testLCDPicture(LCD_DeviceType *lcd)
{
	LOG_I(__FUNCTION__, "TEST_LCD_EN_PICTURE");
	LOG_I(__FUNCTION__, "You need to ensure that the orientation and format of the image are correct");
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_DrawBitmap(lcd, 0, 0, (uint16_t)g_picture_222x480_01.width, (uint16_t)g_picture_222x480_01.height, (const uint8_t *)g_picture_222x480_01.pixel_data);
	LCD_Present(lcd);
	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_SECONDS);
    LCD_DrawBitmap(lcd, 0, 0, (uint16_t)g_picture_222x480_02.width, (uint16_t)g_picture_222x480_02.height, (const uint8_t *)g_picture_222x480_02.pixel_data);
    LCD_Present(lcd);
	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_SECONDS);

	return 0;
}
#endif /* #if TEST_LCD_EN_PICTURE */

#if TEST_LCD_EN_PERFORMANCE
static uint32_t testLCDPerformance(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t i, err;
	int64_t time_start, time_end;
	int64_t time_fill_s, time_fill_e;
	int64_t time_wait_s, time_wait_e;
	uint32_t colors[8];
	float time_s;

	uint32_t frame_cnt = 0;
	int64_t time_fill = 0;
	int64_t time_wait = 0;
	int64_t time_stop = LCD_RUNNING_TIME_S * 1000;
	float fps = 0.0f;

	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PERFORMANCE");

	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		colors[0] = LCD_COLOR_RGB565_WHITE;
		colors[1] = LCD_COLOR_RGB565_BLUE;
		colors[2] = LCD_COLOR_RGB565_GREEN;
		colors[3] = LCD_COLOR_RGB565_GREY;
		colors[4] = LCD_COLOR_RGB565_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB565_ORANGE;
		colors[6] = LCD_COLOR_RGB565_PURPLE;
		colors[7] = LCD_COLOR_RGB565_RED;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		colors[0] = LCD_COLOR_RGB888_WHITE;
		colors[1] = LCD_COLOR_RGB888_BLUE;
		colors[2] = LCD_COLOR_RGB888_GREEN;
		colors[3] = LCD_COLOR_RGB888_GREY;
		colors[4] = LCD_COLOR_RGB888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB888_ORANGE;
		colors[6] = LCD_COLOR_RGB888_PURPLE;
		colors[7] = LCD_COLOR_RGB888_RED;
	}
	else {
		colors[0] = LCD_COLOR_ARGB8888_WHITE;
		colors[1] = LCD_COLOR_ARGB8888_BLUE;
		colors[2] = LCD_COLOR_ARGB8888_GREEN;
		colors[3] = LCD_COLOR_ARGB8888_GREY;
		colors[4] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_ARGB8888_ORANGE;
		colors[6] = LCD_COLOR_ARGB8888_PURPLE;
		colors[7] = LCD_COLOR_ARGB8888_RED;
	}

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	time_start = get_system_ms();
	while (1) {
		time_fill_s = get_system_us();
		LCD_Fill(lcd, colors[frame_cnt % 8]);
		time_fill_e = get_system_us();

		time_wait_s = time_fill_e;
		err = LCD_Present(lcd);
		while (err) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			err = LCD_Present(lcd);
		}

		LCD_IsIdle(lcd, &idle);
		while (idle == false) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			LCD_IsIdle(lcd, &idle);
		}
		time_wait_e = get_system_us();
		time_end = get_system_ms();

		frame_cnt++;
		time_fill += time_fill_e - time_fill_s;
		time_wait += time_wait_e - time_wait_s;
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)frame_cnt / time_s;
	printf("============== Write Each Frame ===============\r\n");
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_cnt);
	printf("Calculate FPS      : %.3f\r\n", fps);

	time_s = (float)time_fill / (float)frame_cnt / 1000.0f;
	printf("Average fill time  : %.3fms\r\n", time_s);
	time_s = (float)time_wait / (float)frame_cnt / 1000.0f;
	printf("Average wait time  : %.3fms\r\n", time_s);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	frame_cnt = lcd->vsync_cnt;
	gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif
	time_start = get_system_ms();
	for (i = 0; i < LCD_RUNNING_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	time_s = (float)(time_end - time_start) / 1000.0f;
	frame_cnt = lcd->vsync_cnt - frame_cnt;
	fps = (float)frame_cnt / time_s;
	printf("=============== Static Display ================\r\n");
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_cnt);
	printf("Calculate FPS      : %.3f\r\n", fps);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	return 0;
}
#endif /* #if TEST_LCD_EN_PERFORMANCE */

#if TEST_LCD_EN_PERF_ONLY_FRESH
static uint32_t testLCDPerformanceOnlyFresh(LCD_DeviceType *lcd)
{
	uint32_t i;
	uint32_t frame_start;
	uint32_t frame_end;
	int64_t time_start, time_end;
	float fps;
	float time_s;

	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PERF_ONLY_FRESH");


	frame_start = lcd->vsync_cnt;
	time_start = get_system_ms();
	for (i = 0; i < LCD_RUNNING_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	frame_end = lcd->vsync_cnt;

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)(frame_end - frame_start) / time_s;
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_end - frame_start);
	printf("Calculate FPS      : %.3f\r\n", fps);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	return 0;
}
#endif /* #if TEST_LCD_EN_PERF_ONLY_FRESH */

#if TEST_LCD_EN_PRESURE
static uint32_t testLCDPresure(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t i;
	uint32_t err;
	uint32_t colors[8];
	int64_t time_start, time_end;
	float sdram_speed_r[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_w[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_r_sum[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_w_sum[TEST_SDRAM_SPEED_COUNT];

	uint32_t frame_cnt = 0;
	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
	int64_t time_stop = LCD_PRESURE_TIME_S * 1000;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PRESURE");

	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		colors[0] = LCD_COLOR_RGB565_WHITE;
		colors[1] = LCD_COLOR_RGB565_BLUE;
		colors[2] = LCD_COLOR_RGB565_GREEN;
		colors[3] = LCD_COLOR_RGB565_GREY;
		colors[4] = LCD_COLOR_RGB565_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB565_ORANGE;
		colors[6] = LCD_COLOR_RGB565_PURPLE;
		colors[7] = LCD_COLOR_RGB565_RED;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		colors[0] = LCD_COLOR_RGB888_WHITE;
		colors[1] = LCD_COLOR_RGB888_BLUE;
		colors[2] = LCD_COLOR_RGB888_GREEN;
		colors[3] = LCD_COLOR_RGB888_GREY;
		colors[4] = LCD_COLOR_RGB888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB888_ORANGE;
		colors[6] = LCD_COLOR_RGB888_PURPLE;
		colors[7] = LCD_COLOR_RGB888_RED;
	}
	else {
		colors[0] = LCD_COLOR_ARGB8888_WHITE;
		colors[1] = LCD_COLOR_ARGB8888_BLUE;
		colors[2] = LCD_COLOR_ARGB8888_GREEN;
		colors[3] = LCD_COLOR_ARGB8888_GREY;
		colors[4] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_ARGB8888_ORANGE;
		colors[6] = LCD_COLOR_ARGB8888_PURPLE;
		colors[7] = LCD_COLOR_ARGB8888_RED;
	}

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	time_start = get_system_ms();
	while (1) {
		LCD_Fill(lcd, colors[frame_cnt % 8]);

		err = LCD_Present(lcd);
		while (err) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			err = LCD_Present(lcd);
		}

		LCD_IsIdle(lcd, &idle);
		while (idle == false) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			LCD_IsIdle(lcd, &idle);
		}

		frame_cnt++;

		time_end = get_system_ms();
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	printf("Result when not issue SDRAM WR\r\n");
	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	memset(sdram_speed_r_sum, 0, sizeof(sdram_speed_r_sum));
	memset(sdram_speed_w_sum, 0, sizeof(sdram_speed_w_sum));
	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	time_start = get_system_ms();
	frame_cnt = 0;
	while (1) {
		LCD_Fill(lcd, colors[frame_cnt % 8]);

		err = LCD_Present(lcd);
		while (err) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			err = LCD_Present(lcd);
		}

		TestSDRAM_SpeedWrite(sdram_speed_w, TEST_SDRAM_WIDTH_32BIT);
		TestSDRAM_SpeedRead(sdram_speed_r, TEST_SDRAM_WIDTH_32BIT);
		for (i = 0; i < TEST_SDRAM_SPEED_COUNT; i++) {
			sdram_speed_r_sum[i] += sdram_speed_r[i];
			sdram_speed_w_sum[i] += sdram_speed_w[i];
		}

		LCD_IsIdle(lcd, &idle);
		while (idle == false) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			LCD_IsIdle(lcd, &idle);
		}

		frame_cnt++;

		time_end = get_system_ms();
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	for (i = 0; i < TEST_SDRAM_SPEED_COUNT; i++) {
		sdram_speed_r_sum[i] = sdram_speed_r_sum[i] / (float)frame_cnt;
		sdram_speed_w_sum[i] = sdram_speed_w_sum[i] / (float)frame_cnt;
	}

	printf("Result when issue SDRAM WR\r\n");
	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	printf("Average SDRAM speed in 32bit width\r\n");
#if BSP_CFG_DCACHE_ENABLED
#else
	printf("Write from DTCM  to SDRAM with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_w_sum[0]);
	printf("Write from RAM   to SDRAM with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_w_sum[1]);
	printf("Read  from SDRAM to DTCM  with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_r_sum[0]);
	printf("Read  from SDRAM to RAM   with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_r_sum[1]);
#endif

	return 0;
}
#endif

#if TEST_LCD_EN_PRESURE_REFINED
static uint32_t testLCDPresureRefined(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t i, err;
	uint32_t color;
	int64_t time_start, time_end;
	float speed_r;
	float speed_w;
	TestSDRAM_WidthEnum width;
	volatile uint32_t frame_cnt;

	const char *width_info = NULL;
	uint32_t test_cnt_w = 0;
	uint32_t test_cnt_r = 0;

	(void)err;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PRESURE_REFINED");

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		color = LCD_COLOR_RGB565_GREY;
		width = TEST_SDRAM_WIDTH_16BIT;
		width_info = "16bit";
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		color = LCD_COLOR_RGB888_GREY;
		width = TEST_SDRAM_WIDTH_32BIT;
		width_info = "32bit";
	}
	else {
		color = LCD_COLOR_ARGB8888_GREY;
		width = TEST_SDRAM_WIDTH_32BIT;
		width_info = "32bit";
	}

	LCD_IsIdle(lcd, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, &idle);
	}

	/* Write speed test */
	frame_cnt = lcd->vsync_cnt;
	while (frame_cnt == lcd->vsync_cnt) {
		__NOP();
	}

#if 0
	LCD_Fill(lcd, color);
	err = LCD_Present(lcd);
	while (err) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		err = LCD_Present(lcd);
	}
#endif

	frame_cnt = lcd->vsync_cnt;
	time_start = get_system_ms();
	while (frame_cnt == lcd->vsync_cnt) {
		TestSDRAM_Speed(&speed_w, width, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
		s_speed_sum_w[test_cnt_w] = speed_w;
		test_cnt_w++;

		if (test_cnt_w >= LCD_PRESURE_REFINED_SIZE) {
			LOG_E(__FUNCTION__, "LCD_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			return 1;
		}
	}
	time_end = get_system_ms();

	printf("============ Writing speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", test_cnt_w);
	printf("SRAM ==> SDRAM, %s width\r\n", width_info);
	for (i = 0; i < test_cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
	}

	/* Read speed test */
	if (lcd->color_format == LCD_COLOR_FORMAT_RGB565) {
		color = LCD_COLOR_RGB565_GREEN;
	}
	else if (lcd->color_format == LCD_COLOR_FORMAT_RGB888) {
		color = LCD_COLOR_RGB888_GREEN;
	}
	else {
		color = LCD_COLOR_ARGB8888_GREEN;
	}

	frame_cnt = lcd->vsync_cnt;
	while (frame_cnt == lcd->vsync_cnt) {
		__NOP();
	}

#if 0
	LCD_Fill(lcd, color);
	err = LCD_Present(lcd);
	while (err) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		err = LCD_Present(lcd);
	}
#endif

	frame_cnt = lcd->vsync_cnt;
	time_start = get_system_ms();
	while (frame_cnt == lcd->vsync_cnt) {
		TestSDRAM_Speed(&speed_r, width, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
		s_speed_sum_r[test_cnt_r] = speed_r;
		test_cnt_r++;

		if (test_cnt_r >= LCD_PRESURE_REFINED_SIZE) {
			LOG_E(__FUNCTION__, "LCD_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			return 1;
		}
	}
	time_end = get_system_ms();

	printf("============ Reading speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", test_cnt_r);
	printf("SDRAM ==> SRAM, %s width\r\n", width_info);
	for (i = 0; i < test_cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
	}

	return 0;
}
#endif

#endif /* #if TEST_EN_LCD */
