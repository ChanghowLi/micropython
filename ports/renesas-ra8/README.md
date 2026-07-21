# MicroPython Renesas RA8P1 移植

**最后更新：** 2026-07-21

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
| Thumb 内联汇编 | ⏭️ 已关闭 | `MICROPY_EMIT_INLINE_THUMB = 0`；避免 Cortex-M85 执行动态生成代码时卡死，相关测试由框架跳过 |
| Thumb 原生代码发射 | ❌ | 关闭——尚未适配 ARMv8.1-M |
| 浮点数 | 🧪 待烧录验证 | 当前源码已配置双精度 `MICROPY_FLOAT_IMPL_DOUBLE`，编译使用硬浮点 ABI；COM9 上仍运行旧的 32 位浮点固件 |
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
| `mp_builtin_open` | ✅ 已切换到通用 VFS 实现，并通过临时 RAM FAT 文件系统验证文件读写 |
| `mp_lexer_new_from_file` | ⚠️ 已切换到 `MICROPY_READER_VFS`；RAM FAT 测试最后的文件模块导入仍以 `OSError(EINVAL)` 终止 |
| `mp_import_stat` | ⚠️ 已切换到通用 VFS 文件查询；普通目录和文件操作已验证，文件模块导入链路尚未完全通过 |

### 缺失的系统（依赖硬件驱动）

| 系统 | 说明 |
|---|---|
| `machine` 模块 | 无 Pin、UART、I2C、SPI、Timer、ADC、PWM 等类 |
| 文件系统（VFS） | VFS/FAT 和 `os` 已通过测试脚本创建的临时 RAM 块设备验证：格式化、挂载、文件/目录操作及卸载均可运行；文件模块导入在测试末尾仍报 `OSError(EINVAL)`，且启动时尚无自动挂载的持久化块设备 |
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
│       ├── mpconfigboard.mk     # FSP 路径、启用 FAT VFS
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

1. **Raw-paste 模式不可用**——FreeRTOS 任务调度的延迟导致流控窗口协议超时。测试框架和 `mpremote` 会自动回退到普通 raw REPL，不影响正常使用。
   
2. **`MICROPY_EMIT_THUMB = 0`**——MicroPython 的 Thumb 原生发射器面向 ARMv7-M 设计，Cortex-M85（ARMv8.1-M）指令差异导致运行时崩溃。需完成适配后才能开启。
   
3. **`console.c` 环形缓冲区 bug**——e2 studio 生成的 `CONSOLE_Read()` 在读取时错误地将 `s_tail_index` 递减而非递增 `s_head_index`，导致连续字节丢失。已本地修复。
   
4. **测试只能在 `tests/` 目录内运行**——从外部调用 `run-tests.py` 时，CPython 子进程的工作目录解析在 Windows 上出错。解决：`cd tests && python run-tests.py ...`
   
5. **Thumb 内联汇编已安全关闭**——此前 `@micropython.asm_thumb` 可以编译，但调用仅含一条 `nop()` 的最小函数也会使 Cortex-M85/REPL 卡死。现已设置
   `MICROPY_EMIT_INLINE_THUMB = 0`；重新构建和烧录后，测试平台信息从 `platform=minimal arch=armv7emdp inlineasm=thumb float=32-bit unicode` 变为 `platform=minimal float=32-bit unicode`，普通 `0prelim.py` 测试通过。关闭的只是 Python/REPL 中的动态 Thumb 汇编，不影响 GCC 生成的 Thumb 固件、`.S` 汇编文件或 C 语言 `__asm`。
   
6. **串口批量测试存在偶发 first-EOF 超时**——`frozenset_binop.py` 和 `int_big_mul.py` 曾在整组测试中以 `timeout waiting for first EOF reception` 失败，但单独复测分别通过全部 896 和 908 个用例，后续整组复测也通过。这两项不是容器或大整数功能缺陷。
   
7. **测试框架的 target-wiring 属性未初始化**——整组运行 `extmod` 到 `machine_spi_rate.py` 时，主机端因 `Pyboard` 没有 `target_wiring_script` 属性而异常退出。排除 3 个需要外部连线的测试后，`extmod` 可完整结束；这是主机测试框架问题，不是固件崩溃。

*2026-07-10，由 Claude (claude.ai/code) 审查、整理并记录。*

## 完整官方测试汇总

