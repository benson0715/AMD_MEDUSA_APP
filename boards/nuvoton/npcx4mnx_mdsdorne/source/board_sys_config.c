/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <soc.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "board_sys_config.h"
#include "board_id.h"
#include "errcodes.h"
#include "app_pseq.h"
#include "acpi_region.h"
#include "f_nv_options.h"
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#include "app_ucsi_tunnel.h"
#include "app_usbc_task.h"

#define NUVOTUON_BSYSCOF_BASE_ADDR      0x1006f100

LOG_MODULE_REGISTER(board_sys_config, CONFIG_BOARD_SYS_CONFIG_LOG_LEVEL);



T_EC_CONFIG_REGION_FIELD *_s_BoardSysConfig;



/**
 * @brief board systerm config init
 *
 * @retval 0 is successful
 */
int brdSysConfig_Init(void)
{
	uint16_t check_crc16;
	uint16_t check_fixed_data;

	check_fixed_data = *((uint16_t*)NUVOTUON_BSYSCOF_BASE_ADDR+126);
	if(check_fixed_data == 0x55AA)
	{
		check_crc16 = crc16(0x1021, 0, ( uint8_t *)NUVOTUON_BSYSCOF_BASE_ADDR, 254);
		check_crc16 = ((check_crc16>>8)&0xff)|((check_crc16<<8)&0xff00); //LSB -->> MSB
		
		if(check_crc16!= *((uint16_t*)NUVOTUON_BSYSCOF_BASE_ADDR+127))
		{

			LOG_ERR("Board sys config Crc Failed, checkcrc =%x config crc = %x",check_crc16,*((uint16_t*)NUVOTUON_BSYSCOF_BASE_ADDR+127));
			return -1;
		}
		else
		{
			_s_BoardSysConfig= (T_EC_CONFIG_REGION_FIELD*)NUVOTUON_BSYSCOF_BASE_ADDR;
		}
	} 
	else
	{
		LOG_ERR("Board sys config OVERFLOW Failed, Fixed data =%x get data = %x",0x55aa,*((uint16_t*)NUVOTUON_BSYSCOF_BASE_ADDR+126));
		return -2;
	}
	return 0;
}