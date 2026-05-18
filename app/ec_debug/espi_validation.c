/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "ec_debug.h"
// #include "sram.h"
#include "acpiplanes.h"
#include "espi_hub.h"
#include "espioob_mngr.h"
#include "f_nv_options.h"

#include <zephyr/drivers/espi.h>

#include <dev_utility.h>
#include <soc.h>
LOG_MODULE_REGISTER(ecdbgi, CONFIG_ECDBGI_LOG_LEVEL);

#define DT_DRV_COMPAT				microchip_xec_espi_v2
#define VW_BANK_BASE				DT_INST_REG_ADDR_BY_NAME(0, vw)
#define ESPI_DEV				DT_NODELABEL(espi0)

static const struct device *espi_dev;
static uintptr_t espi_regsbase = DT_REG_ADDR(DT_NODELABEL(espi0));

extern void npcx_host_init_shaw4_test_ram_base(uint32_t base);
extern int espi_npcx_bm_read(const struct device *dev,
			       struct espi_bm_packet *pckt, bool is64bit);
extern int espi_npcx_bm_write(const struct device *dev,
			       struct espi_bm_packet *pckt, bool is64bit);
extern uint8_t _s_eSpiSramBuf[256];

#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
#define APP_ESPI_PC_BM_BUFFER_SIZE      (80)

#pragma pack(push,1)
typedef struct {
	/* parameters for channel 0 */
	uint8_t triggerChTest : 1;   // byte 0
	uint8_t testStatus    : 7;

	uint8_t isRead        : 1;   // byte 1
	uint8_t               : 6;
	uint8_t use64bitAddr  : 1;

	uint8_t length;              // byte 2

	uint8_t rsv3;                // byte 3

	uint32_t BmHwStatus;         // byte 4/5/6/7

	uint32_t hostAddrLow;        // byte 8/9/10/11
	uint32_t hostAddrHigh;       // byte 12/13/14/15
} APP_ESPI_PC_BM_CTRL;
#pragma pack(pop)

static APP_ESPI_PC_BM_CTRL _s_pcBmCtrl[2];

#define APP_ESPI_BM_STATUS_TIMEOUT        (0x10000000)
/*
 * status checking bits
 *
 * [11] - BM1_BAD_REQUEST
 * [skip: 10] - reserved
 * [9] - BM1_INTERNAL_BUS_ERROR
 * [8] - BM1_FAIL
 * [7] - BM1_INCOMPLETE
 * [6] - BM1_DATA_OVERRUN
 * [5] - BM1_START_OVERFLOW
 * [4] - BM1_ABORTED_BY_CH2_ERROR
 * [3] - BM1_ABORTED_BY_HOST
 * [2] - BM1_ABORTED_BY_EC
 * [skip: 1] - BM1_BUSY
 * [0] - BM1_TRANSFER_DONE
 */
// #define APP_ESPI_BM_STATUS_ERROR_MASK      (c16bit(0000,1011,1111,1100))
// #define APP_ESPI_BM_STATUS_TRANSFER_DONE   (0x00000001)
// #define APP_ESPI_BM_ERR_NotInit            (0x01ul << 12)
// #define APP_ESPI_BM_ERR_IncorrectLength    (0x02ul << 12)
// #define APP_ESPI_BM_ERR_IncorrectHostAddr  (0x03ul << 12)
// #define APP_ESPI_BM_ERR_IncorrectInnerAddr (0x04ul << 12)
// #define APP_ESPI_BM_ERR_unsupportedChannel (0x05ul << 12)
// #define APP_ESPI_BM_ERR_BmIsNotEnabled     (0x0Ful << 12)
// #define APP_ESPI_BM_isTimeOut(x)    ((x) & APP_ESPI_BM_STATUS_TIMEOUT)
// #define APP_ESPI_BM_isBusErr(x)     ((x) & (APP_ESPI_BM_STATUS_ERROR_MASK | 0xF000))
// #define APP_ESPI_BM_isSuccess(x)    ((x) == APP_ESPI_BM_STATUS_TRANSFER_DONE)

// #define     espi_PcMasteringEnable          (regs->PCSTS & cbit(27))

K_TIMER_DEFINE(bmTimer, NULL, NULL);
#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */

#if CONFIG_ECDBGI_ESPI_FA_TEST
static __attribute__ ((aligned(4))) uint8_t _s_fa_pBuf[128];

#pragma pack(push,1)
typedef union {
	struct {
		uint8_t triggerChTest : 1;   /* byte 0 */
		uint8_t testStatus    : 7;
		uint8_t ops           : 4;   /* byte 1; 0 - EC read; 1 - EC write; 2 - erase */
		uint8_t tag           : 4;   /* byte 1;       RW (update by callback) */
		uint16_t length;             /* byte 2-3    ; RW */
		uint32_t flashAddr;          /* byte 4,5,6,7; RW */
		uint32_t timeout;            /* byte 8,9,a,b; RW */
		uint32_t status;             /* byte c,d,e,f; RO (update by callback) */
	} f;
	uint8_t arr[16];
} APP_ESPI_FA_CTRL;
#pragma pack(pop)

#define eSPI_FA_PAYLOAD_SIZE  (64)   /* or 128 if FA is able to support this length */
static APP_ESPI_FA_CTRL _s_faCtrl = { .f.triggerChTest = 0, .f.testStatus = 0, .f.ops = 0, .f.tag = 0, .f.length = 0x40, .f.flashAddr = 0x21000, .f.timeout = 0xFFFFFFFF};
#endif /* CONFIG_ECDBGI_ESPI_FA_TEST */

#if CONFIG_ECDBGI_ESPI_OOB_TEST
/*
 * !! NOTE for (RMB) !!
 * OOB channel upstream/downstream message data maximum size is 128 bytes.
 */
#define APP_ESPI_OOB_MASTER_MAX_PKG_SIZE  (80)
#define APP_ESPI_OOB_SLAVE_MAX_PKG_SIZE   (80)  /* Actual 73h, the default setting of EC in Config 30h (OOB Capabilities) */

__attribute__ ((aligned(4))) static uint8_t _s_oobRx[APP_ESPI_OOB_MASTER_MAX_PKG_SIZE];
__attribute__ ((aligned(4))) static uint8_t _s_oobTx[APP_ESPI_OOB_SLAVE_MAX_PKG_SIZE];

