# MicroPython 测试报告（2026-07-23）

## 1. 报告说明

- 测试日期：2026-07-23
- 目标环境（由日志报告）：`platform=minimal`、64 位浮点、启用 Unicode
- 输入结果：`basics.txt`、`extmod.txt`、`feature_check.txt`、`float.txt`、`import.txt`、`inlineasm.txt`、`micropython.txt`、`unicode.txt`，以及板端逐项复测结果
- 符号：✅ 通过；❌ 失败；⏭ 跳过/未执行；⚠️ 部分可用或证据不足

结论采用以下证据优先级：当前固件板端逐项复测 > 当前整组自动测试 > 旧固件测试结果 > 仅根据源码和编译配置推断。已有板端复测能够解释自动测试差异时，以板端复测结论为准。“跳过”不等于“功能失败”：它可能表示模块没有编入固件、测试依赖缺失、目标平台不适用，或测试框架因特性探测结果而主动排除。

日志中的 `platform=minimal` 是测试框架获得的平台标识，不代表固件采用最小 ROM 功能等级；当前固件配置的 ROM level 为 `EVERYTHING`。

## 2. 总体结论

当前固件的 Python 核心语言、常用内建类型、浮点/复数、Unicode、`io`、RAM FAT 文件读写、符合 FAT 8.3 文件名的源码模块导入、`json`、`heapq`、`marshal`、`re`、`uctypes`、部分 `random`、部分 `binascii`、部分 `hashlib` 和大部分 `micropython` 基础接口可用。

自动导入测试 29 项中仅 7 项通过、22 项失败、1 项跳过，但不能将这些结果全部解释成当前 import 核心缺陷。当前源码逐项测试证明：通用 VFS、`open()`、`mp_import_stat()`、`MICROPY_READER_VFS` 以及 `/ram/mod.py` 的查找、读取、编译、执行和缓存链路均可用。2026-07-23 启用 FAT LFN 后，`vfs_fat_more.py` 已成功创建并导入长文件名模块 `test_module.py`。完整包与复杂导入场景仍需在补齐测试依赖后复测。由于 `run-tests.py` 在每个测试前都会软复位，测试文件系统还必须跨 MicroPython 软复位保留内容并在启动时自动挂载；临时 Python `bytearray` RAM FAT 不能满足整组 import 测试要求。

当前端口源码和构建配置中没有可用的网络协议栈及 `network`/`socket` 适配，也没有 `machine.Pin`、UART、I2C、SPI、Timer、ADC、PWM 等 Python 类。因此这些能力在当前固件中尚未实现；这属于实现状态结论，不是板端功能测试结果。`io` 的内存流测试全部通过，RAM FAT 的格式化、挂载、长文件名、文件/目录操作、卸载以及源码文件读取和导入也已逐项验证；当前限制是没有启动时自动挂载的持久化块设备。

### 2.1 模块/能力总览

