/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 CPKCOR
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// This file is never compiled standalone, it's included directly from
// extmod/modmachine.c via MICROPY_PY_MACHINE_INCLUDEFILE.

#include <stdint.h>
#include "hal_data.h"
#include "py/runtime.h"
#include "py/objarray.h"
#include "modmachine.h"
#include "pin.h"

#if MICROPY_HW_ENABLE_RNG
#include "mphalport.h"
#endif

#define MACHINE_BACKUP_REGION0_WORDS (32)
#define MACHINE_RESET_DEEPSLEEP      (4)
#define MACHINE_RESET_HARD           (2)
#define MACHINE_RESET_POWER_ON       (1)
#define MACHINE_RESET_SOFT           (0)
#define MACHINE_RESET_WDT            (3)
#define MACHINE_WAKE_PIN             (1)
#define MACHINE_WAKE_RTC             (2)
#define MACHINE_WAKE_UNKNOWN         (0)

#if MICROPY_HW_ENABLE_RNG
#define MICROPY_PY_MACHINE_RNG_ENTRY \
    { MP_ROM_QSTR(MP_QSTR_rng), MP_ROM_PTR(&machine_rng_obj) },
#else
#define MICROPY_PY_MACHINE_RNG_ENTRY
#endif

/** RA8 machine 模块提供的额外全局对象。 */
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS \
    { MP_ROM_QSTR(MP_QSTR_Pin),              MP_ROM_PTR(&machine_pin_type) }, \
    { MP_ROM_QSTR(MP_QSTR_mem_backup),       MP_ROM_PTR(&machine_mem_backup_obj) }, \
    MICROPY_PY_MACHINE_RNG_ENTRY \
    /** 唤醒原因。 */ \
    { MP_ROM_QSTR(MP_QSTR_PIN_WAKE),         MP_ROM_INT(MACHINE_WAKE_PIN) }, \
    { MP_ROM_QSTR(MP_QSTR_RTC_WAKE),         MP_ROM_INT(MACHINE_WAKE_RTC) }, \
    { MP_ROM_QSTR(MP_QSTR_WAKE_UNKNOWN),     MP_ROM_INT(MACHINE_WAKE_UNKNOWN) }, \
    { MP_ROM_QSTR(MP_QSTR_wake_reason),      MP_ROM_PTR(&machine_wake_reason_obj) }, \
    /** 复位原因。 */ \
    { MP_ROM_QSTR(MP_QSTR_PWRON_RESET),      MP_ROM_INT(MACHINE_RESET_POWER_ON) }, \
    { MP_ROM_QSTR(MP_QSTR_HARD_RESET),       MP_ROM_INT(MACHINE_RESET_HARD) }, \
    { MP_ROM_QSTR(MP_QSTR_WDT_RESET),        MP_ROM_INT(MACHINE_RESET_WDT) }, \
    { MP_ROM_QSTR(MP_QSTR_DEEPSLEEP_RESET),  MP_ROM_INT(MACHINE_RESET_DEEPSLEEP) }, \
    { MP_ROM_QSTR(MP_QSTR_SOFT_RESET),       MP_ROM_INT(MACHINE_RESET_SOFT) },


static uint32_t machine_mem_backup_region0[MACHINE_BACKUP_REGION0_WORDS];
static mp_int_t machine_reset_cause_value = MACHINE_RESET_POWER_ON;
static mp_int_t machine_wake_reason_value = MACHINE_WAKE_UNKNOWN;

machine_reset_flags_t machine_reset_flags __attribute__((section(".noinit")));

static bool machine_first_init = true;

static mp_obj_t mp_machine_get_freq(void)
{
    return mp_obj_new_int(SystemCoreClock);
}

static void mp_machine_set_freq(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    (void)args;
    mp_raise_NotImplementedError(MP_ERROR_TEXT("machine.freq set not supported"));
}

/**
 * @brief 获取 MCU 的唯一标识符。
 *
 * @return 包含 MCU 唯一标识符的 Python bytes 对象。
 */
static mp_obj_t mp_machine_unique_id(void)
{
    const bsp_unique_id_t *unique_id = R_BSP_UniqueIdGet();

    return mp_obj_new_bytes(
        unique_id->unique_id_bytes,
        sizeof(unique_id->unique_id_bytes));
}

