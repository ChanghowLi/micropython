// CPKCOR-RA8P1 board-specific MicroPython configuration
// Clock configuration (from e2 studio bsp_clock_cfg.h):
//   CPUCLK  = PLL1P /1 = 1000000000 Hz (1 GHz)
//   PCLKA   = PLL1P /8 =  125000000 Hz
//   SCICLK  = PLL2R /4 =  120000000 Hz (UART baud clock)

#define MICROPY_HW_BOARD_NAME       "CPKCOR-RA8P1"
#define MICROPY_HW_MCU_NAME         "RA8P1"
#define MICROPY_HW_MCU_SYSCLK       1000000000
#define MICROPY_HW_MCU_PCLK         125000000
#define MICROPY_LONGINT_IMPL (MICROPY_LONGINT_IMPL_MPZ)

// UART REPL configuration
// SCI9: TX=P208, RX=P209 (connected to onboard USB-UART)
#define MICROPY_HW_UART_REPL        HW_UART_9
#define MICROPY_HW_UART_REPL_BAUD   2000000

// LEDs
#define MICROPY_HW_LED1             (pin_P106)
#define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
#define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
#define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)

// User switch
#define MICROPY_HW_HAS_SWITCH       (1)
#define MICROPY_HW_USRSW_PIN        (pin_P105)
#define MICROPY_HW_USRSW_PULL       (MP_HAL_PIN_PULL_NONE)
#define MICROPY_HW_USRSW_EXTI_MODE  (MP_HAL_PIN_TRIGGER_FALLING)
#define MICROPY_HW_USRSW_PRESSED    (0)

// Peripheral enables — milestone 1: only UART for REPL
#define MICROPY_HW_ENABLE_RTC       (0)
#define MICROPY_HW_ENABLE_ADC       (0)
#define MICROPY_HW_ENABLE_DAC       (0)

// Disable thread support for now (requires mpthreadport.c with FreeRTOS adaptation)
#define MICROPY_PY_THREAD           (0)
