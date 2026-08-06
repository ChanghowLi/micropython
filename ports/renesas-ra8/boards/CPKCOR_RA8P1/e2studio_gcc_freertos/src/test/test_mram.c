#include "test.h"

#if TEST_EN_MRAM

#ifndef TEST_MARM_EN_W
#define TEST_MARM_EN_W		1
#endif

#ifndef TEST_MARM_EN_R
#define TEST_MARM_EN_R		1
#endif

uint32_t TestMRAM(uint32_t start_addr, uint32_t size)
{
	(void)start_addr;
	(void)size;

#if TEST_MARM_EN_W
#endif

	return 0;
}

#endif
