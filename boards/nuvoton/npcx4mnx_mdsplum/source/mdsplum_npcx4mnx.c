/*****************************************************************************
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 ******************************************************************************
 */

#include <soc.h>
#include <zephyr/drivers/gpio.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "espi_hub.h"
#include "board_id.h"
#include "board_config.h"
#include "board_sys_config.h"
#include "f_nv_options.h"
#include "f_romSig.h"
#include "mdsplum_npcx4mnx.h"
#include "amd_crb_drivers_spiFlash.h"
#include "acpiplanes.h"
#include "app_acpi.h"
#if CONFIG_WIRELESS_MANAGEABILITY
#include "app_manOs.h"
#endif
#include "postcode_led_hub.h"
#include "soc.h"

LOG_MODULE_DECLARE(board, CONFIG_BOARD_LOG_LEVEL);

#define VDD_MEM_ADDR                       (0x30)        // For Phoenix/Phoenix2 FP7/FP7r2
#define GPIO_PIN_NULL                      (2ul << 14)   // [15:14]
#define NPCX_SCFG_REG_ADDR DT_REG_ADDR_BY_NAME(DT_NODELABEL(scfg), scfg)

/* wwan and wlan D3 cold flag */
extern uint8_t g_wwanWlanD3Cold;
extern uint8_t g_auxD3Cold;



uint32_t _s_autogen_PorInitTable[] = {
    POR_INIT_ACPI,
    POR_INIT_BAT_CHG,
    POR_INIT_DBG,
    POR_INIT_I2C,
    POR_INIT_GPIO,
    // POR_INIT_JTAG,
    POR_INIT_Keyboard,
    POR_INIT_PWRSEQ,
    POR_INIT_SYS_BUS,
    POR_INIT_THERMAL,

    GPIO_PIN_NULL
};

uint32_t _s_Gpio_IdxOfUseWasSet[] = {
	EC_SLT_PWRBRK_N,
	EC_EVAL_19V_PWREN,

	GPIO_PIN_NULL
};

/**
 * @brief Disable board UART interface
 *
 * @param void
 * @return void
 */
void board_uart_disable(void)
{
	;//md_uart_disable(UART0);
}

/**
 * @brief Enable board UART interface
 *
 * @param void
 * @return void
 */
void board_uart_enable(void)
{
	;//md_uart_enable(UART0, MD_UART_EM_BAUDRATE__115200, MD_UART_EM_BAUDRATE__8, MD_UART_EM_STOPBITS__1, false);
}

/**
 * @brief Initialize the board hardware
 *
 * @param void
 * @return 0 on success, negative error code on failure
 */
int board_init(void)
{
	int ret;

	/**
	 * Get Sys Config from rom 
	 */
	ret = brdSysConfig_Init();
	if (ret) {
		LOG_ERR("Failed to get Sys Config from rom: %d", ret);
		return ret;
	}

	/**
	 * Get binding with GPIO driver
	 */
	ret = gpio_init();
	if (ret) {
		LOG_ERR("Failed to initialize gpio devs: %d", ret);
		return ret;
	}

	/**
	 * Initialize EC GPIO
	 */
	ret = __autoGen_initECGPIO();
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
	}

	/**
	 * Initialize I2C BUS
	 */
	ret = i2c_hub_config(I2C_0);
	if (ret) {
		return ret;
	}

	ret = i2c_hub_config(I2C_1);
	if (ret) {
		return ret;
	}

	ret = i2c_hub_config(I2C_2);
	if (ret) {
		return ret;
	}

	ret = i2c_hub_config(I2C_3);
	if (ret) {
		return ret;
	}

	/**
	 * Initialize SPI Flash
	 */
	ret = amd_crb_drivers_sFlashInit();
	if (ret) {
		return ret;
	}

	/**
	 * Detect boot mode
	 */
	detect_boot_mode();

	/**
	 * Initialize the platform
	 */
	board_init_platformInit();

	k_msleep(100);

	return 0;
}

/**
 * @brief Suspend the board to low power state
 *
 * @param void
 * @return 0 on success, negative error code on failure
 */
int board_suspend(void)
{

	return 0;
}

/**
 * @brief Resume the board from low power state
 *
 * @param void
 * @return 0 on success, negative error code on failure
 */
int board_resume(void)
{

	return 0;
}

/**
 * This is run from APU_PWROK_SVID ISR context
 */
