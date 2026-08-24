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

/* TODO 这里是不是没写完，csv 文件里有很多复用功能，并引申出另一个问题：
 * 为什么自动生成的 pins_CPKCOR_RA8P1.c 中只有 sci_spi 的复用数组记录，其它的复用记录没生成 */
typedef enum {
    MACHINE_PIN_AF_PERIPHERAL_SCI,
    MACHINE_PIN_AF_PERIPHERAL_SPI,
} machine_pin_af_peripheral_t;

typedef enum {
    MACHINE_PIN_AF_SPI_MISO,
    MACHINE_PIN_AF_SPI_MOSI,
    MACHINE_PIN_AF_SPI_SCK,
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
extern const machine_pin_af_obj_t machine_pin_spi_afs[];
extern const mp_obj_type_t machine_pin_board_pins_obj_type;
extern const mp_obj_type_t machine_pin_cpu_pins_obj_type;
extern const mp_obj_type_t machine_pin_type;
extern const size_t machine_pin_spi_afs_count;

void machine_pin_deinit_all(void);
const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);
void machine_pin_configure_alt(bsp_io_port_pin_t pin_id, ioport_peripheral_t peripheral);
void machine_pin_configure_output(const machine_pin_obj_t *pin, bool value);
void machine_pin_write(const machine_pin_obj_t *pin, bool value);
void machine_pin_give(bsp_io_port_pin_t pin_id);
void machine_pin_irq_deinit(void);
bool machine_pin_irq_is_active(const machine_pin_obj_t *pin);
bool machine_pin_take(bsp_io_port_pin_t pin_id);

#endif
