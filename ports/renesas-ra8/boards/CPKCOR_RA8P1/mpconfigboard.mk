# CPKCOR-RA8P1 board configuration for MicroPython

# MCU identification
CMSIS_MCU = RA8P1
MCU_SERIES = m85

# e2 studio project root (relative to port directory, which is the CWD)
# This points to the generated FSP project
E2STUDIO_DIR = boards/CPKCOR_RA8P1/e2studio_gcc_freertos

# Linker script from e2 studio (relative to port directory)
LD_FILES = $(E2STUDIO_DIR)/Debug/fsp_gen.ld

# MicroPython features
MICROPY_VFS_FAT = 0

# FSP include paths (relative to E2STUDIO_DIR)
# These get expanded in the main Makefile
