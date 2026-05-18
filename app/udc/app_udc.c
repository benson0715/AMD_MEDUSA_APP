/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/espi.h>
#include "espi_hub.h"
#include "app_udc.h"
#include "app_pseq.h"
#include "dev_isl9241.h"
#include "dev_ina219.h"
#include "dev_cpld.h"
#include "app_smtbty.h"
#include "espi_hub.h"
#include "i2c_hub.h"
#include <zephyr/drivers/uart.h>
#include "board_config.h"
#include "u_rrq.h"
#include "board_id.h"
#include "postcode_led_hub.h"
#include "app_acpi.h"
#include "task_handler.h"
#include "f_nv_options.h"
#include "dbgLogFifo.h"
#include "debug_info.h"

LOG_MODULE_REGISTER(udc,CONFIG_UDC_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define 	_UDC_DISPLAY_POSTCODE            (0)
#define 	_UDC_DISPLAY_VIN_MONITOR         (1)
#define 	_UDC_DISPLAY_VIN_MONITOR_SUB_V   (0)
#define 	_UDC_DISPLAY_VIN_MONITOR_SUB_C   (1)
#define 	_UDC_DISPLAY_VIN_MONITOR_SUB_W   (2)
#define 	_UDC_DISPLAY_VIN_MONITOR_Vadp    (3)
#define 	_UDC_DISPLAY_VIN_MONITOR_Iadp    (4)
#define 	_UDC_DISPLAY_VIN_MONITOR_Wadp    (5)
#define 	_UDC_DISPLAY_VIN_MONITOR_MAX     (6)
#define 	_UDC_DISPLAY_DEBUG               (2)
#define 	_UDC_DISPLAY_DEBUG_PRINT_LOG     (0)
#define 	_UDC_DISPLAY_DEBUG_STOP_VRI2C    (1)
#define 	_UDC_DISPLAY_DEBUG_RESERVED      (2)
#define 	_UDC_DISPLAY_DEBUG_I2C_SCAN      (3)
#define 	_UDC_DISPLAY_DEBUG_CPLD_SCAN     (4)
#define 	_UDC_DISPLAY_DEBUG_DUMMY_QEVT    (5)
#define 	_UDC_DISPLAY_DEBUG_MAX           (6)
#define 	_UDC_DISPLAY_MAX                 (3)

#define     UDC_TASK_TICK_INTERVAL_10MS      (10)
#define     UDC_TASK_TICK_INTERVAL_500MS     (500)

/* ************************** *
 *     static valuable        *
 * ************************** */
/* task slp ready */
static uint8_t *task_udcSlpReady = NULL;
#if CONFIG_UDC_UART
const struct device *uartDev;
#endif
static bool _s_isUdcBiosLogEnabled = true;
static bool _s_statusChanged       = true;
static bool _s_need2resetPostcode  = false;
extern bool g_biosDebugLogEnable;
static bool _s_biosDebugLogEnable = true;
__attribute__ ((aligned(4))) static uint32_t _s_lastPostCode;
static uint32_t _s_lastPcValue = 0xFAFA5C5C;
static uint32_t _s_scanLimit = 0;
static uint8_t  _s_points = 0;

/* Postcode requested to be displayed */
static uint8_t _s_port80_code;
static uint8_t _s_port81_code;
static uint8_t _s_port82_code;
static uint8_t _s_port83_code;
#define DWORD_FROM_PORTS(p83, p82, p81, p80) ((p83 << 24) | (p82 << 16) | (p81 << 8) | p80)

static uint8_t _s_udcDisplayType = 0;
static uint8_t _s_udcDisplaySubType = 0;
static uint8_t _s_udcDbgTmp = 0x00;
K_SEM_DEFINE(udcTaskSem, 0, 1);

/* ************************** *
 *     global valuable        *
 * ************************** */
bool g_isUartDonglePresent = false;
bool g_isUdcPresent        = false;

void app_udc_displayWrapper(uint32_t val, uint8_t limit, uint8_t points);

#if CONFIG_UDC_UART
static void udc_uart_tx_callback(struct k_timer *timer);
K_TIMER_DEFINE(_s_senderTimerId,udc_uart_tx_callback,NULL);
#if (!CONFIG_SHELL)
static void udc_uart_rx_callback(const struct device *dev, void *user_data);
static uint32_t udc_uart_fifo_write(void * pBuf, uint32_t length);
#endif
#endif /* CONFIG_UDC_UART */
/* Timer to reset display type to PostCode */
static void _displayType_timeout(struct k_timer *timer);
K_TIMER_DEFINE(g_udcDisplayTypeTimer, _displayType_timeout, NULL);

/**
 * @brief UDC state reset
 */
static void _udc_stateReset(void)
{
	/* reset display type */
	_s_udcDisplayType = _UDC_DISPLAY_POSTCODE;
	_s_udcDisplaySubType = 0;

	/* reset inner status */
	_s_lastPcValue = 0xFAFA5C5C;
	_s_scanLimit = 0;
	_s_points = 0x5A;
	postcode_led_hub_32bit_reset();
}

/**
 * @brief UDC handle with the DISP_SW short click
 */
void app_udc_shortClick(void)
{
#if (!DISP_SW_DEBUG_ALWAYS)
	/* DISP_SW only takes effect in S0 */
	if (app_pseq_systemState() != SYSTEM_S0_STATE)
		return;
#endif

	switch (_s_udcDisplayType) {
		case _UDC_DISPLAY_POSTCODE:
			app_udc_displayWrapper(_s_lastPcValue, _s_scanLimit, _s_points);
			break;
		case _UDC_DISPLAY_VIN_MONITOR:
			_s_udcDisplaySubType = (_s_udcDisplaySubType + 1) % _UDC_DISPLAY_VIN_MONITOR_MAX;
			postcode_led_hub_print("P%d-Sub %d", _s_udcDisplayType, _s_udcDisplaySubType);
					
			break;
		case _UDC_DISPLAY_DEBUG:
			_s_udcDisplaySubType = (_s_udcDisplaySubType + 1) % _UDC_DISPLAY_DEBUG_MAX;
			postcode_led_hub_print("DBG - S%d", _s_udcDisplaySubType);
			break;
		default:
			break;
	}
}

/**
 * @brief UDC handle with the DISP_SW long press
 */
void app_udc_longPress(void)
{
#if (!DISP_SW_DEBUG_ALWAYS)
	/* DISP_SW only takes effect in S0 */
	if (app_pseq_systemState() != SYSTEM_S0_STATE)
		return;
#endif

	_s_udcDisplayType = (_s_udcDisplayType + 1) % _UDC_DISPLAY_MAX;
	_s_udcDisplaySubType = 0;

	if (_UDC_DISPLAY_POSTCODE == _s_udcDisplayType) {
		app_udc_displayWrapper(0, 8, 0xFF);
	} else {
		/* trun on all 7-seg LEDs */
		if (_UDC_DISPLAY_VIN_MONITOR == _s_udcDisplayType) {
			postcode_led_hub_print("P%d-Sub %d", _s_udcDisplayType, _s_udcDisplaySubType);
				
		} else if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType) {
			postcode_led_hub_print("DBG - S%d", _s_udcDisplaySubType);
				
		} else {
			postcode_led_hub_print("F%d-Sub %d", _s_udcDisplayType, _s_udcDisplaySubType);
		}
	}
}

