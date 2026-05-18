/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "app_spiS0.h"
#include "acpiplanes.h"
#include "app_pseq.h"
#include <zephyr/drivers/espi.h>
#include "u_misc.h"
#include "f_nv_options.h"
#include "dev_utility.h"
#include "gpio_ec.h"
#include "board_config.h"

LOG_MODULE_REGISTER(s0s_test, CONFIG_S0S_TEST_LOG_LEVEL);

#ifndef LOG_SPIS0_ON
#define LOG_SPIS0_ON                  1
#endif

#if (LOG_SPIS0_ON)
#define LOGS0S(frmt, ...)             LOG_INF(frmt, ##__VA_ARGS__)
#else
#define LOGS0S(frmt, ...)             while(0){}
#endif

#define ESPI_DEV      DT_NODELABEL(espi0)

void app_spiS0_testEnable(void);
void app_spiS0_testDisable(void);

static const struct device *espi_dev;
static struct k_timer _s_tidSpiTest;
static struct k_timer _s_tidSpiTegr;
static enum system_power_state _s_lastPwrSt = SYSTEM_INIT_STATE;
static struct k_work _s_evtIdBM_work;
static volatile int _s_evtIdBM_event;
uint8_t g_ui8ESPIFaProcessing = 0;

#define APP_SPIS0_BLANK_CHKSUM  0x87654321
static uint32_t _s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
#define SPI_BA_VW_TIMEOUT     (500 * 1000) // 500ms, used only for VW case
#endif

#define S0S_TEST_BY_PHYSICAL_PINS  0
#define S0S_TEST_BY_VIRTUAL_PINS   1
#define S0S_TEST_BY_MAFS           2

#define isMAFS                    (S0S_TEST_BY_MAFS == _s_spiS0test.testType)

#define ESPI_FLASH_ACCESS_TIMEOUT            100 // each eSPI Flash Access transaction needs 100ms timeout
/* While issuing flash erase command, it should be ensured that the transfer
 * length specified is non-zero.
 */
#define ESPI_FLASH_ERASE_DUMMY		0x01ul
typedef enum {
	S0S_DISABLED = 0,
	S0S_IDLE,
	S0S_STARTED,
	S0S_REQ_SENT,
	S0S_GRANTED,
	S0S_REQ_RELEASED,
	S0S_REQ_NEXT_BATCH
} APP_S0S_ST;

#pragma pack(push, 1)

static char * _s_description[] = {
	"DISA",  // S0S_DISABLED
	"IDLE",  // S0S_IDLE,
	"STAR",  // S0S_STARTED
	"REQS",  // S0S_REQ_SENT,
	"GNTD",  // S0S_GRANTED,
	"RELD",  // S0S_REQ_RELEASED
	"NEXT"   // S0S_REQ_NEXT_BATCH
};

typedef enum {
	S0S_TESTER_IDLE = 0,
	S0S_TESTER_CYCLE_STARTS,
	S0S_TESTER_ERASE_ALL,

	S0S_TESTER_WRITE,
	S0S_TESTER_WRITE_IN_PROGRESS,
	S0S_TESTER_WRITE_DONE,

	S0S_TESTER_READ,
	S0S_TESTER_READ_IN_PROGRESS,
	S0S_TESTER_READ_DONE,

	S0S_TESTER_READ_IN_PAGE,
} APP_S0S_TESTER_STATUS;

static APP_S0S_TESTER_STATUS _s_emTesterSt = S0S_TESTER_IDLE;

typedef struct {
	// 0-1
	uint16_t testInterval; // in millisecond, the rest of this struct can be overrrided only if testInterval is 0
	// 2-3
	uint16_t spiAddr23_8;  // field [23:8] of the start address, assuming [7:0] is zero.
	// 4-5
	uint8_t spiBlkSize_PwrOf2;       // power of 2; indicate the block size; default 0x0C, i.e. 2^^12 = 4096
	uint8_t enableWriteTest     : 1; // 0 - only read spi; 1 - erasure, write then read to confirm
	uint8_t useFixPattern4Write : 1; // 0 - to use pseudo-random numbers array; 1 - to use fixed pattern
	uint8_t testType            : 2; // 0 - to use physical REQ/GNT; 1 - to use VW REG/GNT; 2 - MAFS by eSPI FA
	uint8_t rsvd_               : 4;

	// 6-7
	uint16_t blkNum;  	   // how many spi block will be handled in the test (both R and W case)
	// 8-11
	uint32_t testPattern;  // the test pattern if useFixPattern4Write = 1
	// 12-15
	uint32_t curChkSum;    // Current checksum of the test region
	// 0x10-0x13, 0x14-0x17
	uint32_t testCycles;   // how many test cycles had been finished
	uint32_t errorCnt;     // how many failed cycles (by comparing check sum)
	// 0x18-0x1B
	APP_S0S_ST emSt;       // Current status

	// 0x1C-0x1F
	// no-use

	// 0x20-0x23
	// 4 x ASCII of APP_S0S_ST

	// 0x2F
} APP_S0S_HELPER;

#pragma pack(pop)

static APP_S0S_HELPER _s_spiS0test;

