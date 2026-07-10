# MicroPython RA8P1 minimal port Makefile
#
# This port compiles MicroPython core together with Renesas FSP (Flexible
# Software Package) source files from an e2 studio project.
#
# Usage:
#   make BOARD=CPKCOR_RA8P1 -j$(nproc)    # build for CPKCOR-RA8P1
#   make BOARD=EK_RA8P1 -j$(nproc)        # build for EK-RA8P1 (not implement yet)
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
$(error E2S_GCC is not set. Set it via command line, environment, or create 'local.mk' with: E2S_GCC ?= /path/to/arm-none-eabi)
endif

CROSS_COMPILE = $(E2S_GCC)/bin/arm-none-eabi-

include ../../py/mkenv.mk

ifeq ($(OS),Windows_NT)
PYTHON = python
endif

# ---- Board selection ----
BOARD ?= CPKCOR_RA8P1
BUILD = build-$(BOARD)

# Include board-specific build settings
include boards/$(BOARD)/mpconfigboard.mk

# qstr definitions
QSTR_DEFS = boards/$(BOARD)/qstrdefsport.h

# Include MicroPython core build definitions
include $(TOP)/py/py.mk

# ---- Source files ----
# FSP source file
FSP_SRC_DIRS := $(FSP_PROJECT_DIR)/ra $(FSP_PROJECT_DIR)/ra_cfg $(FSP_PROJECT_DIR)/ra_gen $(FSP_PROJECT_DIR)/src
ifeq ($(OS),Windows_NT)
__COMMA := ,
__EMPTY :=
__SPACE := $(__EMPTY) $(__EMPTY)
__FSP_SRC_DIRS := $(subst $(__SPACE),$(__COMMA),$(FSP_SRC_DIRS))
FSP_SRC_C := $(shell powershell -NoProfile -Command "Get-ChildItem -Path $(__FSP_SRC_DIRS) -Recurse -Filter *.c -ErrorAction SilentlyContinue | Resolve-Path -Relative | ForEach-Object { \$$_ -replace '^\\.\\\\' -replace '\\\\','/' }")
else
FSP_SRC_C := $(shell find $(FSP_SRC_DIRS) -type f -name '*.c' 2>/dev/null)
endif

SRC_C := $(FSP_SRC_C)

# MicroPython integration sources
SRC_C += extmod/modtime.c
SRC_C += extmod/moductypes.c
SRC_C += shared/libc/printf.c
SRC_C += shared/libc/string0.c
SRC_C += shared/readline/readline.c
SRC_C += shared/runtime/gchelper_generic.c
SRC_C += shared/runtime/pyexec.c
SRC_C += shared/runtime/sys_stdio_mphal.c

# Assembly sources (GC helper for Thumb-2)
SRC_O = shared/runtime/gchelper_thumb2.o

# ---- Include paths ----
INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -Iboards/$(BOARD)

# e2 studio includes (in order from the e2 studio build)
INC += -I$(FSP_PROJECT_DIR)/ra_gen
INC += -I$(FSP_PROJECT_DIR)/ra_cfg/fsp_cfg/bsp
INC += -I$(FSP_PROJECT_DIR)/ra_cfg/fsp_cfg
INC += -I$(FSP_PROJECT_DIR)/ra_cfg/aws
INC += -I$(FSP_PROJECT_DIR)/src
INC += -I$(FSP_PROJECT_DIR)/src/utils
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/inc
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/inc/api
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/inc/instances
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/src/rm_freertos_port
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/src/bsp/cmsis/Device/RENESAS/Include
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/src/bsp/mcu/all
INC += -I$(FSP_PROJECT_DIR)/ra/fsp/src/bsp/mcu/ra8p1
INC += -I$(FSP_PROJECT_DIR)/ra/aws/FreeRTOS/FreeRTOS/Source/include
INC += -I$(FSP_PROJECT_DIR)/ra/arm/CMSIS_6/CMSIS/Core/Include
INC += -I$(FSP_PROJECT_DIR)/ra/board/ra8p1_cpkcor
INC += -I$(FSP_PROJECT_DIR)/Debug

# ---- Compiler flags ----
# Match e2 studio settings exactly: cortex-m85+nopacbti
# (nopacbti disables Pointer Authentication/Branch Target Identification)
CFLAGS_CORTEX_M85 = -mthumb -mfloat-abi=hard -mcpu=cortex-m85+nopacbti

CFLAGS += $(CFLAGS_CORTEX_M85)
CFLAGS += -fdata-sections
CFLAGS += -ffunction-sections
CFLAGS += -flax-vector-conversions
CFLAGS += -fno-strict-aliasing
CFLAGS += -fsigned-char
CFLAGS += --param=min-pagesize=0

