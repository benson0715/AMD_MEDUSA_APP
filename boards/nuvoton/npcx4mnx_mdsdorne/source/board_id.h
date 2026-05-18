/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __BOARD_H__
#define __BOARD_H__


#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

/* MDS CPU OPN */
typedef enum {
	BID_MDS_CPU_TYPE_NA = 0,
	BID_MDS_CPU_TYPE_28W,
	BID_MDS_CPU_TYPE_45W,
	BID_MDS_CPU_TYPE_80W,
    BID_MDS_CPU_TYPE_10W,
} BID_MDS_CPU_TYPE;

/* board id */
uint8_t brdId( void );
/* Cfg */
uint32_t brdCfg(void);
/* SKU */
uint8_t brdSku( void );
/* Hardware version */
uint8_t brdRev( void );
/* Special rework, where bitNum is zero-based bit index, return either 1 or 0 */
uint8_t brdSR(uint8_t bitNum);
/* is the board id for PEO */
bool brdId_isPEO(void);
/* is the board id for 28W board */
bool brdId_is28W();
/* is the board id for 45W board */
bool brdId_is45W();
/* is the board id for DM */
bool brdId_isDM(void);
/* is the board id RevC */
bool brdId_isRevC(void);

// 
// ID assignment (Updated on 3/26/2025)
//
// 0  - Dorne TV
// 5  - Dorne (arrive on 4/8/2025)
//
// SKUs:
//
//  K019 Dorne
//    01	PCBA: Dorne, Rev A, eFFP, MDS FP10 No APU, 45W Max, LPDDR5x No Pop
//    02	PCBA: Dorne, Rev A, eFFP, MDS FP10, 45W DM, LPDDR5x No Pop
//    03	PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10 NO APU, 28W Max, LPDDR5x No Pop
//    04	PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10, 28W DM, LPDDR5x No Pop 

#define BRDID_isTV                  (0)
#define BRDID_isDorneDAP            (0) //(9 == brdId())
#define BRDID_isDAP                 (BRDID_isDorneDAP)
#define BRDID_isDorne          		(5 == brdId() \
									 || BRDID_isDorneDAP)
#define BRDID_isPEO                 (brdId_isPEO())

#define BRDID_isDM                  (brdId_isDM())

/*
 * Platform Configuration
 */
#define CFG_OPS_PD0                   (0)
#define CFG_MSK_PD0                   (0x03)
#define CFG_OPN_PD0_TPS66994BF        (0x01)
#define CFG_OPN_PD0_CCG8DF            (0x02)
#define CFG_OPN_PD0_IT8857            (0x03)

#define CFG_OPS_PD1                   (2)
#define CFG_MSK_PD1                   (0x03)
#define CFG_OPN_PD1_TPS66994BF        (0x01)
#define CFG_OPN_PD1_CCG8SF            (0x02)
#define CFG_OPN_PD1_IT8858            (0x03)

#define CFG_OPS_RETIMER0              (4)
#define CFG_MSK_RETIMER0              (0x03)
#define CFG_OPN_RETIMER0_PS8835       (0x01)
#define CFG_OPN_RETIMER0_KB8010       (0x02)

#define CFG_OPS_RETIMER1              (6)
#define CFG_MSK_RETIMER1              (0x03)
#define CFG_OPN_RETIMER1_PS8835       (0x01)
#define CFG_OPN_RETIMER1_KB8010       (0x02)

#define CFG_OPS_RETIMER2              (8)
#define CFG_MSK_RETIMER2              (0x03)
#define CFG_OPN_RETIMER2_PS8835       (0x01)
#define CFG_OPN_RETIMER2_KB8010       (0x02)

#define CFG_OPS_DIMM                  (10)
#define CFG_MSK_DIMM                  (0x01)
#define CFG_OPN_DIMM_LpDDR5           (0x00)
#define CFG_OPN_DIMM_DDR5             (0x01)

#define _PLATFORM_CFG_                (brdCfg())
#define CHECK_CFG(X, Y)               (((_PLATFORM_CFG_ >> (CFG_OPS_##X)) & (CFG_MSK_##X)) == (CFG_OPN_##X##_##Y))

bool brdId_readBuf (uint32_t offset, uint32_t length, uint8_t * buf);
bool brdId_writeBuf (uint32_t offset, uint32_t length, uint8_t * buf);

/**
 * Run by PURMAIN thread that may conflict with PD thread (as I2C Controller)
 */
#define BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET 0x93
#define BID_ACPI_SPACE_PAGE_BASE             0
#define BID_ACPI_SPACE_PAGE_SELECTOR_OFFSET  0x30

#define _H2C(p, b)      ((((p) >> (b*4)) & 0x0F) > 9) ? (((p) >> (b*4)) & 0x0F) + 0x37 : (((p) >> (b*4)) & 0x0F) + 0x30
/* init board id information */
int brdId_Init();
/* board id ACPI handler */
uint8_t acpi_brdId_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* Board id in acpi ec name space */
void acpi_brdIdExpField_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* use board id to determain DDR5 sequence */
void brdId_ctrl_Ddr5SidimmPwrEn(bool pin);
/* use board id to determain DDR5 sequence */
bool brdId_supportMemPMIC(void);
/* callback in power sequence transition from Sx to S5. */
void brdId_transitionToS5(void);
/* callback in power sequence when system enter G3 for PD controller power role */
void brdId_pdPowerRoleG3(void);
/* callback in power sequence when system enter S5 for PD controller power role */
void brdId_pdPowerRoleS5(void);
/* return the CPU OPN type */
BID_MDS_CPU_TYPE brdId_CpuOpnType(void);

#endif /* BOARD_H */