以下是 2026-07-20 使用 COM9、2,000,000 波特率复测后的结果。关闭 Thumb 内联汇编并重新烧录后，测试环境报告为 `platform=minimal`、`float=32-bit`、`unicode`。能力探针不作为普通官方功能测试统计。

> **历史基线说明：** 本节及后续失败/跳过数量对应 2026-07-20 的旧固件，不代表当前源码。VFS/FAT、`os` 和通用 VFS reader 已使用临时 RAM 块设备做过实际验证，但文件模块导入尚未完全通过；当前源码另已改为 `MICROPY_FLOAT_IMPL_DOUBLE`，这一项尚未重新编译、烧录并完成整组复测。2026-07-21 对 COM9 再次运行能力探针仍报告 `float=32-bit`，确认板上此时运行的固件尚未包含双精度配置。

| 测试组 | 通过 | 失败 | 跳过 | 结果说明 |
|---|---:|---:|---:|---|
| `basics` | 555 | 3 | 15 | 共执行 558 项、17,452 个测试用例；3 项记录分别为文件能力缺失、traceback 文件名差异和一次保守 GC 假根现象，详见下表 |
| `extmod` | 67 | 0 | 134 | 共执行 67 项、814 个测试用例且全部通过；另排除 3 个需要 target wiring 的测试 |
| `float` | 61 | 0 | 7 | 共执行 61 项、3,303 个测试用例且全部通过；7 项双精度或字节序相关测试跳过 |
| `import` | 7 | 21 | 2 | 共执行 28 项；大量依赖文件、包和动态导入的测试因当前文件系统及导入支持不完整而失败 |
| `inlineasm` | — | 0 | — | 已设置 `MICROPY_EMIT_INLINE_THUMB = 0`，测试框架不再报告 `inlineasm=thumb`，相关用例应按未支持功能跳过 |
| `micropython` | 45 | 0 | 63 | 共执行 45 项、260 个测试用例且全部通过；native、viper、`.mpy`、meminfo 等未启用或条件不满足的项目跳过 |
| `unicode` | 12 | 0 | 3 | 共执行 12 项、223 个测试用例且全部通过；3 项依赖文件系统的测试跳过 |

### 结果分析

- **基础语言能力总体稳定**：`basics` 558 项中通过 555 项，编译器、容器、异常、生成器、大整数、GC、内存视图、字符串和基本 `struct` 功能大部分通过。
- **基础测试失败项**：`string_tstring_basic.py` 已确认在执行到 `import os` 时失败，前面的 t-string 功能输出正常；要完成后续用例，需要实现 `os`/VFS 和文件导入链路，但不需要修改 t-string 实现。
  `weakref_callback_exception.py` 的 weakref 回调、异常输出和 GC 行为均正确，失败仅因 raw REPL 将 traceback 文件名显示为 `<stdin>`，与期望的测试文件名不同；不需要修改 weakref 或 GC 功能代码。
  `weakref_finalize_collect.py` 曾表现为第一次 `gc.collect()` 后对象仍存活，但最新整组复测已经通过；普通 `weakref_ref_collect.py` 的 21 个用例也全部通过。该项按保守 GC 假根造成的偶发延迟回收继续观察，当前不直接修改 GC 或 weakref。
- **扩展模块已完整复测**：排除 `machine_spi_rate.py`、`machine_uart_irq_txidle.py` 和 `machine_uart_tx.py` 三个需要外部连线的测试后，67 项、814 个用例全部通过，134 项因功能未启用或条件不满足而跳过。
- **浮点功能全部通过**：已设置 `MICROPY_PY_MATH_GAMMA_FIX_NEGINF = 1`，使 `math.gamma(-inf)` 按官方预期抛出 `ValueError`；整组复测 61 项、3,303 个用例全部通过。当前为 32 位单精度浮点配置，7 项双精度或字节序条件测试正常跳过。
- **导入系统尚未完成**：`import` 组的 21 项失败主要集中在文件、包、循环导入、动态导入和模块覆盖等场景，与当前 VFS、`open()`、`mp_import_stat()` 和
   `mp_lexer_new_from_file()` 尚未完整实现相符。