#if MICROPY_HW_ENABLE_RNG

/**
 * @brief 获取一个 24 位硬件随机数。
 *
 * @return 取值范围为 0 到 0xFFFFFF 的 Python int 对象。
 */
static mp_obj_t machine_rng(void)
{
    uint8_t random[3];
    uint32_t value;

    mp_hal_get_random(sizeof(random), random);

    value = (uint32_t)random[0]
        | ((uint32_t)random[1] << 8)
        | ((uint32_t)random[2] << 16);

    return MP_OBJ_NEW_SMALL_INT(value);
}

static MP_DEFINE_CONST_FUN_OBJ_0(machine_rng_obj, machine_rng);

#endif

static mp_obj_t machine_mem_backup_region0_view(void) 
{
    return mp_obj_new_memoryview(
        MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'I',
        MACHINE_BACKUP_REGION0_WORDS,
        machine_mem_backup_region0
    );
}

static mp_obj_t machine_mem_backup(size_t n_args, const mp_obj_t *args)
{
    mp_int_t region = 0;

    if (n_args == 1) 
    {
        region = mp_obj_get_int(args[0]);
    }

    if (region == 0) 
    {
        return machine_mem_backup_region0_view();
    }

    if (region == -1)
    {
        mp_obj_t regions[] = 
        {
            machine_mem_backup_region0_view(),
        };
        return mp_obj_new_tuple(MP_ARRAY_SIZE(regions), regions);
    }

    mp_raise_ValueError(MP_ERROR_TEXT("invalid backup memory region"));
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_mem_backup_obj, 0, 1, machine_mem_backup);

static mp_int_t machine_decode_reset_cause(void) 
{
    uint32_t rstsr0 = machine_reset_flags.rstsr0;
    uint32_t rstsr1 = machine_reset_flags.rstsr1;
    uint32_t rstsr2 = machine_reset_flags.rstsr2;

    if (!(rstsr2 & R_SYSTEM_RSTSR2_CWSF_Msk)) 
    {
        return MACHINE_RESET_POWER_ON;
    }

    if (rstsr0 & R_SYSTEM_RSTSR0_DPSRSTF_Msk) 
    {
        return MACHINE_RESET_DEEPSLEEP;
    }

    if (rstsr1 & (
        R_SYSTEM_RSTSR1_IWDTRF_Msk |
        R_SYSTEM_RSTSR1_WDTRF_Msk |
        R_SYSTEM_RSTSR1_WDT1RF_Msk
        )) {
        return MACHINE_RESET_WDT;
    }

    if (rstsr1 & R_SYSTEM_RSTSR1_SWRF_Msk) 
    {
        return MACHINE_RESET_HARD;
    }

    if (rstsr0 & R_SYSTEM_RSTSR0_PORF_Msk) 
    {
        return MACHINE_RESET_POWER_ON;
    }

    return MACHINE_RESET_HARD;
}

void machine_init(void) 
{
    if (machine_first_init) {
        machine_reset_cause_value = machine_decode_reset_cause();
        machine_first_init = false;
    }
}

void machine_deinit(void)
{
    machine_reset_cause_value = MACHINE_RESET_SOFT;
    machine_wake_reason_value = MACHINE_WAKE_UNKNOWN;
}

MP_NORETURN static void mp_machine_reset(void)
{
    __disable_irq();
    __DSB();
    NVIC_SystemReset();

    for (;;) {
    }
}

static mp_int_t mp_machine_reset_cause(void)
{
    return machine_reset_cause_value;
}

static mp_obj_t machine_wake_reason(void)
{
    return MP_OBJ_NEW_SMALL_INT(machine_wake_reason_value);
}

static MP_DEFINE_CONST_FUN_OBJ_0(
    machine_wake_reason_obj, machine_wake_reason);

static void mp_machine_idle(void)
{
    __WFI();
}

static void mp_machine_lightsleep(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    (void)args;
    mp_raise_NotImplementedError(MP_ERROR_TEXT("machine.lightsleep not supported"));
}

MP_NORETURN static void mp_machine_deepsleep(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    (void)args;
    mp_raise_NotImplementedError(MP_ERROR_TEXT("machine.deepsleep not supported"));
}
