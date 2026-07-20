# MicroPython Renesas RA8P1 移植

**最后更新：** 2026-07-20

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
| Thumb 内联汇编 | ❌ | 可编译，但调用最小 `nop()` 汇编函数也会使 Cortex-M85/REPL 卡死；适配前不可用 |
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
| `time` | ⚠️ | 编译了 `modtime.c`，但缺少 `mktime()` |
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

5. **Thumb 内联汇编只能编译、不能执行**——`@micropython.asm_thumb` 的能力探针
   可以通过，只定义含 `mov()` 的函数也能输出 `compiled`；但调用仅含一条
   `nop()` 的最小汇编函数就会稳定卡死，硬件复位后普通 raw REPL 恢复正常。
   应检查动态代码所在 RAM 的执行权限、MPU XN 属性、D-cache/I-cache 同步、
   Thumb 函数地址最低位及 ARMv8.1-M 调用/返回约定。在修复前不应宣称该功能可用。

6. **串口批量测试存在偶发 first-EOF 超时**——`frozenset_binop.py` 和
   `int_big_mul.py` 曾在整组测试中以 `timeout waiting for first EOF reception`
   失败，但单独复测分别通过全部 896 和 908 个用例，后续整组复测也通过。
   这两项不是容器或大整数功能缺陷。

7. **测试框架的 target-wiring 属性未初始化**——整组运行 `extmod` 到
   `machine_spi_rate.py` 时，主机端因 `Pyboard` 没有
   `target_wiring_script` 属性而异常退出。排除 3 个需要外部连线的测试后，
   `extmod` 可完整结束；这是主机测试框架问题，不是固件崩溃。

*2026-07-10，由 Claude (claude.ai/code) 审查、整理并记录。*

## 完整官方测试汇总

以下是 2026-07-20 使用 COM9、2,000,000 波特率复测后的结果。测试环境报告为
`platform=minimal`、`arch=armv7emdp`、`inlineasm=thumb`、`float=32-bit`、
`unicode`。`feature_check` 是测试框架的能力探针目录，不应作为普通测试组统计
通过率；`inlineasm` 因最小执行用例会使开发板卡死，也没有继续整组运行。

| 测试组 | 通过 | 失败 | 跳过 | 结果说明 |
|---|---:|---:|---:|---|
| `basics` | 555 | 3 | 15 | 共执行 558 项、17,452 个测试用例；稳定失败为 `string_tstring_basic.py`、`weakref_callback_exception.py` 和 `weakref_finalize_collect.py` |
| `extmod` | 67 | 0 | 134 | 共执行 67 项、814 个测试用例且全部通过；另排除 3 个需要 target wiring 的测试 |
| `feature_check` | — | — | — | 不是普通测试组；直接作为目录运行产生的“失败”不能计入固件失败 |
| `float` | 60 | 1 | 7 | 共执行 61 项、3,303 个测试用例；`math_domain_special.py` 失败，7 项双精度或字节序相关测试跳过 |
| `import` | 7 | 21 | 2 | 共执行 28 项；大量依赖文件、包和动态导入的测试因当前文件系统及导入支持不完整而失败 |
| `inlineasm` | 0 | 1 | 0 | `thumb/asmargs.py` 稳定卡在 first EOF；进一步缩减后确认代码可编译，但调用仅含 `nop()` 的函数也会使开发板卡死 |
| `micropython` | 45 | 0 | 63 | 共执行 45 项、260 个测试用例且全部通过；native、viper、`.mpy`、meminfo 等未启用或条件不满足的项目跳过 |
| `unicode` | 12 | 0 | 3 | 共执行 12 项、223 个测试用例且全部通过；3 项依赖文件系统的测试跳过 |

### 结果分析

- **基础语言能力总体稳定**：`basics` 558 项中通过 555 项，编译器、容器、异常、
  生成器、大整数、GC、内存视图、字符串和基本 `struct` 功能大部分通过。
