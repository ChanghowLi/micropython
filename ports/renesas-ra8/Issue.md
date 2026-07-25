# Issue

## DCache 和 NorFlash

### 问题描述

若开启 DCache，则 NorFlash 的写入会出错，`NorFlash_SetWriteEnable()` 会超时，最终导致 littlefs 挂载 NorFlash 失败或进入 Hardfault

分支：ra-dev

提交记录：815a85549f16c6e293f5d1da24514dd067ababf3

FSP Version：6.4.0

### 错误日志

串口：

```
_SEGGER_RTT address: 0x22049600
MPY: failed to mount NOR LittleFS at /flash (error -5).
MicroPython 815a85549f-dirty on 2026-07-25; CPKCOR-RA8P1 with RA8P1
Type "help()" for more information.
>>>
```

RTT View（仅关键错误信息）：

```
[0.051] DEBUG NorFlashDev_Mount ==> Initial mount: result=-19
[0.095] ERROR NorFlash_EraseSector ==> SetWriteEnable failed: 0x14
[0.095] ERROR nor_flash_dev_ioctl ==> Erase failed: block=1, sector=5, FSP error=0x00000014
[0.095] DEBUG NorFlashDev_Mount ==> Format: result=-5
```

### 问题修复

已在 `NorFlash_EraseSector()`、`NorFlash_Program()`、`NorFlash_SetWriteEnable()` 中临时关闭 DCache。若要在 C 代码中操作 NorFlash，请尽量使用 `nor_flash.c` 提供的 API 而不是直接使用 FSP 的 API。

### 其它参考信息

NorFlash 驱动来自 [gitee](https://gitee.com/dlans/ra8-p1_-generic/tree/cpkcor/) 中的 ep_nor_flash。示例工程仅使用了 NorFlash，且问题能复现，在示例工程中分析可以排除其它干扰。示例工程在写入时关闭了 DCache，若要复现问题，在示例工程中的写入部分开启 DCache 即可。