#define APP_ESPI_OOB_TX_TIMEOUT_FLAG      (0x80000000)
#define APP_ESPI_OOB_isChEnabled          (regs->ESPICFG & BIT(NPCX_ESPICFG_OOBCHANEN))   // bit2 - OOB_CHANNEL_ENABLED

#pragma pack(push,1)
typedef union {
	uint8_t arr[0x10];
	struct {
		uint8_t enableUserPkgHandler : 1;   // byte 0
		uint8_t                      : 7;

		uint8_t triggerUpstream      : 1;   // byte 1
		uint8_t TxStatus             : 7;

		uint8_t RxTag                : 4;   // byte 2
		uint8_t                      : 4;

		uint8_t RxLength;                   // byte 3

		uint8_t TxTag                : 4;   // byte 4
		uint8_t                      : 4;

		uint8_t TxLength             : 5;   // byte 5
		uint8_t                      : 3;

		uint16_t rsv6_7;

		uint16_t RxCycles;                  // byte 8/9
		uint16_t TxCycles;                  // byte 10/11

		uint16_t RxHwStatus;
		uint16_t TxHwStatus;

	} f;
} APP_ESPI_OOB_TEST_CTRL;
#pragma pack(pop)

#pragma pack(push,1)
typedef union {
	uint32_t pkgInfo;
	struct {
		uint32_t txPkgLen     : 8;
		uint32_t outgoingTag  : 4;
		uint32_t rsvd         : 20;
	} f;
} OUTGOING_PKG_INFO;
#pragma pack(pop)

typedef OUTGOING_PKG_INFO (* ESPI_OOB_PKG_HANDLER) (uint8_t incommingTag, uint8_t * pRxBuf, uint32_t rxPkgSize, uint8_t * pTxBuf, uint32_t txMaxSize);

static ESPI_OOB_PKG_HANDLER _s_pkgHandler = NULL;
static uint32_t _s_msgTxLen = 0;
static uint32_t _s_msgRxLen = 0;
static uint8_t _s_incomingTag = 0;
static APP_ESPI_OOB_TEST_CTRL _s_oobTestCtrl;
#endif /* CONFIG_ECDBGI_ESPI_OOB_TEST */

static struct k_work espiTest_work;
static volatile int espiTest_event;

/*
 * Bus master test proc
 */
#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
#ifndef APPAPP_ESPI_P_BM_INTERRUPT_MODE
#define APP_ESPI_P_BM_INTERRUPT_MODE (0)
#endif
#define APP_ESPI_P_BM_TIMEOUT        (10 * 1000)   /* 10ms timeout */

static bool _s_bmInitFlag = false;
static uint32_t _s_bmStatus[2] = {0, 0};
static struct k_mutex _s_chLock[2];

#if (APP_ESPI_P_BM_INTERRUPT_MODE)
static MD_IPC_SEM _s_chStSem[2] = {{0ul, 0ull}, {0ul, 0ull}};
static uint8_t _s_wdtId[2] = {MD_TIMER_INVALID_TIMER_ID, MD_TIMER_INVALID_TIMER_ID};

static void _espi_bm_timeout(uint32_t ch)
{
	GIRQ_Disable( IRQ19_IDX, 1ul + ch);     // Disable BMx_INTR
	_s_bmStatus[ch] |= APP_ESPI_BM_STATUS_TIMEOUT;

	LOG_DBG("BM failed on ch%d -- timeout", ch);
	md_ipc_signalSem(&_s_chStSem[ch]);
}

static void _ch0_wdt (void) { _espi_bm_timeout(0); }
static void _ch1_wdt (void) { _espi_bm_timeout(1); }

static void (*_ch_wdt_callback[2]) (void) = {
	_ch0_wdt, _ch1_wdt
};
#endif

/**
 * @brief Initialize bus master for eSPI peripheral channel
 */
