/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/sys/util.h>
#include "amd_crb_drivers_spiFlash.h"
#include <zephyr/sys/crc.h>
#include <zephyr/kernel.h>
#include "oob_rpmc_rom.h"

LOG_MODULE_REGISTER(rpmc_rom, CONFIG_OOB_RPMC_ROM_LOG_LEVEL);

/**
 * RPMC ROM Memory Map (for RPMC_ROM_MC_NUM = 8, RPMC_ROM_BLOCK_SIZE = 0x1000)
 *
 *  Address Range                        | Feature
 * --------------------------------------+--------------------------
 *  0x000080000 (RPMC_ROM_BASE_ADDR)     | PARAM (parameter/config) [0x1000]
 *  0x000081000                          | ROOT_KEY(0)   [0x1000]
 *  0x000082000                          | ROOT_KEY(1)   [0x1000]
 *  0x000083000                          | ROOT_KEY(2)   [0x1000]
 *  0x000084000                          | ROOT_KEY(3)   [0x1000]
 *  0x000085000                          | ROOT_KEY(4)   [0x1000]
 *  0x000086000                          | ROOT_KEY(5)   [0x1000]
 *  0x000087000                          | ROOT_KEY(6)   [0x1000]
 *  0x000088000                          | ROOT_KEY(7)   [0x1000]
 *  0x000089000                          | COUNTER_VALUE(0) [0x2000]
 *  0x00008B000                          | COUNTER_VALUE(1) [0x2000]
 *  0x00008D000                          | COUNTER_VALUE(2) [0x2000]
 *  0x00008F000                          | COUNTER_VALUE(3) [0x2000]
 *  0x000091000                          | COUNTER_VALUE(4) [0x2000]
 *  0x000093000                          | COUNTER_VALUE(5) [0x2000]
 *  0x000095000                          | COUNTER_VALUE(6) [0x2000]
 *  0x000097000                          | COUNTER_VALUE(7) [0x2000]
 *
 *  - PARAM region is 0x1000 bytes (4KB) at RPMC_ROM_BASE_ADDR.
 *  - Each ROOT_KEY region is 0x1000 bytes (4KB), starting at RPMC_ROM_ROOT_KEY_ADDR.
 *  - Each COUNTER_VALUE region is 0x2000 bytes (8KB), starting at RPMC_ROM_COUNTER_ADDR.
 *
 *  If RPMC_ROM_MC_NUM or RPMC_ROM_BLOCK_SIZE is changed, the map should be updated accordingly:
 *    - PARAM: RPMC_ROM_BASE_ADDR
 *    - ROOT_KEY(i): RPMC_ROM_ROOT_KEY_ADDR + i * RPMC_ROM_BLOCK_SIZE, for i in [0, RPMC_ROM_MC_NUM-1]
 *    - COUNTER_VALUE(i): RPMC_ROM_COUNTER_ADDR + i * (RPMC_ROM_BLOCK_SIZE * RPMC_ROM_MC_BLOCK_NUM), for i in [0, RPMC_ROM_MC_NUM-1]
 */
#define RPMC_ROM_RT_BLOCK_NUM       (1)
#define RPMC_ROM_MC_BLOCK_NUM       (2)

#define RPMC_ROM_PARAM_ADDR         (RPMC_ROM_BASE_ADDR)
#define RPMC_ROM_PARAM_SIZE         (RPMC_ROM_BLOCK_SIZE)

#define RPMC_ROM_ROOT_KEY_ADDR      (RPMC_ROM_PARAM_ADDR + RPMC_ROM_PARAM_SIZE)
#define RPMC_ROM_ROOT_KEY_SIZE      (RPMC_ROM_BLOCK_SIZE * RPMC_ROM_RT_BLOCK_NUM * RPMC_ROM_MC_NUM)

#define RPMC_ROM_COUNTER_ADDR       (RPMC_ROM_ROOT_KEY_ADDR + RPMC_ROM_ROOT_KEY_SIZE)
#define RPMC_ROM_COUNTER_SIZE       (RPMC_ROM_BLOCK_SIZE * RPMC_ROM_MC_BLOCK_NUM * RPMC_ROM_MC_NUM)

#define ESPI_ROM_ACCESS_SIZE_MAX    (RPMC_ROM_ACCESS_SIZE)
#define ESPI_FLASH_ERASE_DUMMY	    (1u)

#define RPMC_ROM_RT_INST_ADDR(n)    ((RPMC_ROM_BLOCK_SIZE * RPMC_ROM_RT_BLOCK_NUM * n) + RPMC_ROM_ROOT_KEY_ADDR)
#define RPMC_ROM_MC_INST_ADDR(n)    ((RPMC_ROM_BLOCK_SIZE * RPMC_ROM_MC_BLOCK_NUM * n) + RPMC_ROM_COUNTER_ADDR)

#define ESPI_DEV					DT_NODELABEL(espi0)

static struct spi_rom_dev_context g_rom_dev_ctx = {0};

/*****************************************************************************
 * Function name:   crc16_compute
 * Description:     Compute CRC16 checksum for data integrity verification
 * @param           data - Pointer to data buffer
 * @param           len  - Length of data buffer
 * @return          CRC16 checksum value
 *****************************************************************************/
static uint16_t crc16_compute(const uint8_t *data, uint16_t len)
{
	return crc16(0x1021, 0, data, len);
}

/*****************************************************************************
 * Function name:   _spi_rom_unique_id_read
 * Description:     Read unique ID from SPI ROM device
 * @param           instance - Device instance number
 * @param           pData    - Buffer to store unique ID
 * @param           len      - Buffer length
 * @return          None
 *****************************************************************************/
static void _spi_rom_unique_id_read(uint8_t instance, uint8_t *pData, uint8_t len)
{
	(void)instance;
	int ret = amd_crb_drivers_sFlash_int_sUniqId(len, pData);
	if (ret) {
		LOG_ERR("RPMC ROM read unique id error %d", ret);
	}
}

/*****************************************************************************
 * Function name:   _spi_rom_read
 * Description:     Read data from SPI ROM via internal flash driver
 * @param           addr - Flash address to read from
 * @param           len  - Number of bytes to read
 * @param           pBuf - Buffer to store read data
 * @return          None
 *****************************************************************************/
