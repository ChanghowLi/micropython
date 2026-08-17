#ifndef __MRAM_DEV_H
#define __MRAM_DEV_H

#include "py/obj.h"

/**
 * 注意：
 * - 当前的 e2 工程是 project，不是 solution，不能在 e2 中设定 MRAM 大小，除非改用自定义链接脚本。因此务必确认编译的固件不大于 0xF0000，后续再考虑转为使用 solution
 * - 由于 LittleFS 对块大小的设置最小为 128，而 MRAM 又以 32 字节为单位进行擦除。因此 MRAM_DEV_SECTOR_SIZE 务必保证为 32 的整数倍 */
#define MRAM_DEV_START_ADDR     0x020F0000
#define MRAM_DEV_SIZE           (1024 * 64)
#define MRAM_DEV_SECTOR_SIZE    128

#ifdef __cplusplus
extern "C" {
#endif

extern const mp_obj_type_t mram_dev_type;

int MRAM_DEV_Format(void);
int MRAM_DEV_Mount(void);
int MRAM_DEV_Unmount(void);

#ifdef __cplusplus
}
#endif

#endif
