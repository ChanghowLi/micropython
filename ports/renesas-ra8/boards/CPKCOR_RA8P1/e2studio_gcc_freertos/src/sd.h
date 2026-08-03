#ifndef __SD_H
#define __SD_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t SD_Deinit(void);
uint32_t SD_GetInfo(uint32_t *block_count, uint32_t *block_size);
uint32_t SD_Init(void);
uint32_t SD_InitMedia(void);
uint32_t SD_IsInsert(void);
uint32_t SD_IsPresent(bool *present);
uint32_t SD_IsTransDone(void);
uint32_t SD_ReadBlock(void *data, uint32_t first_block, uint32_t count, uint32_t timeout_ms);
uint32_t SD_WaitTrans(uint32_t timeout_ms);
uint32_t SD_WriteBlock(void *data, uint32_t first_block, uint32_t count, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
