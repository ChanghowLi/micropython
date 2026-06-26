# CPKCOR-RA8P1 MicroPython 移植规划

## 芯片背景

RA8P1 是 Renesas RA8 系列的 **Cortex-M85 双核** MCU（FSP 中用 `R7KA8P1KF_core0.h` / `core1.h` 区分核）。当前 reneses-ra 端口仅支持 Cortex-M4（RA4/RA6M1/RA6M2）和 Cortex-M33（RA6M5），需要新增 M85 架构和 RA8P1 芯片支持。

---

## 里程碑 1：串口 REPL

**目标**：在 CPKCOR-RA8P1 板子上通过串口看到 MicroPython REPL 提示符（`>>>`）。

**明确不做的内容**：

- 外部 SDRAM — 不用，只用内部 SRAM
- 外部 QSPI Flash — 不用，只用内部 Flash 做文件系统
- ADC / DAC — 暂时禁用
- SD 卡 — 不用
- 蓝牙 / CAN / USB — 暂时禁用

**最简外设清单**：

| 外设            | 用途                        | FSP 驱动             |
| ------------- | ------------------------- | ------------------ |
| 系统时钟          | CPU + 外设时钟源               | BSP (bsp_clocks.c) |
| IOPORT (GPIO) | 引脚控制、LED、按键               | r_ioport           |
| SCI UART ×1   | REPL 串口                   | r_sci_uart         |
| 内部 Flash      | MicroPython 固件存放 + 内部文件系统 | r_flash_hp（待确认）    |
| NVIC / EXTI   | 中断控制、SysTick              | BSP 自带             |

---

## 核心策略：FSP 本地化，不升级全局 lib/fsp

现有 `lib/fsp` 子模块版本为 v4.4.0，不支持 RA8 系列。直接升级会带来 breaking change 风险，影响现有板子。

**替代方案**：用 e2 studio 生成的文件，全部放在 `boards/CPKCOR-RA8P1/e2studio_gcc_freertos/` 目录中。Makefile 通过 `mpconfigboard.mk` 传入的变量来判断是否为 CPKCOR-RA8P1 构建，如果是，则将 `HAL_DIR`、`CMSIS_DIR`、`STARTUP_FILE`、`SYSTEM_FILE` 重定向到板子目录下的 FSP 副本，**完全旁路 `lib/fsp`**。

```
构建 EK_RA6M2：                  构建 CPKCOR_RA8P1：
    lib/fsp  (v4.4.0)                lib/fsp  (v4.4.0, 不使用)
    lib/cmsis/inc (现有)             boards/CPKCOR_RA8P1/e2studio_gcc_freertos/  (最新 FSP)
```

**Makefile 改动量**：4 处 `=` → `?=`（第 52-55 行），1 处 submodule 检查适配。现有板子行为不变。

---

## 里程碑 1 步骤总览

```
[1] 在 boards/EK_RA8P1/fsp/ 部署 FSP 文件子集
        │
[2] Makefile: FSP 路径变量改为 ?=，mpconfigboard.mk 覆盖
        │
[3] Makefile: 添加 M85 编译参数 + RA8P1 过滤条件
        │
[4] 修改 ra_config.h，添加 RA8P1 外设基数和 PCLK
        │
[5] 修改 ra_sci.c，添加 RA8P1 的 SCI 引脚映射
        │
[6] 审查 ra/ 最小外设集，标记/禁用不需要的模块
        │
[7] 创建 ra8p1_af.csv（GPIO 复用功能表）
        │
[8] e² studio 生成 FSP 配置（时钟 + GPIO + 1 路 SCI UART + 内部 Flash）
        │
[9] 创建 mpconfigboard.mk / mpconfigboard.h（最简配置）
        │
[10] 创建 pins.csv + board.json
        │
[11] 创建链接脚本（仅内部 SRAM + 内部 Flash）
        │
[12] 处理 M85 特殊功能（双核、TrustZone、gchelper、Emit Thumb）
        │
[13] 首次 make BOARD=EK_RA8P1 → 修复编译错误
        │
[14] 烧录验证：看到 REPL 提示符
```

---

# 详细步骤

