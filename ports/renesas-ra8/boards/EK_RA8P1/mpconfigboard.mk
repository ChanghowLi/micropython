# EK-RA8P1 board configuration for MicroPython

# MCU identification
CMSIS_MCU = RA8P1
MCU_SERIES = m85

# e2 studio project root (relative to MicroPython repo root)
# TODO: user must create this e2 studio project
E2STUDIO_DIR = ports/renesas-ra8/boards/EK_RA8P1/e2studio_gcc_freertos

# Linker script from e2 studio
LD_FILES = $(E2STUDIO_DIR)/Debug/fsp_gen.ld

# MicroPython features
MICROPY_VFS_FAT = 0
