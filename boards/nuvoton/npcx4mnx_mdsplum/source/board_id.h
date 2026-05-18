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
/* is the board id for DM */
bool brdId_isDM(void);
/* is the board id RevC */
bool brdId_isRevC(void);
//
// ID assignment (Updated on 3/26/2025)
//
// 0  - Plum TV (RevA)
// 1  - Plum RevB+
// 2  - Hickory
// 3  - Juniper
// 4  - Pine
// 5  - (reserverd for eFFP Dorne)
// 6  - Plum DAP
// 7  - Hickory DAP
// 8  - Juniper DAP
// SKUs:
//
//  K012 Plum
//    01	PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 No APU 45W Max, LPDDR5x No Pop, EC onboard  
//    02	PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 DM 28W Max, LPDDR5x No Pop, EC onboard  
//    03	PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 DM 45W Max, LPDDR5x No Pop, EC onboard    
//    04	PCBA: Plum (9600 w/ mid-loss), SLT Rev B, FP10 No APU 45W Max, LPDDR5x No Pop, EC onboard
//    05	PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 No APU 45W Max, LPDDR5x No Pop, Support EC module
//    06	PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 DM 80W Max, LPDDR5x No Pop, EC onboard
//    07    PCBA: Plum (9600 w/ mid-loss), SLT Rev B,  FP10 No APU 28W Max, LPDDR5x No Pop, EC onboard
//    08    PCBA: Plum (9600 w/ mid-loss), Rev B, FP10 No APU 28W, LPDDR5x No Pop, EC onboard
//
//  K016  Hickory
//    01  PCBA: Hickory Rev B, FP10 No APU 45W Max, DDR5 CSODIMM, EC onboard  
//    02  PCBA: Hickory Rev B, FP10 DM 28W Max, DDR5 CSODIMM, EC onboard  
//    03  PCBA: Hickory Rev B, FP10 DM 45W Max, DDR5 CSODIMM, EC onboard  
//    04  PCBA: Hickory Rev B, SLT Rev B, FP10 No APU 45W, DDR5 CSODIMM , EC onboard  
//    05  PCBA: Hickory Rev B, FP10 No APU 80W Max, DDR5 CSODIMM, Support EC module  
//    06  PCBA: Hickory Rev B, FP10 DM 80W Max, DDR5 CSODIMM, EC onboard  
//    07  PCBA: Hickory Rev B, FP10 No APU 28W Max, DDR5 CSODIMM, EC onboard  
//    08  PCBA: Hickory Rev B, SLT Rev B, FP10 No APU 28W Max, DDR5 CSODIMM , EC onboard  
//
//  K018  Juniper
//    01  PCBA: Juniper Rev A, FP10 No APU 45W, LPCAMM2, EC onboard  
//    02  PCBA: Juniper Rev A, FP10 DM 28W, LPCAMM2, EC onboard  
//    03  PCBA: Juniper Rev A, FP10 DM 45W, LPCAMM2, EC onboard  
//    04  PCBA: Juniper Rev A, FP10 DM 80W, LPCAMM2, EC onboard  
//    05  PCBA: Juniper Rev A, FP10 No APU 28W, LPCAMM2, EC onboard  
//
//  K046  Pine
//    01  PCBA: Pine (8533 w/ FR4), Rev A , FP10 No APU 45W Max, LPDDD5x No Pop, EC onboard
//    02  PCBA: Pine (8533 w/ FR4), Rev A , FP10 No APU 28W , LPDDD5x No Pop, EC onboard   
//    03  PCBA: Pine (8533 w/ FR4), Rev A , FP10 DM 28W Max, LPDDD5x No Pop, EC onboard  
//    04  PCBA: Pine (8533 w/ FR4), Rev A , FP10 DM 45W Max, LPDDD5x No Pop, EC onboard  
//    05  PCBA: Pine (8533 w/ FR4), Rev A , FP10 DM 80W Max, LPDDD5x No Pop, EC onboard  
//
//  K013  Plum DAP
//    01  PCBA: Plum DAP(9600 w/ mid-loss), Rev A, FP10 No APU 45W, LPDDR5x No Pop, EC onboard 
//    02  PCBA: Plum DAP(9600 w/ mid-loss), Rev A, FP10 DM 28W, LPDDR5x No Pop, EC onboard  
//    03  PCBA: Plum DAP(9600 w/ mid-loss), Rev A, FP10 DM 45W, LPDDR5x No Pop, EC onboard  
//    04  PCBA: Plum DAP(9600 w/ mid-loss), Rev A, FP10 DM 80W, LPDDR5x No Pop, EC onboard  
//    05  PCBA: Plum DAP(9600 w/ mid-loss), Rev A, FP10 No APU 28W, LPDDR5x No Pop, EC onboard  
//
//  K017  Hickory DAP
//    01  PCBA: Hickory DAP Rev A, FP10 No APU, 45W, DDR5 CSODIMM, EC onboard
//    02  PCBA: Hickory DAP Rev A, FP10 DM 28W Max, DDR5 CSODIMM, EC onboard
//    03  PCBA: Hickory DAP Rev A, FP10 DM 45W Max, DDR5 CSODIMM, EC onboard
//    04  PCBA: Hickory DAP Rev A, FP10 DM 80W Max, DDR5 CSODIMM, EC onboard
//    05  PCBA: Hickory DAP Rev A, FP10 No APU, 28W, DDR5 CSODIMM, EC onboard
//
//  K047  Juniper DAP
//    01  PCBA: Juniper DAP Rev A, FP10 No APU 45W, LPCAMM2, EC onboard  
//    02  PCBA: Juniper DAP Rev A, FP10 DM 28W, LPCAMM2, EC onboard  
//    03  PCBA: Juniper DAP Rev A, FP10 DM 45W, LPCAMM2, EC onboard  
//    04  PCBA: Juniper DAP Rev A, FP10 DM 80W, LPCAMM2, EC onboard  
//    05  PCBA: Juniper DAP Rev A, FP10 No APU 28W, LPCAMM2, EC onboard  