## [1] 复制工程

复制 e2 studio 工程

- [x] 已完成

---

## [2] Makefile: FSP 路径重定向

2a. 修改主 Makefile 第 52-55 行（`ports/renesas-ra/Makefile`）

```makefile
# 修改前：
CMSIS_DIR = lib/cmsis/inc
HAL_DIR = lib/fsp
STARTUP_FILE ?= lib/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o
SYSTEM_FILE ?= lib/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o

# 修改后：
CMSIS_DIR ?= lib/cmsis/inc
HAL_DIR ?= lib/fsp
STARTUP_FILE ?= lib/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o
SYSTEM_FILE ?= lib/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o
```

- [x] 已完成

2b. 修改 git submodule 自动拉取逻辑（第 410-415 行）

```makefile
# 修改前：
all: $(TOP)/lib/fsp/README.md $(BUILD)/firmware.hex $(BUILD)/firmware.bin

$(TOP)/lib/fsp/README.md:
    $(ECHO) "fsp submodule not found, fetching it now..."
    (cd $(TOP) && git submodule update --init lib/fsp)

# 修改后：
FSP_README := $(TOP)/$(HAL_DIR)/README.md

all: $(FSP_README) $(BUILD)/firmware.hex $(BUILD)/firmware.bin

$(FSP_README):
    $(ECHO) "fsp submodule not found, fetching it now..."
    (cd $(TOP) && git submodule update --init lib/fsp)
```

这样需要 CPKCOR_RA8P1 在相应路径添加 README.md 占位文件，避免触发 `git submodule update`。

- [ ] 已完成

2c. mpconfigboard.mk 中覆盖路径

见步骤 [9]。

- [ ] 已完成

---

## [3] Makefile: M85 编译参数 + RA8P1 过滤条件

修改 `ports/renesas-ra/Makefile`。

3a. Cortex-M85 FPU 配置（约第 96-103 行新增）

```makefile
ifeq ($(MCU_SERIES),$(filter $(MCU_SERIES),m85))
CFLAGS_CORTEX_M += -mfpu=fpv5-d16 -mfloat-abi=hard
SUPPORTS_HARDWARE_FP_SINGLE = 1
endif
```

**注意**：M85 FPU 具体参数需确认。候选值有 `fpv5-sp-d16`、`fpv5-d16`、`fpv5-sp`。以 `arm-none-eabi-gcc --target-help` 中 cortex-m85 对应的默认值为准。

- [x] 已完成

> 但是否正确仍需后续验证

3b. MCU 编译器参数（约第 106-110 行新增）

```makefile
CFLAGS_MCU_RA8P1 = $(CFLAGS_CORTEX_M) -mtune=cortex-m85 -mcpu=cortex-m85
```

- [x] 已完成

> 但是变量 `CFLAGS_MCU_RA8P1` 在其它地方没有使用，这很奇怪

3c. CMSIS_MCU 过滤条件——添加 RA8P1

三处需要修改：

**(a) `INC += -Ira` 条件**（第 74-77 行）：

```makefile
ifeq ($(CMSIS_MCU),$(filter $(CMSIS_MCU),RA4M1 RA4W1 RA6M1 RA6M2 RA6M5 RA8P1))
INC += -Ira
endif
```

- [x] 已完成

**(b) `ra/*.c` 包含条件**（第 320-337 行）：

```makefile
ifeq ($(CMSIS_MCU),$(filter $(CMSIS_MCU),RA4M1 RA4W1 RA6M1 RA6M2 RA6M5 RA8P1))
HAL_SRC_C += $(addprefix ra/,\
    ra_adc.c \
    ra_dac.c \
    ra_flash.c \
    ra_gpio.c \
    ra_i2c.c \
    ra_icu.c \
    ra_init.c \
    ra_int.c \
    ra_rtc.c \
    ra_sci.c \
    ra_spi.c \
    ra_timer.c \
    ra_gpt.c \
    ra_utils.c \
    )
endif
```

里程碑 1 阶段所有 `ra_*.c` 都会被编译，但 `mpconfigboard.h` 中会 `#define MICROPY_HW_ENABLE_ADC (0)` 等方式禁用不需要的外设。这样可以减少 Makefile 改动。

