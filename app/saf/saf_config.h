/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#ifndef __SAF_CONFIG_H__
#define __SAF_CONFIG_H__

#include <soc.h>
#include <zephyr/drivers/espi_saf.h>
#include "board_config.h"

#if CONFIG_SOC_FAMILY_MEC
#define ESPI_SAF_0 DT_LABEL(DT_NODELABEL(espi_saf0))
#else
#define ESPI_SAF_0	DT_NODELABEL(espi_taf)
#endif

/* AMD SAF MAP
*
* TAG: 0H ........... MASTER: 0 fully access
* TAG: 4H ........... MASTER: 1 low security
* TAG: 5H ........... MASTER: 2 medium security
* TAG: 6H ........... MASTER: 3 high security
* TAG: other ........ MASTER: 0 fully access by default
*/

enum {
	TAG_0 = 0,
	TAG_1,
	TAG_2,
	TAG_3,
	TAG_4,
	TAG_5,
	TAG_6,
	TAG_7,
	TAG_8,
	TAG_9,
	TAG_10,
	TAG_11,
	TAG_12,
	TAG_13,
	TAG_14,
	TAG_15,
};

/* Security bit definitions for SAF protection regions */
#define AMD_SAF_PR_BOOT_SECURITY_BIT		(TAG_0) /* RW - Boot security region */

#define AMD_SAF_PR_LOW_SECURITY_BIT			(TAG_4) /* RW - Low security region */
#define AMD_SAF_PR_MEDIUM_SECURITY_BIT		(TAG_5) /* RO - Medium security region */
#define AMD_SAF_PR_HIGH_SECURITY_BIT		(TAG_6) /* * - High security region */

/* Override bits: Allow access for tags 0-15 and boot security */
#define OVERRIDE_BITS						(BIT_MASK(16) | BIT(AMD_SAF_PR_BOOT_SECURITY_BIT))
/* Protect bits: Only allow boot security tag access */
#define PROTECT_BITS						(0x0 | BIT(AMD_SAF_PR_BOOT_SECURITY_BIT))

#ifdef SAF_PR_OUTOF_REGION_EN
/* Tags outside AMD spec cannot access protection regions regardless of tag0 */
#define AMD_INIT_BITS						(PROTECT_BITS)
#else
/* Tags outside AMD spec maintain fully access state in protection region */
#define AMD_INIT_BITS						(OVERRIDE_BITS)
#endif

#define OVERRIDE_BIT(bit)					((AMD_INIT_BITS) | BIT(bit))
#define PROTECT_BIT(bit)					((AMD_INIT_BITS) & ~BIT(bit))

/* Full access permissions for read and write operations */
#define SAF_PR_FULLY_ACCESS_R				OVERRIDE_BITS
#define SAF_PR_FULLY_ACCESS_W				OVERRIDE_BITS

/* Low security level override permissions */
#define AMD_SAF_PR_LOW_OVERRIDE_R			OVERRIDE_BIT(AMD_SAF_PR_LOW_SECURITY_BIT)
#define AMD_SAF_PR_LOW_OVERRIDE_W			OVERRIDE_BIT(AMD_SAF_PR_LOW_SECURITY_BIT)
/* Medium security level permissions (read allowed, write protected) */
#define AMD_SAF_PR_MEDIUM_OVERRIDE_R		OVERRIDE_BIT(AMD_SAF_PR_MEDIUM_SECURITY_BIT)
#define AMD_SAF_PR_MEDIUM_OVERRIDE_W		PROTECT_BIT(AMD_SAF_PR_MEDIUM_SECURITY_BIT)
/* High security level permissions (both read and write protected) */
#define AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_R	PROTECT_BIT(AMD_SAF_PR_HIGH_SECURITY_BIT)
#define AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_W	PROTECT_BIT(AMD_SAF_PR_HIGH_SECURITY_BIT)

/* Memory size conversion macros */
#define MEM_B(b)   							(b)                    /* Bytes */
#define MEM_KB(kb) 							((kb) * 1024UL)        /* Kilobytes */
#define MEM_MB(mb) 							((mb) * 1024UL * 1024UL) /* Megabytes */

