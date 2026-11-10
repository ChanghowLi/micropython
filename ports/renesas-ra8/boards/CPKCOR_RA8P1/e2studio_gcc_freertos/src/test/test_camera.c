#include "test.h"

#if TEST_EN_CAMERA

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <stdio.h>
#include "camera_board.h"
#include "lcd_board.h"

#include "r_gpt.h"

#include "hal_data.h"
#include "camera/camera.h"
#include "camera/ov5640.h"
#include "lcd/lcd.h"
#include <string.h>
#include "perf_counter/perf_counter.h"
#include "utils/log.h"

/*================================== MACROS =======================================================*/

#ifndef TEST_CAMERA_EN_PERFORMANCE
#define TEST_CAMERA_EN_PERFORMANCE			0
#endif

#ifndef TEST_CAMERA_EN_PRESURE_REFINED
#define TEST_CAMERA_EN_PRESURE_REFINED		0
#endif

#ifndef TEST_CAMERA_EN_SINGLE_CAPTURE
#define TEST_CAMERA_EN_SINGLE_CAPTURE		1
#endif

/* Show the verified single DVP frame on the previously tested HDMI display. */
#ifndef TEST_CAMERA_EN_HDMI_PREVIEW
#define TEST_CAMERA_EN_HDMI_PREVIEW          1
#endif

/* C-layer HDMI video preview. With CEU A/B enabled below, acquisition into
 * the next SDRAM buffer overlaps copying the completed frame to GLCDC. */
#ifndef TEST_CAMERA_EN_VIDEO_PREVIEW
#define TEST_CAMERA_EN_VIDEO_PREVIEW         1
#endif

#ifndef TEST_CAMERA_VIDEO_DURATION_MS
/* 0: keep preview running (REPL will not start until reset).
 * Set to 60000U for a bounded 60-second regression test. */
#define TEST_CAMERA_VIDEO_DURATION_MS        0U
#endif

/* 1: HDMI preview uses CEU A/B; 0: preserve original serial preview. */
#ifndef TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
#define TEST_CAMERA_EN_DVP_DOUBLE_BUFFER     1
#endif
/* The old CEU-only A/B test is retained as an optional regression test. */
#ifndef TEST_CAMERA_EN_CEU_AB_STANDALONE
#define TEST_CAMERA_EN_CEU_AB_STANDALONE     0
#endif
#ifndef TEST_CAMERA_DOUBLE_DURATION_MS
#define TEST_CAMERA_DOUBLE_DURATION_MS      10000U
#endif

#ifndef TEST_CAMERA_DVP_CACHE_HSIZE
#define TEST_CAMERA_DVP_CACHE_HSIZE			640
#endif

#ifndef TEST_CAMERA_DVP_CACHE_VSIZE
#define TEST_CAMERA_DVP_CACHE_VSIZE			480
#endif

/**
 * 1: DVP Cache in SRAM
 * 2: DVP Cache in SDRAM
 * 3: DVP Cache in HyperRAM(Not available in CPKHMI) */
#ifndef TEST_CAMERA_DVP_CACHE_POS
#define TEST_CAMERA_DVP_CACHE_POS			2
#endif

#if TEST_CAMERA_EN_PERFORMANCE
#ifndef PERFORMANCE_TIME_S
#define PERFORMANCE_TIME_S					20
#endif
#endif /* #if TEST_CAMERA_EN_PERFORMANCE */

#if TEST_CAMERA_EN_PRESURE_REFINED
#ifndef CAMERA_PRESURE_REFINED_SIZE
#define CAMERA_PRESURE_REFINED_SIZE			1024
#endif
#endif

#define DVP_CACHE_IN_SRAM					1
#define DVP_CACHE_IN_SDRAM					2
#define DVP_CACHE_IN_HYPER_RAM				3

#define TEST_CAMERA_DVP_CACHE_SIZE			(TEST_CAMERA_DVP_CACHE_HSIZE * TEST_CAMERA_DVP_CACHE_VSIZE * 2)
#define UNLIKE_RETURN(v, t, msg, ...)   	if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }

#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SRAM
#undef TEST_CAMERA_EN_PRESURE_REFINED
#define TEST_CAMERA_EN_PRESURE_REFINED		0
#endif

#ifndef TEST_CAMERA_SINGLE_TIMEOUT_MS
#define TEST_CAMERA_SINGLE_TIMEOUT_MS 3000U
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

#if TEST_CAMERA_EN_PRESURE_REFINED
static float s_speed_sum_r[CAMERA_PRESURE_REFINED_SIZE];
static float s_speed_sum_w[CAMERA_PRESURE_REFINED_SIZE];
#endif

#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SRAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8)));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SDRAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".sdram_noinit_nocache")));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_HYPER_RAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".ospi1_cs0_noinit_nocache")));
#endif

#if TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SRAM
static uint8_t s_dvp_image_b[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8)));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SDRAM
static uint8_t s_dvp_image_b[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".sdram_noinit_nocache")));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_HYPER_RAM
static uint8_t s_dvp_image_b[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".ospi1_cs0_noinit_nocache")));
#endif
#if TEST_CAMERA_EN_CEU_AB_STANDALONE
static uint32_t testCameraDvpDoubleBuffer(Camera *camera);
#endif
#endif

/*================================== Private Functions Prototypes =================================*/

#if TEST_CAMERA_EN_PERFORMANCE
static bool checkDVPHasError(CameraDVPErrorCountType *err_cnt);
static bool checkMIPIHasError(CameraMIPIErrrorCountType *err_cnt);
static uint32_t testCameraPerformance(Camera *camera);
static uint32_t testCameraPerformance2(Camera *camera1, const CameraConfig *cfg1, Camera *camera2, const CameraConfig *cfg2);
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
static uint32_t testCameraPresureRefined(Camera *camera);
static uint32_t testCameraPresureRefined2(Camera *camera1, Camera *camera2, uint8_t which);
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
uint32_t testCameraSingleCapture(Camera *camera);
#endif

#if TEST_CAMERA_EN_VIDEO_PREVIEW && TEST_CAMERA_EN_HDMI_PREVIEW && TEST_CAMERA_EN_SINGLE_CAPTURE
#if TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
static uint32_t testCameraPipelinedVideoPreview(Camera *camera, LCD_DeviceType *lcd);
#else
static uint32_t testCameraVideoPreview(Camera *camera, LCD_DeviceType *lcd);
#endif
#endif

/*================================== Public Functions =============================================*/

/* GPTCLK = PLL2R / 2 = 240 MHz; 20 counts give 12 MHz XCLK.
 * P109/GTIOC10A feeds J300 pin 13. This instance is owned by the test. */