- [x] 已完成

**(c) Flash 驱动选择**（第 307-318 行）：

```makefile
ifeq ($(CMSIS_MCU),$(filter $(CMSIS_MCU),RA6M1 RA6M2 RA6M5 RA8P1))
ifeq ($(USE_FSP_FLASH), 1)
CFLAGS += -DUSE_FSP_FLASH
HAL_SRC_C += $(HAL_DIR)/ra/fsp/src/r_flash_hp/r_flash_hp.c
endif
endif
```

- [x] 已完成

> 这是内部 Flash 读写相关，在 OTA 时肯定需要用，现在就包含进来应该没问题

3d. AF 文件映射（第 448-462 行新增）

```makefile
ifeq ($(BOARD),"CPKCOR_RA8P1")
AF_FILE = boards/cpkcor_ra8p1.csv
endif
```

RA8P1 后续可能有其它板子，所以这里根据 BOARD 来指定 AF 文件

- [x] 已完成

3e. gchelper_thumb2 条件（第 405-409 行）

现有：

```makefile
ifeq ($(CMSIS_MCU),RA6M5)
$(BUILD)/shared/runtime/gchelper_thumb2.o: $(TOP)/shared/runtime/gchelper_thumb2.s
    $(ECHO) "AS $<"
    $(Q)$(AS) $(ASFLAGS) -o $@ $<
endif
```

RA8P1 (M85) 与 RA6M5 (M33) 同为 ARMv8.1-M，可能需要同样的显式汇编规则。将条件扩展为 `RA6M5 RA8P1`，或在编译后根据错误决定是否添加。重点是现在不知道这是干什么用的。

- [x] 已完成

---

## [4] 修改 ra_config.h——添加 RA8P1 外设基数和 PCLK

修改 `ports/renesas-ra/ra/ra_config.h`：

```c
#elif defined(RA8P1)
#define SCI_CH_MAX   10    // 从数据手册确认 SCI 通道数
#define SCI_CH_NUM   4     // 同时可用的 SCI 通道数
#define SCI_TX_BUF_SIZE 128
#define SCI_RX_BUF_SIZE 256
#define PCLK         ???   // 外设时钟频率，由 mpconfigboard.h 的 MICROPY_HW_MCU_PCLK 覆盖
```

- [ ] 已完成

---

## [5] 修改 ra_sci.c——添加 RA8P1 的 SCI 引脚映射

修改 `ports/renesas-ra/ra/ra_sci.c`，在三个引脚映射数组中添加 `#elif defined(RA8P1)` 分支：

- `ra_sci_tx_pins[]` — TX 引脚
- `ra_sci_rx_pins[]` — RX 引脚
- `ra_sci_cts_pins[]` — CTS 引脚

格式示例：

```c
#elif defined(RA8P1)
{ AF_SCI1, 0, P411 },
{ AF_SCI2, 1, P709 },
...
```

**就里程碑 1 而言，只需要确保 REPL 所用的那个 SCI 通道的 TX/RX 引脚有定义即可**。其余 SCI 通道可以暂不填写，等后续里程碑再补。

数据来源：RA8P1 数据手册 Chapter "I/O Ports" → "Pin Function Control" 表格。

- [ ] 已完成

---

## [6] 审查 ra/ 目录——禁用不需要的外设

里程碑 1 只需要：GPIO、SCI UART、中断管理、基础初始化。其余外设在 `mpconfigboard.h` 中全部禁用：

