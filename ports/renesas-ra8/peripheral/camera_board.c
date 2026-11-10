#include "camera_board.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "hal_data.h"
#include "portable.h"
#include "task.h"
#include "r_gpt.h"
#include "camera/ov5640.h"
#include "camera/ov7725.h"
#include "lcd_board.h"

typedef struct {
    Camera camera;
    i2c_t *owned_bus;
    void *frame_allocation;
    bool initialized;
} CameraBoardState;

static CameraBoardState s_dvp_camera;
static CameraBoardState s_mipi_camera;
static volatile bool s_dual_owned;
static bool s_dual_running;
static bool s_dual_xclk_open;

#define CAMERA_DUAL_DVP_ADDRESS 0x3DU
#define CAMERA_DUAL_DVP_FRAME_ALIGN 128U
#define CAMERA_DUAL_DVP_FRAME_BYTES (640U * 480U * 2U)
#define CAMERA_DUAL_DVP_FRAME_COUNT 3U

#if CAMERA_EN_MIPI
#define CAMERA_MIPI_FRAME_ALIGN 128U
#define CAMERA_MIPI_FRAME_BYTES (1024U * 600U * 2U)
#define CAMERA_MIPI_FRAME_COUNT 3U

static capture_cfg_t s_mipi_capture_cfg;
static vin_extended_cfg_t s_mipi_vin_cfg;
static capture_instance_t s_mipi_instance;
#endif

static CameraBoardState *cameraBoardState(CameraInterfaceEnum interface)
{
    if (interface == CAMERA_INTERFACE_DVP) {
        return &s_dvp_camera;
    }
    if (interface == CAMERA_INTERFACE_MIPI_CSI) {
        return &s_mipi_camera;
    }
    return NULL;
}

uint32_t Camera_BoardInitializeWithConfig(const CameraConfig *config, Camera **camera)
{
    if (camera == NULL) {
        return FSP_ERR_INVALID_POINTER;
    }
    *camera = NULL;
    if (config == NULL || config->i2c == NULL || config->i2c->instance == NULL ||
        config->peripheral_inst == NULL) {
        return FSP_ERR_INVALID_POINTER;
    }
    CameraBoardState *state = cameraBoardState(config->interface);
    if (state == NULL || config->length == 0U || config->width == 0U ||
        (config->model != CAMERA_MODEL_OV5640 && config->model != CAMERA_MODEL_OV7725)) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (config->interface == CAMERA_INTERFACE_MIPI_CSI &&
        config->model != CAMERA_MODEL_OV5640) {
        return FSP_ERR_UNSUPPORTED;
    }
#if !CAMERA_EN_DVP
    if (config->interface == CAMERA_INTERFACE_DVP) {
        return FSP_ERR_UNSUPPORTED;
    }
#endif
#if !CAMERA_EN_MIPI
    if (config->interface == CAMERA_INTERFACE_MIPI_CSI) {
        return FSP_ERR_UNSUPPORTED;
    }
#endif
    if (state->initialized || state->owned_bus != NULL) {
        return FSP_ERR_IN_USE;
    }
    CameraBoardState *other = state == &s_dvp_camera ? &s_mipi_camera : &s_dvp_camera;
    /* Initialization starts at the default SCCB address. Sharing a bus is
     * allowed only after the other OV5640 has been moved to another address. */
    if (other->initialized && config->model == CAMERA_MODEL_OV5640 &&
        other->camera.model == CAMERA_MODEL_OV5640 &&
        other->camera.i2c_address == OV5640_IIC_ADDR &&
        config->i2c->instance->p_ctrl == other->camera.i2c->instance->p_ctrl) {
        return FSP_ERR_IN_USE;
    }

    uint32_t err = CameraInit(&state->camera, config);
    if (err != FSP_SUCCESS) {
        return err;
    }
    state->initialized = true;
    *camera = &state->camera;
    return FSP_SUCCESS;
}

uint32_t Camera_BoardInitialize(CameraModelEnum model, Camera **camera)
{
    if (camera == NULL || (model != CAMERA_MODEL_OV5640 && model != CAMERA_MODEL_OV7725)) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *camera = NULL;
    if (s_dvp_camera.initialized) {
        /* Only reuse the legacy board-owned configuration here. */
        if (s_dvp_camera.camera.model != model || s_dvp_camera.owned_bus == NULL) {
            return FSP_ERR_IN_USE;
        }
        *camera = &s_dvp_camera.camera;
        return FSP_SUCCESS;
    }
    if (s_dvp_camera.owned_bus != NULL) {
        return FSP_ERR_IN_USE;
    }
    i2c_t *bus = NULL;
    uint32_t err = IIC_BoardAcquire(&bus);
    if (err != FSP_SUCCESS) {
        return err;
    }
    CameraConfig config = {
        .i2c = bus,
        .model = model,
        .interface = CAMERA_INTERFACE_DVP,
        .length = 640,
        .width = 480,
        .peripheral_inst = model == CAMERA_MODEL_OV5640
            ? (const void *)&g_ceu_ov5640 : (const void *)&g_ceu_ov7725,
    };
    err = Camera_BoardInitializeWithConfig(&config, camera);
    /* Keep ownership if release fails so Deinitialize can retry. */
    if (err == FSP_SUCCESS || IIC_BoardRelease(bus) != FSP_SUCCESS) {
        s_dvp_camera.owned_bus = bus;
    }
    return err;
}

