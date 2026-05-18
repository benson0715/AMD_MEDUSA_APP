/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_acpi.h"
#include "f_nv_options.h"
#include "board_config.h"
#include "board_dac.h"
LOG_MODULE_DECLARE(dac, CONFIG_EC_LOG_LEVEL);
/* ************************** *
 *      static valuable       *
 * ************************** */
static struct k_work dac_work;
void board_dac_event_handler (void);

/**
 * board_dac_event
 *
 * @note
 *  pmic tunnel handler to access I2C DAC
 */
static void board_dac_event (struct k_work *w)
{
	board_dac_event_handler();
}

/**
 * board_dac_event_trigger
 *
 * @note
 *   trigger pmic i2c tunnel access for DAC.
 */
void board_dac_event_trigger(void)
{
	k_work_submit(&dac_work);
}

BOARD_DAC_TO_VOLTAGE g_dac_voltage_vdd_mem[] = {
	
	{0xB2,    850},
	{0xAD,    860},
	{0xA8,    870},
	{0xA3,    880},
	{0x9E,    890},
	{0x99,    900},
	{0x94,    910},
	{0x8F,    920},
	{0x8A,    930},
	{0x85,    940},
	{0x00,    950},
	{0x05,    960},
	{0x0A,    970},
	{0x0F,    980},
	{0x14,    990},    
	{0x19,    1000},
	{0x1E,    1010},
	{0x23,    1020},
	{0x28,    1030},
	{0x2D,    1040},
	{0x32,    1050},

	{0xFF, 0xFFFF},

};	

BOARD_DAC_ITEM g_dac_Table[] = {
	
	{0x30, 0xFB, g_dac_voltage_vdd_mem, 0, ACPI_DAC_TUNNEL_ERROR, 0},
	{0x50, 0xFB, g_dac_voltage_vdd_mem, 0, ACPI_DAC_TUNNEL_ERROR, 0},
 	{0xFF, 0xFF, NULL,                  0, ACPI_DAC_TUNNEL_ERROR, 0},
	
};

/**
 * board_dac_getDacValue
 *
 * @note
 *   convert the ECRAM data to dac value. ECRAM data with 5mV step size and 500mV offset
 */
uint8_t board_dac_getDacValue(uint8_t val, BOARD_DAC_TO_VOLTAGE *table)
{
	/*
	uint16_t voltage = val*5 + 500;

	for(uint8_t i = 0; table[i].dac_val != 0xFF; i++)
	{
		if(voltage == table[i].voltage){
			return table[i].dac_val;
		}
	}
	return 0xFF;
	*/
	return val;
}

/**
 * board_dac_getNvOptionDacValue
 *
 * @note
 *   get the nv option stored default dac value
 */
uint8_t board_dac_getNvOptionDacValue(uint8_t ui8Idx)
{
	uint8_t option_val = 0xFF;
	
	switch(ui8Idx) {
		case 0:
			GET_NV_OPTIONS(vdd_mem_a_value, option_val);
			break;
		
		case 1:
			GET_NV_OPTIONS(vdd_mem_b_value, option_val);
			break;
		
		case 2:
			// Reserve extended dac item
			break;
		
		case 3:
			// Reserve extended dac item
			break;
		
		default:
			break;
	}
	
	return option_val;
}

/**
 * app_dac_setNvOptionDacValue
 *
 * @note
 *   set the nv option default dac value
 */
void app_dac_setNvOptionDacValue(uint8_t ui8Idx, uint8_t value)
{
	switch(ui8Idx) {
		case 0:
			SET_NV_OPTIONS(vdd_mem_a_value, value);
			break;
		
		case 1:
			SET_NV_OPTIONS(vdd_mem_b_value, value);
			break;
		
		case 2:
			// Reserve extended dac item
			break;
		
		case 3:
			// Reserve extended dac item
			break;
		
		default:
			break;
	}
}

/**
 * board_dac_Tunnel_acpiHandler
 *
 * @note
 *   ACPI handler for dac configuration
 */
