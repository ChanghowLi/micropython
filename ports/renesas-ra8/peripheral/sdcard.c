#include "hal_data.h"
#include "sd.h"
#include "sdcard.h"

#include "extmod/vfs.h"
#include "py/mperrno.h"
#include "py/runtime.h"

#if BSP_CFG_RTOS == 2
#include "FreeRTOS.h"
#include "task.h"
#endif

#define TAG __FUNCTION__

#ifndef __SDCARD_DEBUG
#define __SDCARD_DEBUG  1
#endif

#if __SDCARD_DEBUG
#include "utils/log.h"
#define SDCARD_LOGD(msg, ...)       LOG_D(TAG, msg, ##__VA_ARGS__)
#define SDCARD_LOGI(msg, ...)       LOG_I(TAG, msg, ##__VA_ARGS__)
#define SDCARD_LOGW(msg, ...)       LOG_W(TAG, msg, ##__VA_ARGS__)
#define SDCARD_LOGE(msg, ...)       LOG_E(TAG, msg, ##__VA_ARGS__)
#else
#define SDCARD_LOGD(msg, ...)
#define SDCARD_LOGI(msg, ...)
#define SDCARD_LOGW(msg, ...)
#define SDCARD_LOGE(msg, ...)
#endif

static machine_sdcard_obj_t s_machine_sdcard;

/**
 * @brief   执行 sd.deinit() 时的 C 接口
 * @param   self_in sd 对象，即 Python 中的 self
 * @return  总是返回 mp_const_none
 */
static mp_obj_t machine_sdcard_deinit(mp_obj_t self_in)
{
    uint32_t err;

    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);

    err = SD_Deinit();
    if (err) {
        mp_raise_OSError(MP_EIO);
    }

    self->initialized = false;
    self->block_count = 0;
    self->block_size = 0;

    return mp_const_none;
}

static mp_obj_t machine_sdcard_info(mp_obj_t self_in)
{
    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t tuple[2] = {
        mp_obj_new_int_from_ull((uint64_t)self->block_count * (uint64_t)self->block_size),
        mp_obj_new_int_from_uint(self->block_size),
    };

    return mp_obj_new_tuple(2, tuple);
}

/**
 * @brief   执行 sd.init() 时的 C 接口
 * @param   self_in sd 对象，即 Python 中的 self
 * @return  总是返回 mp_const_none
 */
static mp_obj_t machine_sdcard_init(mp_obj_t self_in)
{
    uint32_t err;
    bool present;

    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);

    SDCARD_LOGD("Call");
    err = SD_Init();
    if (err) {
        if (err == FSP_ERR_ALREADY_OPEN) {
            return mp_const_none;
        }
        mp_raise_OSError(MP_EIO);
    }

    /* SD 卡初始化之后，即使卡一直插在卡槽，SDHI 也需要约 300ms 才能识别到卡插入 */
#if BSP_CFG_RTOS == 2
    vTaskDelay(300);
#else
    R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MILLISECONDS);
#endif

    SD_IsPresent(&present);
    if (present == false) {
        /* 再次等待，留足余量 */
    #if BSP_CFG_RTOS == 2
        vTaskDelay(300);
    #else
        R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MILLISECONDS);
    #endif
        SD_IsPresent(&present);
        if (present == false) {
            mp_raise_OSError(MP_ENODEV);
        }
    }

    err = SD_InitMedia();
    if (err) {
        SD_Deinit();
        self->initialized = false;
        self->block_count = 0;
        self->block_size = 0;
        mp_raise_OSError(MP_EIO);
    }

    SD_GetInfo(&self->block_count, &self->block_size);
    self->initialized = true;

    return mp_const_none;
}

static mp_obj_t machine_sdcard_ioctl(mp_obj_t self_in, mp_obj_t operation, mp_obj_t args)
{
    uint32_t err;

    mp_int_t cmd = mp_obj_get_int(operation);
    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);

    switch (cmd) {
    case MP_BLOCKDEV_IOCTL_INIT:
        break;
    case MP_BLOCKDEV_IOCTL_DEINIT:
        err = SD_Deinit();
        self->initialized = false;
        self->block_count = 0;
        self->block_size = 0;
        if (err) {
            return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
        }
        else {
            return MP_OBJ_NEW_SMALL_INT(0);
        }
    case MP_BLOCKDEV_IOCTL_SYNC:
        err = SD_WaitTrans(10000);
        if (err) {
            return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
        }
        else {
            return MP_OBJ_NEW_SMALL_INT(0);
        }
    case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
        return MP_OBJ_NEW_SMALL_INT(self->block_count);
    case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
        return MP_OBJ_NEW_SMALL_INT(self->block_size);
    default:
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }

    return mp_const_none;
}

/**
 * @brief   构造函数 sd = machine.SDCard() 的 C 接口
 * @param   type   正在构造的类型，通常指向 &machine_sdcard_type
 * @param   n_args 位置参数的数量
 * @param   n_kw   关键字参数的数量
 * @param   args   函数收到的全部参数。前 n_args 项是位置参数，后面是关键字参数，长度为 n_args + 2 * n_kw
 * @return  板子只有一个 SD 卡槽，因此永远返回同一个对象 s_machine_sdcard
 */