void board_onApuPwrOK(void)
{
	uint32_t pinList[] = {
		PIN_LIST_DRIVER_TO_HIGH_AFTER_APU_PWROK,
	};

	/* power on and set the APU_PWRON pins */
	for (uint8_t i = 0; !GPIO_isNullPin(pinList[i]); i++) {
		/* need to bypass the force high PCIE AUX reset for D3 cold */
		if (pinList[i] == EC_WWAN_AUX_RST_N) {
			if (g_auxD3Cold & 0x20) {
				gpio_write_pin(pinList[i], 0);
			} else {
				gpio_write_pin(pinList[i], 1);
			}
		} else if (pinList[i] == EC_WLAN_AUX_RST_N) {
			if (g_auxD3Cold & 0x40) {
				gpio_write_pin(pinList[i], 0);
			} else {
				gpio_write_pin(pinList[i], 1);
			}
		} else if (pinList[i] == EC_LOM_AUX_RST_N) {
			if (g_auxD3Cold & 0x80) {
				gpio_write_pin(pinList[i], 0);
			} else {
				gpio_write_pin(pinList[i], 1);
			}
		} else if (pinList[i] == EC_SD_AUX_RST_N) {
			if (g_auxD3Cold & 0x01) {
				gpio_write_pin(pinList[i], 0);
			} else {
				gpio_write_pin(pinList[i], 1);
			}
		} else if (pinList[i] == EC_EVAL_AUX_RST_N) {
			if (g_auxD3Cold & 0x02) {
				gpio_write_pin(pinList[i], 0);
			} else {
				gpio_write_pin(pinList[i], 1);
			}
		} else {
			gpio_write_pin(pinList[i], 1);
		}

		gpio_set_power(pinList[i], GPIO_CTRL_PWRG_VTR_IO);
	}
	
	uint32_t eSPIPins[] = {
		PIN_LIST_ESPI_SIGNALES_TO_PWRON,
	};
	
	/* power on and set the ESPI pins */
	for (uint8_t i = 0; !GPIO_isNullPin(eSPIPins[i]); i++) {
		gpio_set_power(eSPIPins[i], GPIO_CTRL_PWRG_VTR_IO);
	}

	/* set the PCIE AUX reset */
    if (g_auxD3Cold & 0x20) {
        gpio_write_pin(EC_WWAN_AUX_RST_N, 0);
        LOG_DBG("WWAN: %d", gpio_read_pin( EC_WWAN_AUX_RST_N ));
    }
	if (g_auxD3Cold & 0x40) {
		gpio_write_pin(EC_WLAN_AUX_RST_N, 0);
		LOG_DBG("WLAN: %d", gpio_read_pin( EC_WLAN_AUX_RST_N ));
	}
	if (g_auxD3Cold & 0x80) { 
		gpio_write_pin( EC_LOM_AUX_RST_N , 0);
		LOG_DBG("LOM: %d", gpio_read_pin( EC_LOM_AUX_RST_N ));
	}
	if (g_auxD3Cold & 0x01) { 
		gpio_write_pin( EC_SD_AUX_RST_N , 0);
		LOG_DBG("SD: %d", gpio_read_pin( EC_SD_AUX_RST_N ));
	}
	if (g_auxD3Cold & 0x02) {
		gpio_write_pin( EC_EVAL_AUX_RST_N , 0);
		LOG_DBG("dGPU: %d", gpio_read_pin( EC_EVAL_AUX_RST_N ));
	}
}

/**
 * @brief Initialize eval card control pins to low state
 *
 * PLAT-84470
 * Init all of the pin status to 0 on APU_RST# but keep them untouched across S3/S0i3
 *
 * @param void
 * @return void
 */
void board_evalCardCtrlPins(void)
{
	uint32_t pinList[] = {
		PIN_LIST_DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH,
	};

	for (uint8_t i = 0; !GPIO_isNullPin(pinList[i]); i++) {
		gpio_write_pin(pinList[i], 0);
		gpio_set_power(pinList[i], GPIO_CTRL_PWRG_VTR_IO);
	}
}

/**
 * @brief Turn off the JTAG interface to prevent current leakage
 *
 * @param void
 * @return void
 */
void board_turnOffJtagInterface (void)
{
	// *(unsigned char *)(NPCX_SCFG_REG_ADDR + 0x120) = 0x6; // Disabed Jtag;
	// *(unsigned char *)(NPCX_SCFG_REG_ADDR + 0x121) = 0x6;
}

/**
 * @brief Turn on eSPI interface and configure related pins
 *
 * @param void
 * @return void
 */
void board_turnOn_espi(void)
{
	if (espihub_get_saf_boot_mode() == true) {
		uint32_t eSPIPins[] = {
			PIN_LIST_ESPI_SIGNALES_TO_PWRON,
		};

		for (uint8_t i = 0; !GPIO_isNullPin(eSPIPins[i]); i++) {
			gpio_set_power(eSPIPins[i], GPIO_CTRL_PWRG_VTR_IO);
		}
	}
}