uint32_t Camera_BoardInitializeMIPI(Camera **camera)
{
    if (camera == NULL) {
        return FSP_ERR_INVALID_POINTER;
    }
    *camera = NULL;
#if CAMERA_EN_MIPI
    if (s_mipi_camera.initialized) {
        if (s_mipi_camera.frame_allocation == NULL) {
            return FSP_ERR_IN_USE;
        }
        *camera = &s_mipi_camera.camera;
        return FSP_SUCCESS;
    }
    if (s_mipi_camera.owned_bus != NULL || s_mipi_camera.frame_allocation != NULL ||
        g_mipi_phy_st7796u.p_ctrl->open != 0U) {
        return FSP_ERR_IN_USE;
    }
    const vin_extended_cfg_t *vin_cfg = g_vin_ov5640_cfg.p_extend;
    if (vin_cfg->input_ctrl.image_stride != 1024U ||
        vin_cfg->input_ctrl.preclip.pixel_start != 0U ||
        vin_cfg->input_ctrl.preclip.pixel_end != 1023U ||
        vin_cfg->input_ctrl.preclip.line_start != 0U ||
        vin_cfg->input_ctrl.preclip.line_end != 599U ||
        vin_cfg->input_ctrl.cfg_bits.scaling_enable) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    i2c_t *bus = NULL;
    uint32_t err = IIC_BoardAcquire(&bus);
    if (err != FSP_SUCCESS) {
        return err;
    }
    /* Match the generated 1024x600 VIN configuration. One allocation keeps
     * all three 128-byte-aligned frames in the uncached FreeRTOS SDRAM heap. */
    const size_t bytes = CAMERA_MIPI_FRAME_BYTES * CAMERA_MIPI_FRAME_COUNT;
    void *allocation = pvPortMalloc(bytes + CAMERA_MIPI_FRAME_ALIGN - 1U);
    if (allocation == NULL) {
        err = FSP_ERR_OUT_OF_MEMORY;
        goto release_bus;
    }
    uintptr_t aligned = ((uintptr_t)allocation + CAMERA_MIPI_FRAME_ALIGN - 1U)
        & ~((uintptr_t)CAMERA_MIPI_FRAME_ALIGN - 1U);
    if (aligned < 0x68000000UL || aligned > 0x6A000000UL - bytes) {
        err = FSP_ERR_INVALID_ADDRESS;
        goto release_buffer;
    }
    s_mipi_vin_cfg = *vin_cfg;
    for (uint32_t i = 0; i < CAMERA_MIPI_FRAME_COUNT; i++) {
        s_mipi_vin_cfg.output_ctrl.image_buffer[i] =
            (uint8_t *)aligned + i * CAMERA_MIPI_FRAME_BYTES;
    }
    s_mipi_vin_cfg.output_ctrl.use_runtime_buffer = false;
    s_mipi_capture_cfg = g_vin_ov5640_cfg;
    s_mipi_capture_cfg.p_extend = &s_mipi_vin_cfg;
    s_mipi_instance = g_vin_ov5640;
    s_mipi_instance.p_cfg = &s_mipi_capture_cfg;
    CameraConfig config = {
        .i2c = bus,
        .model = CAMERA_MODEL_OV5640,
        .interface = CAMERA_INTERFACE_MIPI_CSI,
        .length = 1024,
        .width = 600,
        .peripheral_inst = &s_mipi_instance,
    };
    err = Camera_BoardInitializeWithConfig(&config, camera);
    if (err == FSP_SUCCESS) {
        s_mipi_camera.owned_bus = bus;
        s_mipi_camera.frame_allocation = allocation;
        return FSP_SUCCESS;
    }
release_buffer:
    vPortFree(allocation);
release_bus:
    if (IIC_BoardRelease(bus) != FSP_SUCCESS) {
        s_mipi_camera.owned_bus = bus;
    }
    return err;
#else
    return FSP_ERR_UNSUPPORTED;
#endif
}

uint32_t Camera_BoardDeinitializeInterface(CameraInterfaceEnum interface)
{
    CameraBoardState *state = cameraBoardState(interface);
    if (state == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (state->initialized) {
        /* Close CEU/VIN even if a single-frame callback cleared capture_start. */
        uint32_t err = CameraStopCapture(&state->camera);
        if (err != FSP_SUCCESS && err != FSP_ERR_NOT_OPEN) {
            return err;
        }
        state->initialized = false;
        state->camera.image = NULL;
        state->camera.image_ready = NULL;
    }
    if (s_dual_owned) {
        /* Keep a closed module off the shared SCCB bus. Init will release
         * its reset only after the other module has been isolated/remapped. */
        uint32_t err = OV5640_HoldReset(interface);
        if (err != FSP_SUCCESS) {
            return err;
        }
    }
    if (state->frame_allocation != NULL) {
        vPortFree(state->frame_allocation);
        state->frame_allocation = NULL;
    }
    if (state->owned_bus != NULL) {
        uint32_t err = IIC_BoardRelease(state->owned_bus);
        if (err != FSP_SUCCESS) {
            return err;
        }
        state->owned_bus = NULL;
    }
    return FSP_SUCCESS;
}

uint32_t Camera_BoardDeinitialize(void)
{
    return Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_DVP);
}

