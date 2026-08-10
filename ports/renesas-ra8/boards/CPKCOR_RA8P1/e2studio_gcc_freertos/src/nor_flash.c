#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "hal_data.h"
#include "nor_flash.h"

#define OSPI_NAME	g_nor_flash

#define JEDEC_MANUFACTURER_WINBOND	0xEF
#define W35N01JW_DEVICE_ID_L		0x21
#define W35N01JW_DEVICE_ID_H		0xDC
#define W35T51NW_MEMORY_TYPE		0x5B
#define W35T51NW_MEMORY_CAPACITY	0x1A
#define W35T51NW_EXTENSION			0x02

#ifndef __NOR_FLASH_DEBUG
#define __NOR_FLASH_DEBUG 1
#endif

#define TAG __FUNCTION__

#if __NOR_FLASH_DEBUG
#include "utils/log.h"
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#define NF_LOGD(msg, ...)				LOG_D(TAG, msg, ##__VA_ARGS__)
#define NF_LOGI(msg, ...)				LOG_I(TAG, msg, ##__VA_ARGS__)
#define NF_LOGW(msg, ...)				LOG_W(TAG, msg, ##__VA_ARGS__)
#define NF_LOGE(msg, ...)				LOG_E(TAG, msg, ##__VA_ARGS__)
#else
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { return v; }
#define NF_LOGD(msg, ...)
#define NF_LOGI(msg, ...)
#define NF_LOGW(msg, ...)
#define NF_LOGE(msg, ...)
#endif

struct NorFlash {
	uint8_t read_dummy_cycles_opi;
	uint32_t capacity;
	NorFlashChip chip;
	spi_flash_cfg_t cfg;
	ospi_b_extended_cfg_t cfg_ext;
};

const uint32_t gc_autocalibration[] = {
	0xFFFF0000,
	0x000800FF,
	0x00FFF700,
	0xF700F708
};

static struct NorFlash s_flash;

static const spi_flash_erase_command_t sc_erase_cmd_spi_w35n01jw[] = {
	{ .command = 0xD8, .size = 1024 * 256},
};

uint32_t NorFlash_EraseChip(void)
{
	uint32_t err;
	spi_flash_direct_transfer_t cmd;

#if BSP_CFG_DCACHE_ENABLED
	bool dcache_reenable = false;
	bool dcache_enable = SCB->CCR & SCB_CCR_DC_Msk ? true : false;
	if (dcache_enable) {
		__DSB();
		__ISB();
		SCB_DisableDCache();
		dcache_reenable = true;
	}
#endif

	memset(&cmd, 0, sizeof(spi_flash_direct_transfer_t));

	err = NorFlash_SetWriteEnable();
	if (err) {
		NF_LOGE("SetWriteEnable failed: %" PRIu32, err);
		goto EXIT;
	}

	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd.command = 0x60;
		cmd.command_length = 0x01;
	}
	else {
		cmd.command = 0x6060;
		cmd.command_length = 0x02;
	}
	err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	if (err) {
		NF_LOGE("DirectTransfer failed: %" PRIu32, err);
	}

EXIT:
#if BSP_CFG_DCACHE_ENABLED
	if (dcache_reenable) {
		__DSB();
		__ISB();
		SCB_EnableDCache();
	}
#endif

	return 0;
}

uint32_t NorFlash_EraseSector(uint32_t sector)
{
	uint32_t err;
	spi_flash_direct_transfer_t cmd;

#if BSP_CFG_DCACHE_ENABLED
	bool dcache_reenable = false;
	bool dcache_enable = SCB->CCR & SCB_CCR_DC_Msk ? true : false;
	if (dcache_enable) {
		__DSB();
		__ISB();
		SCB_DisableDCache();
		dcache_reenable = true;
	}
#endif

	err = NorFlash_SetWriteEnable();
	UNLIKE_RETURN(err, 0, "SetWriteEnable failed: %" PRIu32, err);

	memset(&cmd, 0, sizeof(spi_flash_direct_transfer_t));
	cmd.address = sector * NORFLASH_SECTOR_SIZE;
	cmd.address_length = 0x04;
	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd.command = 0x21;
		cmd.command_length = 0x01;
	}
	else {
		cmd.command = 0x2121;
		cmd.command_length = 0x02;
	}
	err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	UNLIKE_RETURN(err, 0, "DirectTransfer failed: %" PRIu32, err);
	err = NorFlash_WaitOperation(500);
	UNLIKE_RETURN(err, 0, "SetWriteEnable failed: %" PRIu32, err);

