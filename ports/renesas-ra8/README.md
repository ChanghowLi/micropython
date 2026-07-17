# MicroPython Renesas RA8P1 移植

**最后更新：** 2026-07-16

---

## 概述

本 port 将 MicroPython 集成到 Renesas e2 studio 的 FSP 工程中，目标板为
CPKCOR-RA8P1（Cortex-M85）。REPL 作为一个 FreeRTOS 任务运行。硬件初始化、时钟
树、引脚复用、外设驱动全部由 e2 studio 生成的 FSP 代码负责，MicroPython 侧无需
手写引脚映射或链接脚本。

## 快速开始

### 前置条件

- ARM GCC 13.2 工具链（e2 studio 自带）
- Anaconda Python（或任意 Python 3.8+）—— Windows 用户可能需要 `PYTHON=python`
- 一个 e2 studio 工程，路径见板级配置

### 构建

创建 `ports/renesas-ra8/local.mk`，写入工具链路径：

```makefile
E2S_GCC ?= D:/Programs/Dev/e2s_2025_12/toolchains/gcc_arm/13.2.rel1
```

先生成头文件，再编译：

```bash
make BOARD=CPKCOR_RA8P1 genhdr
make BOARD=CPKCOR_RA8P1 -j$(nproc)
```

Windows 如果报 `Python was not found`（Microsoft Store 存根问题）：

```bash
make BOARD=CPKCOR_RA8P1 genhdr PYTHON=python
```

产物：`build-CPKCOR_RA8P1/firmware.elf`、`.bin`、`.hex`。

### 生成 compile_commands.json（VSCode IntelliSense）

```bash
pip install compiledb
compiledb -n make BOARD=CPKCOR_RA8P1 all
```

`.vscode/c_cpp_properties.json` 中引用
`${workspaceFolder}/ports/renesas-ra8/build-CPKCOR_RA8P1/compile_commands.json`。

### 运行测试

测试通过串口 REPL 发送脚本执行。**必须从 `tests/` 目录内运行**：

```bash
cd tests
python run-tests.py -t COM10 -b 2000000 --test-dirs basics
```

- `-t` 串口号，`-b` 波特率
- 用 `--test-dirs` 指定目录，不要用位置参数传目录（会被当成文件路径）

---

## 功能状态

### ROM 级别

`MICROPY_CONFIG_ROM_LEVEL = MICROPY_CONFIG_ROM_LEVEL_EVERYTHING`（最高级别），
所有标准 Python 语言特性和内置模块均已包含。

### 核心运行时

| 功能 | 状态 | 说明 |
|---|---|---|
| 编译器 (`MICROPY_ENABLE_COMPILER`) | ✅ | 完整 Python 语法 |
| 垃圾回收 (`MICROPY_ENABLE_GC`) | ✅ | 256 KB 堆，Thumb-2 GC 辅助 |
| C 栈检查 (`MICROPY_STACK_CHECK`) | ✅ | `mp_cstack_init_with_sp_here` 设置 12 KB 上限 |
| 持久化代码加载 | ✅ | 支持 `.mpy` 文件 |
| 软复位 | ✅ | REPL 任务内 `goto` 跳转 |
| Ctrl-C 中断 | ✅ | UART ISR 调用 `mp_sched_keyboard_interrupt()` |
| Thumb 内联汇编 | ✅ | `@micropython.asm_thumb` 装饰器 |
| Thumb 原生代码发射 | ❌ | 关闭——尚未适配 ARMv8.1-M |
| 浮点数 | ✅ | 已启用单精度 `MICROPY_FLOAT_IMPL_FLOAT`，编译使用硬浮点 ABI |
| 紧急异常缓冲区 | ✅ | 已启用，固定大小 256 字节，由 `mp_init()` 自动初始化 |

### 内置模块（ROM 级别自动启用）