| 模块或能力 | 结论 | 本次证据 | 使用建议 |
|---|---|---|---|
| Python 核心语法与运行时 | ⚠️ 部分可用（接近完整） | 最新整组日志 554 通过、4 失败；4 项均已有单项复测或输出差异分析 | `frozenset` 二元运算及大整数乘法已单项通过；weakref 为回溯文件名差异；大整数除法单项测试因串口中途丢失约 523 行输出而失败，尚无数值算法错误证据 |
| `float` / `math` / `cmath` / `complex` | ✅ 本次覆盖完整通过 | 66 项全部通过，另有 2 个端序相关测试跳过 | 可用；端序构造未验证 |
| Unicode 字符串 | ✅ 本次覆盖完整通过 | 12 项全部通过，3 个文件测试跳过 | 内存中的 Unicode 可用；Unicode 文件读写未验证 |
| `io` / VFS / RAM FAT | ⚠️ 部分可用 | 9 个 io 测试通过；启用 LFN 后 5 个 FAT 专项、72 个子用例全部通过 | 内存流、RAM FAT、长文件名和源码模块导入可用；当前无启动时自动挂载的持久化块设备 |
| `json` | ✅ 本次覆盖完整通过 | 日志所列 13 项全部通过 | 编解码、流式 load/dump、浮点、64 位整数可用 |
| `heapq` | ✅ 本次覆盖通过 | `heapq1.py` 通过 | 基础堆操作可用 |
| `marshal` | ✅ 本次覆盖完整通过 | basic、嵌套函数、stress 共 3 项通过 | 已测序列化能力可用 |
| `re` | ✅ 本次覆盖通过 | 15 项通过，1 个额外栈溢出场景跳过 | 常用正则、分组、替换、切分、边界及错误处理可用 |
| `random` | ⚠️ 部分可用 | basic、extra、float 共 3 项通过；默认种子测试跳过 | 已测伪随机接口可用；默认硬件熵播种未验证 |
| `uctypes` | ✅ 本次覆盖完整通过 | 21 项全部通过 | 已测结构体、指针、数组、端序、浮点及 sizeof 能力可用 |
| `os` | ⚠️ 部分可用 | VFS 文件/目录操作已测；`os_urandom.py` 跳过 | 常用 VFS 操作可用；`os.urandom()` 当前不可用 |
| `select` | ❌ 未编入 | 6项 poll/ipoll 测试全部在模块可用性检查处跳过；当前 Makefile 未编译对应模块 | 当前固件不可用 |
| `ssl` / `tls` / `websocket` | ❌ 未编入 | 所列测试全部跳过；当前构建未包含网络和 TLS/WebSocket 实现 | 当前固件不可用 |
| `binascii` | ⚠️ 部分可用 | Base64、hexlify、unhexlify 通过；`MICROPY_PY_BINASCII_CRC32 = 0` | 已测 Base64/hex 接口可用；CRC32 当前未启用 |
| `hashlib` | ⚠️ 部分可用 | `sha256`、final 行为通过；MD5、SHA-1 的属性探测跳过 | SHA-256 可用；MD5/SHA-1 当前未提供 |
| `micropython` | ⚠️ 部分可用 | 47 项通过、61 项跳过 | `const`、堆控制、调度、RingIO 等可用；native/viper 已关闭 |
| `import` | ⚠️ 部分可用 | 自动组 7 通过、22 失败；当前源码 `/ram/mod.py` 逐项导入通过 | 8.3 文件名源码模块可用；包、LFN 和复杂导入需部署依赖文件后复测 |
| `asyncio` | ❌ 未编入 | 所列 33 项均在模块可用性检查处跳过；当前 Makefile 未编译 `modasyncio.c` | 当前固件不可用；编入模块及 Python 组件后再测试 |
| `machine` | ❌ 未实现 | 当前端口源码/配置中没有 Pin、UART、I2C、SPI、Timer、ADC、PWM 等 Python 类 | Python 板级外设 API 当前固件不可用 |
| `network` / `socket` | ❌ 未实现 | 当前端口源码/配置中没有可用的网络协议栈及 MicroPython 网络适配 | 网络功能当前固件不可用；是否移植取决于项目需求 |
| `btree` | ❌ 未编入 | 所列用例在 import 检查处跳过；当前 Makefile 未编译 `modbtree.c` | 当前固件不可用 |
| `cryptolib` | ❌ 未编入 | 所列 AES 用例在 import 检查处跳过；当前 Makefile 未编译 `modcryptolib.c` | 当前固件不可用 |
| `deflate` | ❌ 未编入 | 所列用例跳过；当前 Makefile 未编译 `moddeflate.c` | 当前固件不可用 |
| `framebuf` | ❌ 未编入 | 所列用例在 import 检查处跳过；当前 Makefile 未编译 `modframebuf.c` | 当前固件不可用 |
| Inline assembler | ❌ 已关闭/不支持 | `MICROPY_EMIT_INLINE_THUMB = 0`；最小 `nop()` 函数曾导致 Cortex-M85 卡死 | Python 动态 Thumb 汇编不可用；不影响 GCC 生成的 Thumb 固件 |

## 3. 各测试集统计

