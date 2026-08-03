#ifndef __SDCARD_H
#define __SDCARD_H

#include "py/obj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mp_obj_base_t base;
    bool initialized;
    uint32_t block_count;
    uint32_t block_size;
} machine_sdcard_obj_t;

extern const mp_obj_type_t machine_sdcard_type;

#ifdef __cplusplus
}
#endif

#endif
