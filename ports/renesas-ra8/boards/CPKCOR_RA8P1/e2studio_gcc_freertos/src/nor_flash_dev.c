#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nor_flash.h"
#include "nor_flash_dev.h"

#include "extmod/vfs.h"
#include "extmod/vfs_lfs.h"
#include "py/mperrno.h"
#include "py/objarray.h"
#include "py/runtime.h"

#define TAG __FUNCTION__

#ifndef __NOR_FLASH_DEV_DEBUG
#define __NOR_FLASH_DEV_DEBUG    1
#endif

#if __NOR_FLASH_DEV_DEBUG
#include "utils/log.h"
#endif

#define NOR_FLASH_DEV_SYNC_TIMEOUT_MS    (5000U)

_Static_assert(NORFLASH_FS_OFFSET % NORFLASH_SECTOR_SIZE == 0,
    "NOR flash filesystem start must be sector aligned");
_Static_assert(NORFLASH_FS_SIZE % NORFLASH_SECTOR_SIZE == 0,
    "NOR flash filesystem size must contain complete sectors");

typedef struct {
    mp_obj_base_t base;
} nor_flash_dev_obj_t;

const mp_obj_type_t nor_flash_dev_type;

static const nor_flash_dev_obj_t nor_flash_dev_obj = {
    .base = {&nor_flash_dev_type},
};

static mp_obj_t nor_flash_dev_result(uint32_t result)
{
    return MP_OBJ_NEW_SMALL_INT(result == 0 ? 0 : -MP_EIO);
}

static int nor_flash_dev_exception_errno(nlr_buf_t *nlr)
{
    mp_obj_base_t *exc = nlr->ret_val;
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_OSError))) {
        mp_int_t error;
        mp_obj_t value = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
        if (mp_obj_get_int_maybe(value, &error)) {
            return error > 0 ? -error : error;
        }
    }

    return -MP_EIO;
}

static bool nor_flash_dev_resolve_address(mp_obj_t block_in, mp_int_t offset, size_t length, uint32_t *relative_address)
{
    mp_int_t block = mp_obj_get_int(block_in);
    if (block < 0 || offset < 0) {
        return false;
    }

    uint64_t address = (uint64_t)(mp_uint_t)block * NORFLASH_SECTOR_SIZE + (mp_uint_t)offset;
    if (address > NORFLASH_FS_SIZE || length > NORFLASH_FS_SIZE - address) {
        return false;
    }

    *relative_address = (uint32_t)address;
    return true;
}

static uint32_t nor_flash_dev_read(uint32_t relative_address, void *data, size_t length)
{
    /* TODO 读取时没有一次只能读 64 字节的限制，且不要使用 NorFlash_Read()，这是早期调试 NorFlash 时写的用指令读取的 API，效率很低
     * 参考 test_nor_flash.c 中直接使用内存映射模式读 */
    uint8_t *destination = data;
    uint64_t aligned_buffer[8];

    while (length != 0) {
        size_t chunk = length > sizeof(aligned_buffer) ? sizeof(aligned_buffer) : length;
        uint32_t result = NorFlash_Read(NORFLASH_FS_OFFSET + relative_address, aligned_buffer, chunk);
        if (result != 0) {
            return result;
        }

        memcpy(destination, aligned_buffer, chunk);
        destination += chunk;
        relative_address += chunk;
        length -= chunk;
    }

    return 0;
}

static mp_obj_t nor_flash_dev_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    (void)type;
    (void)args;

    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    return MP_OBJ_FROM_PTR(&nor_flash_dev_obj);
}

static mp_obj_t nor_flash_dev_readblocks(size_t n_args, const mp_obj_t *args)
{
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(args[2], &buffer, MP_BUFFER_WRITE);

    mp_int_t offset = n_args == 4 ? mp_obj_get_int(args[3]) : 0;
    uint32_t relative_address;
    if (!nor_flash_dev_resolve_address(args[1], offset, buffer.len, &relative_address)) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }

    uint32_t result = nor_flash_dev_read(relative_address, buffer.buf, buffer.len);
    if (result != 0) {
    #if __NOR_FLASH_DEV_DEBUG
        LOG_E(TAG, "Read failed: address=0x%08lX, length=%lu, FSP error=0x%08lX", NORFLASH_FS_START_ADDR + relative_address, (uint32_t)buffer.len, result);
	#endif
    }
    return nor_flash_dev_result(result);
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(nor_flash_dev_readblocks_obj, 3, 4, nor_flash_dev_readblocks);

