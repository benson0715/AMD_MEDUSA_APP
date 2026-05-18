/*****************************************************************************
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 ******************************************************************************
 */

#include <soc.h>
#include <zephyr/drivers/uart.h>
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
#include "mdsdorne_npcx4mnx.h"
#include "amd_crb_drivers_spiFlash.h"
#include "acpiplanes.h"
#include "app_acpi.h"
#include "app_manOs.h"
#include "postcode_led_hub.h"
#include "soc.h"

LOG_MODULE_DECLARE(board, CONFIG_BOARD_LOG_LEVEL);

#define VDD_MEM_ADDR                       (0x30)        // For Phoenix/Phoenix2 FP7/FP7r2
#define GPIO_PIN_NULL                      (2ul << 14)   // [15:14]
#define NPCX_SCFG_REG_ADDR DT_REG_ADDR_BY_NAME(DT_NODELABEL(scfg), scfg)

/* wwan and wlan D3 cold flag */
extern uint8_t g_wwanWlanD3Cold;
extern uint8_t g_auxD3Cold;

/**
 * @brief Power-on reset initialization table
 *
 * @note Contains list of initialization modules to be called during POR
 */
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

/**
 * @brief GPIO index table for pins that were set
 *
 * @note Contains list of GPIO pin indices that have been configured
 */
uint32_t _s_Gpio_IdxOfUseWasSet[] = {

	GPIO_PIN_NULL
};

/**
 * @brief Disable board UART
 *
 * @param void
 * @retval void
 */
void board_uart_disable(void)
{
	;//md_uart_disable(UART0);
}

/**
 * @brief Enable board UART
 *
 * @param void
 * @retval void
 */
void board_uart_enable(void)
{
	;//md_uart_enable(UART0, MD_UART_EM_BAUDRATE__115200, MD_UART_EM_BAUDRATE__8, MD_UART_EM_STOPBITS__1, false);
}

/**
 * @brief fake uart Rx callback, used to clear the hardware uart data register
 *
 * @param     dev:         Device tree
 * @param     user_data:   uart data buffer
 */
static void fake_uart_rx_callback(const struct device *dev, void *user_data)
{
	uint8_t recvData;
	ARG_UNUSED(user_data);

	/* Verify uart_irq_update() */
	if (!uart_irq_update(dev)) {
		LOG_ERR("retval should always be 1");
		return;
	}

	/* Verify uart_irq_rx_ready() */
	while (uart_irq_rx_ready(dev)) {
		uart_poll_in(dev, &recvData);
		uart_irq_update(dev);
	}
}

/**
 * @brief init the fake uart
 *
 * @note
 *  this function is used to clear the hardware uart data register, avoid the fake data block the nuvoton uart driver
 */
void fake_uart_init(void)
{
	const struct device *uartDev =  DEVICE_DT_GET(UART_0);
	if (!device_is_ready(uartDev)) {
		LOG_ERR("UART Device not ready");
	};

	uart_irq_callback_set(uartDev, fake_uart_rx_callback);
	uart_irq_rx_enable(uartDev);
}

int board_init(void)
{
	int ret;

#if CONFIG_LOG
	//board_uart_enable();
#else
	//board_uart_disable();
#endif

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

	/* init the fake uart to clear the hardware uart data register, the udc thread can reassign the uart to other usage without block the uart driver */
	fake_uart_init();

	/* set vci_in to gpio mode to change pin status */
	//md_vci_set_alt_func_to_gpio();

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

int board_suspend(void)
{

	return 0;
}

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

	/* power on and set the APU_PWRON + ESPI pins */
	for (uint8_t i = 0; !GPIO_isNullPin(pinList[i]); i++) {
		gpio_write_pin(pinList[i], 1);
		gpio_set_power(pinList[i], GPIO_CTRL_PWRG_VTR_IO);
	}
	
	uint32_t eSPIPins[] = {
		PIN_LIST_ESPI_SIGNALES_TO_PWRON,
	};
	
	for (uint8_t i = 0; !GPIO_isNullPin(eSPIPins[i]); i++) {
		gpio_set_power(eSPIPins[i], GPIO_CTRL_PWRG_VTR_IO);
	}

/* set the WWAN and WLAN based on the NV option */
	if (g_auxD3Cold & 0x40) {
		gpio_write_pin(EC_WLAN_AUX_RST_N, 0);
		LOG_DBG("WLAN: %d", gpio_read_pin( EC_WLAN_AUX_RST_N ));
	}
}

/**
* PLAT-84470
* Init all of the pin status to 0 on APU_RST# but keep them untouched across S3/S0i3
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
 * @brief turn off the jtag interface in case of current leakage
 *
 * @param void
 * @retval void
 */
void board_turnOffJtagInterface (void)
{
	// *(unsigned char *)(NPCX_SCFG_REG_ADDR + 0x120) = 0x6; // Disabed Jtag;
	// *(unsigned char *)(NPCX_SCFG_REG_ADDR + 0x121) = 0x6;
}

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