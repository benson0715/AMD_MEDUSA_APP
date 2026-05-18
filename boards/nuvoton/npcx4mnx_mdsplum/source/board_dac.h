/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_DAC_H__
#define __APP_DAC_H__

#include <zephyr/kernel.h>

#define ACPI_DAC_TUNNEL_SUCCESS   0xAC
#define ACPI_DAC_TUNNEL_ERROR     0xE2
#define ACPI_DAC_TUNNEL_ONGOING   0xCC
#define ACPI_DAC_TUNNEL_OUTOFLIST 0xDD

typedef struct {
	uint8_t  dac_val;
	uint16_t voltage;    
}BOARD_DAC_TO_VOLTAGE;

typedef struct {
    uint8_t                I2C_Address;
    uint8_t                REG;
    BOARD_DAC_TO_VOLTAGE     *DacToVoltage_t;
    uint8_t                receive_value;
    uint8_t                response;
    uint8_t                active;
} BOARD_DAC_ITEM;

uint8_t board_dac_Tunnel_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_dac_Init(void);

#endif //end of __APP_DAC_h__
