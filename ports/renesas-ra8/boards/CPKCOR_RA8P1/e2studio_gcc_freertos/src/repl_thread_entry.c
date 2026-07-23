#include <stdio.h>

#include "console.h"
#include "repl_thread.h"
#include "perf_counter/perf_counter.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "utils/log.h"

#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "shared/runtime/gchelper.h"
#include "shared/runtime/pyexec.h"
#include "py/cstack.h"

#define TAG __FUNCTION__

static char s_head[MICROPY_HEAP_SIZE];

void repl_thread_entry(void *pvParameters)
{
    int ret;

    FSP_PARAMETER_NOT_USED(pvParameters);

    perfc_init(false);
#if CONSOLE_CFG_USE_RTT == 0
    SEGGER_RTT_Init();
#endif
    CONSOLE_Init();

    LOG_I("MicroPython", "Starting MicroPython on CPKCOR-RA8P1...");

soft_reset:
    /* 必须小于 FreeRTOS 分配的栈大小，当前：0x4000 */
    mp_cstack_init_with_sp_here(0x3000);

#if MICROPY_ENABLE_GC
    gc_init(s_head, s_head + sizeof(s_head));
#endif
    mp_init();

#if MICROPY_ENABLE_COMPILER
    while (1) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            ret = pyexec_raw_repl();
            switch (ret) {
            case 0:
                LOG_I(TAG, "Raw REPL normal exit, change to Friendly REPL");
                break;
            case PYEXEC_FORCED_EXIT:
                LOG_W(TAG, "Raw REPL forced exit");
                break;
            default:
                LOG_W(TAG, "Raw REPL unexpect exit, code: 0x%X", ret);
                break;
            }
        }
        else {
            ret = pyexec_friendly_repl();
            switch (ret) {
            case 0:
                LOG_I(TAG, "Friendly REPL normal exit, change to Raw REPL");
                break;
            case PYEXEC_FORCED_EXIT:
                LOG_W(TAG, "Friendly REPL forced exit");
                break;
            default:
                LOG_W(TAG, "Friendly REPL unexpect exit, code: 0x%X", ret);
                break;
            }
        }
        
        if (ret != 0) {
            break;
        }
    }
#else
    pyexec_frozen_module("frozentest.py", false);
#endif

    mp_printf(&mp_plat_print, "MPY: soft reboot\n");
    mp_deinit();
    LOG_W(TAG, "Soft reset");
    goto soft_reset;
}

#if LOG_CFG_EN_TIMESTAMP
void LOG_GetTime(uint32_t *s, uint32_t *ms)
{
    TickType_t t;

    if (__get_IPSR() == 0) {
        t = xTaskGetTickCount();
    } else {
        t = xTaskGetTickCountFromISR();
    }
    *s  = (uint32_t)(t / 1000);
    *ms = (uint32_t)(t % 1000);
}
#endif

/* MicroPython dependencies */
#if 0
mp_lexer_t *mp_lexer_new_from_file(qstr filename)
{
    mp_raise_OSError(MP_ENOENT);

    return NULL;
}

mp_import_stat_t mp_import_stat(const char *path)
{
    return MP_IMPORT_STAT_NO_EXIST;
}
#endif

#if 0
mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) 
{
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);
#endif

void nlr_jump_fail(void *val)
{
    LOG_E(TAG, "NLR jump failed, val: 0x%p", val);

#if MICROPY_STACK_CHECK
    volatile int sp_dummy;
    LOG_E(TAG, "C stack: top=0x%p SP=0x%p usage=%u limit=%u", MP_STATE_THREAD(stack_top), &sp_dummy, mp_cstack_usage(), MP_STATE_THREAD(stack_limit));
#endif

    if (val != NULL) {
        mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(val));
    }

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    LOG_E(TAG, "Task: %s, stack free: %lu words", pcTaskGetName(task), uxTaskGetStackHighWaterMark(task));

    while (1) {}
}

void __attribute__((noreturn)) __fatal_error(const char *msg)
{
    LOG_E(TAG, msg);

    while (1) {}
}


#if MICROPY_ENABLE_GC
void gc_collect(void) 
{
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}
#endif
