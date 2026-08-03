#include "peripheral/pin.h"

static const machine_pin_obj_t machine_pin_P000_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P000,
    .pin = BSP_IO_PORT_00_PIN_00,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P001_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P001,
    .pin = BSP_IO_PORT_00_PIN_01,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P002_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P002,
    .pin = BSP_IO_PORT_00_PIN_02,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P003_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P003,
    .pin = BSP_IO_PORT_00_PIN_03,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P004_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P004,
    .pin = BSP_IO_PORT_00_PIN_04,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P005_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P005,
    .pin = BSP_IO_PORT_00_PIN_05,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P006_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P006,
    .pin = BSP_IO_PORT_00_PIN_06,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P007_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P007,
    .pin = BSP_IO_PORT_00_PIN_07,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P008_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P008,
    .pin = BSP_IO_PORT_00_PIN_08,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P009_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P009,
    .pin = BSP_IO_PORT_00_PIN_09,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P010_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P010,
    .pin = BSP_IO_PORT_00_PIN_10,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P011_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P011,
    .pin = BSP_IO_PORT_00_PIN_11,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P014_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P014,
    .pin = BSP_IO_PORT_00_PIN_14,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P015_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P015,
    .pin = BSP_IO_PORT_00_PIN_15,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P100_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P100,
    .pin = BSP_IO_PORT_01_PIN_00,
    .alt_mask = 0x1040406eU,
};

static const machine_pin_obj_t machine_pin_P101_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P101,
    .pin = BSP_IO_PORT_01_PIN_01,
    .alt_mask = 0x1040006eU,
};

static const machine_pin_obj_t machine_pin_P102_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P102,
    .pin = BSP_IO_PORT_01_PIN_02,
    .alt_mask = 0x1001046eU,
};

static const machine_pin_obj_t machine_pin_P103_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P103,
    .pin = BSP_IO_PORT_01_PIN_03,
    .alt_mask = 0x1041446cU,
};

static const machine_pin_obj_t machine_pin_P104_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P104,
    .pin = BSP_IO_PORT_01_PIN_04,
    .alt_mask = 0x1040046cU,
};

static const machine_pin_obj_t machine_pin_P105_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P105,
    .pin = BSP_IO_PORT_01_PIN_05,
    .alt_mask = 0x50402458U,
};

static const machine_pin_obj_t machine_pin_P106_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P106,
    .pin = BSP_IO_PORT_01_PIN_06,
    .alt_mask = 0x5040045eU,
};

static const machine_pin_obj_t machine_pin_P107_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P107,
    .pin = BSP_IO_PORT_01_PIN_07,
    .alt_mask = 0x1040041eU,
};

static const machine_pin_obj_t machine_pin_P109_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P109,
    .pin = BSP_IO_PORT_01_PIN_09,
    .alt_mask = 0x00200008U,
};

static const machine_pin_obj_t machine_pin_P200_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P200,
    .pin = BSP_IO_PORT_02_PIN_00,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P201_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P201,
    .pin = BSP_IO_PORT_02_PIN_01,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P206_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P206,
    .pin = BSP_IO_PORT_02_PIN_06,
    .alt_mask = 0x41ec8a0cU,
};

static const machine_pin_obj_t machine_pin_P207_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P207,
    .pin = BSP_IO_PORT_02_PIN_07,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_P304_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P304,
    .pin = BSP_IO_PORT_03_PIN_04,
    .alt_mask = 0x4160800cU,
};

static const machine_pin_obj_t machine_pin_P305_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P305,
    .pin = BSP_IO_PORT_03_PIN_05,
    .alt_mask = 0x4160800cU,
};

static const machine_pin_obj_t machine_pin_P306_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P306,
    .pin = BSP_IO_PORT_03_PIN_06,
    .alt_mask = 0x41e0800cU,
};

static const machine_pin_obj_t machine_pin_P307_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P307,
    .pin = BSP_IO_PORT_03_PIN_07,
    .alt_mask = 0x41e0a01cU,
};

static const machine_pin_obj_t machine_pin_P308_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P308,
    .pin = BSP_IO_PORT_03_PIN_08,
    .alt_mask = 0x4060822cU,
};

static const machine_pin_obj_t machine_pin_P309_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P309,
    .pin = BSP_IO_PORT_03_PIN_09,
    .alt_mask = 0x03408228U,
};

static const machine_pin_obj_t machine_pin_P310_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P310,
    .pin = BSP_IO_PORT_03_PIN_10,
    .alt_mask = 0x03c0802aU,
};

static const machine_pin_obj_t machine_pin_P311_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P311,
    .pin = BSP_IO_PORT_03_PIN_11,
    .alt_mask = 0x0241c02eU,
};

static const machine_pin_obj_t machine_pin_P312_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P312,
    .pin = BSP_IO_PORT_03_PIN_12,
    .alt_mask = 0x00000000U,
};

static const machine_pin_obj_t machine_pin_P400_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P400,
    .pin = BSP_IO_PORT_04_PIN_00,
    .alt_mask = 0x002484aaU,
};

static const machine_pin_obj_t machine_pin_P401_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P401,
    .pin = BSP_IO_PORT_04_PIN_01,
    .alt_mask = 0x002180acU,
};

static const machine_pin_obj_t machine_pin_P402_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P402,
    .pin = BSP_IO_PORT_04_PIN_02,
    .alt_mask = 0x00654420U,
};

static const machine_pin_obj_t machine_pin_P403_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P403,
    .pin = BSP_IO_PORT_04_PIN_03,
    .alt_mask = 0x00644428U,
};

static const machine_pin_obj_t machine_pin_P404_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P404,
    .pin = BSP_IO_PORT_04_PIN_04,
    .alt_mask = 0x00640428U,
};

static const machine_pin_obj_t machine_pin_P405_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P405,
    .pin = BSP_IO_PORT_04_PIN_05,
    .alt_mask = 0x01e4a01aU,
};

static const machine_pin_obj_t machine_pin_P406_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P406,
    .pin = BSP_IO_PORT_04_PIN_06,
    .alt_mask = 0x01648058U,
};