static void _s_spi_setStatus(APP_S0S_ST s)
{
	//LOGS0S("%s     ==> %s", _s_description[_s_spiS0test.emSt], _s_description[s])
	_s_spiS0test.emSt = s;
}
static APP_S0S_ST _s_spi_getStatus()
{
	return _s_spiS0test.emSt;
}

static void _s_spi_reqire()
{
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	if (!isMAFS)
		f_spi_ba_busRequire(SPI_BA_VW_TIMEOUT);
#endif
}

static void _s_spi_release()
{
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	if (!isMAFS)
		f_spi_ba_busRelease(SPI_BA_VW_TIMEOUT);
#endif
}

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
#define SHD_CS_PIN_CTRL_REG_IDX (GPIO_Pin2Addr(SHD_CS) - GPIO_MMCR_BASE)/4

static void _s_spi_dummpCycle ()
{
	//
	//  GPIO055/PWM2/SHD_CS#/(RSMRST#)
	//
	struct gpio_regs *regs = (struct gpio_regs *)(GPIO_MMCR_BASE);
	uint32_t origSetting = regs->CTRL[SHD_CS_PIN_CTRL_REG_IDX];

	// set to OD_0
	gpio_configure_pin( SHD_CS,
					MD_GPIO_OUTPUT_LOW |
					MD_GPIO_MUX_GPIO |
					MD_GPIO_OUTPUT_SELECT_PINCTRL16 |
					MD_GPIO_DIRECTION_OUTPUT |
					MD_GPIO_EM_BUF_TYPE__OpenDrain |
					MD_GPIO_INTERRUPT_DISABLED |
					MD_GPIO_PAD_NONE );

	k_sleep(K_USEC(20));           // wait for 20 us

	gpio_write_pin(SHD_CS, 1);      // set back to 1

	// restore GPIO055 settings
	regs->CTRL[SHD_CS_PIN_CTRL_REG_IDX] = origSetting;
}
#endif

static void _spi_S0_test_trigger (struct k_timer *timer)
{
	if (S0S_IDLE == _s_spi_getStatus()) {
		_s_spi_setStatus(S0S_STARTED);
	}
}

#define _SPI_S0_STOP_ON_FAIL          (0)    // for debug purpose, stress will stop on any failure

#define _SPI_S0_TEST_BUFFER_SIZE      (256)
#define _SPI_S0_eSPI_FA_PAYLOAD_SIZE  (64)   // or 128 if FA is able to support this length
#define _SPI_S0_eSPI_FA_TIMEOUT       (10000)// 10s
static __attribute__ ((aligned(256))) uint8_t _s_testBuf[_SPI_S0_TEST_BUFFER_SIZE];
extern uint8_t f_qmspi_read_status(void);

#define TSTR_SET_NEXT(x)     _s_emTesterSt = (x)

void _spi_helper_dumpInfo( uint32_t seed, uint32_t idx, uint32_t spiAddr )
{
	int key = irq_lock();

	//
	// LOG read buffer
	//
	LOGS0S("Cycle %d, ERROR: s %08X, ErrIdx %3d, spiAddr %06X, readBuf:", _s_spiS0test.testCycles, seed, idx, spiAddr);
	for ( uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i++ ) {
		if (i % 16 == 0) {
			if (i != 0) {
				LOGS0S("  %02X: %s", i - 16, LOG_BUF);
			}
			LOG_CLEARBUF;
		}
		LOG_APPEND("%02X ", _s_testBuf[i]);
	}
	LOGS0S("  %02X: %s", 0xF0, LOG_BUF);

	//
	// LOG data pattern
	//
	u_misc_pr_genRandArray((uint16_t)seed, _s_testBuf, _SPI_S0_TEST_BUFFER_SIZE);
	LOGS0S("Data pattern by seed %08X:", seed);
	for ( uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i++ ) {
		if (i % 16 == 0) {
			if (i != 0) {
				LOGS0S("  %02X: %s", i - 16, LOG_BUF);
			}
			LOG_CLEARBUF;
		}
		LOG_APPEND("%02X ", _s_testBuf[i]);
	}
	LOGS0S("  %02X: %s", 0xF0, LOG_BUF);

#if (_SPI_S0_STOP_ON_FAIL)
	//
	// deadloop...
	//
	LOG_ERR("MAFS stress test fail ... reconnect AC power for recovery");
	uint32_t duty = 0;
	while(1) {
		duty = (duty + 1) % 150;

		for (uint32_t p = 0; p < 100; p++) {
			if (p < duty) {
				k_sleep(K_USEC(10));
			} else {
				k_sleep(K_USEC(10));
			}
		}
	}
#else
	irq_unlock(key);
#endif
}