void app_espi_p_bmInit(void)
{
	if (_s_bmInitFlag)
		return;

	_s_bmInitFlag = true;

	memset(_s_bmStatus, 0, sizeof(_s_bmStatus));
	k_mutex_init(&_s_chLock[0]);
	k_mutex_init(&_s_chLock[1]);
#if (APP_ESPI_P_BM_INTERRUPT_MODE)
	memset(_s_chStSem,  0, sizeof(_s_chStSem));

	for (uint32_t i = 0; i < 2; i++) {
		_s_wdtId[i] = md_timer_aloc(APP_ESPI_P_BM_TIMEOUT, _ch_wdt_callback[i], false, false);
		GIRQ_Disable(IRQ19_IDX, 1ul + i);
		GIRQ_ClrSrc (IRQ19_IDX, 1ul + i);
	}
#endif
}
#if 0
uint32_t app_espi_p_bmAccess(uint32_t ch, bool isRead, bool is64bitAddressing, uint32_t hostaddr_h, uint32_t hostaddr_l, uint32_t ecaddr, uint32_t len)
{
	struct espi_iom_regs *regs = (struct espi_iom_regs *)espi_regsbase;

	/*
	 * check init flag
	*/
	if (!_s_bmInitFlag) {
		LOG_DBG("BM failed on ch%d this module is not initilized", ch);
		return APP_ESPI_BM_ERR_NotInit;
	}

	/*
	 * !! NOTE for (RMB) !!
	 * Upstream memory read/write request max payload size is limited
	 * to 64 bytes, and not cross 64-bytes boundary
	 */
	if (len > 64 || !len) {
		LOG_DBG("BM failed on ch%d as length (%d) (len > 64 || !len)", ch, len);
		return APP_ESPI_BM_ERR_IncorrectLength;
	}

	/*
	 * if host address is zero or across 64-byte boundary
	 */
	if (!(hostaddr_h|hostaddr_l) || (((hostaddr_l)^(hostaddr_l + len - 1)) & 0x00000040ul)) {
		LOG_DBG("BM failed on ch%d as host address (%08X_%08X) is zero or across 64-byte boundary", ch, hostaddr_h, hostaddr_l);
		return APP_ESPI_BM_ERR_IncorrectHostAddr;
	}

	/*
	 * internal address need to be 4-byte aligned
	 */
	if (ecaddr & 0x03) {
		LOG_DBG("BM failed on ch%d as internal address (%08X) is not 4-byte aligned", ch, ecaddr);
		return APP_ESPI_BM_ERR_IncorrectInnerAddr;
	}

	/*
	 * Peripheral Channel Status Register bit 27 - PC_MASTERING_ENABLE_STATUS
	 * This bit is enabled through global reg 10h bit 2 - Bus Master Enable
	 */
	if (!espi_PcMasteringEnable) {
		LOG_DBG("BM failed on ch%d as BM is not enabled on P channel", ch);
		return APP_ESPI_BM_ERR_BmIsNotEnabled;
	}

	/*
	 * Only 2 channels are supported
	 */
	if (ch >= 2) {
		LOG_DBG("BM failed on ch%d as only 2 channels are supported", ch);
		return APP_ESPI_BM_ERR_unsupportedChannel;
	}

	/*
	 * status checking bits
	 *
	 * [11] - BM1_BAD_REQUEST
	 * [skip: 10] - reserved
	 * [9] - BM1_INTERNAL_BUS_ERROR
	 * [8] - BM1_FAIL
	 * [7] - BM1_INCOMPLETE
	 * [6] - BM1_DATA_OVERRUN
	 * [5] - BM1_START_OVERFLOW
	 * [4] - BM1_ABORTED_BY_CH2_ERROR
	 * [3] - BM1_ABORTED_BY_HOST
	 * [2] - BM1_ABORTED_BY_EC
	 * [skip: 1] - BM1_BUSY
	 * [0] - BM1_TRANSFER_DONE
	 */
	uint32_t stChk = APP_ESPI_BM_STATUS_ERROR_MASK | APP_ESPI_BM_STATUS_TRANSFER_DONE;

	if (ch) {              /* In case it is BM2 */
		stChk <<= 16;      /* Switch to high 16-bit for BM2 */
	}

	k_mutex_lock(&_s_chLock[ch], K_FOREVER);

	regs->BM_STATUS = stChk;         	/* Write to clear status */
	regs->BM_CONFIG = 0x000B000A;    	/* Hardcode BM1/BM2 TAG always as 000A/000B */
										/* the TAG field should not be modified while the bit of BMx_BUSY is 1 */
	if (ch) {
		regs->BM_HADDR2_LSW = hostaddr_l;
		regs->BM_HADDR2_MSW = (is64bitAddressing) ? hostaddr_h : 0;
		regs->BM_EC_ADDR2_LSW = ecaddr;
	}
	else {
		regs->BM_HADDR1_LSW = hostaddr_l;
		regs->BM_HADDR1_MSW = (is64bitAddressing) ? hostaddr_h : 0;
		regs->BM_EC_ADDR1_LSW = ecaddr;
	}

	/* [9:8] BM1_CYCLE_TYPE
	 *   - 11b = Memory Write, 64-bit addressing
	 *   - 10b = Memory  Read, 64-bit addressing
	 *   - 01b = Memory Write, 32-bit addressing
	 *   - 00b = Memory  Read, 32-bit addressing
	 */
	uint32_t cycleType = 0;
	if (is64bitAddressing)   cycleType |= cbit(9);
	if (!isRead)             cycleType |= cbit(8);

#if (APP_ESPI_P_BM_INTERRUPT_MODE)
	ENTER_CRITICAL_ZONE();

	/* Reset sem */
	_s_chStSem[ch].u32Sem  = 0;
	_s_chStSem[ch].pending = 0;

	/* Enable INTR */
	md_timer_enable(_s_wdtId[ch]);
	MMCR_32b (ESPI_BUS_MASTER_INT_EN) |= (0x01ul << (16 * ch));
	GIRQ_Enable(IRQ19_IDX, 1ul + ch);
#endif

	/*
	 * clear BM status
	 */
	_s_bmStatus[ch] = 0;

  /*
   * trigger BM start
   */
  if (ch) {
	  regs->BM_CTRL2 =
			(len  << 16)|       /* [28:16] length */
			(cycleType) |       /* [9:8] Cycle type */
			( 1   << 3 )|       /* Set to 1 so the transfer on this ch will be held until a transfer */
							    /* in progress of another channel has completed. If that transfer */
							    /* completes unsuccessfully, then this transfer will also terminate, */
							    /* before generating any traffic, with the BM1_ABORTED_BY_CH2_ERROR flag set. */
			( 1   << 2 )|       /* Enable internal incremented address so it can copy continues space in one batch */
			( 1   << 0 );       /* Start */
  }
  else {
	  regs->BM_CTRL1 =
			(len  << 16)|       /* [28:16] length */
			(cycleType) |       /* [9:8] Cycle type */
			( 1   << 3 )|       /* Set to 1 so the transfer on this ch will be held until a transfer */
							    /* in progress of another channel has completed. If that transfer */
							    /* completes unsuccessfully, then this transfer will also terminate, */
							    /* before generating any traffic, with the BM1_ABORTED_BY_CH2_ERROR flag set. */
			( 1   << 2 )|       /* Enable internal incremented address so it can copy continues space in one batch */
			( 1   << 0 );       /* Start */
  }

#if (APP_ESPI_P_BM_INTERRUPT_MODE)
	EXIT_CRITICAL_ZONE();

	do {
		md_ipc_waitSem(&_s_chStSem[ch]);

		_s_bmStatus[ch] &= 0xFFFF0000; /* so as to retrieve TIMEOUT bit */
		_s_bmStatus[ch] |= (((regs->BM_STATUS & stChk) >> (16 * ch)) & 0x0000FFFF);
	} while (!_s_bmStatus[ch]);

	/* Disable INTR */
	md_timer_disable(_s_wdtId[ch]);
	regs->BM_IEN &= ~ (0x01ul << (16 * ch));
	GIRQ_Disable(IRQ19_IDX, 1ul + ch);
#else
	//k_timer_start(&bmTimer, K_USEC(APP_ESPI_P_BM_TIMEOUT), K_NO_WAIT);
	k_timer_start(&bmTimer, K_MSEC(10), K_NO_WAIT);
	do {
		_s_bmStatus[ch] = (regs->BM_STATUS & stChk) >> (16 * ch);
		if ( _s_bmStatus[ch] ) {
			break;
		}
	} while (k_timer_status_get(&bmTimer) == 0);

	if (!_s_bmStatus[ch])
		_s_bmStatus[ch] = APP_ESPI_BM_STATUS_TIMEOUT;
#endif

	/*
	 * abort transmission if timeout
	 */
	if (APP_ESPI_BM_isTimeOut(_s_bmStatus[ch])) {
		if (ch) {
			regs->BM_CTRL2 = cbit(1); /* Set BMx_ABORT */
		}
		else {
			regs->BM_CTRL1 = cbit(1); /* Set BMx_ABORT */
		}
	}

	/*
	 * check if only bit 0 (BMx_TRANSFER_DONE) is asserted
	 */
	if (APP_ESPI_BM_isSuccess(_s_bmStatus[ch])) {
		LOG_DBG("BM upstream %s success on ch%d, %d bytes has copied.", isRead ? "read" : "write", ch, len);
	} else {
		LOG_DBG("BM failed on ch%d - St: %08X", ch, _s_bmStatus[ch]);

		LOG_DBG("  ----- BM status (%04X) -----", _s_bmStatus[ch]);
		if(_s_bmStatus[ch] & APP_ESPI_BM_STATUS_TIMEOUT)
			LOG_DBG("  b31 - BM timeout (%d us)", APP_ESPI_P_BM_TIMEOUT);
		if(_s_bmStatus[ch] & cbit(11))     LOG_DBG("  b11 - BM_BAD_REQUEST");
		if(_s_bmStatus[ch] & cbit( 9))     LOG_DBG("  b9  - BM_INTERNAL_BUS_ERROR");
		if(_s_bmStatus[ch] & cbit( 8))     LOG_DBG("  b8  - BM_FAIL");
		if(_s_bmStatus[ch] & cbit( 7))     LOG_DBG("  b7  - BM_INCOMPLETE");
		if(_s_bmStatus[ch] & cbit( 6))     LOG_DBG("  b6  - BM_DATA_OVERRUN");
		if(_s_bmStatus[ch] & cbit( 5))     LOG_DBG("  b5  - BM_START_OVERFLOW");
		if(_s_bmStatus[ch] & cbit( 4))     LOG_DBG("  b4  - BM_ABORTED_BY_CH2_ERROR");
		if(_s_bmStatus[ch] & cbit( 3))     LOG_DBG("  b3  - BM_ABORTED_BY_HOST");
		if(_s_bmStatus[ch] & cbit( 2))     LOG_DBG("  b2  - BM_ABORTED_BY_EC");
		if(!(_s_bmStatus[ch] & cbit( 0)))  LOG_DBG(" (b0) - BM_TRANSFER_DONE is not asserted");
	}

	k_mutex_unlock(&_s_chLock[ch]);
	return _s_bmStatus[ch];
}
#endif
#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */

#if CONFIG_ECDBGI_ESPI_FA_TEST
#if ESPI_FLASH_OPERATION_CALLBACK_SUPPORTED
/**
 * @brief flash access channel operation done
 *
 * @param     tag:        FA tag
 * @param     status:     FA status
 */
static void _fa_doneCallback (uint8_t tag, uint32_t status)
{
	_s_faCtrl.f.tag = tag;
	_s_faCtrl.f.status = status;
	_s_faCtrl.f.testStatus = app_espi_fa_statusCheck(tag, status) ? 0x7F : 0x71; // 0x7F for success or 0x71 (0xE2 at 8-bit view) for fail

	app_espi_fa_clear();
}
#endif

#define ESPI_FLASH_ERASE_DUMMY		0x01ul

/**
 * @brief trigger flash access channel test
 *
 * @param     ops:    operation code
 */
void app_espi_fa_testTrigger(uint8_t ops)
{
	_s_faCtrl.f.triggerChTest = 0;
	_s_faCtrl.f.testStatus = 0;

#if ESPI_FLASH_OPERATION_CALLBACK_SUPPORTED
	bool ret = app_espi_fa_postRequest ((APP_ESPI_FA_OPS)ops, _s_faCtrl.f.tag, _s_faCtrl.f.timeout, _s_faCtrl.f.flashAddr, _s_faCtrl.f.length, _s_fa_pBuf, _fa_doneCallback, true);
	if (!ret) {
		_s_faCtrl.f.testStatus = 0x71; /* 0xE2 for 1st byte */
	}
#else
	struct espi_flash_packet pckt;
	int ret = 0;

	switch (ops) {
		case 0: /* read */
			for (uint16_t sent = 0; sent < _s_faCtrl.f.length; sent += eSPI_FA_PAYLOAD_SIZE) {
				pckt.buf = ((uint8_t *)_s_fa_pBuf) + sent;
				pckt.flash_addr = _s_faCtrl.f.flashAddr + sent;
				pckt.len = eSPI_FA_PAYLOAD_SIZE;
				ret = espi_read_flash(espi_dev, &pckt);
			}
			break;
		case 1: /* write */
			for (uint16_t sent = 0; sent < _s_faCtrl.f.length; sent += eSPI_FA_PAYLOAD_SIZE) {
				pckt.buf = ((uint8_t *)_s_fa_pBuf) + sent;
				pckt.flash_addr = _s_faCtrl.f.flashAddr + sent;
				pckt.len = eSPI_FA_PAYLOAD_SIZE;
				ret = espi_write_flash(espi_dev, &pckt);
			}
			break;
		case 2: case 3: /* erase */
			pckt.buf = NULL;
			pckt.flash_addr = _s_faCtrl.f.flashAddr;
			pckt.len = ESPI_FLASH_ERASE_DUMMY;
			ret = espi_flash_erase(espi_dev, &pckt);
			break;
		default:
			break;
	}

	if (ret) {
		_s_faCtrl.f.testStatus = 0x71; /* 0xE2 for 1st byte */
	}
#endif
}
#endif /* CONFIG_ECDBGI_ESPI_FA_TEST */

#if CONFIG_ESPI_OOB_CHANNEL
/**
 * @brief callback in oob reset or init
 */
