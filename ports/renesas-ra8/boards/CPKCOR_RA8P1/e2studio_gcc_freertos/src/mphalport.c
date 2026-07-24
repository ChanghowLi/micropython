#include "bsp_api.h"
#include "console.h"
#include "hal_data.h"
#include "mpconfigport.h"
#include "mphalport.h"
#include "rtc.h"
#include "perf_counter/perf_counter.h"

#include "FreeRTOS.h"
#include "task.h"

#include "py/mpconfig.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "shared/timeutils/timeutils.h"

#define TAG __FUNCTION__

#ifndef __MPHALPORT_DEBUG
#define __MPHALPORT_DEBUG   1
#endif

#if __MPHALPORT_DEBUG
#include "utils/log.h"
#endif

int g_mp_interrupt_char = -1;

void mp_hal_delay_ms(mp_uint_t ms)
{
    if (__get_IPSR() == 0) {
        vTaskDelay(ms);
    }
    else {
        R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

void mp_hal_delay_us(mp_uint_t us)
{
    R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS);
}

void mp_hal_set_interrupt_char(char c)
{
#if __MPHALPORT_DEBUG
    LOG_D(TAG, "c = %d", c);
#endif
    g_mp_interrupt_char = c;
}

/**
 * @brief   实现 MicroPython 底层的 stdin 流, 会被如 stdio_read() 等函数调用
 * @retval  读取到的字符，来自 UART 的环形缓冲区
 */
int mp_hal_stdin_rx_chr(void)
{
    unsigned char c;

    while (CONSOLE_HasData() == 0) {
        taskYIELD();
    }
    CONSOLE_Read(&c, 1);

    return c;
}

/**
 * @brief   MicroPython 要调用 stdio 时，会使用 stdio_ioctl() 查询要使用的流是否可用，该函数即被 stdio_ioctl() 调用，根据 port 实现
 * @param   poll_flags 会传入的值参考 py\stream.h 53~57 行
 */
uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags)
{
    uintptr_t ret = 0;

#if __MPHALPORT_DEBUG
    LOG_D(TAG, "poll_flags: 0x%04X", poll_flags);
#endif

    if ((poll_flags & MP_STREAM_POLL_RD) && CONSOLE_HasData()) {
        ret |= MP_STREAM_POLL_RD;
    }

    // 当前 TX 是轮询发送，可以认为始终可写。
    if (poll_flags & MP_STREAM_POLL_WR) {
        ret |= MP_STREAM_POLL_WR;
    }

    return ret;
}

/**
 * @brief   部分 MicroPython 的源代码使用这个函数来输出
 * @param   str 要发送的字符串
 * @param   len 要发送的字符串的大小
 * @retval  实际发送的字符数
 */
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len)
{
    sci_b_uart_instance_ctrl_t *ctrl = (sci_b_uart_instance_ctrl_t *)CONSOLE_CFG_UART_INSTANCE.p_ctrl;

    for (mp_uint_t i = 0; i < len; i++) {
        ctrl->p_reg->TDR_BY = (uint8_t)str[i];
        while ((ctrl->p_reg->CSR & R_SCI_B0_CSR_TDRE_Msk) == 0) {}
    }

    return len;
}

/**
 * @brief   部分 MicroPython 的源代码使用这个函数来输出
 * @param   str 要发送的字符串
 */
void mp_hal_stdout_tx_str(const char *str) 
{
    sci_b_uart_instance_ctrl_t *ctrl = (sci_b_uart_instance_ctrl_t *)CONSOLE_CFG_UART_INSTANCE.p_ctrl;

    for (uint32_t i = 0; str[i]; i++) {
        ctrl->p_reg->TDR_BY = (uint8_t)str[i];
        while ((ctrl->p_reg->CSR & R_SCI_B0_CSR_TDRE_Msk) == 0) {}
    }
}

/**
 * @brief   实现 MicroPython 底层的 stdout 流, 用于输出字符串，会被 stdio_write() 调用
 * @param   str 要发送的字符串
 * @param   len 要发送的字符串的大小
 */
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            mp_hal_stdout_tx_strn("\r", 1);
        }
        mp_hal_stdout_tx_strn(&str[i], 1);
    }
}

/**
 * @brief   获取 CPU 的 Cycle Counter
 * @retval  CPU Cycle Counter
 */
mp_uint_t mp_hal_ticks_cpu(void)
{
    return (mp_uint_t)get_system_ticks();
}

/**
 * @brief   获取毫秒 tick。
 * @retval  毫秒计数
 */
mp_uint_t mp_hal_ticks_ms(void)
{
    TickType_t tick;

    if (__get_IPSR() == 0) {
        tick = xTaskGetTickCount();
    }
    else {
        tick = xTaskGetTickCountFromISR();
    }

    return (mp_uint_t)(portTICK_PERIOD_MS * tick);
}

/**
 * @brief   获取微秒 tick。
 * @retval  微秒计数
 */
mp_uint_t mp_hal_ticks_us(void)
{
    return (mp_uint_t)get_system_us();
}

uint64_t mp_hal_time_ns(void)
{
    uint64_t ns = 0;
#if MICROPY_HW_ENABLE_RTC
    rtc_time_t r;
    RTC_GetCalendarTime(&r);
    /* TODO Time may incorrect, wait validate */
    ns = timeutils_seconds_since_epoch(r.tm_year + 2000, r.tm_mon + 1, r.tm_mday, r.tm_hour, r.tm_min, r.tm_sec);
    ns *= 1000000000ULL;
#else
#endif
    return ns;
}