static void _spi_rom_read(uint32_t addr, uint16_t len, void *pBuf)
{
	int ret = amd_crb_drivers_sFlash_int_read(addr, len, (uint8_t*)pBuf);
	if (ret) {
		LOG_ERR("RPMC ROM read error: %d,offset=0x%x size=%d", ret, addr, len);
	}
}

/*****************************************************************************
 * Function name:   _spi_rom_write
 * Description:     Write data to SPI ROM via internal flash driver
 * @param           addr - Flash address to write to
 * @param           len  - Number of bytes to write
 * @param           pBuf - Buffer containing data to write
 * @return          None
 *****************************************************************************/
static void _spi_rom_write(uint32_t addr, uint16_t len, void *pBuf)
{
	int ret = amd_crb_drivers_sFlash_int_write(addr, len, (uint8_t*)pBuf);
	if (ret) {
		LOG_ERR("RPMC ROM write error: %d,offset=0x%x size=%d", ret, addr, len);
	}
}

/*****************************************************************************
 * Function name:   _spi_rom_erase
 * Description:     Erase SPI ROM block via internal flash driver
 * @param           addr - Flash address to erase (block aligned)
 * @return          None
 *****************************************************************************/
static void _spi_rom_erase(uint32_t addr)
{
	int ret = amd_crb_drivers_sFlash_int_erase(addr, RPMC_ROM_BLOCK_SIZE);
	if (ret) {
		LOG_ERR("RPMC ROM erase error: %d,offset=0x%x size=%d", ret, addr, RPMC_ROM_BLOCK_SIZE);
	}
}

/*****************************************************************************
 * Function name:   _espi_rom_read
 * Description:     Read data from ROM via eSPI interface with retry mechanism
 * @param           addr - Flash address to read from
 * @param           len  - Number of bytes to read
 * @param           pBuf - Buffer to store read data
 * @return          None
 *****************************************************************************/
static void _espi_rom_read(uint32_t addr, uint16_t len, void *pBuf)
{
	int ret = 0;
	uint16_t loop_cnt;
	struct espi_flash_packet pckt;

	for (uint16_t sent = 0; sent < len; sent += ESPI_ROM_ACCESS_SIZE_MAX) {
		// Pre-calculate chunk size once per outer loop instead of per retry
		uint16_t chunk_len = (len - sent >= ESPI_ROM_ACCESS_SIZE_MAX) ? 
							 ESPI_ROM_ACCESS_SIZE_MAX : (len - sent);
		
		// Pre-setup packet structure once per chunk
		pckt.buf = ((uint8_t *)pBuf) + sent;
		pckt.flash_addr = addr + sent;
		pckt.len = chunk_len;
		
		loop_cnt = 3;
		while (loop_cnt--) {
			ret = espi_read_flash(g_rom_dev_ctx.espi_dev, &pckt);
			if (ret) {
				LOG_ERR("Read spiAddr %06X gets error! (idx %d), retry ...", addr + sent, sent);
			} else {
				break;
			}
		};
	}
}

/*****************************************************************************
 * Function name:   _espi_rom_write
 * Description:     Write data to ROM via eSPI interface with retry mechanism
 * @param           addr - Flash address to write to
 * @param           len  - Number of bytes to write
 * @param           pBuf - Buffer containing data to write
 * @return          None
 *****************************************************************************/
static void _espi_rom_write(uint32_t addr, uint16_t len, void *pBuf)
{
	int ret = 0;
	uint16_t loop_cnt;
	struct espi_flash_packet pckt;

	for (uint16_t sent = 0; sent < len; sent += ESPI_ROM_ACCESS_SIZE_MAX) {
		// Pre-calculate chunk size once per outer loop instead of per retry
		uint16_t chunk_len = (len - sent >= ESPI_ROM_ACCESS_SIZE_MAX) ? 
							 ESPI_ROM_ACCESS_SIZE_MAX : (len - sent);
		
		// Pre-setup packet structure once per chunk
		pckt.buf = ((uint8_t *)pBuf) + sent;
		pckt.flash_addr = addr + sent;
		pckt.len = chunk_len;
		
		loop_cnt = 3;
		while (loop_cnt--) {
			ret = espi_write_flash(g_rom_dev_ctx.espi_dev, &pckt);
			if (ret) {
				LOG_ERR("Write spiAddr %06X gets error! (idx %d), retry ...", addr + sent, sent);
			} else {
				break;
			}
		};
	}
}

/*****************************************************************************
 * Function name:   _espi_rom_erase
 * Description:     Erase ROM block via eSPI interface with retry mechanism
 * @param           addr - Flash address to erase (block aligned)
 * @return          None
 *****************************************************************************/
static void _espi_rom_erase(uint32_t addr)
{
	int ret = 0;
	uint16_t loop_cnt = 3;
	struct espi_flash_packet pckt;

	while (loop_cnt--) {
		pckt.buf = NULL;
		pckt.flash_addr = addr;
		pckt.len = ESPI_FLASH_ERASE_DUMMY;
		ret = espi_flash_erase(g_rom_dev_ctx.espi_dev, &pckt);
		if (ret) {
			LOG_ERR("Erase spiAddr %06X gets error! retry ...", addr);
		} else {
			break;
		}
	};
}

/*****************************************************************************
 * Function name:   _rom_read_helper
 * Description:     Helper function to read from ROM via SPI or eSPI
 * @param           addr - Flash address to read from
 * @param           len  - Number of bytes to read
 * @param           pBuf - Buffer to store read data
 * @param           spi  - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 *****************************************************************************/
static void _rom_read_helper(uint32_t addr, uint16_t len, void *pBuf, uint8_t spi)
{
	spi = 1; // Force SPI access for security and reliability
	
	if (spi) {
		_spi_rom_read(addr, len, pBuf);
	} else {
		_espi_rom_read(addr, len, pBuf);
	}
}

/*****************************************************************************
 * Function name:   _rom_write_helper
 * Description:     Helper function to write to ROM via SPI or eSPI
 * @param           addr - Flash address to write to
 * @param           len  - Number of bytes to write
 * @param           pBuf - Buffer containing data to write
 * @param           spi  - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 *****************************************************************************/
