/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/espi_saf.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "saf_spi_transaction.h"
#include "saf_config.h"
#include "amd_crb_drivers_spiFlash.h"
#include "app_acpi.h"
#if CONFIG_PINCTRL
#include "gpio_ec.h"
#include <zephyr/drivers/pinctrl.h>
#endif

LOG_MODULE_REGISTER(saf_config, CONFIG_ESPIHUB_LOG_LEVEL);

static const struct device *saf_dev;
static const struct espi_saf_cfg saf_config = {
	.nflash_devices = 1u,
};

/* Default SAF protection regions configuration */
static struct espi_saf_pr amd_security_protect_regions[4] = {
	{
	    /* Low security region: [32MB, 33MB) */
		.start = MEM_MB(32),
		.end =  MEM_MB(32) + MEM_MB(1) - 1,
		.master_bm_we = AMD_BM_INIT,
		.master_bm_rd = AMD_BM_INIT,
		.override_r = AMD_SAF_PR_LOW_OVERRIDE_R,
		.override_w = AMD_SAF_PR_LOW_OVERRIDE_W,
		.pr_num = 1U,                          /* Protection region number */
		// .flags = FLAG_PROTECT | FLAG_LOCK,  /* Commented out for development */
		.flags = 0,
	},
	{
	    /* Medium security region: [33MB, 34MB) */
		.start = MEM_MB(33),
		.end =  MEM_MB(33) + MEM_MB(1) - 1,
		.master_bm_we = AMD_BM_INIT,
		.master_bm_rd = AMD_BM_INIT,
		.override_r = AMD_SAF_PR_MEDIUM_OVERRIDE_R,
		.override_w = AMD_SAF_PR_MEDIUM_OVERRIDE_W,
		.pr_num = 2U,
		// .flags = FLAG_PROTECT | FLAG_LOCK,
		.flags = 0,
	},
	{
	    /* High security region: [34MB, 35MB) */
		.start = MEM_MB(34),
		.end =  MEM_MB(34) + MEM_MB(1) - 1,
		.master_bm_we = AMD_BM_INIT,
		.master_bm_rd = AMD_BM_INIT,
		.override_r = AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_R,
		.override_w = AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_W,
		.pr_num = 3U,
		// .flags = FLAG_PROTECT | FLAG_LOCK,
		.flags = 0,
	},
	{
	    /* Fully accessible region: [0MB, 32MB) */
		.start = MEM_MB(0),
		.end =  MEM_MB(32) - 1,
		.master_bm_we = AMD_BM_INIT,
		.master_bm_rd = AMD_BM_INIT,
		.override_r = SAF_PR_FULLY_ACCESS_R,
		.override_w = SAF_PR_FULLY_ACCESS_W,
		.pr_num = 4U,
		// .flags = FLAG_PROTECT | FLAG_LOCK,
		.flags = 0,
	},
};

/* Default protection regions wrapper */
static struct espi_saf_protection saf_pr_regions = {
	.nregions = 4U,                          /* Number of protection regions */
	.pregions = amd_security_protect_regions,
};

/* Pointer to dynamically registered protection regions */
static struct espi_saf_protection *saf_pr_regions_ptr = NULL;

/* ACPI region storage arrays */
static struct espi_saf_pr g_saf_acpi_protect_regions_a[SAF_REGION_NUM] = {0};
static struct espi_saf_pr g_saf_acpi_protect_regions_b[SAF_REGION_NUM] = {0};

/* ACPI protection structures */
static struct espi_saf_protection_cu saf_pr_acpi_a = {
	.nregions = SAF_REGION_NUM,
	.pregions = g_saf_acpi_protect_regions_a
};

static struct espi_saf_protection_cu saf_pr_acpi_b = {
	.nregions = CONFIG_ESPI_TAF_PR_NUM - SAF_REGION_NUM,    /* Remaining regions */
	.pregions = g_saf_acpi_protect_regions_b
};

/* ACPI region buffers */
static struct saf_region g_saf_acpi_region_a = {0};
static struct saf_region g_saf_acpi_region_b = {0};

/**
 * @brief Configure SAF protection region based on access level
 * 
 * @param pregions Pointer to protection region structure
 * @param access Access level (SAF_AMD_ACCESS_*)
 */