static const machine_pin_obj_t machine_pin_P413_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P413,
    .pin = BSP_IO_PORT_04_PIN_13,
    .alt_mask = 0x4040080cU,
};

static const machine_pin_obj_t machine_pin_P414_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P414,
    .pin = BSP_IO_PORT_04_PIN_14,
    .alt_mask = 0x00418858U,
};

static const machine_pin_obj_t machine_pin_P415_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P415,
    .pin = BSP_IO_PORT_04_PIN_15,
    .alt_mask = 0x00418858U,
};

static const machine_pin_obj_t machine_pin_P511_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P511,
    .pin = BSP_IO_PORT_05_PIN_11,
    .alt_mask = 0x00412098U,
};

static const machine_pin_obj_t machine_pin_P512_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P512,
    .pin = BSP_IO_PORT_05_PIN_12,
    .alt_mask = 0x00410098U,
};

static const machine_pin_obj_t machine_pin_P513_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P513,
    .pin = BSP_IO_PORT_05_PIN_13,
    .alt_mask = 0x0240a018U,
};

static const machine_pin_obj_t machine_pin_P514_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P514,
    .pin = BSP_IO_PORT_05_PIN_14,
    .alt_mask = 0x02402098U,
};

static const machine_pin_obj_t machine_pin_P515_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P515,
    .pin = BSP_IO_PORT_05_PIN_15,
    .alt_mask = 0x02402098U,
};

static const machine_pin_obj_t machine_pin_P600_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P600,
    .pin = BSP_IO_PORT_06_PIN_00,
    .alt_mask = 0x50400408U,
};

static const machine_pin_obj_t machine_pin_P601_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P601,
    .pin = BSP_IO_PORT_06_PIN_01,
    .alt_mask = 0x50002218U,
};

static const machine_pin_obj_t machine_pin_P700_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P700,
    .pin = BSP_IO_PORT_07_PIN_00,
    .alt_mask = 0x01648058U,
};

static const machine_pin_obj_t machine_pin_P701_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P701,
    .pin = BSP_IO_PORT_07_PIN_01,
    .alt_mask = 0x41e4a058U,
};

static const machine_pin_obj_t machine_pin_P702_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P702,
    .pin = BSP_IO_PORT_07_PIN_02,
    .alt_mask = 0x41e48058U,
};

static const machine_pin_obj_t machine_pin_P703_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P703,
    .pin = BSP_IO_PORT_07_PIN_03,
    .alt_mask = 0x09e0824aU,
};

static const machine_pin_obj_t machine_pin_P704_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P704,
    .pin = BSP_IO_PORT_07_PIN_04,
    .alt_mask = 0x08e1804eU,
};

static const machine_pin_obj_t machine_pin_P705_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P705,
    .pin = BSP_IO_PORT_07_PIN_05,
    .alt_mask = 0x09c1806eU,
};

static const machine_pin_obj_t machine_pin_P706_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P706,
    .pin = BSP_IO_PORT_07_PIN_06,
    .alt_mask = 0x0950822aU,
};

static const machine_pin_obj_t machine_pin_P707_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P707,
    .pin = BSP_IO_PORT_07_PIN_07,
    .alt_mask = 0x0a508228U,
};

static const machine_pin_obj_t machine_pin_P708_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P708,
    .pin = BSP_IO_PORT_07_PIN_08,
    .alt_mask = 0x0044acd8U,
};

static const machine_pin_obj_t machine_pin_P709_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P709,
    .pin = BSP_IO_PORT_07_PIN_09,
    .alt_mask = 0x0040a8d8U,
};

static const machine_pin_obj_t machine_pin_P710_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P710,
    .pin = BSP_IO_PORT_07_PIN_10,
    .alt_mask = 0x02408858U,
};

static const machine_pin_obj_t machine_pin_P711_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P711,
    .pin = BSP_IO_PORT_07_PIN_11,
    .alt_mask = 0x0240406aU,
};

static const machine_pin_obj_t machine_pin_P712_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P712,
    .pin = BSP_IO_PORT_07_PIN_12,
    .alt_mask = 0x0240006aU,
};

static const machine_pin_obj_t machine_pin_P713_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P713,
    .pin = BSP_IO_PORT_07_PIN_13,
    .alt_mask = 0x0240001aU,
};

static const machine_pin_obj_t machine_pin_P714_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P714,
    .pin = BSP_IO_PORT_07_PIN_14,
    .alt_mask = 0x82400018U,
};

static const machine_pin_obj_t machine_pin_P715_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P715,
    .pin = BSP_IO_PORT_07_PIN_15,
    .alt_mask = 0x02400018U,
};

static const machine_pin_obj_t machine_pin_P800_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P800,
    .pin = BSP_IO_PORT_08_PIN_00,
    .alt_mask = 0x1000001eU,
};

static const machine_pin_obj_t machine_pin_P801_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P801,
    .pin = BSP_IO_PORT_08_PIN_01,
    .alt_mask = 0x1040001eU,
};

static const machine_pin_obj_t machine_pin_P802_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P802,
    .pin = BSP_IO_PORT_08_PIN_02,
    .alt_mask = 0x1000001cU,
};

static const machine_pin_obj_t machine_pin_P803_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P803,
    .pin = BSP_IO_PORT_08_PIN_03,
    .alt_mask = 0x1000201cU,
};

static const machine_pin_obj_t machine_pin_P804_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P804,
    .pin = BSP_IO_PORT_08_PIN_04,
    .alt_mask = 0x9000201cU,
};

static const machine_pin_obj_t machine_pin_P805_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P805,
    .pin = BSP_IO_PORT_08_PIN_05,
    .alt_mask = 0x02408010U,
};

static const machine_pin_obj_t machine_pin_P806_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P806,
    .pin = BSP_IO_PORT_08_PIN_06,
    .alt_mask = 0x02408010U,
};

static const machine_pin_obj_t machine_pin_P807_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P807,
    .pin = BSP_IO_PORT_08_PIN_07,
    .alt_mask = 0x02000008U,
};

static const machine_pin_obj_t machine_pin_P810_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P810,
    .pin = BSP_IO_PORT_08_PIN_10,
    .alt_mask = 0x48204028U,
};