static gpt_instance_ctrl_t s_camera_xclk_ctrl;
static const gpt_extended_cfg_t s_camera_xclk_extend = {
    .gtioca = { .output_enabled = true, .stop_level = GPT_PIN_LEVEL_LOW },
    .gtiocb = { .output_enabled = false, .stop_level = GPT_PIN_LEVEL_LOW },
    .capture_a_irq = FSP_INVALID_VECTOR,
    .capture_b_irq = FSP_INVALID_VECTOR,
    .compare_match_c_irq = FSP_INVALID_VECTOR,
    .compare_match_d_irq = FSP_INVALID_VECTOR,
    .compare_match_e_irq = FSP_INVALID_VECTOR,
    .compare_match_f_irq = FSP_INVALID_VECTOR,
};
static const timer_cfg_t s_camera_xclk_cfg = {
    .mode = TIMER_MODE_PERIODIC,
    .period_counts = 20,
    .source_div = TIMER_SOURCE_DIV_1,
    .duty_cycle_counts = 10,
    .channel = 10,
    .cycle_end_irq = FSP_INVALID_VECTOR,
    .p_extend = &s_camera_xclk_extend,
};
static const timer_instance_t s_camera_xclk = {
    .p_ctrl = &s_camera_xclk_ctrl,
    .p_cfg = &s_camera_xclk_cfg,
    .p_api = &g_timer_on_gpt,
};

void TestCameraStartup(void)
{
    printf("CAMERA: OV5640 DVP startup test\r\n");
    fsp_err_t err = s_camera_xclk.p_api->open(s_camera_xclk.p_ctrl, s_camera_xclk.p_cfg);
    printf("CAMERA: XCLK open=%lu\r\n", (unsigned long)err);
    if (err != FSP_SUCCESS) {
        return;
    }
    timer_info_t info;
    err = s_camera_xclk.p_api->infoGet(s_camera_xclk.p_ctrl, &info);
    if (err == FSP_SUCCESS && (info.period_counts == 0 ||
        (uint64_t)info.period_counts * OV5640_DVP_XCLK_HZ != info.clock_frequency)) {
        printf("CAMERA: configure GPT10 output to exactly 12000 kHz\r\n");
        err = FSP_ERR_INVALID_ARGUMENT;
    }
    if (err == FSP_SUCCESS) {
        err = R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_09,
            IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_GPT1);
    }
    if (err == FSP_SUCCESS) {
        err = s_camera_xclk.p_api->start(s_camera_xclk.p_ctrl);
    }
    printf("CAMERA: XCLK start=%lu\r\n", (unsigned long)err);
    if (err == FSP_SUCCESS) {
        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
        Camera *camera = NULL;
        uint32_t result = Camera_BoardInitialize(CAMERA_MODEL_OV5640, &camera);
        printf("CAMERA: init=%lu\r\n", (unsigned long)result);
        if (result == FSP_SUCCESS) {
            result = TestCamera(camera);
            printf("CAMERA: capture=%lu (%s)\r\n", (unsigned long)result,
                result == FSP_SUCCESS ? "PASS" : "FAIL");
#if TEST_CAMERA_EN_HDMI_PREVIEW && TEST_CAMERA_EN_SINGLE_CAPTURE
            if (result == FSP_SUCCESS && camera->image_ready != NULL) {
                LCD_DeviceType *lcd = LCD_BoardInitialize(LCD_STARTUP_TFP410);
                if (lcd == NULL) {
                    printf("CAMERA: HDMI init failed\r\n");
                } else if (lcd->color_format != LCD_COLOR_FORMAT_RGB565 ||
                           camera->length > lcd->x_length || camera->width > lcd->y_length) {
                    printf("CAMERA: HDMI frame format or size unsupported\r\n");
                } else {
                    uint16_t x = (uint16_t)((lcd->x_length - camera->length) / 2U);
                    uint16_t y = (uint16_t)((lcd->y_length - camera->width) / 2U);
                    uint32_t display_err = LCD_Fill(lcd, LCD_COLOR_RGB565_BLACK);
                    if (display_err == FSP_SUCCESS) {
                        display_err = LCD_DrawBitmap(lcd, x, y, camera->length,
                                                     camera->width, camera->image_ready);
                    }
                    if (display_err == FSP_SUCCESS) {
                        display_err = LCD_Present(lcd);
                    }
                    printf("CAMERA: HDMI preview=%lu (%s)\r\n", (unsigned long)display_err,
                           display_err == FSP_SUCCESS ? "PASS" : "FAIL");
                    /* Keep the GLCDC and framebuffer active so the image remains on screen. */
#if TEST_CAMERA_EN_VIDEO_PREVIEW
                    if (display_err == FSP_SUCCESS) {
#if TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
                        if (TEST_CAMERA_VIDEO_DURATION_MS == 0U) {
                            printf("CAMERA: HDMI A/B preview starting (continuous; reset to stop)\r\n");
                        } else {
                            printf("CAMERA: HDMI A/B preview starting (%lu ms)\r\n",
                                   (unsigned long)TEST_CAMERA_VIDEO_DURATION_MS);
                        }
                        uint32_t video_err = testCameraPipelinedVideoPreview(camera, lcd);
#else
                        printf("CAMERA: legacy HDMI preview starting (%lu ms; 0=continuous)\r\n",
                               (unsigned long)TEST_CAMERA_VIDEO_DURATION_MS);
                        uint32_t video_err = testCameraVideoPreview(camera, lcd);
#endif
                        printf("CAMERA: video preview=%lu (%s)\r\n", (unsigned long)video_err,
                               video_err == FSP_SUCCESS ? "PASS" : "FAIL");
                    }
#endif
                }
            }
#endif
        }
#if TEST_CAMERA_EN_DVP_DOUBLE_BUFFER && TEST_CAMERA_EN_CEU_AB_STANDALONE
        if (result == FSP_SUCCESS) {
            printf("CAMERA: CEU double buffer test starting (%lu ms)\r\n",
                   (unsigned long)TEST_CAMERA_DOUBLE_DURATION_MS);
            uint32_t double_err = testCameraDvpDoubleBuffer(camera);
            printf("CAMERA: CEU double buffer=%lu (%s)\r\n",
                   (unsigned long)double_err,
                   double_err == FSP_SUCCESS ? "PASS" : "FAIL");
        }
#endif
        uint32_t cleanup = Camera_BoardDeinitialize();
        printf("CAMERA: cleanup=%lu\r\n", (unsigned long)cleanup);
        if (cleanup != FSP_SUCCESS) {
            return;
        }
    }
    err = s_camera_xclk.p_api->close(s_camera_xclk.p_ctrl);
    printf("CAMERA: XCLK close=%lu\r\n", (unsigned long)err);
}

