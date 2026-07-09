/**
 * @file    mpconfigport.h
 * @brief   MicroPython port renesas-ra8 配置文件，针对板子的配置，写在 boards/<BoardName>/mpconfigboard.h
 * @note    不要定义 MICROPY_MIN_USE_CORTEX_CPU
 */
#include <alloca.h>
#include <stdint.h>

#include "mpconfigboard.h"

#ifndef MICROPY_ALLOC_PATH_MAX
#define MICROPY_ALLOC_PATH_MAX          256
#endif

#ifndef MICROPY_ALLOC_PARSE_CHUNK_INIT
#define MICROPY_ALLOC_PARSE_CHUNK_INIT  16
#endif

#ifndef MICROPY_CONFIG_ROM_LEVEL
#define MICROPY_CONFIG_ROM_LEVEL        MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES
#endif

#ifndef MICROPY_ENABLE_COMPILER
#define MICROPY_ENABLE_COMPILER         1
#endif

#ifndef MICROPY_ENABLE_EXTERNAL_IMPORT
#define MICROPY_ENABLE_EXTERNAL_IMPORT  1
#endif

#ifndef MICROPY_ENABLE_GC
#define MICROPY_ENABLE_GC               1
#endif

#ifndef MICROPY_HEAP_SIZE
#define MICROPY_HEAP_SIZE               (256 * 1024)
#endif

#ifndef MICROPY_HELPER_REPL
#define MICROPY_HELPER_REPL             1
#endif

#ifndef MICROPY_MODULE_FROZEN_MPY
#define MICROPY_MODULE_FROZEN_MPY       0
#endif

#ifndef MICROPY_PY_SYS_ARGV
#define MICROPY_PY_SYS_ARGV             0
#endif

#ifndef MICROPY_PY_SYS_EXIT
#define MICROPY_PY_SYS_EXIT             0
#endif

#ifndef MICROPY_PY_SYS_MODULES
#define MICROPY_PY_SYS_MODULES          0
#endif

#ifndef MICROPY_PY_TIME
#define MICROPY_PY_TIME                 1
#endif

#ifndef MICROPY_PY_SYS_PATH
#define MICROPY_PY_SYS_PATH             0
#endif

#ifndef MP_SSIZE_MAX
#define MP_SSIZE_MAX                    0x7fffffff
#endif

#ifndef MP_STATE_PORT
#define MP_STATE_PORT                   MP_STATE_VM
#endif

typedef long mp_off_t;
