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

*2026-07-10，由 Claude (claude.ai/code) 审查、整理并记录。*

## 完整官方测试汇总

以下汇总根据 `basics.txt`、`extmod.txt`、`feature_check.txt`、`float.txt`、
`import.txt`、`inlineasm.txt`、`micropython.txt` 和 `unicode.txt` 整理。测试环境
报告为 `platform=minimal`、`arch=armv7emdp`、`inlineasm=thumb`、
`float=32-bit`、`unicode`。完整结束的测试组共执行 725 项；若加上未正常输出
结束汇总的 `extmod` 日志，全部日志共记录 707 项通过、38 项失败和 164 项跳过。
由于 `extmod` 日志不完整，以上总数不作为正式通过率。

| 测试组 | 通过 | 失败 | 跳过 | 结果说明 |
|---|---:|---:|---:|---|
| `basics` | 556 | 2 | 15 | 共执行 558 项、17,452 个测试用例；`string_tstring_basic.py` 和 `weakref_callback_exception.py` 失败 |
| `extmod` | 20 | 0 | 73 | 日志在 `machine_soft_timer.py` 后结束，没有生成官方 `tests performed` 汇总，结果不完整 |
| `feature_check` | 7 | 14 | 1 | 共执行 21 项；Thumb、Thumb-2、64 位整数、大整数、f-string、反向运算和 t-string 检查通过，native 检查跳过 |
| `float` | 60 | 1 | 7 | 共执行 61 项、3,303 个测试用例；`math_domain_special.py` 失败，7 项双精度或字节序相关测试跳过 |
| `import` | 7 | 21 | 2 | 共执行 28 项；大量依赖文件、包和动态导入的测试因当前文件系统及导入支持不完整而失败 |
| `inlineasm` | 0 | 0 | 0 | 日志显示 0 项测试被执行，不能据此判断 Thumb 内联汇编是否通过 |
| `micropython` | 45 | 0 | 63 | 共执行 45 项、260 个测试用例且全部通过；native、viper、`.mpy`、meminfo 等未启用或条件不满足的项目跳过 |
| `unicode` | 12 | 0 | 3 | 共执行 12 项、223 个测试用例且全部通过；3 项依赖文件系统的测试跳过 |

### 结果分析

- **基础语言能力总体稳定**：`basics` 558 项中通过 556 项，编译器、容器、异常、
  生成器、大整数、GC、内存视图、字符串和基本 `struct` 功能大部分通过。
- **基础测试失败项**：`string_tstring_basic.py` 和
  `weakref_callback_exception.py` 需要继续分析实际输出与期望输出的差异。
- **扩展模块日志不完整**：当前记录中 `binascii` 的 Base64/十六进制接口、
  SHA-256、`heapq` 和 `json` 等测试通过；CRC32、MD5、SHA1、asyncio、btree、
  cryptolib、deflate、framebuf 和 `machine` 等因功能未启用或平台条件不满足而
  跳过。需要重新执行 `extmod` 获取正式汇总。
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

## 官方测试失败项逐项原因

下面对应“完整官方测试汇总”中的 38 项失败。判断依据是
`tests/results` 中保存的 `.out` 与 `.exp` 差异以及各测试源码。需要注意，
`feature_check` 目录中的脚本本质上是测试框架的能力探针，通常由
`run-tests.py` 单独调用并解析输出；将整个目录当作普通测试组执行时，探针输出
会与空的基准输出比较，因此其中多数“失败”不代表端口功能故障。

### `basics`（2 项）

| 失败测试 | 直接表现 | 失败原因 |
|---|---|---|
| `string_tstring_basic.py` | t-string 的前半部分输出均正确，执行到后续用例的 `import os` 时抛出 `ImportError: no module named 'os'` | 测试后半段需要 `os` 和文件系统相关能力；当前端口没有可用的 `os` 模块/VFS，且文件导入接口仍为空壳。不是 t-string 解析、构造或格式化本身失败 |
| `weakref_callback_exception.py` | 实际异常内容、回调顺序和 GC 后输出均与预期一致，差异仅为回溯位置显示 `File "<stdin>"`，而期望用正则匹配测试文件名 | 串口测试通过 raw REPL 把脚本作为标准输入执行，导致 traceback 源文件名丢失。这是测试传输方式造成的文本差异，不是 weakref 或回调异常处理功能失败 |

### `feature_check`（14 项）