/**
 * @brief UDC handle with the WWAN_RADIO click
 */
void app_udc_onWwanRadioClick(void)
{
	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_PRINT_LOG == _s_udcDisplaySubType) {

		dbgLogFifo_flush2uart();
#if CONFIG_BATTERY_MANAGEMENT
		extern void _chgDbg_dumpChgSetting_LogOutput ();
		_chgDbg_dumpChgSetting_LogOutput();
#endif
		info_value_collect();
		info_thread_collect();
		postcode_led_hub_print("LogFifo. .");
	}

	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_DUMMY_QEVT == _s_udcDisplaySubType) {

		_s_udcDbgTmp ++;
		if (_s_udcDbgTmp < 0x60 || _s_udcDbgTmp > 0x70) {
			_s_udcDbgTmp = 0x60;
		}

	    postcode_led_hub_print("Qevt. . .%02X", _s_udcDbgTmp);
	}
}

/**
 * @brief UDC handle with the WLAN_RADIO click
 */
void app_udc_onWlanRadioClick(void)
{
	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
			_UDC_DISPLAY_DEBUG_STOP_VRI2C == _s_udcDisplaySubType) {

		postcode_led_hub_print("EC. . .dead");
		while(1);
	}

	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_RESERVED == _s_udcDisplaySubType) {
		postcode_led_hub_print("48MHz_._._");
		postcode_led_hub_print("48MHz_EN");
	}

	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_I2C_SCAN == _s_udcDisplaySubType) {

		postcode_led_hub_print("SCAN. .I2C");
		postcode_led_hub_print("DONE. .%03d", dev_cpld_scan_i2c_bus());
	}

	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_CPLD_SCAN == _s_udcDisplaySubType) {
		postcode_led_hub_print("SCAN. .I2C");
		postcode_led_hub_print("CPLD. .%03d", dev_cpld_scan_i2c());
	}

	if (_UDC_DISPLAY_DEBUG == _s_udcDisplayType && 
		_UDC_DISPLAY_DEBUG_DUMMY_QEVT == _s_udcDisplaySubType) {
		postcode_led_hub_print("Sending. .", _s_udcDbgTmp);
		k_msleep(300);
		ACPI_NotifyHost(_s_udcDbgTmp);
		postcode_led_hub_print("Sent. . .%02X", _s_udcDbgTmp);
	}
}

/**
 * @brief UDC dispaly the postcode LED
 */
