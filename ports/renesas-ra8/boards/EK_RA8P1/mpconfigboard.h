// EK-RA8P1 board-specific MicroPython configuration
// TODO: verify clock values against the e2 studio bsp_clock_cfg.h for this board

#define MICROPY_HW_BOARD_NAME       "EK-RA8P1"
#define MICROPY_HW_MCU_NAME         "RA8P1"
#define MICROPY_HW_MCU_SYSCLK       1000000000
#define MICROPY_HW_MCU_PCLK         125000000

// UART REPL configuration
// TODO: verify UART channel and pins against EK-RA8P1 schematic
#define MICROPY_HW_UART_REPL        HW_UART_0
#define MICROPY_HW_UART_REPL_BAUD   115200

// TODO: verify LED/switch pins
// #define MICROPY_HW_LED1             (pin_Pxxx)
// #define MICROPY_HW_LED_ON(pin)      mp_hal_pin_high(pin)
// #define MICROPY_HW_LED_OFF(pin)     mp_hal_pin_low(pin)
// #define MICROPY_HW_LED_TOGGLE(pin)  mp_hal_pin_toggle(pin)
// #define MICROPY_HW_HAS_SWITCH       (1)
// #define MICROPY_HW_USRSW_PIN        (pin_Pxxx)

// Peripheral enables
#define MICROPY_HW_ENABLE_RTC       (0)
#define MICROPY_HW_ENABLE_ADC       (0)
#define MICROPY_HW_ENABLE_DAC       (0)
#define MICROPY_PY_THREAD           (0)
