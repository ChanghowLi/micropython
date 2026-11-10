# 摄像头首次出图（C 层）

当前新增的显示测试仅支持 DVP、RGB565，先用 640×480。OV5640 的 MIPI/VIN 路径不属于此测试；不能把 MIPI 模组接到 DVP 引脚使用。无需先封装 MicroPython 接口。

## 接线前

确认实际模组型号、DVP/MIPI 接口和开发板版本。按照原理图核对供电、GND、SCCB 的 SCL/SDA、XCLK、PCLK、VSYNC、HREF、D0–D7，以及复位和掉电控制脚。驱动中的电源控制脚和时钟来源也必须与实际硬件一致。不要仅按模组名称猜针脚或电压。

## 软件调用顺序

1. 在 e2studio 的工程预处理宏中设置 `TEST_EN_CAMERA=1`（或修改 `src/test/test.h` 中默认值）。单帧测试默认开启，性能和压力测试默认关闭。
2. 在 FreeRTOS 任务中完成板级时钟、引脚、SDRAM 和摄像头控制总线初始化。`CameraConfig.i2c` 必须指向已通过现有 I2C 接口初始化的 `i2c_t`，且其生命周期覆盖摄像头使用期。
3. 已有显示初始化使用 `LCD_BoardInitialize()`。向测试传入初始化成功的 `LCD_DeviceType *`，显示格式为 RGB565，尺寸至少 640×480。
4. 按实际摄像头选择下面的配置，检查 `CameraInit()` 返回值；成功后调用 `TestCameraDisplay()`。

```c
/* 放在已有 FreeRTOS 任务流程中；camera_i2c 和 lcd 由板级初始化提供。 */
static Camera camera;
CameraConfig config = {
    .i2c = camera_i2c, /* 已初始化的 i2c_t *，不是 FSP instance */
    .model = CAMERA_MODEL_OV7725,
    .interface = CAMERA_INTERFACE_DVP,
    .length = 640,
    .width = 480,
    .peripheral_inst = &g_ceu_ov7725,
};
uint32_t err = CameraInit(&camera, &config);
if (err == FSP_SUCCESS) {
    err = TestCameraDisplay(&camera, lcd);
}
/* 记录 err；只有 FSP_SUCCESS 才表示测试返回成功。 */
```

需要包含 `camera/camera.h`、`hal_data.h`、`test/test.h` 以及显示接口头文件。若实际是 OV5640 DVP，将型号改为 `CAMERA_MODEL_OV5640`，实例改为 `&g_ceu_ov5640`。两个 CEU 配置不能同时占用同一个硬件模块。

控制总线由调用方管理；同一硬件 I2C 控制器不能用两个对象重复打开。如果与 HDMI 共用总线，需要由板级代码统一持有总线，并串行化摄像头寄存器的完整读写操作；不能直接访问或复制 `lcd_board.c` 的私有总线对象。

## 测试行为与排查

`TestCameraDisplay()` 等待一帧，最多约 3 秒；收到帧后停止采集，再复制到显示 framebuffer 并提交显示。当前单帧模式要求 CEU 使用 DATA_SYNCHRONOUS（现有生成配置使用此模式）。这是静态首帧验证，不是连续视频，也没有增加开机自动调用。

- 初始化失败：先看返回码和摄像头 ID 读取，检查供电、XCLK、复位、SCCB 总线实例及接线。
- 采集超时：检查 PCLK、VSYNC、HREF、数据引脚及 CEU 配置。
- 有图但颜色或行错：核对 RGB565 字节顺序、同步极性和实际分辨率。

源码编译和模拟总线测试不能证明硬件出图成功；需要连接实际模组进行板上验证。MIPI、连续视频和 MicroPython 封装另行接入。

## 板级共享 IIC0 入口

J300 摄像头与 TFP410 共用 P410/SCL、P409/SDA。`IIC_BoardAcquire()` 返回同一个 `i2c_t`，首次获取时打开控制器，最后一个客户端 `IIC_BoardRelease()` 才关闭。HDMI 初始化已使用这个入口。共享总线传输由静态二值信号量串行化；它不提供优先级继承。摄像头 SCCB 读取的地址写入和数据读取之间仍发送 STOP，但两个阶段共用一次锁。