static const machine_pin_obj_t machine_pin_P811_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P811,
    .pin = BSP_IO_PORT_08_PIN_11,
    .alt_mask = 0x48280028U,
};

static const machine_pin_obj_t machine_pin_P812_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P812,
    .pin = BSP_IO_PORT_08_PIN_12,
    .alt_mask = 0x08284028U,
};

static const machine_pin_obj_t machine_pin_P902_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P902,
    .pin = BSP_IO_PORT_09_PIN_02,
    .alt_mask = 0x02048a08U,
};

static const machine_pin_obj_t machine_pin_P903_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P903,
    .pin = BSP_IO_PORT_09_PIN_03,
    .alt_mask = 0x02000008U,
};

static const machine_pin_obj_t machine_pin_P904_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P904,
    .pin = BSP_IO_PORT_09_PIN_04,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_P905_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P905,
    .pin = BSP_IO_PORT_09_PIN_05,
    .alt_mask = 0x0bc08428U,
};

static const machine_pin_obj_t machine_pin_P906_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P906,
    .pin = BSP_IO_PORT_09_PIN_06,
    .alt_mask = 0x4bcc8418U,
};

static const machine_pin_obj_t machine_pin_P907_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P907,
    .pin = BSP_IO_PORT_09_PIN_07,
    .alt_mask = 0x4bcca418U,
};

static const machine_pin_obj_t machine_pin_P908_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P908,
    .pin = BSP_IO_PORT_09_PIN_08,
    .alt_mask = 0x4b598418U,
};

static const machine_pin_obj_t machine_pin_P909_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P909,
    .pin = BSP_IO_PORT_09_PIN_09,
    .alt_mask = 0x4b598418U,
};

static const machine_pin_obj_t machine_pin_P910_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P910,
    .pin = BSP_IO_PORT_09_PIN_10,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_P911_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P911,
    .pin = BSP_IO_PORT_09_PIN_11,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_P912_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P912,
    .pin = BSP_IO_PORT_09_PIN_12,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_P913_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P913,
    .pin = BSP_IO_PORT_09_PIN_13,
    .alt_mask = 0x02400208U,
};

static const machine_pin_obj_t machine_pin_P914_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P914,
    .pin = BSP_IO_PORT_09_PIN_14,
    .alt_mask = 0x02002018U,
};

static const machine_pin_obj_t machine_pin_P915_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_P915,
    .pin = BSP_IO_PORT_09_PIN_15,
    .alt_mask = 0x02000018U,
};

static const machine_pin_obj_t machine_pin_PA07_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PA07,
    .pin = BSP_IO_PORT_10_PIN_07,
    .alt_mask = 0x08208a2cU,
};

static const machine_pin_obj_t machine_pin_PB00_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB00,
    .pin = BSP_IO_PORT_11_PIN_00,
    .alt_mask = 0x0bd04428U,
};

static const machine_pin_obj_t machine_pin_PB01_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB01,
    .pin = BSP_IO_PORT_11_PIN_01,
    .alt_mask = 0x0240cc28U,
};

static const machine_pin_obj_t machine_pin_PB02_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB02,
    .pin = BSP_IO_PORT_11_PIN_02,
    .alt_mask = 0x03c08428U,
};

static const machine_pin_obj_t machine_pin_PB03_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB03,
    .pin = BSP_IO_PORT_11_PIN_03,
    .alt_mask = 0x03408428U,
};

static const machine_pin_obj_t machine_pin_PB04_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB04,
    .pin = BSP_IO_PORT_11_PIN_04,
    .alt_mask = 0x0340c428U,
};

static const machine_pin_obj_t machine_pin_PB05_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB05,
    .pin = BSP_IO_PORT_11_PIN_05,
    .alt_mask = 0x02400028U,
};

static const machine_pin_obj_t machine_pin_PB06_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB06,
    .pin = BSP_IO_PORT_11_PIN_06,
    .alt_mask = 0x02404028U,
};

static const machine_pin_obj_t machine_pin_PB07_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PB07,
    .pin = BSP_IO_PORT_11_PIN_07,
    .alt_mask = 0x02400008U,
};

static const machine_pin_obj_t machine_pin_PC09_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC09,
    .pin = BSP_IO_PORT_12_PIN_09,
    .alt_mask = 0x10000800U,
};

static const machine_pin_obj_t machine_pin_PC10_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC10,
    .pin = BSP_IO_PORT_12_PIN_10,
    .alt_mask = 0x10000808U,
};

static const machine_pin_obj_t machine_pin_PC11_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC11,
    .pin = BSP_IO_PORT_12_PIN_11,
    .alt_mask = 0x00402818U,
};

static const machine_pin_obj_t machine_pin_PC12_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC12,
    .pin = BSP_IO_PORT_12_PIN_12,
    .alt_mask = 0x00402818U,
};

static const machine_pin_obj_t machine_pin_PC13_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC13,
    .pin = BSP_IO_PORT_12_PIN_13,
    .alt_mask = 0x00400818U,
};

static const machine_pin_obj_t machine_pin_PC14_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC14,
    .pin = BSP_IO_PORT_12_PIN_14,
    .alt_mask = 0x0040081cU,
};

static const machine_pin_obj_t machine_pin_PC15_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PC15,
    .pin = BSP_IO_PORT_12_PIN_15,
    .alt_mask = 0x00010814U,
};

static const machine_pin_obj_t machine_pin_PD06_obj = {
    .base = { &machine_pin_type },
    .name = MP_QSTR_PD06,
    .pin = BSP_IO_PORT_13_PIN_06,
    .alt_mask = 0x00700008U,
};