#define BRDID_isTV                  (0)
#define BRDID_isPlumDAP             (6 == brdId()        || \
                                     0x0B == brdId())
#define BRDID_isPlumDAP20W          (0x0B == brdId())
#define BRDID_isHickoryDAP          (7 == brdId())
#define BRDID_isJuniperDAP          (8 == brdId())
#define BRDID_isHemlock             (9 == brdId())
#define BRDID_isSequoia             (0xA == brdId())


#define BRDID_isREVA                (0)
#define BRDID_isDAP                 (BRDID_isPlumDAP     || \
                                     BRDID_isHickoryDAP|| \
                                     BRDID_isJuniperDAP)
#define BRDID_isPlum                (1 == brdId()        || \
                                     4 == brdId()        || \
                                     9 == brdId()        || \
                                     0xA == brdId()      || \
                                     0x0E == brdId()     || \
                                     BRDID_isPlumDAP )     
                                     
#define BRDID_isHickory             (2 == brdId()        || \
                                     BRDID_isHickoryDAP )

#define BRDID_isJuniper             (3 == brdId()        || \
                                     BRDID_isJuniperDAP )

#define BRDID_isPEO                 (brdId_isPEO())

#define BRDID_isDM                  (brdId_isDM())

/*
 * Platform Configuration
 */
#define CFG_OPS_PD0                   (0)
#define CFG_MSK_PD0                   (0x03)
#define CFG_OPN_PD0_TPS66994BF        (0x01)
#define CFG_OPN_PD0_CCG8DF            (0x02)
//#define CFG_OPN_PD0_IT8857            (0x03)
#define CFG_OPN_PD0_RTK               (0x03)

#define CFG_OPS_PD1                   (2)
#define CFG_MSK_PD1                   (0x03)
#define CFG_OPN_PD1_TPS66994BF        (0x01)
#define CFG_OPN_PD1_CCG8SF            (0x02)
//#define CFG_OPN_PD1_IT8858            (0x03)
#define CFG_OPN_PD1_RTK               (0x03)

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
/* use board id to determain memory PMIC sequence */
bool brdId_supportMemPMIC(void);
/* use board id to determain DDR5 sequence */
bool brdId_supportDdr5(void);
/* callback in power sequence transition from Sx to S5. */
void brdId_transitionToS5(void);
/* callback in power sequence when system enter G3 for PD controller power role */
void brdId_pdPowerRoleG3(void);
/* callback in power sequence when system enter S5 for PD controller power role */
void brdId_pdPowerRoleS5(void);
/* return the CPU OPN type */
BID_MDS_CPU_TYPE brdId_CpuOpnType(void);

#endif /* BOARD_H */