uint32_t TestCamera(void *camera)
{
	(void)camera;

#if TEST_CAMERA_EN_PERFORMANCE
	uint32_t err = testCameraPerformance(camera);
	if (err != 0) { return err; }
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
	uint32_t pressure_err = testCameraPresureRefined(camera);
	if (pressure_err != 0) { return pressure_err; }
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
	return testCameraSingleCapture(camera);
#endif

	return 0;
}

uint32_t TestCamera2(void *camera1, void *camera_cfg1, void *camera2, void *camera_cfg2)
{
	(void)camera1;
	(void)camera2;

#if TEST_CAMERA_EN_PERFORMANCE
	testCameraPerformance2(camera1, camera_cfg1, camera2, camera_cfg2);
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
	testCameraPresureRefined2(camera1, camera2, 1);
#endif

	return 0;
}

/*================================== Private Functions ============================================*/

#if TEST_CAMERA_EN_PERFORMANCE

static bool checkDVPHasError(CameraDVPErrorCountType *err_cnt)
{
	bool has_error = false;

	if (err_cnt->firewall) {
		LOG_W(__FUNCTION__, "firewall: %" PRIu32, err_cnt->firewall);
		has_error = true;
	}
	if (err_cnt->hd_mismatch) {
		LOG_W(__FUNCTION__, "hd_mismatch: %" PRIu32, err_cnt->hd_mismatch);
		has_error = true;
	}
	if (err_cnt->hd_missing) {
		LOG_W(__FUNCTION__, "hd_missing: %" PRIu32, err_cnt->hd_missing);
		// has_error = true;
	}
	if (err_cnt->overflow) {
		LOG_W(__FUNCTION__, "overflow: %" PRIu32, err_cnt->overflow);
		has_error = true;
	}
	if (err_cnt->vd_error) {
		LOG_W(__FUNCTION__, "vd_error: %" PRIu32, err_cnt->vd_error);
		has_error = true;
	}
	if (err_cnt->vd_mismatch) {
		LOG_W(__FUNCTION__, "vd_mismatch: %" PRIu32, err_cnt->vd_mismatch);
		has_error = true;
	}
	if (err_cnt->vd_missing) {
		LOG_W(__FUNCTION__, "vd_missing: %" PRIu32, err_cnt->vd_missing);
		has_error = true;
	}

	return has_error;
}

static bool checkMIPIHasError(CameraMIPIErrrorCountType *err_cnt)
{
	bool has_error = false;

	if (err_cnt->axi_err) {
		LOG_W(__FUNCTION__, "axi_error: %" PRIu32, err_cnt->axi_err);
		has_error = true;
	}
	if (err_cnt->fifo_overflow) {
		LOG_W(__FUNCTION__, "fifo_overflow: %" PRIu32, err_cnt->fifo_overflow);
		has_error = true;
	}
	if (err_cnt->preclip_h_err) {
		LOG_W(__FUNCTION__, "preclip_h_err: %" PRIu32, err_cnt->preclip_h_err);
		has_error = true;
	}
	if (err_cnt->preclip_v_err) {
		LOG_W(__FUNCTION__, "preclip_v_err: %" PRIu32, err_cnt->preclip_v_err);
		has_error = true;
	}
	if (err_cnt->resp_overflow) {
		LOG_W(__FUNCTION__, "resp_overflow: %" PRIu32, err_cnt->resp_overflow);
		has_error = true;
	}

	return has_error;
}

static uint32_t testCameraPerformance(Camera *camera)
{
	bool is_capture_start;
	uint32_t i;
	uint32_t ret;
	uint32_t cnt1, cnt2;
	int64_t time_start, time_end;
	float time_s, fps;
	CameraDVPErrorCountType dvp_error_cnt;
	CameraMIPIErrrorCountType mipi_error_cnt;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PERFORMANCE");

	ret = CameraIsCaptureStart(camera, &is_capture_start);
	UNLIKE_RETURN(ret, 0, "IsCaptureStart failed: %" PRIu32, ret);
	if (is_capture_start) {
		LOG_I(__FUNCTION__, "CEU already start, try to close");
		ret = CameraStopCapture(camera);
		UNLIKE_RETURN(ret, 0, "StopCapture failed: %" PRIu32, ret);
		memset(s_dvp_image, 0, sizeof(s_dvp_image));
	}

	if (camera->model == CAMERA_MODEL_OV5640) {
		// OV5640_SetScale(false);
		// OV5640_SetSubsample(1, 1);
		// OV5640_SetWindowSize(640, 480, true);
		// ret = CameraSetOutputSize(camera, camera->length, camera->width);
		// UNLIKE_RETURN(ret, 0, "SetOutputSize failed: %" PRIu32, ret);
	}
	ret = CameraSetFPS(camera, 55, NULL, false);
	UNLIKE_RETURN(ret, 0, "SetFPS failed: %" PRIu32, ret);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
	ret = CameraStartCapture(camera, s_dvp_image);
	UNLIKE_RETURN(ret, 0, "StartCapture failed: %" PRIu32, ret);
	R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
	ret = CameraGetCalFPS(camera, &fps);
	if (ret == 0) {
		printf("Theory FPS: %.3f\r\n", fps);
	}

	ret = CameraGetCaptureCnt(camera, &cnt1);
	UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
		ret = CameraGetCaptureCnt(camera, &cnt1);
		UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);
		if (camera->interface == CAMERA_INTERFACE_DVP) {
			CameraGetDVPErrorCnt(camera, &dvp_error_cnt);
			if (checkDVPHasError(&dvp_error_cnt)) {
				LOG_E(__FUNCTION__, "Exit because DVP error");
				return 1;
			}
		}
		else {
			CameraGetMIPIErrorCnt(camera, &mipi_error_cnt);
			if (checkMIPIHasError(&mipi_error_cnt)) {
				LOG_E(__FUNCTION__, "Exit because MIPI error");
				return 1;
			}
		}
	}

	time_start = get_system_ms();
	for (i = 0; i < PERFORMANCE_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	ret = CameraGetCaptureCnt(camera, &cnt2);
	UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)(cnt2 - cnt1) / time_s;
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total get frame    : %" PRIu32 "\r\n", cnt2 - cnt1);
	printf("Calculate FPS      : %.3f\r\n", fps);
	puts("======== Error Count ========");
	if (camera->interface == CAMERA_INTERFACE_DVP) {
		CameraGetDVPErrorCnt(camera, &dvp_error_cnt);
		printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt.firewall);
		printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.hd_mismatch);
		printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt.hd_missing);
		printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt.overflow);
		printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt.vd_error);
		printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.vd_mismatch);
		printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt.vd_missing);
	}
	else {
		CameraGetMIPIErrorCnt(camera, &mipi_error_cnt);
		printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt.fifo_overflow);
		printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_h_err);
		printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_v_err);
		printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt.resp_overflow);
		printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt.axi_err);
	}

	return 0;
}

