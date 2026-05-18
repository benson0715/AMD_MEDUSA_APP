/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DBG_RW__
#define __DBG_RW__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>

#define DBGRW_ACPI_SPACE_BASE   0x00
#define DBGRW_MAX_ITEM_NUM      6
#define DBGRW_ACPI_CTRL_OFFSET  0x30

#if (DBGRW_MAX_ITEM_NUM > 6)
#error ("The maximal DBGRW_MAX_ITEM_NUM is 6 !!")
#endif

extern uint8_t dbgRw_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __DBG_RW__

