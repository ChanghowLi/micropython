CMSIS_MCU = RA8P1
MCU_SERIES = m85
LD_FILES = boards/CPKCOR_RA8P1/e2studio_gcc_freertos/Debug/fsp_gen.ld

# MicroPython settings
MICROPY_VFS_FAT = 1

CFLAGS+=-DDEFAULT_DBG_CH=0