新增 `peripheral/camera_board.h`（编译时使用 `#include "camera_board.h"`）提供正式的 C 层入口，独立于 `TEST_EN_CAMERA`：

```c
/* 在 FreeRTOS 任务中，先准备 SDRAM、供电以及 DVP 的 XCLK。 */
Camera *camera = NULL;
uint32_t err = Camera_BoardInitialize(CAMERA_MODEL_OV5640, &camera);
/* OV7725 则传 CAMERA_MODEL_OV7725。检查 err 后再使用 camera。 */
```

该入口选择 J300 DVP、640×480，并沿用传感器初始化的 RGB565 配置；不会自动启动采集，不会自动开启 XCLK，也不会自动初始化 HDMI。随后使用已有 CameraStartCapture/CameraIsCaptureDone/CameraStopCapture 接口。用完调用 `Camera_BoardDeinitialize()`；返回的 Camera 指针由板级管理，不应直接用 CameraInit 重新初始化。板级初始化/反初始化由应用串行调用，读取图像的任务必须先停止使用该对象再反初始化。

同一型号重复初始化返回已有对象；另一型号在摄像头工作期间请求初始化会返回 FSP_ERR_IN_USE。两种型号不能同时占用同一个 CEU。

共享总线被 HDMI 或摄像头持有时，Python `machine.I2C(0)` 初始化返回 EBUSY，不会改动总线引脚；如果 Python 先打开 IIC0，板级获取返回 FSP_ERR_IN_USE。先释放原使用方后再切换，IIC1/IIC2 不受此保留机制影响。共享板级总线不接受跨调用的 `stop=false`；Python 独占模式的 repeated-start 行为保持原样。

以上是软件接入，尚需板上验证芯片 ID、单帧采集和 HDMI 显示。不要为了运行这个入口重复创建或打开 g_i2c_master0。

## OV5640 DVP 开机单帧测试

当前 `TEST_EN_CAMERA=1`，REPL 任务在首次进入 Python 前调用 `TestCameraStartup()`，软复位不重复执行。测试包装复用 `TestCamera()`，不会自动显示 HDMI。

当前工程尚未生成 GPT 驱动。此时启动打印 `CAMERA: SKIP - GPT10 XCLK driver missing.`，不访问摄像头。需在 e2studio 的 configuration.xml 中添加 Timer / r_gpt：实例名 g_timer10、通道 10、Periodic、12000 kHz、50% duty、GTIOCA 输出开启、GTIOCB 关闭、停止输出低、周期中断关闭。按已有 DVP 示例设置 GPTCLK=PLL2R/2、GTCLK=GPTCLK；当前 PLL2R=480 MHz，得到 240 MHz 计数时钟，12 MHz 周期为 20 个计数。Pins 中 P109 对应 GTIOC10A（GPT1）。点击 Generate Project Content 后重新构建。

J300 第 13 脚经 R308 连到 P109。测试代码配置此复用、打开并启动 GPT10，确认实际计数频率和周期对应 12 MHz，再初始化 OV5640、采一帧并释放。启动时串口输出 init、capture 和 cleanup 返回值；capture=0 (PASS) 表示单帧测试返回成功。缺少 GPT 配置的当前编译不能验证生成后的 GPT 分支，生成后必须再次编译并上板测试。

测试结束后将 TEST_EN_CAMERA 改回 0，即可取消开机测试。


## DVP / MIPI independent board instances

The board layer now keeps one Camera object for DVP and one for MIPI. The existing Camera_BoardInitialize(model, &camera) and Camera_BoardDeinitialize() remain DVP-only compatibility entries, using the board-owned shared IIC0 bus and VGA configuration. HDMI preview still uses this DVP entry.

Camera_BoardInitializeWithConfig(&config, &camera) accepts the existing CameraConfig for either interface. DVP accepts OV5640 or OV7725; MIPI accepts OV5640. The caller must supply an initialized SCCB bus, a matching CEU/VIN instance, and prepare power, XCLK and SDRAM. This entry does not initialize HDMI or start capture. A second initialization on an occupied interface returns FSP_ERR_IN_USE; it does not reset the active device.

