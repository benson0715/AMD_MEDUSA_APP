/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#ifndef __SYSTEM_H__
#define __SYSTEM_H__

/**
 * @brief System power states as defined in ACPI spec chapter 2-2.
 */
#include "zephyr/toolchain/gcc.h"
enum system_power_state {
	/* Initial system status after system power on */
	SYSTEM_INIT_STATE,
	/* Mechanical off. System is completely off and consumes no power */
	SYSTEM_G3_STATE,
	/* Pre-working state. State before system is fully active */
	SYSTEM_S0_R_STATE,
	/* Working state. System is fully active */
	SYSTEM_S0_STATE,
	/* Sleep. System consumes a small amount of power, nothing is executing
	 * Latency to go back to working state is minimal without reboot system
	 */
	SYSTEM_S3_STATE,
	/* Non-volatile sleep. This special state allows system context to be
	 * saved and restored (relatively slowly) when power is lost
	 */
	SYSTEM_S4_STATE,
	/* Soft off. System consumes a minimal amount of power.
	 * Requires a large latency in order to return to working state, usually
	 * requires a complete re-initialization
	 * The system context will not be preserved by the hardware
	 */
	SYSTEM_S5_STATE,
};

enum system_power_seq_stage {
	/* power on initial status */
	POWER_INIT = 0,  // 0
	SETUP_GPIO_DEFAULT,
	ENTER_TO_G3, 

	FROM_G3_TO_S5,  // 3
	FROM_G3_TO_S5_MID,
	PWRGD_ASSERTED,
	RELEASE_RSMRST,

	FROM_G3_TO_S0,  // 7       
	FROM_G3_TO_S0_MID,
	PWRGD_ASSERTED_IN_DC_MODE,
	RELEASE_RSMRST_IN_DC_MODE,

	FROM_S5_TO_G3,  // 11 
	FROM_S5_TO_G3_Part2,
	FROM_S5_TO_G3_Part3,

	FROM_S5S3_TO_S0,  // 14
	PWR_BTNn_EC_ASSERTED,
	FROM_S3_TO_S0,
	FROM_S3_TO_S0_15ms,
	TRANSFERED_INTO_S0,

	FROM_S0_TO_S3S5,  // 19
	FROM_S0_TO_S5,

	FROM_S0_TO_S5_MEM_PMIC, // 21
	GOING_TO_S5,

	FORCE_RESET,  // 23
	FORCE_RESET_Part1,
	FORCE_RESET_Part2,
	FORCE_RESET_G3_TO_S5,
	FORCE_RESET_G3_TO_S5_MID,
	FORCE_RESET_PWRGD_ASSERTED,

	FORCE_G3,  // 29
	FORCE_G3_Part1,
	FORCE_G3_Part2,
	FORCE_G3_Part3,

	SEQ_NUM_MAX, // 33

	STATE_NO_CHANGE = 0xff,
};
BUILD_ASSERT(SEQ_NUM_MAX == 33, "Sanity check failed: SEQ_NUM_MAX is not 33");

enum boot_config_mode {
	/* SPI sharing. FW is obtained directly from SPI flash but EC HW
	 * may relinquishe its access to SPI flash after obtaining its FW.
	 * Other entities in the system obtain their FW from SPI flash.
	 */
	FLASH_BOOT_MODE_G3_SHARING = 0,
	/* Master-attached flash. Intel SoC is connected to SPI flash,
	 * EC HW is not connected to SPI flash, EC FW is obtained through
	 * eSPI bus.
	 */
	FLASH_BOOT_MODE_MAF,
	/* Slave-attached flash. EC HW is connected to SPI flash,
	 * EC has exclusive access to SPI flash.
	 * Other components obtain their FW via eSPI.
	 */
	FLASH_BOOT_MODE_SAF,
	/* EC HW has a dedicated SPI flash. */
	FLASH_BOOT_MODE_OWN,
};

#endif /* __SYSTEM_H__ */
