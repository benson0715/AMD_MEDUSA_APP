/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include "f_nv_options.h"
#include "amd_crb_drivers_spiFlash.h"

#include "f_bram.h"
#include "gpio_ec.h"
#include "board_config.h"
#include <zephyr/logging/log.h>
#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif

LOG_MODULE_REGISTER(f_nv_options, CONFIG_NVOPTIONS_LOG_LEVEL);

#define SELECT_USE_BID_ROM        0
#ifndef LOG_NVRAM
#define LOG_NVRAM                (1)
#endif

#if (LOG_NVRAM)
#define LOGNVRAM(frmt, ...)      LOG_INF("[NV_RAM]   "frmt, ##__VA_ARGS__)
#else
#define LOGNVRAM(frmt, ...)      while(0){}
#endif

static struct k_work f_nv_options_work;

/*
 * The runtime NV space copy is at BRAM
 */
#if CONFIG_SOC_SERIES_NPCX4
static unsigned int nv_ram[256];
EC_NV_OPTIONS * g_ptrNvSpace = (EC_NV_OPTIONS *)(&nv_ram[4]);
#else
EC_NV_OPTIONS * g_ptrNvSpace = (EC_NV_OPTIONS *)(0x200A0000+0x10);
#endif
static uint8_t  _s_activeBlock   = 0; 
static uint32_t _s_blkLocation[] = {0x0E00, 0x0F00}; // to adapt at minimum 4K-Byte ROM
extern bool g_ecResetAfterSetOptions;


#define NV_RAM_BLK_NUM               (sizeof(_s_blkLocation) / sizeof(uint32_t))
#define FIELD_OFFSET(field)         ((uint32_t)&(((EC_NV_OPTIONS *)0)->f.field))

uint8_t g_bytes256[256];  // for CRC8 table


/**
 * f_nv_options_NvStorageAccess
 * 
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]   offset;     registers' offset 
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/register width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 * This routine provides an uniform interface for other componment to access
 * none-volatile storage on the board. It could be SPI or I2C flash rom
 *
 * The NV storage is on 24LC128 (EEPROM Memory IC 128Kb (16K x 8) I2C 400kHz)
 *   - The first 256+32 bytes are allocated for Board ID
 *   - EC NV Options are at 0x3E00, 0x3F00
 */
int f_nv_options_NvStorageAccess(bool isRead, uint16_t offset, void * pBuf, uint32_t len)
{
	int ret;
#if SELECT_USE_BID_ROM
	if (isRead) {
		ret = eeprom_read_block(offset,len,pBuf);
	} else {
		ret = eeprom_write_block(offset, len, pBuf);
	}
#else
	if (isRead) {
		ret = amd_crb_drivers_sFlash_int_read(offset,len,pBuf);
	} else {
		ret = amd_crb_flash_byte_write(offset, len, pBuf);
	}
#endif
	return ret;
}

/**
 * _nvram_loadDefault 
 *
 * @param [in]   buf;     nvram load buf
 *
 * @note
 *  load the default setting to nvram
 */
static void _nvram_loadDefault (EC_NV_OPTIONS * buf)
{
	memset(buf, 0x00, sizeof(EC_NV_OPTIONS));
	/*
	 * for fixed fields
	 */
	buf->f.magic = NV_OPTIONS_MAGIC;
	buf->f.length = FIELD_OFFSET(magic);

	buf->f.crc8_alg0 = 0xFF;
	buf->f.crc8_alg1 = 0xFF;
	buf->f.reserved  = 0xFF;
	buf->f.hasUpdate = 1;
	/*
	 * platform specific fields
	 */
	SET_NV_OPTIONS_TO_DEFAULT;
}

/**
 * _nvram_checkCrc 
 *
 * @param [in]   buf;     nvram check buf
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 * @note
 *  check the crc for nvram
 */
static bool _nvram_checkCrc (EC_NV_OPTIONS * buf)
{
	LOG_DBG("_nvram_checkCrc");
	if (buf->f.magic != NV_OPTIONS_MAGIC) {
		LOG_DBG("buf->f.magic != NV_OPTIONS_MAGIC");
		return false;
	}
	if (buf->f.length != FIELD_OFFSET(magic)) {
		return false;
	}

	U_CRC8_CONTEXT crc8Handle;
	uint8_t result;
	crc8Handle.pCrcTable = &g_bytes256[0];

	/*
	 * ALG0 - U_CRC8_ALG_WCDMA
	 */
	crc8Handle.alg = U_CRC8_ALG_WCDMA;

	u_crc8_makeTable(crc8Handle.alg, crc8Handle.pCrcTable, 256);
	u_crc8_init(&crc8Handle);

	for (uint32_t i = 0; i < buf->f.length; i++) {
		u_crc8_push(&crc8Handle, ((uint8_t *)g_ptrNvSpace)[i]);
	}
	result = u_crc8_finsh(&crc8Handle);

	if (result != buf->f.crc8_alg0) {
		LOG_DBG("result != buf->f.crc8_alg0");
		return false;
	}

	/*
	 * ALG1 - U_CRC8_ALG_EBU
	 */
	crc8Handle.alg = U_CRC8_ALG_EBU;

	u_crc8_makeTable(crc8Handle.alg, crc8Handle.pCrcTable, 256);
	u_crc8_init(&crc8Handle);

	for (uint32_t i = 0; i < buf->f.length; i++) {
		u_crc8_push(&crc8Handle, ((uint8_t *)g_ptrNvSpace)[i]);
	}
	result = u_crc8_finsh(&crc8Handle);

	if (result != buf->f.crc8_alg1) {
		return false;
	}

	return true;
}