void _spi_helper_read(uint32_t addr, uint16_t len, void * pBuf)
{
	int ret = 0;
	uint8_t loop_cnt;
	struct espi_flash_packet pckt;

	if (isMAFS) {
		for (uint16_t sent = 0; sent < len; sent += _SPI_S0_eSPI_FA_PAYLOAD_SIZE) {
			loop_cnt = ESPI_FLASH_ACCESS_TIMEOUT;
			do {
				pckt.buf = ((uint8_t *)pBuf) + sent;
				pckt.flash_addr = addr + sent;
				pckt.len = _SPI_S0_eSPI_FA_PAYLOAD_SIZE;
				ret = espi_read_flash(espi_dev, &pckt);
				if (ret) {
					LOG_ERR("Read  spiAddr %06X gets error! (idx %d), retry ...", addr + sent, sent);
				} else {
					break;
				}
				k_msleep(1);
				loop_cnt--;
			} while (loop_cnt > 0);
		}
	} else {
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
		//
		// !!NOTE!! the buffer of DMA (pBuf) need to be aligned.
		//
		QMSPI_CMD cmd = {
				.OpCode = 0x6b,     // Fast Quad Read 1-1-4
				.CmdLines = 0,      // 1x
				.AddrLines = 0,     // 1x
				.DataLines = 2,     // 4x
				.FourBytesAddrMode = 0,
				.UseModeByte = 0,
				.ModeByte = 0,
				.DummyCycleNum = 4
			};

		md_qmspi_doRead(cmd.OpCode, addr, len, (uint8_t *)pBuf);
#endif
	}
}

void _spi_helper_write(uint32_t addr, uint16_t len, void * pBuf)
{
	int ret = 0;
	uint8_t loop_cnt;
	struct espi_flash_packet pckt;

	if (isMAFS) {
		for (uint16_t sent = 0; sent < len; sent += _SPI_S0_eSPI_FA_PAYLOAD_SIZE) {
			loop_cnt = ESPI_FLASH_ACCESS_TIMEOUT;
			do {
				pckt.buf = ((uint8_t *)pBuf) + sent;
				pckt.flash_addr = addr + sent;
				pckt.len = _SPI_S0_eSPI_FA_PAYLOAD_SIZE;
				ret = espi_write_flash(espi_dev, &pckt);
				if (ret) {
					LOG_ERR("Write spiAddr %06X gets error! (idx %d), retry ...", addr + sent, sent);
				} else {
					break;
				}
				k_msleep(1);
				loop_cnt--;
			} while (loop_cnt > 0);
		}
	} else {
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
		WriteEnable(1);
		qmspi_write_dma(0, addr, len, (uint32_t)pBuf);
		WriteEnable(0);
		while((f_qmspi_read_status()&0x01));
#endif
	}
}

void _spi_helper_erase(uint32_t addr)
{
	int ret = 0;
	uint8_t loop_cnt = ESPI_FLASH_ACCESS_TIMEOUT;
	struct espi_flash_packet pckt;

	if (isMAFS) {
		do {
			pckt.buf = NULL;
			pckt.flash_addr = addr;
			pckt.len = ESPI_FLASH_ERASE_DUMMY;
			ret = espi_flash_erase(espi_dev, &pckt);
			if (ret) {
				LOG_ERR("Erase spiAddr %06X gets error! retry ...", addr);
			} else {
				break;
			}
			k_msleep(1);
			loop_cnt--;
		} while (loop_cnt > 0);
	} else {
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
		WriteEnable(1);
		qmspi_page_erase(0x00, addr);
		WriteEnable(0);
		while((f_qmspi_read_status()&0x01));
#endif
	}
}