| 测试集 | 通过 | 失败 | 跳过 | 结论 |
|---|---:|---:|---:|---|
| basics | 554 | 4 | 15 | 原始日志记录 4 项失败；逐项复核均未发现已确认的语言功能缺陷，`int_big_div.py` 为大段串口输出丢失 |
| float | 66 | 0 | 2 | 已执行项全部通过 |
| import | 7 | 22 | 1 | 原始自动结果不完整；多数失败缺少目标端依赖文件，不能直接判为 importer 缺陷 |
| micropython | 47 | 0 | 61 | 已执行项全部通过；native/viper 因功能关闭而跳过 |
| unicode | 12 | 0 | 3 | 内存 Unicode 能力通过，文件相关未验证 |
| feature_check | 5 | 16 | 1 | 特性探测结果，不作为正式失败统计 |
| inlineasm | 0 | 0 | 0 | 没有实际测试 |
| extmod | 77 | 0 | 124 | 原完整测试中的 5 项 FAT 失败在启用 LFN 后单项复测全部通过（72 个子用例） |

## 4. basics：核心 API 和语言能力

### 4.1 已验证完整可用的能力

以下能力在其对应正式用例中全部通过，因此不逐条展开每个测试脚本：

| API/语言能力 | 状态 | 覆盖内容摘要 |
|---|:---:|---|
| `abs`、`all`、`any`、`bin`、`callable`、`chr` | ✅ | 普通整数及大整数相关用例通过 |
| `compile`、`eval`、`exec` | ✅ | 字符串、buffer、错误处理通过 |
| `delattr`、`dir`、`getattr`、`hasattr`、`setattr` | ✅ | 属性读取、写入、删除通过 |
| `divmod`、`pow`、`round` | ✅ | 普通整数、大整数及三参数形式通过；但大整数直接乘除另有失败，见后文 |
| `enumerate`、`filter`、`map`、`zip` | ✅ | 迭代相关基本行为通过 |
| `hash`、`hex`、`oct`、`ord`、`id` | ✅ | 含生成器/大整数相关覆盖 |
| `issubclass`、`isinstance` 相关类型行为 | ✅ | 类、继承和类型检查相关用例通过 |
| `len`、`locals`、`globals` | ✅ | 本次正式用例通过 |
| `min`、`max`、`next`、`sum` | ✅ | 参数及常用数据类型通过 |
| `print`、`help` | ✅ | 本次正式用例通过 |
| `property`、`classmethod`、`staticmethod`、`super` | ✅ | 继承、多继承、闭包等场景通过 |
| `range`、`reversed`、`slice`、`sorted` | ✅ | 属性、边界、切片和排序行为通过 |
| `type`、`object` | ✅ | 构造、字典、`__new__` 等通过 |
| `bool` | ✅ | 真值和布尔行为通过 |
| `bytes` | ✅ | 构造、比较、切片、查找、替换、拆分、格式化等；端序用例跳过 |
| `bytearray` | ✅ | 构造、追加、计数、解码、切片赋值等；端序用例跳过 |
| `memoryview` | ✅ | 构造、GC、itemsize、切片与赋值通过 |
| `str` | ✅ | 查找、替换、切分、格式化、f-string/t-string、大小写等通过 |
| `list` | ✅ | 增删改查、切片、排序、复制、扩展等通过 |
| `tuple` | ✅ | 比较、计数、索引、切片、子类等通过 |
| `dict` | ✅ | 构造、视图、迭代、更新、union、pop、setdefault 等通过 |
| `set` | ✅ | 集合运算、更新、迭代、包含、比较等通过 |
| `deque`、`namedtuple`、`OrderedDict` | ✅ | 日志中的相关正式用例全部通过 |
| 函数、参数、闭包、lambda、装饰器 | ✅ | 位置/关键字/可变参数、默认参数、作用域等通过 |
| 生成器及 `yield from` | ✅ | send/throw/close/return/异常传播等通过 |
| `async`/`await` 语法及 async context | ✅ | basics 正式测试通过；不代表 `asyncio` 模块已验证 |
| 类、继承与特殊方法 | ✅ | 描述符、多继承、运算符反射、原生类型子类多数通过 |
| 异常、`try`/`except`/`finally` | ✅ | 常用异常控制流通过；异常链测试跳过 |
| `for`、`while`、`if`、推导式 | ✅ | 常用控制流通过 |
| `with` 上下文管理 | ✅ | break/continue/raise/return 场景通过 |
| `struct` | ✅ | 常规及大整数相关用例通过；端序用例跳过 |
| `sys` 基础接口 | ✅ | stdio、exit、getsizeof、tracebacklimit 通过；`argv == []`、`path == ['']` 已在 REPL 验证 |
| `gc` | ✅ | 本次 `gc1.py` 通过 |