# Warning flags: stricter for MicroPython code, relaxed for FSP
CFLAGS += -Wall
CFLAGS += -Wdouble-promotion
CFLAGS += -Wfloat-conversion
# Suppress these warnings
CFLAGS += -Wno-format-truncation
CFLAGS += -Wno-stringop-overflow
# These warnings upgrad to errors.
CFLAGS += -Werror=implicit-function-declaration

# Macro
CFLAGS += -D_RA_CORE=CPU0
CFLAGS += -D_RA_ORDINAL=1
CFLAGS += -D_RENESAS_RA_
CFLAGS += -D__arm__
CFLAGS += -DCONSOLE_RX_BUF_SECTION=\".dtcm_noinit\"

# Optimization
CFLAGS += -O2 -DNDEBUG
CFLAGS += -g

CFLAGS += $(INC)

# ---- Linker flags ----
# Match e2studio: NO -nostdlib (crt0.o initializes C runtime properly)
# CPU flags MUST be passed to linker for correct multilib selection
LDFLAGS += $(CFLAGS_CORTEX_M85)
LDFLAGS += -T $(LD_SCRIPT)
LD_SEARCH_DIRS = $(sort $(FSP_PROJECT_DIR)/Debug $(FSP_PROJECT_DIR)/Debug)
LDFLAGS += $(addprefix -L,$(LD_SEARCH_DIRS))
LDFLAGS += -Wl,-Map=$@.map -Wl,--cref -Wl,--gc-sections
LDFLAGS += --specs=nano.specs

# Libraries in --start-group/--end-group (same as e2studio)
LIBS = -Wl,--start-group -lgcc -lm -lc -Wl,--end-group

# QSTR sources (for string interning)
SRC_QSTR += extmod/modtime.c
SRC_QSTR += extmod/moductypes.c
SRC_QSTR += shared/readline/readline.c
SRC_QSTR += shared/runtime/pyexec.c
SRC_QSTR += shared/runtime/sys_stdio_mphal.c

# ---- Object files ----
# Preserve the source path below BUILD.  Flattening with notdir makes
# same-named sources share an object and lets an unrelated implicit rule pick
# the input file (notably the two repl_thread_entry.c files).
PORT_OBJ = $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ = $(PY_CORE_O)
OBJ += $(PORT_OBJ)
OBJ += $(addprefix $(BUILD)/, $(SRC_O))

# Disable all warnings for third-party libraries
FREERTOS_SRC_C = $(filter $(FSP_PROJECT_DIR)/ra/aws/FreeRTOS/FreeRTOS/% $(FSP_PROJECT_DIR)/ra/fsp/src/rm_freertos_port/%,$(SRC_C))
FREERTOS_OBJ = $(addprefix $(BUILD)/,$(FREERTOS_SRC_C:.c=.o))
$(FREERTOS_OBJ): CFLAGS += -w

# ---- Assembly rules ----
# Use $(CC) (gcc) instead of $(AS) because $(AS) (arm-none-eabi-as) doesn't
# understand the +nopacbti feature modifier in -mcpu=cortex-m85+nopacbti.
$(BUILD)/shared/runtime/gchelper_thumb2.o: $(TOP)/shared/runtime/gchelper_thumb2.s
	$(ECHO) "AS $<"
	$(Q)$(CC) $(CFLAGS_CORTEX_M85) -c -o $@ $<

# ---- Targets ----

all: $(BUILD)/firmware.elf $(BUILD)/firmware.hex $(BUILD)/firmware.bin

# Frozen module (for testing)
$(BUILD)/_frozen_mpy.c: $(TOP)/tests/assets/frozentest.mpy $(BUILD)/genhdr/qstrdefs.generated.h
	$(ECHO) "MISC freezing bytecode"
	$(Q)$(TOP)/tools/mpy-tool.py -f -q $(BUILD)/genhdr/qstrdefs.preprocessed.h -mlongint-impl=none $< > $@

# Link — use GCC instead of LD so that libgcc/libc paths are resolved
LINKER_DEPS = $(LD_SCRIPT) \
    $(FSP_PROJECT_DIR)/Debug/memory_regions.ld \
    $(FSP_PROJECT_DIR)/Debug/fsp_gen.ld

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

# ---- Generate compile_commands.json ----
# Require python package: compiledb
.PHONY: gencdb
gencdb:
	compiledb -n make BOARD=$(BOARD)
	mkdir -p $(BUILD)
	mv compile_commands.json $(BUILD)

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
	@echo "  AS           = $(AS)"
	@echo "  LD           = $(LD)"
	@echo "  CROSS_COMPILE= $(CROSS_COMPILE)"
	@echo "  GCC version  = $(shell $(CC) --version | head -1)"
	@echo ""
	@echo "Paths:"
	@echo "  TOP          = $(TOP)"
	@echo "  BUILD        = $(BUILD)"
	@echo "  FSP_PROJECT_DIR = $(FSP_PROJECT_DIR)"
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
