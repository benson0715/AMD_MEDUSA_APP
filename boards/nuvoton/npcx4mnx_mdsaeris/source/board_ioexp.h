/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_IOEXP_H__
#define __BOARD_IOEXP_H__

#include <stdint.h>
#include <stdbool.h>
#include "ioexp_hub.h"

extern uint8_t g_S5AlwEnFlag;

/* onboard ioexp init with different power rail */
void board_ioexp_initIoExp(void);

/* onboard ioexp acpi handler */
void board_ioexp_AcpiHandler_IOExp0 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp1 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp2 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp3 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp4 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp5 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp6 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp7 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp8 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp9 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp10 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp11 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp12 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
void board_ioexp_AcpiHandler_IOExp13 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __BOARD_IOEXP_H__

