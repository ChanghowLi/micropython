#include "py/mpconfig.h"
#include "py/mphal.h"
#include "py/stream.h"

#include "bsp_api.h"
#include "console.h"
#include "hal_data.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG __FUNCTION__

#ifndef __MPHALPORT_DEBUG
#define __MPHALPORT_DEBUG   1
#endif

#if __MPHALPORT_DEBUG
#include "utils/log.h"
#endif

int g_mp_interrupt_char = -1;

/* DWT cycle counter timing support. */

// Keep individual busy-wait spans below half of the 32-bit DWT range so that
// unsigned cycle-difference comparisons stay valid across counter wrap.
#define MP_HAL_DWT_MAX_DELAY_CYCLES (0x7fffffffU)

// DWT->CYCCNT is only 32-bit.  The accumulator below extends it to 64-bit for
// ticks_us(), which may be called across many DWT wrap events.
static uint32_t s_dwt_last;
static uint64_t s_dwt_accum;
static uint8_t s_dwt_ready;

/**
 * @brief   启用 DWT Cycle Counter，用于 ticks_cpu/ticks_us。
 */
static void mp_hal_enable_cycle_counter(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

static uint32_t mp_hal_cycles_per_us(void)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    // Avoid division by zero if SystemCoreClock is not initialized yet.
    return cycles_per_us == 0 ? 1 : cycles_per_us;
}

static uint32_t mp_hal_cycles_per_ms(void)
{
    uint32_t cycles_per_ms = SystemCoreClock / 1000U;

    // Avoid division by zero if SystemCoreClock is not initialized yet.
    return cycles_per_ms == 0 ? 1 : cycles_per_ms;
}

static uint64_t mp_hal_dwt_cycles64(void)
{
    uint32_t now;

    mp_hal_enable_cycle_counter();

    now = DWT->CYCCNT;
    if (!s_dwt_ready) {
        // The first call establishes the base point; ticks start from zero.
        s_dwt_last = now;
        s_dwt_accum = 0;
        s_dwt_ready = 1;
    }
    else {
        s_dwt_accum += (uint32_t)(now - s_dwt_last);
        s_dwt_last = now;
    }

    return s_dwt_accum;
}

static void mp_hal_delay_cycles(uint32_t cycles)
{
    uint32_t start;

    if (cycles == 0) {
        return;
    }

    mp_hal_enable_cycle_counter();

    // Unsigned subtraction makes this safe if DWT->CYCCNT wraps during wait.
    start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}

/* Standard input/output. */

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


/* Timing functions. */

void mp_hal_delay_ms(mp_uint_t ms)
{
    uint32_t cycles_per_ms = mp_hal_cycles_per_ms();
    mp_uint_t max_ms_per_chunk = MP_HAL_DWT_MAX_DELAY_CYCLES / cycles_per_ms;

    // Split long waits into chunks that fit safely in the 32-bit DWT counter.
    while (ms > 0) {
        mp_uint_t chunk = ms > max_ms_per_chunk ? max_ms_per_chunk : ms;
        mp_hal_delay_cycles((uint32_t)(chunk * cycles_per_ms));
        ms -= chunk;
    }
}

void mp_hal_delay_us(mp_uint_t us)
{
    uint32_t cycles_per_us = mp_hal_cycles_per_us();
    mp_uint_t max_us_per_chunk = MP_HAL_DWT_MAX_DELAY_CYCLES / cycles_per_us;

    // Split long waits into chunks that fit safely in the 32-bit DWT counter.
    while (us > 0) {
        mp_uint_t chunk = us > max_us_per_chunk ? max_us_per_chunk : us;
        mp_hal_delay_cycles((uint32_t)(chunk * cycles_per_us));
        us -= chunk;
    }
}

/**
 * @brief   获取 CPU 的 Cycle Counter
 * @retval  CPU Cycle Counter
 */
mp_uint_t mp_hal_ticks_cpu(void)
{
    mp_hal_enable_cycle_counter();

    return (mp_uint_t)DWT->CYCCNT;
}

/**
 * @brief   获取微秒 tick。
 * @retval  微秒计数
 */
mp_uint_t mp_hal_ticks_us(void)
{
    return (mp_uint_t)(mp_hal_dwt_cycles64() / mp_hal_cycles_per_us());
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

    return (mp_uint_t)((uint64_t)tick * 1000ULL / configTICK_RATE_HZ);
}

void mp_hal_set_interrupt_char(char c)
{
#if __MPHALPORT_DEBUG
    LOG_D(TAG, "c = %d", c);
#endif
    g_mp_interrupt_char = c;
}

int mp_hal_is_interrupt_char_received(void)
{
    if (g_mp_interrupt_char < 0) {
        return 0;
    }

    return 0;
}