- **MicroPython 专有功能表现稳定**：已执行的 45 项全部通过，包括常量、紧急异常缓冲、堆锁、内存分配失败处理、键盘中断、调度和栈使用等功能。native/viper 因 `MICROPY_EMIT_THUMB = 0` 跳过，`.mpy` 导入仍需在文件系统完成后验证。
- **Unicode 核心功能通过**：字符、索引、迭代、切片、格式化及正则相关测试均通过；仅依赖文件读取的 3 项测试跳过。
- **端口时间接口不完整**：`ports/renesas-ra/modtime.py` 在第一个年份用例就因缺少 `time.mktime()` 终止；若要通过 RA 端口专用测试，需要实现该接口。
- **没有执行多线程 GC 竞态测试**：`thread_gc1.py` 实际在 `import _thread` 时失败。测试框架仅因该脚本带通用 known-flaky 标记而显示 “GC race condition”；当前端口没有 `_thread`，不能据此声称观察到了 GC race。

## 官方测试失败项逐项原因

下面覆盖正式功能测试中的失败，以及本轮复测新增或单独核实的问题。判断依据是 `tests/results` 中保存的 `.out` 与 `.exp` 差异、各测试源码及 COM9 上的单项复测。

### `basics`（3 项）

| 失败测试 | 直接表现 | 失败原因 | 是否需要改代码 |
|---|---|---|---|
| `string_tstring_basic.py` | 单独复测稳定失败；t-string 的前半部分输出均正确，执行到后续用例的 `import os` 时抛出 `ImportError: no module named 'os'` | 测试后半段需要 `os` 和文件系统相关能力；当前端口没有可用的 `os` 模块/VFS，且文件导入接口仍为空壳。不是 t-string 解析、构造或格式化本身失败 | **需要，但不是修改 t-string**：若要完成该测试及文件导入测试，需要实现 `os`/VFS、`open()` 和文件导入链路 |
| `weakref_callback_exception.py` | 单独复测稳定复现；实际异常内容、回调顺序和 GC 后输出均与预期一致，差异仅为回溯位置显示 `File "<stdin>"`，而期望用正则匹配测试文件名 | 串口测试通过 raw REPL 把脚本作为标准输入执行，导致 traceback 源文件名显示为 `<stdin>`。这是测试传输方式造成的文本差异，不是 weakref 或回调异常处理功能失败 | **不需要修改 weakref/GC 代码**；若要求测试结果显示 `pass`，应调整测试运行方式或测试框架的 traceback 文件名匹配 |
| `weakref_finalize_collect.py` | 早期整组及单项复测曾失败：第一次 `gc.collect()` 后 `f.alive` 仍为 `True`；**关闭 Thumb 后的最新整组复测已通过** | 测试已执行 `a = None`、覆盖 Python 栈并主动 GC；普通 `weakref_ref_collect.py` 的 21 个用例也全部通过。最可能是 FreeRTOS/C 栈残留地址偶发形成保守 GC 假根，使对象延迟一个或多个 GC 周期回收 | **当前不改代码，继续观察**；只有后续多次稳定复现时，才检查 `mp_cstack_init_with_sp_here()`、任务栈边界、GC 扫描范围和寄存器保存区 |

### `import`（21 项）

这 21 项具有同一个端口级根因：当前 `mp_import_stat()` 固定返回 `MP_IMPORT_STAT_NO_EXIST`，`mp_lexer_new_from_file()` 固定抛出 `OSError(ENOENT)`，`mp_builtin_open()` 也是空壳，同时没有 VFS。因此内存中的内置模块仍可导入，但测试目录里的 `.py` 文件和包均无法被发现或加载。

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

> pycharm 直接执行，如果不行，把文件系统补上

### Thumb 内联汇编（已通过禁用处理）

| 原测试 | 原直接表现 | 处理结果 | 是否还需改代码 |
|---|---|---|---|
| `inlineasm/thumb/asmargs.py` | 多次稳定出现 `timeout waiting for first EOF reception`，随后 REPL 无响应；最小 `nop()` 动态汇编函数也会卡死 | 已设置 `MICROPY_EMIT_INLINE_THUMB = 0` 并重新烧录；平台信息不再包含 `inlineasm=thumb`，普通 `0prelim.py` 通过。测试框架现在会把相关用例视为不支持并跳过 | **当前不需要**；只有未来明确需要在 Python/REPL 中动态编写 Thumb 汇编时，才适配 Cortex-M85 的可执行 RAM/MPU/ABI |

### 修改优先级结论

根据本轮复测，需要修改或持续观察的问题归并为：

