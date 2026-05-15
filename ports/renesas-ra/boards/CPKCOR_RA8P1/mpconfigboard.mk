CMSIS_MCU = RA8P1
MCU_SERIES = m85
LD_FILES = boards/CPKCOR_RA8P1/e2studio_gcc_freertos/Debug/fsp_gen.ld

# MicroPython settings
MICROPY_VFS_FAT = 1

CFLAGS += -DDEFAULT_DBG_CH=0

# Using e2 studio generated FSP
HAL_DIR = ports/renesas-ra/boards/CPKCOR_RA8P1/e2studio_gcc_freertos/fsp/ra
CMSIS_DIR = ports/renesas-ra/boards/CPKCOR_RA8P1/e2studio_gcc_freertos/ra/arm/CMSIS_6/CMSIS
STARTUP_FILE = ports/renesas-ra/boards/CPKCOR_RA8P1/e2studio_gcc_freertos/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o
SYSTEM_FILE = ports/renesas-ra/boards/CPKCOR_RA8P1/e2studio_gcc_freertos/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o
