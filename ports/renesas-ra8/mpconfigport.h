// MicroPython configuration for RA8P1 (Cortex-M85) minimal port
#include <stdint.h>

// Use the minimal starting configuration (disables all optional features)
//#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)
//#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES)

#include <stdint.h>
#include "mpconfigboard.h"
// Enable the built-in MicroPython compiler for REPL
#define MICROPY_ENABLE_COMPILER     (1)

#define MICROPY_ENABLE_GC                 (1)
#define MICROPY_HELPER_REPL               (1)
#define MICROPY_MODULE_FROZEN_MPY         (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT    (1)

#define MICROPY_ALLOC_PATH_MAX            (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT    (16)

// Disable optional sys module features
#define MICROPY_PY_SYS_MODULES            (0)
#define MICROPY_PY_SYS_EXIT               (0)
#define MICROPY_PY_SYS_PATH               (0)
#define MICROPY_PY_SYS_ARGV               (0)

// RA8P1: Cortex-M85, ARMv8.1-M with FPU
// Note: Do NOT define MICROPY_MIN_USE_CORTEX_CPU — FSP provides startup code.
// Note: Do NOT define MICROPY_MIN_USE_STM32_MCU — this is a Renesas MCU.

// Heap size: 256KB for MicroPython GC (RA8P1 has ~1.8MB SRAM at 0x22000000)
#define MICROPY_HEAP_SIZE           (256 * 1024)

// Board identification (overridden by mpconfigboard.h)
#ifndef MICROPY_HW_BOARD_NAME
#define MICROPY_HW_BOARD_NAME "CPKCOR-RA8P1"
#endif
#ifndef MICROPY_HW_MCU_NAME
#define MICROPY_HW_MCU_NAME "RA8P1"
#endif


#define MP_SSIZE_MAX (0x7fffffff)

typedef long mp_off_t;

// alloca: provided by ARM toolchain's <alloca.h>
#include <alloca.h>

#define MP_STATE_PORT MP_STATE_VM
