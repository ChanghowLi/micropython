#ifndef __NOR_FLASH_H
#define __NOR_FLASH_H

#include <stdint.h>

#define NORFLASH_MAP_START_ADDR		0x78000000
#define NORFLASH_MAP_END_ADDR		0x80000000
#define NORFLASH_SIZE				(1024 * 1024 * 64)
#define NORFLASH_SECTOR_SIZE		(1024 * 4)
#define NORFLASH_BLOCK_SIZE			(1024 * 64)

/* FSP Autocalibration address is 0x78002000, so the first few sectors are to be retained */
#define NORFLASH_FS_START_ADDR		0x78004000
#define NORFLASH_FS_OFFSET			(NORFLASH_FS_START_ADDR - NORFLASH_MAP_START_ADDR)
#define NORFLASH_FS_SIZE			(NORFLASH_SIZE - NORFLASH_FS_OFFSET)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CHIP_W35N01JW,
	CHIP_W35T51NW,
	CHIP_UNKNOW
} NorFlashChip;

extern const uint32_t gc_autocalibration[];

uint32_t NorFlash_EraseChip(void);
uint32_t NorFlash_EraseSector(uint32_t sector);
uint32_t NorFlash_Program(uint32_t address, void const *data, uint32_t length);
uint32_t NorFlash_Init(void);
uint32_t NorFlash_Read(uint32_t address, void *data, uint32_t length);
uint32_t NorFlash_SetWriteEnable(void);
uint32_t NorFlash_WaitOperation(uint32_t timeout);
uint32_t NorFlash_WriteSector(uint32_t sector, void *data, uint32_t length);

void NorFlash_DumpOSPIReg(void);

#ifdef __cplusplus
}
#endif

#endif
