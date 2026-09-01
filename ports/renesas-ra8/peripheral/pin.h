#ifndef MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H
#define MICROPY_INCLUDED_RENESAS_RA8_PERIPHERAL_PIN_H

#include "genhdr/pins.h"
#include "hal_data.h"
#include "py/obj.h"

enum {
    MACHINE_PIN_IRQ_FALLING = 1,
    MACHINE_PIN_IRQ_RISING = 2,
    MACHINE_PIN_IRQ_LOW_LEVEL = 4,
    MACHINE_PIN_IRQ_HIGH_LEVEL = 8,
};

typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    qstr name;
    bsp_io_port_pin_t pin;
    uint32_t alt_mask;
    int8_t irq_channel;
    bool irq_deep_standby;
} machine_pin_obj_t;

typedef enum {
    MACHINE_PIN_AF_PERIPHERAL_IIC,
    MACHINE_PIN_AF_PERIPHERAL_SCI,
    MACHINE_PIN_AF_PERIPHERAL_SPI,
} machine_pin_af_peripheral_t;

typedef enum {
    MACHINE_PIN_AF_I2C_SCL,
    MACHINE_PIN_AF_I2C_SDA,
    MACHINE_PIN_AF_SPI_MISO,
    MACHINE_PIN_AF_SPI_MOSI,
    MACHINE_PIN_AF_SPI_SCK,
    MACHINE_PIN_AF_UART_CTS,
    MACHINE_PIN_AF_UART_CTS_RTS,
    MACHINE_PIN_AF_UART_RXD,
    MACHINE_PIN_AF_UART_SCK,
    MACHINE_PIN_AF_UART_TXD,
} machine_pin_af_signal_t;

typedef struct {
    bsp_io_port_pin_t pin;
    machine_pin_af_peripheral_t peripheral;
    uint8_t channel;
    uint8_t group;
    machine_pin_af_signal_t signal;
} machine_pin_af_obj_t;

extern const mp_obj_dict_t machine_pin_board_pins_locals_dict;
extern const mp_obj_dict_t machine_pin_cpu_pins_locals_dict;
extern const mp_obj_fun_builtin_var_t machine_pin_irq_obj;
extern const mp_obj_fun_builtin_fixed_t machine_pin_irq_stats_obj;
extern const machine_pin_af_obj_t machine_pin_afs[];
extern const mp_obj_type_t machine_pin_board_pins_obj_type;
extern const mp_obj_type_t machine_pin_cpu_pins_obj_type;
extern const mp_obj_type_t machine_pin_type;
extern const size_t machine_pin_afs_count;

void machine_pin_deinit_all(void);
const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);
const machine_pin_af_obj_t *machine_pin_find_af(bsp_io_port_pin_t pin, machine_pin_af_peripheral_t peripheral, uint8_t channel, machine_pin_af_signal_t signal);
void machine_pin_configure_alt(bsp_io_port_pin_t pin_id, ioport_peripheral_t peripheral, uint32_t options);
void machine_pin_configure_input(const machine_pin_obj_t *pin);
void machine_pin_configure_output(const machine_pin_obj_t *pin, bool value);
bool machine_pin_read(const machine_pin_obj_t *pin);
void machine_pin_write(const machine_pin_obj_t *pin, bool value);
bool machine_pin_give(bsp_io_port_pin_t pin_id);
void machine_pin_irq_deinit(void);
bool machine_pin_irq_is_active(const machine_pin_obj_t *pin);
bool machine_pin_take(bsp_io_port_pin_t pin_id);

#endif