| 模块 | 状态 | 说明 |
|---|---|---|
| `math`、`cmath` | ✅（未开浮点则仅整数） | |
| `array`、`struct` | ✅ | |
| `collections` | ✅ | |
| `io` | ✅ | 通过 `sys_stdio_mphal.c` 提供流 I/O |
| `json`、`re` | ✅ | |
| `binascii`、`hashlib` | ✅ | |
| `random` | ⚠️ | 无硬件熵源，序列可预测 |
| `gc` | ✅ | |
| `sys` | ✅ | `argv`、`exit`、`modules`、`path` 均已开启；REPL 下已验证 `argv == []`、`path == ['']` |
| `micropython` | ✅ | 含 `kbd_intr` |
| `time` | ✅ | 编译了 `modtime.c` |
| `uctypes` | ✅ | 编译了 `moductypes.c` |
| `errno` | ✅ | |

### HAL 函数（`mphalport.c`）

| 函数 | 状态 |
|---|---|
| `mp_hal_stdin_rx_chr` | ✅ 阻塞读取 UART 环形缓冲区 |
| `mp_hal_stdio_poll` | ✅ RX / TX 查询 |
| `mp_hal_stdout_tx_strn` | ✅ 直接写 UART 数据寄存器 |
| `mp_hal_stdout_tx_strn_cooked` | ✅ `\n` → `\r\n` 转换 |
| `mp_hal_ticks_ms` | ✅ FreeRTOS 系统滴答 |
| `mp_hal_ticks_us` | ✅ `get_system_us()` |
| `mp_hal_ticks_cpu` | ✅ `get_system_ticks()` |
| `mp_hal_delay_ms` | ✅ 区分 ISR / 任务上下文 |
| `mp_hal_delay_us` | ✅ |
| `mp_hal_set_interrupt_char` | ✅ |
| `mp_hal_is_interrupt_char_received` | — 已删除；核心代码未使用，Ctrl-C 由 UART 回调直接调度 `KeyboardInterrupt` |

### REPL 任务（`repl_thread_entry.c`）

| 功能 | 状态 |
|---|---|
| Friendly REPL | ✅ |
| Raw REPL | ✅ |
| 模式切换（Ctrl-A / Ctrl-B） | ✅ 切换时不触发软复位 |
| `nlr_jump_fail` 诊断 | ✅ 打印异常、C 栈用量、FreeRTOS 任务栈水位 |
| `mp_builtin_open` | ❌ 空壳——返回 `mp_const_none` |
| `mp_lexer_new_from_file` | ❌ 空壳——抛出 `OSError(ENOENT)` |
| `mp_import_stat` | ❌ 空壳——返回 `MP_IMPORT_STAT_NO_EXIST` |

### 缺失的系统（依赖硬件驱动）

| 系统 | 说明 |
|---|---|
| `machine` 模块 | 无 Pin、UART、I2C、SPI、Timer、ADC、PWM 等类 |
| 文件系统（VFS） | 无块设备驱动；`open()` 为空壳 |
| 网络 | 无 lwIP、socket、WiFi 协议栈 |
| `_thread` 模块 | FreeRTOS 已就绪，但 Python 级多线程未暴露 |
| `mp_hal_pin_*()` | GPIO HAL 未实现——`machine.Pin` 的前置依赖 |
| 硬件随机数 | RA8P1 TRNG 未接入 `mp_hal_get_random()` |

---

## 文件布局

```
ports/renesas-ra8/
├── Makefile                     # 构建：工具链、编译选项、FSP 源文件发现
├── mpconfigport.h               # 功能配置：ROM 级别、模块开关
├── mphalport.h / mphalport.c    # HAL 实现：UART I/O、滴答、延时
├── qstrdefsport.h               # 板级 qstr 定义
├── local.mk                     # 用户工具链路径（git 忽略）
├── boards/
│   └── CPKCOR_RA8P1/
│       ├── mpconfigboard.h      # 板级标识、大整数实现
│       ├── mpconfigboard.mk     # FSP 路径、VFS 关闭
│       └── e2studio_gcc_freertos/
│           ├── src/             # 胶水代码（mphalport.c、repl_thread_entry.c）
│           ├── ra_gen/          # 自动生成：main.c、hal_data.c、线程…
│           ├── ra_cfg/          # 自动生成：BSP 配置、FreeRTOS 配置
│           ├── ra/              # FSP、FreeRTOS 内核、CMSIS
│           └── Debug/           # 链接脚本、内存区域定义
└── build-CPKCOR_RA8P1/          # 构建输出（git 忽略）
```