void app_udc_displayWrapper(uint32_t val, uint8_t limit, uint8_t points)
{
	if (_UDC_DISPLAY_POSTCODE == _s_udcDisplayType && SYSTEM_S0_STATE == app_pseq_systemState()) {
		static uint32_t last_postcode = 0u;

		if (val != last_postcode) {
			postcode_led_hub_32bit_postcode(val, points, limit);
		}
		last_postcode = val;
	}

#if(LOG_ENABLE && LOG_PORT80)
	if (val != _s_lastPcValue || limit != _s_scanLimit) {
		switch (limit) {
			case 4:  LOG_ERR("[PC]           %04X", val & 0xFFFF); break;
			case 2:  LOG_ERR("[PC]             %02X", val & 0xFF); break;
			case 8:
			default: LOG_ERR("[PC]       %08X", val);
				 break;
		}
	}
#endif

	_s_lastPcValue = val;
	_s_scanLimit = limit;
	_s_points = points;
}

/**
 * @brief MEC1723 UDC notify the postcode LED
 */
void app_udc_postcodeNotifier(uint8_t port_index, uint8_t code)
{
	switch (port_index) {
	case POSTCODE_PORT80:
		if (_s_port80_code != code) {
			_s_port80_code = code;
		}
		break;
	case POSTCODE_PORT81:
		if (_s_port81_code != code) {
			_s_port81_code = code;
		}
		break;
	case POSTCODE_PORT82:
		if (_s_port82_code != code) {
			_s_port82_code = code;
		}
		break;
	case POSTCODE_PORT83:
		if (_s_port83_code != code) {
			_s_port83_code = code;
		}
		break;
	default:
		break;
	}
	uint32_t new_code = DWORD_FROM_PORTS(_s_port83_code, _s_port82_code, _s_port81_code, _s_port80_code);
	if ((app_pseq_systemState() == SYSTEM_S0_STATE) && (new_code != _s_lastPostCode)) {
		_s_lastPostCode = new_code;
		k_sem_give(&udcTaskSem);
	}
}


#if CONFIG_POWER_MONITOR
/************************************
 *      Current/Power Monitor       *
 ************************************/
#define _UDC_AVERAGE_NUMBER  5

static uint32_t _vinSum;
static uint32_t _vinBuf[_UDC_AVERAGE_NUMBER + 1];
static uint8_t _rrqIdVinBuf = U_RRQ_INVALID_RRQ_ID;

/**
 * @brief UDC init the VIN monitor
 */
static void _udc_vinMonitorInit(void)
{
	_rrqIdVinBuf = u_rrq_setup(_vinBuf, sizeof(_vinBuf), false);
}

/**
 * @brief read the vin monitor ADC value
 *
 * @retval vin report data
 */
static uint32_t _udc_vinMonitorHandler()
{
	uint32_t val = 0;
	uint8_t prefix[3];
	static uint8_t lastSubType = 0xFF;

	if (_UDC_DISPLAY_VIN_MONITOR == _s_udcDisplayType) {
		switch (_s_udcDisplaySubType) {
			case _UDC_DISPLAY_VIN_MONITOR_SUB_V:
				prefix[0] = 'P';
				prefix[1] = 'U';
				val = dev_ina219_GetVoltage_mV();
				break;
			case _UDC_DISPLAY_VIN_MONITOR_SUB_C:
				prefix[0] = 'P';
				prefix[1] = 'i';
				val = dev_ina219_GetCurrent_uA();
				val /= 1000;
				break;
			case _UDC_DISPLAY_VIN_MONITOR_SUB_W:
				prefix[0] = 'P';
				prefix[1] = 'o';
				val = dev_ina219_GetPower_mW();
				break;
			case _UDC_DISPLAY_VIN_MONITOR_Vadp:
				prefix[0] = 'S';
				prefix[1] = 'U';
				val = 0;
#if CONFIG_BATTERY_MANAGEMENT
				if (dev_isl9241_i32Read(CHGID, DEV_ISL9241_REG_VIN_ADC_Results, &val))
					val = val * 3 / 2;
#endif
				break;
			case _UDC_DISPLAY_VIN_MONITOR_Iadp:
				prefix[0] = 'S';
				prefix[1] = 'i';
				val = 0;
#if CONFIG_BATTERY_MANAGEMENT
				if (dev_isl9241_i32Read(CHGID, DEV_ISL9241_REG_IADP_ADC_Results, &val)) {
					val *= 222;   // Reg 0x83 [7:0] stepping 22.2mA
					val /= 10;
				}
#endif
				break;
			case _UDC_DISPLAY_VIN_MONITOR_Wadp:
				prefix[0] = 'S';
				prefix[1] = 'o';
				val = 0;
#if CONFIG_BATTERY_MANAGEMENT
				if (dev_isl9241_i32Read(CHGID, DEV_ISL9241_REG_IADP_ADC_Results, &val)) {
					val *= 222;   // Reg 0x83 [7:0] stepping 22.2mA
					val /= 10;

					uint32_t tmp = 0;
					if (dev_isl9241_i32Read(CHGID, DEV_ISL9241_REG_VIN_ADC_Results, &tmp)) {
						tmp = tmp * 3 / 2;
						val *= tmp;
						val /= 1000;
					} else {
						val = 0;
					}
				}
#endif
				break;
			default:
				return 0xC0EEEEEE;
		}
		prefix[2] = '\0';

		/**
		 * The value being printed on 7-seg LEDs are an average value
		 * of _UDC_AVERAGE_NUMBER readings.
		 */
		/* reset the buffer if subType had changed */
		if (lastSubType != _s_udcDisplaySubType) {
			u_rrq_reset(_rrqIdVinBuf);

			for (uint32_t i = 0; i < _UDC_AVERAGE_NUMBER; i++) {
				u_rrq_put(_rrqIdVinBuf, &val, sizeof(val));
			}

			_vinSum = val * _UDC_AVERAGE_NUMBER;

			lastSubType = _s_udcDisplaySubType;
		} else {
			uint32_t oldest;

			u_rrq_get(_rrqIdVinBuf, &oldest, sizeof(oldest));
			u_rrq_put(_rrqIdVinBuf, &val, sizeof(val));

			_vinSum += val;
			_vinSum -= oldest;
		}
	} else {
		lastSubType = 0xFF;
		return 0;
	}

	val = _vinSum / _UDC_AVERAGE_NUMBER;
		postcode_led_hub_print("%s-%2d.%03d", prefix, val / 1000, val % 1000);
	return val;
}
#endif