/**
 * _nvram_updateCrc
 *
 * @param [in]   buf;     nvram check buf
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 * @note
 *  update the crc for nvram
 */
static bool _nvram_updateCrc (EC_NV_OPTIONS * buf)
{
	assert(buf->f.magic == NV_OPTIONS_MAGIC);
	assert(buf->f.length == FIELD_OFFSET(magic));

	U_CRC8_CONTEXT crc8Handle;
	crc8Handle.pCrcTable = &g_bytes256[0];

	/*
	 * ALG0 - U_CRC8_ALG_WCDMA
	 */
	crc8Handle.alg = U_CRC8_ALG_WCDMA;

	u_crc8_makeTable(crc8Handle.alg, crc8Handle.pCrcTable, 256);
	u_crc8_init(&crc8Handle);

	for (uint32_t i = 0; i < buf->f.length; i++) {
		u_crc8_push(&crc8Handle, ((uint8_t *)g_ptrNvSpace)[i]);
	}
	buf->f.crc8_alg0 = u_crc8_finsh(&crc8Handle);

	/*
	 * ALG1 - U_CRC8_ALG_EBU
	 */
	crc8Handle.alg = U_CRC8_ALG_EBU;

	u_crc8_makeTable(crc8Handle.alg, crc8Handle.pCrcTable, 256);
	u_crc8_init(&crc8Handle);

	for (uint32_t i = 0; i < buf->f.length; i++) {
		u_crc8_push(&crc8Handle, ((uint8_t *)g_ptrNvSpace)[i]);
	}
	buf->f.crc8_alg1 = u_crc8_finsh(&crc8Handle);

	return true;
}

/**
 * _f_nv_options_eventCallback 
 *
 * @param [in]   w;     work item pointer
 *
 * @note
 *  nv option- event callback function
 */
static void _f_nv_options_eventCallback (struct k_work *w)
{
	f_nv_options_save();
}

/**
 * f_sync_nvoption 
 *
 * @param [in]   timer;     timer pointer
 *
 * @note
 *  Timer callback to trigger nv option save 
 */
static void f_sync_nvoption (struct k_timer *timer) 
{
	k_work_submit(&f_nv_options_work);
}

K_TIMER_DEFINE(nvoption_timer, f_sync_nvoption, NULL);

/**
 * f_nv_options_init
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  The necessary setups before all tasks start.
 */
bool f_nv_options_init(void)
{
	LOG_DBG("f_nv_options_init");

	k_work_init(&f_nv_options_work, _f_nv_options_eventCallback);
	k_timer_start(&nvoption_timer, K_MSEC(2000), K_MSEC(2000));
	return f_nv_options_load();
}

/**
 * f_nv_options_load
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  The necessary setups before all tasks start.
 */
bool f_nv_options_load(void)
{
	uint8_t blk = _s_activeBlock;
	bool isSuccess = false;

	LOG_DBG("Load options  from BRAM");
	/*
	 * The last field 'hasUpdate' should not exceed the pre-allocated space
	 */
	assert(FIELD_OFFSET(hasUpdate) <= (NV_OPTIONS_SIZE - 1));

	/*
	 * Try to load from bram
	 */
	if (F_BRAM_VCI_BOOT_SIGNATURE == f_bram_bootReason()) {
		// bram can hold valid options only if it resumes from VCI
		if (_nvram_checkCrc(g_ptrNvSpace)) {
			g_ptrNvSpace->f.hasUpdate = 0;	// ensure hasUpdate = 0
			isSuccess = true;
			LOGNVRAM("Load options success from BRAM");
		}
	}

	/*
	 * Try to load from 24LC128
	 */
	if (!isSuccess) {
		for (uint8_t i = 0; i < NV_RAM_BLK_NUM; i++, blk = (blk + 1) % NV_RAM_BLK_NUM) {
			/*
			 * 1. load this block from EEPROM to g_ptrNvSpace
			 */
			int ret = f_nv_options_NvStorageAccess(1, _s_blkLocation[blk], g_ptrNvSpace, NV_OPTIONS_SIZE);
			if (ret != 0) {
				#if SELECT_USE_BID_ROM
				LOG_ERR("Read from eeprom failed");
				#else
				LOG_ERR("Read from flash failed");
				#endif
				}
			/*
			 * 2. check data integrity
			 */
			if (_nvram_checkCrc(g_ptrNvSpace)) {
				_s_activeBlock = blk;
				g_ptrNvSpace->f.hasUpdate = 0;	// ensure hasUpdate = 0
				isSuccess = true;
				#if SELECT_USE_BID_ROM
				LOGNVRAM("Load options success from EEPROM 0x%04X", _s_blkLocation[blk]);
				#else
				LOGNVRAM("Load options success from Flash 0x%04X", _s_blkLocation[blk]);
				#endif
				break;
			}
		}
	}

	/*
	 * If all of above trials are failed, load the default
	 */
	if (!isSuccess) {
		_s_activeBlock = 0;

		LOGNVRAM("Load default NV options");

		_nvram_loadDefault(g_ptrNvSpace);
		// _nvram_updateCrc(g_ptrNvSpace); // No need to set CRC here,
		                                   // It will be calculated later before save to EEPROM.
		g_ptrNvSpace->f.hasUpdate = 1;     /* Ensure hasUpdate = 1 */
		isSuccess = true;
	}

	return 0;
}