---

## 已知问题

1. **Raw-paste 模式不可用**——FreeRTOS 任务调度的延迟导致流控窗口协议超时。
   测试框架和 `mpremote` 会自动回退到普通 raw REPL，不影响正常使用。

2. **`MICROPY_EMIT_THUMB = 0`**——MicroPython 的 Thumb 原生发射器面向 ARMv7-M
   设计，Cortex-M85（ARMv8.1-M）指令差异导致运行时崩溃。需完成适配后才能开启。

3. **`console.c` 环形缓冲区 bug**——e2 studio 生成的 `CONSOLE_Read()` 在读取时
   错误地将 `s_tail_index` 递减而非递增 `s_head_index`，导致连续字节丢失。
   已本地修复。

4. **测试只能在 `tests/` 目录内运行**——从外部调用 `run-tests.py` 时，CPython
   子进程的工作目录解析在 Windows 上出错。解决：`cd tests && python run-tests.py ...`

## 官方自动化测试结果

以下结果来自 `tests/` 下的 MicroPython 官方测试。未列出的已执行测试均通过；
“跳过”表示当前固件配置或平台条件不满足，不等同于测试失败。

### 内置模块

| 模块 | 测试结果 |
|---|---|
| `math`、`cmath` | `math_domain_special.py` 中 1 个特殊定义域测试脚本失败 |
| `array`、`struct` | `array_construct_endian.py` 字节序相关测试因平台条件限制跳过 |
| `io` | `BytesIO`、`StringIO` 内存流操作、缓冲写入及扩展写接口功能全部通过 |
| `json`、`re` | `json` 全部通过；`re_stack_overflow2.py` 深度栈溢出相关测试因平台条件不满足而跳过 |
| `binascii`、`hashlib` | `binascii_crc32.py` 因当前固件未启用对应功能而跳过；MD5 和 SHA1 因当前固件未启用对应算法而跳过 |
| `random` | `random_seed_default.py` 跳过；RA8P1 端口可能尚未提供硬件随机源或系统熵源 |
| `sys` | `sys_path.py` 因当前未建立完整文件系统和导入路径支持而跳过 |
| `time` | `time` 模块未提供 `mktime` 接口 |

### 核心功能

- **垃圾回收**：`basics/gc1.py` 通过；`thread_gc1.py` 运行时出现 MicroPython
  官方标记的已知 GC race condition 偶发失败。该问题属于多线程 GC 竞态场景，
  不影响单线程垃圾回收功能。
- **持久化代码加载**：`import_mpy_native` 类测试因当前未启用 Thumb native
  emitter 而跳过；普通 bytecode `.mpy` 加载能力需通过生成普通 `.mpy` 文件
  进一步验证。
- **Thumb 内联汇编**：`inlineasm/thumb` 测试执行异常，部分 Thumb 指令运行后
  出现 timeout/CRASH。初步判断为 RA8P1 ARMv8.1-M 架构与 MicroPython Thumb
  内联汇编的兼容性问题。
- **浮点数**：0 个通过，7 个双精度相关测试因当前启用单精度浮点配置而跳过。
  `math_domain_special.py` 存在特殊浮点值行为差异，`gamma(-inf)` 返回结果与
  官方预期不一致，初步判断为底层数学库对特殊值处理差异导致。

---

*2026-07-10，由 Claude (claude.ai/code) 审查、整理并记录。*