### 4.2 自动日志失败项及逐项复核

| 测试项 | 状态 | 对应能力 | 具体结论 |
|---|:---:|---|---|
| `basics/frozenset_binop.py` | ✅ 单项复测通过 | `frozenset` 二元集合运算 | 整组失败是串口偶发 first-EOF 超时；单项 896 个用例及后续复测通过，不是功能缺陷 |
| `basics/int_big_div.py` | ⚠️ 测试失败，疑似传输丢失 | 任意精度整数除法 | 2026-07-23 单项执行完成 2,220 个子用例但仍显示 FAIL。`.out` 在第 683 行附近直接跳到 `.exp` 约第 1206 行，缺少约 523 行、12.6 KB 输出，末尾又重新对齐；现有证据指向串口输出中途丢失，尚未发现明确除法结果错误。需用更可靠的输出方式复测后才能最终确认全部子用例 |
| `basics/int_big_mul.py` | ✅ 单项复测通过 | 任意精度整数乘法 | 整组失败是串口偶发 first-EOF 超时；单项 908 个用例及后续复测通过，不是功能缺陷 |
| `basics/weakref_callback_exception.py` | ✅ 功能正确，框架文本差异 | weakref 回调抛异常 | 回调顺序、异常和 GC 行为正确；raw REPL 显示 `File "<stdin>"`，而期望匹配测试文件名 |

### 4.3 跳过项

| 测试项 | 状态 | 影响 |
|---|:---:|---|
| `array_construct_endian.py` | ⏭ | array 指定端序构造未验证 |
| `attrtuple2.py` | ⏭ | attrtuple 的扩展场景未验证 |
| `bytearray_construct_endian.py` | ⏭ | bytearray 端序构造未验证 |
| `bytes_add_endian.py` | ⏭ | 不同端序数据拼接未验证 |
| `bytes_construct_endian.py` | ⏭ | bytes 端序构造未验证 |
| `class_inplace_op.py` | ⏭ | 类的一个原地运算场景未验证 |
| `exception_chain.py` | ⏭ | 异常链未验证 |
| `fun_code_colines.py` | ⏭ | code object 行号区间接口未验证 |
| `fun_code_full.py` | ⏭ | 完整 code object 属性未验证 |
| `fun_code_lnotab.py` | ⏭ | code object 行号表未验证 |
| `nanbox_smallint.py` | ⏭ | NaN-box small-int 平台实现细节未验证 |
| `struct_endian.py` | ⏭ | struct 端序专项未验证 |
| `subclass_native_call.py` | ⏭ | 原生类型子类 callable 场景未验证 |
| `subclass_native_init.py` | ⏭ | 原生类型子类初始化场景未验证 |
| `sys_path.py` | ⏭ | 自动脚本跳过；已在 REPL 验证 `sys.path == ['']` |

## 5. extmod：扩展模块

`extmod` 原完整测试执行了 77 个测试文件、1,049 个子用例，结果为 72 项通过、5 项失败、124 项跳过，运行时排除了 3 个需要 target wiring 的硬件测试。2026-07-23 启用 FAT LFN 并重新编译、烧录固件后，原来失败的 5 个 FAT VFS 测试已全部通过，共执行 72 个子用例。综合原完整测试和本次针对性复测，已执行项目的当前结论为 77 项通过、0 项失败、124 项跳过。

### 5.1 部分可用模块明细

