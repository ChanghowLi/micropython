#include "test.h"

#if TEST_EN_MRAM

#include <inttypes.h>
#include "hal_data.h"
#include "perf_counter/perf_counter.h"
#include "utils/log.h"

#define TAG __FUNCTION__

#ifndef TEST_MRAM_EN_W
#define TEST_MRAM_EN_W			1
#endif

#ifndef TEST_MRAM_EN_R
#define TEST_MRAM_EN_R			1
#endif

#ifndef TEST_MRAM_CACHE_SIZE
#define TEST_MRAM_CACHE_SIZE	(1024 * 16)
#endif

static uint8_t s_rcache[TEST_MRAM_CACHE_SIZE];
static uint8_t s_wcache[TEST_MRAM_CACHE_SIZE];

uint32_t TestMRAM(uint32_t start_addr, uint32_t size)
{
	fsp_err_t err;
	uint32_t i;
	uint32_t wlen;
	uint32_t repeat, remain;
	int64_t time_start, time_end;
	double speed;

	uint32_t *p32 = (uint32_t *)s_wcache;

	for (i = 0; i < (TEST_MRAM_CACHE_SIZE / 4); i++) {
		p32[i] = i;
	}

	__disable_irq();

	if (size < TEST_MRAM_CACHE_SIZE) {
		time_start = get_system_us();
		err = R_MRAM_Write(g_mram0.p_ctrl, (uint32_t)s_wcache, start_addr, size);
		if (err) {
			LOG_E(TAG, "Write failed: %" PRIu32, err);
			__enable_irq();
			goto EXIT;
		}
		time_end = get_system_us();
		if (time_end != time_start) {
			LOG_I(TAG, "Write %" PRIu32 " bytes using %" PRIu32 "us", size, (uint32_t)(time_end - time_start));
			speed = (double)size / (double)(time_end - time_start) * 1000.0f * 1000.0f / 1024.0f / 1024.0f;
			LOG_I(TAG, "Speed: %.2f MB/s", speed);
		}
		else {
			LOG_W(TAG, "Test size %" PRIu32 " is too small for writing", size);
		}

		time_start = get_system_us();
		memcpy(s_rcache, (uint8_t *)start_addr, size);
		time_end = get_system_us();
		if (memcmp(s_rcache, s_wcache, size) == 0) {
			LOG_I(TAG, "Compare PASS");
			if (time_end != time_start) {
				LOG_I(TAG, "Read %" PRIu32 " bytes using %" PRIu32 "us", size, (uint32_t)(time_end - time_start));
				speed = (double)size / (double)(time_end - time_start) * 1000.0f * 1000.0f / 1024.0f / 1024.0f;
				LOG_I(TAG, "Speed: %.2f MB/s", speed);
			}
			else {
				LOG_W(TAG, "Test size %" PRIu32 " is too small for reading", size);
			}
		}
		else {
			LOG_W(TAG, "Compare FAILED");
		}
	}
	else {
		repeat = size / TEST_MRAM_CACHE_SIZE;
		remain = size % TEST_MRAM_CACHE_SIZE;
		if (remain) {
			LOG_I(TAG, "Ignore the lastest %" PRIu32 " bytes", remain);
		}
		for (i = 0; i < repeat; i++) {
			time_start = get_system_us();
			err = R_MRAM_Write(g_mram0.p_ctrl, (uint32_t)s_wcache, start_addr, TEST_MRAM_CACHE_SIZE);
			if (err) {
				LOG_E(TAG, "Write failed: %" PRIu32, err);
				__enable_irq();
				goto EXIT;
			}
			time_end = get_system_us();
			if (time_end != time_start) {
				LOG_I(TAG, "Write %" PRIu32 " bytes using %" PRIu32 "us", TEST_MRAM_CACHE_SIZE, (uint32_t)(time_end - time_start));
				speed = (double)TEST_MRAM_CACHE_SIZE / (double)(time_end - time_start) * 1000.0f * 1000.0f / 1024.0f / 1024.0f;
				LOG_I(TAG, "Speed: %.2f MB/s", speed);
			}
			else {
				LOG_W(TAG, "Test size %" PRIu32 " is too small for writing", TEST_MRAM_CACHE_SIZE);
			}

			time_start = get_system_us();
			memcpy(s_rcache, (uint8_t *)start_addr, TEST_MRAM_CACHE_SIZE);
			time_end = get_system_us();
			if (memcmp(s_rcache, s_wcache, TEST_MRAM_CACHE_SIZE) == 0) {
				LOG_I(TAG, "Compare PASS");
				if (time_end != time_start) {
					LOG_I(TAG, "Read %" PRIu32 " bytes using %" PRIu32 "us", TEST_MRAM_CACHE_SIZE, (uint32_t)(time_end - time_start));
					speed = (double)TEST_MRAM_CACHE_SIZE / (double)(time_end - time_start) * 1000.0f * 1000.0f / 1024.0f / 1024.0f;
					LOG_I(TAG, "Speed: %.2f MB/s", speed);
				}
				else {
					LOG_W(TAG, "Test size %" PRIu32 " is too small for reading", TEST_MRAM_CACHE_SIZE);
				}
			}
			else {
				LOG_W(TAG, "Compare FAILED");
				goto EXIT;
			}

			start_addr += TEST_MRAM_CACHE_SIZE;
			memset(s_rcache, 0, TEST_MRAM_CACHE_SIZE);
		}
	}

EXIT:
	__enable_irq();

	return err;
}

#endif