void app_espi_oob_reset(void)
{
	/*
	 * Reset control variables
	 */
	_s_msgTxLen = 0;
	_s_msgRxLen = 0;
	_s_incomingTag = 0;

	/*
	 * Reset test ctrl
	 */
	memset(&_s_oobTestCtrl, 0, sizeof(_s_oobTestCtrl));
	_s_oobTestCtrl.f.enableUserPkgHandler = 1;
}

/**
 * @brief oob packet handler and decoder
 *
 * @param     incommingTag:    new tag
 * @param     pRxBuf:          Rx buffer pointer
 * @param     rxPkgSize:       Rx length
 * @param     pTxBuf:          Tx buffer pointer
 * @param     txMaxSize:       Tx max length
 * @return    status
 */
OUTGOING_PKG_INFO app_oob_pkgHandler (uint8_t incommingTag, uint8_t * pRxBuf, uint32_t rxPkgSize, uint8_t * pTxBuf, uint32_t txMaxSize)
{
	OUTGOING_PKG_INFO ret;

	ret.f.outgoingTag = incommingTag;
	ret.f.txPkgLen = rxPkgSize;

	for (uint32_t i = 0; i < ((rxPkgSize < txMaxSize) ? rxPkgSize : txMaxSize); i++) {
		pTxBuf[i] = (pRxBuf[i] + 1);
	}

	return ret;
}

/**
 * @brief ACPI handler for the out of band channel test
 *
 * @param     pPkgHandler:     register the oob packet handler pointer
 */
void app_espi_oob_registerPkgHandler(ESPI_OOB_PKG_HANDLER pPkgHandler)
{
	_s_pkgHandler = pPkgHandler;
}

/**
 * @brief Hook function for eSPI OOB test buffer processing
 *
 * @param     rx_tag:     Received tag
 * @param     length:     Data length
 * @param     rx:         Pointer to received OOB packet
 */
void hook_espi_oob_test_buffer(uint8_t rx_tag, uint16_t length, struct espi_oob_packet *rx)
{
	// Copy RX buffer to _s_oobRx, so that user can check data in EC ram
	memset(_s_oobRx, 0, APP_ESPI_OOB_MASTER_MAX_PKG_SIZE);
	memcpy(_s_oobRx,  rx->buf, rx->len);

	// rx->buf[0] = SMBus Target Address
	// rx->buf[1] = SMBus Command
	// rx->buf[2] = SMBus Byte Count
	// rx->buf[3] = SMBus Data Byte 0
	// rx->buf[4] = SMBus Data Byte 1

	// bit 0 = 1 means read
	rx->buf[0] |= 0x01;
	// Plus 1 to each bye for test
	for (uint16_t i = 0; i < rx->buf[2]; i++) {
		rx->buf[i+3] = (rx->buf[i+3] + 1);
	}
	memcpy(_s_oobTx,  rx->buf, rx->len);
	_s_oobTestCtrl.f.RxLength = (uint8_t) length;
	_s_oobTestCtrl.f.RxTag = rx_tag;
	_s_oobTestCtrl.f.RxCycles ++;

	/*
	 * downstream message has received in _s_oobRx
	 */
//#if (LOG_Enhanced_SPI)
//	LOG_DBG("Received downstream package (%d bytes), TAG 0x%02X", _s_msgRxLen, _s_incomingTag);
//	LOG_CLEARBUF;
//	for (uint32_t i = 0; i < _s_msgRxLen; i++) {
//		LOG_APPEND(" %02X", _s_oobRx[i]);
//	}
//	LOG_DBG("  RxBuf: %s", LOG_BUF);
//#endif
}

/**
 * @brief tigger the oob channel tx only test
 */
void app_espi_oob_testTrigger(void)
{
	uint32_t outgoingTag = 0;
	struct espi_reg *regs = (struct espi_reg *)espi_regsbase;
	struct espi_oob_packet pckt;
	int ret = 0;

	// _s_oobTestCtrl.f.RxHwStatus = (uint16_t)regs->OOBRXSTS; // HwStatus may be altered by the ISR

	/*
	 * User test has triggered
	 */
	if (_s_oobTestCtrl.f.triggerUpstream) {
		_s_oobTestCtrl.f.triggerUpstream = 0;

		outgoingTag = _s_oobTestCtrl.f.TxTag;
		_s_msgTxLen = _s_oobTestCtrl.f.TxLength;
	}

	/*
	 * check if any pending TX message
	 */
	if (_s_msgTxLen && APP_ESPI_OOB_isChEnabled) {
		/*
		 * Send upstream message
		 */
//#if (LOG_Enhanced_SPI)
//		LOG_DBG("Send upstream package (%d bytes), TAG 0x%02X", _s_msgTxLen, outgoingTag);
//		LOG_CLEARBUF;
//		for (uint32_t i = 0; i < _s_msgTxLen; i++) {
//			LOG_APPEND(" %02X", _s_oobTx[i]);
//		}
//		LOG_DBG("  TxBuf: %s", LOG_BUF);
//#endif

		pckt.buf = _s_oobTx;
		pckt.len = _s_msgTxLen;
		pckt.tag = outgoingTag;

		ret = oob_test_send_async(&pckt, NULL);
		if (!ret) {
			LOG_DBG("eSPI oob tx test successfully");
			_s_oobTestCtrl.f.TxCycles++;
			_s_oobTestCtrl.f.TxStatus = 0x68; /* 0xD0 in byte view */
		} else {
			LOG_DBG("eSPI oob tx test unsuccessfully");
			_s_oobTestCtrl.f.TxStatus = ret;
		}

		// _s_oobTestCtrl.f.TxHwStatus = (uint16_t)regs->OOBTXSTS; // HwStatus may be altered by the ISR
	}
}
#endif /* end of #if CONFIG_ESPI_OOB_CHANNEL */

/**
 * @brief event to trigger espi test
 *
 * @param     w:    work item pointer
 */