static mp_obj_t nor_flash_dev_writeblocks(size_t n_args, const mp_obj_t *args)
{
    mp_buffer_info_t buffer;

    mp_get_buffer_raise(args[2], &buffer, MP_BUFFER_READ);

    mp_int_t offset = n_args == 4 ? mp_obj_get_int(args[3]) : 0;
    uint32_t relative_address;
    if (!nor_flash_dev_resolve_address(args[1], offset, buffer.len, &relative_address)) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }

    if (n_args == 3) {
        if (buffer.len % NORFLASH_SECTOR_SIZE != 0) {
            return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
        }

        mp_int_t block = mp_obj_get_int(args[1]);
        size_t block_count = buffer.len / NORFLASH_SECTOR_SIZE;
        for (size_t i = 0; i < block_count; ++i) {
            uint32_t sector = NORFLASH_FS_OFFSET / NORFLASH_SECTOR_SIZE
                + (uint32_t)block + i;
            uint32_t result = NorFlash_EraseSector(sector);
            if (result != 0) {
            #if __NOR_FLASH_DEV_DEBUG
                LOG_E(TAG, "Erase failed: block=%d, sector=%lu, FSP error=0x%08lX", block + (mp_int_t)i, sector, result);
            #endif
                return nor_flash_dev_result(result);
            }
        }
    }

    uint32_t result = NorFlash_Program(NORFLASH_FS_START_ADDR + relative_address,
        buffer.buf, buffer.len);
    if (result != 0) {
    #if __NOR_FLASH_DEV_DEBUG
        LOG_E(TAG, "Program failed: address=0x%08lX, length=%lu, FSP error=0x%08lX", NORFLASH_FS_START_ADDR + relative_address, (uint32_t)buffer.len, result);
    #endif
    }

    return nor_flash_dev_result(result);
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(nor_flash_dev_writeblocks_obj, 3, 4, nor_flash_dev_writeblocks);

static mp_obj_t nor_flash_dev_ioctl(mp_obj_t self_in, mp_obj_t command_in, mp_obj_t argument_in)
{
    (void)self_in;

    mp_int_t command = mp_obj_get_int(command_in);
    switch (command) {
        case MP_BLOCKDEV_IOCTL_INIT:
        case MP_BLOCKDEV_IOCTL_DEINIT:
            return MP_OBJ_NEW_SMALL_INT(0);

        case MP_BLOCKDEV_IOCTL_SYNC: {
            uint32_t result = NorFlash_WaitOperation(NOR_FLASH_DEV_SYNC_TIMEOUT_MS);
            if (result != 0) {
            #if __NOR_FLASH_DEV_DEBUG
                LOG_E(TAG, "Sync failed: FSP error=0x%08lX", result);
            #endif
            }
            return nor_flash_dev_result(result);
        }

        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
            return MP_OBJ_NEW_SMALL_INT(NORFLASH_FS_SIZE / NORFLASH_SECTOR_SIZE);

        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
            return MP_OBJ_NEW_SMALL_INT(NORFLASH_SECTOR_SIZE);

        case MP_BLOCKDEV_IOCTL_BLOCK_ERASE: {
            mp_int_t block = mp_obj_get_int(argument_in);
            if (block < 0 || (mp_uint_t)block >= NORFLASH_FS_SIZE / NORFLASH_SECTOR_SIZE) {
                return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
            }

            uint32_t sector = NORFLASH_FS_OFFSET / NORFLASH_SECTOR_SIZE + (uint32_t)block;
            uint32_t result = NorFlash_EraseSector(sector);
            if (result != 0) {
            #if __NOR_FLASH_DEV_DEBUG
                LOG_E(TAG, "Erase failed: block=%d, sector=%lu, FSP error=0x%08lX", block, sector, result);
            #endif
            }
            return nor_flash_dev_result(result);
        }

        default:
            return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_3(nor_flash_dev_ioctl_obj, nor_flash_dev_ioctl);

static const mp_rom_map_elem_t nor_flash_dev_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&nor_flash_dev_ioctl_obj)},
    {MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&nor_flash_dev_readblocks_obj)},
    {MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&nor_flash_dev_writeblocks_obj)},
};
static MP_DEFINE_CONST_DICT(nor_flash_dev_locals_dict, nor_flash_dev_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    nor_flash_dev_type,
    MP_QSTR_NorFlash,
    MP_TYPE_FLAG_NONE,
    make_new, nor_flash_dev_make_new,
    locals_dict, &nor_flash_dev_locals_dict
    );

mp_obj_t NorFlashDev_GetBlockDevice(void)
{
    return MP_OBJ_FROM_PTR(&nor_flash_dev_obj);
}

int NorFlashDev_Format(void)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t method[3];
        mp_load_method(MP_OBJ_FROM_PTR(&mp_type_vfs_lfs2), MP_QSTR_mkfs, method);
        method[2] = NorFlashDev_GetBlockDevice();
        mp_call_method_n_kw(1, 0, method);
        nlr_pop();
        return 0;
    }

    return nor_flash_dev_exception_errno(&nlr);
}

int NorFlashDev_Mount(void)
{
    mp_obj_t mount_point = MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash);

    int result = mp_vfs_mount_and_chdir_protected(NorFlashDev_GetBlockDevice(), mount_point);
#if __NOR_FLASH_DEV_DEBUG
    LOG_D(TAG, "Initial mount: result=%d", result);
#endif
    if (result == 0) {
        return 0;
    }

    if (result != -MP_ENODEV) {
        return result;
    }

    result = NorFlashDev_Format();
#if __NOR_FLASH_DEV_DEBUG
    LOG_D(TAG, "Format: result=%d", result);
#endif
    if (result != 0) {
        return result;
    }

    result = mp_vfs_mount_and_chdir_protected(NorFlashDev_GetBlockDevice(), mount_point);
#if __NOR_FLASH_DEV_DEBUG
    LOG_D(TAG, "Remount: result=%d", result);
#endif

    return result;
}

int NorFlashDev_Unmount(void)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_vfs_umount(MP_OBJ_NEW_QSTR(MP_QSTR__slash_flash));
        nlr_pop();
        return 0;
    }

    return nor_flash_dev_exception_errno(&nlr);
}