| ra_*.c 文件    | 里程碑 1 状态 | 说明                                  |
| ------------ | -------- | ----------------------------------- |
| `ra_gpio.c`  | **需要**   | GPIO 操作，LED 控制                      |
| `ra_sci.c`   | **需要**   | 串口 REPL                             |
| `ra_int.c`   | **需要**   | 中断管理                                |
| `ra_icu.c`   | **需要**   | 中断控制器                               |
| `ra_init.c`  | **需要**   | ra_init() / ra_deinit()             |
| `ra_utils.c` | **需要**   | 工具函数(延时、时钟控制等)                      |
| `ra_flash.c` | **需要**   | 内部 Flash 存储                         |
| `ra_timer.c` | **可能**   | SysTick 替代？需确认依赖                    |
| `ra_adc.c`   | **禁用**   | `#define MICROPY_HW_ENABLE_ADC (0)` |
| `ra_dac.c`   | **禁用**   | `#define MICROPY_HW_ENABLE_DAC (0)` |
| `ra_rtc.c`   | **禁用**   | `#define MICROPY_HW_ENABLE_RTC (0)` |
| `ra_spi.c`   | **禁用**   | 里程碑 1 无 SPI                         |
| `ra_i2c.c`   | **禁用**   | 里程碑 1 无 I2C                         |
| `ra_gpt.c`   | **禁用**   | 里程碑 1 无 GPT 定时器                     |

禁用方式：在 `mpconfigboard.h` 中不定义 `MICROPY_HW_ENABLE_xxx`（或显式定义为 0），`machine_xxx.c` 中的 `#if MICROPY_HW_ENABLE_XXX` 检查会让这些模块不注册。

如果禁用后某些 `ra_*.c` 编译报错（有未定义的外部符号），说明底层有硬依赖，需要保留编译但初始化留空。

- [ ] 已完成

> 这些东西在考虑要不要直接在 e2studio 中处理

---

## [7] 创建 cpkcor_ra8p1.csv（GPIO 复用功能表）

创建 `ports/renesas-ra/boards/ra8p1_af.csv`：

```
CPU_PIN,PORT_IDX,PORT_BIT,Analog,IRQ,AF0,AF1,...AF22,,,
P000,0,0,AN000,IRQ6-DS,,,,.....
P001,0,1,AN001,IRQ7-DS,,,,.....
...
```

格式说明：

- `CPU_PIN`：`P` + 端口号(0-F) + 位号(00-15)，如 `P001`、`P411`
- `PORT_IDX`：端口号
- `PORT_BIT`：位号
- `Analog`：如果有模拟功能（ANxxx），填功能名
- `IRQ`：如果有外部中断，填 `IRQn` 或 `IRQn-DS`
- `AF0` ~ `AF22`：对应 23 个复用功能，如 `AGTIO0`、`RXD0/MISO0/SCL0` 等

数据来源：**RA8P1 数据手册** Pin Function Control 表格。

RA8P1 的 AF 编号系统可能与 RA6 系列不同，需要确认。`make-pins.py` 中 `add_af(af_idx, af_name, af)` 的 af_idx 是从 0 开始的列号（第 3 列 Analog 是 AF0，第 4 列 IRQ 是系统功能而非 AF，第 5 列开始是 AF0~AF22 的真正复用功能）。

- [ ] 已完成

---

## [8] 创建 mpconfigboard.mk 和 mpconfigboard.h（最简配置）

mpconfigboard.mk

```makefile
CMSIS_MCU = RA8P1
MCU_SERIES = m85
LD_FILES = boards/EK_RA8P1/ra8p1_ek.ld

# MicroPython settings
MICROPY_VFS_FAT = 1

# ---- 使用本地 FSP，旁路 lib/fsp ----
# 注意：这些路径以 $(TOP) (MicroPython 仓库根) 为基准
HAL_DIR = ports/renesas-ra/boards/EK_RA8P1/fsp
CMSIS_DIR = ports/renesas-ra/boards/EK_RA8P1/fsp/cmsis
STARTUP_FILE = ports/renesas-ra/boards/EK_RA8P1/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.o
SYSTEM_FILE = ports/renesas-ra/boards/EK_RA8P1/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.o
```

- [x] 已完成

mpconfigboard.h（里程碑 1 精简版）