void _eSpiTestEvent(struct k_work *w)
{
	int evt = espiTest_event;

	switch (evt) {
#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
	case 0: 
	case 1: // bm
		for (uint32_t i = 0; i < 2; i++) {
			if (_s_pcBmCtrl[i].triggerChTest) {
				_s_pcBmCtrl[i].triggerChTest = 0;

				uint32_t isRead = _s_pcBmCtrl[i].isRead ? 1 : 0;
				uint32_t length = _s_pcBmCtrl[i].length;
				uint32_t hostMemAddrHigh = _s_pcBmCtrl[i].hostAddrHigh;
				uint32_t hostMemAddrLow  = _s_pcBmCtrl[i].hostAddrLow;

				uint32_t ret = 0;
				struct espi_bm_packet pckt;
				if (isRead) {
					pckt.buf = (uint8_t *)&_s_eSpiSramBuf[0x10 + (0x80 * i)];
					pckt.mem_addr = ((uint64_t)hostMemAddrHigh << 32) + hostMemAddrLow;
					pckt.len = length;
					ret = espi_npcx_bm_read(
							espi_dev, 
							&pckt,
							_s_pcBmCtrl[i].use64bitAddr ? true : false);
				} else {
					pckt.buf = (uint8_t *)&_s_eSpiSramBuf[0x10 + (0x80 * i)];
					pckt.mem_addr = ((uint64_t)hostMemAddrHigh << 32) + hostMemAddrLow;
					pckt.len = length;
					ret = espi_npcx_bm_write(
							espi_dev, 
							&pckt,
							_s_pcBmCtrl[i].use64bitAddr ? true : false);
				}
				_s_pcBmCtrl[i].testStatus = (ret == 0) ? 0x61 : 0x60;
				_s_pcBmCtrl[i].BmHwStatus = ret;
				LOG_DBG("eSpiBusMaster ch%d run %s", i,
					(ret == 0) ? "Success" : "Fail");
			}
		}
		break;
#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */

#if CONFIG_ECDBGI_ESPI_FA_TEST
	case 0x10:   /* fa */
		app_espi_fa_testTrigger(_s_faCtrl.f.ops);
		break;
#endif
#if CONFIG_ECDBGI_ESPI_OOB_TEST
	case 0x20:   /* oob */
		app_espi_oob_testTrigger();
		break;
#endif
	default:
		break;
	}
}

/**
 * @brief espi validation test init
 */
void espi_validation_init(void)
{
	espi_dev = DEVICE_DT_GET(ESPI_DEV);
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("ESPI Device not ready");
		return;
	}

#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
	memset((void *)_s_eSpiSramBuf, 0xFF, sizeof(_s_eSpiSramBuf));
	for (uint8_t i = 0; i < APP_ESPI_PC_BM_BUFFER_SIZE; i++) {
		_s_eSpiSramBuf[i + 0x10] = i;
		_s_eSpiSramBuf[i + 0x90] = (i | 0x80);
	}
	memset((void *)_s_pcBmCtrl, 0, sizeof(_s_pcBmCtrl));
	_s_pcBmCtrl[1].hostAddrLow = _s_pcBmCtrl[0].hostAddrLow = 0x04000000;
	_s_pcBmCtrl[1].length = _s_pcBmCtrl[0].length = 64;
	_s_pcBmCtrl[1].isRead = 1;
	app_espi_p_bmInit();

#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */

#if CONFIG_ESPI_OOB_CHANNEL
	/* register OOB package handler */
	app_espi_oob_registerPkgHandler(app_oob_pkgHandler);
#if CONFIG_OOB_MSG_MNGR
	register_oob_hndlr(OOB_MASTER_ADDR_FCH, espi_fch_oob_rx_handler);
#else
	register_oob_hndlr(OOB_MASTER_ADDR_FCH, espi_fch_oob_rx_legacy_handler);
#endif
#endif

	/* Flash access channel quick test */
#if CONFIG_ECDBGI_ESPI_FA_TEST
	for (uint8_t i = 0; i < sizeof (_s_fa_pBuf); i++) {
		_s_fa_pBuf[i] = 0x80 | i;
	}
#endif

	/* create eSPI test trigger event */
	k_work_init(&espiTest_work, _eSpiTestEvent);
}

/********************************************************
 * eSPI ACPI Handlers
 ********************************************************/
#define ACPI_SPACE_SIZE   (MD_ACPI_HYPERPLANE_REGION_END - MD_ACPI_HYPERPLANE_REGION_START + 1)

/**
 * @brief ACPI handler for the espi set/get configuration test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */

uint8_t app_espi_cfg_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint32_t nvOption;
	static uint32_t ecramBar;
	static uint32_t testRamBar;
	uint8_t u8tmp;

	if (isRead) {
		switch (ui8Idx) {
			case 0: /* channel support flags */
				u8tmp = 0;
				GET_NV_OPTIONS(espi_peripheralSupport, nvOption);
				u8tmp |= ((nvOption) ? 0x01 : 0x00);
				GET_NV_OPTIONS(espi_virtialWireSupport, nvOption);
				u8tmp |= ((nvOption) ? 0x02 : 0x00);
				GET_NV_OPTIONS(espi_OobSupport, nvOption);
				u8tmp |= ((nvOption) ? 0x04 : 0x00);
				GET_NV_OPTIONS(espi_FlashAccSupport, nvOption);
				u8tmp |= ((nvOption) ? 0x08 : 0x00);

				*pui8Data = u8tmp;
				break;
			case 1: /* {maxSpeed, mode} */
				u8tmp = 0;
				GET_NV_OPTIONS(espi_modeSupport, nvOption);
				u8tmp = nvOption;
				GET_NV_OPTIONS(espi_maxSpeed, nvOption);
				u8tmp |= (nvOption << 4);

				*pui8Data = u8tmp;
				break;
			case 11: /* espi_mmio_ecram_bar */
				GET_NV_OPTIONS(espi_mmio_ecram_bar, ecramBar);
			case 10:
			case 9:
			case 8:
				*pui8Data = 0xFF & (ecramBar >> (8 * (ui8Idx - 8)));
				break;
			case 15: /* espi_mmio_test_bar */
				GET_NV_OPTIONS(espi_mmio_test_bar, testRamBar);
			case 14:
			case 13:
			case 12:
				*pui8Data = 0xFF & (testRamBar >> (8 * (ui8Idx - 12)));
				break;

			default:
				*pui8Data = 0xFF;
				break;
		}
	} else {
		switch (ui8Idx) {
			case 0: /* channel support flags */
				u8tmp = *pui8Data;

				SET_NV_OPTIONS(espi_peripheralSupport,  !!(u8tmp & 0x01));
				SET_NV_OPTIONS(espi_virtialWireSupport, !!(u8tmp & 0x02));
				SET_NV_OPTIONS(espi_OobSupport,         !!(u8tmp & 0x04));
				SET_NV_OPTIONS(espi_FlashAccSupport,    !!(u8tmp & 0x08));
				break;
			case 1: /* {maxSpeed, mode} */
				u8tmp = *pui8Data;

				SET_NV_OPTIONS(espi_modeSupport, (u8tmp & 0x0F));
				SET_NV_OPTIONS(espi_maxSpeed,    (u8tmp >> 4));
				break;

			case 8: /* espi_mmio_ecram_bar */
				ecramBar = 0;
			case 9:
			case 10:
			case 11:
				ecramBar |= ((uint32_t)(*pui8Data)) << (8 * (ui8Idx - 8));
				if (11 == ui8Idx)
					SET_NV_OPTIONS(espi_mmio_ecram_bar, ecramBar);
				break;

			case 12: /* espi_mmio_test_bar */
				testRamBar = 0;
			case 13:
			case 14:
			case 15:
				testRamBar |= ((uint32_t)(*pui8Data)) << (8 * (ui8Idx - 12));
				if (15 == ui8Idx) {
					SET_NV_OPTIONS(espi_mmio_test_bar, testRamBar);
					npcx_host_init_shaw4_test_ram_base(testRamBar);
				}
				break;

			default:
				break;
		}
	}

	return 1;
}

