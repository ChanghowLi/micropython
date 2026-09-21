#include "bsp_api.h"
#include "sci.h"

#define MACHINE_SCI_CHANNEL_COUNT (10U)

void sci_b_i2c_tei_isr(void);
void sci_b_i2c_txi_isr(void);
void sci_b_spi_eri_isr(void);
void sci_b_spi_rxi_isr(void);
void sci_b_spi_tei_isr(void);
void sci_b_spi_txi_isr(void);
void sci_b_uart_eri_isr(void);
void sci_b_uart_rxi_isr(void);
void sci_b_uart_tei_isr(void);
void sci_b_uart_txi_isr(void);

static volatile machine_sci_owner_t machine_sci_owners[MACHINE_SCI_CHANNEL_COUNT];

static void machine_sci_irq_clear(void)
{
    R_BSP_IrqStatusClear(R_FSP_CurrentIrqGet());
}

static void machine_sci_rxi_isr(uint8_t channel)
{
    machine_sci_owner_t owner = machine_sci_get_owner(channel);

    if (owner == MACHINE_SCI_OWNER_SPI) {
        sci_b_spi_rxi_isr();
    } else if (owner == MACHINE_SCI_OWNER_UART) {
        sci_b_uart_rxi_isr();
    } else {
        machine_sci_irq_clear();
    }
}

static void machine_sci_txi_isr(uint8_t channel)
{
    machine_sci_owner_t owner = machine_sci_get_owner(channel);

    if (owner == MACHINE_SCI_OWNER_SPI) {
        sci_b_spi_txi_isr();
    } else if (owner == MACHINE_SCI_OWNER_I2C) {
        sci_b_i2c_txi_isr();
    } else if (owner == MACHINE_SCI_OWNER_UART) {
        sci_b_uart_txi_isr();
    } else {
        machine_sci_irq_clear();
    }
}

static void machine_sci_tei_isr(uint8_t channel)
{
    machine_sci_owner_t owner = machine_sci_get_owner(channel);

    if (owner == MACHINE_SCI_OWNER_SPI) {
        sci_b_spi_tei_isr();
    } else if (owner == MACHINE_SCI_OWNER_I2C) {
        sci_b_i2c_tei_isr();
    } else if (owner == MACHINE_SCI_OWNER_UART) {
        sci_b_uart_tei_isr();
    } else {
        machine_sci_irq_clear();
    }
}

static void machine_sci_eri_isr(uint8_t channel)
{
    machine_sci_owner_t owner = machine_sci_get_owner(channel);

    if (owner == MACHINE_SCI_OWNER_SPI) {
        sci_b_spi_eri_isr();
    } else if (owner == MACHINE_SCI_OWNER_UART) {
        sci_b_uart_eri_isr();
    } else {
        machine_sci_irq_clear();
    }
}

void machine_sci1_rxi_isr(void) { machine_sci_rxi_isr(1U); }
void machine_sci1_txi_isr(void) { machine_sci_txi_isr(1U); }
void machine_sci1_tei_isr(void) { machine_sci_tei_isr(1U); }
void machine_sci1_eri_isr(void) { machine_sci_eri_isr(1U); }

void machine_sci2_rxi_isr(void) { machine_sci_rxi_isr(2U); }
void machine_sci2_txi_isr(void) { machine_sci_txi_isr(2U); }
void machine_sci2_tei_isr(void) { machine_sci_tei_isr(2U); }
void machine_sci2_eri_isr(void) { machine_sci_eri_isr(2U); }

void machine_sci4_rxi_isr(void) { machine_sci_rxi_isr(4U); }
void machine_sci4_txi_isr(void) { machine_sci_txi_isr(4U); }
void machine_sci4_tei_isr(void) { machine_sci_tei_isr(4U); }
void machine_sci4_eri_isr(void) { machine_sci_eri_isr(4U); }

void machine_sci5_rxi_isr(void) { machine_sci_rxi_isr(5U); }
void machine_sci5_txi_isr(void) { machine_sci_txi_isr(5U); }
void machine_sci5_tei_isr(void) { machine_sci_tei_isr(5U); }
void machine_sci5_eri_isr(void) { machine_sci_eri_isr(5U); }

void machine_sci6_rxi_isr(void) { machine_sci_rxi_isr(6U); }
void machine_sci6_txi_isr(void) { machine_sci_txi_isr(6U); }
void machine_sci6_tei_isr(void) { machine_sci_tei_isr(6U); }
void machine_sci6_eri_isr(void) { machine_sci_eri_isr(6U); }

void machine_sci8_rxi_isr(void) { machine_sci_rxi_isr(8U); }
void machine_sci8_txi_isr(void) { machine_sci_txi_isr(8U); }
void machine_sci8_tei_isr(void) { machine_sci_tei_isr(8U); }
void machine_sci8_eri_isr(void) { machine_sci_eri_isr(8U); }

machine_sci_owner_t machine_sci_get_owner(uint8_t channel)
{
    if (channel >= MACHINE_SCI_CHANNEL_COUNT) {
        return MACHINE_SCI_OWNER_NONE;
    }

    return machine_sci_owners[channel];
}

void machine_sci_give(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel < MACHINE_SCI_CHANNEL_COUNT && machine_sci_owners[channel] == owner) {
        machine_sci_owners[channel] = MACHINE_SCI_OWNER_NONE;
    }
}

bool machine_sci_take(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel >= MACHINE_SCI_CHANNEL_COUNT || owner == MACHINE_SCI_OWNER_NONE) {
        return false;
    }

    if (machine_sci_owners[channel] != MACHINE_SCI_OWNER_NONE) {
        return false;
    }

    machine_sci_owners[channel] = owner;
    return true;
}
