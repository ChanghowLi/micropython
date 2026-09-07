#ifndef MICROPY_INCLUDED_RENESAS_RA8_I2C_H
#define MICROPY_INCLUDED_RENESAS_RA8_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool i2c_deinit(uint32_t id);
int i2c_init(uint32_t id, uint32_t freq);
// Returns errno (zero on success); transferred reports received bytes or data ACKs.
int i2c_transfer(uint32_t id, uint16_t addr, size_t len, uint8_t *buf, bool read, bool stop, uint32_t timeout_ms, size_t *transferred);
int i2c_validate_freq(uint32_t id, uint32_t freq);
void machine_i2c_deinit_all(void);

#endif