bool _spi_s0_sharing_tester()
{
	static uint32_t startAddr;
	static uint32_t endAddr;   // end address plus 1
	static uint32_t curPageStart;
	static uint32_t pageSize;
	static uint32_t numOfBlk;
	static uint32_t seed;
	static uint32_t errDetected = 0;

	bool thisCycleDone = false;

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	if (!isMAFS) {
		md_qmspi_init();
	}
#endif

	switch (_s_emTesterSt) {
		case S0S_TESTER_IDLE:
			startAddr = 0;
			endAddr   = 0;
			errDetected = 0;

			TSTR_SET_NEXT(S0S_TESTER_CYCLE_STARTS);
			break;

		case S0S_TESTER_CYCLE_STARTS:
			startAddr = ((uint32_t)_s_spiS0test.spiAddr23_8) << 8;
			pageSize  = 1ul << _s_spiS0test.spiBlkSize_PwrOf2;
			numOfBlk  = _s_spiS0test.blkNum;
			endAddr   = startAddr + pageSize * numOfBlk;

			if (_s_spiS0test.enableWriteTest) {
				TSTR_SET_NEXT(S0S_TESTER_ERASE_ALL);
			} else {
				TSTR_SET_NEXT(S0S_TESTER_READ);
			}
			break;

		case S0S_TESTER_ERASE_ALL:
			{
				// eSPI Flash Access is processing
				if(g_ui8ESPIFaProcessing) {
					break;
				}
//				LOGS0S("cycle %d: erase all start", _s_spiS0test.testCycles);
				for (curPageStart = startAddr; curPageStart < endAddr; curPageStart += pageSize) {
					_spi_helper_erase(curPageStart);
				}
//				LOGS0S("cycle %d: erase all done", _s_spiS0test.testCycles);
			}
			TSTR_SET_NEXT(S0S_TESTER_WRITE);
			break;

		case S0S_TESTER_WRITE:
			curPageStart = startAddr;
			seed = (uint32_t)k_uptime_ticks();       // update seed only a write cycle is start
			if (!seed) seed += 0x0001;       // seed can never be zero

			// generate test pattern
			if (_s_spiS0test.useFixPattern4Write) {
				for (uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i += 4) {
					uint32_t * pU32 = (uint32_t *)&_s_testBuf[i];
					*pU32 = _s_spiS0test.testPattern;
				}
			} else {
				u_misc_pr_genRandArray((uint16_t)seed, _s_testBuf, _SPI_S0_TEST_BUFFER_SIZE);
			}
//			LOGS0S("cycle %d: write start", _s_spiS0test.testCycles);
			TSTR_SET_NEXT(S0S_TESTER_WRITE_IN_PROGRESS);
			break;

		case S0S_TESTER_WRITE_IN_PROGRESS:
			{
				if (curPageStart >= endAddr) {
					TSTR_SET_NEXT(S0S_TESTER_WRITE_DONE);
					break;
				}

				// eSPI Flash Access is processing
				if(g_ui8ESPIFaProcessing) {
					break;
				}

				// update one page in a batch
				for (uint32_t offset = 0; offset <= pageSize - _SPI_S0_TEST_BUFFER_SIZE; offset += _SPI_S0_TEST_BUFFER_SIZE) {
					_spi_helper_write(curPageStart + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
				}

				curPageStart += pageSize;
			}
			break;

		case S0S_TESTER_WRITE_DONE:
//			LOGS0S("cycle %d: write done", _s_spiS0test.testCycles);
			TSTR_SET_NEXT(S0S_TESTER_READ);
			break;

		case S0S_TESTER_READ:
			curPageStart = startAddr;
			_s_spiS0test.curChkSum = 0;
//			LOGS0S("cycle %d: read start", _s_spiS0test.testCycles);
			TSTR_SET_NEXT(S0S_TESTER_READ_IN_PROGRESS);
			break;

		case S0S_TESTER_READ_IN_PROGRESS:
			{
				if (curPageStart >= endAddr) {
					TSTR_SET_NEXT(S0S_TESTER_READ_DONE);
					break;
				}

				// eSPI Flash Access is processing
				if(g_ui8ESPIFaProcessing) {
					break;
				}

				// check one page in a batch
				for (uint32_t offset = 0; offset <= pageSize - _SPI_S0_TEST_BUFFER_SIZE; offset += _SPI_S0_TEST_BUFFER_SIZE) {
					// clear buffer first
					for (uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i += 4) {
						uint32_t * pU32 = (uint32_t *)&_s_testBuf[i];
						*pU32 = 0xC0C1C2C3;
					}

					//
					// Do the read
					//
					_spi_helper_read(curPageStart + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);

					if (_s_spiS0test.enableWriteTest) {
						// do verify
						if (_s_spiS0test.useFixPattern4Write) {
							for (uint32_t i = 0; i <= _SPI_S0_TEST_BUFFER_SIZE - 4; i += 4) {
								uint32_t * pU32 = (uint32_t *)&_s_testBuf[i];
								if (*pU32 != _s_spiS0test.testPattern) {
									//
									// ERROR detected
									//
									errDetected ++;
								}
							}
						} else {
							uint32_t idx = u_misc_pr_verifyRandArray((uint16_t)seed, _s_testBuf, _SPI_S0_TEST_BUFFER_SIZE);
							if (_SPI_S0_TEST_BUFFER_SIZE != idx) {
								//
								// ERROR detected
								//
								errDetected ++;

								_spi_helper_dumpInfo(seed, idx, curPageStart + offset);
							}
						}
					} else {  // write test is not enabled
						for (uint32_t i = 0; i <= _SPI_S0_TEST_BUFFER_SIZE - 4; i += 4) {
							uint32_t * pU32 = (uint32_t *)&_s_testBuf[i];
							_s_spiS0test.curChkSum += *pU32;
						}
					}
				}

				curPageStart += pageSize;
			}
			break;

		case S0S_TESTER_READ_DONE:
			if (!_s_spiS0test.enableWriteTest) {
				if (APP_SPIS0_BLANK_CHKSUM == _s_goldChkSum) {
					_s_goldChkSum = _s_spiS0test.curChkSum;
				} else if (_s_goldChkSum != _s_spiS0test.curChkSum) {
					_s_goldChkSum = _s_spiS0test.curChkSum;

					//
					// ERROR detected
					//
					errDetected ++;
				}
			}
			TSTR_SET_NEXT(S0S_TESTER_IDLE);
			thisCycleDone = true;
//			LOGS0S("cycle %d: read done", _s_spiS0test.testCycles);
			break;
		default:
			TSTR_SET_NEXT(S0S_TESTER_IDLE);
			thisCycleDone = true;
			break;
	}

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	if (!isMAFS) {
		md_qmspi_free();
	}
#endif

	if (thisCycleDone) {
		_s_spiS0test.testCycles ++;
		if (errDetected)
			_s_spiS0test.errorCnt ++;

		LOGS0S("One test cycle finished (%d/%d) errDetected %d in this cycle, s %08X", _s_spiS0test.errorCnt, _s_spiS0test.testCycles, errDetected, seed);
		errDetected = 0;
	}

	return thisCycleDone;
}

static void _spi_S0_sharing_stateMachine (struct k_timer *timer)
{
	_s_evtIdBM_event = 0x10;
	k_work_submit(&_s_evtIdBM_work);
}

static void _spi_S0_sharing_stateMachine2 ()
{
	// state machine running only in S0
	enum system_power_state curPwrSt = app_pseq_systemState();
	if (_s_lastPwrSt != curPwrSt) {
		if (SYSTEM_S0_STATE == curPwrSt) {                // Enter S0
			app_spiS0_testEnable();
		} else if (SYSTEM_S0_STATE == _s_lastPwrSt) {     // Exit S0
			app_spiS0_testDisable();
			if (_s_spiS0test.testInterval) {
				k_timer_start(&_s_tidSpiTest, K_MSEC(50), K_MSEC(50));  // Enable main timer again thus it will be enabled when system back to S0
			}
		}

		_s_lastPwrSt = curPwrSt;
	}
	if (SYSTEM_S0_STATE != curPwrSt)
		return;

	//
	// In order for accurate timer.
	//
	//md_tasks_active();

	switch (_s_spi_getStatus()) {
		case S0S_IDLE:
			// timer _s_tidSpiTegr triggers the start
			TSTR_SET_NEXT(S0S_TESTER_IDLE);
			break;
		case S0S_STARTED:
		case S0S_REQ_NEXT_BATCH:
			_s_spi_reqire();
			_s_spi_setStatus(S0S_REQ_SENT);
			break;
		case S0S_REQ_SENT:
			if (isMAFS
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
				|| f_spi_ba_isBusGranted()
#endif
				) {
				_s_spi_setStatus(S0S_GRANTED);
			} else if (SYSTEM_S0_STATE == app_pseq_systemState()) {
				// reqire again
				_s_spi_setStatus(S0S_STARTED);
			} else {
			}
			break;
		case S0S_GRANTED:
			// do the test
			if (_spi_s0_sharing_tester()) {
				_s_spi_setStatus(S0S_REQ_RELEASED);
			} else {
				_s_spi_setStatus(S0S_REQ_NEXT_BATCH);
			}

			// release REQ
			_s_spi_release();
			break;
		case S0S_REQ_RELEASED:
			if (isMAFS
#if S0S_TEST_BY_NON_MAFS_SUPPORTED
				 || !f_spi_ba_isBusGranted()
#endif
				) {
				// trigger the next test
				_s_spi_setStatus(S0S_IDLE);
				if (_s_spiS0test.testInterval) {
					k_timer_start(&_s_tidSpiTegr, K_MSEC(_s_spiS0test.testInterval), K_NO_WAIT);
				}
				else {
					k_timer_start(&_s_tidSpiTegr, K_MSEC(1000), K_NO_WAIT);
				}
			}
			break;
		case S0S_DISABLED:
		default:
			break;
	}
}

/**
 * @brief Initialize SPI S0 test module
 */
void app_spiS0_Init(void)
{
	LOGS0S("%s: Entry", __FUNCTION__);

	memset(&_s_spiS0test, 0, sizeof(_s_spiS0test));

	espi_dev = DEVICE_DT_GET(ESPI_DEV);
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("ESPI Device not ready");
		return;
	}

	// default test case
	_s_spiS0test.spiAddr23_8 = 0x0290;   // this is field [23:8], so it means 0x29000 by default
	_s_spiS0test.spiBlkSize_PwrOf2 = 12; // block size is 2^^12 = 4096 or 4K
	_s_spiS0test.blkNum = 16;            // 16 * 4K start from 0x29000, i.e. [0x29000, 0x38FFF]
	_s_spiS0test.enableWriteTest = 1;

	_s_spiS0test.testPattern = 0xC0A0BC11;

	// enable timers
	k_timer_init(&_s_tidSpiTest, _spi_S0_sharing_stateMachine, NULL);
	k_timer_init(&_s_tidSpiTegr, _spi_S0_test_trigger, NULL);
	md_acpiplanes_register_fun(app_spiS0_AcpiHandler, 0xE4);

	_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;

	// init event
	void _s_bmEvent(struct k_work *w);
	k_work_init(&_s_evtIdBM_work, _s_bmEvent);

	GET_NV_OPTIONS(spi_ba_Type, _s_spiS0test.testType);

	LOGS0S("%s: Exit", __FUNCTION__);
}

