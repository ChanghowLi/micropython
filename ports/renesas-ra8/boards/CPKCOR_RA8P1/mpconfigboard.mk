# CPKCOR-RA8P1 board configuration for MicroPython

# MCU identification
CMSIS_MCU = RA8P1
MCU_SERIES = m85

# FSP project inputs used by the standalone MicroPython Makefile build.
FSP_PROJECT_DIR = boards/CPKCOR_RA8P1/e2studio_gcc_freertos
RA_CFG_DIR = $(FSP_PROJECT_DIR)/ra_cfg
RA_GEN_DIR = $(FSP_PROJECT_DIR)/ra_gen
BOARD_SRC_DIR = $(FSP_PROJECT_DIR)/src
FSP_DIR = $(FSP_PROJECT_DIR)/ra/fsp
FREERTOS_DIR = $(FSP_PROJECT_DIR)/ra/aws/FreeRTOS/FreeRTOS
CMSIS_DIR = $(FSP_PROJECT_DIR)/ra/arm/CMSIS_6/CMSIS
FSP_BOARD_DIR = $(FSP_PROJECT_DIR)/ra/board/ra8p1_cpkcor

# Keep linker-generator inputs in the board directory.  The e2 studio script
# and Debug directories are ignored build products and are not standalone
# Makefile inputs.
LD_SCRIPT_DIR = boards/CPKCOR_RA8P1/$(FSP_PROJECT_DIR)/Debug
LD_SCRIPT = $(LD_SCRIPT_DIR)/fsp.ld
FSP_LD_GENERATED_DIR = $(LD_SCRIPT_DIR)
BSP_LINKER_INFO_DIR = $(FSP_LD_GENERATED_DIR)

# MicroPython features
MICROPY_VFS_FAT = 0