static void _rom_write_helper(uint32_t addr, uint16_t len, void *pBuf, uint8_t spi)
{
	spi = 1; // Force SPI access for security and reliability

	if (spi) {
		_spi_rom_write(addr, len, pBuf);
	} else {
		_espi_rom_write(addr, len, pBuf);
	}
}

/*****************************************************************************
 * Function name:   _rom_erase_helper
 * Description:     Helper function to erase ROM block via SPI or eSPI
 * @param           addr - Flash address to erase (block aligned)
 * @param           spi  - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 *****************************************************************************/
static void _rom_erase_helper(uint32_t addr, uint8_t spi)
{
	spi = 1; // Force SPI access for security and reliability

	if (spi) {
		_spi_rom_erase(addr);
	} else {
		_espi_rom_erase(addr);
	}
}

/*****************************************************************************
 * Function name:   _data_empty
 * Description:     Check if data buffer is empty (all 0xFF or all 0x00)
 * @param           pData      - Data buffer to check
 * @param           len        - Buffer length
 * @param           check_zero - Flag to also check for all zeros
 * @return          True if empty, false otherwise
 *****************************************************************************/
static bool _data_empty(uint8_t *pData, uint8_t len, bool check_zero)
{
    uint8_t first = pData[0];
    if (first != 0xFFu && (!check_zero || first != 0x0u)) {
        return false;
    }

    bool all_ff = true;
    bool all_00 = check_zero ? true : false;

    for (uint8_t i = 0; i < len; ++i) {
        if (pData[i] != 0xFFu) {
            all_ff = false;
        }
        if (check_zero && pData[i] != 0x0u) {
            all_00 = false;
        }
        if (!all_ff && (!all_00 || !check_zero)) {
            return false;
        }
    }

    return all_ff || (check_zero && all_00);
}

/*****************************************************************************
 * Function name:   _data_match
 * Description:     Compare two data buffers for equality
 * @param           pData_1  - First data buffer
 * @param           length_1 - First buffer length
 * @param           pData_2  - Second data buffer
 * @param           length_2 - Second buffer length
 * @return          0 if match, negative if different
 *****************************************************************************/