static inline void configure_region_access(struct espi_saf_pr *pregions, uint8_t access)
{
	/* Set bus master permissions */
	pregions->master_bm_rd = AMD_BM_INIT;
	pregions->master_bm_we = AMD_BM_INIT;

	/* Configure override permissions based on access level */
	switch (access) {
	case SAF_AMD_ACCESS_FULLY:
		pregions->override_r = SAF_PR_FULLY_ACCESS_R;
		pregions->override_w = SAF_PR_FULLY_ACCESS_W;
		break;
	case SAF_AMD_ACCESS_LOW_SECURITY:
		pregions->override_r = AMD_SAF_PR_LOW_OVERRIDE_R;
		pregions->override_w = AMD_SAF_PR_LOW_OVERRIDE_W;
		break;
	case SAF_AMD_ACCESS_MEDIUM_SECURITY:
		pregions->override_r = AMD_SAF_PR_MEDIUM_OVERRIDE_R;
		pregions->override_w = AMD_SAF_PR_MEDIUM_OVERRIDE_W;
		break;
	case SAF_AMD_ACCESS_HIGH_SECURITY:
		pregions->override_r = AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_R;
		pregions->override_w = AMD_SAF_PR_HIGH_SECURITY_OVERRIDE_W;
		break;
	default:
		/* Default to fully accessible if invalid access level */
		pregions->override_r = SAF_PR_FULLY_ACCESS_R;
		pregions->override_w = SAF_PR_FULLY_ACCESS_W;
		break;
	}
}

/**
 * @brief Process SAF region configuration from ACPI data
 * 
 * @param setting Pointer to region setting structure
 * @param pregions Pointer to protection regions array
 * @param region_offset Offset to add to region numbers
 * @return 0 on success, negative error code on failure
 */
static int process_saf_regions(const struct region_setting *setting, 
                               struct espi_saf_pr *pregions, 
                               uint8_t region_offset)
{
	if (!setting || !pregions) {
		return -EINVAL;
	}

	for (uint8_t i = 0; i < SAF_REGION_NUM; i++) {
		struct espi_saf_pr *pregion = &pregions[i];
		
		/* Extract start address and size from ACPI data */
		uint32_t start = setting->item[i].start;
		uint32_t size = setting->item[i].size;

		/* Validate region parameters */
		if (size == 0) {
			/* Skip empty regions */
			continue;
		}

		/* Convert from 4KB units to byte addresses */
		pregion->start = start << SAF_ADDR_SHIFT;
		pregion->end = pregion->start + (size << SAF_ADDR_SHIFT) - 1;
		
		/* Extract access control for this region (2 bits per region) */
		uint8_t access = (setting->access >> (i * 2)) & SAF_AMD_ACCESS_MSK;
		configure_region_access(pregion, access);
		
		/* Extract control flags for this region (2 bits per region) */
		uint8_t control = (setting->control >> (i * 2)) & SAF_AMD_ACCESS_MSK;
		
		/* Configure protection flags */
		pregion->flags = 0;
		if (control & SAF_AMD_CTRL_SECURITY_ENABLE) {
			pregion->flags |= FLAG_PROTECT;
		}
		if (control & SAF_AMD_CTRL_SECURITY_LOCK) {
			pregion->flags |= FLAG_LOCK;
		}
		
		/* Set protection region number */
		pregion->pr_num = i + region_offset;
	}
	
	return 0;
}

/**
 * @brief ACPI handler for SAF configuration region A
 * 
 * This function handles read/write operations for the SAF ACPI region A.
 * When a complete region is written (last byte), it configures the SAF
 * protection regions based on the ACPI data.
 * 
 * @param isRead 1 for read operation, 0 for write operation
 * @param ui8Idx Index of the byte to read/write in the ACPI region
 * @param pui8Data Pointer to the data byte to read/write
 * @return 0 on success
 */
uint8_t app_saf_cfg_a_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	/* Check if index is within valid range */
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET || !pui8Data) {
		return 0;
	}

	if (isRead) {
		/* Read operation: return byte from buffer */
		*pui8Data = g_saf_acpi_region_a.buffer[ui8Idx];
	} else {
		/* Write operation: store byte in buffer */
		g_saf_acpi_region_a.buffer[ui8Idx] = *pui8Data;

		/* Check if this is the last byte write - triggers region configuration */
		if (ui8Idx == (sizeof(struct saf_region) - 1u)) {
			/* Process all SAF regions using helper function */
			int ret = process_saf_regions(&g_saf_acpi_region_a.setting, saf_pr_acpi_a.pregions, 1);
			if (ret) {
				LOG_ERR("Failed to process SAF regions A: %d", ret);
				return 0;
			}

			/* Apply the configured protection regions to hardware */
			ret = espi_saf_set_protection_regions(saf_dev, (const struct espi_saf_protection*)&saf_pr_acpi_a);
			if (ret) {
				LOG_ERR("Failed to set SAF protection region(s) A: %d", ret);
			} else {
				LOG_DBG("eSPI SAF protection regions A configured!");
			}
		}
	}

	return 0;
}

/**
 * @brief ACPI handler for SAF configuration region B
 * 
 * This function handles read/write operations for the SAF ACPI region B.
 * When a complete region is written (last byte), it configures the SAF
 * protection regions based on the ACPI data.
 * 
 * @param isRead 1 for read operation, 0 for write operation
 * @param ui8Idx Index of the byte to read/write in the ACPI region
 * @param pui8Data Pointer to the data byte to read/write
 * @return 0 on success
 */
