# CPKCOR-RA8P1 board configuration for MicroPython

# MCU identification
CMSIS_MCU = RA8P1
MCU_SERIES = m85

# FSP project inputs used by the standalone MicroPython Makefile build.
FSP_PROJECT_DIR = boards/CPKCOR_RA8P1/e2studio_gcc_freertos

# Keep linker-generator inputs in the board directory.  The e2 studio script
# and Debug directories are ignored build products and are not standalone
# Makefile inputs.
LD_SCRIPT = $(FSP_PROJECT_DIR)/script/fsp.ld

# MicroPython features
MICROPY_VFS_FAT = 1
MICROPY_ROM_TEXT_COMPRESSION = 1