/* OV5640 FreeRTOS preview task and HDMI pipeline. */
/* 640x480 RGB565 DVP frames; CEU DMA requires an uncached SDRAM buffer. */
#define CAMERA_PREVIEW_FRAME_ALIGN         64U
#define CAMERA_PREVIEW_FRAME_BYTES         (640U * 480U * 2U)
#define CAMERA_PREVIEW_FRAME_TIMEOUT_MS    3000U
#define CAMERA_PREVIEW_LCD_WAIT_MS         1000U
#define CAMERA_PREVIEW_LOG_INTERVAL_MS     5000U
#define CAMERA_PREVIEW_STACK_WORDS         4096U
#define CAMERA_PREVIEW_TASK_PRIORITY       (tskIDLE_PRIORITY + 3U)

#define SDRAM_BASE_ADDR                    0x68000000UL
#define SDRAM_END_ADDR                     0x6A000000UL

static void *s_capture_allocation;
static uint8_t *s_capture_a;
static uint8_t *s_capture_b;
static LCD_DeviceType *s_preview_lcd;
static bool s_preview_resources_owned;
static bool s_preview_xclk_open;

static volatile bool s_preview_active;
static volatile bool s_preview_stop_requested;
static volatile uint32_t s_preview_last_error;
static CameraModelEnum s_preview_model = CAMERA_MODEL_OV5640;

typedef enum {
    CAMERA_SNAPSHOT_IDLE,
    CAMERA_SNAPSHOT_PENDING,
    CAMERA_SNAPSHOT_COPYING,
    CAMERA_SNAPSHOT_DONE,
} CameraSnapshotState;

static struct {
    uint8_t *destination;
    CameraSnapshotState state;
    uint32_t result;
} s_preview_snapshot;

/* GPT10 generates OV5640 DVP XCLK on P109 (12 MHz, GPTCLK=240 MHz).
 * The preview task or dual-camera entry owns it, never both. */
static gpt_instance_ctrl_t s_xclk_ctrl;
static const gpt_extended_cfg_t s_xclk_extend = {
    .gtioca = { .output_enabled = true, .stop_level = GPT_PIN_LEVEL_LOW },
    .gtiocb = { .output_enabled = false, .stop_level = GPT_PIN_LEVEL_LOW },
    .capture_a_irq = FSP_INVALID_VECTOR,
    .capture_b_irq = FSP_INVALID_VECTOR,
    .compare_match_c_irq = FSP_INVALID_VECTOR,
    .compare_match_d_irq = FSP_INVALID_VECTOR,
    .compare_match_e_irq = FSP_INVALID_VECTOR,
    .compare_match_f_irq = FSP_INVALID_VECTOR,
};
static const timer_cfg_t s_xclk_cfg = {
    .mode = TIMER_MODE_PERIODIC,
    .period_counts = 20,
    .source_div = TIMER_SOURCE_DIV_1,
    .duty_cycle_counts = 10,
    .channel = 10,
    .cycle_end_irq = FSP_INVALID_VECTOR,
    .p_extend = &s_xclk_extend,
};
static const timer_instance_t s_xclk = {
    .p_ctrl = &s_xclk_ctrl,
    .p_cfg = &s_xclk_cfg,
    .p_api = &g_timer_on_gpt,
};

static bool cameraPreviewHasResources(void)
{
    return s_capture_allocation != NULL || s_preview_lcd != NULL ||
        s_preview_xclk_open || s_dvp_camera.initialized ||
        s_dvp_camera.owned_bus != NULL || s_dvp_camera.frame_allocation != NULL;
}

/* Called only by the owning task during shutdown, or after it has exited.
 * Never release DMA buffers while CEU could still be writing them. Retain
 * failed GLCDC/I2C/XCLK cleanup state so a later stop() can retry. */
static uint32_t cameraPreviewCleanupResources(void)
{
    uint32_t err = FSP_SUCCESS;
    if (s_dvp_camera.initialized || s_dvp_camera.owned_bus != NULL) {
        uint32_t cleanup = Camera_BoardDeinitialize();
        if (cleanup != FSP_SUCCESS) {
            err = cleanup;
        }
    }
    if (s_preview_lcd != NULL) {
        uint32_t cleanup = LCD_BoardDeinitialize(s_preview_lcd);
        if (cleanup == FSP_SUCCESS) {
            s_preview_lcd = NULL;
        } else if (err == FSP_SUCCESS) {
            err = cleanup;
        }
    }
    /* XCLK must stay available if CEU close failed. */
    if (s_preview_xclk_open && !s_dvp_camera.initialized) {
        uint32_t cleanup = s_xclk.p_api->stop(s_xclk.p_ctrl);
        if (cleanup == FSP_SUCCESS) {
            cleanup = s_xclk.p_api->close(s_xclk.p_ctrl);
        }
        if (cleanup == FSP_SUCCESS) {
            s_preview_xclk_open = false;
        } else if (err == FSP_SUCCESS) {
            err = cleanup;
        }
    }
    if (!s_dvp_camera.initialized && s_capture_allocation != NULL) {
        s_capture_a = NULL;
        s_capture_b = NULL;
        s_dvp_camera.camera.image_ready = NULL;
        vPortFree(s_capture_allocation);
        s_capture_allocation = NULL;
    }
    return err;
}