static inline int _data_match(const void *pData_1, size_t length_1, const void *pData_2, size_t length_2)
{
	if (length_1 != length_2) {
		return -1;
	}

	if (memcmp(pData_2, pData_1, length_2) != 0) {
		return -2;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   _mc_write
 * Description:     Write monotonic counter value with checksum to ROM
 * @param           addr  - Flash address to write to
 * @param           value - Counter value to write
 * @param           cycle - Counter cycle to write
 * @param           spi   - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 * @note            Automatically calculates and stores CRC16 checksum
 *****************************************************************************/
static void _mc_write(uint32_t addr, uint32_t value, uint16_t cycle, uint8_t spi)
{
	struct mc_data data = {0};
	uint16_t chksum = 0;
	uint8_t buf[sizeof(value) + sizeof(cycle)] = {0};

	memcpy(buf, &value, sizeof(value));
	memcpy(buf + sizeof(value), &cycle, sizeof(cycle));
	chksum = crc16_compute(buf, sizeof(buf));

	data.value = value;
	data.cycle = cycle;
	data.chksum = chksum;

	_rom_write_helper(addr, sizeof(struct mc_data), (uint8_t*)&data, spi);
}

/*****************************************************************************
 * Function name:   _rt_write
 * Description:     Write root key data with checksum to ROM
 * @param           addr  - Flash address to write to
 * @param           value - Root key data to write
 * @param           len   - Length of root key data
 * @param           cycle - Root key cycle to write
 * @param           spi   - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 * @note            Automatically calculates and stores CRC16 checksum
 *****************************************************************************/
static void _rt_write(uint32_t addr, uint8_t *value, uint8_t len, uint16_t cycle, uint8_t spi)
{
	struct rt_data data = {0};
	uint16_t chksum = 0;
	uint8_t buf[RPMC_ROM_RK_DATA_SIZE + sizeof(cycle)] = {0};

	if (len != sizeof(data.value) && len != RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RT length %d not valid", len);
		return;
	}

	memcpy(buf, value, len);
	memcpy(buf + len, &cycle, sizeof(cycle));
	chksum = crc16_compute(buf, sizeof(buf));

	memcpy(data.value, value, len);
	data.cycle = cycle;
	data.chksum = chksum;
	memset(data.rsvd, 0xFF, sizeof(data.rsvd));

	_rom_write_helper(addr, sizeof(struct rt_data), (uint8_t*)&data, spi);
}

#if (RPMC_ROM_ALW_DIRECT_RD)
/*****************************************************************************
 * Function name:   _mc_read
 * Description:     Read monotonic counter value and checksum from ROM
 * @param           addr   - Flash address to read from
 * @param           value  - Pointer to store counter value
 * @param           cycle  - Pointer to store counter cycle
 * @param           chksum - Pointer to store checksum
 * @param           spi    - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 *****************************************************************************/
static void _mc_read(uint32_t addr, uint32_t *value, uint16_t *cycle, uint16_t *chksum, uint8_t spi)
{
	struct mc_data data = {0};

	_rom_read_helper(addr, sizeof(struct mc_data), (uint8_t*)&data, spi);
	*value = data.value;
	*cycle = data.cycle;
	*chksum = data.chksum;
}

/*****************************************************************************
 * Function name:   _rt_read
 * Description:     Read root key data and checksum from ROM
 * @param           addr   - Flash address to read from
 * @param           value  - Buffer to store root key data
 * @param           len    - Length of root key data
 * @param           cycle  - Pointer to store root key cycle
 * @param           chksum - Pointer to store checksum
 * @param           spi    - Bus selection (1: SPI, 0: eSPI)
 * @return          None
 *****************************************************************************/
static void _rt_read(uint32_t addr, uint8_t *value, uint8_t len, uint16_t *cycle, uint16_t *chksum, uint8_t spi)
{
	struct rt_data data = {0};

	if (len != sizeof(data.value) && len != RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RT length %d not valid", len);
		return;
	}

	_rom_read_helper(addr, sizeof(struct rt_data), (uint8_t*)&data, spi);
	memcpy(value, data.value, len);
	*cycle = data.cycle;
	*chksum = data.chksum;
}
#endif

/*****************************************************************************
 * Function name:   _mc_check
 * Description:     Verify monotonic counter value against its checksum
 * @param           value  - Counter value to verify
 * @param           cycle  - Counter cycle to verify
 * @param           chksum - Stored checksum
 * @return          True if checksum is valid, false otherwise
 *****************************************************************************/
static bool _mc_check(uint32_t value, uint16_t cycle, uint16_t chksum)
{
	uint8_t buf[sizeof(value) + sizeof(cycle)] = {0};
	memcpy(buf, &value, sizeof(value));
	memcpy(buf + sizeof(value), &cycle, sizeof(cycle));

	if (crc16_compute(buf, sizeof(buf)) != chksum) {
		LOG_ERR("MC checksum error: 0x%x", chksum);
		return false;
	}
	return true;
}

/*****************************************************************************
 * Function name:   _rt_check
 * Description:     Verify root key data against its checksum
 * @param           value  - Root key data to verify
 * @param           len    - Length of root key data
 * @param           cycle  - Root key cycle to verify
 * @param           chksum - Stored checksum
 * @return          True if checksum is valid, false otherwise
 *****************************************************************************/
static bool _rt_check(uint8_t *value, uint8_t len, uint16_t cycle, uint16_t chksum)
{
	uint8_t buf[RPMC_ROM_RK_DATA_SIZE + sizeof(cycle)] = {0};
	memcpy(buf, value, len);
	memcpy(buf + len, &cycle, sizeof(cycle));

	if (len != RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RT length %d not valid", len);
		return false;
	}

	if (crc16_compute(buf, sizeof(buf)) != chksum) {
		LOG_ERR("RT checksum error: 0x%x", chksum);
		return false;
	}
	return true;
}

/*****************************************************************************
 * Function name:   _rom_rpmc_timerCallback
 * Description:     Timer callback to trigger deferred ROM erase operations
 * @param           timer - Pointer to timer structure (unused)
 * @return          None
 * @note            Signals the work queue to perform pending erase operations
 * @note            Called after RPMC_ROM_WORK_POST_DELAY_MS timeout
 *****************************************************************************/
static void _rom_rpmc_timerCallback(struct k_timer *timer)
{
	k_work_submit(&g_rom_dev_ctx.rom_work);
}

/*****************************************************************************
 * Function name:   _rom_post_worksCallback
 * Description:     Work queue callback for deferred ROM erase operations
 * @param           w - Pointer to work structure (unused)
 * @return          None
 * @note            Processes pending erase operations for all RPMC instances
 * @note            Triggered by timer expiration or signal events
 * @note            Resets poll event state and resubmits work for next cycle
 *****************************************************************************/
static void _rom_post_worksCallback(struct k_work *w)
{
	LOG_DBG("Post works callback");
	k_timer_stop(&g_rom_dev_ctx.rom_timer);

	for(uint8_t i = 0; i < RPMC_ROM_MC_NUM; i++) {
		struct spi_rom_data *p_rom_data = &g_rom_dev_ctx.data[i];
		if (!p_rom_data->inited) {
			continue;
		}

		struct post_work *work = &g_rom_dev_ctx.post_work;
		if (work->erase_addr[i]) {
			_rom_erase_helper(work->erase_addr[i], true);
			LOG_INF("Erase ROM MC in post work handler at addr: 0x%x", work->erase_addr[i]);
			work->erase_addr[i] = 0;
		}
	}
}

/*****************************************************************************
 * Function name:   _rom_post_works_notify
 * Description:     Initiate deferred erase operation by starting timer
 * @param           None
 * @return          None
 * @note            Restarts timer if already running, enabling new countdown
 * @note            Timer expires after RPMC_ROM_WORK_POST_DELAY_MS milliseconds
 * @note            Used to batch erase operations and reduce flash wear
 *****************************************************************************/
static void _rom_post_works_notify(void)
{
	/* It will kill the timer if it is already running, and enable a new count down */
	k_timer_start(&g_rom_dev_ctx.rom_timer,  K_MSEC(RPMC_ROM_WORK_POST_DELAY_MS), K_NO_WAIT);
}

/*****************************************************************************
 * Function name:   _rom_works_init
 * Description:     Initialize work queue system for deferred ROM operations
 * @param           None
 * @return          None
 * @note            Sets up timer, polling events, and work queue infrastructure
 * @note            Must be called during ROM initialization
 * @note            Creates persistent work queue for background erase operations
 *****************************************************************************/
static void _rom_works_init(void)
{
	k_timer_init(&g_rom_dev_ctx.rom_timer, _rom_rpmc_timerCallback, NULL);

	k_work_init(&g_rom_dev_ctx.rom_work, _rom_post_worksCallback);
}

/*****************************************************************************
 * Function name:   _mc_program
 * Description:     Program monotonic counter with wear-leveling logic
 * @param           instance  - RPMC instance number for deferred erase tracking
 * @param           base_addr - Base address of counter region
 * @param           addr      - Pointer to current write address (updated)
 * @param           value     - Counter value to program
 * @param           cycle     - Pointer to current write cycle (updated)
 * @param           spi       - Bus selection (1: SPI, 0: eSPI)
 * @return          0 on success
 * @note            Implements wear-leveling by alternating between blocks
 * @note            Uses deferred erase operations to improve performance
 * @note            Schedules erase operations via work queue system
 *****************************************************************************/
static int _mc_program(uint8_t instance, uint32_t base_addr, uint32_t *addr, uint32_t value, uint16_t *cycle, uint8_t spi)
{
	uint32_t spi_addr = *addr;
	uint32_t erase_addr = 0;
	const uint32_t block_size = RPMC_ROM_BLOCK_SIZE;
	const uint32_t block_end_offset = block_size - sizeof(struct mc_data);
	const uint32_t alt_block_addr = base_addr + 2 * block_size - sizeof(struct mc_data);

	/* Init address if not set */
	if (spi_addr == 0) {
		spi_addr = alt_block_addr;
	} else {
		if (spi_addr == base_addr) {
			erase_addr = spi_addr;
			spi_addr = alt_block_addr;
		} else {
			spi_addr -= sizeof(struct mc_data);
			if (spi_addr == base_addr + block_end_offset) {
				erase_addr = *addr;
			}
		}
	}

	if (erase_addr) {
		if (g_rom_dev_ctx.post_work.erase_addr[instance] == 0) {
			g_rom_dev_ctx.post_work.erase_addr[instance] = erase_addr;
			_rom_post_works_notify();
			LOG_INF("Post work MC erase addr set at addr: 0x%x", erase_addr);
		} else {
			_rom_erase_helper(erase_addr, spi);
			_rom_erase_helper(g_rom_dev_ctx.post_work.erase_addr[instance], spi);
			LOG_ERR("MC erase addr already set: 0x%x and 0x%x", erase_addr, g_rom_dev_ctx.post_work.erase_addr[instance]);
			g_rom_dev_ctx.post_work.erase_addr[instance] = 0;
		}
	}

	if (!value) {
		*cycle += 1;
	}
	*addr = spi_addr;
	_mc_write(spi_addr, value, *cycle, spi);
	LOG_DBG("MC program at addr: 0x%x, value: 0x%x, cycle: 0x%x", spi_addr, value, *cycle);

	return 0;
}

/*****************************************************************************
 * Function name:   _rt_program
 * Description:     Program root key with wear-leveling logic
 * @param           base_addr - Base address of root key region
 * @param           addr      - Pointer to current write address (updated)
 * @param           value     - Root key data to program
 * @param           len       - Length of root key data
 * @param           cycle     - Pointer to current write cycle (updated)
 * @param           spi       - Bus selection (1: SPI, 0: eSPI)
 * @return          0 on success
 * @note            Implements wear-leveling within single block
 *****************************************************************************/
static int _rt_program(uint32_t base_addr, uint32_t *addr, uint8_t *value, uint8_t len, uint16_t *cycle, uint8_t spi)
{
	uint32_t spi_addr = *addr;
	uint32_t erase_addr = 0;
	const uint32_t block_size = RPMC_ROM_BLOCK_SIZE;
	const uint32_t alt_block_addr = base_addr + block_size - sizeof(struct rt_data);

	if (len != RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RT length %d not valid", len);
		return -1;
	}

	/* Init address if not set */
	if (spi_addr == 0) {
		spi_addr = alt_block_addr;
	} else {
		if (spi_addr == base_addr) {
			erase_addr = spi_addr;
			spi_addr = alt_block_addr;
		} else {
			spi_addr -= sizeof(struct rt_data);
		}
	}

	if (erase_addr) {
		_rom_erase_helper(erase_addr, spi);
		LOG_DBG("RT erase at addr: 0x%x", erase_addr);
	}

	*cycle += 1;
	*addr = spi_addr;
	_rt_write(spi_addr, value, len, *cycle, spi);
	LOG_DBG("RT program at addr: 0x%x, cycle: 0x%x", spi_addr, *cycle);

	return 0;
}

/*****************************************************************************
 * Function name:   _mc_init
 * Description:     Initialize monotonic counter by scanning existing data
 * @param           base_addr - Base address of counter region
 * @param           value     - Pointer to store found counter value
 * @param           cycle     - Pointer to store found counter cycle
 * @param           addr      - Pointer to store current write address
 * @param           spi       - Bus selection (1: SPI, 0: eSPI)
 * @return          0 on success
 * @note            Scans both blocks, selects highest valid value
 * @note            Erases corrupted or unused blocks
 *****************************************************************************/
static int _mc_init(uint32_t base_addr, uint32_t *value, uint16_t *cycle, uint32_t *addr, uint8_t spi)
{
	struct mc_data fd_mc[RPMC_ROM_MC_BLOCK_NUM] = {0};
	uint32_t fd_addr[RPMC_ROM_MC_BLOCK_NUM] = {0};
	bool fd_broken = false;
	uint8_t fd_bits = 0;
	uint8_t fd_block = 0;
	uint32_t fd_val = 0;
	uint16_t fd_cycle = 0;

	for (uint8_t block = 0; block < RPMC_ROM_MC_BLOCK_NUM; ++block) {
		uint8_t chunk_data[RPMC_ROM_ACCESS_SIZE];
		uint32_t block_base = base_addr + block * RPMC_ROM_BLOCK_SIZE;
		uint32_t block_end = block_base + RPMC_ROM_BLOCK_SIZE;
		struct mc_data *mc = &fd_mc[block];

		fd_broken = false;
		for (uint32_t spi_addr = block_base; spi_addr < block_end; spi_addr += sizeof(chunk_data)) {
			_rom_read_helper(spi_addr, sizeof(chunk_data), chunk_data, spi);

			for (uint16_t i = 0; i < sizeof(chunk_data); i += sizeof(struct mc_data)) {
				struct mc_data *pData = (struct mc_data *)(chunk_data + i);
				uint32_t cur_addr = spi_addr + i;

				if (_data_empty((uint8_t*)pData, sizeof(struct mc_data), false)) {
					continue;
				}

				if (!_mc_check(pData->value, pData->cycle, pData->chksum)) {
					LOG_ERR("MC value broken at addr: 0x%x", cur_addr);

					// Mark first broken data in block
					if (fd_addr[block] == 0) {
						fd_addr[block] = cur_addr;
					}
					fd_broken = true;
					continue;
				}

				// Found valid data
				mc->value = pData->value;
				mc->cycle = pData->cycle;
				if (fd_addr[block] == 0) {
					fd_addr[block] = cur_addr;
				}

				/* Update the found data if it meets the condition */
				uint32_t erase_addr = 0;
				uint16_t cur_cycle = pData->cycle;
				uint16_t wrap_offset = RPMC_ROM_BLOCK_SIZE / 2;
				bool cycle_reset_cond_a = ((cur_cycle < wrap_offset) && (fd_cycle > (UINT16_MAX - wrap_offset)));
				bool cycle_reset_cond_b = ((cur_cycle > (UINT16_MAX - wrap_offset)) && (fd_cycle < wrap_offset));
				bool cycle_wraparound = cycle_reset_cond_a || cycle_reset_cond_b;

				if (((fd_cycle == cur_cycle) && (fd_val <= pData->value)) || // Same cycle: prefer the greater value.
					((!cycle_wraparound) && (fd_cycle < cur_cycle)) ||       // Different cycle without wraparound: prefer the greater cycle.
					((cycle_reset_cond_a)) ||                                // Different cycle with wraparound: prefer the smaller cycle.
					(!fd_bits)) {                                            // Init first found data
					
					if (fd_bits) {
						erase_addr = base_addr + RPMC_ROM_BLOCK_SIZE * fd_block;
					}
					fd_block = block;

					fd_cycle = cur_cycle;
					fd_val = pData->value;
				} else {
					erase_addr = block_base;
				}

				if (erase_addr) {
					_rom_erase_helper(erase_addr, spi);
					LOG_INF("Erase used MC block at addr: 0x%x", erase_addr);
				}

				fd_broken = false;
				fd_bits |= BIT(block);
				break;
			}

			/* If valid data is found in the block, break the loop */
			if (fd_bits & BIT(block)) {
				break;
			}
		}

		/* If the block is broken, erase the block and set the address to 0 */
		if (fd_broken) {
			fd_addr[block] = 0;
			_rom_erase_helper(block_base, spi);

			LOG_ERR("Erase broken MC block addr: 0x%x", block_base);
		}
	}

	if (!fd_bits) {
		*value = 0;
		*addr = 0;
		*cycle = 0;

		LOG_INF("MC not found");
		return 0;
	}

	*value = fd_mc[fd_block].value;
	*cycle = fd_mc[fd_block].cycle;
	*addr = fd_addr[fd_block];
	LOG_INF("MC found 0x%x cycle: 0x%x at addr: 0x%x", *value, *cycle, *addr);
	return 0;
}

/*****************************************************************************
 * Function name:   _rt_init
 * Description:     Initialize root key by scanning existing data
 * @param           base_addr - Base address of root key region
 * @param           value     - Buffer to store found root key data
 * @param           len       - Length of root key buffer
 * @param           cycle     - Pointer to store found root key cycle
 * @param           addr      - Pointer to store current write address
 * @param           spi       - Bus selection (1: SPI, 0: eSPI)
 * @return          0 on success
 * @note            Returns 0xFF filled buffer if no valid data found
 * @note            Erases corrupted blocks
 *****************************************************************************/
static int _rt_init(uint32_t base_addr, uint8_t *value, uint8_t len, uint16_t *cycle, uint32_t *addr, uint8_t spi)
{
	bool fd_broken = false;
	uint8_t fd_val[RPMC_ROM_RK_DATA_SIZE] = {0};
	uint32_t fd_address = 0;
	uint16_t fd_cycle = 0;

	if (len != RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RT length %d not valid", len);
		return -1;
	}

	uint8_t chunk_data[RPMC_ROM_ACCESS_SIZE];
	for (uint32_t spi_addr = base_addr; spi_addr < base_addr + RPMC_ROM_BLOCK_SIZE; spi_addr += sizeof(chunk_data)) {
		_rom_read_helper(spi_addr, sizeof(chunk_data), chunk_data, spi);

		struct rt_data *pData = (struct rt_data *)chunk_data;
		uint32_t cur_addr = spi_addr;
		if (_data_empty((uint8_t*)pData, sizeof(struct rt_data), false)) {
			continue;
		}

		if (!_rt_check(pData->value, sizeof(pData->value), pData->cycle, pData->chksum)) {
			LOG_ERR("RT value broken at addr: 0x%x", cur_addr);

			// Mark first broken data in block
			fd_broken = true;
			if (fd_address == 0) {
				fd_address = cur_addr;
			}
			continue;
		}

		// Found valid data
		fd_broken = false;
		fd_cycle = pData->cycle;
		memcpy(fd_val, pData->value, len);
		if (fd_address == 0) {
			fd_address = cur_addr;
		}
		break;
	}

	/* Erase broken block */
	if (fd_broken) {
		fd_address = 0;
		_rom_erase_helper(base_addr, spi);
		LOG_ERR("Erase broken RT block addr: 0x%x", base_addr);
	}

	/* If no valid data found */
	if (fd_address == 0) {
		memset(value, 0xFFu, len);
		*addr = 0;
		*cycle = 0;

		LOG_INF("RT not found");
		return 0;
	}

	memcpy(value, fd_val, len);
	*addr = fd_address;
	*cycle = fd_cycle;

	LOG_INF("RT found at addr: 0x%x, cycle: 0x%x", fd_address, fd_cycle);
	return 0;
}

/*****************************************************************************
 * Function name:   _param_init
 * Description:     Initialize parameter data
 * @param           instance    - RPMC instance number
 * @param           p_unique_id - Unique ID
 * @param           version     - Version
 * @param           spi         - Bus selection (1: SPI, 0: eSPI)
 * @return          0 on success
 *****************************************************************************/
static int _param_init(uint8_t instance, uint8_t *p_unique_id, uint16_t version, uint8_t spi)
{
	if (instance) {
		return 0; // no needs to initialize
	}

	struct param_data data = {0};
	_rom_read_helper(RPMC_ROM_PARAM_ADDR, sizeof(struct param_data), (uint8_t*)&data, spi);
	if (version != RPMC_ROM_VERSION_DEBUG) {
		if ((_data_match(data.value, sizeof(data.value), p_unique_id, RPMC_ROM_UNIQUE_ID_SIZE) == 0) && 
			(data.version == version)) {
			return 0;
		}
	}
	LOG_WRN("Have to initialize RPMC ROM version: %d %d", data.version, version);
	LOG_HEXDUMP_WRN(data.value, sizeof(data.value), "Old ID:");
	LOG_HEXDUMP_WRN(p_unique_id, RPMC_ROM_UNIQUE_ID_SIZE, "New ID:");

	/* Initialize parameter data and erase RT and MC blocks */
	_rom_erase_helper(RPMC_ROM_PARAM_ADDR, spi);
	LOG_DBG("Erase parameter data at addr: 0x%x", RPMC_ROM_PARAM_ADDR);
	for (uint32_t addr = RPMC_ROM_ROOT_KEY_ADDR; addr < RPMC_ROM_ROOT_KEY_ADDR + RPMC_ROM_ROOT_KEY_SIZE; addr += RPMC_ROM_BLOCK_SIZE) {
		_rom_erase_helper(addr, spi);
		LOG_DBG("Erase root key at addr: 0x%x", addr);
	}
	for (uint32_t addr = RPMC_ROM_COUNTER_ADDR; addr < RPMC_ROM_COUNTER_ADDR + RPMC_ROM_COUNTER_SIZE; addr += RPMC_ROM_BLOCK_SIZE) {
		_rom_erase_helper(addr, spi);
		LOG_DBG("Erase monotonic counter at addr: 0x%x", addr);
	}

	memcpy(data.value, p_unique_id, sizeof(data.value));
	data.version = version;
	data.chksum = crc16_compute((uint8_t*)&data, sizeof(data));
	_rom_write_helper(RPMC_ROM_PARAM_ADDR, sizeof(struct param_data), (uint8_t*)&data, spi);
	LOG_DBG("Initialize unique ID at addr: 0x%x", RPMC_ROM_PARAM_ADDR);

	return 0;
}

/*****************************************************************************
 * Function name:   rom_rpmc_data_init
 * Description:     Initialize RPMC ROM data structures for all instances
 * @param           num - Number of RPMC instances (must equal RPMC_ROM_MC_NUM)
 * @return          0 on success, negative on error
 *                  -1: Invalid device number
 *                  -2: Internal flash device not ready
 *                  -3: Parameter initialization failed
 *                  -4: ESPI device not ready or root key init failed
 *                  -5: Counter value initialization failed
 * @note            Must be called before using any other RPMC functions
 * @note            Initializes unique IDs, root keys, and monotonic counters
 *****************************************************************************/
int rom_rpmc_data_init(uint8_t num)
{
	if (num > RPMC_ROM_MC_NUM) {
		LOG_ERR("RPMC ROM invalid device number %d", num);
		return -1;
	}

	const struct device *intflash_dev = DEVICE_DT_GET(INT_FLASH);
	if (!device_is_ready(intflash_dev)) {
		LOG_ERR("Internal flash Device not ready");
		return -2;
	}

	g_rom_dev_ctx.espi_dev = DEVICE_DT_GET(ESPI_DEV);
	if (!device_is_ready(g_rom_dev_ctx.espi_dev)) {
		LOG_ERR("ESPI Device not ready");
		return -4;
	}

	for(uint8_t i = 0; i < RPMC_ROM_MC_NUM; i++) {
		struct spi_rom_data *p_rom_data = &g_rom_dev_ctx.data[i];

		if (p_rom_data->inited) {
			continue;
		}

		memset(p_rom_data->unique_id, 0x0, sizeof(p_rom_data->unique_id));
		_spi_rom_unique_id_read(i, p_rom_data->unique_id, sizeof(p_rom_data->unique_id));

		int ret = _param_init(i, p_rom_data->unique_id, RPMC_ROM_VERSION, true);
		if (ret != 0) {
			LOG_ERR("SPI ROM RPMC param init fail: %d", ret);
			return -3;
		}
		LOG_HEXDUMP_DBG(p_rom_data->unique_id, sizeof(p_rom_data->unique_id), "Unique ID:");

		ret = _rt_init(RPMC_ROM_RT_INST_ADDR(i), p_rom_data->rootk, sizeof(p_rom_data->rootk), &p_rom_data->rt_cycle, &p_rom_data->rt_addr, true);
		if (ret != 0) {
			LOG_ERR("SPI ROM RPMC root key init fail: %d", ret);
			return -4;
		}
		LOG_HEXDUMP_DBG(p_rom_data->rootk, sizeof(p_rom_data->rootk), "Root key:");

		ret = _mc_init(RPMC_ROM_MC_INST_ADDR(i), &p_rom_data->u32_ct, &p_rom_data->ct_cycle, &p_rom_data->ct_addr, true);
		if (ret != 0) {
			LOG_ERR("SPI ROM RPMC counter value init fail: %d", ret);
			return -5;
		}
		LOG_DBG("Counter value: 0x%x", p_rom_data->u32_ct);

		p_rom_data->inited = true;
	}

	_rom_works_init();
	return 0;
}

/*****************************************************************************
 * Function name:   rom_unique_id_read
 * Description:     Read unique ID for specified RPMC instance
 * @param           inst  - RPMC instance number
 * @param           pData - Buffer to store unique ID
 * @param           len   - Length of buffer
 * @return          0 on success, -1 if instance not initialized
 * @note            Copies min(RPMC_ROM_UNIQUE_ID_SIZE, len) bytes
 *****************************************************************************/
int rom_unique_id_read(uint8_t inst, uint8_t* pData, uint8_t len)
{
	struct spi_rom_data *p_rom_data = &g_rom_dev_ctx.data[inst];
	if (!p_rom_data->inited) {
		LOG_ERR("RPMC ROM instance %d not initialized", inst);
		return -1;
	}

	memcpy(pData, p_rom_data->unique_id, MIN(RPMC_ROM_UNIQUE_ID_SIZE, len));
	return 0;
}

/*****************************************************************************
 * Function name:   rom_rpmc_mc_access
 * Description:     Access (read/write) monotonic counter for specified instance
 * @param           instance - RPMC instance number (0 to RPMC_ROM_MC_NUM-1)
 * @param           pData    - Buffer for read/write data
 * @param           len      - Buffer length (must be RPMC_ROM_MC_DATA_SIZE)
 * @param           read     - Access type: true=read, false=write
 * @param           bus      - Access method: RPMC_ROM_BUS_SPI/ESPI/CACHE
 * @return          0 on success, negative on error:
 *                  -1: Invalid instance number
 *                  -2: Invalid buffer length
 *                  -3: Instance not initialized
 *                  -4: Write not allowed for cache access
 * @note            CACHE access reads from memory, SPI/ESPI access flash
 * @note            Write operations update both flash and cache
 *****************************************************************************/
int rom_rpmc_mc_access(uint8_t instance, uint8_t *pData, uint8_t len, bool read, uint8_t bus)
{
	struct spi_rom_data *p_rom_data = NULL;
	bool spi = (bus == RPMC_ROM_BUS_SPI) ? true : false;
	
	/* Check null pointer first */
	if (pData == NULL) {
		LOG_ERR("RPMC ROM pData is NULL");
		return -1;
	}

	if (instance >= RPMC_ROM_MC_NUM) {
		LOG_ERR("RPMC ROM instance %d not valid", instance);
		return -2;
	}
	
	if (len != RPMC_ROM_MC_DATA_SIZE) {
		LOG_ERR("RPMC ROM instance %d buffer length %d not valid", instance, len);
		return -3;
	}
	
	p_rom_data = &g_rom_dev_ctx.data[instance];
	if (!p_rom_data->inited) {
		LOG_ERR("RPMC ROM instance %d not initialized", instance);
		return -4;
	}

	if (bus == RPMC_ROM_BUS_CACHE) {
		if (!read) {
			LOG_ERR("RPMC ROM instance %d write not allowed for cache access", instance);
			return -5;
		}
		memcpy(pData, (uint8_t *)&p_rom_data->u32_ct, sizeof(p_rom_data->u32_ct));
		goto exit;
	}
	
	if (bus == RPMC_ROM_BUS_SPI) {
		// SPI Bus initialization required for direct flash access
	}

#if (RPMC_ROM_ALW_DIRECT_RD)
	uint32_t ct_val = 0;
	uint16_t ct_cycle = 0;
	uint16_t chksum = 0;
	_mc_read(p_rom_data->ct_addr, &ct_val, &ct_cycle, &chksum, spi);
	if (_mc_check(ct_val, ct_cycle, chksum)) {
		p_rom_data->u32_ct = ct_val;
		p_rom_data->ct_cycle = ct_cycle;
	}
#endif

	if (read) {
		memcpy(pData, (uint8_t *)&p_rom_data->u32_ct, sizeof(p_rom_data->u32_ct));
		goto exit;
	}

	if( _data_match((uint8_t *)&p_rom_data->u32_ct, sizeof(p_rom_data->u32_ct), pData, len) == 0) {
		goto exit;
	}

	_mc_program(instance, RPMC_ROM_MC_INST_ADDR(instance), &p_rom_data->ct_addr, *((uint32_t *)pData), &p_rom_data->ct_cycle, spi);
	memcpy((uint8_t *)&p_rom_data->u32_ct, pData, sizeof(p_rom_data->u32_ct));

exit:
	if (bus == RPMC_ROM_BUS_SPI) {
		// SPI Bus cleanup and resource release
	}
	return 0;
}

/*****************************************************************************
 * Function name:   rom_rpmc_rt_access
 * Description:     Access (read/write) root key for specified instance
 * @param           instance - RPMC instance number (0 to RPMC_ROM_MC_NUM-1)
 * @param           pData    - Buffer for read/write data
 * @param           len      - Buffer length (must be RPMC_ROM_RK_DATA_SIZE)
 * @param           read     - Access type: true=read, false=write
 * @param           bus      - Access method: RPMC_ROM_BUS_SPI/ESPI/CACHE
 * @return          0 on success, negative on error:
 *                  -1: Invalid instance number
 *                  -2: Invalid buffer length
 *                  -3: Instance not initialized
 *                  -4: Write not allowed for cache access
 * @note            CACHE access reads from memory, SPI/ESPI access flash
 * @note            Write operations update both flash and cache
 * @note            Root key is 32-byte SHA256 hash value
 *****************************************************************************/
int rom_rpmc_rt_access(uint8_t instance, uint8_t *pData, uint8_t len, bool read, uint8_t bus)
{
	uint8_t rk_tmp[RPMC_ROM_RK_DATA_SIZE] = {0};
	struct spi_rom_data *p_rom_data = NULL;
	bool spi = (bus == RPMC_ROM_BUS_SPI) ? true : false;
	
	/* Check null pointer first */
	if (pData == NULL) {
		LOG_ERR("RPMC ROM pData is NULL");
		return -1;
	}

	if (instance >= RPMC_ROM_MC_NUM) {
		LOG_ERR("RPMC ROM instance %d not valid", instance);
		return -2;
	}

	if (len > RPMC_ROM_RK_DATA_SIZE) {
		LOG_ERR("RPMC ROM instance %d buffer length %d is too long", instance, len);
		return -3;
	}
	
	p_rom_data = &g_rom_dev_ctx.data[instance];
	if (!p_rom_data->inited) {
		LOG_ERR("RPMC ROM instance %d not initialized", instance);
		return -4;
	}

	if (bus == RPMC_ROM_BUS_CACHE) {
		if (!read) {
			LOG_ERR("RPMC ROM instance %d write not allowed for cache access", instance);
			return -5;
		}
		memcpy(pData, p_rom_data->rootk, len);
		goto exit;
	}
	
	if (bus == RPMC_ROM_BUS_SPI) {
		// SPI Bus initialization required for direct flash access
	} 

#if (RPMC_ROM_ALW_DIRECT_RD)
	uint16_t rt_cycle = 0;
	uint16_t chksum = 0;
	_rt_read(p_rom_data->rt_addr, rk_tmp, sizeof(rk_tmp), &rt_cycle, &chksum, spi);
	if (_rt_check(rk_tmp, sizeof(rk_tmp), rt_cycle, chksum)) {
		memcpy(p_rom_data->rootk, rk_tmp, len);
	}
#endif

	if (read) {
		memcpy(pData, p_rom_data->rootk, len);
		goto exit;
	}

	if( _data_match(p_rom_data->rootk, len, pData, len) == 0) {
		goto exit;
	}

	memset(rk_tmp, 0xFFu, sizeof(rk_tmp));
	memcpy(rk_tmp, pData, len);
	_rt_program(RPMC_ROM_RT_INST_ADDR(instance), &p_rom_data->rt_addr, rk_tmp, sizeof(rk_tmp), &p_rom_data->rt_cycle, spi);
	memcpy(p_rom_data->rootk, pData, len);

exit:
	if (bus == RPMC_ROM_BUS_SPI) {
		// SPI Bus cleanup and resource release
	}
	return 0;
}