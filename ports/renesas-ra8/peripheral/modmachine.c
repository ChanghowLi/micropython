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

#include "py/runtime.h"
#include "py/objarray.h"
#include "hal_data.h"
#include "modmachine.h"

#define MACHINE_BACKUP_REGION0_WORDS (32)
#define MACHINE_RESET_SOFT           (0)
#define MACHINE_RESET_POWER_ON       (1)
#define MACHINE_RESET_HARD           (2)
#define MACHINE_RESET_WDT            (3)
#define MACHINE_RESET_DEEPSLEEP      (4)

// Extra entries provided by the RA8 machine module.
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS \
    { MP_ROM_QSTR(MP_QSTR_mem_backup),       MP_ROM_PTR(&machine_mem_backup_obj) }, \
    /* Reset causes. */ \
    { MP_ROM_QSTR(MP_QSTR_PWRON_RESET),      MP_ROM_INT(MACHINE_RESET_POWER_ON) }, \
    { MP_ROM_QSTR(MP_QSTR_HARD_RESET),       MP_ROM_INT(MACHINE_RESET_HARD) }, \
    { MP_ROM_QSTR(MP_QSTR_WDT_RESET),        MP_ROM_INT(MACHINE_RESET_WDT) }, \
    { MP_ROM_QSTR(MP_QSTR_DEEPSLEEP_RESET),  MP_ROM_INT(MACHINE_RESET_DEEPSLEEP) }, \
    { MP_ROM_QSTR(MP_QSTR_SOFT_RESET),       MP_ROM_INT(MACHINE_RESET_SOFT) },

// Module state.
static uint32_t machine_mem_backup_region0[MACHINE_BACKUP_REGION0_WORDS];
static mp_int_t machine_reset_cause_value = MACHINE_RESET_POWER_ON;

machine_reset_flags_t machine_reset_flags __attribute__((section(".noinit")));

static bool machine_first_init = true;

// Create a writable memoryview for backup memory region 0.
static mp_obj_t machine_mem_backup_region0_view(void) 
{
    return mp_obj_new_memoryview(
        MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'I',
        MACHINE_BACKUP_REGION0_WORDS,
        machine_mem_backup_region0
    );
}

// Return a backup memory region.
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

// Decode the hardware reset flags.
static mp_int_t machine_decode_reset_cause(void) 
{
    uint32_t rstsr0 = machine_reset_flags.rstsr0;
    uint32_t rstsr1 = machine_reset_flags.rstsr1;
    uint32_t rstsr2 = machine_reset_flags.rstsr2;


    // A cleared CWSF indicates a cold start.
    if (!(rstsr2 & R_SYSTEM_RSTSR2_CWSF_Msk)) 
    {
        return MACHINE_RESET_POWER_ON;
    }

    // Deep-standby cancellation reset.
    if (rstsr0 & R_SYSTEM_RSTSR0_DPSRSTF_Msk) 
    {
        return MACHINE_RESET_DEEPSLEEP;
    }

    // Watchdog reset.
    if (rstsr1 & (
        R_SYSTEM_RSTSR1_IWDTRF_Msk |
        R_SYSTEM_RSTSR1_WDTRF_Msk |
        R_SYSTEM_RSTSR1_WDT1RF_Msk
        )) {
        return MACHINE_RESET_WDT;
    }

    // Software-triggered system reset.
    if (rstsr1 & R_SYSTEM_RSTSR1_SWRF_Msk) 
    {
        return MACHINE_RESET_HARD;
    }

    // PORF indicates a power-on reset.
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

void machine_deinit(void) {
    machine_reset_cause_value = MACHINE_RESET_SOFT;
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

static void mp_machine_idle(void)
{
    __WFI();
}
