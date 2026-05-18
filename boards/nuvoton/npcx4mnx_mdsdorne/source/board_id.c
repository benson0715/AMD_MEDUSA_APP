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
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#include "app_ucsi_tunnel.h"
#include "app_usbc_task.h"
#include "board_pwrSrc.h"
#include "oob_misc_msg.h"

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
static struct k_work  _s_bid_work;
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
 * Page ID       Physical Addr       LOG_INFical Addr        Page Size
 *      00       0x0000 ~ 0x002F     0x0000 ~ 0x002F     0x30
 *      01       0x3000 ~ 0x302F     0x0030 ~ 0x005F     0x30
 *      02       0x2000 ~ 0x202F     0x0060 ~ 0x008F     0x30
 *      03       0x1000 ~ 0x102F     0x0090 ~ 0x00BF     0x30
 *      04       0x0400 ~ 0x042F     0x00C0 ~ 0x00EF     0x30
 *      05       0x0500 ~ 0x052F     0x00F0 ~ 0x011F     0x30
 */

static uint16_t _s_bidAddrMapping[] = {
	0x0000, 0x0030, 0x0060, 0x0090, 0x00C0, 0x00F0
};

/* ************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_epromPageSelector = 0;
uint8_t g_epromUpdateFlag   = 0;      /* bitmap to pages */
uint8_t g_epromWriteTrigger = 0;

/**
 * @brief read board id eeprom based on page id
 *
 * @param     isRead:     True is read and false is write
 * @param     pageId:     The eeprom page id
 * @param     pBuf32:     Read and write data buffer
 * @retval 0 if successful.
 */
static int brdId_pageAccess(bool isRead, uint8_t pageId, void * pBuf32)
{
	assert (pageId < BID_MAX_PAGE_NUM);

	int ret;
	if (isRead) {
		ret = eeprom_read_block(_s_bidAddrMapping[pageId], BID_PAGE_LENGTH, pBuf32);
	} else {
		ret = eeprom_write_block(_s_bidAddrMapping[pageId], BID_PAGE_LENGTH, pBuf32);
	}

	return ret;

}

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
static void _mapBoardId2Config()
{
	_s_bidCfg = 0;

	/* eFFP uses TI PD and no need to detect */
	_SET_CFG_(PD0, TPS66994BF);
	_SET_CFG_(PD1, TPS66994BF);
	LOG_DBG ("detect tps");

	if (BRDID_isTV) {
		_SET_CFG_(RETIMER0, PS8835);
		_SET_CFG_(RETIMER1, PS8835);

		LOG_DBG("TV Board ID - Config: 0x%08X", _s_bidCfg);
		return;
	}

	if (BRDID_isDorne) {
		_SET_CFG_(RETIMER0, PS8835);
		_SET_CFG_(RETIMER1, PS8835);

		_SET_CFG_(DIMM, LpDDR5);
	}

	LOG_DBG("Board ID - Config: 0x%08X", _s_bidCfg);
}


/**
 * @brief load the board id info from eeprom
 *
 * @retval 0 if successful.
 */
