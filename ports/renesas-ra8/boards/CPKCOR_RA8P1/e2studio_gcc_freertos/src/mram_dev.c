#include <inttypes.h>
#include "hal_data.h"
#include "mram_dev.h"

#include "extmod/vfs.h"
#include "extmod/vfs_lfs.h"
#include "py/mperrno.h"
#include "py/objarray.h"
#include "py/runtime.h"

#ifndef __MRAM_DEV_DEBUG
#define __MRAM_DEV_DEBUG    0
#endif

#if __MRAM_DEV_DEBUG
#include "utils/log.h"
#define MRAM_DEV_LOGD(msg, ...)     LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define MRAM_DEV_LOGI(msg, ...)     LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define MRAM_DEV_LOGW(msg, ...)     LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define MRAM_DEV_LOGE(msg, ...)     LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define MRAM_DEV_LOGD(msg, ...)
#define MRAM_DEV_LOGI(msg, ...)
#define MRAM_DEV_LOGW(msg, ...)
#define MRAM_DEV_LOGE(msg, ...)
#endif

struct mram_dev_obj_t {
    mp_obj_base_t base;
};

const mp_obj_type_t mram_dev_type;
static const struct mram_dev_obj_t sc_mram_dev_obj = {
    .base = {&mram_dev_type},
};

int MRAM_DEV_Format(void)
{
    nlr_buf_t nlr;
    mp_int_t error;
    mp_obj_t method[3];
    mp_obj_t value;

    mp_obj_base_t *exc = NULL;

    if (nlr_push(&nlr) == 0) {
        mp_load_method(MP_OBJ_FROM_PTR(&mp_type_vfs_lfs2), MP_QSTR_mkfs, method);
        method[2] = MP_OBJ_FROM_PTR(&sc_mram_dev_obj);
        mp_call_method_n_kw(1, 0, method);
        nlr_pop();
        return 0;
    }

    exc = nlr.ret_val;
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_OSError))) {
        MRAM_DEV_LOGE("Catch exception: OSError");
        value = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
        if (mp_obj_get_int_maybe(value, &error)) {
            error = error > 0 ? -error : error;
            MRAM_DEV_LOGE("error: %d", error);
            return error;
        }
    }
    MRAM_DEV_LOGE("Uncatch exception occur");

    return -MP_EIO;
}

int MRAM_DEV_Mount(void)
{
    int result;

    mp_obj_t mount_point = MP_OBJ_NEW_QSTR(MP_QSTR__slash_mram);

    result = mp_vfs_mount_and_chdir_protected(MP_OBJ_FROM_PTR(&sc_mram_dev_obj), mount_point);
    if (result == 0) {
        MRAM_DEV_LOGD("MRAM mount success");
        return 0;
    }
    if (result != -MP_ENODEV) {
        MRAM_DEV_LOGE("MRAM mount failed: %d", result);
        return result;
    }

    MRAM_DEV_LOGW("First mount failed, try format and remount");
    result = MRAM_DEV_Format();
    if (result) {
        MRAM_DEV_LOGE("Format failed: %d", result);
        return result;
    }
    MRAM_DEV_LOGD("Format success");
    result = mp_vfs_mount_and_chdir_protected(MP_OBJ_FROM_PTR(&sc_mram_dev_obj), mount_point);
    if (result) {
        MRAM_DEV_LOGE("Second mount failed: %d", result);
        return result;
    }
    MRAM_DEV_LOGI("Second mount success");

    return 0;
}

int MRAM_DEV_Unmount(void)
{
    mp_int_t error;
    mp_obj_t value;
    nlr_buf_t nlr;

    mp_obj_base_t *exc = NULL;

    if (nlr_push(&nlr) == 0) {
        mp_vfs_umount(MP_OBJ_NEW_QSTR(MP_QSTR__slash_mram));
        nlr_pop();
        return 0;
    }

    exc = nlr.ret_val;
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_OSError))) {
        MRAM_DEV_LOGE("Catch exception: OSError");
        value = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
        if (mp_obj_get_int_maybe(value, &error)) {
            error = error > 0 ? -error : error;
            MRAM_DEV_LOGE("error: %d", error);
            return error;
        }
    }
    MRAM_DEV_LOGE("Uncatch exception occur");

    return -MP_EIO;
}

static mp_obj_t mram_dev_ioctl(mp_obj_t self, mp_obj_t command, mp_obj_t argument)
{
    mp_int_t block;
    uint32_t err;
    uint32_t erase_addr;

    (void)self;

    mp_obj_t ret = MP_OBJ_NEW_SMALL_INT(0);

    mp_int_t cmd = mp_obj_get_int(command);
    switch (cmd) {
    case MP_BLOCKDEV_IOCTL_INIT:
        MRAM_DEV_LOGD("IOCTL_INIT");
        ret = MP_OBJ_NEW_SMALL_INT(0);
        break;
    case MP_BLOCKDEV_IOCTL_DEINIT:
        MRAM_DEV_LOGD("IOCTL_DEINIT");
        ret = MP_OBJ_NEW_SMALL_INT(0);
        break;
    case MP_BLOCKDEV_IOCTL_SYNC:
        MRAM_DEV_LOGD("IOCTL_SYNC");
        ret = MP_OBJ_NEW_SMALL_INT(0);
        break;
    case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
        MRAM_DEV_LOGD("IOCTL_BLOCK_COUNT");
        ret = MP_OBJ_NEW_SMALL_INT(MRAM_DEV_SIZE / MRAM_DEV_SECTOR_SIZE);
        break;
    case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
        MRAM_DEV_LOGD("IOCTL_BLOCK_SIZE");
        ret = MP_OBJ_NEW_SMALL_INT(MRAM_DEV_SECTOR_SIZE);
        break;
    case MP_BLOCKDEV_IOCTL_BLOCK_ERASE:
        MRAM_DEV_LOGD("IOCTL_BLOCK_ERASE");
        block = mp_obj_get_int(argument);
        if ((block < 0) || (block >= MRAM_DEV_SIZE / MRAM_DEV_SECTOR_SIZE)) {
            MRAM_DEV_LOGE("block [%d] out of range", block);
            ret = MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
        }
        else {
            erase_addr = MRAM_DEV_START_ADDR + block * MRAM_DEV_SECTOR_SIZE;
            __disable_irq();
            err = R_MRAM_Erase(g_mram0.p_ctrl, erase_addr, MRAM_DEV_SECTOR_SIZE / 32);
            __enable_irq();
            if (err) {
                MRAM_DEV_LOGE("Erase failed [%" PRIu32 "] at address 0x%" PRIX32, err, erase_addr);
                ret = MP_OBJ_NEW_SMALL_INT(-MP_EIO);
            }
            else {
                MRAM_DEV_LOGI("Erase address 0x%" PRIX32 " success", erase_addr);
            }
        }
        break;
    default:
        MRAM_DEV_LOGW("Unhandle command: %d", cmd);
        break;
    }

    return ret;
}

