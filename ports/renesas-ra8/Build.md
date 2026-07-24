# 编译

## 前置条件

- e2studio 2025-12
- FSP Version 6.4.0
- arm-none-eabi-gcc 13.2.1.arm-13-7
- python 3.13
- 对于 windows 平台，建议使用 w64devkit 提供的 make 命令：[w64devkit: Portable C and C++ Development Kit for x64 (and x86) Windows](https://github.com/skeeto/w64devkit)

## 步骤

- 预编译 Python 文件为字节码：在项目根目录执行 `make -C mpy-cross`
- 更新 git 子模块：`git submodule update --init --recursive`
- 导入板子下的 e2studio 工程，工程同样位于 `ports\renesas-ra8\boards\<board_name>` 目录下。例如，对于 CPKCOR-RA8P1 板，e2studio 工程位于 `ports\renesas-ra8\boards\CPKCOR_RA8P1\e2studio_gcc_freertos`。导入后，打开 `configuration.xml`，点击 `Generate Project Content`。

### 使用 e2studio 构建

- 指定编译器路径。在 `ports/renesas-ra8` 目录下创建 `local.mk`，里面写 E2S_GCC 的路径

```
E2S_GCC ?= D:/Programs/Dev/e2s_2025_12/toolchains/gcc_arm/13.2.rel1
```

- 生成 MicroPython QSTR 相关头文件

```
make BOARD=CPKCOR_RA8P1 genhdr
```

> 如果使用了 make 进行构建，那么在不删除 make 的输出目录的情况下，可以不执行此命令。make 的构建同样会生成 QSTR 相关头文件。

- 现在可以回到 e2studio 中进行构建。生成的 hex 文件位于 e2studio 的 Debug 目录下

### 使用 make 构建

- 指定编译器路径。在 `ports/renesas-ra8` 目录下创建 `local.mk`，里面写 E2S_GCC 的路径

```
E2S_GCC ?= D:/Programs/Dev/e2s_2025_12/toolchains/gcc_arm/13.2.rel1
```

- 在 `ports/renesas-ra8` 目录执行

```
make BOARD=CPKCOR_RA8P1
```

生成的 hex 文件位于 `build-<board_name>` 目录下

> 对于 windows，默认使用单线程编译，可以使用 `-j` 选项使用多线程编译。例如，使用 16 个线程进行编译：`-j16`

- 生成编译数据库。为便于代码浏览与使用 Intelli Sence，Makefile 中提供了一个假目标，用于生成 `compile_commands.json`

```
# 安装 python 包
pip install compiledb

# 在 ports/renesas-ra8 目录执行
make BOARD=CPKCOR_RA8P1 -j16
```

## 测试

micropython 提供了对固件功能的测试。

- 安装所需的包

```
pip install pyserial
```

- 进入 `tests` 目录，执行

```
python run-tests.py -t COM10 -b 2000000 --test-dirs basics
```

> `COM10` 是电脑的串口号，不同的电脑可能不一样，需要根据设备管理器显示内容进行修改
>
> `--test-dirs` 是要执行的测试脚本的目录，当前选择 basics，可以选择在 `tests` 文件夹下的其它目录