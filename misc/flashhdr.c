/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "board_config.h"
#include "flashhdr.h"

#define KSC_MAJOR_VER     1
#define KSC_MINOR_VER     65
#define KSC_PATCH_ID      0
#define KSC_QS_BUILD_VER  0

__in_section(ecfw_info, static, var) struct ksc_img_hdr header = {
	/* This is replaced by real checksum in build. */
	.checksum = 0x0000,
	.signature = "TKSC",
	/* version info */
	.version = {KSC_MAJOR_VER, KSC_MINOR_VER, KSC_PATCH_ID, KSC_QS_BUILD_VER },
	.copyright = "Copyright (c) 2019 Intel Corporation All Rights Reserved",
	/* image size*/
	.img_size = 0x00000000,
	.platform_str = KSC_PLAT_NAME,
	.platform_id = { 1 },
};

/**
 * @brief Get the major version number of the firmware image.
 *
 * @return Major version number.
 */
inline uint8_t major_version(void)
{
	return header.version[0];
}

/**
 * @brief Get the minor version number of the firmware image.
 *
 * @return Minor version number.
 */
inline uint8_t minor_version(void)
{
	return header.version[1];
}

/**
 * @brief Get the patch ID of the firmware image.
 *
 * @return Patch ID number.
 */
inline uint8_t patch_id(void)
{
	return header.version[2];
}

/**
 * @brief Get the QS build version of the firmware image.
 *
 * @return QS build version number.
 */
inline uint8_t qs_build_version(void)
{
	return header.version[3];
}