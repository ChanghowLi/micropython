# Note

在 `ports/renesas-ra8` 目录下创建 `local.mk`，里面写 E2S_GCC 的路径

```makefile
# 根据安装路径填写
E2S_GCC ?= D:/Programs/Dev/e2s_2025_12_BSP_6.4.0/toolchains/gcc_arm/13.2.rel1
```

先用 e2studio 生成工程，然后在 `ports/renesas-ra8` 目录执行

```bash
make BOARD=CPKCOR_RA8P1 genhdr
```

对于 windows，如果没有使用 Microsoft Store 里的 Python，则需要指定 python

```bash
# 如果 shell 中有 python
make BOARD=CPKCOR_RA8P1 genhdr PYTHON=python
```

# RA8P1 MicroPython 最小化移植方案

## 核心思路

**完全绕过 `ports/renesas-ra` 的复杂适配层**，参照 `ports/minimal` 的模式，用 e2 studio 生成的 FSP 代码处理所有硬件初始化，MicroPython 只负责 REPL。

### 为什么这样做？

| 对比项                | 适配 renesas-ra       | 本方案                  |
| ------------------ | ------------------- | -------------------- |
| 需要修改/创建的文件         | 20+ 个               | **6 个**              |
| 需要 ra/ 适配层？        | 是（14+ 个 .c 文件）      | **否**                |
| 需要 AF 引脚映射表？       | 是（CSV 表格）           | **否**（e2 studio 已配置） |
| 需要 machine_*.c 模块？ | 全部编译                | **否**（后续按需添加）        |
| 需要手写链接脚本？          | 是                   | **否**（e2 studio 生成）  |
| 外设初始化              | 手动在 FSP 配置 + ra/*.c | **e2 studio 全自动**    |
| 时钟配置               | 手动填 mpconfigboard.h | **e2 studio 时钟树**    |
| REPL 串口            | 通过 ra_sci.c 多层适配    | **直接用 FSP API**      |

### 关键洞察

`ports/minimal` 的结构极其简单：**MicroPython 核心只需要 4 个硬件函数**：

| 函数                            | 用途             |
| ----------------------------- | -------------- |
| `mp_hal_stdin_rx_chr()`       | 从串口读一个字符       |
| `mp_hal_stdout_tx_strn()`     | 向串口写一串字符       |
| `mp_hal_ticks_ms()`           | 获取毫秒计数器        |
| `mp_hal_set_interrupt_char()` | 设置中断字符（Ctrl-C） |

而 e2 studio 已经生成了：

- ✅ 启动代码（Reset_Handler、中断向量表、data/bss 初始化）
- ✅ 完整的时钟树配置（CPU 1GHz、SCICLK 120MHz、PCLK 125MHz）
- ✅ 引脚复用配置（IOPORT）
- ✅ UART 驱动（r_sci_b_uart）+ 带中断的环形缓冲区（console.c）
- ✅ 链接脚本（fsp_gen.ld + memory_regions.ld）
- ✅ CMSIS 6 头文件（core_cm85.h）
- ✅ 所有 BSP 基础设施

**两者之间的桥梁只需要 ~200 行代码。**

---

## 文件结构

```
ports/renesas-ra8/
├── README.md                      # 本文档
├── Makefile                       # 构建脚本（~80 行）
├── main.c                         # 入口：FSP 初始化 → MicroPython REPL（~80 行）
├── mpconfigport.h                 # MicroPython 功能配置（~30 行）
├── mphalport.h                    # HAL 函数声明
├── mphalport.c                    # HAL 实现：UART I/O + SysTick（~50 行）
├── qstrdefsport.h                 # 空的 qstr 定义
└── boards/
    ├── CPKCOR_RA8P1/
    │   ├── mpconfigboard.h        # 板级配置（时钟频率、UART 引脚等）
    │   └── mpconfigboard.mk       # 板级构建设置（FSP 路径等）
    └── EK_RA8P1/
        ├── mpconfigboard.h
        └── mpconfigboard.mk
```

**注意**：此 port 不包含任何 FSP 源文件。FSP 文件来自 e2 studio 项目，由 `mpconfigboard.mk` 中的路径指向。

---

## 步骤总览

```
[1] 用户：用 e2 studio 创建 Bare Metal 工程
        │
[2] Claude：创建 ports/renesas-ra8 全部源文件
        │
[3] Claude：首次编译 → 修复编译错误
        │
[4] 用户：烧录验证 → 看到 REPL 提示符 >>>
```

---

## 步骤 [1] — 用户操作：创建 e2 studio Bare Metal 工程

> **注意**：当前 `boards/CPKCOR_RA8P1/e2studio_gcc_freertos/` 是 FreeRTOS 工程，其生成的 `bsp_cfg.h` 中 `BSP_CFG_RTOS=2`，会导致 FSP 代码走 RTOS 路径。必须重新创建 Bare Metal 工程。

### 操作步骤

1. 打开 e2 studio
2. **File → New → Renesas RA C/C++ Project**
3. 选择 **"Bare Metal - Minimal"** 模板
4. Device 选择 **R7KA8P1KF**（与现有工程相同）
5. FSP 版本使用与现有工程相同的版本（或更新版本）
6. 在 FSP Configurator 中添加以下 Stacks：

| Stack           | 用途        | 配置                                                |
| --------------- | --------- | ------------------------------------------------- |
| **IOPORT**      | GPIO 引脚控制 | 默认即可                                              |
| **SCI UART** ×1 | REPL 串口   | Channel 9, TX=P208, RX=P209, 波特率 115200 或 2000000 |

7. 不需要的 Stacks（确保不添加）：
   
   - ❌ FreeRTOS
   - ❌ ADC / DAC
   - ❌ SPI / I2C
   - ❌ GPT / AGT 定时器
   - ❌ SDHI / QSPI / SDRAM
   - ❌ USB / CAN / Ethernet

8. 生成代码（点击 "Generate Project Content"）

9. **验证工程能编译并运行**：在 e2 studio 中编译，烧录到板子上，确认 LED 闪烁或串口能输出 "hello"。

10. 将生成的工程目录复制到：
    
    ```
    ports/renesas-ra/boards/CPKCOR_RA8P1/e2studio_baremetal/
    ```
    
    （或直接在此路径创建工程）

### 为什么这一步必须由用户完成？

- e2 studio 是 Windows GUI 工具，无法在命令行中自动化
- FSP 时钟树、引脚复用配置需要通过 FSP Configurator 图形界面完成
- 代码生成依赖 Renesas 的 FSP 许可和版本管理

---

## 步骤 [2] — 创建源文件

我将创建以下文件：

### 2.1 `Makefile`

**职责**：

1. 设置 `CROSS_COMPILE = arm-none-eabi-`
2. 包含 MicroPython 核心构建系统（`py/mkenv.mk` + `py/py.mk`）
3. 从 `mpconfigboard.mk` 获取 FSP 源文件路径
4. 编译 MicroPython 集成代码 + FSP 源文件 + MicroPython 核心
5. 链接出 `firmware.elf`

**关键设计决策**：

- FSP 源文件分散在多个目录（`ra_gen/`、`ra/fsp/src/`、`src/` 等），需要用 `VPATH` 或显式规则处理
- 使用 e2 studio 的链接脚本 `fsp_gen.ld`
- CFLAGS: `-mthumb -mtune=cortex-m85 -mcpu=cortex-m85 -mfpu=fpv5-d16 -mfloat-abi=hard`

### 2.2 `main.c`

**启动流程**：

```
Reset_Handler (FSP startup.c)
  → SystemInit() (FSP system.c: 使能 cache、FPU、TCM)
    → R_BSP_WarmStart() (配置引脚、初始化 SDRAM 等)
      → main() (我们的函数)
        → 配置 SysTick (1ms 间隔)
        → 打开 UART (R_SCI_B_UART_Open)
        → gc_init()
        → mp_init()
        → pyexec_friendly_repl()  ← REPL 交互
```

**与 FreeRTOS 版 main.c 的区别**：

- FreeRTOS 版：`main()` → 创建信号量 → 创建线程 → `vTaskStartScheduler()`（永不返回）
- Bare Metal 版：`main()` → 初始化外设 → 调用 `hal_entry()`（用户代码）
- 我们的版本：直接在我们的 `main.c` 中提供 `main()` 函数，完成 MicroPython 初始化后进入 REPL

### 2.3 `mphalport.c`

**4 个硬件抽象函数的实现**：

```c
// 1. 串口接收：利用 console.c 的环形缓冲区（UART RX 中断填充）
int mp_hal_stdin_rx_chr(void) {
    while (!CONSOLE_HasData()) { __WFI(); }
    CONSOLE_Read(&c, 1);
    return c;
}

// 2. 串口发送：直接写 UART 数据寄存器（轮询）
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    for (i = 0; i < len; i++) {
        SCI9->TDR_BY = str[i];
        while (!(SCI9->CSR & TDRE)) {}
    }
}

// 3. 毫秒计数器：SysTick 中断 +1
volatile uint32_t systick_ms;
void SysTick_Handler(void) { systick_ms++; }
mp_uint_t mp_hal_ticks_ms(void) { return systick_ms; }

// 4. 中断字符：基础 REPL 不需要，空函数即可
void mp_hal_set_interrupt_char(char c) { (void)c; }
```

### 2.4 `mpconfigport.h`

参考 `ports/minimal/mpconfigport.h`，调整：

- `MICROPY_HW_BOARD_NAME` / `MICROPY_HW_MCU_NAME` 由 `mpconfigboard.h` 定义
- **不定义** `MICROPY_MIN_USE_CORTEX_CPU`（因为启动代码由 FSP 提供）
- `MICROPY_HEAP_SIZE` 设置为 128KB（RA8P1 有 1.8MB SRAM）
- 启用 GC、编译器、REPL

### 2.5 板级配置

**`boards/CPKCOR_RA8P1/mpconfigboard.mk`**：

- `CMSIS_MCU = RA8P1`
- `MCU_SERIES = m85`
- `E2STUDIO_DIR` — 指向 e2 studio 工程根目录
- `LD_FILES` — 指向 e2 studio 生成的链接脚本

**`boards/CPKCOR_RA8P1/mpconfigboard.h`**：

- `MICROPY_HW_MCU_SYSCLK` — 1,000,000,000（1GHz，从 bsp_clock_cfg.h 可知）
- `MICROPY_HW_MCU_PCLK` — 125,000,000（PCLKA = 1GHz/8）
- UART REPL 引脚配置
- LED 引脚配置

---

## 步骤 [3] — 首次编译及错误修复

```bash
cd ports/renesas-ra8
make BOARD=CPKCOR_RA8P1 -j$(nproc)
```

### 预期可能遇到的错误

| 预期错误                                          | 原因                | 解决方案                             |
| --------------------------------------------- | ----------------- | -------------------------------- |
| `R7KA8P1KF_core0.h: No such file`             | FSP Include 路径不对  | 检查 `E2STUDIO_DIR` 和 INC 路径       |
| `core_cm85.h: No such file`                   | CMSIS 路径不对        | 检查 CMSIS Include 路径              |
| `bsp_feature.h: No such file`                 | RA8P1 特定 BSP 路径缺失 | 添加 `ra/fsp/src/bsp/mcu/ra8p1`    |
| `undefined reference to g_uart9`              | `hal_data.c` 未编译  | 确认 FSP 源文件列表完整                   |
| `error: BSP_CFG_RTOS` 相关冲突                    | 使用了 FreeRTOS 配置   | 确认步骤 [1] 的 Bare Metal 工程         |
| `undefined reference to _write/_read`         | libc 系统调用未实现      | 用 console.c 中的 `_write/_read` 实现 |
| Section 溢出                                    | Flash/RAM 区域不对    | 确认链接脚本中的内存布局                     |
| `error: target CPU does not support ARM mode` | 缺少 `-mthumb`      | 确认 CFLAGS_CORTEX_M85             |

---

## 步骤 [4] — 烧录验证

```bash
# 生成 hex 文件
make BOARD=CPKCOR_RA8P1

# 通过 J-Link 烧录
JLinkExe -device R7KA8P1KF -if SWD -speed 4000 -autoconnect 1 \
    -CommanderScript flash.jlink
```

**验证清单**：

- [ ] 串口有输出（`screen /dev/ttyUSB0 115200`）
- [ ] 看到 MicroPython 启动信息和 `>>>` 提示符
- [ ] 可以执行 `print("hello")`、`1 + 1` 等基本 Python 语句
- [ ] Ctrl-C 能中断正在运行的程序

---

## 与现有 reneses-ra 的关系

本 port **完全不依赖** `ports/renesas-ra` 的任何文件。它只依赖：

1. MicroPython 核心（`py/`、`shared/`、`extmod/`）
2. e2 studio 生成的 FSP 文件
3. ARM GCC 工具链

后续如果需要添加 `machine.Pin`、`machine.UART` 等模块，可以：

- 逐步从 `ports/renesas-ra` 移植 `machine_*.c` 文件
- 或者直接用 FSP API 重新实现更简洁的版本

---

## 需要用户完成的任务（待办）

- [ ] **步骤 [1]**：用 e2 studio 创建 CPKCOR-RA8P1 Bare Metal 工程
- [ ] **步骤 [1]**：创建 EK-RA8P1 Bare Metal 工程（如果需要支持第二块板）
- [ ] **步骤 [1]**：验证 Bare Metal 工程能独立编译和运行
- [ ] **步骤 [4]**：烧录验证 MicroPython REPL 正常工作

其余所有代码文件的创建和编译调试由 Claude 完成。