#if BSP_CFG_DCACHE_ENABLED
	if (dcache_reenable) {
		__DSB();
		__ISB();
		SCB_EnableDCache();
	}
#endif

	return 0;
}

uint32_t NorFlash_Program(uint32_t address, void const *data, uint32_t length)
{
	uint32_t i;
	uint32_t err;

	uint8_t *p_flash = (uint8_t *)address;
	uint8_t const *p_data = (uint8_t *)data;
	uint32_t repeat = length / 64;
	uint32_t remain = length % 64;

#if BSP_CFG_DCACHE_ENABLED
	bool dcache_reenable = false;
	bool dcache_enable = SCB->CCR & SCB_CCR_DC_Msk ? true : false;
	if (dcache_enable) {
		__DSB();
		__ISB();
		SCB_DisableDCache();
		dcache_reenable = true;
	}
#endif

	for (i = 0; i < repeat; i++) {
		err = R_OSPI_B_Write(&g_nor_flash_ctrl, p_data, p_flash, 64);
		UNLIKE_RETURN(err, 0, "Write failed: %" PRIu32 ". Src: 0x%p, Target: 0x%p. i = %" PRIu32, err, p_data, p_flash, i);
		err = NorFlash_WaitOperation(5000);
		UNLIKE_RETURN(err, 0, "Wait failed: %" PRIu32 ". Src: 0x%p, Target: 0x%p. i = %" PRIu32, err, p_data, p_flash, i);
		p_data = &p_data[64];
		p_flash = &p_flash[64];
	}
	if (remain) {
		err = R_OSPI_B_Write(&g_nor_flash_ctrl, p_data, p_flash, remain);
		UNLIKE_RETURN(err, 0, "Write failed: %" PRIu32 ". Src: 0x%p, Target: 0x%p", err, p_data, p_flash);
		err = NorFlash_WaitOperation(5000);
		UNLIKE_RETURN(err, 0, "Wait failed: %" PRIu32 ". Src: 0x%p, Target: 0x%p", err, p_data, p_flash);
	}

#if BSP_CFG_DCACHE_ENABLED
	if (dcache_reenable) {
		__DSB();
		__ISB();
		SCB_EnableDCache();
	}
#endif

	return 0;
}

