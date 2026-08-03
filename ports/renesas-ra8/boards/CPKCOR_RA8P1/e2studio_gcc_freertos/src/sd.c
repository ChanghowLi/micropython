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
#define SD_LOGW(msg, ...)
#define SD_LOGE(msg, ...)
#endif

union SD_Status {
	struct {
		uint32_t error : 1;
		uint32_t inserted : 1;
		uint32_t open : 1;
		uint32_t resume : 1;
		uint32_t suspend : 1;
		uint32_t trans_done : 1;
		uint32_t : 26;
	} b;
	uint32_t val;
};

static uint32_t s_sd_block_num;
static uint32_t s_sd_block_size;
static volatile union SD_Status s_sd_status;

#if BSP_CFG_RTOS == 2
#define SD_EVENT_REMOVED			(0x01 << 0)
#define SD_EVENT_INSERTED			(0x01 << 1)
#define SD_EVENT_TRANS_DONE			(0x01 << 2)
#define SD_EVENT_ERROR				(0x01 << 3)
#define SD_EVENT_POLL_STATUS		(0x01 << 4)
#define SD_EVENT_SUSPEND			(0x01 << 5)
#define SD_EVENT_RESUME				(0x01 << 6)
#define SD_EVENT_WAIT				(0x01 << 7)
#define SD_EVENT_WAIT_END			(0x01 << 8)
static EventGroupHandle_t s_sd_event = NULL;
#endif

uint32_t SD_Deinit(void)
{
	uint32_t err;

	err = RM_BLOCK_MEDIA_SDMMC_Close(SD_INSTANCE.p_ctrl);
	UNLIKE_RETURN(err, 0, "Close failed: %" PRIu32, err);
	s_sd_status.val = 0;
	s_sd_block_num = 0;
	s_sd_block_size = 0;

#if BSP_CFG_RTOS == 2
	if (s_sd_event != NULL) {
		vEventGroupDelete(s_sd_event);
	}
#endif

	return 0;
}

uint32_t SD_GetInfo(uint32_t *block_count, uint32_t *block_size)
{
	rm_block_media_info_t info;
	rm_block_media_status_t status;

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	if (status.initialized == false) {
		return FSP_ERR_NOT_OPEN;
	}
	RM_BLOCK_MEDIA_SDMMC_InfoGet(SD_INSTANCE.p_ctrl, &info);
	*block_count = info.num_sectors;
	*block_size = info.sector_size_bytes;

	return 0;
}