/************************************
 *         Warning message          *
 ************************************/
static const char * _s_warnMsg[4] = {
	"Proc-HOT",
	"APU-Thrl",
	NULL,
	NULL
};

/**
 * @brief show the warning message in the postcode LED
 *
 * @param msgId: report warning message index
 */
void app_postcode_setWarnMsg(uint8_t msgId)
{
	if (msgId < 4) {
		if (_UDC_DISPLAY_MAX + 1 != _s_udcDisplayType) {
			_s_udcDisplayType = _UDC_DISPLAY_MAX + 1;
		}
		k_timer_start(&g_udcDisplayTypeTimer, K_MSEC(700), K_NO_WAIT);
		postcode_led_hub_print(_s_warnMsg[msgId]);
	}
}

/**
 * @brief display type timeout handler
 *
 * @param timer: timer instance
 */
static void _displayType_timeout(struct k_timer *timer)
{
	if (_s_udcDisplayType >= _UDC_DISPLAY_MAX) {
		_s_udcDisplayType = _UDC_DISPLAY_MAX;

		app_udc_displayWrapper(_s_lastPcValue, 8, 0xFF);
		_s_udcDisplayType = _UDC_DISPLAY_POSTCODE;
	}
}
#if CONFIG_UDC_UART
/************************************
 *       UDC command handling       *
 ************************************/
static uint8_t _s_TxBuf[256]; /* UART0 TX buffer */
static uint8_t _s_RxBuf[512]; /* UART0 RX buffer */
static uint8_t _s_txQid = U_RRQ_INVALID_RRQ_ID;
static uint8_t _s_rxQid = U_RRQ_INVALID_RRQ_ID;

/**
 * @brief decode the uart package from UDC
 *
 * @param c:   header
 * @param pos: position
 * @retval uart package status
 */
static U_RRQ_PKG_OPS _udcPackage (uint8_t c, uint32_t pos)
{
	static uint8_t pkgLen = 0;
	static uint8_t checksum = 0;

	if (pos == 0 && c == 0xAA)
		return U_RRQ_POS_MOVE_RIGHT;

	if (pos == 1 && c == 0x55) {
		checksum = 0;
		return U_RRQ_POS_MOVE_RIGHT;
	}
	
	if (pos == 2) {
		pkgLen = c;
	}

	/* checksum starts from 3rd bytes */
	if (pos >= 2 && pos < pkgLen + 2) {
		checksum += c;
		return U_RRQ_POS_MOVE_RIGHT;
	}

	if (pos >= 2 && pos == pkgLen + 2) {
		if (c == checksum) {
			return U_RRQ_MATCH;
		}
	}

	/* reset status */
	pkgLen = 0;
	checksum = 0;
	return U_RRQ_FRAME_MOVE_RIGHT;
}

/*
 * PORT80 command
 */
#define APP_UDC_CMD_PORT80    0x80

typedef struct __packed {
	uint8_t length;
	uint8_t cmd;
	union {
		uint8_t  u8pc;
		uint16_t u16pc;
		uint32_t u32pc;
	} d;
	uint8_t crc;
} APP_UDC_CMD_PORT80_PKG;

/**
 * @brief UDC postcode dispaly with package
 *
 * @param pPkg:   postcode package
 */
static void _postcodeDisplay (APP_UDC_CMD_PORT80_PKG * pPkg)
{
	pPkg->length -= 2;

	if (pPkg->length == 4) {
		app_udc_displayWrapper((uint32_t)pPkg->d.u32pc, 8, 0);
	} else if (pPkg->length == 2) {
		app_udc_displayWrapper((uint32_t)pPkg->d.u16pc, 4, 0);
	} else if (pPkg->length == 1) {
		app_udc_displayWrapper((uint32_t)pPkg->d.u8pc, 2, 0);
	}
}

