/**
 * @file    mpconfigport.h
 * @brief   MicroPython port renesas-ra8 配置文件，针对板子的配置，写在 boards/<BoardName>/mpconfigboard.h
 * @note    不要定义 MICROPY_MIN_USE_CORTEX_CPU
 */
#include <alloca.h>
#include <stdint.h>

#include "mpconfigboard.h"

#ifndef MICROPY_ALLOC_PARSE_CHUNK_INIT
#define MICROPY_ALLOC_PARSE_CHUNK_INIT              16
#endif

#ifndef MICROPY_ALLOC_PATH_MAX
#define MICROPY_ALLOC_PATH_MAX                      256
#endif

#ifndef MICROPY_CONFIG_ROM_LEVEL
#define MICROPY_CONFIG_ROM_LEVEL                    MICROPY_CONFIG_ROM_LEVEL_EVERYTHING
#endif

/* py/test says its arch=armv7emdp, it has problem in test */
#ifndef MICROPY_EMIT_INLINE_THUMB
#define MICROPY_EMIT_INLINE_THUMB                   0
#endif

/* py/test will lost of failed when it set to 1, perhaps MP has not yet supported ARMv8.1 */
#ifndef MICROPY_EMIT_THUMB
#define MICROPY_EMIT_THUMB                          0
#endif

#ifndef MICROPY_ENABLE_COMPILER
#define MICROPY_ENABLE_COMPILER                     1
#endif

#ifndef MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE
#define MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE        256
#endif

#ifndef MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF
#define MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF      1
#endif

#ifndef MICROPY_ENABLE_EXTERNAL_IMPORT
#define MICROPY_ENABLE_EXTERNAL_IMPORT              1
#endif

#ifndef MICROPY_ENABLE_GC
#define MICROPY_ENABLE_GC                           1
#endif

#ifndef MICROPY_FATFS_ENABLE_LFN
#define MICROPY_FATFS_ENABLE_LFN                    2
#endif

#ifndef MICROPY_FATFS_EXFAT
#define MICROPY_FATFS_EXFAT                         1
#endif

#ifndef MICROPY_FATFS_LFN_CODE_PAGE
#define MICROPY_FATFS_LFN_CODE_PAGE                 437
#endif

#ifndef MICROPY_FATFS_NORTC
#define MICROPY_FATFS_NORTC                         1
#endif

#ifndef MICROPY_FATFS_RPATH
#define MICROPY_FATFS_RPATH                         2
#endif

#ifndef MICROPY_HEAP_SIZE
#define MICROPY_HEAP_SIZE                           (256 * 1024)
#endif

#ifndef MICROPY_HELPER_REPL
#define MICROPY_HELPER_REPL                         1
#endif

#ifndef MICROPY_MODULE_FROZEN_MPY
#define MICROPY_MODULE_FROZEN_MPY                   0
#endif

#ifndef MICROPY_PERSISTENT_CODE_LOAD
#define MICROPY_PERSISTENT_CODE_LOAD                1
#endif

#ifndef MICROPY_READER_VFS
#define MICROPY_READER_VFS                          1
#endif

#ifndef MICROPY_VFS
#define MICROPY_VFS                                 1
#endif

#ifndef MICROPY_VFS_LFS2
#define MICROPY_VFS_LFS2                            1
#endif

#ifndef MICROPY_VFS_FAT
#define MICROPY_VFS_FAT                             1
#endif

#ifndef MICROPY_PY_BINASCII_CRC32
#define MICROPY_PY_BINASCII_CRC32                   0
#endif

#ifndef MICROPY_PY_BUILTINS_MEMORYVIEW
#define MICROPY_PY_BUILTINS_MEMORYVIEW              1
#endif

#ifndef MICROPY_PY_MACHINE
#define MICROPY_PY_MACHINE                          1
#endif

#ifndef MICROPY_PY_MACHINE_BARE_METAL_FUNCS
#define MICROPY_PY_MACHINE_BARE_METAL_FUNCS         1
#endif

#ifndef MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ
#define MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ   1
#endif

#ifndef MICROPY_PY_MACHINE_INCLUDEFILE
#define MICROPY_PY_MACHINE_INCLUDEFILE              "ports/renesas-ra8/peripheral/modmachine.c"
#endif

#ifndef MICROPY_PY_MACHINE_MEMX
#define MICROPY_PY_MACHINE_MEMX                     1
#endif

#ifndef MICROPY_PY_MACHINE_RESET
#define MICROPY_PY_MACHINE_RESET                    1
#endif

#ifndef MICROPY_PY_MACHINE_SIGNAL
#define MICROPY_PY_MACHINE_SIGNAL                   0
#endif

#ifndef MICROPY_PY_MACHINE_SPI
#define MICROPY_PY_MACHINE_SPI                      1
#endif

#ifndef MICROPY_PY_MACHINE_SPI_LSB
#define MICROPY_PY_MACHINE_SPI_LSB                  1
#endif

#ifndef MICROPY_PY_MACHINE_SPI_MSB
#define MICROPY_PY_MACHINE_SPI_MSB                  0
#endif

#ifndef MICROPY_PY_MATH_GAMMA_FIX_NEGINF
#define MICROPY_PY_MATH_GAMMA_FIX_NEGINF            1
#endif

#ifndef MICROPY_PY_OS_URANDOM
#if MICROPY_HW_ENABLE_RNG
#define MICROPY_PY_OS_URANDOM				    	1
#else
#define MICROPY_PY_OS_URANDOM				    	0
#endif
#endif

#ifndef MICROPY_PY_OS_UNAME
#define MICROPY_PY_OS_UNAME                         1
#endif

#ifndef MICROPY_PY_SYS_ARGV
#define MICROPY_PY_SYS_ARGV                         1
#endif

#ifndef MICROPY_PY_SYS_MODULES
#define MICROPY_PY_SYS_MODULES                      1
#endif

#ifndef MICROPY_PY_SYS_PLATFORM
#define MICROPY_PY_SYS_PLATFORM                     "renesas-ra8"
#endif

#ifndef MICROPY_PY_SYS_EXIT
#define MICROPY_PY_SYS_EXIT                         1
#endif

#ifndef MICROPY_PY_SYS_PATH
#define MICROPY_PY_SYS_PATH                         1
#endif

#ifndef MICROPY_PY_TIME
#define MICROPY_PY_TIME                             1
#endif

#ifndef MP_SSIZE_MAX
#define MP_SSIZE_MAX                                0x7fffffff
#endif

#ifndef MP_STATE_PORT
#define MP_STATE_PORT                               MP_STATE_VM
#endif

typedef long mp_off_t;