#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
/**
 * @brief ACPI handler for the peripheral channel test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */
uint8_t app_espi_pc_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint8_t pgSel = 0;
	uint32_t addr;

	if (ui8Idx == 0x30) {

		if (isRead)             *pui8Data = pgSel;
		else if (*pui8Data < 4) pgSel = *pui8Data;
		return 1;

	} else if (ui8Idx < 0x30) {
		addr = pgSel * 0x30 + ui8Idx;

		/*
		 * buffer 0
		 */
		if (addr >= 0x10 && addr < 0x10 + APP_ESPI_PC_BM_BUFFER_SIZE) {
			if (isRead)          *pui8Data = _s_eSpiSramBuf[addr];
			else                 _s_eSpiSramBuf[addr] = *pui8Data;
		}

		/*
		 * buffer 1
		 */
		if (addr >= 96 + 0x10 && addr < 96 + 0x10 + APP_ESPI_PC_BM_BUFFER_SIZE) {
			if (isRead)          *pui8Data = _s_eSpiSramBuf[addr - 96 + 0x80];
			else                 _s_eSpiSramBuf[addr - 96 + 0x80] = *pui8Data;
		}

		/*
		 * Ctrl block
		 */
		if ((pgSel == 0 || pgSel == 2) && ui8Idx < 0x10) {
			uint32_t s = (pgSel == 0) ? 0 : 1;
			uint8_t * p = (uint8_t *)&_s_pcBmCtrl[s];
			uint8_t wasEn = _s_pcBmCtrl[s].triggerChTest;

			if (isRead)          *pui8Data = p[ui8Idx];
			else                 p[ui8Idx] = *pui8Data;

			if (!isRead) {
				if (_s_pcBmCtrl[s].triggerChTest && !wasEn) {
					_s_pcBmCtrl[s].testStatus = 0;
					/* trigger BM test start */
					espiTest_event = s;
					k_work_submit(&espiTest_work);
				}
			}
		}
	}

	return 1;
}
#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */

/**
 * @brief ACPI handler for the virtual wire channel test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */
uint8_t app_espi_vw_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint8_t pgSel = 0;
	uint32_t addr;

	if (ui8Idx == 0x30) {

		if (isRead)                *pui8Data = pgSel;
		else if (*pui8Data < 14)   pgSel = *pui8Data;
		return 1;

	} else {

		addr = pgSel * 0x30 + ui8Idx;
		if ((addr >= 0x100 && addr < 0x127) || (addr >= 0x140 && addr < 0x16C)) {  // S2M or M2S
			if (isRead) {
				*pui8Data = GET_FIELD(*(volatile uint32_t *)(espi_regsbase + addr - (addr % 4)),
					  FIELD(8 * (addr % 4), 8));
			} else {
				SET_FIELD(*(volatile uint32_t *)(espi_regsbase + addr - (addr % 4)),
					  FIELD(8 * (addr % 4), 8), *pui8Data);
			}
		}
		return 1;

	}
}

/**
 * @brief ACPI handler for the virtual wire aggr channel test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */
uint8_t app_espi_vw_aggr_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	uint8_t i = (ui8Idx >> 1);
	struct espi_reg *regs = (struct espi_reg *)espi_regsbase;

	if ( i >= 0 && i <= 11 ) {  // M2S
	
		if (!(ui8Idx & 0x01)) {   // INDEX
			if (isRead) {
				*pui8Data = GET_FIELD(regs->VWEVMS[i],
							NPCX_VWEVMS_INDEX);
			} else {
				SET_FIELD(regs->VWEVMS[i], NPCX_VWEVMS_INDEX, *pui8Data);
			}
		} else {                  // VW_St
			if (isRead) {
				uint8_t temp = GET_FIELD(regs->VWEVMS[i],
							NPCX_VWEVMS_WIRE);
				uint8_t pinSt = 0;

				if (temp & 0x1) pinSt |= 0x01;
				if (temp & 0x2) pinSt |= 0x02;
				if (temp & 0x4) pinSt |= 0x04;
				if (temp & 0x8) pinSt |= 0x08;

				*pui8Data = pinSt;
			} else {
				uint8_t pinSt = *pui8Data;
				uint32_t temp = 0;

				if (pinSt & 0x01)  temp |= 0x1;
				if (pinSt & 0x02)  temp |= 0x2;
				if (pinSt & 0x04)  temp |= 0x4;
				if (pinSt & 0x08)  temp |= 0x8;

				if (GET_FIELD(regs->VWEVMS[i], NPCX_VWEVMS_WIRE) != temp) {
					SET_FIELD(regs->VWEVMS[i], NPCX_VWEVMS_WIRE, *pui8Data);
				}
			}
		}

		return 1;
	}

	if ( i >= 12 && i <= 21 ) {   /* S2M */
		i -= 11;                  /* to zero based */

		if (!(ui8Idx & 0x01)) {   /* INDEX */
			if (isRead) {
				*pui8Data = GET_FIELD(regs->VWEVSM[i],
							NPCX_VWEVSM_INDEX);
			} else {
				SET_FIELD(regs->VWEVSM[i], NPCX_VWEVSM_INDEX, *pui8Data);
			}
		} else {                  /* VW_St */
			if (isRead) {
				uint32_t temp = GET_FIELD(regs->VWEVSM[i],
							NPCX_VWEVSM_WIRE);
				uint8_t pinSt = 0;

				if (temp & 0x1) pinSt |= 0x01;
				if (temp & 0x2) pinSt |= 0x02;
				if (temp & 0x4) pinSt |= 0x04;
				if (temp & 0x8) pinSt |= 0x08;

				*pui8Data = pinSt;
			} else {
				uint8_t pinSt = *pui8Data;
				uint32_t temp = 0;

				if (pinSt & 0x01)  temp |= 0x1;
				if (pinSt & 0x02)  temp |= 0x2;
				if (pinSt & 0x04)  temp |= 0x4;
				if (pinSt & 0x08)  temp |= 0x8;

				if (GET_FIELD(regs->VWEVSM[i], NPCX_VWEVSM_WIRE) != temp) {
					SET_FIELD(regs->VWEVSM[i], NPCX_VWEVSM_WIRE, *pui8Data);
				}
			}
		}

		return 1;
	}

	return 0;
}

#if CONFIG_ECDBGI_ESPI_FA_TEST
/**
 * @brief ACPI handler for the flash access channel test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */
uint8_t app_espi_fa_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	struct espi_reg *regs = (struct espi_reg *)espi_regsbase;

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint8_t pgSel = 0;
	uint32_t addr;

	if (ui8Idx == 0x30) {

		if (isRead)                *pui8Data = pgSel;
		else if (*pui8Data < 10)   pgSel = *pui8Data;
		return 1;

	} else if (ui8Idx < 0x30) {

		addr = pgSel * 0x30 + ui8Idx;
		if (addr >= 0x30 && addr < 128 + 0x30) {  /* S2M or M2S */
			if (isRead) {
				*pui8Data = _s_fa_pBuf[addr - 0x30];
			} else {
				_s_fa_pBuf[addr - 0x30] = *pui8Data;
			}
		} else if (addr < 16) {
			if (isRead) {
				*pui8Data = _s_faCtrl.arr[addr];
			} else {
				_s_faCtrl.arr[addr] = *pui8Data;

				if (0 == addr && _s_faCtrl.f.triggerChTest) {
					espiTest_event = 0x10;
					k_work_submit(&espiTest_work);
				}
			}
		} else {
			if (isRead) {
				switch (addr) {
					case 16: case 17: case 18: case 19:
						/* RO - ESPI_FLASH_CH_STATUS        ( ESPI_BASE + 0x2A0 ) */
						// *pui8Data = (regs->FCSTS     >> ((addr - 17) * 8)) & 0xFF;
						break;
					case 21: case 22: case 23: case 24:
						/* RO - ESPI_FLASH_CH_CONFIG        ( ESPI_BASE + 0x34 ) */
						*pui8Data = (regs->FLASHCFG     >> ((addr - 21) * 8)) & 0xFF;
						break;
					case 25: case 26: case 27: case 28:
						/* RO - ESPI_FLASH_CH_CONTROL       ( ESPI_BASE + 0x38 ) */
						*pui8Data = (regs->FLASHCTL    >> ((addr - 25) * 8)) & 0xFF;
						break;
					case 29: case 30:	case 31: case 32:
						/* RO - ESPI_FLASH_CH_INT_ENABLE    ( ESPI_BASE + 0x298 ) */
						// *pui8Data = (regs->FCIEN    >> ((addr - 29) * 8)) & 0xFF;
						break;
					default:
						break;
				}
			}
		}

		return 1;

	}

	return 0;
}
#endif /* CONFIG_ECDBGI_ESPI_FA_TEST */

#if CONFIG_ECDBGI_ESPI_OOB_TEST
/**
 * @brief ACPI handler for the oob channel test
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 * @return    true if successfully
 */
uint8_t app_espi_oob_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint8_t pgSel = 0;
	uint32_t addr;

	if (ui8Idx == 0x30) {

		if (isRead)             *pui8Data = pgSel;
		else if (*pui8Data < 4) pgSel = *pui8Data;
		return 1;

	} else if (ui8Idx < 0x30) {
		addr = pgSel * 0x30 + ui8Idx;

		/*
		 * RX buffer
		 */
		if (addr >= 0x10 && addr < 0x10 + APP_ESPI_OOB_MASTER_MAX_PKG_SIZE) {
			if (isRead)          *pui8Data = _s_oobRx[addr - 0x10];
			else                 _s_oobRx[addr - 0x10] = *pui8Data;
		}

		/*
		 * TX buffer
		 */
		if (addr >= 96 + 0x10 && addr < 96 + 0x10 + APP_ESPI_OOB_MASTER_MAX_PKG_SIZE) {
			if (isRead)          *pui8Data = _s_oobTx[addr - 96 - 0x10];
			else                 _s_oobTx[addr - 96 - 0x10] = *pui8Data;
		}

		/*
		 * Ctrl block
		 */
		if ((pgSel == 0 || pgSel == 2) && ui8Idx < 0x10) {
			uint8_t wasEn = _s_oobTestCtrl.f.triggerUpstream;

			if (isRead)          *pui8Data = _s_oobTestCtrl.arr[ui8Idx];
			else                 _s_oobTestCtrl.arr[ui8Idx] = *pui8Data;

			if (!isRead) {
				if (_s_oobTestCtrl.f.triggerUpstream && !wasEn) {
					_s_oobTestCtrl.f.TxStatus = 0;
					espiTest_event = 0x20;
					k_work_submit(&espiTest_work);
				}
			}
		}
	}

	return 1;
}
#endif /* CONFIG_ECDBGI_ESPI_OOB_TEST */