| 模块 | API/能力 | 状态 |
|---|---|:---:|
| `binascii` | `a2b_base64` | ✅ |
| `binascii` | `b2a_base64` | ✅ |
| `binascii` | `hexlify` | ✅ |
| `binascii` | `unhexlify` | ✅ |
| `binascii` | `crc32` | ❌ 当前配置关闭 |
| `hashlib` | SHA-256 | ✅ |
| `hashlib` | digest/final 行为 | ✅ |
| `hashlib` | MD5 | ❌ 当前未提供 |
| `hashlib` | SHA-1 | ❌ 当前未提供 |

### 5.2 已执行项完整通过

- `json`：dump/dumps/load/loads、IOBase、separators、float、OrderedDict、bytes 输入、64 位整数等日志所列 13 项均为 ✅。
- `heapq`：基础测试 `heapq1.py` 为 ✅。

### 5.3 因模块未编入而全部跳过

- `asyncio`：33 个所列用例在模块可用性检查处跳过；当前 Makefile 未编译 `modasyncio.c`。
- `btree`：基础、关闭、错误、GC 用例跳过；当前 Makefile 未编译 `modbtree.c`。
- `cryptolib`：AES-128/AES-256 的 CBC/CTR/ECB 用例跳过；当前 Makefile 未编译 `modcryptolib.c`。
- `deflate`：压缩、解压、内存错误和流错误用例跳过；当前 Makefile 未编译 `moddeflate.c`。
- `framebuf`：像素格式、blit、边界、椭圆、多边形、滚动和子类用例跳过；当前 Makefile 未编译 `modframebuf.c`。
- `machine`：相关自动用例全部跳过；当前端口源码/配置进一步确认 Python `machine` 类没有实现，因此当前判定为 ❌，不是单纯“未验证”。

这些模块的跳过结果与当前构建源文件清单一致，因此可以判定为当前固件未编入，而不是单纯“测试结果不确定”。未来将对应源码和依赖加入构建后，必须重新运行相关测试，不能沿用本报告的不可用结论评价新固件。

### 5.4 其他已验证能力

- `marshal`：basic、嵌套函数和 stress 共 3 项全部通过。
- `platform`：基础接口通过。
- `random`：basic、extra 和浮点接口共 3 项通过；默认种子测试跳过，且当前没有硬件熵源。
- `re`：常用匹配、分组、命名字符类、位置、切分、替换、错误和栈溢出处理等 15 项通过；`re_stack_overflow2.py` 跳过。
- ticks/time 基础能力：`ticks_add`、`ticks_diff`、毫秒/微秒和分辨率测试通过。
- `uctypes`：21 项全部通过，覆盖 32 位大整数、地址、数组、字节访问、端序、浮点、指针、结构大小和布局。
- VFS：`vfs_basic`、FAT finaliser、FAT `ilistdir` 删除、mountinfo 和 userfs 等基础场景通过。

### 5.5 FAT VFS 的5个失败项

| 测试项 | 直接表现 | 结论 |
|---|---|---|
| `vfs_fat_fileio1.py` | ✅ 通过 | 文件和目录名称大小写、文件 I/O 行为符合预期 |
| `vfs_fat_fileio2.py` | ✅ 通过 | 目录中的长文件名创建和读写通过 |
| `vfs_fat_more.py` | ✅ 通过 | 成功创建并导入 `sys/test_module.py`，长文件名源码模块导入链路通过 |
| `vfs_fat_oldproto.py` | ✅ 通过 | 旧块设备协议、文件读写及文件名大小写行为通过 |
| `vfs_fat_ramdisk.py` | ✅ 通过 | RAM disk、文件、目录、`stat` 和异常行为通过 |

结论：当前 FAT VFS 的格式化、挂载、长文件名、文件读写、目录操作、重命名、`stat`、卸载、RAM 块设备以及长文件名源码模块导入链路均已通过。5 个专项测试共 72 个子用例全部通过。当前文件系统方面尚未完成的是启动时自动挂载的持久化块设备。

## 6. import：模块导入机制