- **基础测试失败项**：`string_tstring_basic.py` 已确认在执行到
  `import os` 时失败，前面的 t-string 功能输出正常；要完成后续用例，需要实现
  `os`/VFS 和文件导入链路，但不需要修改 t-string 实现。
  `weakref_callback_exception.py` 的 weakref 回调、异常输出和 GC 行为均正确，
  失败仅因 raw REPL 将 traceback 文件名显示为 `<stdin>`，与期望的测试文件名
  不同；不需要修改 weakref 或 GC 功能代码。
  `weakref_finalize_collect.py` 则稳定表现为第一次 `gc.collect()` 后对象仍存活；
  普通 `weakref_ref_collect.py` 的 21 个用例全部通过，因此应优先检查 FreeRTOS
  任务的 GC 栈边界和保守扫描假根，而不是重写 weakref。
- **扩展模块已完整复测**：排除 `machine_spi_rate.py`、
  `machine_uart_irq_txidle.py` 和 `machine_uart_tx.py` 三个需要外部连线的测试后，
  67 项、814 个用例全部通过，134 项因功能未启用或条件不满足而跳过。
- **特性检查需按架构解释**：RV32、RV32 Zba、Xtensa 内联汇编检查不适用于
  Cortex-M85；REPL 编辑功能和部分配置探测失败也不等同于 Python 核心功能故障。
  `inlineasm_thumb.py` 与 `inlineasm_thumb2.py` 的特性检查通过。
- **浮点功能总体可用**：60 项通过；当前为 32 位单精度浮点配置，因此双精度
  相关测试跳过。`math_domain_special.py` 的特殊值行为仍与官方预期存在差异。
- **导入系统尚未完成**：`import` 组的 21 项失败主要集中在文件、包、循环导入、
  动态导入和模块覆盖等场景，与当前 VFS、`open()`、`mp_import_stat()` 和
  `mp_lexer_new_from_file()` 尚未完整实现相符。
- **MicroPython 专有功能表现稳定**：已执行的 45 项全部通过，包括常量、
  紧急异常缓冲、堆锁、内存分配失败处理、键盘中断、调度和栈使用等功能。
  native/viper 因 `MICROPY_EMIT_THUMB = 0` 跳过，`.mpy` 导入仍需在文件系统完成
  后验证。
- **Unicode 核心功能通过**：字符、索引、迭代、切片、格式化及正则相关测试均
  通过；仅依赖文件读取的 3 项测试跳过。
- **端口时间接口不完整**：`ports/renesas-ra/modtime.py` 在第一个年份用例就因
  缺少 `time.mktime()` 终止；若要通过 RA 端口专用测试，需要实现该接口。
- **没有执行多线程 GC 竞态测试**：`thread_gc1.py` 实际在 `import _thread` 时
  失败。测试框架仅因该脚本带通用 known-flaky 标记而显示 “GC race condition”；
  当前端口没有 `_thread`，不能据此声称观察到了 GC race。

## 官方测试失败项逐项原因

下面覆盖原“完整官方测试汇总”中的 38 项记录，以及本轮复测新增或单独核实的
失败。判断依据是 `tests/results` 中保存的 `.out` 与 `.exp` 差异、各测试源码
及 COM9 上的单项复测。需要注意，
`feature_check` 目录中的脚本本质上是测试框架的能力探针，通常由
`run-tests.py` 单独调用并解析输出；将整个目录当作普通测试组执行时，探针输出
会与空的基准输出比较，因此其中多数“失败”不代表端口功能故障。

### `basics`（3 项）