static uint32_t previewElapsedMs(TickType_t since)
{
    return (uint32_t)(xTaskGetTickCount() - since) * portTICK_PERIOD_MS;
}

/* Called only by Camera_Task while a completed CEU frame is still stable.
 * The reader owns destination until DONE. Do not hold interrupts disabled
 * across the full VGA memcpy. */
static void cameraPreviewSnapshotCopy(const uint8_t *complete)
{
    taskENTER_CRITICAL();
    bool requested = s_preview_snapshot.state == CAMERA_SNAPSHOT_PENDING;
    if (requested) {
        s_preview_snapshot.state = CAMERA_SNAPSHOT_COPYING;
    }
    taskEXIT_CRITICAL();
    if (!requested) {
        return;
    }

    memcpy(s_preview_snapshot.destination, complete, CAMERA_PREVIEW_FRAME_BYTES);
    taskENTER_CRITICAL();
    s_preview_snapshot.result = FSP_SUCCESS;
    s_preview_snapshot.state = CAMERA_SNAPSHOT_DONE;
    taskEXIT_CRITICAL();
}

/* A timed-out pending request can be cancelled immediately. Once copying
 * begins, the caller must wait for DONE before freeing its destination. */
uint32_t CameraPreview_CopyFrame(uint8_t *buffer, size_t size, uint32_t timeout_ms)
{
    if (buffer == NULL || size < CAMERA_PREVIEW_FRAME_BYTES || timeout_ms == 0U) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    taskENTER_CRITICAL();
    if (!s_preview_active || s_preview_stop_requested) {
        taskEXIT_CRITICAL();
        return FSP_ERR_NOT_OPEN;
    }
    if (s_preview_snapshot.state != CAMERA_SNAPSHOT_IDLE) {
        taskEXIT_CRITICAL();
        return FSP_ERR_IN_USE;
    }
    s_preview_snapshot.destination = buffer;
    s_preview_snapshot.result = FSP_SUCCESS;
    s_preview_snapshot.state = CAMERA_SNAPSHOT_PENDING;
    taskEXIT_CRITICAL();

    TickType_t begin = xTaskGetTickCount();
    for (;;) {
        bool expired = previewElapsedMs(begin) >= timeout_ms;
        taskENTER_CRITICAL();
        CameraSnapshotState state = s_preview_snapshot.state;
        if (state == CAMERA_SNAPSHOT_DONE) {
            uint32_t result = s_preview_snapshot.result;
            s_preview_snapshot.destination = NULL;
            s_preview_snapshot.state = CAMERA_SNAPSHOT_IDLE;
            taskEXIT_CRITICAL();
            return result;
        }
        if (expired && state == CAMERA_SNAPSHOT_PENDING) {
            s_preview_snapshot.destination = NULL;
            s_preview_snapshot.state = CAMERA_SNAPSHOT_IDLE;
            taskEXIT_CRITICAL();
            return FSP_ERR_TIMEOUT;
        }
        taskEXIT_CRITICAL();
        /* If already COPYING, wait for the memcpy to finish even after the
         * deadline; otherwise the worker could write into freed SDRAM. */
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

static uint32_t previewWaitForFrame(Camera *camera)
{
    TickType_t since = xTaskGetTickCount();
    while (!s_preview_stop_requested &&
           previewElapsedMs(since) < CAMERA_PREVIEW_FRAME_TIMEOUT_MS) {
        uint32_t ready = CameraIsCaptureDone(camera);
        if (ready != 0U) {
            return ready;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return FSP_ERR_TIMEOUT;
}

static uint32_t previewWaitForLcd(LCD_DeviceType *lcd)
{
    TickType_t since = xTaskGetTickCount();
    while (!s_preview_stop_requested &&
           previewElapsedMs(since) < CAMERA_PREVIEW_LCD_WAIT_MS) {
        bool idle = false;
        uint32_t err = LCD_IsIdle(lcd, &idle);
        if (err != FSP_SUCCESS) {
            return err;
        }
        if (idle) {
            return FSP_SUCCESS;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return FSP_ERR_TIMEOUT;
}

/* GLCDC can reject a bufferChange near its line-detection/update boundary.
 * LCD_Present maps that timing-specific busy status to FSP_ERR_IN_USE.
 * Retry on a later tick without modifying the source bitmap or dirty rect. */
static uint32_t previewPresentWithRetry(LCD_DeviceType *lcd)
{
    TickType_t since = xTaskGetTickCount();
    uint32_t err = FSP_SUCCESS;
    do {
        err = LCD_Present(lcd);
        if (err != FSP_ERR_IN_USE) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    } while (!s_preview_stop_requested &&
             previewElapsedMs(since) < CAMERA_PREVIEW_LCD_WAIT_MS);

    return FSP_ERR_TIMEOUT;
}

static fsp_err_t cameraStartXclk(void)
{
    fsp_err_t err = s_xclk.p_api->open(s_xclk.p_ctrl, s_xclk.p_cfg);
    if (err != FSP_SUCCESS) {
        return err;
    }

    timer_info_t info;
    err = s_xclk.p_api->infoGet(s_xclk.p_ctrl, &info);
    if (err == FSP_SUCCESS &&
        (info.period_counts == 0U ||
         (uint64_t)info.period_counts * OV5640_DVP_XCLK_HZ != info.clock_frequency)) {
        err = FSP_ERR_INVALID_ARGUMENT;
    }
    if (err == FSP_SUCCESS) {
        err = R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_09,
                              IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_GPT1);
    }
    if (err == FSP_SUCCESS) {
        err = s_xclk.p_api->start(s_xclk.p_ctrl);
    }
    if (err != FSP_SUCCESS) {
        (void)s_xclk.p_api->close(s_xclk.p_ctrl);
        return err;
    }
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
    return FSP_SUCCESS;
}

bool CameraDual_IsActive(void)
{
    return s_dual_owned;
}

bool CameraDual_IsRunning(void)
{
    return s_dual_running;
}

uint32_t CameraDual_CopyFrame(CameraInterfaceEnum interface, uint8_t *buffer,
    size_t size, uint32_t timeout_ms)
{
    CameraBoardState *state = cameraBoardState(interface);
    if (state == NULL || buffer == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (!s_dual_running || !state->initialized) {
        return FSP_ERR_NOT_OPEN;
    }
    Camera *camera = &state->camera;
    size_t bytes = (size_t)camera->length * camera->width * 2U;
    if (size < bytes) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    /* Do not consume the ready flag while DMA is still rotating buffers. */
    TickType_t begin = xTaskGetTickCount();
    uint32_t count = 0;
    uint32_t err = CameraGetCaptureCnt(camera, &count);
    if (err != FSP_SUCCESS) {
        return err;
    }
    uint32_t initial_count = count;
    do {
        err = CameraGetCaptureCnt(camera, &count);
        if (err != FSP_SUCCESS) {
            return err;
        }
        if (count != initial_count) {
            break;
        }
        if (previewElapsedMs(begin) >= timeout_ms) {
            return FSP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    } while (true);

    err = CameraStopCapture(camera);
    if (err != FSP_SUCCESS) {
        s_dual_running = false;
        return err;
    }
    /* No Python allocation or exception can occur while this path is stopped. */
    uint32_t ready = CameraIsCaptureDone(camera);
    if (ready == 1U && camera->image_ready != NULL) {
        memcpy(buffer, camera->image_ready, bytes);
        err = FSP_SUCCESS;
    } else {
        err = FSP_ERR_TIMEOUT;
    }
    uint32_t restart = CameraStartCapture(camera, NULL);
    if (restart != FSP_SUCCESS) {
        s_dual_running = false;
        return restart;
    }
    return err;
}

uint32_t CameraDual_Stop(void)
{
    s_dual_running = false;
    if (!s_dual_owned) {
        return FSP_SUCCESS;
    }
    uint32_t err = Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_MIPI_CSI);
    uint32_t dvp_err = Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_DVP);
    if (err == FSP_SUCCESS) {
        err = dvp_err;
    }
    if (!s_dvp_camera.initialized && s_dual_xclk_open) {
        uint32_t clock_err = s_xclk.p_api->stop(s_xclk.p_ctrl);
        if (clock_err == FSP_SUCCESS) {
            clock_err = s_xclk.p_api->close(s_xclk.p_ctrl);
        }
        if (clock_err == FSP_SUCCESS) {
            s_dual_xclk_open = false;
        }
        if (err == FSP_SUCCESS) {
            err = clock_err;
        }
    }
    if (err == FSP_SUCCESS) {
        taskENTER_CRITICAL();
        s_dual_owned = false;
        taskEXIT_CRITICAL();
    }
    return err;
}

uint32_t CameraDual_Start(Camera **dvp, Camera **mipi)
{
    if (dvp == NULL || mipi == NULL || dvp == mipi) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *dvp = NULL;
    *mipi = NULL;
#if CAMERA_EN_DVP && CAMERA_EN_MIPI
    /* Lifecycle/control calls are serialized by the caller. Reserve against
     * the asynchronous preview task before touching its CEU or XCLK. */
    taskENTER_CRITICAL();
    bool busy = s_dual_owned || s_preview_active || cameraPreviewHasResources() ||
        s_dvp_camera.initialized || s_mipi_camera.initialized ||
        s_dvp_camera.owned_bus != NULL || s_mipi_camera.owned_bus != NULL ||
        s_dvp_camera.frame_allocation != NULL || s_mipi_camera.frame_allocation != NULL;
    if (!busy) {
        s_dual_owned = true;
    }
    taskEXIT_CRITICAL();
    if (busy) {
        return FSP_ERR_IN_USE;
    }

    Camera *dvp_camera = NULL;
    Camera *mipi_camera = NULL;
    uint32_t err = OV5640_HoldReset(CAMERA_INTERFACE_MIPI_CSI);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    err = OV5640_HoldReset(CAMERA_INTERFACE_DVP);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    err = cameraStartXclk();
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    s_dual_xclk_open = true;
    err = Camera_BoardInitialize(CAMERA_MODEL_OV5640, &dvp_camera);
    if (err != FSP_SUCCESS) {
        goto fail;
    }

    /* 0x3100 stores the 8-bit SCCB ID (7-bit address shifted left).
     * MIPI stays in reset until DVP acknowledges its new address. */
    err = IIC_WriteReg(dvp_camera->i2c, OV5640_IIC_ADDR, 0x3100, 2U,
                      CAMERA_DUAL_DVP_ADDRESS << 1, 1U);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    dvp_camera->i2c_address = CAMERA_DUAL_DVP_ADDRESS;
    uint8_t address_readback = 0;
    err = IIC_ReadRegSCCB(dvp_camera->i2c, CAMERA_DUAL_DVP_ADDRESS,
                          0x3100, 2U, &address_readback);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    if (address_readback != (CAMERA_DUAL_DVP_ADDRESS << 1)) {
        err = FSP_ERR_INVALID_DATA;
        goto fail;
    }

    const size_t bytes = CAMERA_DUAL_DVP_FRAME_BYTES * CAMERA_DUAL_DVP_FRAME_COUNT;
    void *allocation = pvPortMalloc(bytes + CAMERA_DUAL_DVP_FRAME_ALIGN - 1U);
    if (allocation == NULL) {
        err = FSP_ERR_OUT_OF_MEMORY;
        goto fail;
    }
    uintptr_t aligned = ((uintptr_t)allocation + CAMERA_DUAL_DVP_FRAME_ALIGN - 1U)
        & ~((uintptr_t)CAMERA_DUAL_DVP_FRAME_ALIGN - 1U);
    if (aligned < SDRAM_BASE_ADDR || aligned > SDRAM_END_ADDR - bytes) {
        vPortFree(allocation);
        err = FSP_ERR_INVALID_ADDRESS;
        goto fail;
    }
    s_dvp_camera.frame_allocation = allocation;
    for (uint32_t i = 0; i < CAMERA_DUAL_DVP_FRAME_COUNT; i++) {
        dvp_camera->rotation_buffer.buffer[i] =
            (uint8_t *)aligned + i * CAMERA_DUAL_DVP_FRAME_BYTES;
    }
    dvp_camera->rotation_buffer.length = CAMERA_DUAL_DVP_FRAME_COUNT;
    dvp_camera->rotation_buffer.single_frame = false;

    err = Camera_BoardInitializeMIPI(&mipi_camera);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    err = CameraStartCapture(dvp_camera, NULL);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    err = CameraStartCapture(mipi_camera, NULL);
    if (err != FSP_SUCCESS) {
        goto fail;
    }
    s_dual_running = true;
    *dvp = dvp_camera;
    *mipi = mipi_camera;
    return FSP_SUCCESS;

fail:
    {
        uint32_t cleanup = CameraDual_Stop();
        /* Retain resources after a failed stop; call CameraDual_Stop to retry. */
        return cleanup == FSP_SUCCESS ? err : cleanup;
    }
#else
    return FSP_ERR_UNSUPPORTED;
#endif
}

static uint32_t previewRun(Camera *camera, LCD_DeviceType *lcd)
{
    if (camera->interface != CAMERA_INTERFACE_DVP ||
        (camera->model != CAMERA_MODEL_OV5640 &&
         camera->model != CAMERA_MODEL_OV7725) ||
        camera->length > lcd->x_length || camera->width > lcd->y_length ||
        lcd->color_format != LCD_COLOR_FORMAT_RGB565 ||
        (uint32_t)camera->length * camera->width * 2U > CAMERA_PREVIEW_FRAME_BYTES) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    uint16_t x = (uint16_t)((lcd->x_length - camera->length) / 2U);
    uint16_t y = (uint16_t)((lcd->y_length - camera->width) / 2U);
    uint8_t *const buffers[2] = {s_capture_a, s_capture_b};
    uint8_t current = 0U;
    uint32_t err = FSP_SUCCESS;
    uint32_t frames = 0U;
    uint32_t ceu_start_count = 0U;
    uint32_t ceu_end_count = 0U;
    uint32_t copy_sum_ms = 0U;
    uint32_t copy_max_ms = 0U;
    bool old_single_frame = camera->rotation_buffer.single_frame;

    camera->rotation_buffer.single_frame = true;
    (void)CameraGetCaptureCnt(camera, &ceu_start_count);
    err = CameraStartCapture(camera, buffers[current]);
    if (err != FSP_SUCCESS) {
        camera->rotation_buffer.single_frame = old_single_frame;
        return err;
    }
    TickType_t begin = xTaskGetTickCount();
    TickType_t last_report = begin;
    printf("CAMERA_TASK: HDMI A/B preview running, buffers=%p/%p\r\n",
           (void *)buffers[0], (void *)buffers[1]);

    while (!s_preview_stop_requested) {
        uint32_t ready = previewWaitForFrame(camera);
        if (s_preview_stop_requested) {
            err = FSP_SUCCESS;
            break;
        }
        if (ready != 1U) {
            err = ready == 0U ? FSP_ERR_TIMEOUT : ready;
            printf("CAMERA_TASK: frame wait error=%lu\r\n", (unsigned long)err);
            break;
        }

        const uint8_t *complete = camera->image_ready;
        if (complete != buffers[current]) {
            err = FSP_ERR_INVALID_DATA;
            printf("CAMERA_TASK: unexpected frame buffer %p, expected %p\r\n",
                   (const void *)complete, (void *)buffers[current]);
            break;
        }

        /* A snapshot copies this completed frame before CEU rearms the next
         * buffer. Both OV5640 and OV7725 use the same RGB565 frame layout. */
        cameraPreviewSnapshotCopy(complete);

        /* ISR finished writing A: start writing B while CPU reads A.
         * This task never gives a buffer back to CEU before bitmap copy
         * completes, preventing DMA from overwriting the in-use image. */
        uint8_t next = current ^ 1U;
        err = camera->model == CAMERA_MODEL_OV5640
            ? OV5640_DVP_CaptureNext(buffers[next])
            : OV7725_DVP_CaptureNext(buffers[next]);
        if (err != FSP_SUCCESS) {
            printf("CAMERA_TASK: CEU rearm error=%lu\r\n", (unsigned long)err);
            break;
        }
        err = previewWaitForLcd(lcd);
        if (s_preview_stop_requested) {
            err = FSP_SUCCESS;
            break;
        }
        if (err != FSP_SUCCESS) {
            printf("CAMERA_TASK: LCD idle error=%lu\r\n", (unsigned long)err);
            break;
        }

        TickType_t copy_start = xTaskGetTickCount();
        err = LCD_DrawBitmap(lcd, x, y, camera->length, camera->width, complete);
        uint32_t copy_ms = previewElapsedMs(copy_start);
        if (err != FSP_SUCCESS) {
            printf("CAMERA_TASK: bitmap error=%lu\r\n", (unsigned long)err);
            break;
        }
        err = previewPresentWithRetry(lcd);
        if (s_preview_stop_requested) {
            err = FSP_SUCCESS;
            break;
        }
        if (err != FSP_SUCCESS) {
            printf("CAMERA_TASK: present error=%lu\r\n", (unsigned long)err);
            break;
        }
        copy_sum_ms += copy_ms;
        if (copy_ms > copy_max_ms) {
            copy_max_ms = copy_ms;
        }
        frames++;
        current = next;

        if (previewElapsedMs(last_report) >= CAMERA_PREVIEW_LOG_INTERVAL_MS) {
            uint32_t elapsed = previewElapsedMs(begin);
            float fps = elapsed ? 1000.0f * frames / elapsed : 0.0f;
            printf("CAMERA_TASK: frames=%lu elapsed=%lu ms FPS=%.2f copy avg=%.2f max=%lu ms\r\n",
                   (unsigned long)frames, (unsigned long)elapsed, (double)fps,
                   (double)copy_sum_ms / frames, (unsigned long)copy_max_ms);
            last_report = xTaskGetTickCount();
        }
    }

    (void)CameraGetCaptureCnt(camera, &ceu_end_count);
    uint32_t elapsed = previewElapsedMs(begin);
    float fps = elapsed ? 1000.0f * frames / elapsed : 0.0f;
    printf("CAMERA_TASK: stop frames=%lu elapsed=%lu ms FPS=%.2f CEU ends=%lu\r\n",
           (unsigned long)frames, (unsigned long)elapsed, (double)fps,
           (unsigned long)(ceu_end_count - ceu_start_count));
    /* The task owns all capture operations. Deinitialize closes CEU even
     * if the last single-frame callback already cleared its running flag. */
    camera->rotation_buffer.single_frame = old_single_frame;
    return err;
}

static void cameraPreviewTask(void *arg)
{
    (void)arg;
    Camera *camera = NULL;
    LCD_DeviceType *lcd = NULL;
    uint32_t err = FSP_SUCCESS;

    printf("CAMERA_TASK: startup\r\n");
    /* Residual resources must be cleaned by stop(), not reused by this task. */
    if (cameraPreviewHasResources()) {
        err = FSP_ERR_IN_USE;
        goto exit_task;
    }
    s_preview_resources_owned = true;
    void *allocation = pvPortMalloc(2U * CAMERA_PREVIEW_FRAME_BYTES +
                                   CAMERA_PREVIEW_FRAME_ALIGN - 1U);
    if (allocation == NULL) {
        err = FSP_ERR_OUT_OF_MEMORY;
        goto exit_task;
    }
    uintptr_t aligned = ((uintptr_t)allocation + CAMERA_PREVIEW_FRAME_ALIGN - 1U)
        & ~((uintptr_t)CAMERA_PREVIEW_FRAME_ALIGN - 1U);
    if (aligned < SDRAM_BASE_ADDR ||
        aligned > SDRAM_END_ADDR - 2U * CAMERA_PREVIEW_FRAME_BYTES) {
        vPortFree(allocation);
        err = FSP_ERR_INVALID_ADDRESS;
        goto exit_task;
    }
    /* The FreeRTOS SDRAM heap is uncached. Each VGA RGB565 frame is a
     * multiple of 64 bytes, so B is also aligned. */
    s_capture_allocation = allocation;
    s_capture_a = (uint8_t *)aligned;
    s_capture_b = s_capture_a + CAMERA_PREVIEW_FRAME_BYTES;

    err = cameraStartXclk();
    if (err != FSP_SUCCESS) {
        printf("CAMERA_TASK: XCLK error=%lu\r\n", (unsigned long)err);
        goto exit_task;
    }
    s_preview_xclk_open = true;

    err = Camera_BoardInitialize(s_preview_model, &camera);
    if (err != FSP_SUCCESS || camera == NULL) {
        printf("CAMERA_TASK: sensor init error=%lu\r\n", (unsigned long)err);
        goto exit_task;
    }

    lcd = LCD_BoardInitialize(LCD_STARTUP_TFP410);
    if (lcd == NULL) {
        printf("CAMERA_TASK: HDMI init failed\r\n");
        err = FSP_ERR_NOT_OPEN;
        goto exit_task;
    }
    s_preview_lcd = lcd;
    err = previewWaitForLcd(lcd);
    if (err != FSP_SUCCESS) {
        printf("CAMERA_TASK: HDMI initial idle error=%lu\r\n", (unsigned long)err);
        goto exit_task;
    }
    err = LCD_Fill(lcd, LCD_COLOR_RGB565_BLACK);
    if (err != FSP_SUCCESS) {
        printf("CAMERA_TASK: HDMI fill error=%lu\r\n", (unsigned long)err);
        goto exit_task;
    }
    if (err == FSP_SUCCESS) {
        err = previewPresentWithRetry(lcd);
    }
    if (err != FSP_SUCCESS) {
        printf("CAMERA_TASK: HDMI first present error=%lu\r\n", (unsigned long)err);
        goto exit_task;
    }
    if (!s_preview_stop_requested) {
        err = previewRun(camera, lcd);
        if (err != FSP_SUCCESS) {
            printf("CAMERA_TASK: preview ended, error=%lu\r\n", (unsigned long)err);
        }
    }

exit_task:
    /* Wake a pending snapshot even when sensor/HDMI initialization fails or
     * the preview is stopped before delivering a complete frame. */
    taskENTER_CRITICAL();
    if (s_preview_snapshot.state == CAMERA_SNAPSHOT_PENDING) {
        s_preview_snapshot.result = err == FSP_SUCCESS ? FSP_ERR_NOT_OPEN : err;
        s_preview_snapshot.state = CAMERA_SNAPSHOT_DONE;
    }
    taskEXIT_CRITICAL();
    if (s_preview_resources_owned) {
        uint32_t cleanup = cameraPreviewCleanupResources();
        if (cleanup != FSP_SUCCESS) {
            printf("CAMERA_TASK: cleanup error=%lu, resources retained\r\n",
                   (unsigned long)cleanup);
            if (err == FSP_SUCCESS) {
                err = cleanup;
            }
        }
        if (!cameraPreviewHasResources()) {
            s_preview_resources_owned = false;
        }
    }
    printf("CAMERA_TASK: exit, stack free=%lu words\r\n",
           (unsigned long)uxTaskGetStackHighWaterMark(NULL));

    taskENTER_CRITICAL();
    s_preview_last_error = err;
    s_preview_active = false;
    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

bool CameraPreview_StartModel(CameraModelEnum model)
{
    if (model != CAMERA_MODEL_OV5640 && model != CAMERA_MODEL_OV7725) {
        s_preview_last_error = FSP_ERR_INVALID_ARGUMENT;
        return false;
    }
    taskENTER_CRITICAL();
    if (s_preview_active || s_dual_owned || cameraPreviewHasResources() ||
        s_mipi_camera.initialized || s_mipi_camera.owned_bus != NULL ||
        s_mipi_camera.frame_allocation != NULL ||
        s_preview_snapshot.state != CAMERA_SNAPSHOT_IDLE) {
        s_preview_last_error = FSP_ERR_IN_USE;
        taskEXIT_CRITICAL();
        return false;
    }
    s_preview_active = true;
    s_preview_stop_requested = false;
    s_preview_last_error = FSP_SUCCESS;
    s_preview_model = model;
    taskEXIT_CRITICAL();

    /* No FSP-generated thread instance is needed. The REPL task has priority
     * 2; this worker uses priority 3 and yields while waiting for CEU/LCD. */
    BaseType_t created = xTaskCreate(cameraPreviewTask, "Camera_Task",
                                    CAMERA_PREVIEW_STACK_WORDS, NULL,
                                    CAMERA_PREVIEW_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        taskENTER_CRITICAL();
        s_preview_last_error = FSP_ERR_OUT_OF_MEMORY;
        s_preview_active = false;
        taskEXIT_CRITICAL();
        return false;
    }
    return true;
}

bool CameraPreview_Start(void)
{
    return CameraPreview_StartModel(CAMERA_MODEL_OV5640);
}

uint32_t CameraPreview_Cleanup(void)
{
    if (s_preview_active || s_dual_owned) {
        return FSP_ERR_IN_USE;
    }
    /* Never release resources acquired directly by other camera API users. */
    if (!s_preview_resources_owned) {
        return FSP_SUCCESS;
    }
    uint32_t err = cameraPreviewCleanupResources();
    if (!cameraPreviewHasResources()) {
        s_preview_resources_owned = false;
    }
    s_preview_last_error = err;
    return err;
}

void CameraPreview_RequestStop(void)
{
    s_preview_stop_requested = true;
}

bool CameraPreview_IsRunning(void)
{
    return s_preview_active;
}

CameraModelEnum CameraPreview_GetModel(void)
{
    return s_preview_model;
}

uint32_t CameraPreview_GetLastError(void)
{
    return s_preview_last_error;
}