static uint8_t _brdId_preLoad(void)
{
	memset(&_s_bidBuf, 0, BID_BUF_SIZE);
	for (uint8_t i = 0; i < BID_MAX_PAGE_NUM; i++) {
		brdId_pageAccess(1, i, &_s_bidBuf.page[i].pageBuf[0]);
		LOG_HEXDUMP_DBG(&_s_bidBuf.page[i].pageBuf[0], BID_PAGE_LENGTH, "EEPROM_HEX_DUMP: ");
	}
	brdId_update_exportedField();

	extern struct acpi_tbl g_acpi_tbl;
	for (uint8_t i=0;i<10;i++) {
		g_acpi_tbl.ACPI_BOARD_INFO[i] = _s_bidExpField.arr[i];
		LOG_DBG("_s_bidExpField.arr[%x] = %d",i,_s_bidExpField.arr[i]);
	}
	_s_bidInitFlag = 1;

	/* load board config */
	_mapBoardId2Config();

	/* set memory info type */
	if (brdId_supportMemPMIC()) {
		oob_misc_mem_info_type_set(MEM_INFO_TYPE_DDR5);
	} else {
		oob_misc_mem_info_type_set(MEM_INFO_TYPE_LPDDR5);
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
uint32_t brdCfg()
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
	assert (bitNum < 72);

	if (!_s_bidInitFlag)
		_brdId_preLoad();

  /** As the limited size of _s_bidExpField, Not all special rework bits
   * are exported to that space, so use the original buffer _s_bidBuf
   * in this function.
   */
	return (_s_bidBuf.f.specRework[bitNum / 8] & (1 << (bitNum % 8))) ? 1 : 0;
}

/**
 * @brief check if it is PEO board
 *
 * @retval True means is PEO board
 */
bool brdId_isPEO(void)
{
	bool isPeo = false;
	// uint8_t sku = brdSku();

	if (!BRDID_isDAP) {
		// if ( BRDID_isDorne && 4 == sku ) {
		//	isPeo = true;
		// }
	}

	LOG_INF("Board ID - isPEO: %s", isPeo ? "YES" : "NO");
	return isPeo;
}

/**
 * @brief check if it is 28W board
 *
 * @retval True means is 28W board
 */
bool brdId_is28W()
{
	bool is28W = false;
	uint8_t sku = brdSku();

	// 3. PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10 NO APU, 28W Max, LPDDR5x No Pop
	// 4. PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10, 28W DM, LPDDR5x No Pop
	if (3 == sku || 4 == sku) {
		is28W = true;
	}

	LOG_DBG("Board ID - is 28W: %s", is28W  ? "YES" : "NO");
	return is28W;
}

/**
 * @brief check if it is 45W board
 *
 * @retval True means is 45W board
 */
bool brdId_is45W()
{
	bool is45W = false;
	uint8_t sku = brdSku();

	// 1. PCBA: Dorne, Rev A, eFFP, MDS FP10 NO APU, 45W Max, LPDDR5x No Pop
	// 2. PCBA: Dorne, Rev A, eFFP, MDS FP10, 45W DM, LPDDR5x No Pop
	if (1 == sku || 2 == sku) {
		is45W = true;
	}

	LOG_DBG("Board ID - is 45W: %s", is45W  ? "YES" : "NO");
	return is45W;
}

/**
 * @brief check if it is DM board
 *
 * @retval True means is DM board
 */
bool brdId_isDM(void)
{
	bool isDM = false;
	uint8_t sku = brdSku();

	/*
	 * SKU2: PCBA: Dorne, Rev A, eFFP, MDS FP10, 45W DM, LPDDR5x No Pop
	 * SKU4: PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10, 28W DM, LPDDR5x No Pop
	 * */
	if (5 == brdId()) {
		if (sku == 2 || sku == 4) {
			isDM = true;
		}
	}

	LOG_INF("Board ID - isMD: %s", isDM ? "YES" : "NO");
	return isDM;
}

/**
 * @brief return the default CPU OPEN type based on board id
 *
 * @retval default CPU OPN for DM only (not DM return NA)
 */
BID_MDS_CPU_TYPE brdId_CpuOpnType(void)
{
	BID_MDS_CPU_TYPE opn = BID_MDS_CPU_TYPE_NA;
	uint8_t sku = brdSku();

	/*
	 * SKU1: PCBA: Dorne, Rev A, eFFP, MDS FP10 No APU, 45W Max, LPDDR5x No Pop
	 * SKU2: PCBA: Dorne, Rev A, eFFP, MDS FP10, 45W DM, LPDDR5x No Pop
	 * SKU3: PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10 NO APU, 28W Max, LPDDR5x No Pop
	 * SKU4: PCBA: Dorne_Lite, Rev A, eFFP, MDS FP10, 28W DM, LPDDR5x No Pop
	 * */
	if (5 == brdId()) {
		if (sku == 4) {
			opn = BID_MDS_CPU_TYPE_28W;
		} else if (sku == 2) {
			opn = BID_MDS_CPU_TYPE_45W;
		}
	}
	else {
		/* If cannot get valid board id, return NA */
		LOG_INF("Board ID - Invalid bid. ");
	}

	LOG_INF("Board ID - OPN: %X", opn);
	return opn;
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
	if (!_s_bidInitFlag)
		return false;

	if (offset + length <= BID_BUF_SIZE) {
		for (uint32_t i = 0; i < length; i++, offset++) {
			buf[i] = _s_bidBuf.arr[offset];
		}

		return true;
	}

	return false;
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
	if (!_s_bidInitFlag)
		return false;

	uint8_t changedPages = 0;
	if (offset + length <= BID_BUF_SIZE) {
		for (uint32_t i = 0; i < length; i++, offset++) {
			if (_s_bidBuf.arr[offset] != buf[i]) {
				_s_bidBuf.arr[offset] = buf[i];
				changedPages |= ( 1 << (offset / BID_PAGE_LENGTH) );
			}
		}

		if (changedPages) {
			g_epromUpdateFlag |= changedPages;
			g_epromWriteTrigger = 1;
			k_work_submit(&_s_bid_work);
		}

		return true;
	}

	return false;
}

/**
 * @brief write the board id EEPROM.
 */
static void _brdId_Writer(void)
{
	if (g_epromWriteTrigger) {
		uint16_t offset = 0;

		/* In LOG_INFic, EC define BID area from 0 to 255 as 6 page with 48 btyes in each page.
		 * However, in EEPROM physical level, page write can only operation in one page.
		 * If page write overflow one page, it will loop back from the beginning.
		 * EEPROM one page size is 64. So the start address for each page is 0x00, 0x40, 0x80, 0xC0
		 * Page Write: Address high, Address low, byte0, byte1, ........ byte63
		 */
		for (uint8_t i = 0; i < 4; i++) {
			/* Start address i*0x40. length 0x40 */
			eeprom_write_block(offset,0x40,&_s_bidBuf.arr[(uint8_t)offset]);
			offset += 0x40;
		}

		/* Clear flags */
		g_epromUpdateFlag = 0;
		g_epromWriteTrigger = 0;

		/* Update the exproted field as well */
		brdId_update_exportedField();
	}
}

/**
 * @brief board id event call back
 *
 * @param w pointer to work item
 */
static void _brdId_eventCallback (struct k_work *w)
{
	_brdId_Writer();
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

	k_work_init(&_s_bid_work, _brdId_eventCallback);
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
	ioexp_set(ex_EP_SSD0_PWREN, 0); // IO_7 = 0
}

/**
 * @brief callback in power sequence when system enter G3 for PD controller power role
 */
void brdId_pdPowerRoleG3(void)
{
#if CONFIG_USBC_4CC
	/* Set TI PD controller to sink only mode before enter G3. As it cannot provide VBUS in G3 */
	if ((CHECK_CFG(PD0, TPS66994BF) || CHECK_CFG(PD1, TPS66994BF)) &&
		(g_PdAdapterConnected == false) && board_pwrSrc_getCurrentPowerSource() != BOARD_PWR_PD) {
		app_4cc_sinkOnlyAll();
	}
#endif
}

/**
 * @brief callback in power sequence when system enter S5 for PD controller power role
 */
void brdId_pdPowerRoleS5(void)
{
#if CONFIG_USBC_4CC
	if (CHECK_CFG(PD0, TPS66994BF) || CHECK_CFG(PD1, TPS66994BF)) {
		/* Recover disabled ports */
		app_4cc_recoverPorts();
	}
#endif
}


/**
 * @brief Board id in acpi dump eeprom space
 *
 * @note this function is purely for debug
 */
void brdId_dumpEepromThroughEcUart0()
{
#if 0
	uint8_t buffer[BID_PAGE_LENGTH];
	uint32_t acks = 0;
	uint32_t length = BID_PAGE_LENGTH;
	uint8_t retry;
	board_uart_enable();

	for (uint16_t p = 0; p < 0x4000; p += BID_PAGE_LENGTH) {
		retry = 5;
		do {
			if (p + BID_PAGE_LENGTH > 0x4000)
				length = 0x10;

			acks = f_nv_options_NvStorageAccess(1, p, buffer, length);
		} while (length + 2 != acks && --retry > 0);

		// print address
		_UART_PUTC_(_H2C(p, 3));
		_UART_PUTC_(_H2C(p, 2));
		_UART_PUTC_(_H2C(p, 1));
		_UART_PUTC_(_H2C(p, 0));
		_UART_PUTC_(':');
		_UART_PUTC_(' ');

		// print the line (0x30 bytes each)
		if (!retry) {
			_UART_PUTC_('E');
			_UART_PUTC_('R');
			_UART_PUTC_('R');
			_UART_PUTC_('O');
			_UART_PUTC_('R');
			_UART_PUTC_('!');
		} else {
			for (uint32_t i = 0; i < length; i++) {
				_UART_PUTC_(_H2C(buffer[i], 1));
				_UART_PUTC_(_H2C(buffer[i], 0));
				_UART_PUTC_(' ');

				if (length == BID_PAGE_LENGTH && (0x0F == i || 0x1F == i)) {
					_UART_PUTC_('\x0A');
					_UART_PUTC_(_H2C(p + i + 1, 3));
					_UART_PUTC_(_H2C(p + i + 1, 2));
					_UART_PUTC_(_H2C(p + i + 1, 1));
					_UART_PUTC_(_H2C(p + i + 1, 0));
					_UART_PUTC_(':');
					_UART_PUTC_(' ');
				}
			}
		}

		_UART_PUTC_('\x0A');
	}

	_UART_PUTC_('!');
	_UART_PUTC_('!');
	_UART_PUTC_('D');
	_UART_PUTC_('O');
	_UART_PUTC_('N');
	_UART_PUTC_('E');
	_UART_PUTC_('!');
	_UART_PUTC_('!');
#endif
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
	if (ui8Idx == BID_ACPI_SPACE_PAGE_SELECTOR_OFFSET) {
		if (isRead) {
			if (g_epromWriteTrigger)
				g_epromPageSelector |= BID_ACPI_SPACE_WR_OPS;
			else
				g_epromPageSelector &= ~BID_ACPI_SPACE_WR_OPS;

			*pui8Data = g_epromPageSelector;
		} else {
			g_epromPageSelector = *pui8Data;

			/* Purely for debug */
			if (0xEC == g_epromPageSelector) {
				brdId_dumpEepromThroughEcUart0();
				g_epromPageSelector = 0x66;
			}

			if (g_epromPageSelector & BID_ACPI_SPACE_WR_OPS) {
				g_epromWriteTrigger = 1;
				k_work_submit(&_s_bid_work);

				/* Clear WE to avoid continues write */
				g_epromPageSelector &= ~BID_ACPI_SPACE_WE_OPS;
			}
		}

		return 1;
	} else {
		uint8_t pageIdx = BID_ACPI_SPACE_PAGE_ID_MASK & g_epromPageSelector;
		uint8_t pageofs = ui8Idx - BID_ACPI_SPACE_PAGE_BASE;

		if ( pageIdx < BID_MAX_PAGE_NUM && pageofs < BID_PAGE_LENGTH ) {
			if (isRead) {
				*pui8Data = _s_bidBuf.page[pageIdx].pageBuf[pageofs];
			} else if (_s_bidBuf.page[pageIdx].pageBuf[pageofs] != *pui8Data && (BID_ACPI_SPACE_WE_OPS & g_epromPageSelector) == BID_ACPI_SPACE_WE_OPS){
				_s_bidBuf.page[pageIdx].pageBuf[pageofs] = *pui8Data;
				g_epromUpdateFlag |= (1 << pageIdx);
			}

			return 1;
		}
	}

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

/**
 * @brief check if board revision is C
 *
 * @retval True if board revision is C
 */
bool brdId_isRevC()
{
	/* 'C' - 'A' = 2 */
	// if (brdRev() == 2)
	// 	return true;
	// else
		return false;
}