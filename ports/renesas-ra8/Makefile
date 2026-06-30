# MicroPython RA8P1 minimal port Makefile
#
# This port compiles MicroPython core together with Renesas FSP (Flexible
# Software Package) source files from an e2 studio project.
#
# Usage:
#   make BOARD=CPKCOR_RA8P1 -j$(nproc)    # build for CPKCOR-RA8P1
#   make BOARD=EK_RA8P1 -j$(nproc)         # build for EK-RA8P1
#
# The FSP project inputs are selected by boards/<BOARD>/mpconfigboard.mk.

# ---- Toolchain ----
# Match e2 studio: ARM GCC 13.2
# E2S_GCC can be set via:
#   1. Command line:  make BOARD=... E2S_GCC=/path/to/gcc
#   2. Environment:   export E2S_GCC=/path/to/gcc
#   3. local.mk:      create this file and set E2S_GCC ?= /path/to/gcc
-include local.mk

ifeq ($(E2S_GCC),)
$(error E2S_GCC is not set. Set it via command line, environment, \
        or create 'local.mk' with: E2S_GCC ?= /path/to/arm-none-eabi)
endif

CROSS_COMPILE = $(E2S_GCC)/bin/arm-none-eabi-

include ../../py/mkenv.mk

ifeq ($(OS),Windows_NT)
# Avoid the non-executable Microsoft Store python3 alias on Windows.
PYTHON = python
endif

# ---- Board selection ----
BOARD ?= CPKCOR_RA8P1
BUILD = build-$(BOARD)

# Include board-specific build settings
include boards/$(BOARD)/mpconfigboard.mk

# qstr definitions
QSTR_DEFS = qstrdefsport.h

# MicroPython feature config
MICROPY_ROM_TEXT_COMPRESSION ?= 1

# Include MicroPython core build definitions
include $(TOP)/py/py.mk

# ---- FSP paths ----
# Board mpconfigboard.mk files may override these for a different FSP layout.
FSP_PROJECT_DIR ?= $(E2STUDIO_DIR)
RA_CFG_DIR ?= $(FSP_PROJECT_DIR)/ra_cfg
RA_GEN_DIR ?= $(FSP_PROJECT_DIR)/ra_gen
BOARD_SRC_DIR ?= $(FSP_PROJECT_DIR)/src
FSP_DIR ?= $(FSP_PROJECT_DIR)/ra/fsp
FREERTOS_DIR ?= $(FSP_PROJECT_DIR)/ra/aws/FreeRTOS/FreeRTOS
CMSIS_DIR ?= $(FSP_PROJECT_DIR)/ra/arm/CMSIS_6/CMSIS
FSP_BOARD_DIR ?= $(FSP_PROJECT_DIR)/ra/board/ra8p1_cpkcor
LD_SCRIPT ?= $(FSP_PROJECT_DIR)/script/fsp.ld
LD_SCRIPT_DIR ?= $(dir $(LD_SCRIPT))
FSP_LD_GENERATED_DIR ?= $(FSP_PROJECT_DIR)/Debug
BSP_LINKER_INFO_DIR ?= $(FSP_LD_GENERATED_DIR)

# ---- Compiler flags ----
# Match e2 studio settings exactly: cortex-m85+nopacbti
# (nopacbti disables Pointer Authentication/Branch Target Identification)
CFLAGS_CORTEX_M85 = -mthumb -mfloat-abi=hard -mcpu=cortex-m85+nopacbti

CFLAGS += $(CFLAGS_CORTEX_M85)
CFLAGS += -fsigned-char -fno-strict-aliasing
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -flax-vector-conversions --param=min-pagesize=0
CFLAGS += -D_RENESAS_RA_ -D_RA_CORE=CPU0 -D_RA_ORDINAL=1

# Warning flags: stricter for MicroPython code, relaxed for FSP
CFLAGS += -Wall -Werror -Wdouble-promotion -Wfloat-conversion
CFLAGS += -Wno-stringop-overflow -Wno-format-truncation

# Optimization: match e2 studio (-O2)
CFLAGS += -O2 -DNDEBUG
CFLAGS += -g  # always include debug info

# ---- Include paths ----
INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -Iboards/$(BOARD)