### 6.1 可用能力

| 测试项 | 状态 | 说明 |
|---|:---:|---|
| `builtin_import.py` | ✅ | 内建 `__import__` 的已测基本行为可用 |
| `gen_context2.py` | ✅ | 一种生成器上下文导入场景可用 |
| `import1b.py` | ✅ | 一种基本导入变体可用 |
| `import_long_dyn2.py` | ✅ | 一种长动态导入变体可用 |
| `import_star_error.py` | ✅ | 星号导入错误处理场景可用 |
| `module_getattr.py` | ✅ | 模块级 `__getattr__` 场景可用 |
| `rel_import_inv.py` | ✅ | 非法相对导入处理可用 |

### 6.2 自动测试未能验证的能力

| 能力 | 失败测试 | 结论 |
|---|---|---|
| 外部/扩展内建模块导入 | `builtin_ext.py` | 自动测试失败，原因待复核 |
| 生成器上下文中的一种导入行为 | `gen_context.py` | 缺依赖模块，需重测 |
| 基本文件模块导入 | `import1a.py`、`import2a.py`、`import3a.py` | 测试依赖未部署；`/ram/mod.py` 对照已通过 |
| 损坏模块的导入处理 | `import_broken.py` | 缺测试包，需重测 |
| 循环导入 | `import_circular.py` | 缺测试包，需重测 |
| 长动态导入的一种场景 | `import_long_dyn.py` | 缺依赖模块，需重测 |
| 覆盖/替换导入机制 | `import_override.py`、`import_override2.py` | 缺依赖模块/包，需重测 |
| 包导入 | `import_pkg1.py` 至 `import_pkg9.py` | FAT LFN 已启用，但仍缺测试包，需重测 |
| `from ... import *` 正常路径 | `import_star.py` | 缺测试包，需重测 |
| 模块字典行为 | `module_dict.py` | 缺依赖模块，需重测 |
| 尝试导入模块的异常路径 | `try_module.py` | 缺依赖模块，需重测 |
| 文件导入专项 | `import_file.py` | ⏭，未验证 |

结论：当前 import 核心、普通单文件源码模块以及 FAT 长文件名源码模块均已验证可用，不需要重新实现 import。表中失败场景依赖的主机测试文件/包仍未部署到目标文件系统，因此它们是“当前自动测试环境未验证”，不能写成当前源码已确认失败。部署完整依赖树后，才能评价包、循环导入、`import *` 和 import override 是否完整可用。

## 7. float：浮点、复数和数学运算

正式 `float` 测试 66 项全部通过，覆盖：

- 浮点构造、解析、比较、真值、哈希、格式化、取整、幂、divmod；
- 整数与浮点互转，包括 64 位整数和任意精度整数；
- `inf`/`nan` 算术；
- `math` 常量、常用函数、特殊值、domain error、`factorial`、`isclose`；
- `cmath` 常用函数及特殊值；
- `complex` 构造、反向运算和特殊方法；
- 浮点 array 和 struct。

仅 `float/bytearray_construct_endian.py` 与 `float/bytes_construct_endian.py` 跳过，因此端序相关构造仍未验证。

## 8. micropython：MicroPython 专有能力

### 8.1 已验证可用

| 能力 | 状态 | 相关测试 |
|---|:---:|---|
| `micropython.const` | ✅ | const、类型、注解、错误、float、大整数、math 等 |
| 紧急异常与极端异常路径 | ✅ | `emg_exc`、`extreme_exc`、`incomplete_exc` |
| 堆锁及无堆分配场景 | ✅ | `heap_lock`、`heap_locked` 及多种 heapalloc 测试 |
| `micropython.schedule` | ✅ | 基础调度及 sleep 场景 |
| `RingIO` | ✅ | 基础及大 buffer 场景 |
| `stack_use` | ✅ | 通过 |
| `kbd_intr` | ✅ | 通过 |
| `execfile` | ✅ | 通过 |
| 无效 `.mpy` 处理 | ✅ | `import_mpy_invalid.py` 通过 |

### 8.2 已关闭或未验证

