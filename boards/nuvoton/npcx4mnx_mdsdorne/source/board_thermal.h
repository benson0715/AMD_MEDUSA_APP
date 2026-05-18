/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#ifndef __BOARD_THERMAL_H__
#define __BOARD_THERMAL_H__

/* ************************** *
 *          Macro             *
 * ************************** */
#define APML_ERROR_NUM_THRESHOLD  (500)
#define APML_MB_DELAY_MS          (2400)

#define _GET_INT_FROM_U16(x)   (((x) >> 8) & 0xFF)
#define _GET_DEC_FROM_U16(x)   (((uint32_t)(x) & 0x00000000FFul) *  391/100)

typedef struct s_board_thermal_info {
	uint16_t _s_u16pcbTmp;     /* Local Sensor */
	uint16_t _s_u16pcbTmpQ[2];   /* Remote1 Sensor */
	uint16_t _s_u16pcbTmpQMax;   /* The Max of board Sensor */
	uint16_t _s_u16pcbTmp2;     /* Local2 Sensor */
	uint16_t _s_u16pcbTmpQ2[4];   /* Remote1 Sensor */
} board_thermal_info;

extern void board_thermal_getinfo (board_thermal_info * BoardTmp);
extern bool board_thermal_setResistanceCorrection(bool toEnable);
extern void board_thermal_reporter(board_thermal_info  BoardTmp);
extern uint8_t board_thermal_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
#endif /* __BOARD_THERMAL_H__ */