1. 实现 VFS、`os`、`open()`、文件查找和文件模块导入链路；它同时阻断 `string_tstring_basic.py` 后半段、21 项 `import`、Unicode 文件测试和 `.mpy`/`execfile` 等测试。
2. 实现 RA 端口的 `time.mktime()`。
3. 持续观察 `weakref_finalize_collect.py`；当前最新整组测试已通过，不把一次保守 GC 假根现象列为立即修改项。只有稳定复现后才检查 FreeRTOS 任务栈边界。

不需要修改对应固件功能的项目包括：`weakref_callback_exception.py` 的 `<stdin>` 文件名差异、没有 `_thread` 时的 `thread_gc1.py`、三个未配置 target wiring 的硬件测试，以及单项复测已通过的 `frozenset_binop.py`、`int_big_mul.py`。Thumb 内联汇编已通过关闭可选功能安全处理，不再计为待修失败。

## 2026-07-20 旧固件的官方测试跳过项原因汇总

本节集中说明上方“完整官方测试汇总”中所有跳过项的原因，仅适用于 2026-07-20 测试的旧固件。这里的“跳过”是 `run-tests.py` 根据目标平台能力探针、模块可用性或测试自身条件作出的正常判定，不等同于测试失败。2026-07-20 的运行结果只保存了失败项的 `.out`/`.exp`，没有保存逐个 skipped 文件名，因此以下按触发跳过的能力类别记录，不凭空补写文件名。

| 测试组 | 跳过数 | 跳过原因 |
|---|---:|---|
| `basics` | 15 | 测试要求当前固件未提供或未报告的可选语言/运行时能力；测试框架在执行前依据能力探针和测试条件跳过。文件系统相关能力缺失也会使依赖外部文件的用例无法作为普通可执行项运行 |
| `extmod` | 134 | 对应扩展模块、网络、TLS、压缩、哈希、VFS、文件 I/O 或 `machine` 外设能力未编入当前最小固件，或者测试要求目标板具备当前未配置的硬件条件。未启用模块属于端口裁剪结果，不表示已启用模块测试失败 |
| `float` | 7 | 当前固件使用 32 位单精度浮点；要求双精度浮点、特定浮点表示或特定字节序条件的测试不适用，因而跳过 |
| `import` | 2 | 测试前置条件与当前导入配置不匹配，测试框架按条件跳过。其余 21 项不是跳过，而是因为 VFS、`open()`、`mp_import_stat()` 和文件 lexer 尚未实现而实际失败 |
| `inlineasm` | 全部相关项 | `MICROPY_EMIT_INLINE_THUMB = 0`，平台能力信息不再报告 `inlineasm=thumb`；所有 `inlineasm/thumb` 用例按“不支持该可选功能”跳过。关闭原因是 Cortex-M85 上调用最小动态 Thumb 汇编函数也会使 REPL 卡死 |
| `micropython` | 63 | native emitter、viper emitter、动态 Thumb 汇编、`.mpy` 文件导入、meminfo/内存布局检查及其他仅在相应编译选项或运行条件满足时才执行的 MicroPython 专有测试未启用或条件不满足 |
| `unicode` | 3 | 测试需要从文件系统读取 Unicode 源文件或数据文件；当前端口没有完整 VFS、`open()` 和文件导入链路，因此按前置条件跳过。已执行的 Unicode 核心测试全部通过 |

另外，下列项目不计入上表的“跳过数”，但同样没有作为普通通过测试执行：

- `machine_spi_rate.py`、`machine_uart_irq_txidle.py` 和 `machine_uart_tx.py`：需要外部 target wiring；由于主机端 `Pyboard.target_wiring_script` 属性未初始化，本轮从 `extmod` 命令行中主动排除。
- `thread_gc1.py`：当前固件没有 `_thread`，脚本在 `import _thread` 时终止；框架显示的“GC race condition” 来自该测试的通用 known-flaky 标记，不能解释为本端口观察到 GC 竞态，也不能算作已通过的线程测试。
- `ports/renesas-ra/modtime.py`：因缺少 `time.mktime()` 在首个年份用例终止，属于端口专用测试失败，不属于跳过。

因此，旧固件跳过项的根因可以归并为四类：固件按最小配置裁剪了可选模块或 emitter、当时使用 32 位单精度浮点、当时文件系统及文件导入链路尚未完成、硬件连线或目标能力前置条件不满足。当前源码已经改变浮点和 VFS 配置，旧跳过数量不能直接作为新固件结论；重新烧录后必须重跑对应测试，再更新本节统计。