| 失败测试 | 失败原因 |
|---|---|
| `async_check.py` | 成功输出 `async`，说明 `async`/`await` 语法可用；因该能力探针的 `.exp` 为空，被普通输出比较误记为失败 |
| `bytearray.py` | 成功输出 `bytearray`，说明内置类型存在；属于能力探针输出与空 `.exp` 的预期性差异 |
| `byteorder.py` | 成功输出 `little`，正确反映 RA8P1 为小端；属于目标信息探针，不是功能失败 |
| `complex.py` | 成功输出 `complex`，说明复数类型存在；属于能力探针输出与空 `.exp` 的预期性差异 |
| `const.py` | 成功输出 `1`，说明 `const()` 可用；属于能力探针输出与空 `.exp` 的预期性差异 |
| `coverage.py` | 输出 `no`，表示固件未编入仅供 MicroPython 内部覆盖率测试使用的 `extra_coverage` 对象；正式固件不启用它是正常配置 |
| `inlineasm_rv32.py` | `@micropython.asm_rv32` 报 `invalid micropython decorator`；RA8P1 是 ARM Cortex-M85，不支持也不应支持 RISC-V 内联汇编 |
| `inlineasm_rv32_zba.py` | 与上一项相同，RISC-V Zba 扩展不适用于 ARM Cortex-M85 |
| `inlineasm_xtensa.py` | `@micropython.asm_xtensa` 报 `invalid micropython decorator`；Xtensa 内联汇编不适用于 ARM Cortex-M85 |
| `repl_emacs_check.py` | 脚本包含用于交互式 Friendly REPL 的 Ctrl-B/光标编辑控制字符；以 raw REPL 普通脚本方式批量发送后形成非法语法，故报 `SyntaxError` |
| `repl_words_move_check.py` | 脚本包含用于交互式 Friendly REPL 的 Ctrl-W 编辑控制字符；raw REPL 不执行行编辑，控制字符进入源码后形成非法语法，故报 `SyntaxError` |
| `set_check.py` | 成功输出 `{1}`，说明 set 字面量语法可用；属于能力探针输出与空 `.exp` 的预期性差异 |
| `slice.py` | 成功输出 `slice`，说明内置 `slice` 类型存在；属于能力探针输出与空 `.exp` 的预期性差异 |
| `target_info.py` | 正常输出 `minimal armv7emdp 0 CPKCOR_RA8P1 None 32 True`；这是供测试框架解析的平台、架构、线程、浮点精度和 Unicode 配置信息，不应按普通测试的空 `.exp` 判定 |

### `float`（1 项）

| 失败测试 | 直接表现 | 失败原因 |
|---|---|---|
| `math_domain_special.py` | 仅 `gamma(-inf)` 与官方期望不同：实际返回 `inf`，期望抛出 `ValueError`；其余特殊值结果一致 | 当前 32 位单精度配置所链接的 ARM/FSP C 数学库对负无穷 `gammaf()` 的定义域处理与 MicroPython 测试预期不同，端口尚未在 `math.gamma` 封装层将该返回值规范化为 `ValueError` |

### `import`（21 项）

这 21 项具有同一个端口级根因：当前 `mp_import_stat()` 固定返回
`MP_IMPORT_STAT_NO_EXIST`，`mp_lexer_new_from_file()` 固定抛出
`OSError(ENOENT)`，`mp_builtin_open()` 也是空壳，同时没有 VFS。因此内存中的
内置模块仍可导入，但测试目录里的 `.py` 文件和包均无法被发现或加载。

| 失败测试 | 首个无法导入的对象 | 该测试因此无法验证的场景 |
|---|---|---|
| `gen_context.py` | `gen_context2` | 跨模块调用生成器时的全局上下文 |
| `import_broken.py` | `pkg` | 导入失败后的 `sys.modules` 清理及再次导入 |
| `import_circular.py` | `circular.main` | 包内循环导入 |
| `import_long_dyn.py` | `import_long_dyn2` | 较大模块的动态/星号导入 |
| `import_override.py` | `import1a` | 覆盖 `__import__` 后再从文件系统执行普通导入 |
| `import_override2.py` | `pkg7` | 自定义 `__import__` 与多级相对包导入 |
| `import_pkg1.py` | `pkg.mod` | 包和子模块的普通导入、缓存及对象同一性 |
| `import_pkg2.py` | `pkg.mod` | `from package.module import name` 及模块缓存 |
| `import_pkg3.py` | `pkg` | `from package import module` |
| `import_pkg4.py` | `pkg2` | 包 `__init__.py` 中的递归导入 |
| `import_pkg5.py` | `pkg3` | 包及子包中的相对导入 |
| `import_pkg6.py` | `pkg6` | 包内相对导入 |
| `import_pkg7.py` | `pkg7` | 多级相对导入及越过包根目录的错误处理 |
| `import_pkg8.py` | `pkg8` | 无 `__init__.py` 的命名空间包导入 |
| `import_pkg9.py` | `pkg9` | 首次导入子模块时设置包属性的行为 |
| `import_star.py` | `pkgstar_default` | 包的 `import *`、`__all__` 和动态导入规则 |
| `import1a.py` | `import1b` | 基本 `import module` |
| `import2a.py` | `import1b` | `from module import name` 及别名 |
| `import3a.py` | `import1b` | `from module import *` |
| `module_dict.py` | `import1b` | 用户文件模块的可读写 `__dict__` |
| `try_module.py` | `import1b` | 跨模块异常后的命名空间恢复 |

综上，38 项记录并不代表 38 个相互独立的端口缺陷：其中 12 项是成功输出却因
能力探针被按普通测试比较而记为失败或是不适用架构，2 项 REPL 探针使用了错误的
执行模式，1 项是 traceback 文件名差异，1 项依赖缺失的 `os`/VFS，1 项是数学库
特殊值语义差异，剩余 21 项全部归结为同一个尚未实现的文件系统导入链路。