/*
 * Board ID command
 */
#define APP_UDC_CMD_READ_BOARDID_LOW    0x30
#define APP_UDC_CMD_READ_BOARDID_HIGH   0x32
#define APP_UDC_CMD_WRITE_BOARDID_LOW   0x31
#define APP_UDC_CMD_WRITE_BOARDID_HIGH  0x33

#pragma pack(push,1)
typedef struct {
	uint16_t magic;
	uint8_t length;
	uint8_t cmd; 
	uint8_t seqNo;
	uint8_t crc;
} APP_UDC_CMD_BOARDID;

typedef struct {
	uint16_t magic;
	uint8_t length;
	uint8_t cmd; 
	uint8_t seqNo;
	uint8_t data[128];
	uint8_t crc;
} APP_UDC_CMD_BOARDID_WITH_PAYLOAD;
#pragma pack(pop)

/**
 * @brief UDC to handle with the boardid read/write
 *
 * @param pPkg:   postcode package
 */
void _boardIdProcess(uint8_t * buf)
{
	APP_UDC_CMD_BOARDID              * pPkg = (APP_UDC_CMD_BOARDID *)buf;
	APP_UDC_CMD_BOARDID_WITH_PAYLOAD * pBid = (APP_UDC_CMD_BOARDID_WITH_PAYLOAD *)buf;
	static APP_UDC_CMD_BOARDID_WITH_PAYLOAD bidPkg;
	static APP_UDC_CMD_BOARDID ackPkg;

	uint8_t chkSum;

	/* len/cmd/seqNo/<128B_data>/CRC */
	if (3 == pPkg->length) {
		uint8_t start = 0xFF;
		if (APP_UDC_CMD_READ_BOARDID_LOW == pPkg->cmd) {
			start = 0;
		} else if (APP_UDC_CMD_READ_BOARDID_HIGH == pPkg->cmd) {
			start = 128;
		}

		if (0xFF != start) {
			brdId_readBuf(start, 128, &bidPkg.data[0]);
			bidPkg.magic = 0x55AA;
			bidPkg.length = 3 + 128;
			bidPkg.cmd = pPkg->cmd;
			bidPkg.seqNo = pPkg->seqNo;

			chkSum = bidPkg.length + bidPkg.cmd + bidPkg.seqNo;

			for (uint8_t i = 0; i < 128; i++) {
				chkSum += bidPkg.data[i];
			}
			bidPkg.crc = chkSum;

			/* Write package to FIFO */
			udc_uart_fifo_write(&bidPkg, sizeof(bidPkg));
		}

	} else if (3 + 128 == pBid->length) {
		uint8_t start = 0xFF;
		if (APP_UDC_CMD_WRITE_BOARDID_LOW == pBid->cmd) {
			start = 0;
		} else if (APP_UDC_CMD_WRITE_BOARDID_HIGH == pBid->cmd) {
			start = 128;
		}

		if (0xFF != start) {
			brdId_writeBuf(start, 128, &pBid->data[0]);

			/* Wait until write is done */

			/* ACK host */
			ackPkg.magic = 0x55AA;
			ackPkg.length = 3;
			ackPkg.cmd = pBid->cmd;
			ackPkg.seqNo = pBid->seqNo;

			chkSum = ackPkg.length + ackPkg.cmd + ackPkg.seqNo;
			ackPkg.crc = chkSum;

			/* Write package to FIFO */
			udc_uart_fifo_write(&ackPkg, sizeof(ackPkg));
		}
	}
}

/*
 * Board ID command
 */
#define APP_UDC_CMD_BIOS_LOG_SWITCH   0x34

#pragma pack(push,1)
typedef struct {
	uint16_t magic;
	uint8_t length;
	uint8_t cmd;
	uint8_t seqNo;
	uint8_t operator;
	uint8_t crc;
} APP_UDC_CMD_LOG_SWITCH;
#pragma pack(pop)

/**
 * @brief UDC to handle with the bios log
 *
 * @param buf:   bios log switch process
 */
void _biosLogSwitchProcess(uint8_t * buf)
{
	APP_UDC_CMD_LOG_SWITCH * pPkg = (APP_UDC_CMD_LOG_SWITCH *)buf;

	switch (pPkg->operator) {
		case 0:    /* enable  BIOS log */
		case 1:    /* disable BIOS log */
			if (_s_isUdcBiosLogEnabled != pPkg->operator) {
				_s_isUdcBiosLogEnabled = pPkg->operator;
				_s_statusChanged = true; // set the flag so UDC task will setup COM0 again as need.
			}
			break;

		case 0xFF: /* Query log status */
			pPkg->operator = _s_isUdcBiosLogEnabled;
			pPkg->crc = pPkg->length + pPkg->cmd + pPkg->seqNo + pPkg->operator;
			break;
		default:
			return;
	}

	/* Send package back as ACK */
	udc_uart_fifo_write(buf, sizeof(APP_UDC_CMD_LOG_SWITCH));
}

