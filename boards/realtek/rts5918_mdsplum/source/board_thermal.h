/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thermal-sensor hooks for AMD MDS Plum on RTS5918. On NPCX4 Plum
 * thermal data came from SB-TSI (socket), TMP432 (board), and
 * LM95234 (zones) via I2C, plus PECI for CPU temp. RTS5918 has no
 * PECI block; CPU temp routes through SB-TSI exclusively.
 */

#ifndef __BOARD_THERMAL_H__
#define __BOARD_THERMAL_H__

#include <stdint.h>
#include <stdbool.h>

#define APML_ERROR_NUM_THRESHOLD  (500)
#define APML_MB_DELAY_MS          (2400)

#define _GET_INT_FROM_U16(x)   (((x) >> 8) & 0xFF)
#define _GET_DEC_FROM_U16(x)   (((uint32_t)(x) & 0x00000000FFul) *  391 / 100)

typedef struct s_board_thermal_info {
	uint16_t _s_u16pcbTmp;
	uint16_t _s_u16pcbTmpQ[2];
	uint16_t _s_u16pcbTmpQMax;
	uint16_t _s_u16pcbTmp2;
	uint16_t _s_u16pcbTmpQ2[4];
} board_thermal_info;

extern void board_thermal_getinfo(board_thermal_info *BoardTmp);
extern bool board_thermal_setResistanceCorrection(bool toEnable);
extern void board_thermal_reporter(board_thermal_info BoardTmp);
extern uint8_t board_thermal_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);

#endif /* __BOARD_THERMAL_H__ */
