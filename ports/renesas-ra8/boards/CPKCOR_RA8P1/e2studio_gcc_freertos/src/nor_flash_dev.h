#ifndef MICROPY_INCLUDED_RENESAS_RA8_NOR_FLASH_DEV_H
#define MICROPY_INCLUDED_RENESAS_RA8_NOR_FLASH_DEV_H

#include "py/obj.h"

extern const mp_obj_type_t nor_flash_dev_type;

mp_obj_t NorFlashDev_GetBlockDevice(void);
int NorFlashDev_Format(void);
int NorFlashDev_Mount(void);
int NorFlashDev_Unmount(void);

#endif
