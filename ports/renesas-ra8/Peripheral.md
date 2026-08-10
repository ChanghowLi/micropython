# machine

此类参考：[MicroPython machine class](https://docs.micropython.org/en/latest/library/machine.html) 

## 内存访问

### machine.mem8()

按字节读写内存上的地址

### machine.mem16()

按半字读写内存上的地址

### machine.mem32()

按字读写内存上的地址

### machine.mem_backup(region=0)

返回一个不受软复位影响的内存。当前只提供一个 backup mem，且仅不受软复位影响，硬复位和断电会清除状态

```python
# 有参调用和无参调用会返回同一块内存，虽然是不同的对象
backup_a = machine.mem_backup()
backup_b = machine.mem_backup(0)

# 这种调用会抛出异常 ValueError
backup_c = machine.mem_backup(1)
```

## 复位相关

### machine.reset()

硬件复位，要求效果相当于按下 RESET 按键

### machine.soft_reset()

软件复位

### machine.reset_cause()

获取复位原因

### machine.bootloader([value])

复位进入 Bootloader

## 中断相关

### machine.disable_irq()

全局关中断

### machine.enable_irq()

全局恢复中断

## 电源相关

### machine.freq()

返回以 Hz 为单位的 CPU 时钟频率。当前仅支持查询，不支持在运行时设置频率。

### machine.idle()

执行 `__WFI()` 进入等待中断状态，并在中断发生后继续执行。

### machine.lightsleep([time_ms])

### machine.deepsleep([time_ms])

看描述是进入更低功耗的状态，MCU 一般都有多级的低功耗设定，这个状态关掉的模块会更多

### machine.wake_reason()

获取唤醒原因。当前仅完成接口框架，在低功耗唤醒检测接入前返回 `machine.WAKE_UNKNOWN`。

## 其它功能

### machine.unique_id()

获取 MCU 的唯一 ID，并以 `bytes` 对象返回。

### machine.time_pulse_us(pin,pulse_level,timeout_us=1000000,/)

测量指定引脚上的脉冲时间，并返回脉冲持续时间（单位为微秒）

### machine.bitstream(pin,encoding,timing,data,/)

通过位拼接方式传输指定引脚的数据。编码参数用于指定比特的编码方式，而时序则是针对特定编码的时序规范。

### machine.rng()

使用 RSIP 硬件随机数生成器返回一个 24 位随机数。

---

API 完成情况。函数说明写在上方每个函数的标题下

| 函数                    | 状态 |
| ----------------------- | ---- |
| machine.mem8()          | ✅    |
| machine.mem16()         | ✅    |
| machine.mem32()         | ✅    |
| machine.mem_backup()    | ✅    |
| machine.reset()         | ✅    |
| machine.soft_reset()    | ✅    |
| machine.reset_cause()   | ✅    |
| machine.bootloader()    | ❌    |
| machine.disable_irq()   | ✅    |
| machine.enable_irq()    | ✅    |
| machine.freq()          | ✅（仅查询） |
| machine.idle()          | ✅    |
| machine.lightsleep()    | ❌    |
| machine.deepsleep()     | ❌    |
| machine.wake_reason()   | ⚠️（仅接口框架） |
| machine.wake_pins()     | ❌    |
| machine.unique_id()     | ✅    |
| machine.time_pulse_us() | ❌    |
| machine.bitstream()     | ❌    |
| machine.rng()           | ✅    |

## class Pin

参考：[MicroPython class Pin](https://docs.micropython.org/en/latest/library/machine.Pin.html) 

需要检查原理图，有些 Pin 已经用来连接其它器件了，这些 Pin 不允许再操作。

验证方法：选几个 Pin，能正常用 Python 操作电平即可

### machine.Pin(id,mode=-1,pull=-1,*,value=None,drive=0,alt=-1)

参数：

- id: int、字符串或由端口号和引脚号组成的元组。RA8 上使用字符串时，比如 `'P006'`
- mode
    - Pin.IN: 配置为输入
    - Pin.OUT: 配置为输出
    - Pin.OPEN_DRAIN: 配置为开漏
    - Pin.ALT: 配置为复用功能
    - Pin.ALT_OPEN_DRAIN: 配置为复用开漏
    - Pin.ANALOG: 配置为模拟输入
- pull
    - None: 无上下拉
    - Pin.PULL_UP: 内部上拉
    - Pin.PULL_DOWN: 内部下拉，RA8P1 当前不支持
- value: 仅在 `Pin.OUT` 和 `Pin.OPEN_DRAIN` 模式下有效，用于设置引脚的初始输出值。未指定时保持引脚原有状态
- drive: 设置引脚的输出驱动能力，可使用 `Pin.DRIVE_0`、`Pin.DRIVE_1` 等常量。数值越大，驱动能力越强，具体能力由端口决定
- alt: 设置引脚的复用功能，仅在 `Pin.ALT` 和 `Pin.ALT_OPEN_DRAIN` 模式下有效，可用值由端口决定

未指定的参数会保持引脚原有状态。配置为复用功能后，除重新调用构造函数或 `Pin.init()` 外，其它 `Pin` 方法的行为未定义。

当前 RA8P1 实现状态：

- 当前仅注册 `P000`，尚未接入 `pins.csv` 自动生成流程
- 支持 `Pin.IN`、`Pin.OUT`
- 支持 `Pin.PULL_NONE`、`Pin.PULL_UP`；RA8P1 的 PFS/FSP 未提供内部下拉配置
- 支持仅限关键字传入的 `value=` 和 `drive=`
- 支持 `Pin.DRIVE_0`、`Pin.DRIVE_1`、`Pin.DRIVE_2`、`Pin.DRIVE_3`
- 当前 `Pin.OUT` 未指定 `value` 时使用 FSP 的默认低电平配置，尚未实现“保持原输出值”
- 尚未支持 `Pin.OPEN_DRAIN`、`Pin.ALT`、`Pin.ALT_OPEN_DRAIN`、`Pin.ANALOG` 和 `alt=`
- `Pin("P000")`、类常量以及 `mode`、`pull`、`value`、`drive` 参数解析已完成 REPL 验证
- P000 的实际输入输出电平、内部上拉效果和各档驱动能力尚待万用表或其它仪器验证

### Pin.init(mode=-1,pull=-1,*,value=None,drive=0,alt=-1)

使用指定参数重新初始化引脚。未指定的参数保持原有状态，参数含义与构造函数相同。返回 `None`。

### Pin.value([x])

读取或设置引脚的数字电平：

- 不传入 `x` 时，读取引脚电平，低电平返回 `0`，高电平返回 `1`
- 传入 `x` 时，根据其布尔值设置输出电平：真值设置为 `1`，假值设置为 `0`，并返回 `None`

读取和设置的具体行为与当前引脚模式有关：

- Pin.IN: 读取引脚的实际输入电平；写入值仅保存到输出缓冲区，引脚仍保持高阻态
- Pin.OUT: 写入值会立即更新输出电平；读取行为未定义
- Pin.OPEN_DRAIN: 写入 `0` 时输出低电平，写入 `1` 时进入高阻态；高阻态下可读取引脚的实际输入电平

### Pin.__call__([x])

`Pin` 对象可直接调用，作用与 `Pin.value([x])` 相同，是快速读取或设置引脚电平的简写。

### Pin.on()

将引脚输出电平设置为 `1`。

### Pin.off()

将引脚输出电平设置为 `0`。

### Pin.irq(handler=None,trigger=Pin.IRQ_FALLING\|Pin.IRQ_RISING,*,priority=1,wake=None,hard=False)

配置引脚中断，并返回中断回调对象。

参数：

- handler: 中断触发时调用的函数。该函数必须接收一个参数，即触发中断的 `Pin` 对象；设置为 `None` 可禁用回调
- trigger: 中断触发条件，多个条件可以使用按位或组合
    - Pin.IRQ_FALLING: 下降沿触发
    - Pin.IRQ_RISING: 上升沿触发
    - Pin.IRQ_LOW_LEVEL: 低电平触发
    - Pin.IRQ_HIGH_LEVEL: 高电平触发
- priority: 中断优先级，可用范围由端口决定，数值越大优先级越高
- wake: 指定该中断可以从哪些低功耗模式唤醒系统，可使用 `machine.IDLE`、`machine.SLEEP`、`machine.DEEPSLEEP` 或它们的按位或组合
- hard: 为 `True` 时使用硬中断。硬中断延迟更低，但中断处理函数中不能分配内存；并非所有端口都支持此参数

当前 RA8P1 端口支持安全引脚使用的 IRQ0～IRQ19、IRQ21～IRQ31 通道，支持下降沿、上升沿、双边沿和低电平触发。当前 FSP ICU 驱动不支持高电平触发，使用 `Pin.IRQ_HIGH_LEVEL` 时抛出 `ValueError`。低功耗唤醒尚未实现，`wake` 仅支持 `None`，其它值抛出 `NotImplementedError`。

以下方法不属于 `Pin` 核心 API，仅部分端口提供。

### Pin.low()

将引脚输出电平设置为 `0`，作用与 `Pin.off()` 相同。

### Pin.high()

将引脚输出电平设置为 `1`，作用与 `Pin.on()` 相同。

### Pin.mode([mode])

读取或设置引脚模式，`mode` 参数含义与构造函数相同。

### Pin.pull([pull])

读取或设置引脚的上下拉状态，`pull` 参数含义与构造函数相同。

### Pin.drive([drive])

读取或设置引脚的输出驱动能力，`drive` 参数含义与构造函数相同。

### Pin.toggle()

翻转引脚的输出电平，即在 `0` 和 `1` 之间切换。

### Pin.board

包含以开发板丝印或原理图名称命名的引脚，例如 `Pin.board.LED`。可使用 `help(Pin.board)` 查看当前开发板提供的名称。

### Pin.cpu

包含以芯片数据手册名称命名的引脚，例如 `Pin.cpu.P006`。可使用 `help(Pin.cpu)` 查看当前芯片提供的名称。

### Pin 常量

- 引脚模式：`Pin.IN`、`Pin.OUT`、`Pin.OPEN_DRAIN`、`Pin.ALT`、`Pin.ALT_OPEN_DRAIN`、`Pin.ANALOG`
- 上下拉配置：`Pin.PULL_UP`、`Pin.PULL_DOWN`，不启用上下拉时使用 `None`
- 输出驱动能力：`Pin.DRIVE_0`、`Pin.DRIVE_1`、`Pin.DRIVE_2` 等，端口可能提供更多等级
- 中断触发类型：`Pin.IRQ_FALLING`、`Pin.IRQ_RISING`、`Pin.IRQ_LOW_LEVEL`、`Pin.IRQ_HIGH_LEVEL`

---

API 完成情况。函数说明写在上方每个函数的标题下

状态说明：✅ 已实现并验证；🟡 部分实现或尚待板端验证；❌ 未实现。

| 函数或属性       | 状态 |
| ---------------- | ---- |
| machine.Pin()    | 🟡 `IN`、`OUT`、`OPEN_DRAIN`、`ANALOG`、`ALT`、`ALT_OPEN_DRAIN` 及 `Pin.cpu`/`Pin.board` 对象已通过 REPL 验证；逐引脚 PSEL 合法与非法配置路径已通过板端验证，ALT 实际外设信号及 GPIO 电气行为待验证 |
| Pin.init()       | 🟡 支持 6 种引脚模式及 `pull`、`value`、`drive`、`alt`；配置、模式读回及逐引脚 PSEL 合法与非法路径已通过板端验证，ALT 实际外设信号待验证 |
| Pin.value()      | ✅    |
| Pin.__call__()   | ✅    |
| Pin.on()         | ✅    |
| Pin.off()        | ✅    |
| Pin.irq()        | 🟡 已实现IRQ对象、回调启停、下降沿/上升沿/双边沿/低电平、优先级及软/硬中断；IRQ0已通过真实引脚验证，IRQ1已通过独立实例配置验证；`wake`和高电平触发暂不支持，其余通道尚未逐个板端验证 |
| Pin.low()        | ✅    |
| Pin.high()       | ✅    |
| Pin.mode()       | 🟡 已通过 REPL 验证 6 种模式的查询及从 ALT 切回 GPIO；设置 ALT 时须使用 `Pin.init(..., alt=...)` |
| Pin.pull()       | ✅ 支持 `PULL_NONE`、`PULL_UP` 和 `None`；RA8P1 硬件不支持 `PULL_DOWN` |
| Pin.drive()      | ✅ 支持并验证 `DRIVE_0`～`DRIVE_3` |
| Pin.toggle()     | ✅    |
| Pin.board        | ✅ 已生成并通过对象一致性测试；当前板级名称仍与 CPU 引脚名相同 |
| Pin.cpu          | ✅ 已生成并通过对象一致性测试 |

## class SPI

参考：[MicroPython class SPI](https://docs.micropython.org/en/latest/library/machine.SPI.html)

SPI 是由控制器驱动的同步串行通信协议。总线通常包含 `SCK`、`MOSI` 和 `MISO` 三根信号线。多个设备可以共用同一总线，但每个设备需要独立的 `CS` 片选信号，片选信号由用户通过 `machine.Pin` 控制。

RA8P1 有 2 个 SPI 和 9 个 SCI（SCI9 已被 REPL 占用），对 Python 代码来说，先从专用的 SPI 开始分配资源，然后再到 SCI。需要注意看原理图，不要影响到其它板子已经连接的器件，如果某个外设会影响到现有连接，则对应的外设不允许 Python 调用。对 SCI 来说还需要注意与 IIC、UART 的共享。

验证方式：需要与另一个板子进行通信，或者开多个外设，互相传数据

### machine.SPI(id,...)

创建指定硬件总线上的 SPI 对象。

参数：

- id: SPI 外设编号，可用值由端口和硬件决定，通常使用 `0`、`1` 等编号
- 其它参数: 与 `SPI.init()` 的初始化参数相同

未传入其它参数时，只创建 SPI 对象而不重新初始化总线，保留该总线上一次初始化后的配置。

### machine.SoftSPI(baudrate=500000,*,polarity=0,phase=0,bits=8,firstbit=SoftSPI.MSB,sck=None,mosi=None,miso=None)

创建并初始化软件 SPI 对象。软件 SPI 通过 GPIO 模拟时序，可以使用任意合适的引脚，但效率通常低于硬件 SPI。

参数含义与 `SPI.init()` 相同，通常至少需要指定 `sck`、`mosi` 和 `miso`。

### SPI.init(baudrate=1000000,*,polarity=0,phase=0,bits=8,firstbit=SPI.MSB,sck=None,mosi=None,miso=None,pins=(SCK,MOSI,MISO))

使用指定参数初始化 SPI 总线。

参数：

- baudrate: SCK 时钟频率，单位为 Hz。硬件实际产生的频率可能低于请求值
- polarity: 时钟极性，可取 `0` 或 `1`，表示 SCK 空闲时的电平
- phase: 时钟相位，可取 `0` 或 `1`，分别表示在第一个或第二个时钟边沿采样数据
- bits: 每次传输的数据位宽，所有硬件至少应支持 `8`
- firstbit: 数据位传输顺序，可取 `SPI.MSB` 或 `SPI.LSB`
- sck: 用作 SPI 时钟信号的 `machine.Pin` 对象
- mosi: 用作控制器输出、外设输入信号的 `machine.Pin` 对象
- miso: 用作控制器输入、外设输出信号的 `machine.Pin` 对象
- pins: WiPy 端口使用的 `(SCK, MOSI, MISO)` 引脚元组，用于替代 `sck`、`mosi` 和 `miso` 参数

硬件 SPI 可使用的引脚通常由外设固定，部分硬件可能提供几组可选引脚；软件 SPI 可以使用任意合适的引脚。

### SPI.deinit()

关闭 SPI 总线。

### SPI.read(nbytes,write=0x00)

读取 `nbytes` 个字节。读取期间，每接收一个字节都会发送一次 `write` 指定的字节。返回包含接收数据的 `bytes` 对象。

### SPI.readinto(buf,write=0x00)

将接收的数据写入 `buf`，读取长度由缓冲区长度决定。读取期间持续发送 `write` 指定的字节。返回 `None`。

### SPI.write(buf)

发送 `buf` 中的数据。返回 `None`。

### SPI.write_readinto(write_buf,read_buf)

发送 `write_buf` 中的数据，同时将接收的数据写入 `read_buf`。两个缓冲区可以是同一个对象，也可以是不同对象，但长度必须相同。返回 `None`。

### SPI 常量

- `SPI.MSB`、`SoftSPI.MSB`: 最高有效位先传输
- `SPI.LSB`、`SoftSPI.LSB`: 最低有效位先传输
- `SPI.CONTROLLER`: 将 SPI 总线配置为控制器，仅 WiPy 端口使用

---

API 完成情况。函数说明写在上方每个函数的标题下

| 函数或常量                 | 状态 |
| -------------------------- | ---- |
| machine.SPI()              | ❌    |
| machine.SoftSPI()          | ❌    |
| SPI.init()                 | ❌    |
| SPI.deinit()               | ❌    |
| SPI.read()                 | ❌    |
| SPI.readinto()             | ❌    |
| SPI.write()                | ❌    |
| SPI.write_readinto()       | ❌    |
| SPI.MSB                    | ❌    |
| SPI.LSB                    | ❌    |
| SPI.CONTROLLER             | ❌    |

## class I2C

参考：[MicroPython class I2C](https://docs.micropython.org/en/latest/library/machine.I2C.html)

I2C 是一种双线串行通信协议，由时钟线 `SCL` 和数据线 `SDA` 组成。`SCL` 和 `SDA` 均需要连接上拉电阻，常见阻值为 1～10 kΩ。

RA8P1 有 2 个 IIC（IIC0 已连接板子上其它器件，不允许 Python 调用） 和 9 个 SCI（SCI9 已被 REPL 占用），对 Python 代码来说，先从专用的 SPI 开始分配资源，然后再到 SCI。需要注意看原理图，不要影响到其它板子已经连接的器件，如果某个外设会影响到现有连接，则对应的外设不允许 Python 调用。对 SCI 来说还需要注意与 SPI、UART 的共享。

验证方式：需要另一个板子来互相通信，或者开多个外设互相通信

### machine.I2C(id,*,scl,sda,freq=400000,timeout=50000)

创建并返回指定硬件总线上的 I2C 对象。

参数：

- id: I2C 外设编号，可用值由端口和开发板决定
- scl: 用作 SCL 时钟线的 `machine.Pin` 对象
- sda: 用作 SDA 数据线的 `machine.Pin` 对象
- freq: SCL 的最大时钟频率，单位为 Hz
- timeout: I2C 事务允许使用的最长时间，单位为微秒；部分端口不支持此参数

部分端口允许修改 `scl` 和 `sda`，部分端口则使用固定引脚或提供默认值。

### machine.SoftI2C(scl,sda,*,freq=400000,timeout=50000)

创建并初始化软件 I2C 对象。软件 I2C 通过 GPIO 模拟时序，可以使用任意合适的引脚，但效率通常低于硬件 I2C。

参数：

- scl: 用作 SCL 时钟线的 `machine.Pin` 对象
- sda: 用作 SDA 数据线的 `machine.Pin` 对象
- freq: SCL 的最大时钟频率，单位为 Hz
- timeout: 等待时钟拉伸的最长时间，单位为微秒。超时后抛出 `OSError(ETIMEDOUT)` 异常

### I2C.init(scl,sda,*,freq=400000)

使用指定参数初始化 I2C 总线。

参数：

- scl: 用作 SCL 时钟线的 `machine.Pin` 对象
- sda: 用作 SDA 数据线的 `machine.Pin` 对象
- freq: SCL 时钟频率，单位为 Hz。硬件实际产生的频率可能低于请求值

### I2C.deinit()

关闭 I2C 总线。

### I2C.scan()

扫描 `0x08` 到 `0x77` 范围内的 7 位 I2C 地址，返回所有响应设备的地址列表。

以下原始总线操作仅由 `machine.SoftI2C` 提供，可组合实现自定义 I2C 事务。

### I2C.start()

在总线上产生 START 起始条件，即 SCL 为高电平时 SDA 从高电平变为低电平。

### I2C.stop()

在总线上产生 STOP 停止条件，即 SCL 为高电平时 SDA 从低电平变为高电平。

### I2C.readinto(buf,nack=True,/)

从总线读取数据并写入 `buf`，读取长度由缓冲区长度决定。除最后一个字节外，每个字节接收后都会发送 ACK；最后一个字节接收后，根据 `nack` 发送 NACK 或 ACK。

### I2C.write(buf)

向总线发送 `buf` 中的数据，并检查每个字节对应的 ACK。收到 NACK 后停止发送剩余字节，返回收到的 ACK 数量。

以下方法用于与指定地址的 I2C 设备进行标准读写操作。

### I2C.readfrom(addr,nbytes,stop=True,/)

从地址为 `addr` 的设备读取 `nbytes` 个字节。`stop` 为 `True` 时在传输结束后产生 STOP 条件。返回包含接收数据的 `bytes` 对象。

### I2C.readfrom_into(addr,buf,stop=True,/)

从地址为 `addr` 的设备读取数据并写入 `buf`，读取长度由缓冲区长度决定。`stop` 为 `True` 时在传输结束后产生 STOP 条件。返回 `None`。

### I2C.writeto(addr,buf,stop=True,/)

向地址为 `addr` 的设备发送 `buf` 中的数据。收到 NACK 后停止发送剩余字节；`stop` 为 `True` 时，无论是否收到 NACK，都会在传输结束后产生 STOP 条件。返回收到的 ACK 数量。

### I2C.writevto(addr,vector,stop=True,/)

向地址为 `addr` 的设备依次发送 `vector` 中多个缓冲区的数据，设备地址只发送一次。`vector` 应为由缓冲区对象组成的元组或列表，其中可以包含长度为零的对象。收到 NACK 后停止发送剩余内容，返回收到的 ACK 数量。

以下方法用于访问具有寄存器或存储器地址的 I2C 设备。

### I2C.readfrom_mem(addr,memaddr,nbytes,*,addrsize=8)

从地址为 `addr` 的设备中读取数据，从内部地址 `memaddr` 开始读取 `nbytes` 个字节。`addrsize` 指定内部地址的位数。返回包含接收数据的 `bytes` 对象。

### I2C.readfrom_mem_into(addr,memaddr,buf,*,addrsize=8)

从地址为 `addr` 的设备内部地址 `memaddr` 开始读取数据，并写入 `buf`。读取长度由缓冲区长度决定，`addrsize` 指定内部地址的位数。返回 `None`。

### I2C.writeto_mem(addr,memaddr,buf,*,addrsize=8)

从地址为 `addr` 的设备内部地址 `memaddr` 开始写入 `buf` 中的数据。`addrsize` 指定内部地址的位数。返回 `None`。

---

API 完成情况。函数说明写在上方每个函数的标题下

| 函数                       | 状态 |
| -------------------------- | ---- |
| machine.I2C()              | ❌    |
| machine.SoftI2C()          | ❌    |
| I2C.init()                 | ❌    |
| I2C.deinit()               | ❌    |
| I2C.scan()                 | ❌    |
| I2C.start()                | ❌    |
| I2C.stop()                 | ❌    |
| I2C.readinto()             | ❌    |
| I2C.write()                | ❌    |
| I2C.readfrom()             | ❌    |
| I2C.readfrom_into()        | ❌    |
| I2C.writeto()              | ❌    |
| I2C.writevto()             | ❌    |
| I2C.readfrom_mem()         | ❌    |
| I2C.readfrom_mem_into()    | ❌    |
| I2C.writeto_mem()          | ❌    |

## class UART

RA8P1 的 UART 全部由 SCI 实现，共有 SCI0 到 SCI9 共 10 个 SCI 通道，其中 SCI9 已被 REPL 占用。使用前必须检查开发板原理图，避免影响板上已经连接的器件；如果某个 SCI 通道或其引脚会影响现有连接，则该外设不允许由 Python 调用。SCI 通道还与 SPI、IIC 功能共享，同一个 SCI 通道在同一时间只能配置为 UART、SPI 或 IIC 中的一种模式。

参考：[MicroPython class UART](https://docs.micropython.org/en/latest/library/machine.UART.html)

UART 用于实现异步全双工串行通信，通常使用 `TX` 和 `RX` 两根信号线。UART 对象同时也是 Stream 对象，可以使用标准的流读取和写入方法。

### machine.UART(id,...)

创建指定编号的 UART 对象。

参数：

- id: UART 编号。对于 RA8P1，该编号对应可用于 UART 的 SCI 通道
- 其它参数: 与 `UART.init()` 的初始化参数相同

### UART.init(baudrate=9600,bits=8,parity=None,stop=1,*,...)

使用指定参数初始化或重新配置 UART。可以多次调用 `init()`，在不释放对象的情况下修改 UART 配置。

参数：

- baudrate: 通信波特率
- bits: 每个字符的数据位数，常见值为 `7`、`8` 或 `9`，实际支持范围由端口决定
- parity: 奇偶校验。`None` 表示无校验，`0` 表示偶校验，`1` 表示奇校验
- stop: 停止位数量，通常为 `1` 或 `2`
- tx: 用作 TX 信号的 `machine.Pin` 对象
- rx: 用作 RX 信号的 `machine.Pin` 对象
- rts: 用作 RTS 硬件流控制输出的 `machine.Pin` 对象
- cts: 用作 CTS 硬件流控制输入的 `machine.Pin` 对象
- txbuf: 发送缓冲区的字符数量
- rxbuf: 接收缓冲区的字符数量
- timeout: 等待第一个字符的最长时间，单位为毫秒
- timeout_char: 等待相邻字符的最长时间，单位为毫秒
- invert: 指定需要反相的信号，可以使用 `UART.INV_TX`、`UART.INV_RX` 或它们的按位或组合
- flow: 指定硬件流控制信号，可以使用 `UART.RTS`、`UART.CTS` 或它们的按位或组合；设置为 `0` 时禁用硬件流控制

可用的 SCI 通道、引脚、数据位数、流控制和反相功能由具体芯片及开发板配置决定。

### UART.deinit()

关闭 UART。调用后不能对同一个对象再次调用 `init()`，如需重新使用该 UART，应重新创建一个 UART 对象。

### UART.any()

返回当前无需阻塞即可读取的字符数量。没有可读字符时返回 `0`；有数据时返回正整数，但该值可能只返回 `1`，即使缓冲区中还有更多字符。

### UART.read([nbytes])

读取 UART 数据。指定 `nbytes` 时最多读取该数量的字节；未指定时尽可能读取当前可用的数据。返回包含接收数据的 `bytes` 对象，等待超时时返回 `None`。

### UART.readinto(buf[,nbytes])

读取 UART 数据并写入 `buf`。指定 `nbytes` 时最多读取该数量的字节；未指定时最多读取 `len(buf)` 个字节。返回实际读取的字节数，等待超时时返回 `None`。

### UART.readline()

读取一行数据，通常在读取到换行符时结束。返回读取到的字节数据，等待超时时返回 `None`。

### UART.write(buf)

发送 `buf` 中的数据。返回实际发送的字节数，等待超时时可能返回 `None`。

### UART.sendbreak()

发送 Break 条件，使 TX 信号保持低电平，持续时间长于正常发送一个字符所需的时间。

### UART.flush()

等待所有待发送数据发送完成。等待超时时抛出异常。

### UART.txdone()

检查发送是否完成。没有正在进行的发送或全部数据已经发送时返回 `True`，否则返回 `False`。

### UART.irq(handler=None,trigger=0,hard=False)

配置 UART 中断回调，并返回中断对象。

参数：

- handler: 中断触发时调用的函数。该函数必须接收一个参数，即触发中断的 `UART` 对象；设置为 `None` 可禁用回调
- trigger: 中断触发条件，多个条件可以使用按位或组合
    - UART.IRQ_RXIDLE: 接收到至少一个字符后，RX 信号进入空闲状态时触发
    - UART.IRQ_RX: 每接收到一个字符时触发
    - UART.IRQ_TXIDLE: 最后一个或最后几个字符正在发送或已经发送时触发
    - UART.IRQ_BREAK: RX 检测到 Break 条件时触发
- hard: 为 `True` 时使用硬中断。硬中断延迟更低，但中断处理函数中不能分配内存

具体可用的中断触发条件由端口和硬件决定。

### UART 常量

- 硬件流控制：`UART.RTS`、`UART.CTS`
- 信号反相：`UART.INV_TX`、`UART.INV_RX`
- 中断触发条件：`UART.IRQ_RXIDLE`、`UART.IRQ_RX`、`UART.IRQ_TXIDLE`、`UART.IRQ_BREAK`

---

API 完成情况。函数说明写在上方每个函数的标题下

| 函数或常量            | 状态 |
| --------------------- | ---- |
| machine.UART()        | ❌    |
| UART.init()           | ❌    |
| UART.deinit()         | ❌    |
| UART.any()            | ❌    |
| UART.read()           | ❌    |
| UART.readinto()       | ❌    |
| UART.readline()       | ❌    |
| UART.write()          | ❌    |
| UART.sendbreak()      | ❌    |
| UART.flush()          | ❌    |
| UART.txdone()         | ❌    |
| UART.irq()            | ❌    |
| UART.RTS              | ❌    |
| UART.CTS              | ❌    |
| UART.INV_TX           | ❌    |
| UART.INV_RX           | ❌    |
| UART.IRQ_RXIDLE       | ❌    |
| UART.IRQ_RX           | ❌    |
| UART.IRQ_TXIDLE       | ❌    |
| UART.IRQ_BREAK        | ❌    |
