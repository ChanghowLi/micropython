#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "hal_data.h"
#include "i2c.h"
#include "sci.h"
#include "lcd_board.h"
#include "portable.h"
#include "utils/log.h"

#define TAG __FUNCTION__

#if LCD_EN_ST7796U
static LCD_DeviceType s_lcd_st7796u;
#endif

#if LCD_EN_ST7796U && LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#define ST7796U_MIPI_FB_ALIGN 64U
#define ST7796U_MIPI_FB_BYTES (224U * 480U * 2U)

static bool s_st7796u_mipi_initialized;
static void *s_st7796u_mipi_fb_allocation;
static uint8_t *s_st7796u_mipi_framebuffer;
static display_cfg_t s_st7796u_mipi_glcdc_cfg;
static display_instance_t s_st7796u_mipi_glcdc;

static LCD_ConfigType s_st7796u_mipi_cfg = {
    .model = LCD_MODEL_ST7796U,
    .peri_1 = &s_st7796u_mipi_glcdc,
    .peri_2 = &g_mipi_dsi_st7796u,
    .peri_model_1 = LCD_PERI_GLCDC,
    .peri_model_2 = LCD_PERI_MIPI,
};

static bool ST7796U_MipiAllocateFramebuffer(void)
{
    if (s_st7796u_mipi_framebuffer != NULL) {
        return true;
    }

    void *allocation = pvPortMalloc(ST7796U_MIPI_FB_BYTES + ST7796U_MIPI_FB_ALIGN - 1U);
    if (allocation == NULL) {
        return false;
    }

    uintptr_t aligned = ((uintptr_t)allocation + ST7796U_MIPI_FB_ALIGN - 1U)
        & ~((uintptr_t)ST7796U_MIPI_FB_ALIGN - 1U);
    if (aligned < 0x68000000UL || aligned + ST7796U_MIPI_FB_BYTES > 0x6A000000UL) {
        vPortFree(allocation);
        return false;
    }

    s_st7796u_mipi_fb_allocation = allocation;
    s_st7796u_mipi_framebuffer = (uint8_t *)aligned;
    return true;
}

static void ST7796U_MipiFreeFramebuffer(void)
{
    s_st7796u_mipi_framebuffer = NULL;
    vPortFree(s_st7796u_mipi_fb_allocation);
    s_st7796u_mipi_fb_allocation = NULL;
}

static void ST7796U_MipiPrepareGlcdcCfg(void)
{
    s_st7796u_mipi_glcdc_cfg = g_lcdc_st7796u_mipi_cfg;
    s_st7796u_mipi_glcdc_cfg.input[0].p_base = (uint32_t *)s_st7796u_mipi_framebuffer;
    s_st7796u_mipi_glcdc.p_ctrl = g_lcdc_st7796u_mipi.p_ctrl;
    s_st7796u_mipi_glcdc.p_cfg = &s_st7796u_mipi_glcdc_cfg;
    s_st7796u_mipi_glcdc.p_api = g_lcdc_st7796u_mipi.p_api;
}
#endif

#if LCD_EN_ST7796U && LCD_EN_PERI_SCI_SPI
static uint32_t LCD_SpiWrite(const void *data, uint32_t len)
{
    return g_sci_spi4.p_api->write(g_sci_spi4.p_ctrl, (const uint8_t *)data, len, SPI_BIT_WIDTH_8_BITS);
}

static LCD_ConfigType s_st7796u_spi_cfg = {
    .model = LCD_MODEL_ST7796U,
    .peri_1 = &g_sci_spi4,
    .peri_model_1 = LCD_PERI_SCI_SPI,
    .peri_model_2 = LCD_PERI_SCI_SPI,
    .spiWrite = LCD_SpiWrite,
};
#endif

#if LCD_EN_TFP410PAPR
#define TFP410_IIC_ADDR 0x38U
#define TFP410_FB_ALIGN 64U
#define TFP410_FB_BYTES (1280U * 720U * 2U)
#define SDRAM_BASE_ADDR 0x68000000UL
#define SDRAM_END_ADDR  0x6A000000UL