| 失败测试 | 直接表现 | 失败原因 | 是否需要改代码 |
|---|---|---|---|
| `string_tstring_basic.py` | 单独复测稳定失败；t-string 的前半部分输出均正确，执行到后续用例的 `import os` 时抛出 `ImportError: no module named 'os'` | 测试后半段需要 `os` 和文件系统相关能力；当前端口没有可用的 `os` 模块/VFS，且文件导入接口仍为空壳。不是 t-string 解析、构造或格式化本身失败 | **需要，但不是修改 t-string**：若要完成该测试及文件导入测试，需要实现 `os`/VFS、`open()` 和文件导入链路 |
| `weakref_callback_exception.py` | 单独复测稳定复现；实际异常内容、回调顺序和 GC 后输出均与预期一致，差异仅为回溯位置显示 `File "<stdin>"`，而期望用正则匹配测试文件名 | 串口测试通过 raw REPL 把脚本作为标准输入执行，导致 traceback 源文件名显示为 `<stdin>`。这是测试传输方式造成的文本差异，不是 weakref 或回调异常处理功能失败 | **不需要修改 weakref/GC 代码**；若要求测试结果显示 `pass`，应调整测试运行方式或测试框架的 traceback 文件名匹配 |
| `weakref_finalize_collect.py` | 整组测试出现后单独复测仍稳定失败；第一次 `gc.collect()` 后 `f.alive` 仍为 `True`，对象和回调仍可由 `peek()`/`detach()` 取得 | 测试已执行 `a = None`、用列表覆盖 Python 栈并主动 GC；普通 `weakref_ref_collect.py` 的 21 个用例全部通过。最可能是 FreeRTOS/C 栈保守扫描中的残留地址形成假根，使对象延迟回收 | **需要检查端口代码**：优先核对 `mp_cstack_init_with_sp_here()` 调用位置、任务栈顶/增长方向、GC 扫描范围和寄存器保存区；暂不应修改 weakref 核心实现 |

### `feature_check`（14 项）

| 失败测试 | 失败原因 | 是否需要改代码 |
|---|---|---|
| `async_check.py` | 单独复测成功输出 `async`，说明 `async`/`await` 语法可用；因该能力探针的 `.exp` 为空，被普通输出比较误记为失败 | **不需要**；应从普通失败统计中移除 |
| `bytearray.py` | 成功输出 `bytearray`，说明内置类型存在；属于能力探针输出与空 `.exp` 的预期性差异 | **不需要**；应修正测试分类 |
| `byteorder.py` | 成功输出 `little`，正确反映 RA8P1 为小端；属于目标信息探针，不是功能失败 | **不需要** |
| `complex.py` | 成功输出 `complex`，说明复数类型存在；属于能力探针输出与空 `.exp` 的预期性差异 | **不需要** |
| `const.py` | 成功输出 `1`，说明 `const()` 可用；属于能力探针输出与空 `.exp` 的预期性差异 | **不需要** |
| `coverage.py` | 输出 `no`，表示固件未编入仅供 MicroPython 内部覆盖率测试使用的 `extra_coverage` 对象；正式固件不启用它是正常配置 | **不需要**，除非专门构建内部覆盖率固件 |
| `inlineasm_rv32.py` | `@micropython.asm_rv32` 报 `invalid micropython decorator`；RA8P1 是 ARM Cortex-M85，不支持也不应支持 RISC-V 内联汇编 | **不需要** |
| `inlineasm_rv32_zba.py` | 与上一项相同，RISC-V Zba 扩展不适用于 ARM Cortex-M85 | **不需要** |
| `inlineasm_xtensa.py` | `@micropython.asm_xtensa` 报 `invalid micropython decorator`；Xtensa 内联汇编不适用于 ARM Cortex-M85 | **不需要** |
| `repl_emacs_check.py` | 脚本包含用于交互式 Friendly REPL 的 Ctrl-B/光标编辑控制字符；以 raw REPL 普通脚本方式批量发送后形成非法语法，故报 `SyntaxError` | **不需要改核心功能**；应在交互式 Friendly REPL 中测试或调整测试执行方式 |
| `repl_words_move_check.py` | 脚本包含用于交互式 Friendly REPL 的 Ctrl-W 编辑控制字符；raw REPL 不执行行编辑，控制字符进入源码后形成非法语法，故报 `SyntaxError` | **不需要改核心功能**；应调整测试执行方式 |
| `set_check.py` | 成功输出 `{1}`，说明 set 字面量语法可用；属于能力探针输出与空 `.exp` 的预期性差异 | **不需要** |
| `slice.py` | 成功输出 `slice`，说明内置 `slice` 类型存在；属于能力探针输出与空 `.exp` 的预期性差异 | **不需要** |
| `target_info.py` | 正常输出 `minimal armv7emdp 0 CPKCOR_RA8P1 None 32 True`；这是供测试框架解析的平台、架构、线程、浮点精度和 Unicode 配置信息，不应按普通测试的空 `.exp` 判定 | **不需要**；应由测试框架解析输出 |