Use CameraStartCapture / CameraIsCaptureDone / CameraStopCapture on the returned object. Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_DVP) or Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_MIPI_CSI) closes only that interface. Explicit configurations borrow the caller's bus and buffers: keep them alive until capture is closed and all image users have finished, then release them yourself. The board only releases an IIC0 reference acquired by the legacy wrapper.

Serialize initialization, teardown and sensor register control across both cameras. Each Camera retains its SCCB bus; the Camera control APIs select that bus before register access. Direct OV5640 register helpers still use the last selected bus and must not be mixed into interleaved dual-camera control. Frame polling and DMA callbacks do not switch the bus. Ordinary initialization starts at the default OV5640 address and rejects another OV5640 still using that address on the same controller. CameraDual_Start performs reset isolation and remaps the DVP address before bringing up MIPI, as described below.

This change provides independent board instance management only. The generated MIPI CSI/VIN instances are in ra_gen/common_data.c (not hal_data.c); the board MIPI entry described below now reuses them. The existing dynamic A/B allocation belongs to the DVP preview task. No build, tests or hardware validation were performed for this change, per the current development instruction.


## OV5640 MIPI board capture (C only)

Camera_BoardInitializeMIPI(&camera) initializes the MIPI sensor and prepares three dynamically allocated, 128-byte-aligned 1024x600 RGB565 output frames in the uncached FreeRTOS SDRAM heap (3,686,400 bytes plus alignment padding). It reuses the generated CSI/VIN/PHY settings, including YUV422 sensor input and VIN color conversion. XCLK is the module's 24 MHz oscillator; sensor reset/power pins remain those in ov5640.c. This does not start HDMI or a background capture task.

Call CameraStartCapture(camera, NULL) to start continuous VIN capture. CameraIsCaptureDone(camera) returns 1 when camera->image_ready points to a completed frame, 0 while waiting. The three frames are reused by hardware, so a pointer is not a permanent snapshot and an idle check does not lock it. For a stable image, wait for a frame, call CameraStopCapture(camera), then call CameraIsCaptureDone(camera) again to obtain the last completed frame after DMA has stopped. Consume it before restarting or deinitializing. Use a bounded wait in the calling task.

Camera_BoardDeinitializeInterface(CAMERA_INTERFACE_MIPI_CSI) closes capture before freeing the three frames and releasing its shared IIC0 reference. Initialization failure unwinds allocations. A stop timeout or bus error retains buffers for a later cleanup retry. Start/stop and sensor controls must be serialized with the other camera's control calls. The capture dimensions and pixel format must remain fixed for this board entry.

Default wiring uses the same IIC0 as DVP/HDMI. Ordinary single-camera initialization does not isolate two default-address OV5640 modules; use CameraDual_Start for the coordinated reset/address sequence when both are connected. OV7725 has a different SCCB address. MIPI display and MIPI camera share the PHY; starting either while the other owns it is rejected.

The generated common_data.c still defines its original static VIN buffers. This entry overrides the active VIN buffer addresses with heap allocations, but does not remove generated storage; account for that reserved memory separately until the generator configuration is changed. No generated sources were hand-edited, no IDE action is required for the C entry, and no build or tests have been run for this change.


## 两颗 OV5640 同时采集（C 层）

使用 `CameraDual_Start(&dvp, &mipi)` 一次启动两路，不启动 HDMI 或额外后台任务。启动前两路必须均未被占用，现有 HDMI 摄像头预览任务也必须停止。SDRAM 和板级初始化沿用原工程。