uint32_t NorFlash_Init(void)
{
	uint8_t i8;
	uint32_t expect;
	uint32_t cali[4];
	bsp_octaclk_settings_t octaclk;
	spi_flash_direct_transfer_t cmd;

	uint32_t err = 0;
	const ospi_b_extended_cfg_t * const p_cfg_extend = g_nor_flash_cfg.p_extend;
	const ospi_b_xspi_command_set_t *p_cmd_table = p_cfg_extend->p_xspi_command_set->p_table;
	uint8_t *p8 = p_cfg_extend->p_autocalibration_preamble_pattern_addr;

#if BSP_CFG_DCACHE_ENABLED
	bool dcache_reenable = false;
	bool dcache_enable = SCB->CCR & SCB_CCR_DC_Msk ? true : false;
	if (dcache_enable) {
		__DSB();
		__ISB();
		SCB_DisableDCache();
		dcache_reenable = true;
	}
#endif

	memset(&s_flash, 0, sizeof(s_flash));
	memcpy(&s_flash.cfg, &g_nor_flash_cfg, sizeof(spi_flash_cfg_t));
	memcpy(&s_flash.cfg_ext, g_nor_flash_cfg.p_extend, sizeof(ospi_b_extended_cfg_t));
	s_flash.chip = CHIP_UNKNOW;

	R_OSPI_B_Open(g_nor_flash.p_ctrl, g_nor_flash.p_cfg);
	R_OSPI_B_SpiProtocolSet(g_nor_flash.p_ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);

#if 1
	R_XSPI1->LIOCTL_b.RSTCS0 = 0;
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	R_XSPI1->LIOCTL_b.RSTCS0 = 1;
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MICROSECONDS);
#else
	uint32_t cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_DRIVE_HIGH | IOPORT_CFG_PORT_OUTPUT_HIGH;
	R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_12_PIN_07, cfg);
	R_BSP_PinWrite(BSP_IO_PORT_12_PIN_07, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(BSP_IO_PORT_12_PIN_07, BSP_IO_LEVEL_HIGH);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
#endif

	for (i8 = 0; i8 < p_cfg_extend->p_xspi_command_set->length; i8++) {
		if (p_cmd_table[i8].protocol == SPI_FLASH_PROTOCOL_8D_8D_8D) {
			break;
		}
	}
	if (i8 == p_cfg_extend->p_xspi_command_set->length) {
		NF_LOGW("Can't find a command table's protoal is 8D-8D-8D");
		s_flash.read_dummy_cycles_opi = 0x10;
	}
	else {
		s_flash.read_dummy_cycles_opi = p_cmd_table[i8].read_dummy_cycles;
		NF_LOGD("Set 8D-8D-8D read dummy cycles: %" PRIu8, s_flash.read_dummy_cycles_opi);
	}

	cmd.address = 0x00;
	cmd.address_length = 0x00;
	cmd.command = 0x9F;
	cmd.command_length = 0x01;
	cmd.data = 0x00;
	cmd.data_length = 0x04;
	cmd.dummy_cycles = 0x00;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	expect = W35N01JW_DEVICE_ID_L;
	expect = (expect << 0x08) | W35N01JW_DEVICE_ID_H;
	expect = (expect << 0x08) | JEDEC_MANUFACTURER_WINBOND;
	expect = (expect << 0x08) | 0xFF;
	if (cmd.data == expect) {
		NF_LOGD("Chip: W35N01JW");
		s_flash.chip = CHIP_W35N01JW;
		s_flash.capacity = 1024 * 1024 * 128;
		s_flash.cfg.erase_command_list_length = 0x01;
		s_flash.cfg.p_erase_command_list = sc_erase_cmd_spi_w35n01jw;
		err = FSP_ERR_UNSUPPORTED;
		goto EXIT;
	}
	expect = W35T51NW_EXTENSION;
	expect = (expect << 0x08) | W35T51NW_MEMORY_CAPACITY;
	expect = (expect << 0x08) | W35T51NW_MEMORY_TYPE;
	expect = (expect << 0x08) | JEDEC_MANUFACTURER_WINBOND;
	if (cmd.data == expect) {
		NF_LOGD("Chip: W35T51NW");
		s_flash.chip = CHIP_W35T51NW;
		s_flash.capacity = 1024 * 1024 * 64;
		goto CHECK_ADDR_MODE;
	}

	NF_LOGE("Unsupport NorFlash: 0x%" PRIX32, cmd.data);
	err = FSP_ERR_UNSUPPORTED;
	goto EXIT;

CHECK_ADDR_MODE:
	/* 检查地址模式 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x70;
	cmd.command_length = 0x01;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	/* 如果在 3 字节地址模式就使用指令进入 4 字节地址模式 */
	if ((cmd.data & 0x01) == 0x00) {
		cmd.command = 0xB7;
		cmd.data_length = 0x00;
		R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
		R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MICROSECONDS);

		cmd.command = 0x70;
		cmd.data_length = 0x01;
		R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		if ((cmd.data & 0x01) == 0x00) {
			NF_LOGW("Enter 4-byte address mode failed");
		}
		else {
			NF_LOGD("Enter 4-byte address mode");
		}
	}

	NF_LOGD("Cali address: %p", p8);
	memcpy(cali, p8, 16);
	NF_LOGD("cali[0]: 0x%08" PRIX32, cali[0]);
	NF_LOGD("cali[1]: 0x%08" PRIX32, cali[1]);
	NF_LOGD("cali[2]: 0x%08" PRIX32, cali[2]);
	NF_LOGD("cali[3]: 0x%08" PRIX32, cali[3]);
	memset(cali, 0, 16);

	if (memcmp(p8, gc_autocalibration, sizeof(gc_autocalibration))) {
		NF_LOGD("Write autocalibration");
		R_OSPI_B_Erase(g_nor_flash.p_ctrl, p8, 4096);
		NorFlash_WaitOperation(200);
		R_OSPI_B_Write(g_nor_flash.p_ctrl, (uint8_t *)gc_autocalibration, p8, sizeof(gc_autocalibration));
		NorFlash_WaitOperation(200);
	}

	cmd.address = 0x00;
	cmd.address_length = 0x04;
	cmd.command = 0x85;
	cmd.data = 0x00;
	cmd.data_length = 0x01;
	cmd.dummy_cycles = 0x08;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-IOC : 0x%" PRIX32, cmd.data);
	cmd.address = 0x01;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-DC  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x02;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-VLB : 0x%" PRIX32, cmd.data);
	cmd.address = 0x03;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-DS  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x04;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-CRC : 0x%" PRIX32, cmd.data);
	cmd.address = 0x05;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-AM  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x06;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-XIP : 0x%" PRIX32, cmd.data);
	cmd.address = 0x07;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-Wrap: 0x%" PRIX32, cmd.data);

	/* 设置 Dummy cycle */
	NorFlash_SetWriteEnable();
	memset(&cmd, 0, sizeof(cmd));
	cmd.address = 0x01;
	cmd.address_length = 0x04;
	cmd.command = 0x81;
	cmd.command_length = 0x01;
	cmd.data = s_flash.read_dummy_cycles_opi;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

	/* 设置 VCR-IOC = 0xE7 进入 Octal DDR 模式 */
	NorFlash_SetWriteEnable();
	memset(&cmd, 0, sizeof(cmd));
	cmd.address_length = 0x04;
	cmd.command = 0x81;
	cmd.command_length = 0x01;
	cmd.data = 0xE7;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	octaclk.divider = BSP_CLOCKS_OCTA_CLOCK_DIV_1;
	octaclk.source_clock = BSP_CLOCKS_SOURCE_CLOCK_PLL2Q;
	R_BSP_OctaclkUpdate(&octaclk);
	err = R_OSPI_B_SpiProtocolSet(g_nor_flash.p_ctrl, SPI_FLASH_PROTOCOL_8D_8D_8D);
	if (err) {
		NF_LOGE("R_OSPI_B_SpiProtocolSet failed: %" PRIu32, err);
	}
	else {
		NF_LOGD("Enter ODDR mode");
	}
	memset(cali, 0, 16);

	cmd.address = 0x00;
	cmd.address_length = 0x04;
	cmd.command = 0x8585;
	cmd.command_length = 0x02;
	cmd.data = 0x00;
	cmd.data_length = 0x02;
	cmd.dummy_cycles = 0x1F;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-IOC : 0x%" PRIX32, cmd.data);
	cmd.address = 0x01;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-DC  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x02;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-VLB : 0x%" PRIX32, cmd.data);
	cmd.address = 0x03;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-DS  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x04;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-CRC : 0x%" PRIX32, cmd.data);
	cmd.address = 0x05;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-AM  : 0x%" PRIX32, cmd.data);
	cmd.address = 0x06;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-XIP : 0x%" PRIX32, cmd.data);
	cmd.address = 0x07;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	NF_LOGD("VCR-Wrap: 0x%" PRIX32, cmd.data);