### `float`（1 项）

| 失败测试 | 直接表现 | 失败原因 | 是否需要改代码 |
|---|---|---|---|
| `math_domain_special.py` | 单项及整组复测均稳定失败；56 个特殊值检查中仅 `gamma(-inf)` 不同：实际返回 `inf`，期望抛出 `ValueError` | 当前 32 位单精度配置所链接的 ARM/FSP C 数学库对负无穷 `gammaf()` 的定义域处理与 MicroPython 测试预期不同，端口尚未在 `math.gamma` 封装层将该返回值规范化为 `ValueError` | **需要**：在端口或 `math.gamma` 封装层对负无穷执行官方兼容的定义域异常处理 |

### `import`（21 项）

这 21 项具有同一个端口级根因：当前 `mp_import_stat()` 固定返回
`MP_IMPORT_STAT_NO_EXIST`，`mp_lexer_new_from_file()` 固定抛出
`OSError(ENOENT)`，`mp_builtin_open()` 也是空壳，同时没有 VFS。因此内存中的
内置模块仍可导入，但测试目录里的 `.py` 文件和包均无法被发现或加载。

| 失败测试 | 首个无法导入的对象 | 该测试因此无法验证的场景 | 是否需要改代码 |
|---|---|---|---|
| `gen_context.py` | `gen_context2` | 跨模块调用生成器时的全局上下文 | **需要**：实现共同的 VFS/文件导入链路 |
| `import_broken.py` | `pkg` | 导入失败后的 `sys.modules` 清理及再次导入 | **需要**：同上 |
| `import_circular.py` | `circular.main` | 包内循环导入 | **需要**：同上 |
| `import_long_dyn.py` | `import_long_dyn2` | 较大模块的动态/星号导入 | **需要**：同上 |
| `import_override.py` | `import1a` | 覆盖 `__import__` 后再从文件系统执行普通导入 | **需要**：同上 |
| `import_override2.py` | `pkg7` | 自定义 `__import__` 与多级相对包导入 | **需要**：同上 |
| `import_pkg1.py` | `pkg.mod` | 包和子模块的普通导入、缓存及对象同一性 | **需要**：同上 |
| `import_pkg2.py` | `pkg.mod` | `from package.module import name` 及模块缓存 | **需要**：同上 |
| `import_pkg3.py` | `pkg` | `from package import module` | **需要**：同上 |
| `import_pkg4.py` | `pkg2` | 包 `__init__.py` 中的递归导入 | **需要**：同上 |
| `import_pkg5.py` | `pkg3` | 包及子包中的相对导入 | **需要**：同上 |
| `import_pkg6.py` | `pkg6` | 包内相对导入 | **需要**：同上 |
| `import_pkg7.py` | `pkg7` | 多级相对导入及越过包根目录的错误处理 | **需要**：同上 |
| `import_pkg8.py` | `pkg8` | 无 `__init__.py` 的命名空间包导入 | **需要**：同上 |
| `import_pkg9.py` | `pkg9` | 首次导入子模块时设置包属性的行为 | **需要**：同上 |
| `import_star.py` | `pkgstar_default` | 包的 `import *`、`__all__` 和动态导入规则 | **需要**：同上 |
| `import1a.py` | `import1b` | 基本 `import module` | **需要**：已单项复测确认 |
| `import2a.py` | `import1b` | `from module import name` 及别名 | **需要**：实现共同导入链路 |
| `import3a.py` | `import1b` | `from module import *` | **需要**：实现共同导入链路 |
| `module_dict.py` | `import1b` | 用户文件模块的可读写 `__dict__` | **需要**：实现共同导入链路 |
| `try_module.py` | `import1b` | 跨模块异常后的命名空间恢复 | **需要**：实现共同导入链路 |

