/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-board board-id/SKU/config interface for AMD MDS Plum on RTS5918.
 * Mirrors the API surface declared by AMD's NPCX4 Plum header
 * (boards/nuvoton/npcx4mnx_mdsplum/source/board_id.h) so app/smchost
 * and friends keep linking. Implementations return zero until the
 * Plum EEPROM read path is wired through i2c_hub on RTS5918.
 *
 * TODO(realtek-schematic): once the I2C bus that hosts the board-ID
 * EEPROM (0x54) is confirmed on Plum + RTS5918, brdId_Init() should
 * read the EEPROM and populate the cached SKU/Rev/Cfg values.
 */

#ifndef __BOARD_ID_H__
#define __BOARD_ID_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

typedef enum {
	BID_MDS_CPU_TYPE_NA = 0,
	BID_MDS_CPU_TYPE_28W,
	BID_MDS_CPU_TYPE_45W,
	BID_MDS_CPU_TYPE_80W,
	BID_MDS_CPU_TYPE_10W,
} BID_MDS_CPU_TYPE;

uint8_t brdId(void);
uint32_t brdCfg(void);
uint8_t brdSku(void);
uint8_t brdRev(void);
uint8_t brdSR(uint8_t bitNum);
bool brdId_isPEO(void);
bool brdId_isDM(void);
bool brdId_isRevC(void);

/* High-level board-class predicates referenced from app/* and
 * Jupiter/*. All default to false on RTS5918 until the EEPROM SKU read
 * path is implemented.
 */
#define BRDID_isTV          (0)
#define BRDID_isPlumDAP     (0)
#define BRDID_isPlumDAP20W  (0)
#define BRDID_isHickoryDAP  (0)
#define BRDID_isJuniperDAP  (0)
#define BRDID_isHemlock     (0)
#define BRDID_isSequoia     (0)
#define BRDID_isREVA        (0)
#define BRDID_isDAP         (0)
#define BRDID_isPlum        (1)  /* board target IS Plum, so this is true */
#define BRDID_isHickory     (0)
#define BRDID_isJuniper     (0)
#define BRDID_isPEO         (brdId_isPEO())
#define BRDID_isDM          (brdId_isDM())

/* Platform-cfg bitfield positions; values mirror AMD layout so the
 * CHECK_CFG() expression resolves the same way against board-id reads.
 */
#define CFG_OPS_PD0                 (0)
#define CFG_MSK_PD0                 (0x03)
#define CFG_OPN_PD0_TPS66994BF      (0x01)
#define CFG_OPN_PD0_CCG8DF          (0x02)
#define CFG_OPN_PD0_RTK             (0x03)

#define CFG_OPS_PD1                 (2)
#define CFG_MSK_PD1                 (0x03)
#define CFG_OPN_PD1_TPS66994BF      (0x01)
#define CFG_OPN_PD1_CCG8SF          (0x02)
#define CFG_OPN_PD1_RTK             (0x03)

#define CFG_OPS_RETIMER0            (4)
#define CFG_MSK_RETIMER0            (0x03)
#define CFG_OPN_RETIMER0_PS8835     (0x01)
#define CFG_OPN_RETIMER0_KB8010     (0x02)

#define CFG_OPS_RETIMER1            (6)
#define CFG_MSK_RETIMER1            (0x03)
#define CFG_OPN_RETIMER1_PS8835     (0x01)
#define CFG_OPN_RETIMER1_KB8010     (0x02)

#define CFG_OPS_RETIMER2            (8)
#define CFG_MSK_RETIMER2            (0x03)
#define CFG_OPN_RETIMER2_PS8835     (0x01)
#define CFG_OPN_RETIMER2_KB8010     (0x02)

#define CFG_OPS_DIMM                (10)
#define CFG_MSK_DIMM                (0x01)
#define CFG_OPN_DIMM_LpDDR5         (0x00)
#define CFG_OPN_DIMM_DDR5           (0x01)

#define _PLATFORM_CFG_              (brdCfg())
#define CHECK_CFG(X, Y)             (((_PLATFORM_CFG_ >> (CFG_OPS_##X)) & (CFG_MSK_##X)) == (CFG_OPN_##X##_##Y))

bool brdId_readBuf(uint32_t offset, uint32_t length, uint8_t *buf);
bool brdId_writeBuf(uint32_t offset, uint32_t length, uint8_t *buf);

#define BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET 0x93
#define BID_ACPI_SPACE_PAGE_BASE             0
#define BID_ACPI_SPACE_PAGE_SELECTOR_OFFSET  0x30

#define _H2C(p, b)      ((((p) >> ((b)*4)) & 0x0F) > 9) ? (((p) >> ((b)*4)) & 0x0F) + 0x37 : (((p) >> ((b)*4)) & 0x0F) + 0x30

int brdId_Init(void);
uint8_t acpi_brdId_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void acpi_brdIdExpField_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void brdId_ctrl_Ddr5SidimmPwrEn(bool pin);
bool brdId_supportMemPMIC(void);
bool brdId_supportDdr5(void);
void brdId_transitionToS5(void);
void brdId_pdPowerRoleG3(void);
void brdId_pdPowerRoleS5(void);
BID_MDS_CPU_TYPE brdId_CpuOpnType(void);

#endif /* __BOARD_ID_H__ */