#define BM_ENABLED							(1)
#define BM_DISABLED							(0)
/* Protection region flag definitions */
#define FLAG_PROTECT						BIT(0)  /* Enable protection */
#define FLAG_LOCK							BIT(1)  /* Lock configuration */

#define AMD_BM_INIT							(BM_ENABLED) /* Don't modify it */

/* SAF region configuration constants */
#define SAF_REGION_NUM					(10)    /* Number of SAF regions */
#define SAF_ADDR_SHIFT					(12)    /* Address shift for 4KB alignment */

/* AMD access control masks and values */
#define SAF_AMD_ACCESS_MSK				(BIT_MASK(2))        /* 2-bit access mask */
#define SAF_AMD_ACCESS_FULLY			(0u)                 /* Full access */
#define SAF_AMD_ACCESS_LOW_SECURITY		(1u)                 /* Low security */
#define SAF_AMD_ACCESS_MEDIUM_SECURITY	(2U)                 /* Medium security */
#define SAF_AMD_ACCESS_HIGH_SECURITY	(3U)                 /* High security */

/* AMD control flags masks and values */
#define SAF_AMD_CTRL_MSK				(BIT_MASK(2))        /* 2-bit control mask */
#define SAF_AMD_CTRL_SECURITY_DISABLE	(0u)                 /* Security disabled */
#define SAF_AMD_CTRL_SECURITY_ENABLE	(BIT(0))            /* Security enabled */
#define SAF_AMD_CTRL_SECURITY_LOCK		(BIT(1))            /* Security locked */

/* SAF memory map structure - 4 bytes per entry */
struct saf_map {
	uint16_t start;     /* Start address (in 4KB units) */
	uint16_t size;      /* Size (in 4KB units) */
} __packed;
BUILD_ASSERT(sizeof(struct saf_map) == 4, "Invalid saf_map structure!");

/* Region settings structure - 48 bytes total */
struct region_setting {
	struct saf_map item[SAF_REGION_NUM];    /* 10 regions * 4 bytes = 40 bytes */
	uint32_t access;                        /* Access control bits (2 bits per region) */
	uint32_t control;                       /* Control flags (2 bits per region) */
} __packed;
BUILD_ASSERT(sizeof(struct region_setting) == 48, "Invalid region_setting structure!");

/* SAF region buffer union for ACPI access */
struct saf_region {
	union {
		uint8_t buffer[(SAF_REGION_NUM + 2) *4];    /* Raw byte buffer */
		struct region_setting setting;              /* Structured access */
	};
} __packed;
BUILD_ASSERT(sizeof(struct saf_region) == 48, "Invalid saf_region structure!");

/* Extended protection structure for read/write operations */
struct espi_saf_protection_cu {
	size_t nregions;                        /* Number of regions */
	struct espi_saf_pr *pregions;          /* Pointer to region array */
};

/**
 * @brief Performs eSPI controller initialization.
 *
 * Note: This operation should complete before eSPI flash channel negotiation
 * is started to avoid cases where eSPI master attempts flash operations
 * before SAF block is ready.
 *
 * It's recomended to perform this operation before RSMRST is de-asserted.
 *
 * @retval -EIO General input / output error, failed to configure device.
 * @retval -EINVAL invalid capabilities, failed to configure device.
 * @retval -ENOTSUP capability not supported by eSPI slave.
 * @retval 0 if success.
 */

int initialize_saf_bridge(void);

/**
 * @brief Sends a write request packet for slave attached flash.
 *
 * This routines provides an interface to send a request to write to the flash
 * components shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Address of the representation of write flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int saf_write_flash(struct espi_saf_packet *pckt);
/**
 * @brief Sends a read request packet for slave attached flash.
 *
 * This routines provides an interface to send a request to write to the flash
 * components shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Address of the representation of write flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int saf_read_flash(struct espi_saf_packet *pckt);

/**
 * @brief Sends an erase request packet for slave attached flash.
 *
 * This routines provides an interface to send a request to erase a page in
 * the flash components shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Address of the representation of erase flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int saf_erase_flash(struct espi_saf_packet *pckt);

#endif /* __SAF_CONFIG_H__ */