uint8_t app_saf_cfg_b_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	/* Check if index is within valid range */
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET || !pui8Data) {
		return 0;
	}

	if (isRead) {
		/* Read operation: return byte from buffer */
		*pui8Data = g_saf_acpi_region_b.buffer[ui8Idx];
	} else {
		/* Write operation: store byte in buffer */
		g_saf_acpi_region_b.buffer[ui8Idx] = *pui8Data;

		/* Check if this is the last byte write - triggers region configuration */
		if (ui8Idx == (sizeof(struct saf_region) - 1u)) {
			/* Process all SAF regions using helper function */
			int ret = process_saf_regions(&g_saf_acpi_region_b.setting, saf_pr_acpi_b.pregions, SAF_REGION_NUM + 1);
			if (ret) {
				LOG_ERR("Failed to process SAF regions B: %d", ret);
				return 0;
			}

			/* Apply the configured protection regions to hardware */
			ret = espi_saf_set_protection_regions(saf_dev, (const struct espi_saf_protection*)&saf_pr_acpi_b);
			if (ret) {
				LOG_ERR("Failed to set SAF protection region(s) B: %d", ret);
			} else {
				LOG_DBG("eSPI SAF protection regions B configured!");
			}
		}
	}

	return 0;
}

/**
 * @brief Register custom SAF protection regions
 * 
 * This function allows external modules to register custom SAF protection
 * regions that will be used instead of the default regions.
 * 
 * @param pregions Pointer to the protection regions structure
 */
void espi_saf_pr_regions_register(struct espi_saf_protection *pregions)
{
	saf_pr_regions_ptr = pregions;
}

/**
 * @brief Get the registered SAF protection regions pointer
 * 
 * This function returns the pointer to the currently registered SAF
 * protection regions.
 * 
 * @return Pointer to the registered protection regions, or NULL if none
 */
static struct espi_saf_protection* espi_saf_pr_regions_ptr_get(void)
{
	return saf_pr_regions_ptr;
}

/**
 * @brief Initialize SAF protection regions
 * 
 * This function registers the ACPI handlers for SAF configuration and
 * sets up the initial protection regions. It uses either custom registered
 * regions or the default regions.
 * 
 * @return 0 on success, negative error code on failure
 */
static int espi_saf_pr_init(void)
{
	md_acpiplanes_register_fun(app_saf_cfg_a_acpiHandler, 0xD8);
	md_acpiplanes_register_fun(app_saf_cfg_b_acpiHandler, 0xD9);

	struct espi_saf_protection* pregions = espi_saf_pr_regions_ptr_get();
	if (!pregions) {
		pregions = &saf_pr_regions;
	}

	int ret = espi_saf_set_protection_regions(saf_dev, pregions);
	if (ret) {
		LOG_ERR("SAF set protection regions failed: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Initialize the SAF bridge
 * 
 * This function initializes the eSPI SAF device, configures it,
 * sets up protection regions, and activates the SAF channel.
 * 
 * @return 0 on success, negative error code on failure
 */
int initialize_saf_bridge(void)
{
	int ret;
	bool saf_ready;

	saf_dev = DEVICE_DT_GET(ESPI_SAF_0);
	if (!device_is_ready(saf_dev)) {
		LOG_ERR("eSPI SAF not ready");
		return -ENODEV;
	}

	ret = espi_saf_config(saf_dev, &saf_config);
	if (ret) {
		LOG_ERR("SAF configuration failed: %d", ret);
		return ret;
	}

	ret = espi_saf_pr_init();
	if (ret) {
		LOG_ERR("SAF protection failed: %d", ret);
		return ret;
	}

	espi_saf_activate(saf_dev);

	LOG_DBG("Check if SAF channel is getting disabled");
	saf_ready = espi_saf_get_channel_status(saf_dev);
	if (!saf_ready) {
		LOG_ERR("SAF channel not ready");
		return -EIO;
	}
	

	LOG_DBG("SAF channel ready");

	return 0;
}

/**
 * @brief Write data to SAF flash
 * 
 * @param pckt Pointer to the SAF packet containing write parameters
 * @return 0 on success, negative error code on failure
 */
int saf_write_flash(struct espi_saf_packet *pckt)
{
	return espi_saf_flash_write(saf_dev, pckt);
}

/**
 * @brief Read data from SAF flash
 * 
 * @param pckt Pointer to the SAF packet containing read parameters
 * @return 0 on success, negative error code on failure
 */
int saf_read_flash(struct espi_saf_packet *pckt)
{
	return espi_saf_flash_read(saf_dev, pckt);
}

/**
 * @brief Erase SAF flash sector
 * 
 * @param pckt Pointer to the SAF packet containing erase parameters
 * @return 0 on success, negative error code on failure
 */
int saf_erase_flash(struct espi_saf_packet *pckt)
{
	return espi_saf_flash_erase(saf_dev, pckt);
}