#if 0
	memset(&cmd, 0, sizeof(cmd));
	cmd.address = (uint32_t)p8 - NORFLASH_MAP_START_ADDR;
	cmd.address_length = 0x04;
	cmd.command = 0x7C7C;
	cmd.command_length = 0x02;
	cmd.data_length = 0x04;
	cmd.dummy_cycles = s_read_dummy_cycles;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	cali[0] = cmd.data;
	cmd.address += 0x04;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	cali[1] = cmd.data;
	cmd.address += 0x04;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	cali[2] = cmd.data;
	cmd.address += 0x04;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	cali[3] = cmd.data;
#else
	memcpy(cali, p8, 16);
#endif
	NF_LOGD("cali[0]: 0x%08" PRIX32, cali[0]);
	NF_LOGD("cali[1]: 0x%08" PRIX32, cali[1]);
	NF_LOGD("cali[2]: 0x%08" PRIX32, cali[2]);
	NF_LOGD("cali[3]: 0x%08" PRIX32, cali[3]);

#if 0
	cmd.address = 0x2000;
	cmd.address_length = 0x04;
	cmd.command = 0x1212;
	cmd.data_u64 = 0x122355AA55BBCCDD;
	cmd.data_length = 0x08;
	cmd.dummy_cycles = 0x00;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

	cmd.command = 0x0C0C;
	cmd.data_u64 = 0x00;
	cmd.dummy_cycles = 0x08;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	if (cmd.data_u64 != 0x122355AA55BBCCDD) {
		NF_LOGW("Read not equal to write");
	}