```c
// MCU 基础信息
#define MICROPY_HW_BOARD_NAME       "CPKCOR-RA8P1"
#define MICROPY_HW_MCU_NAME         "RA8P1"
#define MICROPY_HW_MCU_SYSCLK       ???     // 系统时钟 (Hz)，从数据手册/时钟配置获取
#define MICROPY_HW_MCU_PCLK         ???     // 外设时钟 (Hz)，从时钟树分频获取

// Python 特性
#define MICROPY_EMIT_THUMB          (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)
#define MICROPY_PY_BUILTINS_COMPLEX (1)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_HEAPQ            (1)
#define MICROPY_PY_THREAD           (0)     // 里程碑 1 禁用多线程

// 内部 Flash 存储
#define MICROPY_HW_HAS_FLASH                    (1)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)

// ---- 禁用所有非必要外设（里程碑 1） ----
#define MICROPY_HW_ENABLE_RTC       (0)
#define MICROPY_HW_ENABLE_ADC       (0)
#define MICROPY_HW_ENABLE_DAC       (0)

// ---- REPL UART（对照原理图） ----
#define MICROPY_HW_UART_REPL        HW_UART_0       // 选择第一个 SCI 通道
#define MICROPY_HW_UART_REPL_BAUD   115200
#define MICROPY_HW_UART0_TX         (pin_P???)      // 对照原理图填写
#define MICROPY_HW_UART0_RX         (pin_P???)      // 对照原理图填写

// ---- LED（对照原理图） ----
#define MICROPY_HW_LED1             (pin_P???)
#define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
#define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
#define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)

// ---- 用户按键（对照原理图） ----
#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_USRSW_PIN        (pin_P???)
#define MICROPY_HW_USRSW_PULL       (MP_HAL_PIN_PULL_NONE)
#define MICROPY_HW_USRSW_EXTI_MODE  (MP_HAL_PIN_TRIGGER_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)
```

---

## [9] 创建 pins.csv + board.json

pins.csv

列出 CPKCOR-RA8P1 上所有引出的 GPIO：

```
P000,P000
P001,P001
...
USBDP,P???
USBDM,P???
LED1,P???
SW1,P???
```

数据来源：**CPKCOR-RA8P1 原理图** + RA8P1 数据手册。需要对照板子上的排针和功能确定哪些引脚是真正引出的。

- [ ] 已完成

board.json

```json
{
    "deploy": ["../deploy.md"],
    "docs": "",
    "features": [],
    "images": [],
    "mcu": "ra8p1",
    "product": "EK-RA8P1",
    "vendor": "Renesas Electronics"
}
```

- [ ] 已完成

---

## [10] 创建链接脚本——仅内部存储器

链接脚本可以直接用 e2studio 生成的

- [ ] 已完成

---

## [11] 处理 M85 特殊功能

11a. gchelper_thumb2.s

RA6M5 (M33) 需要显式汇编 `gchelper_thumb2.s`，M85 也是 ARMv8.1-M，大概率同样需要。在步骤 [13] 编译时如果报 `gchelper_thumb2.o` 相关链接错误，就在 Makefile 第 389 行条件中加入 `RA8P1`。

11b. MICROPY_EMIT_THUMB / INLINE_THUMB

确认 Cortex-M85 的 Thumb-2 指令集与 M4/M33 的兼容性。M85 是 ARMv8.1-M，向后兼容 ARMv7-M/ARMv8-M 的 Thumb 指令，应该可以直接用。但如果出现 native/viper emit 相关的崩溃，需要排查。

11c. MPY_CROSS_FLAGS

```makefile
MPY_CROSS_FLAGS += -march=armv7m
```

M85 是 ARMv8.1-M，`-march=armv8.1-m.main` 更准确。但这个只影响 `.mpy` 跨文件兼容性，里程碑 1 可以不改。

---

## [12] 首次编译及错误修复

```bash
cd ports/renesas-ra
make BOARD=CPKCOR_RA8P1 -j$(nproc)
```

预期会遇到的错误及应对：

