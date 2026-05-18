/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board-ID stubs for AMD MDS Plum on RTS5918. Real EEPROM read path
 * to be added once the Plum I2C bus assignment is finalised.
 */

#include "board_id.h"

uint8_t brdId(void)
{
	return 1; /* Plum RevB+ default. TODO(realtek-schematic): read from EEPROM. */
}

uint32_t brdCfg(void)
{
	return 0;
}

uint8_t brdSku(void)
{
	return 0;
}

uint8_t brdRev(void)
{
	return 0;
}

uint8_t brdSR(uint8_t bitNum)
{
	ARG_UNUSED(bitNum);
	return 0;
}

bool brdId_isPEO(void)
{
	return false;
}

bool brdId_isDM(void)
{
	return false;
}

bool brdId_isRevC(void)
{
	return false;
}

bool brdId_readBuf(uint32_t offset, uint32_t length, uint8_t *buf)
{
	ARG_UNUSED(offset);
	ARG_UNUSED(length);
	ARG_UNUSED(buf);
	return false;
}

bool brdId_writeBuf(uint32_t offset, uint32_t length, uint8_t *buf)
{
	ARG_UNUSED(offset);
	ARG_UNUSED(length);
	ARG_UNUSED(buf);
	return false;
}

int brdId_Init(void)
{
	return 0;
}

uint8_t acpi_brdId_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	ARG_UNUSED(isRead);
	ARG_UNUSED(ui8Idx);
	ARG_UNUSED(pui8Data);
	return 0;
}

void acpi_brdIdExpField_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	ARG_UNUSED(isRead);
	ARG_UNUSED(ui8Idx);
	ARG_UNUSED(pui8Data);
}

void brdId_ctrl_Ddr5SidimmPwrEn(bool pin)
{
	ARG_UNUSED(pin);
}

bool brdId_supportMemPMIC(void)
{
	return false;
}

bool brdId_supportDdr5(void)
{
	return false;
}

void brdId_transitionToS5(void)
{
}

void brdId_pdPowerRoleG3(void)
{
}

void brdId_pdPowerRoleS5(void)
{
}

BID_MDS_CPU_TYPE brdId_CpuOpnType(void)
{
	return BID_MDS_CPU_TYPE_45W;
}