#endif

EXIT:
#if BSP_CFG_DCACHE_ENABLED
	if (dcache_reenable) {
		__DSB();
		__ISB();
		SCB_EnableDCache();
	}
#endif

	return err;
}

uint32_t NorFlash_Read(uint32_t address, void *data, uint32_t length)
{
	uint32_t i, err;
	spi_flash_direct_transfer_t cmd;

	uint32_t remain = length % 8;
	uint64_t *p64 = (uint64_t *)data;

	memset(&cmd, 0, sizeof(spi_flash_direct_transfer_t));
	cmd.address = address;
	cmd.address_length = 0x04;
	cmd.data_length = 0x08;
	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd.command = 0x0C;
		cmd.command_length = 0x01;
		cmd.dummy_cycles = 0x08;
	}
	else {
		cmd.command = 0x7C7C;
		cmd.command_length = 0x02;
		cmd.dummy_cycles = s_flash.read_dummy_cycles_opi;
	}

	for (i = 0; i < (length / 8); i++) {
		err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		UNLIKE_RETURN(err, 0, "DirectTransfer failed: %" PRIu32 ", No.%" PRIu32, err, i);
		p64[i] = cmd.data_u64;
		cmd.address += 8;
	}

	if (remain) {
		cmd.data_length = (uint8_t)remain;
		err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		UNLIKE_RETURN(err, 0, "DirectTransfer failed: %" PRIu32 " while process remain", err);
		memcpy(&p64[i], &cmd.data, remain);
	}

	return 0;
}

uint32_t NorFlash_SetWriteEnable(void)
{
	spi_flash_status_t status;
	spi_flash_direct_transfer_t cmd_we;
	spi_flash_direct_transfer_t cmd_rs;

	uint16_t r_cnt = 0;
	uint16_t w_cnt = 0;
	uint32_t err = 0;

#if BSP_CFG_DCACHE_ENABLED
	bool dcache_reenable = false;
	bool dcache_enable = SCB->CCR & SCB_CCR_DC_Msk ? true : false;
	if (dcache_enable) {
		__DSB();
		__ISB();
		SCB_DisableDCache();
		dcache_reenable = true;
	}
#endif

	memset(&cmd_we, 0, sizeof(spi_flash_direct_transfer_t));
	memset(&cmd_rs, 0, sizeof(spi_flash_direct_transfer_t));
	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd_we.command = 0x06;
		cmd_we.command_length = 0x01;
		cmd_rs.command = 0x05;
		cmd_rs.command_length = 0x01;
		cmd_rs.data_length = 0x01;
	}
	else {
		cmd_we.command = 0x0606;
		cmd_we.command_length = 0x02;
		cmd_rs.command = 0x0505;
		cmd_rs.command_length = 0x02;
		cmd_rs.address = 0x00;
		cmd_rs.address_length = 0x04;
		cmd_rs.data_length = 0x01;
		cmd_rs.dummy_cycles = 0x08;
	}
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd_we, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	R_OSPI_B_StatusGet(g_nor_flash.p_ctrl, &status);
	r_cnt = 100;
	while (status.write_in_progress && r_cnt) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		R_OSPI_B_StatusGet(g_nor_flash.p_ctrl, &status);
		r_cnt--;
	}
	if (r_cnt == 0) {
		NF_LOGE("Wait OSPI timeout");
		return FSP_ERR_TIMEOUT;
	}
	r_cnt = 0;

	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd_rs, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	while ((cmd_rs.data & 0x02) != 0x02) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd_rs, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		r_cnt++;

		if ((r_cnt == 50) && ((cmd_rs.data & 0x02) != 0x02)) {
			r_cnt = 0;
			R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd_we, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
			w_cnt++;
		}

		if (w_cnt == 1200) {
			err = FSP_ERR_TIMEOUT;
			break;
		}
	}

#if BSP_CFG_DCACHE_ENABLED
	if (dcache_reenable) {
		__DSB();
		__ISB();
		SCB_EnableDCache();
	}
#endif

	return err;
}