- native emitter：当前配置已关闭，对应闭包、常量、循环、函数属性、生成器、异常、with 等测试正常跳过。
- viper emitter：依赖 native 机器码发射能力，当前不可用；对应参数、算术、比较、指针读写、全局变量、异常、with 等测试正常跳过。
- 包含 native/viper/汇编机器码的 `.mpy` 导入及其 GC 行为不适用于当前配置，相关测试正常跳过；普通字节码 `.mpy` 文件导入尚未专项验证。
- `meminfo`、`memstats`、`opt_level`、`ringio_async` 等跳过。

因此 `micropython` 的已测常规辅助接口可用；native/viper 是明确关闭的可选功能，不属于结果不确定。

## 9. Unicode：Unicode 字符串

以下内存 Unicode 能力全部通过：基本字符串、`chr`、标识符、索引、迭代、`ord`、位置、切片、`format`、`%` 格式化、下标和 Unicode 正则相关用例。

`unicode/file1.py`、`file2.py`、`file_invalid.py` 全部跳过，所以 Unicode 文件读写和非法编码文件处理未验证。RAM FAT 普通文件 I/O 已可用，下一步应把这些测试数据部署到目标文件系统后重测。

## 10. feature_check：解释器能力探测

MicroPython 测试仓库明确说明：`feature_check` 目录“不包含真正的测试”，而是用于探测解释器特性并决定其他测试组的执行/排除。因此它显示的 16 个 `FAIL` 是“探测结果与参考输出不同”，不能简单计为 16 个产品缺陷。

特别是以下探测项虽显示 `FAIL`，但正式用例已证明对应能力存在：

| 探测项 | 探测结果 | 正式测试证据 | 最终判断 |
|---|:---:|---|---|
| async 语法 | ❌ | basics 中 async/await 系列通过 | async/await 语法可用；`asyncio` 模块当前未编入 |
| bytearray | ❌ | basics 中 bytearray 系列通过 | 可用 |
| complex | ❌ | float 中 complex/cmath 系列通过 | 可用 |
| const | ❌ | micropython 中 const 系列通过 | 可用 |
| set literal | ❌ | basics 中 set 系列通过 | 可用 |
| slice | ❌ | basics 中 slice 系列通过 | 可用 |

`int_64`、`int_big`、f-string、reverse ops、t-string 的探测通过。inlineasm 架构、REPL Emacs 键、coverage、target_info 等探测差异留作环境/构建配置信息，不作为 Python API 失败项。

## 11. 附录：Inline assembler 与非 Python API 测试

`inlineasm.txt` 结果为 0 tests performed。`feature_check` 中 RV32、RV32 Zba、Thumb、Thumb2、Xtensa 的 inlineasm 探测均显示差异，但这些只是架构探测，不是实际汇编指令测试。

inline assembler 的可靠结论是：**当前明确不支持，并已安全关闭**。此前 `@micropython.asm_thumb` 能编译，但调用最小 `nop()` 函数也会使 Cortex-M85/REPL 卡死；现已设置 `MICROPY_EMIT_INLINE_THUMB = 0`，所以本轮 0 项执行是预期行为。关闭 Python 动态 Thumb 汇编不影响 GCC 生成的 Thumb 固件、`.S` 文件或 C `__asm`。

其他不直接对应普通 Python API 的结果：

- `native_check.py` 跳过；当前配置确认 native emitter 已关闭。
- native/viper 大量用例跳过；当前代码生成后端不支持，不是普通待验证功能。
- REPL Emacs 键与单词移动探测失败，仅影响交互编辑体验，不应算作 Python API 失败。
- coverage 探测失败属于测试覆盖构建能力，不是用户 API 缺陷。
- `target_info` 探测失败说明目标信息输出与参考不匹配，应检查 `sys.implementation._mpy`、架构标识、build/thread 信息；它不等于浮点或 Unicode 不可用，因为正式测试已验证这些能力。

## 12. 最终可用性清单

### 可以使用（在本次覆盖范围内）