### Thumb 内联汇编执行（1 项及最小复现）

| 失败测试 | 直接表现 | 正确原因 | 是否需要改代码 |
|---|---|---|---|
| `inlineasm/thumb/asmargs.py` | 多次稳定出现 `timeout waiting for first EOF reception`，随后 REPL 无响应；硬件复位后普通 `0prelim.py` 和 `print(123)` 均通过 | `@micropython.asm_thumb` 能识别，最小函数及 `mov()` 能编译并输出 `compiled`；但调用仅含 `nop()` 的无参数函数也会卡死。因此不是参数、`mov` 或通用串口问题，而是动态生成 Thumb 代码的执行环境/ABI 问题 | **必须处理或禁用**：检查可执行 RAM/MPU XN、D-cache/I-cache 同步、Thumb 地址位及 ARMv8.1-M 调用返回约定；修复前应关闭或明确禁用内联汇编 |

### 端口及测试基础设施补充核查

| 测试/项目 | 实际结果与正确原因 | 是否需要改代码 |
|---|---|---|
| `ports/renesas-ra/modtime.py` | 单项稳定失败；在首个 `Testing 2000` 用例调用 `time.mktime()` 时抛出 `AttributeError`，后续时间转换用例未执行 | **需要修改端口代码**：实现 `time.mktime()`，然后重新运行完整端口时间测试 |
| `thread/thread_gc1.py` | 实际在 `import _thread` 时失败，未执行任何多线程 GC 逻辑；框架因脚本带通用 known-flaky 标签而显示 “GC race condition” | **当前不需要**，除非端口目标包含 Python `_thread`；必须修正 README/统计，不能把它写成已观察到的 GC race |
| `extmod/machine_spi_rate.py` | 主机在发送脚本前访问缺失的 `pyb.target_wiring_script` 属性而异常退出；脚本按当前无 `machine.SPI` 配置本应跳过 | **不需要改固件**；需要初始化主机测试框架属性或在未配置连线时排除 |
| `extmod/machine_uart_irq_txidle.py` | 需要 target wiring，本轮未提供外部 UART 连线配置而排除 | **不需要改固件**；若要测试 UART，需先实现 `machine.UART` 并提供连线配置 |
| `extmod/machine_uart_tx.py` | 需要 target wiring，本轮未提供外部 UART 连线配置而排除 | **不需要改固件**；条件同上 |
| `frozenset_binop.py` | 批量测试曾 first-EOF 超时；单项 896 个用例全部通过，后续整组也通过 | **不需要修改 frozenset**；属于偶发 raw REPL/UART 传输问题 |
| `int_big_mul.py` | 批量测试曾失败；单项 908 个用例全部通过，后续整组也通过 | **不需要修改大整数**；按偶发测试传输问题处理 |

### 修改优先级结论

原 38 项记录并不代表 38 个独立端口缺陷。根据本轮复测，需要修改固件/端口代码
的实际问题归并为：

1. 实现 VFS、`os`、`open()`、文件查找和文件模块导入链路；它同时阻断
   `string_tstring_basic.py` 后半段、21 项 `import`、Unicode 文件测试和
   `.mpy`/`execfile` 等测试。
2. 修正 `math.gamma(-inf)`，使其按官方预期抛出 `ValueError`。
3. 修复 Cortex-M85 动态 Thumb 代码执行环境；在修复前禁用 Thumb 内联汇编。
4. 实现 RA 端口的 `time.mktime()`。
5. 检查 FreeRTOS 任务的 GC 栈边界与保守扫描范围，解决
   `weakref_finalize_collect.py` 中稳定出现的假根/延迟回收。

不需要修改对应固件功能的项目包括：14 项被误当普通测试的 `feature_check`
探针、`weakref_callback_exception.py` 的 `<stdin>` 文件名差异、没有 `_thread`
时的 `thread_gc1.py`、三个未配置 target wiring 的硬件测试，以及单项复测已通过
的 `frozenset_binop.py`、`int_big_mul.py`。