static mp_obj_t mram_dev_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    MRAM_DEV_LOGD("Call");

    (void)type;
    (void)args;

    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    return MP_OBJ_FROM_PTR(&sc_mram_dev_obj);
}

static mp_obj_t mram_dev_readblock(size_t n_args, const mp_obj_t *args)
{
    /** Two forms:
     * vfs.AbstractBlockDev.readblocks(block_num, buf)
     *  args[0] = self
     *  args[1] = block_num
     *  args[2] = buf
     * vfs.AbstractBlockDev.readblocks(block_num, buf, offset)
     *  args[0] = self
     *  args[1] = block_num
     *  args[2] = buf
     *  args[3] = offset */

    mp_buffer_info_t buffer;
    uint32_t read_addr;
    uint32_t avaliable_len;

    mp_int_t block_num = mp_obj_get_int(args[1]);
    mp_int_t offset = n_args == 4 ? mp_obj_get_int(args[3]) : 0;

    mp_get_buffer_raise(args[2], &buffer, MP_BUFFER_WRITE);
    read_addr = MRAM_DEV_START_ADDR + block_num * MRAM_DEV_SECTOR_SIZE + offset;
    if ((read_addr < MRAM_DEV_START_ADDR) || (read_addr > (MRAM_DEV_START_ADDR + MRAM_DEV_SIZE))) {
        MRAM_DEV_LOGE("Try to read a invalid address: %" PRIu32, read_addr);
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }
    avaliable_len = MRAM_DEV_START_ADDR + MRAM_DEV_SIZE - read_addr;
    if (buffer.len > avaliable_len) {
        MRAM_DEV_LOGE("Apply length [%u] greater then avaliable length [%" PRIu32 "]", buffer.len, avaliable_len);
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }
    MRAM_DEV_LOGD("block_num: %d, offset: %d, len: %d", block_num, offset, buffer.len);
    memcpy(buffer.buf, (void *)read_addr, buffer.len);

    return MP_OBJ_NEW_SMALL_INT(0);
}

static mp_obj_t mram_dev_writeblocks(size_t n_args, const mp_obj_t *args)
{
    /* parameter like readblocks() */

    mp_buffer_info_t buffer;
    uint32_t avaliable_len;
    uint32_t err;
    uint32_t write_addr;

    mp_int_t block_num = mp_obj_get_int(args[1]);
    mp_int_t offset = n_args == 4 ? mp_obj_get_int(args[3]) : 0;
    mp_get_buffer_raise(args[2], &buffer, MP_BUFFER_READ);
    write_addr = MRAM_DEV_START_ADDR + block_num * MRAM_DEV_SECTOR_SIZE + offset;
    if ((write_addr < MRAM_DEV_START_ADDR) || (write_addr > (MRAM_DEV_START_ADDR + MRAM_DEV_SIZE))) {
        MRAM_DEV_LOGE("Try to write a invalid address: %" PRIu32, write_addr);
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }
    avaliable_len = MRAM_DEV_START_ADDR + MRAM_DEV_SIZE - write_addr;
    if (buffer.len > avaliable_len) {
        MRAM_DEV_LOGE("Apply length [%u] greater then avaliable length [%" PRIu32 "]", buffer.len, avaliable_len);
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }
    MRAM_DEV_LOGD("block_num: %d, offset: %d, len: %d", block_num, offset, buffer.len);
    __disable_irq();
    err = R_MRAM_Write(g_mram0.p_ctrl, (uint32_t)buffer.buf, write_addr, (uint32_t)buffer.len);
    __enable_irq();
    if (err) {
        MRAM_DEV_LOGE("Write failed: %" PRIu32, err);
        return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
    }

    return MP_OBJ_NEW_SMALL_INT(0);
}

static MP_DEFINE_CONST_FUN_OBJ_3(mram_dev_ioctl_obj, mram_dev_ioctl);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mram_dev_read_obj, 3, 4, mram_dev_readblock);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mram_dev_write_obj, 3, 4, mram_dev_writeblocks);

static mp_rom_map_elem_t mram_dev_local_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&mram_dev_ioctl_obj)},
    {MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&mram_dev_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&mram_dev_write_obj)}
};

static MP_DEFINE_CONST_DICT(mram_dev_local_dict, mram_dev_local_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mram_dev_type,
    MP_QSTR_MRAM,
    MP_TYPE_FLAG_NONE,
    make_new, mram_dev_make_new,
    locals_dict, &mram_dev_local_dict
);