static LCD_DeviceType s_lcd_tfp410;
static bool s_tfp410_initialized;
static void *s_tfp410_fb_allocation;
static uint8_t *s_tfp410_framebuffer;
static i2c_t s_tfp410_i2c = {
    .instance = &g_i2c_master0,
};

static uint32_t TFP410_IicWrite(uint8_t *data, uint8_t len)
{
    return IIC_Write(&s_tfp410_i2c, TFP410_IIC_ADDR, data, len);
}

static uint32_t TFP410_IicRead(uint8_t *data_w, uint8_t len_w, uint8_t *data_r, uint8_t len_r)
{
    if (data_w == NULL || len_w != 1U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    return IIC_ReadMemory(&s_tfp410_i2c, TFP410_IIC_ADDR, data_w[0], 1U, data_r, len_r);
}

static bool TFP410_AllocateFramebuffer(void)
{
    if (s_tfp410_framebuffer != NULL) {
        return true;
    }

    void *allocation = pvPortMalloc(TFP410_FB_BYTES + TFP410_FB_ALIGN - 1U);
    if (allocation == NULL) {
        return false;
    }

    uintptr_t aligned = ((uintptr_t)allocation + TFP410_FB_ALIGN - 1U)
        & ~((uintptr_t)TFP410_FB_ALIGN - 1U);
    if (aligned < SDRAM_BASE_ADDR || aligned + TFP410_FB_BYTES > SDRAM_END_ADDR) {
        vPortFree(allocation);
        return false;
    }

    s_tfp410_fb_allocation = allocation;
    s_tfp410_framebuffer = (uint8_t *)aligned;
    return true;
}

static void TFP410_FreeFramebuffer(void)
{
    s_tfp410_framebuffer = NULL;
    vPortFree(s_tfp410_fb_allocation);
    s_tfp410_fb_allocation = NULL;
}

/* The HDMI path uses GLCDC RGB output without a MIPI DSI device. */
static fsp_err_t TFP410_NoDsiOpen(mipi_dsi_ctrl_t * const ctrl, mipi_dsi_cfg_t const * const cfg)
{
    (void)ctrl;
    (void)cfg;
    return FSP_SUCCESS;
}

static fsp_err_t TFP410_NoDsiControl(mipi_dsi_ctrl_t * const ctrl)
{
    (void)ctrl;
    return FSP_SUCCESS;
}

static const mipi_dsi_api_t s_tfp410_no_dsi_api = {
    .open = TFP410_NoDsiOpen,
    .close = TFP410_NoDsiControl,
    .start = TFP410_NoDsiControl,
    .stop = TFP410_NoDsiControl,
};

static const mipi_dsi_instance_t s_tfp410_no_dsi = {
    .p_api = &s_tfp410_no_dsi_api,
};

static const glcdc_extended_cfg_t s_tfp410_glcdc_extend_cfg = {
    .tcon_hsync = GLCDC_TCON_PIN_1,
    .tcon_vsync = GLCDC_TCON_PIN_0,
    .tcon_de = GLCDC_TCON_PIN_2,
    .correction_proc_order = GLCDC_CORRECTION_PROC_ORDER_BRIGHTNESS_CONTRAST2GAMMA,
    .clksrc = GLCDC_CLK_SRC_INTERNAL,
    .clock_div_ratio = GLCDC_PANEL_CLK_DIVISOR_4,
    .dithering_mode = GLCDC_DITHERING_MODE_TRUNCATE,
    .dithering_pattern_A = GLCDC_DITHERING_PATTERN_11,
    .dithering_pattern_B = GLCDC_DITHERING_PATTERN_11,
    .dithering_pattern_C = GLCDC_DITHERING_PATTERN_11,
    .dithering_pattern_D = GLCDC_DITHERING_PATTERN_11,
    .phy_layer = (void *)&s_tfp410_no_dsi,
};

static display_cfg_t s_tfp410_glcdc_cfg;
static display_instance_t s_tfp410_glcdc;

static void TFP410_PrepareGlcdcCfg(void)
{
    s_tfp410_glcdc_cfg = g_lcdc_st7796u_mipi_cfg;

    s_tfp410_glcdc_cfg.input[0].p_base = (uint32_t *)s_tfp410_framebuffer;
    s_tfp410_glcdc_cfg.input[0].hsize = 1280U;
    s_tfp410_glcdc_cfg.input[0].vsize = 720U;
    s_tfp410_glcdc_cfg.input[0].hstride = 1280U;
    s_tfp410_glcdc_cfg.input[0].format = DISPLAY_IN_FORMAT_16BITS_RGB565;
    s_tfp410_glcdc_cfg.underflow_1_ipl = BSP_IRQ_DISABLED;
    s_tfp410_glcdc_cfg.underflow_1_irq = FSP_INVALID_VECTOR;
    s_tfp410_glcdc_cfg.input[1].p_base = NULL;

    s_tfp410_glcdc_cfg.output.htiming.total_cyc = 1440U;
    s_tfp410_glcdc_cfg.output.htiming.display_cyc = 1280U;
    s_tfp410_glcdc_cfg.output.htiming.back_porch = 80U;
    s_tfp410_glcdc_cfg.output.htiming.sync_width = 40U;
    s_tfp410_glcdc_cfg.output.vtiming.total_cyc = 750U;
    s_tfp410_glcdc_cfg.output.vtiming.display_cyc = 720U;
    s_tfp410_glcdc_cfg.output.vtiming.back_porch = 20U;
    s_tfp410_glcdc_cfg.output.vtiming.sync_width = 5U;
    s_tfp410_glcdc_cfg.output.format = DISPLAY_OUT_FORMAT_24BITS_RGB888;
    s_tfp410_glcdc_cfg.p_extend = (void *)&s_tfp410_glcdc_extend_cfg;

    /* Share the GLCDC hardware, but use independent HDMI timing and framebuffer. */
    s_tfp410_glcdc.p_ctrl = g_lcdc_st7796u_mipi.p_ctrl;
    s_tfp410_glcdc.p_cfg = &s_tfp410_glcdc_cfg;
    s_tfp410_glcdc.p_api = g_lcdc_st7796u_mipi.p_api;
}

static LCD_ConfigType s_tfp410_cfg = {
    .model = LCD_MODEL_TFP410PAPR,
    .peri_1 = &s_tfp410_glcdc,
    .peri_model_1 = LCD_PERI_GLCDC,
    .iicWrite = TFP410_IicWrite,
    .iicRead = TFP410_IicRead,
};
#endif

LCD_DeviceType *LCD_BoardInitialize(LCD_BoardMode mode)
{
    LCD_DeviceType *lcd;
    LCD_ConfigType *cfg;
    uint32_t err;

    switch (mode) {
    case LCD_STARTUP_TFP410:
#if LCD_EN_TFP410PAPR
        if (s_tfp410_initialized) {
            return &s_lcd_tfp410;
        }
        if (!TFP410_AllocateFramebuffer()) {
            LOG_E(TAG, "TFP410 SDRAM framebuffer allocation failed");
            return NULL;
        }
        err = IIC_Init(&s_tfp410_i2c);
        if (err != FSP_SUCCESS) {
            LOG_E(TAG, "TFP410 IIC initialization failed: 0x%08lX", (unsigned long)err);
            TFP410_FreeFramebuffer();
            return NULL;
        }
        TFP410_PrepareGlcdcCfg();
        lcd = &s_lcd_tfp410;
        cfg = &s_tfp410_cfg;
        break;
#else
        LOG_E(TAG, "TFP410 driver is disabled");
        return NULL;
#endif

    case LCD_STARTUP_ST7796U_MIPI:
#if LCD_EN_ST7796U && LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
        if (s_st7796u_mipi_initialized) {
            return &s_lcd_st7796u;
        }
        if (!ST7796U_MipiAllocateFramebuffer()) {
            LOG_E(TAG, "ST7796U MIPI SDRAM framebuffer allocation failed");
            return NULL;
        }
        ST7796U_MipiPrepareGlcdcCfg();
        lcd = &s_lcd_st7796u;
        cfg = &s_st7796u_mipi_cfg;
        break;
#else
        LOG_E(TAG, "ST7796U MIPI driver is disabled");
        return NULL;
#endif

    case LCD_STARTUP_ST7796U_SPI:
#if LCD_EN_ST7796U && LCD_EN_PERI_SCI_SPI
        if (!machine_sci_take(4U, MACHINE_SCI_OWNER_SPI)) {
            LOG_E(TAG, "SCI4 is busy");
            return NULL;
        }
        lcd = &s_lcd_st7796u;
        cfg = &s_st7796u_spi_cfg;
        break;
#else
        LOG_E(TAG, "ST7796U SPI driver is disabled");
        return NULL;
#endif

    default:
        LOG_E(TAG, "Invalid LCD startup mode: %lu", (unsigned long)mode);
        return NULL;
    }

    err = LCD_Initialize(lcd, cfg);
    if (err != FSP_SUCCESS) {
        LOG_E(TAG, "LCD initialization failed (mode %lu): 0x%08lX", (unsigned long)mode, (unsigned long)err);
#if LCD_EN_ST7796U && LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
        if (mode == LCD_STARTUP_ST7796U_MIPI) {
            (void)LCD_BoardDeinitialize(&s_lcd_st7796u);
        }
#endif
#if LCD_EN_TFP410PAPR
        if (mode == LCD_STARTUP_TFP410) {
            glcdc_instance_ctrl_t *glcdc_ctrl = (glcdc_instance_ctrl_t *)s_tfp410_glcdc.p_ctrl;
            if (glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
                if (s_tfp410_glcdc.p_api->close(s_tfp410_glcdc.p_ctrl) != FSP_SUCCESS) {
                    return NULL;
                }
            }
            s_tfp410_glcdc_cfg.input[0].p_base = NULL;
            (void)IIC_DeInit(&s_tfp410_i2c);
            TFP410_FreeFramebuffer();
        }
#endif
        return NULL;
    }
#if LCD_EN_ST7796U && LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
    if (mode == LCD_STARTUP_ST7796U_MIPI) {
        s_st7796u_mipi_initialized = true;
    }
#endif
#if LCD_EN_TFP410PAPR
    if (mode == LCD_STARTUP_TFP410) {
        s_tfp410_initialized = true;
    }
#endif
    return lcd;
}

uint32_t LCD_BoardDeinitialize(LCD_DeviceType *lcd)
{
#if LCD_EN_ST7796U && LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
    if (lcd == &s_lcd_st7796u && s_st7796u_mipi_framebuffer != NULL) {
        glcdc_instance_ctrl_t *glcdc_ctrl = (glcdc_instance_ctrl_t *)s_st7796u_mipi_glcdc.p_ctrl;
        if (glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
            fsp_err_t err = s_st7796u_mipi_glcdc.p_api->close(s_st7796u_mipi_glcdc.p_ctrl);
            if (err != FSP_SUCCESS) {
                return err;
            }
        }
        s_st7796u_mipi_glcdc_cfg.input[0].p_base = NULL;
        lcd->graphic_mem_1 = NULL;
        lcd->graphic_mem_2 = NULL;
        lcd->graphic_writeable = NULL;
        ST7796U_MipiFreeFramebuffer();
        s_st7796u_mipi_initialized = false;
        return FSP_SUCCESS;
    }
#endif
#if LCD_EN_TFP410PAPR
    if (lcd == &s_lcd_tfp410 && s_tfp410_framebuffer != NULL) {
        glcdc_instance_ctrl_t *glcdc_ctrl = (glcdc_instance_ctrl_t *)s_tfp410_glcdc.p_ctrl;
        if (glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
            fsp_err_t err = s_tfp410_glcdc.p_api->close(s_tfp410_glcdc.p_ctrl);
            if (err != FSP_SUCCESS) {
                return err; /* Never free memory while GLCDC may still be reading it. */
            }
        }
        s_tfp410_glcdc_cfg.input[0].p_base = NULL;
        lcd->graphic_mem_1 = NULL;
        lcd->graphic_mem_2 = NULL;
        lcd->graphic_writeable = NULL;
        (void)IIC_DeInit(&s_tfp410_i2c);
        TFP410_FreeFramebuffer();
        s_tfp410_initialized = false;
        return FSP_SUCCESS;
    }
#endif
    return FSP_ERR_UNSUPPORTED;
}