uint32_t SD_Init(void)
{
	uint32_t err;

	if (s_sd_status.b.open) {
		SD_LOGW("Already open");
		return FSP_ERR_ALREADY_OPEN;
	}

	err = RM_BLOCK_MEDIA_SDMMC_Open(SD_INSTANCE.p_ctrl, SD_INSTANCE.p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %lu", err);
	s_sd_status.val = 0;

#if BSP_CFG_RTOS == 2
	s_sd_event = xEventGroupCreate();
	if (s_sd_event == NULL) {
		SD_LOGE("Create event group failed");
		return FSP_ERR_ABORTED;
	}
#endif

	s_sd_status.b.open = 1;

	return 0;
}

uint32_t SD_InitMedia(void)
{
	uint32_t err;
	rm_block_media_status_t status;

	if (s_sd_status.b.open == 0) {
		SD_LOGW("Not open");
		return FSP_ERR_NOT_OPEN;
	}

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	if (status.media_inserted == false) {
		SD_LOGE("SD Card not inserted");
		return FSP_ERR_ABORTED;
	}

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
	err = RM_BLOCK_MEDIA_SDMMC_MediaInit(SD_INSTANCE.p_ctrl);
	if (err) {
		SD_LOGE("MediaInit failed: %" PRIu32, err);
	}
	else {
		s_sd_status.b.trans_done = 1;
		SD_GetInfo(&s_sd_block_num, &s_sd_block_size);
		SD_LOGD("Block Num: %" PRIu32 ", Block Size: %" PRIu32, s_sd_block_num, s_sd_block_size);
	}

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return err;
}

uint32_t SD_IsInsert(void)
{
	return s_sd_status.b.inserted;
}

/**
 * @brief		检查 SD 卡是否已插入，功能等同于 SD_IsInsert()。只是该函数读取 FSP 状态，而 SD_IsInsert() 读取自行维护的中断标志
 * @note		受硬件设计缺陷，必须重新上电卡插入检测功能才有效，且只能检测第一次卡插入，后续一旦卡有拔出，检测将一直失效，总会返回 true
 * @param[out]	present 已插入，值 true；否则为 false
 * @return		总是返回 0
 */
uint32_t SD_IsPresent(bool *present)
{
	rm_block_media_status_t status;

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	*present = status.media_inserted;

	return 0;
}

uint32_t SD_IsTransDone(void)
{
	return s_sd_status.b.trans_done;
}

uint32_t SD_ReadBlock(void *data, uint32_t first_block, uint32_t count, uint32_t timeout_ms)
{
	uint32_t i, err;
#if BSP_CFG_RTOS == 2
	EventBits_t eb;
#endif

	if (s_sd_status.b.inserted == 0) {
		SD_LOGE("SD Card not insert");
		return FSP_ERR_ABORTED;
	}
	if (s_sd_block_size == 0) {
		SD_LOGE("Block size if zero, some error occur");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((first_block + count) > s_sd_block_num) {
		SD_LOGE("Block out of memory");
		return FSP_ERR_INVALID_BLOCKS;
	}

	uint8_t *p_read = (uint8_t *)data;
	uint32_t repeat = count / 0x10000;
	uint32_t block_addr = first_block;

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	for (i = 0; i < repeat; i++) {
		err = SD_WaitTrans(timeout_ms);
		if (err) {
			SD_LOGE("Wait trans failed: %" PRIu32, err);
			goto READ_BLOCK_EXIT;
		}
		err = RM_BLOCK_MEDIA_SDMMC_Read(SD_INSTANCE.p_ctrl, p_read, block_addr, 0x10000);
		if (err) {
			SD_LOGE("Read failed: %" PRIu32, err);
			goto READ_BLOCK_EXIT;
		}
		s_sd_status.b.trans_done = 0;
	#if BSP_CFG_RTOS == 2
		xEventGroupClearBits(s_sd_event, SD_EVENT_TRANS_DONE);
		eb = xEventGroupWaitBits(s_sd_event, SD_EVENT_TRANS_DONE, pdTRUE, pdTRUE, timeout_ms);
		if ((eb & SD_EVENT_TRANS_DONE) == 0x00) {
			SD_LOGE("Read timeout");
			err = FSP_ERR_TIMEOUT;
			goto READ_BLOCK_EXIT;
		}
	#else
		uint32_t timeout_us = timeout_ms * 1000;
		while (s_sd_status.b.trans_done == 0) {
			if (timeout_us == 0) {
				SD_LOGE("Write timeout");
				err = FSP_ERR_TIMEOUT;
				goto READ_BLOCK_EXIT;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
			timeout_us--;
		}
	#endif
		block_addr += 0x10000;
		count -= 0x10000;
		p_read = &p_read[s_sd_block_size * 0x10000];
	}

	if (count) {
		err = SD_WaitTrans(timeout_ms);
		if (err) {
			SD_LOGE("Wait trans failed: %" PRIu32, err);
			goto READ_BLOCK_EXIT;
		}
		err = RM_BLOCK_MEDIA_SDMMC_Read(SD_INSTANCE.p_ctrl, p_read, block_addr, count);
		if (err) {
			SD_LOGE("Read failed: %" PRIu32, err);
			goto READ_BLOCK_EXIT;
		}
		s_sd_status.b.trans_done = 0;
	#if BSP_CFG_RTOS == 2
		xEventGroupClearBits(s_sd_event, SD_EVENT_TRANS_DONE);
		eb = xEventGroupWaitBits(s_sd_event, SD_EVENT_TRANS_DONE, pdTRUE, pdTRUE, timeout_ms);
		if ((eb & SD_EVENT_TRANS_DONE) == 0x00) {
			SD_LOGE("Read timeout");
			err = FSP_ERR_TIMEOUT;
			goto READ_BLOCK_EXIT;
		}
	#else
		while (s_sd_status.b.trans_done == 0) {
			R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MICROSECONDS);
		}
	#endif
	}

READ_BLOCK_EXIT:
#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif
	return err;
}

uint32_t SD_WaitTrans(uint32_t timeout_ms)
{
	rm_block_media_status_t status;

	uint32_t err = 0;
#if BSP_CFG_RTOS == 2
	EventBits_t event;
#else
	uint32_t timeout_us = timeout_ms * 1000;
#endif

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

#if BSP_CFG_RTOS == 2
	event = xEventGroupGetBits(s_sd_event);
	if (event & SD_EVENT_REMOVED) {
		SD_LOGW("SD Card removed");
		xEventGroupClearBits(s_sd_event, SD_EVENT_REMOVED);
		err = FSP_ERR_ASSERTION;
	}
	if (event & SD_EVENT_ERROR) {
		SD_LOGE("Error occur");
		xEventGroupClearBits(s_sd_event, SD_EVENT_ERROR);
		err = FSP_ERR_ASSERTION;
	}
	if (err) {
		goto WAIT_TRANS_EXIT;
	}

	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	if (status.busy == true) {
		event = xEventGroupWaitBits(s_sd_event, SD_EVENT_TRANS_DONE, pdTRUE, pdTRUE, timeout_ms);
		RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
		if ((status.busy == true) || ((event & SD_EVENT_TRANS_DONE) == 0)) {
			SD_LOGE("Timeout occur");
			err = FSP_ERR_TIMEOUT;
		}
	}
#else
	RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	while ((status.busy == true) || (s_sd_status.b.trans_done == 0)) {
		if (timeout_us == 0) {
			SD_LOGE("Timeout occur");
			err = FSP_ERR_TIMEOUT;
			break;
		}
		if (s_sd_status.b.error) {
			SD_LOGE("Error occur");
			err = FSP_ERR_ASSERTION;
			break;
		}
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
		timeout_us--;
		RM_BLOCK_MEDIA_SDMMC_StatusGet(SD_INSTANCE.p_ctrl, &status);
	}
#endif

#if BSP_CFG_RTOS == 2
WAIT_TRANS_EXIT:
#endif
#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif

	return err;
}

uint32_t SD_WriteBlock(void *data, uint32_t first_block, uint32_t count, uint32_t timeout_ms)
{
	uint32_t i, err;

	uint8_t *p_write = (uint8_t *)data;
	uint32_t repeat = count / 0x10000;
	uint32_t block_addr = first_block;

	if (s_sd_status.b.inserted == 0) {
		SD_LOGE("SD Card not insert");
		return FSP_ERR_ABORTED;
	}
	if (s_sd_block_size == 0) {
		SD_LOGE("Block size if zero, some error occur");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((first_block + count) > s_sd_block_num) {
		SD_LOGE("Block out of memory");
		return FSP_ERR_INVALID_BLOCKS;
	}

#if BSP_CFG_RTOS == 2
	EventBits_t eb;
#else
	uint32_t timeout_us = timeout_ms * 1000;
#endif

	if (s_sd_status.b.trans_done == 0) {
		return FSP_ERR_IN_USE;
	}

#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_DisableDCache();
#endif

	for (i = 0; i < repeat; i++) {
		err = SD_WaitTrans(timeout_ms);
		if (err) {
			SD_LOGE("Wait trans failed: %" PRIu32, err);
			goto WRITE_BLOCK_EXIT;
		}
		err = RM_BLOCK_MEDIA_SDMMC_Write(SD_INSTANCE.p_ctrl, p_write, block_addr, 0x10000);
		if (err) {
			SD_LOGE("Write failed: %" PRIu32, err);
			goto WRITE_BLOCK_EXIT;
		}
		s_sd_status.b.trans_done = 0;
	#if BSP_CFG_RTOS == 2
		xEventGroupClearBits(s_sd_event, SD_EVENT_TRANS_DONE);
		eb = xEventGroupWaitBits(s_sd_event, SD_EVENT_TRANS_DONE, pdTRUE, pdTRUE, timeout_ms);
		if ((eb & SD_EVENT_TRANS_DONE) == 0x00) {
			SD_LOGE("Write timeout");
			err = FSP_ERR_TIMEOUT;
			goto WRITE_BLOCK_EXIT;
		}
	#else
		while (s_sd_status.b.trans_done == 0) {
			if (timeout_us == 0) {
				SD_LOGE("Write timeout");
				err = FSP_ERR_TIMEOUT;
				goto WRITE_BLOCK_EXIT;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
			timeout_us--;
		}
	#endif
		block_addr += 0x10000;
		count -= 0x10000;
		p_write = &p_write[s_sd_block_size * 0x10000];
	}

	if (count) {
		err = SD_WaitTrans(timeout_ms);
		if (err) {
			SD_LOGE("Wait trans failed: %" PRIu32, err);
			goto WRITE_BLOCK_EXIT;
		}
		err = RM_BLOCK_MEDIA_SDMMC_Write(SD_INSTANCE.p_ctrl, p_write, block_addr, count);
		if (err) {
			SD_LOGE("Write failed: %" PRIu32, err);
			goto WRITE_BLOCK_EXIT;
		}
		s_sd_status.b.trans_done = 0;
	#if BSP_CFG_RTOS == 2
		xEventGroupClearBits(s_sd_event, SD_EVENT_TRANS_DONE);
		eb = xEventGroupWaitBits(s_sd_event, SD_EVENT_TRANS_DONE, pdTRUE, pdTRUE, timeout_ms);
		if ((eb & SD_EVENT_TRANS_DONE) == 0x00) {
			SD_LOGE("Write timeout");
			err = FSP_ERR_TIMEOUT;
			goto WRITE_BLOCK_EXIT;
		}
	#else
		while (s_sd_status.b.trans_done == 0) {
			if (timeout_us == 0) {
				SD_LOGE("Write timeout");
				err = FSP_ERR_TIMEOUT;
				goto WRITE_BLOCK_EXIT;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
			timeout_us--;
		}
	#endif
	}

WRITE_BLOCK_EXIT:
#if BSP_CFG_DCACHE_ENABLED
	__DSB();
	__ISB();
    SCB_EnableDCache();
#endif
	s_sd_status.b.trans_done = 1;

	return err;
}

void SD_CALLBACK(rm_block_media_callback_args_t *p_args)
{
#if BSP_CFG_RTOS == 2
	BaseType_t xHigherPriorityTaskWoken;
#endif

	switch (p_args->event) {
	case RM_BLOCK_MEDIA_EVENT_MEDIA_REMOVED:
		SD_LOGD("Remove");
		s_sd_status.b.inserted = 0;
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_REMOVED, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_INSERTED:
		SD_LOGD("Inserted");
		s_sd_status.b.inserted = 1;
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_INSERTED, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_OPERATION_COMPLETE:
		s_sd_status.b.trans_done = 1;
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_TRANS_DONE, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_ERROR:
		SD_LOGE("Error occur");
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_ERROR, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_POLL_STATUS:
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_POLL_STATUS, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_SUSPEND:
		SD_LOGD("Suspend");
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_SUSPEND, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_MEDIA_RESUME:
		SD_LOGD("Resume");
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_RESUME, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_WAIT:
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_WAIT, &xHigherPriorityTaskWoken);
	#endif
		break;
	case RM_BLOCK_MEDIA_EVENT_WAIT_END:
	#if BSP_CFG_RTOS
		xEventGroupSetBitsFromISR(s_sd_event, SD_EVENT_WAIT_END, &xHigherPriorityTaskWoken);
	#endif
		break;
	default:
		SD_LOGW("Uncheck event: 0x%" PRIx32, (uint32_t)p_args->event);
		break;
	}
}
