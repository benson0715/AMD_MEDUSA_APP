/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IO-expander hooks for AMD MDS Plum on RTS5918. Included unconditionally
 * from boards/board_config.h; implementations live in board_ioexp.c
 * which is gated by CONFIG_IO_EXPANDER.
 *
 * TODO(realtek-schematic): Plum carries up to 7× PCA9535 expanders on
 * I2C_1 (per AMD docs). Decide which RTS5918 I2C controller hosts each
 * one before re-enabling CONFIG_IO_EXPANDER.
 */

#ifndef __BOARD_IOEXP_H__
#define __BOARD_IOEXP_H__

#include <stdint.h>
#include <stdbool.h>

extern uint8_t g_S5AlwEnFlag;

void board_ioexp_initIoExp(void);
void board_ioexp_initIoExp0(void);

void board_ioexp_AcpiHandler_IOExp0(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp1(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp2(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp3(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp4(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp5(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp6(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp7(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp8(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp9(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp10(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp11(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp12(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void board_ioexp_AcpiHandler_IOExp13(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);

#endif /* __BOARD_IOEXP_H__ */