static const mp_rom_map_elem_t machine_pin_cpu_pins_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_P000), MP_ROM_PTR(&machine_pin_P000_obj) },
    { MP_ROM_QSTR(MP_QSTR_P001), MP_ROM_PTR(&machine_pin_P001_obj) },
    { MP_ROM_QSTR(MP_QSTR_P002), MP_ROM_PTR(&machine_pin_P002_obj) },
    { MP_ROM_QSTR(MP_QSTR_P003), MP_ROM_PTR(&machine_pin_P003_obj) },
    { MP_ROM_QSTR(MP_QSTR_P004), MP_ROM_PTR(&machine_pin_P004_obj) },
    { MP_ROM_QSTR(MP_QSTR_P005), MP_ROM_PTR(&machine_pin_P005_obj) },
    { MP_ROM_QSTR(MP_QSTR_P006), MP_ROM_PTR(&machine_pin_P006_obj) },
    { MP_ROM_QSTR(MP_QSTR_P007), MP_ROM_PTR(&machine_pin_P007_obj) },
    { MP_ROM_QSTR(MP_QSTR_P008), MP_ROM_PTR(&machine_pin_P008_obj) },
    { MP_ROM_QSTR(MP_QSTR_P009), MP_ROM_PTR(&machine_pin_P009_obj) },
    { MP_ROM_QSTR(MP_QSTR_P010), MP_ROM_PTR(&machine_pin_P010_obj) },
    { MP_ROM_QSTR(MP_QSTR_P011), MP_ROM_PTR(&machine_pin_P011_obj) },
    { MP_ROM_QSTR(MP_QSTR_P014), MP_ROM_PTR(&machine_pin_P014_obj) },
    { MP_ROM_QSTR(MP_QSTR_P015), MP_ROM_PTR(&machine_pin_P015_obj) },
    { MP_ROM_QSTR(MP_QSTR_P100), MP_ROM_PTR(&machine_pin_P100_obj) },
    { MP_ROM_QSTR(MP_QSTR_P101), MP_ROM_PTR(&machine_pin_P101_obj) },
    { MP_ROM_QSTR(MP_QSTR_P102), MP_ROM_PTR(&machine_pin_P102_obj) },
    { MP_ROM_QSTR(MP_QSTR_P103), MP_ROM_PTR(&machine_pin_P103_obj) },
    { MP_ROM_QSTR(MP_QSTR_P104), MP_ROM_PTR(&machine_pin_P104_obj) },
    { MP_ROM_QSTR(MP_QSTR_P105), MP_ROM_PTR(&machine_pin_P105_obj) },
    { MP_ROM_QSTR(MP_QSTR_P106), MP_ROM_PTR(&machine_pin_P106_obj) },
    { MP_ROM_QSTR(MP_QSTR_P107), MP_ROM_PTR(&machine_pin_P107_obj) },
    { MP_ROM_QSTR(MP_QSTR_P109), MP_ROM_PTR(&machine_pin_P109_obj) },
    { MP_ROM_QSTR(MP_QSTR_P200), MP_ROM_PTR(&machine_pin_P200_obj) },
    { MP_ROM_QSTR(MP_QSTR_P201), MP_ROM_PTR(&machine_pin_P201_obj) },
    { MP_ROM_QSTR(MP_QSTR_P206), MP_ROM_PTR(&machine_pin_P206_obj) },
    { MP_ROM_QSTR(MP_QSTR_P207), MP_ROM_PTR(&machine_pin_P207_obj) },
    { MP_ROM_QSTR(MP_QSTR_P304), MP_ROM_PTR(&machine_pin_P304_obj) },
    { MP_ROM_QSTR(MP_QSTR_P305), MP_ROM_PTR(&machine_pin_P305_obj) },
    { MP_ROM_QSTR(MP_QSTR_P306), MP_ROM_PTR(&machine_pin_P306_obj) },
    { MP_ROM_QSTR(MP_QSTR_P307), MP_ROM_PTR(&machine_pin_P307_obj) },
    { MP_ROM_QSTR(MP_QSTR_P308), MP_ROM_PTR(&machine_pin_P308_obj) },
    { MP_ROM_QSTR(MP_QSTR_P309), MP_ROM_PTR(&machine_pin_P309_obj) },
    { MP_ROM_QSTR(MP_QSTR_P310), MP_ROM_PTR(&machine_pin_P310_obj) },
    { MP_ROM_QSTR(MP_QSTR_P311), MP_ROM_PTR(&machine_pin_P311_obj) },
    { MP_ROM_QSTR(MP_QSTR_P312), MP_ROM_PTR(&machine_pin_P312_obj) },
    { MP_ROM_QSTR(MP_QSTR_P400), MP_ROM_PTR(&machine_pin_P400_obj) },
    { MP_ROM_QSTR(MP_QSTR_P401), MP_ROM_PTR(&machine_pin_P401_obj) },
    { MP_ROM_QSTR(MP_QSTR_P402), MP_ROM_PTR(&machine_pin_P402_obj) },
    { MP_ROM_QSTR(MP_QSTR_P403), MP_ROM_PTR(&machine_pin_P403_obj) },
    { MP_ROM_QSTR(MP_QSTR_P404), MP_ROM_PTR(&machine_pin_P404_obj) },
    { MP_ROM_QSTR(MP_QSTR_P405), MP_ROM_PTR(&machine_pin_P405_obj) },
    { MP_ROM_QSTR(MP_QSTR_P406), MP_ROM_PTR(&machine_pin_P406_obj) },
    { MP_ROM_QSTR(MP_QSTR_P413), MP_ROM_PTR(&machine_pin_P413_obj) },
    { MP_ROM_QSTR(MP_QSTR_P414), MP_ROM_PTR(&machine_pin_P414_obj) },
    { MP_ROM_QSTR(MP_QSTR_P415), MP_ROM_PTR(&machine_pin_P415_obj) },
    { MP_ROM_QSTR(MP_QSTR_P511), MP_ROM_PTR(&machine_pin_P511_obj) },
    { MP_ROM_QSTR(MP_QSTR_P512), MP_ROM_PTR(&machine_pin_P512_obj) },
    { MP_ROM_QSTR(MP_QSTR_P513), MP_ROM_PTR(&machine_pin_P513_obj) },
    { MP_ROM_QSTR(MP_QSTR_P514), MP_ROM_PTR(&machine_pin_P514_obj) },
    { MP_ROM_QSTR(MP_QSTR_P515), MP_ROM_PTR(&machine_pin_P515_obj) },
    { MP_ROM_QSTR(MP_QSTR_P600), MP_ROM_PTR(&machine_pin_P600_obj) },
    { MP_ROM_QSTR(MP_QSTR_P601), MP_ROM_PTR(&machine_pin_P601_obj) },
    { MP_ROM_QSTR(MP_QSTR_P700), MP_ROM_PTR(&machine_pin_P700_obj) },
    { MP_ROM_QSTR(MP_QSTR_P701), MP_ROM_PTR(&machine_pin_P701_obj) },
    { MP_ROM_QSTR(MP_QSTR_P702), MP_ROM_PTR(&machine_pin_P702_obj) },
    { MP_ROM_QSTR(MP_QSTR_P703), MP_ROM_PTR(&machine_pin_P703_obj) },
    { MP_ROM_QSTR(MP_QSTR_P704), MP_ROM_PTR(&machine_pin_P704_obj) },
    { MP_ROM_QSTR(MP_QSTR_P705), MP_ROM_PTR(&machine_pin_P705_obj) },
    { MP_ROM_QSTR(MP_QSTR_P706), MP_ROM_PTR(&machine_pin_P706_obj) },
    { MP_ROM_QSTR(MP_QSTR_P707), MP_ROM_PTR(&machine_pin_P707_obj) },
    { MP_ROM_QSTR(MP_QSTR_P708), MP_ROM_PTR(&machine_pin_P708_obj) },
    { MP_ROM_QSTR(MP_QSTR_P709), MP_ROM_PTR(&machine_pin_P709_obj) },
    { MP_ROM_QSTR(MP_QSTR_P710), MP_ROM_PTR(&machine_pin_P710_obj) },
    { MP_ROM_QSTR(MP_QSTR_P711), MP_ROM_PTR(&machine_pin_P711_obj) },
    { MP_ROM_QSTR(MP_QSTR_P712), MP_ROM_PTR(&machine_pin_P712_obj) },
    { MP_ROM_QSTR(MP_QSTR_P713), MP_ROM_PTR(&machine_pin_P713_obj) },
    { MP_ROM_QSTR(MP_QSTR_P714), MP_ROM_PTR(&machine_pin_P714_obj) },
    { MP_ROM_QSTR(MP_QSTR_P715), MP_ROM_PTR(&machine_pin_P715_obj) },
    { MP_ROM_QSTR(MP_QSTR_P800), MP_ROM_PTR(&machine_pin_P800_obj) },
    { MP_ROM_QSTR(MP_QSTR_P801), MP_ROM_PTR(&machine_pin_P801_obj) },
    { MP_ROM_QSTR(MP_QSTR_P802), MP_ROM_PTR(&machine_pin_P802_obj) },
    { MP_ROM_QSTR(MP_QSTR_P803), MP_ROM_PTR(&machine_pin_P803_obj) },
    { MP_ROM_QSTR(MP_QSTR_P804), MP_ROM_PTR(&machine_pin_P804_obj) },
    { MP_ROM_QSTR(MP_QSTR_P805), MP_ROM_PTR(&machine_pin_P805_obj) },
    { MP_ROM_QSTR(MP_QSTR_P806), MP_ROM_PTR(&machine_pin_P806_obj) },
    { MP_ROM_QSTR(MP_QSTR_P807), MP_ROM_PTR(&machine_pin_P807_obj) },
    { MP_ROM_QSTR(MP_QSTR_P810), MP_ROM_PTR(&machine_pin_P810_obj) },
    { MP_ROM_QSTR(MP_QSTR_P811), MP_ROM_PTR(&machine_pin_P811_obj) },
    { MP_ROM_QSTR(MP_QSTR_P812), MP_ROM_PTR(&machine_pin_P812_obj) },
    { MP_ROM_QSTR(MP_QSTR_P902), MP_ROM_PTR(&machine_pin_P902_obj) },
    { MP_ROM_QSTR(MP_QSTR_P903), MP_ROM_PTR(&machine_pin_P903_obj) },
    { MP_ROM_QSTR(MP_QSTR_P904), MP_ROM_PTR(&machine_pin_P904_obj) },
    { MP_ROM_QSTR(MP_QSTR_P905), MP_ROM_PTR(&machine_pin_P905_obj) },
    { MP_ROM_QSTR(MP_QSTR_P906), MP_ROM_PTR(&machine_pin_P906_obj) },
    { MP_ROM_QSTR(MP_QSTR_P907), MP_ROM_PTR(&machine_pin_P907_obj) },
    { MP_ROM_QSTR(MP_QSTR_P908), MP_ROM_PTR(&machine_pin_P908_obj) },
    { MP_ROM_QSTR(MP_QSTR_P909), MP_ROM_PTR(&machine_pin_P909_obj) },
    { MP_ROM_QSTR(MP_QSTR_P910), MP_ROM_PTR(&machine_pin_P910_obj) },
    { MP_ROM_QSTR(MP_QSTR_P911), MP_ROM_PTR(&machine_pin_P911_obj) },
    { MP_ROM_QSTR(MP_QSTR_P912), MP_ROM_PTR(&machine_pin_P912_obj) },
    { MP_ROM_QSTR(MP_QSTR_P913), MP_ROM_PTR(&machine_pin_P913_obj) },
    { MP_ROM_QSTR(MP_QSTR_P914), MP_ROM_PTR(&machine_pin_P914_obj) },
    { MP_ROM_QSTR(MP_QSTR_P915), MP_ROM_PTR(&machine_pin_P915_obj) },
    { MP_ROM_QSTR(MP_QSTR_PA07), MP_ROM_PTR(&machine_pin_PA07_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB00), MP_ROM_PTR(&machine_pin_PB00_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB01), MP_ROM_PTR(&machine_pin_PB01_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB02), MP_ROM_PTR(&machine_pin_PB02_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB03), MP_ROM_PTR(&machine_pin_PB03_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB04), MP_ROM_PTR(&machine_pin_PB04_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB05), MP_ROM_PTR(&machine_pin_PB05_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB06), MP_ROM_PTR(&machine_pin_PB06_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB07), MP_ROM_PTR(&machine_pin_PB07_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC09), MP_ROM_PTR(&machine_pin_PC09_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC10), MP_ROM_PTR(&machine_pin_PC10_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC11), MP_ROM_PTR(&machine_pin_PC11_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC12), MP_ROM_PTR(&machine_pin_PC12_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC13), MP_ROM_PTR(&machine_pin_PC13_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC14), MP_ROM_PTR(&machine_pin_PC14_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC15), MP_ROM_PTR(&machine_pin_PC15_obj) },
    { MP_ROM_QSTR(MP_QSTR_PD06), MP_ROM_PTR(&machine_pin_PD06_obj) },
};
MP_DEFINE_CONST_DICT(machine_pin_cpu_pins_locals_dict, machine_pin_cpu_pins_locals_dict_table);

