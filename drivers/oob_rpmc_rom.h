/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#ifndef __OOB_RPMC_ROM_H__
#define __OOB_RPMC_ROM_H__

#include <zephyr/toolchain/gcc.h>
#include "board_config.h"

#define RPMC_ROM_VERSION_DEBUG   (0) /* Debug version for development, will erase the rom data after power on */

#define RPMC_ROM_SHA_MAX_SIZE    (48u) /* SHA-256/384/3/384: 32 bytes, SHA-256/384/3/384: 48 bytes */
#define RPMC_ROM_MC_DATA_SIZE    (4u)
#define RPMC_ROM_RK_DATA_SIZE    (RPMC_ROM_SHA_MAX_SIZE)
#define RPMC_ROM_BLOCK_SIZE      (0x1000u)
#define RPMC_ROM_UNIQUE_ID_SIZE  (16u)
#define RPMC_ROM_ACCESS_SIZE     (64u)
#define RPMC_ROM_ALW_DIRECT_RD   (0) // 0: use cache (default), 1: always direct read from flash (low performance)

#define RPMC_ROM_OP_READ   true
#define RPMC_ROM_OP_WRITE  false

#define RPMC_ROM_BUS_SPI    (2u)
#define RPMC_ROM_BUS_ESPI   (1u)
#define RPMC_ROM_BUS_CACHE  (0u)

#define RPMC_ROM_WORK_POST_TIME_EVT  (1)
#define RPMC_ROM_WORK_POST_DELAY_MS  (3000)

#ifndef RPMC_ROM_VERSION
#define RPMC_ROM_VERSION  (1)
#endif

#ifndef RPMC_ROM_BASE_ADDR
#define RPMC_ROM_BASE_ADDR  (0x80000)
#endif

#ifndef RPMC_ROM_MC_NUM
#define RPMC_ROM_MC_NUM  (8)
#endif

struct param_data {
	uint8_t value[RPMC_ROM_UNIQUE_ID_SIZE];
	uint8_t rsvd[12];
	uint16_t version;
	uint16_t chksum;
} __packed;
BUILD_ASSERT(sizeof(struct param_data) == 32, "The param data structure length is invalid!");

struct mc_data {
	uint32_t value;
	uint16_t cycle;
	uint16_t chksum;
} __packed;
BUILD_ASSERT(sizeof(struct mc_data) == 8, "The MC data structure length is invalid!");

struct rt_data {
	uint8_t value[48];
	uint8_t rsvd[12];
	uint16_t cycle;
	uint16_t chksum;
} __packed;
BUILD_ASSERT(sizeof(struct rt_data) == 64, "The RT data structure length is invalid!");

struct post_work {
	uint32_t erase_addr[RPMC_ROM_MC_NUM];
};

struct spi_rom_data {
    uint8_t __aligned(4) rootk[RPMC_ROM_RK_DATA_SIZE];
	uint32_t rt_addr;
	uint16_t rt_cycle;

    uint32_t __aligned(4) u32_ct;
	uint32_t ct_addr;
	uint16_t ct_cycle;

	uint8_t unique_id[RPMC_ROM_UNIQUE_ID_SIZE];

	bool inited;
};

struct spi_rom_dev_context {
	const struct device *espi_dev;
	
	struct k_timer rom_timer;
	struct k_work rom_work;

	struct post_work post_work;

	struct spi_rom_data data[RPMC_ROM_MC_NUM];
};

/**
 * rom_rpmc_data_init
 *
 * @param [in] num:   Device number.
 * @param [in] p_kdv_root: The default root key data.
 * @param [in] len: 	   The default root key data length.
 *
 * @return value The result of rpmc rom data init operation.
 */
int rom_rpmc_data_init(uint8_t num);

/**
 * rom_unique_id_read
 *
 * @param [in] inst:  Device instance.
 * @param [out] pData: Pointer to buffer for unique ID.
 * @param [in] len:   Length of the buffer.
 *
 * @return value The result of unique ID read operation.
 */
int rom_unique_id_read(uint8_t inst, uint8_t* pData, uint8_t len);

/**
 * rom_rpmc_mc_access
 *
 * @param [in] instance:  Device instance.
 * @param [in/out] pData: Read or write buffer pointer.
 * @param [in] len: 	  Read or write buffer length.
 * @param [in] read:      Access type, 1: read, 0: write.
 * @param [in] bus:       Access bus, 1: SPI bus, 0: eSPI bus.
 *
 * @return value The result of monotonic counter access operation.
 */
int rom_rpmc_mc_access(uint8_t instance, uint8_t *pData, uint8_t len, bool read, uint8_t bus);

/**
 * rom_rpmc_rt_access
 *
 * @param [in] instance:  Device instance.
 * @param [in/out] pData: Read or write buffer pointer.
 * @param [in] len: 	  Read or write buffer length.
 * @param [in] read:      Access type, 1: read, 0: write.
 * @param [in] bus:       Access bus, 1: SPI bus, 0: eSPI bus.
 *
 * @return value The result of root key access operation.
 */
int rom_rpmc_rt_access(uint8_t instance, uint8_t *pData, uint8_t len, bool read, uint8_t bus);

#endif // end of __F_RPMC_ROM_H__

