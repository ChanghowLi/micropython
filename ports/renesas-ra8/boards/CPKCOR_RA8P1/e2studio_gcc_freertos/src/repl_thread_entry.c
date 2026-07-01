#include "console.h"
#include "repl_thread.h"
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

#define MICROPY_HEAP_SIZE (256 * 1024) /* MicroPython GC s_head size */

static char s_head[MICROPY_HEAP_SIZE];
static char *s_stack_top;

void repl_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    int stack_dummy;
    s_stack_top = (char *)&stack_dummy;

#if CONSOLE_CFG_USE_RTT == 0
    SEGGER_RTT_Init();
#endif
    CONSOLE_Init();

    LOG_I("MicroPython", "Starting MicroPython on CPKCOR-RA8P1...");

    /* Init MicroPython */
#if MICROPY_ENABLE_GC
    gc_init(s_head, s_head + sizeof(s_head));
#endif
    mp_init();
#if MICROPY_ENABLE_COMPILER
    pyexec_friendly_repl();
#else
    pyexec_frozen_module("frozentest.py", false);
#endif
    mp_deinit();

    LOG_E("MicroPython", "REPL exited unexpectedly");
    while (1) {
        R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);
        vTaskDelay(100);
        R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW);
        vTaskDelay(100);
    }
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
mp_lexer_t *mp_lexer_new_from_file(qstr filename)
{
    mp_raise_OSError(MP_ENOENT);

    return NULL;
}

mp_import_stat_t mp_import_stat(const char *path)
{
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val)
{
    LOG_E(__FUNCTION__, "*val: 0x%p", val);

    while (1) {}
}

void __attribute__((noreturn)) __fatal_error(const char *msg)
{
    LOG_E(__FUNCTION__, msg);

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
