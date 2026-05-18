/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <soc.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "board_id.h"
#include "errcodes.h"
#include "app_pseq.h"
#include "acpi_region.h"
#include "f_nv_options.h"
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "app_ucsi_tunnel.h"
#include "app_usbc_task.h"
#include "board_pwrSrc.h"

LOG_MODULE_REGISTER(board, CONFIG_BOARD_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define BID_BUF_SIZE                         (256 + 32)
#define BID_MAX_PAGE_NUM                     (6)
#define BID_MAX_ECO_NUM                      (22)
#define BID_PAGE_LENGTH                      (0x30)
#define BID_MAX_BOARD_NAME_LENGTH            (0x20)
#define BID_MAX_MANFC_NAME_LENGTH            (0x20)
#define BID_BOARD_VER_OFFSET                 (13)
#define BID_EXPORTED_FIELD_LENGTH            (10)
#define BID_ACPI_SPACE_RD_OPS                (0x00)
#define BID_ACPI_SPACE_WE_OPS                (0x80)  /* write enable */
#define BID_ACPI_SPACE_WR_OPS                (0x40)  /* write trigger */
#define BID_ACPI_SPACE_PAGE_ID_MASK          (~(BID_ACPI_SPACE_WE_OPS + BID_ACPI_SPACE_WR_OPS))
#define BID_4CPTR_TO_U32(p)                  (*((uint32_t *)(p)))

#define _CLR_CFG_(X)       					_s_bidCfg &= (~((CFG_MSK_##X) << (CFG_OPS_##X)))
#define _SET_CFG_(X, Y)    					_s_bidCfg |= ((CFG_OPN_##X##_##Y) << (CFG_OPS_##X))

/* ************************** *
 *     static valuable        *
 * ************************** */
static uint32_t _s_bidCfg = 0;
static uint8_t _s_bidInitFlag = 0;

/*         SKU(0x44) hwLevel(0x45)
 *         ||        ||
 *         vv        vv
 * 102-D27012-00A REV31
 *     ^  ^     ^
 *     |  |     |
 *  pcbName     pcbRev(0x4E [7:4])
 */

#pragma pack(push,1)

typedef union {
	uint8_t arr[BID_BUF_SIZE];                       // 256 bytes in use; 32 bytes reserved; so the structure can fully cover 6 pages
 	struct {
		uint8_t pageBuf[BID_PAGE_LENGTH];
	} page[BID_MAX_PAGE_NUM];                        // 6 pages by each has 48 bytes.
	struct {
		uint8_t board[BID_MAX_BOARD_NAME_LENGTH];    // 0x00 - 0x1F
		uint8_t manfc[BID_MAX_MANFC_NAME_LENGTH];    // 0x20 - 0x3F
		uint8_t pcb[4];                              // 0x40 - 0x43 from ASKII Board[4 .. 7]
		uint8_t sku;                                 // 0x44        from Dec (Board[8 .. 9], MSB Board[8])
		uint8_t hwLevel;                             // 0x45        from Dec (Board[18 .. 19], MSB Board[18])
		uint8_t specRework[9];                       // 0x46 - 0x4E
		uint8_t memType;                             // 0x4F
		struct {
			uint32_t num;                            // [3 .. 0]    Display as ECO%06d
			uint8_t  flg[4];                         // [4 .. 7]    Display as %02X %02X %02X %02X
		} eco[BID_MAX_ECO_NUM];
		uint8_t rsvd[32];                            // rsvd[0 .. 4] holds contents of physical address 0x40 .. 0x44
	} f;
} T_BOARD_ID_SPACE;

typedef union {
	uint8_t arr[BID_EXPORTED_FIELD_LENGTH];
	struct {
		uint8_t pcbr  : 4; // byte0: pcb revision, 0 - '00A', 1 - '00B', 2 - '00C' ...
		uint8_t bid   : 4; // byte0: board id
		uint8_t sku;       // byte1: 
		uint8_t hwLevel;   // byte2: 
		uint8_t specRework[BID_EXPORTED_FIELD_LENGTH - 4]; // byte3-8
		uint8_t memType;   // byte9: memory id
	} f;
} T_BOARD_ID_EXPORTED_FIELD;

#pragma pack(pop)

static T_BOARD_ID_SPACE _s_bidBuf;
static T_BOARD_ID_EXPORTED_FIELD _s_bidExpField;

/**
 * @brief Transfer PCB ID to board id .
 *
 * @param pcb pointer to buffer holding the data.PCB name of EEPROM.
 * @retval BoardID.
 */
static uint8_t brdId_ProjectBoadId(uint8_t *pcb)
{
	uint8_t bid = 0;

	/* Board ID */
	if (BRDID_isTV) {
		/* Plum TV Rev-A, which has hardcoded board ID. */
		bid= 0x00;
	} else if (0 == memcmp(pcb, "\0\0\0\0", 4) ||
			   0 == memcmp(pcb, "\xFF\xFF\xFF\xFF", 4) ) {
		/* (null) */
		bid = 0x0E;
	} else if (0 == memcmp(pcb, "K012", 4)) {
		/* Plum RevB+ */
		bid = 0x01;
	} else if (0 == memcmp(pcb, "K016", 4)) { 
		/* Hickory */
		bid = 0x02;
	} else if (0 == memcmp(pcb, "K018", 4)) {
		/* Juniper */
		bid = 0x03;
	} else if (0 == memcmp(pcb, "K046", 4)) {
		/* Pine */
		bid = 0x04;
	} else if (0 == memcmp(pcb, "K019", 4)) {
		/* Dorne */
		bid = 0x05;
	} else if (0 == memcmp(pcb, "K013", 4)) {
		/* Plum DAP */
		bid = 0x06;
	} else if (0 == memcmp(pcb, "K017", 4)) {
		/* Hickory DAP*/
		bid = 0x07;
	} else if (0 == memcmp(pcb, "K047", 4)) {
		/* Juniper DAP*/
		bid = 0x08;
	} else if (0 == memcmp(pcb, "K028", 4)) {
		/*Hemlock */
		bid = 0x09;
	} else if (0 == memcmp(pcb, "K057", 4)) {
		/* Sequoia*/
		bid = 0x0A;
	} else if (0 == memcmp(pcb, "K079", 4)) {
		/* Aeris (Able)*/
		bid = 0x0C;
	} else {
		bid = 0x0F;                             // (unknown)
	}
	return bid;
}

/**
 * @brief update the board id original format to experted format
 */
static void brdId_update_exportedField(void)
{
	uint8_t pcbrev = 0;

	/* board id: EEPROM 0x40:0x43 */
	_s_bidExpField.f.bid = brdId_ProjectBoadId(_s_bidBuf.f.pcb);

	/* mid: EEPROM 0x4F */
	_s_bidExpField.f.memType = _s_bidBuf.f.memType;

	/* sku */
	_s_bidExpField.f.sku = _s_bidBuf.f.sku;

	/* hwLevel */
	_s_bidExpField.f.hwLevel = _s_bidBuf.f.hwLevel;

	/* rework, export only the 1st 7 bytes */
	for (uint8_t i = 0; i < sizeof (_s_bidExpField.f.specRework); i++) {
		_s_bidExpField.f.specRework[i] = _s_bidBuf.f.specRework[i];
	}
	
	/* Check the board version and compare with the spec rework */
	pcbrev = _s_bidBuf.f.board[BID_BOARD_VER_OFFSET];
	
	if (pcbrev >= 'A' && pcbrev <= 'P') {
		/**
		 * If the pcb version between A and P, so the maxinum value is 15,
		 * convert it to 0-based pcb revision.
		 */
		pcbrev -= 'A';
		_s_bidExpField.f.pcbr = pcbrev;
	} else {
		_s_bidExpField.f.pcbr = 0; /* treat as 'A' by default */
	}
}

/**
 * @brief update the config based on board id
 */
static void _mapBoardId2Config(void)
{
	_s_bidCfg = 0;

	/* able uses RTK PD and no need to detect */
	_SET_CFG_(PD0, RTK);
	_SET_CFG_(PD1, RTK);

	LOG_DBG ("detect rtk");
	/* force re-timer as PS8835 */
	_SET_CFG_(RETIMER0, PS8835);
	_SET_CFG_(RETIMER1, PS8835);

	/* force the LP DDR5 */
	_SET_CFG_(DIMM, LpDDR5);

	LOG_DBG("Board ID - Config: 0x%08X", _s_bidCfg);
}


/**
 * @brief load the board id info from eeprom
 *
 * @retval 0 if successful.
 */
static uint8_t _brdId_preLoad(void)
{
	uint8_t bid_pin = 0;
	memset(&_s_bidBuf, 0, BID_BUF_SIZE);

	/* able doesn't have board id rom. Need to full the buffer based on two GPIO pins */
	memcpy(_s_bidBuf.f.pcb, "K079", 4);
	_s_bidBuf.f.sku = 0;
	_s_bidBuf.f.hwLevel = 0;
	_s_bidBuf.f.memType = 0;

	brdId_update_exportedField();

	extern struct acpi_tbl g_acpi_tbl;
	for (uint8_t i=0;i<10;i++) {
		g_acpi_tbl.ACPI_BOARD_INFO[i] = _s_bidExpField.arr[i];
		LOG_DBG("_s_bidExpField.arr[%x] = %d",i,_s_bidExpField.arr[i]);
	}
	_s_bidInitFlag = 1;

	/* load board config */
	_mapBoardId2Config();

	/* check board id pins */
	bid_pin = (gpio_read_pin(BOARD_ID1) << 1) + gpio_read_pin(BOARD_ID0);

	//   +-------+-----------------------------------------------+
	//   | MemID | Description                                   |
	//   +-------+-----------------------------------------------+
	//   | 0x10  | K3KL9L9DM-MGCU @ 8533                         |
	//   | 0x20  | H58G66DK9BX067_9600_2R (Default)              |
	//   | 0x30  | K3KL8L80EM-MGCV                               |
	//   | 0x40  | K3KL9L90EM-MGCV                               |
	//   | 0x50  | K3KLALA0EM-MGCV                               |
	//   | 0x60  | H58G56DK9BX068                                |
	//   | 0x70  | H58G78DK9BX114                                |
	//   | 0x80  | MT62F1G32D2DS-020 WT:F                        |
	//   | 0x90  | MT62F2G32D4DS-020 WT:F                        |
	//   | 0xA0  | MT62F4G32D8DV-020 WT:F                        |
	//   +-------+-----------------------------------------------+
	switch (bid_pin) {
		case 0:
			/* LP5x - SK Hynix: H58G66DK9BX067 */
			_s_bidBuf.f.memType = 0x20;
			break;
		case 1:
			/* LP5x - Micron: MT62F2G32D4DS-020 WT D */
			_s_bidBuf.f.memType = 0x90;
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			break;
	}

	return 0;
}

/**
 * @brief return the board id data
 *
 * @retval board id data
 */
uint8_t brdId(void)
{
	if (!_s_bidInitFlag)
		_brdId_preLoad();

	return _s_bidExpField.f.bid;
}

/**
 * @brief return the SKU data
 *
 * @retval SKU data
 */
uint32_t brdCfg(void)
{
	if (!_s_bidInitFlag)
		_brdId_preLoad();

	return _s_bidCfg;
}

/**
 * @brief return the SKU data
 *
 * @retval SKU data
 */
uint8_t brdSku(void)
{
	if (!_s_bidInitFlag)
		_brdId_preLoad();

	return _s_bidExpField.f.sku;
}

/**
 * @brief return the Rev data
 *
 * @retval Rev data
 */
uint8_t brdRev(void)
{
	if (!_s_bidInitFlag)
		_brdId_preLoad();

	return _s_bidExpField.f.pcbr;
}

/**
 * @brief return the SR data
 *
 * @param     bitNum:     Special rework, where bitNum is zero-based bit index
 * @retval SR data
 */
uint8_t brdSR(uint8_t bitNum)
{
	return false;
}

/**
 * @brief check if it is PEO board
 *
 * @retval True means is PEO board
 */
bool brdId_isPEO(void)
{
	return false;
}

/**
 * @brief check if it is 28W board
 *
 * @retval True means is 28W board
 */
bool brdId_is28W(void)
{
	return true;
}

/**
 * @brief check if it is 45W board
 *
 * @retval True means is 45W board
 */
bool brdId_is45W(void)
{
	return false;
}

/**
 * @brief check if it is DM board
 *
 * @retval True means is DM board
 */
bool brdId_isDM(void)
{
	return true;
}

/**
 * @brief return the default CPU OPEN type based on board id
 *
 * @retval able only uses 28W CPU
 */
BID_MDS_CPU_TYPE brdId_CpuOpnType(void)
{
	LOG_INF("Board ID - OPN: %X", BID_MDS_CPU_TYPE_28W);
	return BID_MDS_CPU_TYPE_28W;
}

/**
 * @brief Read Board ID from buffer instead of EEPROM
 *
 * @param     offset:     read data offset from the struct
 * @param     length:     read data length
 * @param     buf:        the buffer to store read data
 * @retval True is successful
 */
bool brdId_readBuf (uint32_t offset, uint32_t length, uint8_t * buf)
{
	return true;
}

/**
 * @brief Write Board ID to buffer and set flag.
 *        acpi_brdId_Writer() is the only routine to write EEPROM
 *
 * @param     offset:     write data offset from the struct
 * @param     length:     write data length
 * @param     buf:        the buffer to store write data
 * @retval True is successful
 */
bool brdId_writeBuf (uint32_t offset, uint32_t length, uint8_t * buf)
{
	return true;
}

/**
 * @brief board id init
 *
 * @retval 0 is successful
 */
int brdId_Init(void)
{
	int ret =0;
	ret = _brdId_preLoad();
	if(ret) return ret;

	return 0;
}

/**
 * @brief change DDR5 related pin status based on board id
 *
 * @param     pin:     DDR5_SODIMM pin status
 */
void brdId_ctrl_Ddr5SidimmPwrEn(bool pin)
{

}

/**
 * @brief check if system support DDR5 based on board id
 *
 * @retval True is support DDR5
 */
bool brdId_supportMemPMIC(void)
{
	return false;
}

/**
 * @brief callback in power sequence transition from Sx to S5.
 */
void brdId_transitionToS5(void)
{
	/*
	 * APU_RST handler doesn't set M2_SSD0/M2_SSD1 anymore as of PLAT-66924/PLAT-65515.
	 * So turn it off here for S4/S5 case. This doesn't impact S0i3 sequence.
	 */
	gpio_write_pin(EC_SSD0_PWREN, 0);

}

/**
 * @brief callback in power sequence when system enter G3 for PD controller power role
 */
void brdId_pdPowerRoleG3(void)
{

}

/**
 * @brief callback in power sequence when system enter S5 for PD controller power role
 */
void brdId_pdPowerRoleS5(void)
{

}


/**
 * @brief Board id in acpi dump eeprom space
 *
 * @note this function is purely for debug
 */
void brdId_dumpEepromThroughEcUart0(void)
{

}

/**
 * @brief ACPI board handler
 *
 * @param     isRead:     1 is read and 0 is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   read and write data buffer
 * @retval 1 is successful
 */
uint8_t acpi_brdId_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	return 0;
}

/**
 * @brief Board id in acpi ec namespace
 *
 * @param isRead write permission.
 * @param ui8Idx acpi offset
 * @param pui8Data pointer to buffer holding the data.
 */
void acpi_brdIdExpField_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET && ui8Idx < BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET + BID_EXPORTED_FIELD_LENGTH) {
		if (isRead) {
			ui8Idx -= BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET;
			*pui8Data = _s_bidExpField.arr[ui8Idx];
		}
	}
}