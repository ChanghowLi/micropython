#ifndef MICROPY_INCLUDED_RENESAS_RA8_SPI_H
#define MICROPY_INCLUDED_RENESAS_RA8_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RA8_SCI_SPI_COUNT (8)

void machine_spi_deinit_all(void);
bool spi_deinit(uint32_t id);

int spi_init(
    uint32_t id,
    uint32_t baudrate,
    uint8_t polarity,
    uint8_t phase,
    uint8_t bits,
    uint8_t firstbit
    );

int spi_transfer(
    uint32_t id,
    size_t len,
    const uint8_t *src,
    uint8_t *dest,
    uint32_t timeout_ms
    );

#endif 