# e2 studio includes (in order from the e2 studio build)
INC += -I$(RA_GEN_DIR)
INC += -I$(RA_CFG_DIR)/fsp_cfg/bsp
INC += -I$(RA_CFG_DIR)/fsp_cfg
INC += -I$(RA_CFG_DIR)/aws
INC += -I$(BOARD_SRC_DIR)
INC += -I$(BOARD_SRC_DIR)/utils
INC += -I$(FSP_DIR)/inc
INC += -I$(FSP_DIR)/inc/api
INC += -I$(FSP_DIR)/inc/instances
INC += -I$(FSP_DIR)/src/rm_freertos_port
INC += -I$(FSP_DIR)/src/bsp/cmsis/Device/RENESAS/Include
INC += -I$(FSP_DIR)/src/bsp/mcu/all
INC += -I$(FSP_DIR)/src/bsp/mcu/ra8p1
INC += -I$(FREERTOS_DIR)/Source/include
INC += -I$(CMSIS_DIR)/Core/Include
INC += -I$(FSP_BOARD_DIR)
# FSP's bsp_linker.c includes the linker-generator output bsp_linker_info.h.
INC += -I$(LD_SCRIPT_DIR)

CFLAGS += $(INC)

# ---- Linker flags ----
# Match e2studio: NO -nostdlib (crt0.o initializes C runtime properly)
# CPU flags MUST be passed to linker for correct multilib selection
LDFLAGS += $(CFLAGS_CORTEX_M85)
LDFLAGS += -T $(LD_SCRIPT)
LD_SEARCH_DIRS = $(sort $(FSP_LD_GENERATED_DIR) $(LD_SCRIPT_DIR))
LDFLAGS += $(addprefix -L,$(LD_SEARCH_DIRS))
LDFLAGS += -Wl,-Map=$@.map -Wl,--cref -Wl,--gc-sections
LDFLAGS += --specs=nano.specs

# Libraries in --start-group/--end-group (same as e2studio)
LIBS = -Wl,--start-group -lgcc -lm -lc -Wl,--end-group

# ---- Source files ----

# -- MicroPython integration sources --
SRC_C = \
    $(BOARD_SRC_DIR)/repl_thread_entry.c \
    $(BOARD_SRC_DIR)/mphalport.c \
    shared/libc/printf.c \
    shared/libc/string0.c \
    shared/readline/readline.c \
    shared/runtime/pyexec.c \

# -- Generated e2 studio sources (ra_gen/) --
SRC_C += \
    $(RA_GEN_DIR)/common_data.c \
    $(RA_GEN_DIR)/hal_data.c \
    $(RA_GEN_DIR)/main.c \
    $(RA_GEN_DIR)/pin_data.c \
    $(RA_GEN_DIR)/repl_thread.c \
    $(RA_GEN_DIR)/vector_data.c \

# -- FSP BSP sources (startup, system, clocks, etc.) --
SRC_C += \
    $(FSP_DIR)/src/bsp/cmsis/Device/RENESAS/Source/startup.c \
    $(FSP_DIR)/src/bsp/cmsis/Device/RENESAS/Source/system.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_clocks.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_common.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_delay.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_group_irq.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_guard.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_io.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_irq.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_register_protection.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_sbrk.c \
    $(FSP_DIR)/src/bsp/mcu/all/bsp_security.c \
    $(FSP_DIR)/src/bsp/mcu/ra8p1/bsp_linker.c \

# -- FSP peripheral drivers --
SRC_C += \
    $(FSP_DIR)/src/r_ioport/r_ioport.c \
    $(FSP_DIR)/src/r_sci_b_uart/r_sci_b_uart.c \

# -- FreeRTOS kernel --
SRC_C += \
    $(FREERTOS_DIR)/Source/event_groups.c \
    $(FREERTOS_DIR)/Source/list.c \
    $(FREERTOS_DIR)/Source/queue.c \
    $(FREERTOS_DIR)/Source/stream_buffer.c \
    $(FREERTOS_DIR)/Source/tasks.c \
    $(FREERTOS_DIR)/Source/timers.c \

# -- FreeRTOS port (Cortex-M85) --
SRC_C += \
    $(FSP_DIR)/src/rm_freertos_port/port.c \

# -- User sources from e2 studio (console, warmstart, fault handler, etc.) --
SRC_C += \
    $(BOARD_SRC_DIR)/console.c \
    $(BOARD_SRC_DIR)/hal_warmstart.c \
    $(BOARD_SRC_DIR)/cm85_fault_handle.c \
    $(BOARD_SRC_DIR)/SEGGER_RTT/SEGGER_RTT.c \
    $(BOARD_SRC_DIR)/SEGGER_RTT/SEGGER_RTT_printf.c \
    $(BOARD_SRC_DIR)/utils/log.c \

# -- Board-specific sources --
SRC_C += \
    $(FSP_BOARD_DIR)/board_init.c \
    $(FSP_BOARD_DIR)/board_leds.c \

# -- Memory manager (heap_4 for static+dynamic allocation) --
SRC_C += \
    $(FREERTOS_DIR)/Source/portable/MemMang/heap_4.c \

# QSTR sources (for string interning)
SRC_QSTR += shared/readline/readline.c shared/runtime/pyexec.c