uint8_t board_dac_Tunnel_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t option_val = 0xFF;
	
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	if (ui8Idx == 0x30) {
		
		return 1;

	} else if (ui8Idx < 0x30) {
		
		if(isRead){
			switch(ui8Idx) {
			case 0:
				/* get the default dac voltage */
                for(uint8_t i = 0; g_dac_Table[i].REG != 0xFF; i++){
    				option_val = board_dac_getNvOptionDacValue(i);
                }
				* pui8Data = option_val;
			  break;
			
			case 1:		
			    // because of the routine running sequence, we only update the last item status as the final result.
                for(uint8_t i = 0; g_dac_Table[i].REG != 0xFF; i++){
    				*pui8Data = g_dac_Table[i].response;
                }
			  break;
			
			case 2:
			    *pui8Data = (uint8_t)(0x01); // 0x01h: Voltage Tunnel Supported, 0x00h: not support.
				break;
			
			case 3:
				// Reserve extended dac item
				break;
			
			default:
				break;	
		  }
	  } else {
			switch(ui8Idx) {
			case 0:
			    // Update all table here, because the Maple Mem A/B always have same tunnel DAC.
                for(uint8_t i = 0; g_dac_Table[i].REG != 0xFF; i++){
    				g_dac_Table[i].receive_value = *pui8Data;
    				g_dac_Table[i].response = ACPI_DAC_TUNNEL_ONGOING;
				/* set element update as active */
    				g_dac_Table[i].active = 1;
                }
				/* trigger the update event */
				board_dac_event_trigger();
			  break;
			
			case 1:
				// Do not support to wirte
				break;
			
			case 2:
				// Reserve extended dac item
				break;
			
			case 3:
				// Do not support to wirte
				break;
			
			default:
				break;
			}		
		}
	}
	return 1;
}

/**
 * board_dac_event_handler
 *
 * @note
 *   handle DAC event for I2C programming
 */
void board_dac_event_handler ( )
{
		uint8_t option_val = 0xFF;
		uint32_t ret = 0;
		uint8_t u8data = 0;
	
		for(uint8_t i = 0; g_dac_Table[i].REG != 0xFF; i++){
				if (g_dac_Table[i].active){
					/* clear the active after make it effective */
					g_dac_Table[i].active = 0;
					
					/* get default dac voltage */
					option_val = board_dac_getNvOptionDacValue(i);
					
					/* No need to change */
					if(g_dac_Table[i].receive_value == option_val){
						g_dac_Table[i].response = ACPI_DAC_TUNNEL_SUCCESS;		
						LOG_DBG("Write PMIC/vdd_mem DAC the same value on I2C, skip [%02X]\n", g_dac_Table[i].receive_value);				
						continue;
					}			
					/* convert the voltage to DAC value */
					u8data = board_dac_getDacValue(g_dac_Table[i].receive_value, g_dac_Table[i].DacToVoltage_t);

					/* Cannot find */
					if(u8data == 0xFF){
						g_dac_Table[i].response = ACPI_DAC_TUNNEL_OUTOFLIST;
						LOG_DBG("Write PMIC/vdd_mem _s_vdd_mem_val out of list on I2C [%02X]\n", g_dac_Table[i].receive_value);				
						continue;
					}

					/* I2C programming to make it effective */
					ret = i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, g_dac_Table[i].I2C_Address, &(g_dac_Table[i].REG), 1, &u8data, 1);
					if (ret == 0) {
						/* set the nv option to save new default dac voltage */
						app_dac_setNvOptionDacValue(i, g_dac_Table[i].receive_value);
						g_dac_Table[i].response = ACPI_DAC_TUNNEL_SUCCESS;	
					} else {
						g_dac_Table[i].response = ACPI_DAC_TUNNEL_ERROR;
					}
					
					LOG_DBG("Write PMIC/vdd_mem on I2C [%d] Addr: %02X, Idx: %02X, DAC: %02X, Val: %02X; ack = %d\n",
					I2C_DAC_PWR_PORT, g_dac_Table[i].I2C_Address, g_dac_Table[i].REG, u8data, g_dac_Table[i].receive_value, ret);	
				}
		}
}

/**
 * board_dac_restore_setting
 *
 * @note
 *   restore DAC settings from NV options during initialization
 */
void board_dac_restore_setting(void)
{
	uint8_t option_val = 0xFF;
	uint8_t u8data;
	uint32_t ret;
	
	for(uint8_t i = 0; g_dac_Table[i].REG != 0xFF; i++){
		/* get the default dac value for sepcific index */
		option_val = board_dac_getNvOptionDacValue(i);
		/* convert the voltage to dac value */
		u8data = board_dac_getDacValue(option_val, g_dac_Table[i].DacToVoltage_t); 
		/* I2C programming to make it effective */
		ret = i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, g_dac_Table[i].I2C_Address, &(g_dac_Table[i].REG), 1, &u8data, 1);
		LOG_DBG("VDDP_MEM		 ... ret %d vdd_mem = 0x%02X reg 0x%02X = 0x%02X%s\n", ret, option_val, g_dac_Table[i].REG, u8data, (ret != 0) ? ", but !!ERROR!!" : "");
	}
}

/**
 * board_dac_Init
 *
 * @note
 *   initialize DAC module, restore settings and register work event
 */
void board_dac_Init(void)
{
	/* restore the dac setting */
	board_dac_restore_setting();
	/* register the event for dac */
	k_work_init(&dac_work, board_dac_event);
}