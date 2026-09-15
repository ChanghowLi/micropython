#include "test.h"

#if TEST_EN_I2C

#include "hal_data.h"
#include "i2c.h"
#include "utils/log.h"

#define TAG                         __FUNCTION__
#define TEST_I2C_EEPROM_ADDR        (0x50U)
#define TEST_I2C_EEPROM_SIZE        (256U)
#define TEST_I2C_PAGE_SIZE          (8U)
#define TEST_I2C_PIN_OPTIONS        (IOPORT_CFG_NMOS_ENABLE      \
                                     | IOPORT_CFG_DRIVE_MID      \
                                     | IOPORT_CFG_PERIPHERAL_PIN \
                                     | IOPORT_PERIPHERAL_IIC)
#define TEST_I2C_WRITE_DELAY_MS     (10U)

static i2c_t s_test_i2c = {
    .instance = &g_i2c_master1,
};

static fsp_err_t test_i2c_write_page(uint8_t page_addr, const uint8_t *data)
{
    uint8_t tx_data[TEST_I2C_PAGE_SIZE + 1U];

    tx_data[0] = page_addr;
    for (uint32_t i = 0U; i < TEST_I2C_PAGE_SIZE; ++i) {
        tx_data[i + 1U] = data[i];
    }

    fsp_err_t err = (fsp_err_t)IIC_Write(
        &s_test_i2c,
        TEST_I2C_EEPROM_ADDR,
        tx_data,
        (uint8_t)sizeof(tx_data));
    if (err != FSP_SUCCESS) {
        return err;
    }

    R_BSP_SoftwareDelay(TEST_I2C_WRITE_DELAY_MS, BSP_DELAY_UNITS_MILLISECONDS);
    return FSP_SUCCESS;
}

static fsp_err_t test_i2c_read_page(uint8_t page_addr, uint8_t *data)
{
    return (fsp_err_t)IIC_ReadMemory(
        &s_test_i2c,
        TEST_I2C_EEPROM_ADDR,
        page_addr,
        1U,
        data,
        TEST_I2C_PAGE_SIZE);
}

static uint32_t test_i2c_fill_and_verify(bool sequential)
{
    uint8_t expected[TEST_I2C_PAGE_SIZE];
    uint8_t actual[TEST_I2C_PAGE_SIZE];

    for (uint32_t base = 0U; base < TEST_I2C_EEPROM_SIZE; base += TEST_I2C_PAGE_SIZE) {
        for (uint32_t i = 0U; i < TEST_I2C_PAGE_SIZE; ++i) {
            expected[i] = sequential ? (uint8_t)(base + i) : 0xFFU;
        }

        fsp_err_t err = test_i2c_write_page((uint8_t)base, expected);
        if (err != FSP_SUCCESS) {
            LOG_E(TAG, "Write failed at 0x%02X: %u", (unsigned int)base, (unsigned int)err);
            return (uint32_t)err;
        }
    }

    for (uint32_t base = 0U; base < TEST_I2C_EEPROM_SIZE; base += TEST_I2C_PAGE_SIZE) {
        for (uint32_t i = 0U; i < TEST_I2C_PAGE_SIZE; ++i) {
            expected[i] = sequential ? (uint8_t)(base + i) : 0xFFU;
            actual[i] = 0U;
        }

        fsp_err_t err = test_i2c_read_page((uint8_t)base, actual);
        if (err != FSP_SUCCESS) {
            LOG_E(TAG, "Read failed at 0x%02X: %u", (unsigned int)base, (unsigned int)err);
            return (uint32_t)err;
        }

        for (uint32_t i = 0U; i < TEST_I2C_PAGE_SIZE; ++i) {
            if (actual[i] != expected[i]) {
                LOG_E(
                    TAG,
                    "Verify failed at 0x%02X: expected=0x%02X actual=0x%02X",
                    (unsigned int)(base + i),
                    (unsigned int)expected[i],
                    (unsigned int)actual[i]);
                return 1U;
            }
        }
    }

    return 0U;
}

uint32_t TestI2C(void)
{
    fsp_err_t err;
    uint32_t result = 0U;
    bool opened = false;

    LOG_I(TAG, "AT24C02 I2C test start");

    err = R_IOPORT_PinCfg(g_ioport.p_ctrl, BSP_IO_PORT_05_PIN_12, TEST_I2C_PIN_OPTIONS);
    if (err != FSP_SUCCESS) {
        LOG_E(TAG, "Configure SCL failed: %u", (unsigned int)err);
        return (uint32_t)err;
    }

    err = R_IOPORT_PinCfg(g_ioport.p_ctrl, BSP_IO_PORT_05_PIN_11, TEST_I2C_PIN_OPTIONS);
    if (err != FSP_SUCCESS) {
        LOG_E(TAG, "Configure SDA failed: %u", (unsigned int)err);
        return (uint32_t)err;
    }

    err = (fsp_err_t)IIC_Init(&s_test_i2c);
    if (err != FSP_SUCCESS) {
        LOG_E(TAG, "IIC_Init failed: %u", (unsigned int)err);
        return (uint32_t)err;
    }

    opened = true;

    LOG_I(TAG, "Test 1: full chip write 0xFF");
    result = test_i2c_fill_and_verify(false);
    if (result != 0U) {
        goto exit;
    }
    LOG_I(TAG, "Test 1 PASS");

    LOG_I(TAG, "Test 2: full chip write 0x00..0xFF");
    result = test_i2c_fill_and_verify(true);
    if (result != 0U) {
        goto exit;
    }
    LOG_I(TAG, "Test 2 PASS");

exit:
    if (opened) {
        err = (fsp_err_t)IIC_DeInit(&s_test_i2c);
        if (err != FSP_SUCCESS) {
            LOG_E(TAG, "IIC_DeInit failed: %u", (unsigned int)err);
            if (result == 0U) {
                result = (uint32_t)err;
            }
        }
    }

    if (result == 0U) {
        LOG_I(TAG, "AT24C02 I2C test PASS");
    }
    else {
        LOG_E(TAG, "AT24C02 I2C test FAILED");
    }

    return result;
}

#endif
