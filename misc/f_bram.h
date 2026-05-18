/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __F_BRAM_H__
#define __F_BRAM_H__

#include <zephyr/kernel.h>
#include "u_numbers.h"

#define F_BRAM_ACPI_SPACE_PAGE_BASE             0
#define F_BRAM_ACPI_SPACE_PAGE_SELECTOR_OFFSET  0x30
#define F_BRAM_RAM_SIZE                         128
#define F_BRAM_RAM_BASE                         (VBAT_RAM_BASE)

#define F_BRAM_RAM_USER_SIZE                    (F_BRAM_RAM_SIZE - 0x10)
#define F_BRAM_RAM_USER_BASE                    (F_BRAM_RAM_BASE + 0x10)

#define _CHAR4_TO_UINT32(a, b, c, d)            (((d) << 24) | ((c) << 16) | ((b) << 8) | (a))

#define F_BRAM_COLD_BOOT_SIGNATURE              _CHAR4_TO_UINT32('C', 'O', 'L', 'D')
#define F_BRAM_WDT_BOOT_SIGNATURE               _CHAR4_TO_UINT32('_', 'W', 'D', 'T')
#define F_BRAM_VCI_BOOT_SIGNATURE               _CHAR4_TO_UINT32('_', 'V', 'C', 'I')
#define F_BRAM_UNEXPECTED_BOOT_SIGNATURE        _CHAR4_TO_UINT32('U', 'E', 'X', 'P')
#define F_BRAM_FLASH_BIOS_WDT_BOOT_SIGNATURE    _CHAR4_TO_UINT32('F', 'A', 'S', 'H')
#define F_BRAM_NV_OPTION_UPDATE_SIGNATURE       _CHAR4_TO_UINT32('N', 'V', 'U', 'D')

#pragma pack(push,1)
typedef struct {
	uint32_t bramSig;
} F_BRAM_COMM;

typedef union {
	volatile uint8_t u8Arr[F_BRAM_RAM_SIZE];
	volatile uint32_t u32Arr[F_BRAM_RAM_SIZE / sizeof(uint32_t)];
	F_BRAM_COMM Comm;
} F_BRAM_SPACE;
#pragma pack(pop)

/*
 * This function should be called only once whenever off-chip FW starts running
 */
void f_bram_onBootUp (void);
void f_bram_setSig (uint32_t sig);

uint32_t f_bram_bootReason (void);
void f_bram_bootReasonClear (void);
uint32_t f_bram_getRow(uint8_t r);
void f_bram_setRow(uint8_t r, uint32_t u32Val, uint32_t u32Msk);

uint8_t f_bram_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __F_BRAM_H__