| 预期错误                                            | 排查方向                                                          |
| ----------------------------------------------- | ------------------------------------------------------------- |
| `fatal error: R7KA8P1KF_core0.h: No such file`  | 检查 `fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Include/` 下是否有此文件 |
| `fatal error: core_cm85.h: No such file`        | 检查 CMSIS_DIR 路径和 cmsis/inc/ 目录                                |
| `fatal error: bsp_mcu_info.h: No such file`     | 检查 `fsp/ra/fsp/src/bsp/mcu/ra8p1/` 是否存在                       |
| `error: target CPU does not support ARM mode`   | 确保有 `-mthumb`                                                 |
| `error: unknown FPU variant 'fpv5-d16'`         | 调整 `-mfpu` 参数                                                 |
| `undefined reference to VECTOR_NUMBER_SCI0_RXI` | 检查 `ra_gen/vector_data.h` 是否定义了 SCI 0 的中断向量                   |
| `undefined reference to gchelper_thumb2`        | 将 Makefile 第 389 行条件扩展至 RA8P1（见 12c）                          |
| 链接器报 section 溢出                                 | Flash/SRAM 大小设置不对，调整链接脚本                                      |
| `#error "CMSIS MCU Series is not specified"`    | 检查 `ra_config.h` 是否添加了 RA8P1 分支                               |

**编译成功的判别标准**：`build-EK_RA8P1/firmware.elf` + `firmware.hex` 生成，无链接错误。

---

## [13] 烧录验证：看到 REPL 提示符

烧录

```bash
# 通过 J-Link / SEGGER 工具链烧录
JLinkExe -device R7KA8P1KF -if SWD -speed 4000 -autoconnect 1 \
    -CommanderScript flash.jlink
# 或使用 Renesas Flash Programmer
```

验证清单

- [ ] **LED 闪烁**：如果 FSP 配置和 MicroPython 初始化正确，`main.c` 执行后 LED 应该工作

- [ ] **串口有输出**：`minicom -D /dev/ttyUSB0 -b 115200` 或 `screen /dev/ttyUSB0 115200`

- [ ] **看到启动信息**：
  
  ```
  MicroPython v1.xx on 202x-xx-xx; EK-RA8P1 with RA8P1
  >>>
  ```

- [ ] **能输入 Python**：
  
  ```python
  >>> print("hello")
  hello
  >>> 1 + 1
  2
  ```

如果没看到 REPL

1. **检查串口连接**：RX/TX 引脚是否与原理图一致
2. **检查波特率**：FSP 配置中的波特率是否与 `mpconfigboard.h` 一致
3. **检查时钟**：`MICROPY_HW_MCU_SYSCLK` 和 `PCLK` 是否与实际 FSP 配置匹配
4. **检查 startup**：`startup.c` 是否正确配置了 VTOR 和中断向量表
5. **检查 `SystemCoreClock`**：这个变量由 `system.c` 初始化，必须与 FSP 时钟树一致，否则 `SysTick_Config` 会产生错误的中断间隔
6. **调试器断点**：在 `main()` 函数入口设断点，确认代码是否执行到了这一步

---

依赖关系

```
[1] 部署本地 FSP ──→ [2] Makefile ?= 重定向
  │                      │
  │                      ├──→ [3] Makefile M85 支持 + RA8P1 过滤
  │                      ├──→ [4] ra_config.h
  │                      ├──→ [5] ra_sci.c
  │                      ├──→ [6] 审查 ra/
  │                      └──→ [7] ra8p1_af.csv
  │
  └──→ [8] e² studio 生成 FSP 配置
         │
         ├──→ [9] mpconfigboard.mk / .h
         ├──→ [10] pins.csv / board.json
         ├──→ [11] 链接脚本
         └──→ [12] M85 特殊功能

                                    [13] 首次编译
                                        │
                                    [14] 烧录验证
```

**关键路径**：[1] → [8] → [13] → [14]。FSP 配置文件 [8] 是整个里程碑能否成功的最关键环节。

---

## 里程碑 1 完成标准

- [x] MicroPython 在 EK-RA8P1 上启动
- [x] REPL 可以通过串口正常交互
- [x] LED 能控制
- [x] GPIO 基本操作正常
- [x] 内部 Flash 文件系统可用（`import os; os.listdir()` 返回文件列表）

## 后续里程碑（不在此文档范围内）

| 里程碑 | 内容                            |
| --- | ----------------------------- |
| M2  | 外部 SDRAM（增大堆空间）、外部 QSPI Flash |
| M3  | ADC/DAC、RTC、SPI/I2C 等完整外设     |
| M4  | USB CDC（替代串口 REPL）、SD 卡       |