# ---- Object files ----
# Preserve the source path below BUILD.  Flattening with notdir makes
# same-named sources share an object and lets an unrelated implicit rule pick
# the input file (notably the two repl_thread_entry.c files).
PORT_OBJ = $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ = $(PY_CORE_O)
OBJ += $(PORT_OBJ)

# Keep third-party FreeRTOS sources on their existing relaxed warning policy.
FREERTOS_SRC_C = $(filter $(FREERTOS_DIR)/% $(FSP_DIR)/src/rm_freertos_port/%,$(SRC_C))
FREERTOS_OBJ = $(addprefix $(BUILD)/,$(FREERTOS_SRC_C:.c=.o))
$(FREERTOS_OBJ): CFLAGS += -w

# ---- Targets ----

all: $(BUILD)/firmware.elf $(BUILD)/firmware.hex $(BUILD)/firmware.bin

# Frozen module (for testing)
$(BUILD)/_frozen_mpy.c: $(TOP)/tests/assets/frozentest.mpy $(BUILD)/genhdr/qstrdefs.generated.h
	$(ECHO) "MISC freezing bytecode"
	$(Q)$(TOP)/tools/mpy-tool.py -f -q $(BUILD)/genhdr/qstrdefs.preprocessed.h -mlongint-impl=none $< > $@

# Link — use GCC instead of LD so that libgcc/libc paths are resolved
LINKER_DEPS = $(LD_SCRIPT) \
    $(FSP_LD_GENERATED_DIR)/memory_regions.ld \
    $(FSP_LD_GENERATED_DIR)/fsp_gen.ld

$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
	$(Q)$(SIZE) $@

# Binary and hex outputs
$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary $^ $@

$(BUILD)/firmware.hex: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O ihex $^ $@

hex: $(BUILD)/firmware.hex

# ---- Pre-build target for e2 studio integration ----
# Generates all auto-generated headers without compiling/linking.
# Usage: make BOARD=CPKCOR_RA8P1 genhdr
GENHDR_DEPS  = $(BUILD)/genhdr/mpversion.h
GENHDR_DEPS += $(BUILD)/genhdr/qstrdefs.generated.h
GENHDR_DEPS += $(BUILD)/genhdr/moduledefs.h
GENHDR_DEPS += $(BUILD)/genhdr/root_pointers.h
GENHDR_DEPS += $(BUILD)/genhdr/compressed.data.h

.PHONY: genhdr
genhdr: $(GENHDR_DEPS)

# ---- Debug target ----
.PHONY: info
info:
	@echo "============================================"
	@echo "  RA8P1 MicroPython Build Configuration"
	@echo "============================================"
	@echo ""
	@echo "Toolchain:"
	@echo "  CC           = $(CC)"
	@echo "  LD           = $(LD)"
	@echo "  CROSS_COMPILE= $(CROSS_COMPILE)"
	@echo "  GCC version  = $(shell $(CC) --version | head -1)"
	@echo ""
	@echo "Paths:"
	@echo "  TOP          = $(TOP)"
	@echo "  BUILD        = $(BUILD)"
	@echo "  FSP_PROJECT_DIR = $(FSP_PROJECT_DIR)"
	@echo "  RA_CFG_DIR      = $(RA_CFG_DIR)"
	@echo "  RA_GEN_DIR      = $(RA_GEN_DIR)"
	@echo "  BOARD_SRC_DIR   = $(BOARD_SRC_DIR)"
	@echo "  FSP_DIR         = $(FSP_DIR)"
	@echo ""
	@echo "Multi-arch:"
	@echo "  MULTI_DIR    = $(MULTI_DIR)"
	@echo "  GCC_MULTI    = $(GCC_MULTI)"
	@echo "  NEWLIB_MULTI = $(NEWLIB_MULTI)"
	@echo ""
	@echo "CFLAGS:"
	@echo "  $(CFLAGS)" | tr ' ' '\n' | sed 's/^/  /'
	@echo ""
	@echo "LDFLAGS:"
	@echo "  $(LDFLAGS)" | tr ' ' '\n' | sed 's/^/  /'
	@echo ""
	@echo "LIBS:"
	@echo "  $(LIBS)"
	@echo ""
	@echo "INC paths:"
	@echo "  $(INC)" | tr ' ' '\n' | sed 's/^-I/  /'
	@echo ""
	@echo "============================================"
	@echo "  Source files (SRC_C):"
	@echo "============================================"
	@for f in $(SRC_C); do echo "  $$f"; done
	@echo ""
	@echo "============================================"
	@echo "  QSTR sources (SRC_QSTR):"
	@echo "============================================"
	@for f in $(SRC_QSTR); do echo "  $$f"; done

# Include MicroPython standard rules
include $(TOP)/py/mkrules.mk