- Python 常用语法、函数、类、异常、生成器、上下文管理器；
- 常用内建函数及 `str`、`bytes`、`bytearray`、`list`、`tuple`、`dict`、`set`；
- 浮点、复数、`math`、`cmath`；
- 内存 Unicode 字符串；
- `io.BytesIO`、`io.StringIO`、基础 IOBase/缓冲写入；
- RAM FAT 格式化、挂载、文件/目录操作、卸载和符合 8.3 命名的 `.py` 模块导入；
- `json`、`heapq`；
- `binascii` 的 Base64/hex 接口；
- `hashlib.sha256`；
- `micropython.const`、堆控制、schedule、RingIO 等已测接口。

### 部分可用，使用时要避开已知问题

- 任意精度整数：乘法单项复测通过；除法单项测试因串口中途丢失约 523 行输出而失败，现有已接收结果未显示明确算法错误，但仍需可靠链路复测完整输出；
- `frozenset`：二元运算单项复测 896 个用例通过；整组曾有串口 first-EOF 超时；
- weakref：功能正确；自动测试仅因 raw REPL 回溯文件名为 `<stdin>` 而显示失败；
- `sys`：基础接口及 REPL 下的 `argv`、`path` 已验证；自动 `sys_path.py` 跳过；
- `binascii`：Base64/hex 接口可用，CRC32 当前配置关闭；
- `hashlib`：SHA-256 可用，MD5、SHA-1 当前未提供；
- `micropython`：普通辅助接口可用，native/viper 已关闭；
- `import`：8.3 单文件模块可用；LFN、包和复杂导入场景尚未完成有效复测；
- 文件系统：RAM FAT 和 FAT LFN 可用，但无启动时自动挂载的持久化块设备。

### 当前不能作为可用功能交付

- `network`/`socket`：没有协议栈；
- `machine`：没有 Python Pin、UART、I2C、SPI、Timer、ADC、PWM 等类；
- `asyncio`、`btree`、`cryptolib`、`deflate`、`framebuf`：当前构建未编入；
- `select`、`ssl`、`tls`、`websocket`：当前构建未编入；
- Python 动态 Thumb inline assembler、native emitter 和 viper emitter；
- 尚未复测包语义的文件系统应用。

### 本轮未完成有效验证的功能

以下功能因缺少依赖文件而没有完成有效测试，现有结果不足以确认它们是否可用：

- Unicode 文件 I/O；
- 普通字节码 `.mpy` 文件导入。

`asyncio`、`btree`、`cryptolib`、`deflate`、`framebuf`、`machine` 板级外设、`network`/`socket`、Inline Thumb、native 和 viper 不列在这里，因为它们在当前固件中是未编入、尚未实现或主动关闭，并非测试结果不确定。

## 13. 建议的下一轮最小补测

按优先级执行以下最小集合：

1. **使用更可靠的串口输出方式重新运行 `basics/int_big_div.py`。** 2026-07-23 单项结果已确认中途缺少约 523 行输出；应降低波特率、修复/缓冲 UART TX，或把测试拆分为更小批次，再确认 2,220 个子用例的完整输出。不要把当前由输出丢失造成的 FAIL 直接归因于大整数除法算法。
2. **补齐 import 测试环境后重跑 import 组。** FAT LFN 已启用并通过专项复测；下一步是准备能够跨 MicroPython 软复位保留内容并在启动时自动挂载的 VFS，再将测试依赖的模块、包和目录结构完整部署到该 VFS。临时 Python `bytearray` RAM FAT 会在测试框架的逐项软复位中丢失，不能直接用于整组 import 测试。测试依赖缺失时的失败不能用于评价 importer。
3. **在同一 VFS 环境补跑 3 个 Unicode 文件测试和普通字节码 `.mpy` 导入。** 包含 native/viper/汇编机器码的 `.mpy` 不适用于当前配置，无需补测。
以下项目不属于“下一轮最小补测”：`network`/`socket`、`machine` 当前尚未实现；Inline Thumb、native、viper 当前主动关闭。只有项目需求决定实现这些能力后，才安排对应移植和测试。