static uint32_t testCameraPerformance2(Camera *camera1, const CameraConfig *cfg1, Camera *camera2, const CameraConfig *cfg2)
{
	uint32_t i, err;
	uint32_t cnt1_start, cnt1_end;
	uint32_t cnt2_start, cnt2_end;
	int64_t time_start, time_end;
	CameraDVPErrorCountType dvp_error_cnt;
	CameraMIPIErrrorCountType mipi_error_cnt;
	float fps1, fps2, time_s;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PERFORMANCE");

	if ((cfg1->interface != CAMERA_INTERFACE_DVP) && (cfg2->interface != CAMERA_INTERFACE_MIPI_CSI)) {
		LOG_E(__FUNCTION__, "camera1 must using DVP and camera2 must using MIPI");
		return 1;
	}

#if 0
	bool capture_start;

	/* 确保两个相机都停止捕获 */
	err = CameraIsCaptureStart(camera1, &capture_start);
	UNLIKE_RETURN(err, 0, "Camera1 IsCaptureStart failed: %" PRIu32, err);
	if (capture_start) {
		LOG_I(__FUNCTION__, "Camera1 already start, try to close");
		err = CameraStopCapture(camera1);
		UNLIKE_RETURN(err, 0, "Camera1 StopCapture failed: %" PRIu32, err);
		if (camera1->interface == CAMERA_INTERFACE_DVP) {
			memset(s_dvp_image, 0, sizeof(s_dvp_image));
		}
	}
	err = CameraIsCaptureStart(camera2, &capture_start);
	UNLIKE_RETURN(err, 0, "Camera2 IsCaptureStart failed: %" PRIu32, err);
	if (capture_start) {
		LOG_I(__FUNCTION__, "Camera2 already start, try to close");
		err = CameraStopCapture(camera2);
		UNLIKE_RETURN(err, 0, "Camera2 StopCapture failed: %" PRIu32, err);
		if (camera2->interface == CAMERA_INTERFACE_DVP) {
			memset(s_dvp_image, 0, sizeof(s_dvp_image));
		}
	}
#endif

	LOG_D(__FUNCTION__, "camera1 resolution: %" PRIu32 "x%" PRIu32, cfg1->length, cfg1->width);
	LOG_D(__FUNCTION__, "camera2 resolution: %" PRIu32 "x%" PRIu32, cfg2->length, cfg2->width);

	/* 两个相机都重新指定帧率，由于 MIPI 接口的 OV5640 Power down 功能暂时异常，因此这里使用掉电重配置 */
	CameraPowerOff(camera1, true);
	CameraPowerOff(camera2, true);
	LOG_D(__FUNCTION__, "camera1 re-init");
	err = CameraInit(camera1, cfg1);
	UNLIKE_RETURN(err, 0, "camera1 init failed: %" PRIu32, err);
	err = CameraSetFPS(camera1, 60, NULL, false);
	UNLIKE_RETURN(err, 0, "camera1 set FPS failed: %" PRIu32, err);
	CameraPowerDown(camera1, 1);
	LOG_D(__FUNCTION__, "camera1 re-init done, and enter power down");

	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	LOG_D(__FUNCTION__, "camera2 re-init");
	err = CameraInit(camera2, cfg2);
	UNLIKE_RETURN(err, 0, "camera2 init failed: %" PRIu32, err);
	err = CameraSetFPS(camera2, 60, NULL, false);
	UNLIKE_RETURN(err, 0, "camera2 set FPS failed: %" PRIu32, err);
	R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
	LOG_D(__FUNCTION__, "camera2 re-init done, and ready to capture");

	CameraPowerDown(camera1, 0);
	err = CameraStartCapture(camera1, s_dvp_image);
	UNLIKE_RETURN(err, 0, "camera1 start capture failed: %" PRIu32, err);
	err = CameraStartCapture(camera2, NULL);
	UNLIKE_RETURN(err, 0, "camera2 start capture failed: %" PRIu32, err);

	CameraGetCaptureCnt(camera1, &cnt1_start);
	CameraGetCaptureCnt(camera2, &cnt2_start);
	time_start = get_system_ms();
	for (i = 0; i < PERFORMANCE_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();

	CameraGetCaptureCnt(camera1, &cnt1_end);
	CameraGetCaptureCnt(camera2, &cnt2_end);
	time_s = (float)(time_end - time_start) / 1000.0f;
	fps1 = (float)(cnt1_end - cnt1_start) / time_s;
	fps2 = (float)(cnt2_end - cnt2_start) / time_s;
	printf("Actual running time  : %.3f\r\n", time_s);
	printf("Camera1 get frame    : %" PRIu32 "\r\n", cnt1_end - cnt1_start);
	printf("Camera1 calculate FPS: %.3f\r\n", fps1);
	printf("Camera2 get frame    : %" PRIu32 "\r\n", cnt2_end - cnt2_start);
	printf("Camera2 calculate FPS: %.3f\r\n", fps2);

	if (camera1->interface == CAMERA_INTERFACE_DVP) {
		CameraGetDVPErrorCnt(camera1, &dvp_error_cnt);
		CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt);
	}
	else {
		CameraGetDVPErrorCnt(camera2, &dvp_error_cnt);
		CameraGetMIPIErrorCnt(camera1, &mipi_error_cnt);
	}
	puts("======== DVP Error Count ========");
	printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt.firewall);
	printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.hd_mismatch);
	printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt.hd_missing);
	printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt.overflow);
	printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt.vd_error);
	printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.vd_mismatch);
	printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt.vd_missing);
	puts("======== MIPI Error Count ========");
	printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt.fifo_overflow);
	printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_h_err);
	printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_v_err);
	printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt.resp_overflow);
	printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt.axi_err);

	return 0;
}
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
static uint32_t testCameraPresureRefined(Camera *camera)
{
	bool is_capture_start;
	uint32_t i, ret;
	uint32_t cnt1, cnt2;
	int64_t time_start, time_end;

	uint32_t cnt_w = 0;
	uint32_t cnt_r = 0;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PRESURE_REFINED");

	ret = CameraIsCaptureStart(camera, &is_capture_start);
	UNLIKE_RETURN(ret, 0, "IsCaptureStart failed: %" PRIu32, ret);
	if (is_capture_start == false) {
		LOG_I(__FUNCTION__, "CEU already start, try to start");
		ret = CameraStartCapture(camera, s_dvp_image);
		UNLIKE_RETURN(ret, 0, "StartCapture failed: %" PRIu32, ret);
	}

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	CameraGetCaptureCnt(camera, &cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(camera, &cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_w[cnt_w], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
		cnt_w++;
		if (cnt_w >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_end = get_system_ms();
	printf("============ Write speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_w);
	printf("SRAM ==> SDRAM, 16bit width\r\n");
	for (i = 0; i < cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
		if ((i % 10) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetCaptureCnt(camera, &cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_r[cnt_r], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
		cnt_r++;
		if (cnt_r >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_end = get_system_ms();
	printf("============ Read speed summury  ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_r);
	printf("SDRAM ==> SRAM, 16bit width\r\n");
	for (i = 0; i < cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
		if ((i % 5) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	return 0;
}

/**
 * @brief	测试在 CEU 和 VIN 都启动时的 SDRAM 读写速度
 * @note	务必确保进入此函数前相机已开启捕获，与单个相机的 testCameraPresureRefined() 不同，此函数不会主动启动捕获
 * @param	camera1 相机 1 实例
 * @param	camera2 相机 2 实例
 * @param	which	以哪个相机实例的帧为基准，1 为相机 1，其它为相机 2
 * @return	总是返回 0
 */
static uint32_t testCameraPresureRefined2(Camera *camera1, Camera *camera2, uint8_t which)
{
	uint32_t i;
	volatile uint32_t cnt1, cnt2;
	int64_t time_start, time_end;
	CameraDVPErrorCountType dvp_error_cnt_old;
	CameraDVPErrorCountType dvp_error_cnt_new;
	CameraMIPIErrrorCountType mipi_error_cnt_old;
	CameraMIPIErrrorCountType mipi_error_cnt_new;

	uint32_t cnt_w = 0;
	uint32_t cnt_r = 0;
	Camera *cam = NULL;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PRESURE_REFINED");

	if ((camera1->interface != CAMERA_INTERFACE_DVP) && (camera2->interface != CAMERA_INTERFACE_MIPI_CSI)) {
		LOG_E(__FUNCTION__, "camera1 must using DVP and camera2 must using MIPI");
		return FSP_ERR_ASSERTION;
	}

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	if (which == 1) {
		cam = camera1;
	}
	else {
		cam = camera2;
	}

	CameraGetDVPErrorCnt(camera1, &dvp_error_cnt_old);
	CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt_old);

	CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_w[cnt_w], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
		cnt_w++;
		if (cnt_w >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}
	time_end = get_system_ms();

	printf("============ Write speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_w);
	printf("SRAM ==> SDRAM, 16bit width\r\n");
	for (i = 0; i < cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
		if ((i % 10) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_r[cnt_r], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
		cnt_r++;
		if (cnt_r >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}
	time_end = get_system_ms();

	printf("============ Read speed summury  ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_r);
	printf("SDRAM ==> SRAM, 16bit width\r\n");
	for (i = 0; i < cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
		if ((i % 5) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetDVPErrorCnt(camera1, &dvp_error_cnt_new);
	CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt_new);
	printf("============ New CEU error ============\r\n");
	printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt_new.firewall - dvp_error_cnt_old.firewall);
	printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt_new.hd_mismatch - dvp_error_cnt_old.hd_mismatch);
	printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt_new.hd_missing - dvp_error_cnt_old.hd_missing);
	printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt_new.overflow - dvp_error_cnt_old.overflow);
	printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt_new.vd_error - dvp_error_cnt_old.vd_error);
	printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt_new.vd_mismatch - dvp_error_cnt_old.vd_mismatch);
	printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt_new.vd_missing - dvp_error_cnt_old.vd_missing);
	printf("============ New VIN error ============\r\n");
	printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt_new.fifo_overflow - mipi_error_cnt_old.fifo_overflow);
	printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt_new.preclip_h_err - mipi_error_cnt_old.preclip_h_err);
	printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt_new.preclip_v_err - mipi_error_cnt_old.preclip_v_err);
	printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt_new.resp_overflow - mipi_error_cnt_old.resp_overflow);
	printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt_new.axi_err - mipi_error_cnt_old.axi_err);

	return 0;
}
#endif

#if TEST_CAMERA_EN_DVP_DOUBLE_BUFFER && TEST_CAMERA_EN_CEU_AB_STANDALONE
static uint32_t dvpSampleHash(const uint8_t *image, uint32_t size)
{
    uint32_t hash = 2166136261U;
    for (uint32_t offset = 0U; offset < size; offset += 128U) {
        hash = (hash ^ image[offset]) * 16777619U;
    }
    return hash;
}

/* Two physical capture buffers, with one-frame CEU rearming in the owning
 * task. The ISR completes A; we start capture into B immediately and keep A
 * locked while simulating CPU image processing. Only once A is released can
 * the loop rearm it after B completes. No buffer can be overwritten while
 * the CPU is still reading it; no per-frame CEU Close/Open is required. */
static uint32_t testCameraDvpDoubleBuffer(Camera *camera)
{
    if (camera == NULL || camera->model != CAMERA_MODEL_OV5640 ||
        camera->interface != CAMERA_INTERFACE_DVP ||
        (uint32_t)camera->length * camera->width * 2U > sizeof(s_dvp_image_b)) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    uint8_t *buffers[2] = {s_dvp_image, s_dvp_image_b};
    uint32_t image_size = (uint32_t)camera->length * camera->width * 2U;
    uint32_t frames = 0U;
    uint32_t rearmed = 0U;
    uint32_t first_count = 0U;
    uint32_t last_count = 0U;
    uint32_t stable_checks = 0U;
    bool old_single_frame = camera->rotation_buffer.single_frame;
    uint8_t index = 0U;
    uint32_t err;

    memset(buffers[0], 0xA5, image_size);
    memset(buffers[1], 0xA5, image_size);
    camera->rotation_buffer.single_frame = true;
    (void) CameraGetCaptureCnt(camera, &first_count);
    int64_t start_ms = get_system_ms();
    err = CameraStartCapture(camera, buffers[0]);
    if (err != FSP_SUCCESS) {
        camera->rotation_buffer.single_frame = old_single_frame;
        return err;
    }

    while ((get_system_ms() - start_ms) < TEST_CAMERA_DOUBLE_DURATION_MS) {
        uint32_t ready;
        int64_t wait_start = get_system_ms();
        do {
            ready = CameraIsCaptureDone(camera);
            if (ready != 0U) {
                break;
            }
            vTaskDelay(1);
        } while ((get_system_ms() - wait_start) < TEST_CAMERA_SINGLE_TIMEOUT_MS);

        if (ready != 1U) {
            err = ready == 0U ? FSP_ERR_TIMEOUT : ready;
            break;
        }
        uint8_t *completed = camera->image_ready;
        if (completed != buffers[index]) {
            err = FSP_ERR_INVALID_DATA;
            printf("CAMERA: CEU A/B unexpected ready buffer %p, expected %p\r\n",
                   (void *)completed, (void *)buffers[index]);
            break;
        }

        /* CEU writes only the OTHER buffer while this frame is in use. */
        uint8_t next = index ^ 1U;
        err = OV5640_DVP_CaptureNext(buffers[next]);
        if (err != FSP_SUCCESS) {
            break;
        }
        rearmed++;

        uint32_t hash_before = dvpSampleHash(completed, image_size);
        /* Hold the completed frame during the next CEU acquisition.
         * This intentionally exercises buffer ownership rather than HDMI. */
        vTaskDelay(pdMS_TO_TICKS(50U));
        if (hash_before != dvpSampleHash(completed, image_size)) {
            err = FSP_ERR_INVALID_DATA;
            printf("CAMERA: CEU A/B frame overwritten while CPU held it\r\n");
            break;
        }
        stable_checks++;
        frames++;
        index = next;
    }

    uint32_t elapsed_ms = (uint32_t)(get_system_ms() - start_ms);
    (void) CameraGetCaptureCnt(camera, &last_count);
    uint32_t stop_err = CameraStopCapture(camera);
    camera->rotation_buffer.single_frame = old_single_frame;
    if (err == FSP_SUCCESS && stop_err != FSP_SUCCESS) {
        err = stop_err;
    }
    float fps = elapsed_ms ? (1000.0f * frames) / (float)elapsed_ms : 0.0f;
    printf("CAMERA: CEU A/B buffers=%p / %p, bytes=%lu each\r\n",
           (void *)buffers[0], (void *)buffers[1], (unsigned long)image_size);
    printf("CAMERA: CEU A/B frames=%lu, elapsed=%lu ms, FPS=%.2f, rearmed=%lu (CPU hold=50ms/frame)\r\n",
           (unsigned long)frames, (unsigned long)elapsed_ms, (double)fps,
           (unsigned long)rearmed);
    printf("CAMERA: CEU A/B frame ends=%lu, stable held frames=%lu, stop=%lu\r\n",
           (unsigned long)(last_count - first_count),
           (unsigned long)stable_checks, (unsigned long)stop_err);
    return err;
}
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE && TEST_CAMERA_EN_VIDEO_PREVIEW && \
    TEST_CAMERA_EN_HDMI_PREVIEW && TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
/* HDMI preview with CEU A/B pipelining. The CEU frame-end interrupt stops
 * the current one-shot capture; this task immediately starts the next shot
 * into the other SDRAM buffer, THEN copies the completed one to GLCDC.
 * The CEU cannot reuse a buffer until the CPU is finished copying from it.
 * This is a video-preview test, not a replacement for the camera API. */
static uint32_t testCameraPipelinedVideoPreview(Camera *camera, LCD_DeviceType *lcd)
{
    if (camera == NULL || lcd == NULL || camera->model != CAMERA_MODEL_OV5640 ||
        camera->interface != CAMERA_INTERFACE_DVP ||
        lcd->color_format != LCD_COLOR_FORMAT_RGB565 ||
        camera->length > lcd->x_length || camera->width > lcd->y_length ||
        (uint32_t)camera->length * camera->width * 2U > sizeof(s_dvp_image) ||
        (uint32_t)camera->length * camera->width * 2U > sizeof(s_dvp_image_b)) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    uint8_t *const buffers[2] = {s_dvp_image, s_dvp_image_b};
    uint16_t x = (uint16_t)((lcd->x_length - camera->length) / 2U);
    uint16_t y = (uint16_t)((lcd->y_length - camera->width) / 2U);
    uint8_t index = 0U;
    bool old_single_frame = camera->rotation_buffer.single_frame;
    uint32_t err = FSP_SUCCESS;
    uint32_t frames = 0U;
    uint32_t copy_sum_ms = 0U;
    uint32_t copy_max_ms = 0U;
    uint32_t idle_sum_ms = 0U;
    uint32_t ceu_count_start = 0U;
    uint32_t ceu_count_end = 0U;
    int64_t start_ms = get_system_ms();
    int64_t report_ms = start_ms;

    camera->rotation_buffer.single_frame = true;
    (void) CameraGetCaptureCnt(camera, &ceu_count_start);
    err = CameraStartCapture(camera, buffers[0]);
    if (err != FSP_SUCCESS) {
        camera->rotation_buffer.single_frame = old_single_frame;
        return err;
    }

    for (;;) {
        if (TEST_CAMERA_VIDEO_DURATION_MS != 0U &&
            (get_system_ms() - start_ms) >= TEST_CAMERA_VIDEO_DURATION_MS) {
            break;
        }

        /* Wait until the ISR reports a fully written frame. */
        int64_t wait_start = get_system_ms();
        uint32_t ready;
        do {
            ready = CameraIsCaptureDone(camera);
            if (ready != 0U) {
                break;
            }
            vTaskDelay(1);
        } while ((get_system_ms() - wait_start) < TEST_CAMERA_SINGLE_TIMEOUT_MS);
        if (ready != 1U) {
            err = ready == 0U ? FSP_ERR_TIMEOUT : ready;
            printf("CAMERA: HDMI A/B CEU wait failed: %lu\r\n", (unsigned long)err);
            break;
        }

        const uint8_t *completed = camera->image_ready;
        if (completed != buffers[index]) {
            err = FSP_ERR_INVALID_DATA;
            printf("CAMERA: HDMI A/B wrong completed buffer %p, expected %p\r\n",
                   (const void *)completed, (void *)buffers[index]);
            break;
        }

        /* Capture B can run while LCD_DrawBitmap copies completed A. */
        uint8_t next = index ^ 1U;
        err = OV5640_DVP_CaptureNext(buffers[next]);
        if (err != FSP_SUCCESS) {
            printf("CAMERA: HDMI A/B rearm failed: %lu\r\n", (unsigned long)err);
            break;
        }

        int64_t idle_start = get_system_ms();
        bool idle = false;
        do {
            err = LCD_IsIdle(lcd, &idle);
            if (err != FSP_SUCCESS || idle) {
                break;
            }
            vTaskDelay(1);
        } while ((get_system_ms() - idle_start) < 1000U);
        idle_sum_ms += (uint32_t)(get_system_ms() - idle_start);
        if (err != FSP_SUCCESS || !idle) {
            if (err == FSP_SUCCESS) {
                err = FSP_ERR_TIMEOUT;
            }
            printf("CAMERA: HDMI A/B LCD idle failed: %lu\r\n", (unsigned long)err);
            break;
        }

        int64_t copy_start = get_system_ms();
        err = LCD_DrawBitmap(lcd, x, y, camera->length, camera->width, completed);
        uint32_t copy_ms = (uint32_t)(get_system_ms() - copy_start);
        copy_sum_ms += copy_ms;
        if (copy_ms > copy_max_ms) {
            copy_max_ms = copy_ms;
        }
        if (err != FSP_SUCCESS) {
            printf("CAMERA: HDMI A/B bitmap failed: %lu\r\n", (unsigned long)err);
            break;
        }
        err = LCD_Present(lcd);
        if (err != FSP_SUCCESS) {
            printf("CAMERA: HDMI A/B present failed: %lu\r\n", (unsigned long)err);
            break;
        }
        /* The CEU buffer is no longer needed after synchronous bitmap copy.
         * Only now may it become a capture target on the next iteration. */
        frames++;
        index = next;

        int64_t now_ms = get_system_ms();
        if ((now_ms - report_ms) >= 5000U) {
            uint32_t elapsed = (uint32_t)(now_ms - start_ms);
            float fps = elapsed ? (1000.0f * frames) / (float)elapsed : 0.0f;
            printf("CAMERA: HDMI A/B live frames=%lu, elapsed=%lu ms, FPS=%.2f, LCD copy avg=%.2f max=%lu ms\r\n",
                   (unsigned long)frames, (unsigned long)elapsed, (double)fps,
                   (double)copy_sum_ms / frames, (unsigned long)copy_max_ms);
            report_ms = now_ms;
        }
    }

    /* Clean exit for a bounded run or any error. In unlimited mode a board
     * reset stops the preview; REPL does not start until this returns. */
    uint32_t elapsed_ms = (uint32_t)(get_system_ms() - start_ms);
    (void) CameraGetCaptureCnt(camera, &ceu_count_end);
    uint32_t stop_err = CameraStopCapture(camera);
    camera->rotation_buffer.single_frame = old_single_frame;
    if (err == FSP_SUCCESS && stop_err != FSP_SUCCESS) {
        err = stop_err;
    }
    float fps = elapsed_ms ? (1000.0f * frames) / (float)elapsed_ms : 0.0f;
    printf("CAMERA: HDMI A/B frames=%lu, elapsed=%lu ms, FPS=%.2f, CEU ends=%lu\r\n",
           (unsigned long)frames, (unsigned long)elapsed_ms, (double)fps,
           (unsigned long)(ceu_count_end - ceu_count_start));
    if (frames != 0U) {
        printf("CAMERA: HDMI A/B LCD copy avg=%.2f max=%lu ms, idle avg=%.2f ms, stop=%lu\r\n",
               (double)copy_sum_ms / frames, (unsigned long)copy_max_ms,
               (double)idle_sum_ms / frames, (unsigned long)stop_err);
    }
    return err;
}
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
#if TEST_CAMERA_EN_VIDEO_PREVIEW && TEST_CAMERA_EN_HDMI_PREVIEW && \
    !TEST_CAMERA_EN_DVP_DOUBLE_BUFFER
static uint32_t testCameraVideoPreview(Camera *camera, LCD_DeviceType *lcd)
{
    enum {
        VIDEO_STAGE_CEU_START,
        VIDEO_STAGE_FRAME_WAIT,
        VIDEO_STAGE_CEU_STOP,
        VIDEO_STAGE_LCD_WAIT,
        VIDEO_STAGE_LCD_COPY,
        VIDEO_STAGE_LCD_PRESENT,
        VIDEO_STAGE_COUNT
    };
    static const char *const stage_names[VIDEO_STAGE_COUNT] = {
        "CEU start/reopen",
        "CEU frame wait",
        "CEU stop",
        "LCD idle wait",
        "LCD bitmap copy",
        "LCD present"
    };

    if (camera == NULL || lcd == NULL || camera->interface != CAMERA_INTERFACE_DVP ||
        camera->length > lcd->x_length || camera->width > lcd->y_length ||
        lcd->color_format != LCD_COLOR_FORMAT_RGB565) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    uint16_t x = (uint16_t)((lcd->x_length - camera->length) / 2U);
    uint16_t y = (uint16_t)((lcd->y_length - camera->width) / 2U);
    uint32_t frames = 0U;
    uint32_t stage_sum_ms[VIDEO_STAGE_COUNT] = {0};
    uint32_t stage_max_ms[VIDEO_STAGE_COUNT] = {0};
    bool old_single_frame = camera->rotation_buffer.single_frame;
    int64_t start_ms = get_system_ms();
    uint32_t err = FSP_SUCCESS;

    while (TEST_CAMERA_VIDEO_DURATION_MS == 0U ||
           (get_system_ms() - start_ms) < TEST_CAMERA_VIDEO_DURATION_MS) {
        uint32_t stage_ms[VIDEO_STAGE_COUNT];
        int64_t phase_start = get_system_ms();
        /* The single-frame flag prevents automatic ISR buffer rotation. */
        camera->rotation_buffer.single_frame = true;
        err = CameraStartCapture(camera, s_dvp_image);
        stage_ms[VIDEO_STAGE_CEU_START] = (uint32_t)(get_system_ms() - phase_start);
        if (err != FSP_SUCCESS) {
            break;
        }

        int64_t frame_start = get_system_ms();
        uint32_t ready = 0U;
        do {
            ready = CameraIsCaptureDone(camera);
            if (ready != 0U) {
                break;
            }
            vTaskDelay(1);
        } while ((get_system_ms() - frame_start) < TEST_CAMERA_SINGLE_TIMEOUT_MS);
        stage_ms[VIDEO_STAGE_FRAME_WAIT] = (uint32_t)(get_system_ms() - frame_start);

        phase_start = get_system_ms();
        uint32_t stop_err = CameraStopCapture(camera);
        stage_ms[VIDEO_STAGE_CEU_STOP] = (uint32_t)(get_system_ms() - phase_start);
        if (ready != 1U) {
            err = (ready == 0U) ? FSP_ERR_TIMEOUT : ready;
            break;
        }
        if (stop_err != FSP_SUCCESS) {
            err = stop_err;
            break;
        }
        if (camera->image_ready == NULL) {
            err = FSP_ERR_INVALID_POINTER;
            break;
        }

        /* Wait for the previous GLCDC present before modifying its framebuffer. */
        int64_t display_start = get_system_ms();
        bool idle = false;
        do {
            err = LCD_IsIdle(lcd, &idle);
            if (err != FSP_SUCCESS || idle) {
                break;
            }
            vTaskDelay(1);
        } while ((get_system_ms() - display_start) < 1000U);
        stage_ms[VIDEO_STAGE_LCD_WAIT] = (uint32_t)(get_system_ms() - display_start);
        if (err != FSP_SUCCESS) {
            break;
        }
        if (!idle) {
            err = FSP_ERR_TIMEOUT;
            break;
        }

        phase_start = get_system_ms();
        err = LCD_DrawBitmap(lcd, x, y, camera->length, camera->width, camera->image_ready);
        stage_ms[VIDEO_STAGE_LCD_COPY] = (uint32_t)(get_system_ms() - phase_start);
        if (err != FSP_SUCCESS) {
            break;
        }
        phase_start = get_system_ms();
        err = LCD_Present(lcd);
        stage_ms[VIDEO_STAGE_LCD_PRESENT] = (uint32_t)(get_system_ms() - phase_start);
        if (err != FSP_SUCCESS) {
            break;
        }
        for (uint32_t i = 0U; i < VIDEO_STAGE_COUNT; i++) {
            stage_sum_ms[i] += stage_ms[i];
            if (stage_ms[i] > stage_max_ms[i]) {
                stage_max_ms[i] = stage_ms[i];
            }
        }
        frames++;
    }
    camera->rotation_buffer.single_frame = old_single_frame;
    uint32_t elapsed_ms = (uint32_t)(get_system_ms() - start_ms);
    float fps = elapsed_ms ? (float)frames * 1000.0f / (float)elapsed_ms : 0.0f;
    printf("CAMERA: video frames=%" PRIu32 ", elapsed=%" PRIu32 " ms, FPS=%.2f\r\n",
           frames, elapsed_ms, fps);
    if (frames != 0U) {
        printf("CAMERA: timing over %" PRIu32 " complete frames (ms, avg/max):\r\n", frames);
        for (uint32_t i = 0U; i < VIDEO_STAGE_COUNT; i++) {
            printf("CAMERA: %-17s avg=%7.2f, max=%" PRIu32 "\r\n",
                   stage_names[i], (double)stage_sum_ms[i] / (double)frames,
                   stage_max_ms[i]);
        }
    }
    return err;
}
#endif

uint32_t testCameraSingleCapture(Camera *camera)
{
    if (camera == NULL || camera->interface != CAMERA_INTERFACE_DVP) {
        return FSP_ERR_UNSUPPORTED;
    }
    if (camera->length == 0 || camera->width == 0 ||
        (uint32_t)camera->length * camera->width * 2U > sizeof(s_dvp_image)) {
        return FSP_ERR_INVALID_SIZE;
    }
    uint32_t image_size = (uint32_t)camera->length * camera->width * 2U;
    bool started;
    uint32_t err = CameraIsCaptureStart(camera, &started);
    if (err != FSP_SUCCESS) { return err; }
    if (started) { return FSP_ERR_IN_USE; }
    /* Distinguish a DMA-written frame from unchanged SDRAM test data. */
    memset(s_dvp_image, 0xA5, image_size);
    bool old_single_frame = camera->rotation_buffer.single_frame;
    camera->rotation_buffer.single_frame = true;
    err = CameraStartCapture(camera, s_dvp_image);
    if (err != FSP_SUCCESS) {
        camera->rotation_buffer.single_frame = old_single_frame;
        return err;
    }
    int64_t start = get_system_ms();
    do {
        uint32_t ready = CameraIsCaptureDone(camera);
        if (ready == 1U) {
            err = FSP_SUCCESS;
            break;
        }
        if (ready != 0U) {
            err = ready;
            break;
        }
        err = FSP_ERR_TIMEOUT;
        vTaskDelay(1);
    } while (get_system_ms() - start < TEST_CAMERA_SINGLE_TIMEOUT_MS);
    uint32_t stop_err = CameraStopCapture(camera);
    camera->rotation_buffer.single_frame = old_single_frame;
    if (err != FSP_SUCCESS) {
        LOG_E(__FUNCTION__, "Capture failed: %" PRIu32, err);
        return err;
    }
    if (stop_err != FSP_SUCCESS) { return stop_err; }
    LOG_I(__FUNCTION__, "Frame ready: %ux%u at %p", camera->length,
        camera->width, (void *)camera->image_ready);
    if (camera->image_ready == NULL) { return FSP_ERR_INVALID_POINTER; }

    /* Sample the frame without dumping 600 KiB through the serial console. */
    const uint8_t *image = camera->image_ready;
    uint32_t changed = 0U;
    uint32_t samples = 0U;
    uint8_t min_value = 0xFFU;
    uint8_t max_value = 0U;
    for (uint32_t i = 0U; i < image_size; i += 128U) {
        uint8_t value = image[i];
        if (value != 0xA5U) { changed++; }
        if (value < min_value) { min_value = value; }
        if (value > max_value) { max_value = value; }
        samples++;
    }
    printf("CAMERA: frame bytes=%" PRIu32 ", sampled=%" PRIu32 ", changed=%" PRIu32
           ", min=0x%02X, max=0x%02X\r\n", image_size, samples, changed,
           (unsigned)min_value, (unsigned)max_value);
    printf("CAMERA: first 8 bytes:");
    for (uint32_t i = 0U; i < 8U; i++) {
        printf(" %02X", (unsigned)image[i]);
    }
    printf("\r\n");
    if (changed == 0U) {
        printf("CAMERA: WARNING: sampled data unchanged; check CEU/SDRAM path\r\n");
    }
    return FSP_SUCCESS;
}

/* Caller supplies an initialized RGB565 display; no bus or pins are chosen here. */
uint32_t TestCameraDisplay(void *camera_device, void *lcd_device)
{
    Camera *camera = camera_device;
    LCD_DeviceType *lcd = lcd_device;
    if (camera == NULL || lcd == NULL || lcd->color_format != LCD_COLOR_FORMAT_RGB565 ||
        camera->length > lcd->x_length || camera->width > lcd->y_length) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    uint32_t err = testCameraSingleCapture(camera);
    if (err != FSP_SUCCESS) { return err; }
    err = LCD_DrawBitmap(lcd, 0, 0, camera->length, camera->width, camera->image_ready);
    if (err != FSP_SUCCESS) { return err; }
    return LCD_Present(lcd);
}
#endif

#endif /* #if TEST_EN_CAMERA */