初始化顺序：保持两颗模组复位 → 启动 DVP 的 GPT10/P109 12 MHz XCLK → 单独初始化 DVP → 写入 SCCB 寄存器 `0x3100`，将 DVP 的 7 位地址从 `0x3C` 改成 `0x3D`，并在新地址回读确认 → 初始化 MIPI（仍为 `0x3C`，模组自带 24 MHz 时钟）→ 启动两路采集。复位引脚沿用驱动定义：DVP P709、MIPI P704。地址编码参考 [Linux OV5640 的 ov5640_init_slave_id](https://code.googlesource.com/linux/torvalds/linux/+/refs/heads/master/drivers/media/i2c/ov5640.c)，写入值为七位地址左移一位。

`Camera` 保存各自的 SCCB 地址；现有 Camera 控制 API 在寄存器访问前选择对应总线与地址。两路共享 IIC0，但不会将启动、停止或配置寄存器写入另一颗。控制和生命周期调用仍由应用串行执行，不要绕过 Camera 接口交错调用低层 OV5640 寄存器函数。

- DVP：640×480 RGB565，动态三帧轮转，约 1.76 MiB；CEU 回调沿用现有连续重启逻辑。
- MIPI：1024×600 RGB565，动态三帧轮转，约 3.52 MiB。
- 合计动态采集内存约 5.27 MiB（另有对齐开销），不含原生成文件中的静态 VIN 数组；按要求保留这些旧数组。

```c
Camera *dvp = NULL;
Camera *mipi = NULL;
uint32_t err = CameraDual_Start(&dvp, &mipi);
if (err == FSP_SUCCESS) {
    /* 在任务循环中分别检查两路。返回 1 后读取对应 image_ready；
     * 指针为轮转缓冲的借用地址，必须及时处理，不能长期持有。 */
    uint32_t dvp_ready = CameraIsCaptureDone(dvp);
    uint32_t mipi_ready = CameraIsCaptureDone(mipi);
    (void)dvp_ready;
    (void)mipi_ready;
}
/* 所有图像使用方停止访问之后，再统一关闭。检查 cleanup 返回码。 */
uint32_t cleanup = CameraDual_Stop();
```

`CameraStopCapture(dvp)` 或 `CameraStopCapture(mipi)` 单独停止对应路径，不释放帧缓冲；`CameraStartCapture(camera, NULL)` 可再次启动该路。`CameraDual_Stop()` 关闭两路、将传感器保持复位、释放动态缓冲和 IIC0 引用，并关闭其拥有的 DVP XCLK。初始化失败自动执行同样清理；如果清理失败，保留未安全关闭的资源，应用应处理错误并再次调用 `CameraDual_Stop()`，不要直接释放帧指针。

不要在双路会话中改变分辨率、格式或直接复位传感器；硬件复位会丢失分配的 SCCB 地址。需要重新配置时，停止整个双路会话后重新启动。动态缓冲由板级持有，返回的 Camera 对象不应再次传给 CameraInit。两路同时采集不表示曝光或帧起点同步。

本次仅完成代码和源码检查，未编译、未运行测试、未验证实机地址重映射和双路出图。


### MicroPython dual capture control

Single-camera HDMI preview supports both OV5640 and OV7725 over DVP/CEU:

```python
from machine import Camera

cam = Camera("ov7725")  # Select OV7725; Camera() defaults to OV5640.
cam.start()             # DVP -> CEU A/B -> HDMI, 640x480 RGB565.
cam.stop()
```

`Camera("ov5640")` explicitly selects the original OV5640 preview. Only one
sensor can own DVP/CEU preview at a time; switching models while capture is
active raises `OSError(EBUSY)`. OV7725 is DVP-only; `start_dual()` remains
an OV5640-only operation. Single-camera `start()` displays to HDMI, and
`snapshot()` copies one frame from the currently selected DVP camera:

```python
from machine import Camera

cam = Camera("ov7725")  # Or Camera("ov5640").
cam.start()
try:
    frame = cam.snapshot()  # CameraFrame, 640x480 RGB565, 614400 bytes.
    try:
        chunk = bytearray(4096)
        offset = 0
        while offset < frame.size():
            n = frame.readinto(chunk, offset)
            # Process/write the first n bytes of chunk.
            offset += n
    finally:
        frame.release()
finally:
    cam.stop()
```

Single-camera `snapshot()` returns a `CameraFrame` (not a tuple). The
preview task copies a fully completed CEU frame before arming the next frame.
The snapshot is independent of HDMI and remains readable after `cam.stop()`.
Only one snapshot request can be in progress. The timeout is 5000 ms,
including asynchronous preview startup; a timed-out request is cancelled if
copying has not begun. If the copy has already started, the caller waits for
it to finish to protect against freeing memory still in use. A stopped or
failed preview raises `OSError`. `CameraFrame.release()` or GC returns its
SDRAM copy to the FreeRTOS heap; no full frame enters the MicroPython heap.

```python
from machine import Camera
cam = Camera()
cam.start_dual()       # Start both OV5640 paths; no HDMI output.
cam.is_running()       # True after both paths start successfully.
cam.last_error()       # Raw FSP result from the last dual start/stop operation.
cam.stop()             # Stop both paths and release their resources.
```

`start()` / `init()` retain the single-camera HDMI preview behavior. Stop the current session before switching modes. `start_dual()` returns `True` on success, `False` when already running, and raises `OSError` on failure (busy: EBUSY, allocation failure: ENOMEM, unsupported: ENOSYS, other hardware errors: EIO). `deinit()` aliases `stop()` for either mode.

If dual cleanup fails, `stop()` raises and can be retried; new starts remain blocked while resources are retained. `is_running()` reports successful session startup, not live frame arrival or per-path hardware health; after a stop attempt it is false even if cleanup needs retrying. Serialize all camera control calls. Python frame retrieval is described below. This change has only received source inspection; no build or hardware testing was performed.

HDMI preview also retains failed CEU, GLCDC and XCLK cleanup state. If `cam.start()` raises `OSError(EBUSY)` after a failed preview shutdown, use `cam.stop()` to retry resource cleanup before starting again. A MicroPython soft reset attempts to stop the active camera session before releasing the Python I2C/pin state; if hardware cleanup fails, it logs the error and retains those peripherals so DMA memory is not released unsafely. Soft reset always frees standalone `CameraFrame` snapshots.


### Python frame copy

```python
from machine import Camera
cam = Camera()
cam.start_dual()
try:
    frame, width, height, fmt = cam.snapshot(Camera.DVP)
    try:
        chunk = bytearray(4096)
        offset = 0
        while offset < frame.size():
            n = frame.readinto(chunk, offset)
            # Process or write chunk[:n] here; do not keep every chunk in RAM.
            offset += n
    finally:
        frame.release()
finally:
    cam.stop()
```

`snapshot(interface)` returns `(CameraFrame, width, height, "RGB565")`: DVP is 640x480 (614400 bytes) and MIPI is 1024x600 (1228800 bytes). The frame owns a separate `pvPortMalloc` allocation in the uncached FreeRTOS SDRAM heap; only the small `CameraFrame` object uses MicroPython's 256-KiB GC heap. The copy is independent of the DMA ring and remains valid after `cam.stop()`.

Use `frame.size()` to query the byte count and `frame.readinto(writable_buffer[, offset])` to copy a slice, returning the copied byte count (zero at EOF). The optional offset is positional (defaults to zero). Do not call `memoryview(frame)` or treat the frame as a Python bytearray; the frame does not expose its SDRAM pointer. `frame.release()` frees SDRAM memory and is idempotent. Access after release raises `OSError(ENODEV)`. Unreferenced frames are automatically freed by MicroPython GC finalisers, and `machine_deinit()` frees remaining frame allocations on MicroPython soft reset. Explicit `release()` is still recommended to reclaim SDRAM promptly, especially when taking repeated snapshots.

The binding allocates an independent SDRAM frame first. `CameraDual_CopyFrame` then waits for a new completed frame, stops only that path, copies its completed frame and restarts capture. The other path continues running. This intentionally pauses the selected path; it is not uninterrupted capture or synchronized snapshots. The frame wait is bounded to 1000 ms; stopping/restarting adds time. A timeout raises ETIMEDOUT. Stop/restart failure marks the dual session not running; call `stop()` to release remaining resources before starting again. Serialize snapshot/start/stop with all other C camera controls; do not consume the ready flag from another task during a snapshot. No compilation or hardware testing has been performed.