static const mp_rom_map_elem_t machine_pin_board_pins_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_P000), MP_ROM_PTR(&machine_pin_P000_obj) },
    { MP_ROM_QSTR(MP_QSTR_P001), MP_ROM_PTR(&machine_pin_P001_obj) },
    { MP_ROM_QSTR(MP_QSTR_P002), MP_ROM_PTR(&machine_pin_P002_obj) },
    { MP_ROM_QSTR(MP_QSTR_P003), MP_ROM_PTR(&machine_pin_P003_obj) },
    { MP_ROM_QSTR(MP_QSTR_P004), MP_ROM_PTR(&machine_pin_P004_obj) },
    { MP_ROM_QSTR(MP_QSTR_P005), MP_ROM_PTR(&machine_pin_P005_obj) },
    { MP_ROM_QSTR(MP_QSTR_P006), MP_ROM_PTR(&machine_pin_P006_obj) },
    { MP_ROM_QSTR(MP_QSTR_P007), MP_ROM_PTR(&machine_pin_P007_obj) },
    { MP_ROM_QSTR(MP_QSTR_P008), MP_ROM_PTR(&machine_pin_P008_obj) },
    { MP_ROM_QSTR(MP_QSTR_P009), MP_ROM_PTR(&machine_pin_P009_obj) },
    { MP_ROM_QSTR(MP_QSTR_P010), MP_ROM_PTR(&machine_pin_P010_obj) },
    { MP_ROM_QSTR(MP_QSTR_P011), MP_ROM_PTR(&machine_pin_P011_obj) },
    { MP_ROM_QSTR(MP_QSTR_P014), MP_ROM_PTR(&machine_pin_P014_obj) },
    { MP_ROM_QSTR(MP_QSTR_P015), MP_ROM_PTR(&machine_pin_P015_obj) },
    { MP_ROM_QSTR(MP_QSTR_P100), MP_ROM_PTR(&machine_pin_P100_obj) },
    { MP_ROM_QSTR(MP_QSTR_P101), MP_ROM_PTR(&machine_pin_P101_obj) },
    { MP_ROM_QSTR(MP_QSTR_P102), MP_ROM_PTR(&machine_pin_P102_obj) },
    { MP_ROM_QSTR(MP_QSTR_P103), MP_ROM_PTR(&machine_pin_P103_obj) },
    { MP_ROM_QSTR(MP_QSTR_P104), MP_ROM_PTR(&machine_pin_P104_obj) },
    { MP_ROM_QSTR(MP_QSTR_P105), MP_ROM_PTR(&machine_pin_P105_obj) },
    { MP_ROM_QSTR(MP_QSTR_P106), MP_ROM_PTR(&machine_pin_P106_obj) },
    { MP_ROM_QSTR(MP_QSTR_P107), MP_ROM_PTR(&machine_pin_P107_obj) },
    { MP_ROM_QSTR(MP_QSTR_P109), MP_ROM_PTR(&machine_pin_P109_obj) },
    { MP_ROM_QSTR(MP_QSTR_P200), MP_ROM_PTR(&machine_pin_P200_obj) },
    { MP_ROM_QSTR(MP_QSTR_P201), MP_ROM_PTR(&machine_pin_P201_obj) },
    { MP_ROM_QSTR(MP_QSTR_P206), MP_ROM_PTR(&machine_pin_P206_obj) },
    { MP_ROM_QSTR(MP_QSTR_P207), MP_ROM_PTR(&machine_pin_P207_obj) },
    { MP_ROM_QSTR(MP_QSTR_P304), MP_ROM_PTR(&machine_pin_P304_obj) },
    { MP_ROM_QSTR(MP_QSTR_P305), MP_ROM_PTR(&machine_pin_P305_obj) },
    { MP_ROM_QSTR(MP_QSTR_P306), MP_ROM_PTR(&machine_pin_P306_obj) },
    { MP_ROM_QSTR(MP_QSTR_P307), MP_ROM_PTR(&machine_pin_P307_obj) },
    { MP_ROM_QSTR(MP_QSTR_P308), MP_ROM_PTR(&machine_pin_P308_obj) },
    { MP_ROM_QSTR(MP_QSTR_P309), MP_ROM_PTR(&machine_pin_P309_obj) },
    { MP_ROM_QSTR(MP_QSTR_P310), MP_ROM_PTR(&machine_pin_P310_obj) },
    { MP_ROM_QSTR(MP_QSTR_P311), MP_ROM_PTR(&machine_pin_P311_obj) },
    { MP_ROM_QSTR(MP_QSTR_P312), MP_ROM_PTR(&machine_pin_P312_obj) },
    { MP_ROM_QSTR(MP_QSTR_P400), MP_ROM_PTR(&machine_pin_P400_obj) },
    { MP_ROM_QSTR(MP_QSTR_P401), MP_ROM_PTR(&machine_pin_P401_obj) },
    { MP_ROM_QSTR(MP_QSTR_P402), MP_ROM_PTR(&machine_pin_P402_obj) },
    { MP_ROM_QSTR(MP_QSTR_P403), MP_ROM_PTR(&machine_pin_P403_obj) },
    { MP_ROM_QSTR(MP_QSTR_P404), MP_ROM_PTR(&machine_pin_P404_obj) },
    { MP_ROM_QSTR(MP_QSTR_P405), MP_ROM_PTR(&machine_pin_P405_obj) },
    { MP_ROM_QSTR(MP_QSTR_P406), MP_ROM_PTR(&machine_pin_P406_obj) },
    { MP_ROM_QSTR(MP_QSTR_P413), MP_ROM_PTR(&machine_pin_P413_obj) },
    { MP_ROM_QSTR(MP_QSTR_P414), MP_ROM_PTR(&machine_pin_P414_obj) },
    { MP_ROM_QSTR(MP_QSTR_P415), MP_ROM_PTR(&machine_pin_P415_obj) },
    { MP_ROM_QSTR(MP_QSTR_P511), MP_ROM_PTR(&machine_pin_P511_obj) },
    { MP_ROM_QSTR(MP_QSTR_P512), MP_ROM_PTR(&machine_pin_P512_obj) },
    { MP_ROM_QSTR(MP_QSTR_P513), MP_ROM_PTR(&machine_pin_P513_obj) },
    { MP_ROM_QSTR(MP_QSTR_P514), MP_ROM_PTR(&machine_pin_P514_obj) },
    { MP_ROM_QSTR(MP_QSTR_P515), MP_ROM_PTR(&machine_pin_P515_obj) },
    { MP_ROM_QSTR(MP_QSTR_P600), MP_ROM_PTR(&machine_pin_P600_obj) },
    { MP_ROM_QSTR(MP_QSTR_P601), MP_ROM_PTR(&machine_pin_P601_obj) },
    { MP_ROM_QSTR(MP_QSTR_P700), MP_ROM_PTR(&machine_pin_P700_obj) },
    { MP_ROM_QSTR(MP_QSTR_P701), MP_ROM_PTR(&machine_pin_P701_obj) },
    { MP_ROM_QSTR(MP_QSTR_P702), MP_ROM_PTR(&machine_pin_P702_obj) },
    { MP_ROM_QSTR(MP_QSTR_P703), MP_ROM_PTR(&machine_pin_P703_obj) },
    { MP_ROM_QSTR(MP_QSTR_P704), MP_ROM_PTR(&machine_pin_P704_obj) },
    { MP_ROM_QSTR(MP_QSTR_P705), MP_ROM_PTR(&machine_pin_P705_obj) },
    { MP_ROM_QSTR(MP_QSTR_P706), MP_ROM_PTR(&machine_pin_P706_obj) },
    { MP_ROM_QSTR(MP_QSTR_P707), MP_ROM_PTR(&machine_pin_P707_obj) },
    { MP_ROM_QSTR(MP_QSTR_P708), MP_ROM_PTR(&machine_pin_P708_obj) },
    { MP_ROM_QSTR(MP_QSTR_P709), MP_ROM_PTR(&machine_pin_P709_obj) },
    { MP_ROM_QSTR(MP_QSTR_P710), MP_ROM_PTR(&machine_pin_P710_obj) },
    { MP_ROM_QSTR(MP_QSTR_P711), MP_ROM_PTR(&machine_pin_P711_obj) },
    { MP_ROM_QSTR(MP_QSTR_P712), MP_ROM_PTR(&machine_pin_P712_obj) },
    { MP_ROM_QSTR(MP_QSTR_P713), MP_ROM_PTR(&machine_pin_P713_obj) },
    { MP_ROM_QSTR(MP_QSTR_P714), MP_ROM_PTR(&machine_pin_P714_obj) },
    { MP_ROM_QSTR(MP_QSTR_P715), MP_ROM_PTR(&machine_pin_P715_obj) },
    { MP_ROM_QSTR(MP_QSTR_P800), MP_ROM_PTR(&machine_pin_P800_obj) },
    { MP_ROM_QSTR(MP_QSTR_P801), MP_ROM_PTR(&machine_pin_P801_obj) },
    { MP_ROM_QSTR(MP_QSTR_P802), MP_ROM_PTR(&machine_pin_P802_obj) },
    { MP_ROM_QSTR(MP_QSTR_P803), MP_ROM_PTR(&machine_pin_P803_obj) },
    { MP_ROM_QSTR(MP_QSTR_P804), MP_ROM_PTR(&machine_pin_P804_obj) },
    { MP_ROM_QSTR(MP_QSTR_P805), MP_ROM_PTR(&machine_pin_P805_obj) },
    { MP_ROM_QSTR(MP_QSTR_P806), MP_ROM_PTR(&machine_pin_P806_obj) },
    { MP_ROM_QSTR(MP_QSTR_P807), MP_ROM_PTR(&machine_pin_P807_obj) },
    { MP_ROM_QSTR(MP_QSTR_P810), MP_ROM_PTR(&machine_pin_P810_obj) },
    { MP_ROM_QSTR(MP_QSTR_P811), MP_ROM_PTR(&machine_pin_P811_obj) },
    { MP_ROM_QSTR(MP_QSTR_P812), MP_ROM_PTR(&machine_pin_P812_obj) },
    { MP_ROM_QSTR(MP_QSTR_P902), MP_ROM_PTR(&machine_pin_P902_obj) },
    { MP_ROM_QSTR(MP_QSTR_P903), MP_ROM_PTR(&machine_pin_P903_obj) },
    { MP_ROM_QSTR(MP_QSTR_P904), MP_ROM_PTR(&machine_pin_P904_obj) },
    { MP_ROM_QSTR(MP_QSTR_P905), MP_ROM_PTR(&machine_pin_P905_obj) },
    { MP_ROM_QSTR(MP_QSTR_P906), MP_ROM_PTR(&machine_pin_P906_obj) },
    { MP_ROM_QSTR(MP_QSTR_P907), MP_ROM_PTR(&machine_pin_P907_obj) },
    { MP_ROM_QSTR(MP_QSTR_P908), MP_ROM_PTR(&machine_pin_P908_obj) },
    { MP_ROM_QSTR(MP_QSTR_P909), MP_ROM_PTR(&machine_pin_P909_obj) },
    { MP_ROM_QSTR(MP_QSTR_P910), MP_ROM_PTR(&machine_pin_P910_obj) },
    { MP_ROM_QSTR(MP_QSTR_P911), MP_ROM_PTR(&machine_pin_P911_obj) },
    { MP_ROM_QSTR(MP_QSTR_P912), MP_ROM_PTR(&machine_pin_P912_obj) },
    { MP_ROM_QSTR(MP_QSTR_P913), MP_ROM_PTR(&machine_pin_P913_obj) },
    { MP_ROM_QSTR(MP_QSTR_P914), MP_ROM_PTR(&machine_pin_P914_obj) },
    { MP_ROM_QSTR(MP_QSTR_P915), MP_ROM_PTR(&machine_pin_P915_obj) },
    { MP_ROM_QSTR(MP_QSTR_PA07), MP_ROM_PTR(&machine_pin_PA07_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB00), MP_ROM_PTR(&machine_pin_PB00_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB01), MP_ROM_PTR(&machine_pin_PB01_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB02), MP_ROM_PTR(&machine_pin_PB02_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB03), MP_ROM_PTR(&machine_pin_PB03_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB04), MP_ROM_PTR(&machine_pin_PB04_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB05), MP_ROM_PTR(&machine_pin_PB05_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB06), MP_ROM_PTR(&machine_pin_PB06_obj) },
    { MP_ROM_QSTR(MP_QSTR_PB07), MP_ROM_PTR(&machine_pin_PB07_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC09), MP_ROM_PTR(&machine_pin_PC09_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC10), MP_ROM_PTR(&machine_pin_PC10_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC11), MP_ROM_PTR(&machine_pin_PC11_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC12), MP_ROM_PTR(&machine_pin_PC12_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC13), MP_ROM_PTR(&machine_pin_PC13_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC14), MP_ROM_PTR(&machine_pin_PC14_obj) },
    { MP_ROM_QSTR(MP_QSTR_PC15), MP_ROM_PTR(&machine_pin_PC15_obj) },
    { MP_ROM_QSTR(MP_QSTR_PD06), MP_ROM_PTR(&machine_pin_PD06_obj) },
};
MP_DEFINE_CONST_DICT(machine_pin_board_pins_locals_dict, machine_pin_board_pins_locals_dict_table);

