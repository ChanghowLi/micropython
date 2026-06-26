// MicroPython REPL FreeRTOS task entry point
//
// Called from ra_gen/repl_thread.c:repl_thread_func() after
// FreeRTOS common initialization.
//
// This replaces the template src/repl_thread_entry.c from e2 studio.

#include "console.h"
#include "repl_thread.h"

#include "SEGGER_RTT/SEGGER_RTT.h"
#include "utils/log.h"

// MicroPython headers
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "shared/runtime/pyexec.h"

// GC heap — allocated statically in .bss
// RA8P1 has ~1.8MB SRAM at 0x22000000; use 256KB for MicroPython GC
#define MICROPY_HEAP_SIZE (256 * 1024)
static char heap[MICROPY_HEAP_SIZE] __attribute__((aligned(8)));

// Stack top pointer for GC collection — set at task entry
static char *stack_top;

// --- MicroPython dependencies ---

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
    return NULL;
}

mp_import_stat_t mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    (void)val;
    while (1) {}
}

void __attribute__((noreturn)) __fatal_error(const char *msg) {
    (void)msg;
    while (1) {}
}

#if MICROPY_ENABLE_GC

void gc_collect(void) {
    // WARNING: This implementation does not scan CPU registers for root
    // pointers. This may miss some live objects. For production use,
    // shared/runtime/gchelper_thumb2.s should be compiled for M85.
    void *dummy;
    gc_collect_start();
    gc_collect_root(&dummy,
        ((mp_uint_t)stack_top - (mp_uint_t)&dummy) / sizeof(mp_uint_t));
    gc_collect_end();
}

#endif // MICROPY_ENABLE_GC

// --- FreeRTOS task entry ---

void repl_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    // Init SEGGER RTT for debug logging (not used as console)
    #if CONSOLE_CFG_USE_RTT == 0
    SEGGER_RTT_Init();
    #endif

    // Init UART console (opens SCI9, starts RX interrupts)
    CONSOLE_Init();

    LOG_I("MicroPython", "Starting MicroPython on CPKCOR-RA8P1...");

    // Init MicroPython
    #if MICROPY_ENABLE_GC
    gc_init(heap, heap + sizeof(heap));
    #endif

    mp_init();

    #if MICROPY_ENABLE_COMPILER
    // Start the friendly REPL
    pyexec_friendly_repl();
    #else
    // If compiler is disabled, run frozen module
    pyexec_frozen_module("frozentest.py", false);
    #endif

    mp_deinit();

    LOG_E("MicroPython", "REPL exited unexpectedly");

    // If REPL exits, loop forever
    while (1) {
        vTaskDelay(1000);
    }
}

// --- Timestamp for log component ---

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