/**
 * f_nv_options_save
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Call it when nvram data need to save after restart.
 */
bool f_nv_options_save(void)
{
	assert (g_ptrNvSpace->f.magic == NV_OPTIONS_MAGIC);
	assert (g_ptrNvSpace->f.length == FIELD_OFFSET(magic));
	if (!g_ptrNvSpace->f.hasUpdate) {
		return true;
	}
	LOG_DBG("f_nv_options_save");

	uint32_t emptyWord = 0xFFFFFFFF;

	/*
	 * Srite to _s_activeBlock, eraze other blocks
	 * Start from next one of current active block
	 */
	uint8_t blk = (_s_activeBlock + 1) % NV_RAM_BLK_NUM;
	_s_activeBlock = blk;

	//md_mutex_w_lock(&g_nvRwLock);

	/* save block to EEPROM */
	_nvram_updateCrc(g_ptrNvSpace);
	int ret = f_nv_options_NvStorageAccess(0, _s_blkLocation[blk], g_ptrNvSpace, NV_OPTIONS_SIZE);
	#if SELECT_USE_BID_ROM
	LOGNVRAM("Save NV options to EEPROM 0x%04X, ret = %d", _s_blkLocation[blk], ret);
	#else
	LOGNVRAM("Save NV options to Flash 0x%04X, ret = %d", _s_blkLocation[blk], ret);
	#endif
	/* clear hasUpdate flag */
	g_ptrNvSpace->f.hasUpdate = 0;

	//md_mutex_w_unlock(&g_nvRwLock);

	blk = (blk + 1) % NV_RAM_BLK_NUM;
	for (uint8_t i = 0; i < NV_RAM_BLK_NUM - 1; i++, blk = (blk + 1) % NV_RAM_BLK_NUM) {
		/*
		 * eraze block by clear its magic word
		 */
		ret = f_nv_options_NvStorageAccess(0, _s_blkLocation[blk] + FIELD_OFFSET(magic), &emptyWord, 4);
		#if SELECT_USE_BID_ROM
		LOGNVRAM("Clear NV block EEPROM 0x%04X, ret = %d", _s_blkLocation[blk], ret);
		#else
		LOGNVRAM("Clear NV block Flash 0x%04X, ret = %d", _s_blkLocation[blk], ret);
		#endif
	}

	LOG_DBG("f_nv_options_save done");
	if (g_ecResetAfterSetOptions)
	{
		g_ecResetAfterSetOptions = false;
		
		/* G3 system reset Bios ROM */
#if CONFIG_FAN_RPM_CONTROL
		app_fan_ctrl_sys_disable();
#endif
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
		gpio_write_pin(EC_SLP_S5_N, 0);
		k_busy_wait(20000);
		gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#else
		gpio_write_pin(EC_SLP_S5_N, 0);
		gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#endif

#ifdef EC_S0_LED
		gpio_write_pin(EC_S0_LED, 0);
#endif
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
		ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 0);
#endif

		gpio_write_pin(RSMRST_N, 0);
		k_busy_wait(10000);
#ifdef EC_USB32_RD_EN
		gpio_write_pin(EC_USB32_RD_EN, 0);
#endif
#ifdef EC_USB32_RD_RST_N
		gpio_write_pin(EC_USB32_RD_RST_N, 0);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
		ioexp_set(ex_EP_SENSOR_3V3_PWREN, 0);
#endif

		gpio_write_pin(EC_1V8_S5_EN, 0);

		k_busy_wait(20000);

		gpio_write_pin(EC_S5_PWREN, 0);

		f_bram_setSig(F_BRAM_NV_OPTION_UPDATE_SIGNATURE);
		k_busy_wait(10000);
		sys_reboot(SYS_REBOOT_COLD);
	}

	return true;
}