const machine_pin_obj_t *const machine_pin_generated_pins[] = {
    &machine_pin_P000_obj,
    &machine_pin_P001_obj,
    &machine_pin_P002_obj,
    &machine_pin_P003_obj,
    &machine_pin_P004_obj,
    &machine_pin_P005_obj,
    &machine_pin_P006_obj,
    &machine_pin_P007_obj,
    &machine_pin_P008_obj,
    &machine_pin_P009_obj,
    &machine_pin_P010_obj,
    &machine_pin_P011_obj,
    &machine_pin_P014_obj,
    &machine_pin_P015_obj,
    &machine_pin_P100_obj,
    &machine_pin_P101_obj,
    &machine_pin_P102_obj,
    &machine_pin_P103_obj,
    &machine_pin_P104_obj,
    &machine_pin_P105_obj,
    &machine_pin_P106_obj,
    &machine_pin_P107_obj,
    &machine_pin_P109_obj,
    &machine_pin_P200_obj,
    &machine_pin_P201_obj,
    &machine_pin_P206_obj,
    &machine_pin_P207_obj,
    &machine_pin_P304_obj,
    &machine_pin_P305_obj,
    &machine_pin_P306_obj,
    &machine_pin_P307_obj,
    &machine_pin_P308_obj,
    &machine_pin_P309_obj,
    &machine_pin_P310_obj,
    &machine_pin_P311_obj,
    &machine_pin_P312_obj,
    &machine_pin_P400_obj,
    &machine_pin_P401_obj,
    &machine_pin_P402_obj,
    &machine_pin_P403_obj,
    &machine_pin_P404_obj,
    &machine_pin_P405_obj,
    &machine_pin_P406_obj,
    &machine_pin_P413_obj,
    &machine_pin_P414_obj,
    &machine_pin_P415_obj,
    &machine_pin_P511_obj,
    &machine_pin_P512_obj,
    &machine_pin_P513_obj,
    &machine_pin_P514_obj,
    &machine_pin_P515_obj,
    &machine_pin_P600_obj,
    &machine_pin_P601_obj,
    &machine_pin_P700_obj,
    &machine_pin_P701_obj,
    &machine_pin_P702_obj,
    &machine_pin_P703_obj,
    &machine_pin_P704_obj,
    &machine_pin_P705_obj,
    &machine_pin_P706_obj,
    &machine_pin_P707_obj,
    &machine_pin_P708_obj,
    &machine_pin_P709_obj,
    &machine_pin_P710_obj,
    &machine_pin_P711_obj,
    &machine_pin_P712_obj,
    &machine_pin_P713_obj,
    &machine_pin_P714_obj,
    &machine_pin_P715_obj,
    &machine_pin_P800_obj,
    &machine_pin_P801_obj,
    &machine_pin_P802_obj,
    &machine_pin_P803_obj,
    &machine_pin_P804_obj,
    &machine_pin_P805_obj,
    &machine_pin_P806_obj,
    &machine_pin_P807_obj,
    &machine_pin_P810_obj,
    &machine_pin_P811_obj,
    &machine_pin_P812_obj,
    &machine_pin_P902_obj,
    &machine_pin_P903_obj,
    &machine_pin_P904_obj,
    &machine_pin_P905_obj,
    &machine_pin_P906_obj,
    &machine_pin_P907_obj,
    &machine_pin_P908_obj,
    &machine_pin_P909_obj,
    &machine_pin_P910_obj,
    &machine_pin_P911_obj,
    &machine_pin_P912_obj,
    &machine_pin_P913_obj,
    &machine_pin_P914_obj,
    &machine_pin_P915_obj,
    &machine_pin_PA07_obj,
    &machine_pin_PB00_obj,
    &machine_pin_PB01_obj,
    &machine_pin_PB02_obj,
    &machine_pin_PB03_obj,
    &machine_pin_PB04_obj,
    &machine_pin_PB05_obj,
    &machine_pin_PB06_obj,
    &machine_pin_PB07_obj,
    &machine_pin_PC09_obj,
    &machine_pin_PC10_obj,
    &machine_pin_PC11_obj,
    &machine_pin_PC12_obj,
    &machine_pin_PC13_obj,
    &machine_pin_PC14_obj,
    &machine_pin_PC15_obj,
    &machine_pin_PD06_obj,
};

const size_t machine_pin_generated_pins_count =
    MP_ARRAY_SIZE(machine_pin_generated_pins);
