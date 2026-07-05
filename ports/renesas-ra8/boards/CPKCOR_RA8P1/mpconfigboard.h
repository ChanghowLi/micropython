/**
 * @file    mpconfigport.h
 * @brief   MicroPython port renesas-ra8 配置文件
 * @note    不要定义 MICROPY_MIN_USE_CORTEX_CPU
 */

#define MICROPY_HW_BOARD_NAME       "CPKCOR-RA8P1"
#define MICROPY_HW_MCU_NAME         "RA8P1"
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)

#define MICROPY_PY_THREAD           (0)