#endif /* CONFIG_UDC_UART */

/************************************
 *         Postcode display         *
 ************************************/
static bool _s_turnOn7SegLEDs = false;
static bool _s_is7SegLEDsPwrOn = false;

/**
 * @brief UDC to turn off the postcode
 */
void app_postcode_turnOff( void )
{
	_s_turnOn7SegLEDs = false;
	task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	k_sem_give(&udcTaskSem);
}

/**
 * @brief UDC to turn on the postcode
 */
void app_postcode_turnOn( void )
{
	_s_turnOn7SegLEDs = true;
	task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	k_sem_give(&udcTaskSem);
}

/**
 * @brief UDC to reset the postcode
 */
void app_postcode_OnReset()
{
	_s_need2resetPostcode = true;
	task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	k_sem_give(&udcTaskSem);
}
#if CONFIG_UDC_UART
/**
 * @brief process the UART write from UDC as FIFO
 *
 * @param     pBuf:     buffer point
 * @param     length:   buffer length
 * @retval write data length.
 */
static uint32_t udc_uart_fifo_write(void * pBuf, uint32_t length)
{
	uint32_t bytes = 0;
	if (U_RRQ_INVALID_RRQ_ID == _s_txQid)
		return 0;

	/* Read data from circular array and returns how many bytes had written. */
	bytes = u_rrq_put(_s_txQid, pBuf, length);
	k_timer_start(&_s_senderTimerId, K_MSEC(1), K_NO_WAIT);

	return bytes;
}
#endif /* CONFIG_UDC_UART */
/************************************
 *             UDC task             *
 ************************************/
#if (!CONFIG_SHELL)
/**
 * @brief UDC data arrival handler
 */
static void _app_udc_onDataArrive()
{
	task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	k_sem_give(&udcTaskSem);
}
#if CONFIG_UDC_UART
/**
 * @brief UDC UART data arrival handler
 */
static void _app_udc_onUartDataArrive()
{
	_app_udc_onDataArrive();
}
#endif /* CONFIG_UDC_UART */
#endif

/**
 * @brief Refresh UDC status and trigger status update
 */
void app_udc_refreshUdcStatus(void)
{
	_s_statusChanged = true;
	#if (!CONFIG_SHELL)
	_app_udc_onDataArrive();
	#endif
}

/**
 * @brief get UDC status
 *
 * @retval UDC status
 */
uint8_t app_udc_getUdcStatus(void)
{
	if (_s_biosDebugLogEnable) {
		if (g_isUdcPresent && _s_isUdcBiosLogEnabled) {
			return ESPI_UDC_UDCPRESENT;
		} else if (g_isUartDonglePresent) {
			return ESPI_UDC_UARTDONGLE_PRESENT;
		}
	}

	return ESPI_UDC_NONE;
}

/**
 * @brief UDC uart enable
 *
 * @retval True if successful
 */
bool app_udc_uart_enable(void)
{
	board_uart_enable();
	return true;
}

/**
 * @brief UDC init
 *
 * @retval True if successful
 */
bool app_udc_init( void )
{
	/* Timer to reset display type to PostCode */
	k_timer_start(&g_udcDisplayTypeTimer, K_MSEC(700), K_NO_WAIT);
#if CONFIG_UDC_UART
	/* UART0 is connected to UDC */
	_s_txQid = u_rrq_setup(_s_TxBuf, sizeof(_s_TxBuf), 0);
	_s_rxQid = u_rrq_setup(_s_RxBuf, sizeof(_s_RxBuf), 0);
#endif /* CONFIG_UDC_UART */
	/* Sync up UART dongle status
	 *
	 * DBG_CARD_PSNT#_3V3 Notify EC that debug card is presented, low active
	 */
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
	g_isUdcPresent = gpio_read_pin(DBG_CARD_PRSNT_N) ? false : true;
	g_isUartDonglePresent = false; // alway false
#else
#ifdef ex_EC_UART_DOGL_PRSNTn
	g_isUartDonglePresent = ioexp_get(ex_EC_UART_DOGL_PRSNTn) ? false : true;
#else
	g_isUartDonglePresent = gpio_read_pin(UART_DBG_PRNST_N) ? false : true;
#endif
#ifdef ex_UDC_EP_DBG_CARD_PRSNTn
	g_isUdcPresent = ioexp_get(ex_UDC_EP_DBG_CARD_PRSNTn) ? false : true;
#else
	g_isUdcPresent = ioexp_get(ex_DBG_CARD_PSNTn_3V3) ? false : true;
#endif
#endif
	_s_isUdcBiosLogEnabled = true;
	espihub_add_get_udc_init_status_handler(app_udc_getUdcStatus);
#if CONFIG_POWER_MONITOR
 	dev_ina219_calibrate(DEV_INA219_10_mOHM);

	_udc_vinMonitorInit();
#endif
 	espihub_add_postcode_handler(app_udc_postcodeNotifier);
#if CONFIG_UDC_UART
	uartDev =  DEVICE_DT_GET(UART_0);
	if (!device_is_ready(uartDev)) {
		LOG_ERR("UART Device not ready");
	};
#endif /* CONFIG_UDC_UART */
	return true;
}