uint32_t NorFlash_WaitOperation(uint32_t timeout)
{
	spi_flash_direct_transfer_t cmd;

	(void)timeout;

#if 0
	spi_flash_status_t status;
	R_OSPI_B_StatusGet(g_nor_flash.p_ctrl, &status);
	while (status.write_in_progress) {
		if (timeout) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			R_OSPI_B_StatusGet(g_nor_flash.p_ctrl, &status);
			timeout--;
		}
		else {
			return FSP_ERR_TIMEOUT;
		}
	}
#endif

	memset(&cmd, 0, sizeof(spi_flash_direct_transfer_t));
	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd.command = 0x05;
		cmd.command_length = 0x01;
		cmd.data_length = 0x01;
	}
	else {
		cmd.command = 0x0505;
		cmd.command_length = 0x02;
		cmd.data_length = 0x02;
		cmd.dummy_cycles = 0x08;
	}
#if 1
	uint32_t us = timeout * 1000;
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	while (cmd.data & 0x01) {
		if (us == 0) {
			return FSP_ERR_TIMEOUT;
		}
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
		us--;
		R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	}
#else
	R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	while (cmd.data & 0x01) {
		__NOP();
		R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	}
#endif

	return 0;
}

uint32_t NorFlash_WriteSector(uint32_t sector, void *data, uint32_t length)
{
	uint32_t i;
	uint32_t err;
	spi_flash_direct_transfer_t cmd;

	uint32_t remain = length % 8;
	uint64_t *p64 = (uint64_t *)data;

	err = NorFlash_EraseSector(sector);
	UNLIKE_RETURN(err, 0, "EraseSector failed");
	memset(&cmd, 0, sizeof(spi_flash_direct_transfer_t));
	cmd.address = sector * NORFLASH_SECTOR_SIZE;
	cmd.address_length = 0x04;
	cmd.data_length = 0x08;
	if (g_nor_flash_ctrl.spi_protocol == SPI_FLASH_PROTOCOL_EXTENDED_SPI) {
		cmd.command = 0x12;
		cmd.command_length = 0x01;
	}
	else {
		cmd.command = 0x1212;
		cmd.command_length = 0x02;
	}
	for (i = 0; i < (length / 8); i++) {
		cmd.data_u64 = p64[i];
		err = NorFlash_SetWriteEnable();
		UNLIKE_RETURN(err, 0, "SetWriteEnable failed");
		err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
		UNLIKE_RETURN(err, 0, "DirectTransfer failed");
		err = NorFlash_WaitOperation(20);
		UNLIKE_RETURN(err, 0, "WaitOperation failed");
		cmd.address += 8;
	}

	if (remain) {
		cmd.data_length = (uint8_t)remain;
		memcpy(&cmd.data, &p64[i], remain);
		err = NorFlash_SetWriteEnable();
		UNLIKE_RETURN(err, 0, "SetWriteEnable failed while process remain");
		err = R_OSPI_B_DirectTransfer(g_nor_flash.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
		UNLIKE_RETURN(err, 0, "DirectTransfer failed while process remain");
		err = NorFlash_WaitOperation(20);
		UNLIKE_RETURN(err, 0, "WaitOperation failed while process remain");
	}

	return 0;
}

void NorFlash_DumpOSPIReg(void)
{
	ospi_b_instance_ctrl_t *p_ctrl = (ospi_b_instance_ctrl_t *)OSPI_NAME.p_ctrl;

	printf("============== %s ==============\r\n", __FUNCTION__);
	printf("ABMCFG:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->ABMCFG);
	printf("BMCFGCH[0]: 0x%08" PRIX32 "\r\n", p_ctrl->p_reg->BMCFGCH[0]);
	printf("BMCFGCH[1]: 0x%08" PRIX32 "\r\n", p_ctrl->p_reg->BMCFGCH[1]);
	printf("BMCTL0:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->BMCTL0);
	printf("BMCTL1:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->BMCTL1);
	printf("CASTTCS[0]: 0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CASTTCS[0]);
	printf("CASTTCS[1]: 0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CASTTCS[1]);
	printf("CCCTL0:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CCCTLCS[0].CCCTL0);
	printf("CCCTL1:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CCCTLCS[0].CCCTL1);
	printf("CCCTL2:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CCCTLCS[0].CCCTL2);
	printf("CCCTL3:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CCCTLCS[0].CCCTL3);
	printf("CCCTL4:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CCCTLCS[0].CCCTL4);
	printf("CDCTL0:     0x%08" PRIX32 "\r\n", p_ctrl->p_reg->CDCTL0);
}