static mp_obj_t machine_sdcard_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    (void)type;
    (void)args;

    /* Minimum positional arguments: 0
     * Maximum positional parameters: 0
     * Keyword parameters are not allowed */
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    if (s_machine_sdcard.initialized == false) {
        s_machine_sdcard.base.type = &machine_sdcard_type;
        s_machine_sdcard.block_count = 0;
        s_machine_sdcard.block_size = 0;
    }

    return MP_OBJ_FROM_PTR(&s_machine_sdcard);
}

static mp_obj_t machine_sdcard_present(mp_obj_t self_in)
{
    bool present;

    (void)self_in;

    SD_IsPresent(&present);
    if (present) {
        return mp_const_true;
    }
    else {
        return mp_const_false;
    }
}

/**
 * @brief   machine.SDCard() 的读取函数 C 接口
 * @note    MicroPython 的 VFS 已确保扇区对齐，故不再处理扇区不对齐时的读取，不对齐时将直接返回错误
 * @param   self_in   sd 对象，即 Python 中的 self
 * @param   block_num 要读取的块起始地址
 * @param   buffer    接收块对象，包含长度和接收地址信息
 * @retval  0          成功
 * @retval  -MP_EIO    buffer 不可写入或 SD Card 操作失败
 * @retval  -MP_EINVAL 非对齐的扇区访问
 */
static mp_obj_t machine_sdcard_readblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buffer)
{
    uint32_t err;
    uint32_t block_counts;
    mp_buffer_info_t buffer_info;

    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);

    mp_get_buffer_raise(buffer, &buffer_info, MP_BUFFER_WRITE);
    if (self->initialized == false) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
    }
    if ((buffer_info.len % self->block_size) != 0) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }

    block_counts = buffer_info.len / self->block_size;
    err = SD_ReadBlock(buffer_info.buf, mp_obj_get_uint(block_num), block_counts, 30000);
    if (err) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
    }

    return MP_OBJ_NEW_SMALL_INT(0);
}

/**
 * @brief   machine.SDCard() 的写入函数 C 接口
 * @note    MicroPython 的 VFS 已确保扇区对齐，故不再处理扇区不对齐时的写入，不对齐时将直接返回错误
 * @param   self_in   sd 对象，即 Python 中的 self
 * @param   block_num 要写入的块起始地址
 * @param   buffer    接收块对象，包含长度和接收地址信息
 * @retval  0          成功
 * @retval  -MP_EIO    buffer 不可写入或 SD Card 操作失败
 * @retval  -MP_EINVAL 非对齐的扇区访问
 */
static mp_obj_t machine_sdcard_writeblocks(mp_obj_t self_in, mp_obj_t block_num, mp_obj_t buffer)
{
    uint32_t err;
    uint32_t block_counts;
    mp_buffer_info_t buffer_info;

    machine_sdcard_obj_t *self = MP_OBJ_TO_PTR(self_in);

    mp_get_buffer_raise(buffer, &buffer_info, MP_BUFFER_READ);
    if (self->initialized == false) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
    }

    if ((buffer_info.len % self->block_size) != 0) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EINVAL);
    }

    block_counts = buffer_info.len / self->block_size;
    err = SD_WriteBlock(buffer_info.buf, mp_obj_get_uint(block_num), block_counts, 30000);
    if (err) {
        return MP_OBJ_NEW_SMALL_INT(-MP_EIO);
    }

    return MP_OBJ_NEW_SMALL_INT(0);
}

static MP_DEFINE_CONST_FUN_OBJ_1(machine_sdcard_deinit_obj, machine_sdcard_deinit);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_sdcard_info_obj, machine_sdcard_info);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_sdcard_init_obj, machine_sdcard_init);
static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_ioctl_obj, machine_sdcard_ioctl);
static MP_DEFINE_CONST_FUN_OBJ_1(machine_sdcard_present_obj, machine_sdcard_present);
static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_readblocks_obj, machine_sdcard_readblocks);
static MP_DEFINE_CONST_FUN_OBJ_3(machine_sdcard_writeblocks_obj, machine_sdcard_writeblocks);

static const mp_rom_map_elem_t machine_sdcard_local_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_sdcard_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&machine_sdcard_info_obj)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_sdcard_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&machine_sdcard_ioctl_obj)},
    {MP_ROM_QSTR(MP_QSTR_present), MP_ROM_PTR(&machine_sdcard_present_obj)},
    {MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&machine_sdcard_readblocks_obj)},
    {MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&machine_sdcard_writeblocks_obj)}
};

static MP_DEFINE_CONST_DICT(machine_sdcard_local_dict, machine_sdcard_local_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    machine_sdcard_type,
    MP_QSTR_SDCard,
    MP_TYPE_FLAG_NONE,
    make_new, machine_sdcard_make_new,
    locals_dict, &machine_sdcard_local_dict
);