/**
 * @brief Routine that handles udc application.
 *
 * udc_thread must run after 1.8V EC alw power enabled in amd crb, or init error will
 * happen.
 *
 * @param p1 thread run period.
 * @param p2 thread sleep ready.
 * @param p3 reserved.
 */
void udc_thread(void *p1, void *p2, void *p3)
{
	task_udcSlpReady = (uint8_t *)p2;
#if CONFIG_UDC_UART
	uint32_t pkgLength = 0;
	uint8_t pkgBuf[150];
	bool isRecvQueueFull = false;
#endif /* CONFIG_UDC_UART */
	bool lastUartDongleStatus = !g_isUartDonglePresent;
	bool lastUdcStatus = !g_isUdcPresent;
	uint32_t optionVal;
	app_udc_init();
	
	while (true) {
		task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
			
		if (app_pseq_systemState() > SYSTEM_G3_STATE) {
		/* Change global bios log status regardless UDC/UART dongle present */
		if (g_biosDebugLogEnable != _s_biosDebugLogEnable) {
			_s_biosDebugLogEnable = g_biosDebugLogEnable;
			_s_statusChanged = true;
		}

		/* These two status are debounced by input buffer */
		if (lastUartDongleStatus != g_isUartDonglePresent) {
			lastUartDongleStatus = g_isUartDonglePresent;
			_s_statusChanged = true;
		}
		if (lastUdcStatus != g_isUdcPresent) {
			lastUdcStatus = g_isUdcPresent;
			if (g_isUdcPresent)
				_s_isUdcBiosLogEnabled = true;

			_s_statusChanged = true;
		}

		/* Setup system COM port and enable UART0 as needed */
		if (_s_statusChanged) {
			_s_statusChanged = false;

			if (_s_biosDebugLogEnable) {
				if (g_isUdcPresent && _s_isUdcBiosLogEnabled) { /* has UDC & BiosLog enabled - dummy COM1 port */
					LOG_ERR("[U_D_C]    Enable Universal Debug Card.");
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
					uart_irq_rx_disable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
					if (app_pseq_systemState() == SYSTEM_S0_STATE) {
						espihub_set_udc_port(ESPI_UDC_UDCPRESENT);
					}
#if CONFIG_UDC_UART
					/* Enable UART0 and FIFO */
					u_rrq_reset(_s_txQid);
					u_rrq_reset(_s_rxQid);
#endif /* CONFIG_UDC_UART */
					/* Enable Tx/Rx interrupt before using fifo */
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
					uart_irq_callback_set(uartDev, udc_uart_rx_callback);
					uart_irq_rx_enable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
					LOG_ERR("[U_D_C]    ... done");

				} else if (g_isUartDonglePresent) { /* UART dongle only - UART0 @ COM1 */
					LOG_ERR("[U_D_C]    Enable EC UART0 at COM1. (IRQ_4)");
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
					uart_irq_rx_disable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
					if (app_pseq_systemState() == SYSTEM_S0_STATE) {
						espihub_set_udc_port(ESPI_UDC_UARTDONGLE_PRESENT);
					}
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
					uart_irq_callback_set(uartDev, udc_uart_rx_callback);
					uart_irq_rx_enable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
					LOG_ERR("[U_D_C]    ... done");

				} else {                          /* disable 3f8 */
					LOG_ERR("[U_D_C]    Disable UDC.");
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
					uart_irq_rx_disable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
					if (app_pseq_systemState() == SYSTEM_S0_STATE) {
						espihub_set_udc_port(ESPI_UDC_NONE);
					}
#if CONFIG_UDC_UART
					u_rrq_reset(_s_txQid);
					u_rrq_reset(_s_rxQid);
#endif /* CONFIG_UDC_UART */
				}
			}
			else {
				LOG_ERR("[U_D_C]    Disable UDC global.");
#if (!CONFIG_SHELL)
#if CONFIG_UDC_UART
				uart_irq_rx_disable(uartDev);
#endif /* CONFIG_UDC_UART */
#endif
				if (app_pseq_systemState() == SYSTEM_S0_STATE) {
					espihub_set_udc_port(ESPI_UDC_NONE);
				}
#if CONFIG_UDC_UART
				u_rrq_reset(_s_txQid);
				u_rrq_reset(_s_rxQid);
#endif /* CONFIG_UDC_UART */
			}
		}

		/* Mimic TX Buffer empty */
		espihub_mimic_tx_buffer_empty();

		/* DBG_CARD_PSNTn, To notify EC, the FPGA debug card is present. low active */
		if (!_s_statusChanged) {
			if ( !g_isUdcPresent ) {                              /* No UDC - display P80 by itself */
				if (_s_udcDisplayType == _UDC_DISPLAY_POSTCODE) {
					app_udc_displayWrapper(_s_lastPostCode, 8, 0);
				}
			} else { 
#if CONFIG_UDC_UART
				/* has UDC - process commands from UDC */
				isRecvQueueFull = u_rrq_isFull(_s_rxQid);

				pkgLength = u_rrq_package_get(_s_rxQid, _udcPackage, pkgBuf, sizeof(pkgBuf));
				if (pkgLength) {
					/* Process the package */
					uint8_t cmd = pkgBuf[3];
					switch (cmd) {
						case APP_UDC_CMD_PORT80:
							if (app_pseq_systemState() == SYSTEM_S0_STATE && _s_udcDisplayType == _UDC_DISPLAY_POSTCODE) {
								_postcodeDisplay((APP_UDC_CMD_PORT80_PKG *)&pkgBuf[2]);
							}
							break;
						case APP_UDC_CMD_READ_BOARDID_LOW:
						case APP_UDC_CMD_READ_BOARDID_HIGH:
						case APP_UDC_CMD_WRITE_BOARDID_LOW:
						case APP_UDC_CMD_WRITE_BOARDID_HIGH:
							_boardIdProcess(&pkgBuf[0]);
							break;
						case APP_UDC_CMD_BIOS_LOG_SWITCH:
							_biosLogSwitchProcess(&pkgBuf[0]);
							break;
						default:
							break;
					}
				} else if (isRecvQueueFull) {
					/* No valid package but queue is full */
					u_rrq_reset(_s_rxQid);
				}
#endif /* CONFIG_UDC_UART */
			}
		}
#if CONFIG_POWER_MONITOR
		if (_s_udcDisplayType == _UDC_DISPLAY_VIN_MONITOR) {
			static uint32_t c = 0; /* to redice the refresh rate */

			if (!c) {
				_udc_vinMonitorHandler();
				c = 75;
			} else {
				c --;
			}
		}
#endif
	
		/* 7-seg LEDs reset */
		if (_s_need2resetPostcode) {
			_s_need2resetPostcode = false;
#if CONFIG_UDC_UART
			if (_s_rxQid != U_RRQ_INVALID_RRQ_ID)
				u_rrq_reset(_s_rxQid);
#endif /* CONFIG_UDC_UART */
			_s_lastPostCode = 0;
				postcode_led_hub_32bit_reset();

			if (!_s_turnOn7SegLEDs)
			{
				postcode_led_hub_32bit_turnOff();
			}
		}
		if (_s_is7SegLEDsPwrOn != _s_turnOn7SegLEDs) {
			if (_s_turnOn7SegLEDs) {
				app_postcode_OnReset();
				_udc_stateReset();
				GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
				if (optionVal)
				postcode_led_hub_32bit_turnOn(16);
			} else {
				postcode_led_hub_32bit_turnOff();
			}

			_s_is7SegLEDsPwrOn = _s_turnOn7SegLEDs;	
		}

		}
		
		task_status_set(task_udcSlpReady, TASK_STASTUS_SLEEP_READY, 1);
		k_sem_take(&udcTaskSem, K_FOREVER);
	}
}
#if CONFIG_UDC_UART
#if (!CONFIG_SHELL)
/**
 * @brief UDC uart Rx callback
 *
 * @param     dev:         Device tree
 * @param     user_data:   uart data buffer
 */
