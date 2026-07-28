#include "hal_data.h"
#include "sd.h"

#if BSP_CFG_RTOS == 2
#include "FreeRTOS.h"
#include "event_groups.h"
#endif

#define SD_INSTANCE		g_rm_block_media0
#define SD_CALLBACK		RM_BLOCK_MEDIA_Callback

#define TAG	__FUNCTION__

#ifndef __SD_DEBUG
#define __SD_DEBUG	1
#endif

#if __SD_DEBUG

#include "utils/log.h"
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#define SD_LOGD(msg, ...)				LOG_D(TAG, msg, ##__VA_ARGS__)
#define SD_LOGW(msg, ...)				LOG_W(TAG, msg, ##__VA_ARGS__)
#define SD_LOGE(msg, ...)				LOG_E(TAG, msg, ##__VA_ARGS__)

#else

#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { return v; }
#define SD_LOGD(msg, ...)

#endif

union SD_Status {
	struct {
		uint32_t inserted : 1;
		uint32_t trans_done : 1;
		uint32_t : 30;
	} b;
	uint32_t val;
};

static volatile union SD_Status s_sd_status;

#if BSP_CFG_RTOS == 2
static EventGroupHandle_t s_sd_event = NULL;
#endif

uint32_t SD_Deinit(void)
{
	uint32_t err;

	err = RM_BLOCK_MEDIA_SDMMC_Close(SD_INSTANCE.p_ctrl);

	return 0;
}

uint32_t SD_Init(void)
{
	uint32_t err;

	err = RM_BLOCK_MEDIA_SDMMC_Open(SD_INSTANCE.p_ctrl, SD_INSTANCE.p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %lu", err);

	return 0;
}

uint32_t SD_InitMedia(void)
{
	uint32_t err;
	rm_block_media_status_t status;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	if (status.media_inserted != true) {
		while (s_sd_status.b.inserted == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	#if __SD_DEBUG
		LOG_D(TAG, "Detect SD Card insert");
	#endif
	}
	R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
	err = RM_BLOCK_MEDIA_SDMMC_MediaInit(SD_INSTANCE.p_ctrl);
	UNLIKE_RETURN(err, 0, "MediaInit failed: %lu", err);
	s_sd_status.b.trans_done = 1;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return 0;
}

uint32_t SD_IsInsert(void)
{
	return s_sd_status.b.inserted;
}

uint32_t SD_IsTransDone(void)
{
	return s_sd_status.b.trans_done;
}

uint32_t SD_Read(uint8_t *data, uint32_t block_addr, uint32_t size)
{
	uint32_t i;
	uint32_t err;
	uint32_t num_blocks;
	uint32_t repeat;

	uint8_t *p_read = data;

	num_blocks = size / 512;
	repeat = num_blocks / 0x10000;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	for (i = 0; i < repeat; i++) {
		err = RM_BLOCK_MEDIA_SDMMC_Read(SD_INSTANCE.p_ctrl, p_read, block_addr, 0x10000);
		UNLIKE_RETURN(err, 0, "Read failed: %lu", err);
		s_sd_status.b.trans_done = 0;
		while (s_sd_status.b.trans_done == 0) {
			R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		}
		block_addr += 0x10000;
		num_blocks -= 0x10000;
		p_read = &p_read[512 * 0x10000];
	}

	err = RM_BLOCK_MEDIA_SDMMC_Read(SD_INSTANCE.p_ctrl, p_read, block_addr, num_blocks);
	UNLIKE_RETURN(err, 0, "Read failed: %lu", err);
	s_sd_status.b.trans_done = 0;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return 0;
}

uint32_t SD_WaitTrans(void)
{
	rm_block_media_status_t status;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	while (status.busy == true) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
		RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	}

	while (s_sd_status.b.trans_done == 0) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
	}

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return 0;
}

uint32_t SD_Write(uint8_t const *src, uint32_t block_addr, uint32_t size)
{
	uint32_t i;
	uint32_t err;
	uint32_t num_blocks;
	uint32_t repeat;

	uint8_t const *p8 = src;

	if (s_sd_status.b.trans_done == 0) {
		return FSP_ERR_IN_USE;
	}

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	num_blocks = size / 512;
	repeat = num_blocks / 0x10000;

	for (i = 0; i < repeat; i++) {
		err = RM_BLOCK_MEDIA_SDMMC_Write(SD_INSTANCE.p_ctrl, p8, block_addr, 0x10000);
		UNLIKE_RETURN(err, 0, "Write failed: %lu", err);
		s_sd_status.b.trans_done = 0;
		while (s_sd_status.b.trans_done == 0) {
			R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		}
		block_addr += 0x10000;
		num_blocks -= 0x10000;
		p8 = &p8[512 * 0x10000];
	}

	err = RM_BLOCK_MEDIA_SDMMC_Write(SD_INSTANCE.p_ctrl, p8, block_addr, num_blocks);
	UNLIKE_RETURN(err, 0, "Write failed: %lu", err);
	s_sd_status.b.trans_done = 0;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return 0;
}

void SD_CALLBACK(rm_block_media_callback_args_t *p_args)
{
	switch (p_args->event) {
	case RM_BLOCK_MEDIA_EVENT_MEDIA_REMOVED:
		SD_LOGD("Remove");
		s_sd_status.b.inserted = 0;
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_INSERTED:
		SD_LOGD("Inserted");
		s_sd_status.b.inserted = 1;
		break;
	case RM_BLOCK_MEDIA_EVENT_OPERATION_COMPLETE:
		s_sd_status.b.trans_done = 1;
		break;
	case RM_BLOCK_MEDIA_EVENT_ERROR:
		break;
	case RM_BLOCK_MEDIA_EVENT_POLL_STATUS:
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_SUSPEND:
		SD_LOGD("Suspend");
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_RESUME:
		SD_LOGD("Resume");
		break;
	case RM_BLOCK_MEDIA_EVENT_WAIT:
		break;
	case RM_BLOCK_MEDIA_EVENT_WAIT_END:
		break;
	default:
		break;
	}
}