/**
 * @brief Quick test wrapper for SPI S0 test cases
 * @param testCase Test case number to execute
 */
void app_spiS0_quickTestWrapper(uint8_t testCase)
{
	switch (testCase) {
		case 7:   // _spiS0_fillRom(0x72); Fill the region by u_misc_pr_genRandArray
		case 6:   // _spiS0_fillRom(0xCC); Region specified by _s_spiS0test
		case 5:   // _spiS0_fillRom(0xFF); Region specified by _s_spiS0test
		case 4:   // _spiS0_fillRom(0);    Region specified by _s_spiS0test
		case 3:   // _spiS0_perfBenchmark(1); Hardcoded 500KB Region
		case 2:   // _spiS0_perfBenchmark(0); Hardcoded 120KB Region
			_s_evtIdBM_event = testCase;
			k_work_submit(&_s_evtIdBM_work);
			break;
		case 1:  // enable stress tress
			_s_spiS0test.testInterval = 512;
			app_spiS0_testEnable();
			break;
		case 0:  // disable test
		default:
			_s_spiS0test.testInterval = 0;
			app_spiS0_testDisable();
			_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;
			break;
	}
}

static void _spiS0_perfBenchmark(bool expandTestRegion)
{
	uint32_t maxAddr;
	uint32_t counter = 0;

	if (expandTestRegion)
		maxAddr = 0x80000;  // totally 500KB/512000B data
	else
		maxAddr = 0x21000;  // totally 120KB/122880B data

	// step 01, disable test if it is running
	_s_spiS0test.testInterval = 0;
	app_spiS0_testDisable();
	_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;

	// step 02, enable test
	_s_spiS0test.testInterval = 65000;
	app_spiS0_testEnable();

	// step 03, disable timer
	k_timer_stop(&_s_tidSpiTest);
	k_timer_stop(&_s_tidSpiTegr);

	// step 04, stop RTOS timer, so this benchmark will be run as top priority
	//RTOS_TIMER_halt();
/* 
FWDEV-125773: System can't boot after running ESPI stress and then do G3 with TI PD
	// step 05, prepare EC SIG
	_spi_helper_read(0, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
	for (uint32_t i = 0x10; i < _SPI_S0_TEST_BUFFER_SIZE; i++) {
		_s_testBuf[i] = 0x00;
	}
	_spi_helper_write(0, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
*/
	LOGS0S("spiS0_perfBenchmark start, maxAddr = 0x%06X", maxAddr);

	// init data buffer
	for (uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i++) {
		_s_testBuf[i] = i;
	}

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 06, eraze (totally 500KB/512000B data)
	counter = 0;
	LOGS0S("1st eraze start ...");
	for(uint32_t spiAddr = 0x1000; spiAddr < 0x1F000; spiAddr+= 0x1000) {  // 30 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}
	for(uint32_t spiAddr = 0x21000; spiAddr < maxAddr; spiAddr+= 0x1000) { // 95 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}

	LOGS0S("1st eraze done ... service counter %d", counter);
	counter = 0;

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 07, eraze again, make sure it is eraze black region
	LOGS0S("2nd eraze start ...");
	for(uint32_t spiAddr = 0x1000; spiAddr < 0x1F000; spiAddr+= 0x1000) {  // 30 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}
	for(uint32_t spiAddr = 0x21000; spiAddr < maxAddr; spiAddr+= 0x1000) { // 95 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}

	LOGS0S("2nd eraze done ... service counter %d", counter);
	counter = 0;

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 08, write
	LOGS0S("write start ...");
	for(uint32_t spiAddr = 0x1000; spiAddr < 0x1F000; spiAddr+= 0x1000) {  // 30 pages
		for (uint32_t offset = 0; offset < 0x1000; offset += _SPI_S0_TEST_BUFFER_SIZE) {
			_spi_helper_write(spiAddr + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
			counter ++;
		}
	}
	for(uint32_t spiAddr = 0x21000; spiAddr < maxAddr; spiAddr+= 0x1000) { // 95 pages
		for (uint32_t offset = 0; offset < 0x1000; offset += _SPI_S0_TEST_BUFFER_SIZE) {
			_spi_helper_write(spiAddr + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
			counter ++;
		}
	}

	LOGS0S("write done ... service counter %d", counter);
	counter = 0;

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 09, read
	LOGS0S("read start ...");
	for(uint32_t spiAddr = 0x1000; spiAddr < 0x1F000; spiAddr+= 0x1000) {  // 30 pages
		for (uint32_t offset = 0; offset < 0x1000; offset += _SPI_S0_TEST_BUFFER_SIZE) {
			_spi_helper_read(spiAddr + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
			counter ++;
		}
	}
	for(uint32_t spiAddr = 0x21000; spiAddr < maxAddr; spiAddr+= 0x1000) { // 95 pages
		for (uint32_t offset = 0; offset < 0x1000; offset += _SPI_S0_TEST_BUFFER_SIZE) {
			_spi_helper_read(spiAddr + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
			counter ++;
		}
	}

	LOGS0S("read done ... service counter %d", counter);
	counter = 0;

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 10, eraze 3rd time, so it is erazing none blank data
	for(uint32_t spiAddr = 0x1000; spiAddr < 0x1F000; spiAddr+= 0x1000) {  // 30 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}
	for(uint32_t spiAddr = 0x21000; spiAddr < maxAddr; spiAddr+= 0x1000) { // 95 pages
		_spi_helper_erase(spiAddr);
		counter ++;
	}

	LOGS0S("3rd eraze done ... service counter %d", counter);
	counter = 0;

	// stall 10 ms
	k_sleep(K_MSEC(10));

	// step 11, resume RTOS
	//RTOS_TIMER_resume();

	LOGS0S("spiS0_perfBenchmark end, maxAddr = 0x%06X", maxAddr);

	// step 12, disable test
	_s_spiS0test.testInterval = 0;
	app_spiS0_testDisable();
	_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;
}

//
// This function used for filling SPI ROM region through eSPI FA interface
//
// pad:  0xFF - Eraze the region
//       0x72 - Fill the region by u_misc_pr_genRandArray, where the seed comes from now()
//       Others from 0 - 0xFE, Fill the region by the byte.
//
static void _spiS0_fillRom(uint8_t pad)
{
	uint32_t counter = 0;
	uint16_t seed = 0;

	// step 01, disable test if it is running
	_s_spiS0test.testInterval = 0;
	app_spiS0_testDisable();
	_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;

	// step 02, enable test
	_s_spiS0test.testInterval = 65000;
	app_spiS0_testEnable();

	// step 03, disable timer
	k_timer_stop(&_s_tidSpiTest);
	k_timer_stop(&_s_tidSpiTegr);

	// step 04, stop RTOS timer, so this benchmark will be run as top priority
	//RTOS_TIMER_halt();

	// step 05, prepare EC SIG
	_spi_helper_read(0, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
	for (uint32_t i = 0x10; i < _SPI_S0_TEST_BUFFER_SIZE; i++) {
		_s_testBuf[i] = 0x00;
	}
	_spi_helper_write(0, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);

	// step 06, erase the region
	uint32_t startAddr = ((uint32_t)_s_spiS0test.spiAddr23_8) << 8;
	uint32_t pageSize  = 1ul << _s_spiS0test.spiBlkSize_PwrOf2;
	uint32_t numOfBlk  = _s_spiS0test.blkNum;
	uint32_t endAddr   = startAddr + pageSize * numOfBlk;
	LOGS0S("spiS0_fillRom erase [%06X..%06X]", startAddr, endAddr-1);
	for(uint32_t spiAddr = startAddr; spiAddr < endAddr; spiAddr+= 0x1000) {
		_spi_helper_erase(spiAddr);
		counter ++;
	}

	// step 06, fill the region
	if (pad != 0xFF) {
		if (0x72 != pad) {     // 0x72 has special meaning
			LOGS0S("spiS0_fillRom fill  [%06X..%06X] with data %02X", startAddr, endAddr-1, pad);
			for (uint32_t i = 0; i < _SPI_S0_TEST_BUFFER_SIZE; i++) {
				_s_testBuf[i] = pad;
			}
		} else {
			seed = (uint16_t)k_uptime_ticks();
			LOGS0S("spiS0_fillRom fill  [%06X..%06X] with random pattern, s %04X", startAddr, endAddr-1, seed);
		}

		for(uint32_t spiAddr = startAddr; spiAddr < endAddr; spiAddr+= 0x1000) {
			for (uint32_t offset = 0; offset < 0x1000; offset += _SPI_S0_TEST_BUFFER_SIZE) {

				if (0x72 == pad) { // 0x72 has special meaning

					u_misc_pr_genRandArray(seed + offset, _s_testBuf, _SPI_S0_TEST_BUFFER_SIZE);
				}

				_spi_helper_write(spiAddr + offset, _SPI_S0_TEST_BUFFER_SIZE, _s_testBuf);
				counter ++;
			}
		}
	}

	// stall 1 ms
	k_sleep(K_MSEC(1));

	// step 07, resume RTOS
	//RTOS_TIMER_resume();

	LOGS0S("spiS0_fillRom end");

	// step 08, disable test
	_s_spiS0test.testInterval = 0;
	app_spiS0_testDisable();
	_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;
}

/**
 * @brief Work handler for benchmark events
 * @param w Pointer to work item
 */
void _s_bmEvent(struct k_work *w)
{
	int evt = _s_evtIdBM_event;

	switch (evt) {
		case 0x10:
			_spi_S0_sharing_stateMachine2();
			break;
		case 7:
			_spiS0_fillRom(0x72); // 0x72 has special meaning, fill the region by u_misc_pr_genRandArray
			break;
		case 6:
			_spiS0_fillRom(0xCC);
			break;
		case 5:
			_spiS0_fillRom(0xFF);
			break;
		case 4:
			_spiS0_fillRom(0);
			break;
		case 3:
			_spiS0_perfBenchmark(1);
			break;
		case 2:
			_spiS0_perfBenchmark(0);
			break;
	default:
		break;
	}
}

/**
 * @brief Enable SPI S0 test
 */
void app_spiS0_testEnable(void)
{
	if (!_s_spiS0test.testInterval)
		return;

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	if (S0S_TEST_BY_PHYSICAL_PINS == _s_spiS0test.testType) {
		// Set SPI_ROM_REQ as Push-pull output low + PD.
		gpio_configure_pin( SPI_ROM_REQ,
			MD_GPIO_OUTPUT_LOW |
			MD_GPIO_MUX_GPIO |
			MD_GPIO_OUTPUT_SELECT_PINCTRL16 |
			MD_GPIO_DIRECTION_OUTPUT |
			MD_GPIO_EM_BUF_TYPE__PushPull |
			MD_GPIO_INTERRUPT_DISABLED |
			MD_GPIO_PAD_PULLDOWN );

		// Set SPI_ROM_GNT as input with both edge interrupt enabled + PD.
		gpio_configure_pin( SPI_ROM_GNT,
			MD_GPIO_MUX_GPIO |
			MD_GPIO_DIRECTION_INPUT |
			MD_GPIO_INTERRUPT_EITHER_EDGE |
			MD_GPIO_PAD_PULLDOWN );

		// Init the ba for physical pins
		f_spi_ba_Init(0, _s_spi_dummpCycle);
	} else if (S0S_TEST_BY_VIRTUAL_PINS == _s_spiS0test.testType) {
		// Init the ba for VW
		f_spi_ba_Init(1, _s_spi_dummpCycle);
	}
#endif

	// Init status and enable timers
	_s_spi_setStatus(S0S_IDLE);
	k_timer_start(&_s_tidSpiTest, K_MSEC(1), K_MSEC(1));
	k_timer_start(&_s_tidSpiTegr, K_MSEC(_s_spiS0test.testInterval), K_NO_WAIT);

	// dump the test paramters
#if (LOG_SPIS0_ON)
	LOGS0S("Test enabled by interval %d ms!", _s_spiS0test.testInterval);
	uint32_t startAddr = ((uint32_t)_s_spiS0test.spiAddr23_8) << 8;
	uint32_t pageSize  = 1ul << _s_spiS0test.spiBlkSize_PwrOf2;
	uint32_t numOfBlk  = _s_spiS0test.blkNum;
	uint32_t endAddr   = startAddr + pageSize * numOfBlk;
	LOGS0S("  Region    [%06X..%06X]", startAddr, endAddr-1);
	LOGS0S("  pageSize  %d", pageSize);
	LOGS0S("  numOfBlk  %d", _s_spiS0test.blkNum);
	switch (_s_spiS0test.testType) { // 0 - to use physical REQ/GNT; 1 - to use VW REG/GNT; 2 - MAFS by eSPI FA
		case 0: 	LOGS0S("  TestType  physical REQ/GNT"); break;
		case 1:   LOGS0S("  TestType  VW REQ/GNT"); break;
		case 2:   LOGS0S("  TestType  MAFS");       break;
		default:  LOGS0S("  TestType  (unknown? -> %d)", _s_spiS0test.testType); break;
	}
	LOGS0S("  WriteTest %s", _s_spiS0test.enableWriteTest ? "YES" : "NO");
	if (_s_spiS0test.enableWriteTest) {
		LOGS0S("  FixedPtn  %s", _s_spiS0test.useFixPattern4Write ? "YES" : "NO");
		if (_s_spiS0test.useFixPattern4Write) {
			LOGS0S("  DataPtn   0x%08X", _s_spiS0test.testPattern);
		}
	}
#endif
}

/**
 * @brief Disable SPI S0 test
 */
void app_spiS0_testDisable(void)
{
	// Disable timer
	k_timer_stop(&_s_tidSpiTest);
	k_timer_stop(&_s_tidSpiTegr);

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	// Set SPI_ROM_REQ as input with interrupt disabled + PD.
	gpio_configure_pin( SPI_ROM_REQ,
		MD_GPIO_MUX_GPIO |
		MD_GPIO_DIRECTION_INPUT |
		MD_GPIO_INTERRUPT_DISABLED |
		MD_GPIO_PAD_PULLDOWN );

	// Set SPI_ROM_GNT as input with interrupt disabled + PD.
	gpio_configure_pin( SPI_ROM_GNT,
		MD_GPIO_MUX_GPIO |
		MD_GPIO_DIRECTION_INPUT |
		MD_GPIO_INTERRUPT_DISABLED |
		MD_GPIO_PAD_PULLDOWN );
#endif

	// clear flag
	_s_spi_setStatus(S0S_DISABLED);
	TSTR_SET_NEXT(S0S_TESTER_IDLE);
	g_ui8ESPIFaProcessing = 0;

#if S0S_TEST_BY_NON_MAFS_SUPPORTED
	// free spi ba
	f_spi_ba_Free();
#endif

	LOGS0S("Test disabled!");
}

/**
 * @brief ACPI handler for SPI S0 test
 * @param isRead Read or write operation flag
 * @param ui8Idx Index of the data
 * @param pui8Data Pointer to data buffer
 * @return 1 on success, 0 on failure
 */
uint8_t app_spiS0_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	uint8_t * ptr = (uint8_t *)&_s_spiS0test;

	if (ui8Idx < sizeof(APP_S0S_HELPER)) {
		if (isRead) {
			*pui8Data = *(ptr + ui8Idx);
		} else {
			if (ui8Idx == 0) {
				ptr[ui8Idx] = *pui8Data;
			}
			if (ui8Idx == 1) {
				ptr[ui8Idx] = *pui8Data;

				if (_s_spiS0test.testInterval) {
					app_spiS0_testEnable();
				} else {
					app_spiS0_testDisable();
					// clear checksum
					_s_goldChkSum = APP_SPIS0_BLANK_CHKSUM;
				}
			}

			// writable when if testInterval is zero
			if (ui8Idx >= 2 && !_s_spiS0test.testInterval) {
				ptr[ui8Idx] = *pui8Data;
			}
		}

		return 1;

	} else if (isRead) {

		switch (ui8Idx) {
			case 0x20:
			case 0x21:
			case 0x22:
			case 0x23:
			{
				uint32_t s = (uint32_t)_s_spi_getStatus();
				*pui8Data = _s_description[s][ui8Idx - 0x20];
				return 1;
			}
			case 0x2F:
			{
				GET_NV_OPTIONS(spi_ba_Type, _s_spiS0test.testType);
				*pui8Data = _s_spiS0test.testType;
				return 1;
			}
			default:
				break;
		}
	} else {  // write
		switch (ui8Idx) {
			case 0x2F:
			{
				_s_spiS0test.testType = *pui8Data;
				SET_NV_OPTIONS(spi_ba_Type, _s_spiS0test.testType);
				return 1;
			}
			default:
				break;
		}
	}

	return 0;
}