static void udc_uart_rx_callback(const struct device *dev, void *user_data)
{
	uint8_t recvData;
	bool arrived = false;
	ARG_UNUSED(user_data);

	/* Verify uart_irq_update() */
	if (!uart_irq_update(dev)) {
		LOG_ERR("retval should always be 1");
		return;
	}

	/* Verify uart_irq_rx_ready() */
	while (uart_irq_rx_ready(dev)) {
		uart_poll_in(dev, &recvData);
		uart_irq_update(dev);
		u_rrq_put(_s_rxQid, &recvData, 1);
		arrived = true;
	}
	if (arrived) {
		_app_udc_onUartDataArrive();
	}
}
#endif

/**
 * @brief UDC uart Tx callback
 *
 * @param timer: timer instance
 */
static void udc_uart_tx_callback(struct k_timer *timer)
{
	uint8_t char2send;
	uint8_t bytesSend = 0;
	bool done = true;

	while(true) {

		if (U_RRQ_INVALID_RRQ_ID != _s_txQid) {
			if (u_rrq_get(_s_txQid, &char2send, 1)) {
				uart_poll_out(uartDev, char2send);
				bytesSend ++;
				done = false;
			} else {
				done = true;
			}
		}
		/* transmit at most 100 chars in a batch */
		if (bytesSend > 100 || done)
			break;
	}

	if (!u_rrq_isEmpty(_s_txQid) ) {
	k_timer_start(&_s_senderTimerId, K_MSEC(1), K_NO_WAIT);
	}
}
#endif /* CONFIG_UDC_UART */