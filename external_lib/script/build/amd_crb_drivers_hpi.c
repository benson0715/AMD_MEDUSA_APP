/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include "amd_crb_drivers_hpi.h"
#include "i2c_hub.h"
#include "board_config.h"
#include "system.h"
#include "dev_utility.h"

LOG_MODULE_REGISTER(hpi, CONFIG_HPI_LOG_LEVEL);

#ifndef I2C_HPI_ADDRESS_ID1
#define I2C_HPI_ADDRESS_ID1    0x08
#endif
#ifndef I2C_UCSI_ADDRESS_ID1
#define I2C_UCSI_ADDRESS_ID1   I2C_HPI_ADDRESS_ID1
#endif

#ifndef I2C_HPI_ADDRESS_ID2
#define I2C_HPI_ADDRESS_ID2    0x40
#endif
#ifndef I2C_UCSI_ADDRESS_ID2
#define I2C_UCSI_ADDRESS_ID2   I2C_HPI_ADDRESS_ID2
#endif

#define HPI_UFP_ONLY                  (1)
#define HPI_DRP                       (2)

#define ACD_HPI_RETRY_PERIOD          (1)

#if (CONFIG_LOG)
void amd_crb_drivers_hpi_dumpEventMessage(uint8_t id);
#else
#define amd_crb_drivers_hpi_dumpEventMessage(x) 	while(0){}
#endif

/* I2C port number */
#ifndef HPI_I2C_PORT
#define HPI_I2C_PORT                  I2C_10
#endif

/* HPI register */
DRV_HPI_Rx0000_DEVICE_MODE g_hpi_Regx0000[2] = {0}; /* two PD chips */
DRV_HPI_Rx0006_INT_STATE   g_hpi_Regx0006[2] = {0}; /* two PD chips */
DRV_HPI_PD_STATUS g_hpi_Regx1008[2] = {0}; /* two PD chips */
DRV_HPI_PD_STATUS g_hpi_Regx2008[2] = {0}; /* two PD chips */
DRV_HPI_TYPEC_STATUS  g_hpi_Regx100C[2] = {0}; /* two PD chips */
DRV_HPI_TYPEC_STATUS  g_hpi_Regx200C[2] = {0}; /* two PD chips */
volatile DRV_HPI_Rx0040_REG g_hpi_Regx0040[2] = {0}; /* two PD chips */

static struct k_mutex _s_hpi_RdoLock;
static pd_do_t _s_hpi_port0Pdo = {0};
static pd_do_t _s_hpi_port1Pdo = {0};
static pd_do_t _s_hpi_port2Pdo = {0};
static pd_do_t _s_hpi_xSysPdo = {0};
static pd_do_t _s_hpi_curRdo = {0};
static uint8_t _s_hpi_intBottomFlag[2] = {0}; /* two PD chips */
static uint8_t _s_hpi_DrpUfpStatusPending[2] = {0};  /* 1: pending sink; 2: pending drp. And two PD chips */
static bool g_hpi_prSwapFirstPlug[NO_OF_TYPEC_PORTS] = {true};

#if PD_TRIPPORT_ENABLE
extern void board_pwrSrcEvent(void);
#endif

static struct k_timer g_hpi_prSwapTimerP0, g_hpi_prSwapTimerP1, g_hpi_prSwapTimerP2;
static struct k_work g_hpi_work;
static volatile int g_hpi_work_event;

static void amd_crb_drivers_hpi_prSwapP0(struct k_timer *timer);
static void amd_crb_drivers_hpi_prSwapP1(struct k_timer *timer);
static void amd_crb_drivers_hpi_prSwapP2(struct k_timer *timer);

/* External functions */
extern bool app_ucsi_tunnel_intHandler (uint8_t chipID);  /* for ucsi soft interrupt handler */
extern bool app_ucsi_tunnel_start(uint8_t chipID);  /* start ucsi tunnel */
extern bool app_ucsi_tunnel_end(uint8_t chipID);  /* end ucsi tunnel */

/**
 * amd_crb_drivers_hpi_regAccess
 * 
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]      reg;     registers' offset (see HPI spec.)
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/register width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the HPI spec defined CCGx registers
 */
int amd_crb_drivers_hpi_regAccess(bool isRead, uint16_t reg, void * pBuf, uint32_t len, uint8_t chipID)
{
	uint8_t retry = 3;
	int ret = 0;
	uint8_t i2c_hpi_address = I2C_HPI_ADDRESS_ID1;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 1;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 1;
#endif
	
#if PD_TRIPPORT_ENABLE
	if (chipID == 1) {
		i2c_hpi_address = I2C_HPI_ADDRESS_ID2;
	}
#endif

	while (retry) {
		retry --;
		if (isRead) {
			/* I2C read */
			ret = i2c_hub_write_read(HPI_I2C_PORT, i2c_hpi_address, (uint8_t *)&reg, 2, pBuf, len);
		}
		else {
			/* I2C write */
			ret = i2c_hub_burst_write_multi_cmd(HPI_I2C_PORT, i2c_hpi_address, (uint8_t *)&reg, 2, pBuf, len);
		}
		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
		}

		if (ret == 0)
			return ret;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on %s HPIx[%04X], ret %d, chipID:%d", isRead ? "R" : "W", reg, ret, chipID);
	}

	return ret;
}

#if CONFIG_EC_UCSI_ENABLE
/**
 * amd_crb_drivers_hpi_ucsiRegAccess
 * 
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]      reg;     valid range is from 0 to sizeof(F_UCSI_MAILBOX) - 1.
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/field width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the UCSI mail box from/to CCGx
 *  The UCSI mail box was resident in I2C slave 0x48 by offset 0 if the CCGx FW is V9 or older. 
 *  After V10 it is migrated to I2C slave 0x08 by offset 0xF000.
 */
int amd_crb_drivers_hpi_ucsiRegAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t chipID)
{
	uint8_t retry = 3;
	int ret = 0;
	uint8_t i2c_ucsi_address = I2C_UCSI_ADDRESS_ID1;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 1;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 1;
#endif
	
#if PD_TRIPPORT_ENABLE
	if (chipID == 1)
	{
		i2c_ucsi_address = I2C_UCSI_ADDRESS_ID2;
	}
#endif

	uint16_t regIdx = 0xF000 + reg;

	while (retry) {
			retry --;
			if (isRead) {
				/* I2C read */
				ret = i2c_hub_write_read(HPI_I2C_PORT, i2c_ucsi_address, (uint8_t *)&regIdx, 2, pBuf, len);
			}
			else {
				/* I2C write */
				ret = i2c_hub_burst_write_multi_cmd(HPI_I2C_PORT, i2c_ucsi_address, (uint8_t *)&regIdx, 2, pBuf, len);
			}

			if (ret == 0)
				return ret;
	}
	if (!ret) {
		LOG_ERR("[!!Fatal error!!] on %s UCSIx[%02X], ret %d, chipID:%d", isRead ? "R" : "W", reg, ret, chipID);
	}
	return ret;
}
#endif

/**
 * amd_crb_drivers_hpi_prSwapP0
 *
 * @note
 *  send pr swap to port0
 */
static void amd_crb_drivers_hpi_prSwapP0(struct k_timer *timer)
{
	g_hpi_work_event = 1;
	k_work_submit(&g_hpi_work);
}

/**
 * amd_crb_drivers_hpi_prSwapP0
 *
 * @note
 *  send pr swap to port1
 */
static void amd_crb_drivers_hpi_prSwapP1(struct k_timer *timer)
{
	g_hpi_work_event = 2;
	k_work_submit(&g_hpi_work);
}

/**
 * amd_crb_drivers_hpi_prSwapP0
 *
 * @note
 *  send pr swap to port2
 */
static void amd_crb_drivers_hpi_prSwapP2(struct k_timer *timer)
{
	g_hpi_work_event = 3;
	k_work_submit(&g_hpi_work);
}

/**
 * amd_crb_drivers_hpi_prSwapPx_post
 *
 * @note
 * amd_crb_drivers_hpi_prSwapPx_post work queue callback
 */
static void amd_crb_drivers_hpi_prSwapPx_post(struct k_work *w)
{
	int evt = g_hpi_work_event;

	if (evt == 1) {
		amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_0_IDX, 0, true);
		/* Send PR_SWAP if sink 5V when EC power on */
		if (_s_hpi_port0Pdo.fixed_snk.voltage == 100) { //5V
			LOG_DBG("CYPD P0 sink 5V");
			amd_crb_drivers_hpi_prSwap(TYPEC_PORT_0_IDX, 0);
		}
		
		k_timer_stop(&g_hpi_prSwapTimerP0);
	} else if (evt == 2) {
		amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_1_IDX, 0, true);
		/* Send PR_SWAP if sink 5V when EC power on */
		if (_s_hpi_port1Pdo.fixed_snk.voltage == 100) { //5V
			LOG_DBG("CYPD P1 sink 5V");
			amd_crb_drivers_hpi_prSwap(TYPEC_PORT_1_IDX, 0);
		}
		
		k_timer_stop(&g_hpi_prSwapTimerP1);
	} else if (evt == 3) {
		amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_0_IDX, 1, true);
		/* Send PR_SWAP if sink 5V when EC power on */
		if (_s_hpi_port2Pdo.fixed_snk.voltage == 100) { //5V
			LOG_DBG("CYPD P2 sink 5V");
			amd_crb_drivers_hpi_prSwap(TYPEC_PORT_0_IDX, 1);
		}
		
		k_timer_stop(&g_hpi_prSwapTimerP2);
	}
}

/**
 * amd_crb_drivers_hpi_init
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  The necessary setups hpi before run into main loop.
 */
int amd_crb_drivers_hpi_init (void)
{
	uint32_t u32EventMask;

	/* set up mutex */
	k_mutex_init(&_s_hpi_RdoLock);

	/* Init the timer */
	k_timer_init(&g_hpi_prSwapTimerP0, amd_crb_drivers_hpi_prSwapP0, NULL);
	k_timer_init(&g_hpi_prSwapTimerP1, amd_crb_drivers_hpi_prSwapP1, NULL);
	k_timer_init(&g_hpi_prSwapTimerP2, amd_crb_drivers_hpi_prSwapP2, NULL);

	k_work_init(&g_hpi_work, amd_crb_drivers_hpi_prSwapPx_post);

	/* Port0 chipID 0 event mask */
	amd_crb_drivers_hpi_regAccess(HPI_READ,  0x1024, &u32EventMask, 4, 0);
	/** ************************************
	 * To enable below CCGx events
	 * b5: PD contract Negotiation Complete
	 * b4: Type C Port Disconnect Detected
	 * b3: Type C Port Connect Detected
	 */
	u32EventMask |= 0x00000038;
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1024, &u32EventMask, 4, 0);
	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, 0);  /* 20ms */
	/** Update PDO on each POR initial routine. 
	 * If PD power source is the only AC input (dead battery mode),
	 * PD NEGOTIATION is completed before EC alive.
	 * In this case, the PDO status is missing can cause the dead
	 * battery will not charge.
	 */
	amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_0_IDX, 0, true);

	/* Port1 chipID 0 event mask */
	amd_crb_drivers_hpi_regAccess(HPI_READ,  0x2024, &u32EventMask, 4, 0);
	/** ************************************
	 * To enable below CCGx events
	 * b5: PD contract Negotiation Complete
	 * b4: Type C Port Disconnect Detected
	 * b3: Type C Port Connect Detected
	 */
	u32EventMask |= 0x00000038;
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2024, &u32EventMask, 4, 0);
	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, 0); /* 20ms */
	/** Update PDO on each POR initial routine. 
	 * If PD power source is the only AC input (dead battery mode),
	 * PD NEGOTIATION is completed before EC alive.
	 * In this case, the PDO status is missing can cause the dead
	 * battery will not charge.
	 */
	amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_1_IDX, 0, true);
	
#if PD_TRIPPORT_ENABLE
	/* Port0 chipID 1 event mask */
	amd_crb_drivers_hpi_regAccess(HPI_READ,  0x1024, &u32EventMask, 4, 1);
	/** ************************************
	 * To enable below CCGx events
	 * b5: PD contract Negotiation Complete
	 * b4: Type C Port Disconnect Detected
	 * b3: Type C Port Connect Detected
	 */
	u32EventMask |= 0x00000038;
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1024, &u32EventMask, 4, 1);
	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, 1);  /* 20ms */
	/** Update PDO on each POR initial routine. 
	 * If PD power source is the only AC input (dead battery mode),
	 * PD NEGOTIATION is completed before EC alive.
	 * In this case, the PDO status is missing can cause the dead
	 * battery will not charge.
	 */
	amd_crb_drivers_hpi_updatePdo(TYPEC_PORT_0_IDX, 1, true);
	
	/* Change CCG6SF to 3A Rp */
	amd_crb_drivers_hpi_reconfigTypecRp(TYPEC_PORT_0_IDX, DPM_CMD_SET_RP_3A, 1);
#endif /* end of PD_TRIPPORT_ENABLE */
	return 0;
}

/**
 * amd_crb_drivers_hpi_getDeviceMode
 *
 * @param [in]   chipID;     pd chip index
 *
 * @return device mode app or bootloader
 *
 * @note
 *  get current device mode
 */
uint8_t amd_crb_drivers_hpi_getDeviceMode (uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif
	
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0000, &g_hpi_Regx0000[chipID], 1, chipID);
	return g_hpi_Regx0000[chipID].u8Mode;
}

/**
 * amd_crb_drivers_hpi_getPdStatus
 *
 * @param [in]   port;       port number
 * @param [in]   chipID;     pd chip index
 *
 * @return port status
 *
 * @note
 *  get port status
 */
uint32_t amd_crb_drivers_hpi_getPdStatus(uint8_t port, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1008, &g_hpi_Regx1008[chipID], 4, chipID);
		return g_hpi_Regx1008[chipID].u32PdSt;
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2008, &g_hpi_Regx2008[chipID], 4, chipID);
		return g_hpi_Regx2008[chipID].u32PdSt;
	}
	else
		return 0;
}

/**
 * amd_crb_drivers_hpi_getTypecStatus
 *
 * @param [in]   port;       port number
 * @param [in]   chipID;     pd chip index
 *
 * @return typec status
 *
 * @note
 *  get typec status
 */
uint32_t amd_crb_drivers_hpi_getTypecStatus(uint8_t port, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x100C, &g_hpi_Regx100C[chipID], 1, chipID);
		return g_hpi_Regx100C[chipID].u8Status;
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x200C, &g_hpi_Regx200C[chipID], 1, chipID);
		return g_hpi_Regx200C[chipID].u8Status;
	}
	else
		return 0;
}

/**
 * amd_crb_drivers_hpi_triggerRowReadWrite
 *
 * @param [in]   isRead;       read or write
 * @param [in]   rIdx;         data buffer index
 * @param [in]   chipID;       chip index
 *
 * @return true/false for successful/unsuccessful
 *
 * @note
 *  send row read and write command after buffer write and before buffer read
 */
uint8_t amd_crb_drivers_hpi_triggerRowReadWrite(bool isRead, DRV_HPI_FW_ROW_INDEX rIdx, uint8_t chipID)
{
	uint8_t cmd[8];

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif

	cmd[0] = 'F';
	cmd[1] = (isRead ? 0 : 1);
	cmd[2] = rIdx.rowIdx[2];
	cmd[3] = rIdx.rowIdx[1];

	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000C, cmd, 4, chipID);

	return amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
}

/**
 * amd_crb_drivers_hpi_triggerRowReadWriteCcg8
 *
 * @param [in]   isRead;       read or write
 * @param [in]   rIdx;         data buffer index
 * @param [in]   chipID;       chip index
 *
 * @return true/false for successful/unsuccessful
 *
 * @note
 *  send row read and write command after buffer write and before buffer read for CCG8 specific
 */
uint8_t amd_crb_drivers_hpi_triggerRowReadWriteCcg8(bool isRead, DRV_HPI_FW_ROW_INDEX_CCG8 rIdx, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif

	uint8_t cmd[] = {'F', (isRead ? 0 : 1), rIdx.rowIdx[2], rIdx.rowIdx[1]};
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000C, cmd, sizeof(cmd), chipID);
	return amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
}


/**
 * amd_crb_drivers_hpi_getDeviceResponse
 *
 * @param [in]   chipID;       chip index
 *
 * @return device response
 *
 * @note
 *  read device response by waiting 1000ms
 */
DRV_HPI_DEV_RESPONSE amd_crb_drivers_hpi_getDeviceResponse (uint8_t chipID)
{
	DRV_HPI_DEV_RESPONSE resp = {0};
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return resp;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return resp;
#endif

	do {
		if (0 == amd_crb_drivers_hpi_regAccess(HPI_READ, 0x007E, &resp, 2, chipID)) {
			break;
		}
		k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
		attempts = attempts + ACD_HPI_RETRY_PERIOD;
	} while (attempts < 1000); /* check 1000ms */

	return resp;
}

/**
 * amd_crb_drivers_hpi_getPortResponse
 *
 * @param [in]   port;         port number
 * @param [in]   chipID;       chip index
 *
 * @return port response
 *
 * @note
 *  read port response by wait 1000ms
 */
DRV_HPI_PORT_RESPONSE amd_crb_drivers_hpi_getPortResponse (uint8_t port, uint8_t chipID)
{
	DRV_HPI_PORT_RESPONSE resp = {0};
	uint16_t reg = 0;
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return resp;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return resp;
#endif
	
	if (TYPEC_PORT_0_IDX == port)
		reg = 0x1400;
	else if (TYPEC_PORT_1_IDX == port)
		reg = 0x2400;
	else
		return resp;

	do {
		if (0 == amd_crb_drivers_hpi_regAccess(HPI_READ, reg, &resp, 4, chipID)) {
			break;
		}
		k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
		attempts = attempts + ACD_HPI_RETRY_PERIOD;
	} while (attempts < 1000); /* check 1000ms */

	return resp;
}

/**
 * amd_crb_drivers_hpi_DevEventMsgHandler
 *
 * @param [in]   resp;         response code
 * @param [in]   chipID;       chip index
 *
 * @return if response correct or not
 *
 * @note
 *  check the device response and point the handler
 */
bool amd_crb_drivers_hpi_DevEventMsgHandler (DRV_HPI_DEV_RESPONSE resp, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	LOG_DBG("chipID:%d, Skip to handle DEV asynchronous Message [%02X]", chipID, resp.u8Response[0]);
	switch(resp.u8Response[0]) {
		case HPI_EVENT_OVER_CURRENT_DETECTED:
		case HPI_EVENT_OVER_VOLTAGE_DETECTED:
			break;
		case HPI_EVENT_TYPE_C_PORT_CONNECT:
		case HPI_EVENT_TYPE_C_PORT_DISCONNECT:
		case HPI_EVENT_PD_CONTRACT_NEGOTIATION_COMPLETE:
		default:
			break;
	}

	/*
	 * return TRUE means the message of device was read 
	 * and the according INT BITS in g_hpi_Regx0006 can be cleared.
	 * for now, if none can do here, just return TRUE to clear the INT bit.
	 */
	return true;
}

/**
 * amd_crb_drivers_hpi_DevEcCmdHandler
 *
 * @param [in]   resp;         response code
 * @param [in]   chipID;       chip index
 *
 * @return if response correct or not
 *
 * @note
 *  check the ec command response and point the handler
 */
bool amd_crb_drivers_hpi_DevEcCmdHandler (DRV_HPI_DEV_RESPONSE resp, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	LOG_DBG("chipID:%d, Skip to handle DEV command response [%02X]", chipID, resp.f.msgCode);
	switch(resp.u8Response[0]) {
		case HPI_RESP_NO_RESPONSE:          break;
		case HPI_RESP_SUCCESS:              break;
		case HPI_RESP_FLASH_DATA_AVAILABLE: break;
		case HPI_RESP_INVALID_COMMAND:      break;
		case HPI_RESP_FLASH_UPDATE_FAILED:  break;
		case HPI_RESP_INVALID_FW:           break;
		case HPI_RESP_INVALID_ARGUMENTS:    break;
		case HPI_RESP_NOT_SUPPORTED:        break;
		case HPI_RESP_TRANSACTION_FAILED:   break;
		case HPI_RESP_PD_COMMAND_FAILED:    break;
		case HPI_RESP_UNDEFINED_ERROR:      break;
		case HPI_RESP_READ_PDO_DATA:        break;
		case HPI_RESP_CMD_ABORTED:          break;
		case HPI_RESP_PORT_BUSY:            break;
		case HPI_RESP_MIN_MAX_CURRENT:      break;
		case HPI_RESP_EXT_SRC_CAP:          break;
		case HPI_RESP_UPDATE_AC_LIMIT:
			amd_crb_drivers_hpi_updateAcLimit(chipID);
			break;
		default:
			break;
	}

	/**
	 * return TRUE means the message of device was read 
	 * and the according INT BITS in g_hpi_Regx0006 can be cleared.
	 * for now, if none can do here, just return TRUE to clear the INT bit.
	 */
	return true;
}

/**
 * amd_crb_drivers_hpi_PortEventMsgHandler
 *
 * @param [in]   port;         port number
 * @param [in]   resp;         response code
 * @param [in]   chipID;       chip index
 *
 * @return if response correct or not
 *
 * @note
 *  check the port event response and point the handler
 */
bool amd_crb_drivers_hpi_PortEventMsgHandler (DRV_HPI_PORT_RESPONSE resp, uint8_t port, uint8_t chipID)
{
	bool ack = false;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (port > 1)
		return false;

	switch(resp.u8Response[0]) {
		case HPI_EVENT_OVER_CURRENT_DETECTED:
		case HPI_EVENT_OVER_VOLTAGE_DETECTED:
			break;
		case HPI_EVENT_TYPE_C_PORT_CONNECT:

			/* read typec status after connect event */
			amd_crb_drivers_hpi_getTypecStatus(port, chipID);
			if((port == TYPEC_PORT_0_IDX) && (chipID == 0)) {
				dpm_set_status(TYPEC_PORT_0_IDX)->attach = true;
			}
			else if((port == TYPEC_PORT_1_IDX) && (chipID == 0)) {
				dpm_set_status(TYPEC_PORT_1_IDX)->attach = true;
			}
#if PD_TRIPPORT_ENABLE
			else if((port == TYPEC_PORT_0_IDX) && (chipID == 1)) {
				dpm_set_status(TYPEC_PORT_2_IDX)->attach = true;
			}
#endif
			break;

		/* PDO needs to be updated when disconnect or negotiation done are received */
		case HPI_EVENT_TYPE_C_PORT_DISCONNECT:

			/* read typec status after disconnect event */
			amd_crb_drivers_hpi_getTypecStatus(port, chipID);
			if((port == TYPEC_PORT_0_IDX) && (chipID == 0)) {
				dpm_set_status(TYPEC_PORT_0_IDX)->attach = false;
			}
			else if((port == TYPEC_PORT_1_IDX) && (chipID == 0)) {
				dpm_set_status(TYPEC_PORT_1_IDX)->attach = false;
			}
#if PD_TRIPPORT_ENABLE
			else if((port == TYPEC_PORT_0_IDX) && (chipID == 1)) {
				dpm_set_status(TYPEC_PORT_2_IDX)->attach = false;
			}
#endif
            /* CCGx interrupt and try to process pending DRP or UFP only */
			if (_s_hpi_DrpUfpStatusPending[chipID] == HPI_UFP_ONLY) {
				amd_crb_drivers_hpi_allPortsSinkOnly(chipID);
			}
			else if (_s_hpi_DrpUfpStatusPending[chipID] == HPI_DRP) {
				amd_crb_drivers_hpi_allPortsDrp(chipID);
			}

			/* no break */
		case HPI_EVENT_PD_CONTRACT_NEGOTIATION_COMPLETE:
			amd_crb_drivers_hpi_updatePdo(port, chipID, true);
			if (g_hpi_prSwapFirstPlug[port + (1 << chipID)])
			{
				amd_crb_drivers_hpi_updatePdo(port, chipID, true);
				g_hpi_prSwapFirstPlug[port + (1 << chipID)] = false;
			}
			else
				amd_crb_drivers_hpi_updatePdo(port, chipID, false);
			amd_crb_drivers_hpi_getPdStatus(port, chipID);
#if CONFIG_POWER_SOURCE_MANAGEMENT
#if PD_TRIPPORT_ENABLE	
			board_pwrSrcEvent();
#endif
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
			ack = true;
			break;
		default:
			break;
	}

	if (!ack) {
		LOG_DBG("Skip to handle DEV asynchronous Message [%02X]", resp.u8Response[0]);
	}

	/*
	 * return TRUE means the response of previous command of device was read 
	 * and the according INT BITS in g_hpi_Regx0006 can be cleared.
	 * for now, if none can do here, just return TRUE to clear the INT bit.
	 */
	return true;
}

/**
 * amd_crb_drivers_hpi_PortEcCmdHandler
 *
 * @param [in]   port;         port number
 * @param [in]   resp;         response code
 * @param [in]   chipID;       chip index
 *
 * @return if response correct or not
 *
 * @note
 *  check the ec command event response and point the handler
 */
bool amd_crb_drivers_hpi_PortEcCmdHandler (DRV_HPI_PORT_RESPONSE resp, uint8_t port, uint8_t chipID)
{
	bool ack = false;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	switch(resp.u8Response[0]) {
		case HPI_RESP_NO_RESPONSE:          break;
		case HPI_RESP_SUCCESS:              break;
		case HPI_RESP_FLASH_DATA_AVAILABLE: break;
		case HPI_RESP_INVALID_COMMAND:      break;
		case HPI_RESP_FLASH_UPDATE_FAILED:  break;
		case HPI_RESP_INVALID_FW:           break;
		case HPI_RESP_INVALID_ARGUMENTS:    break;
		case HPI_RESP_NOT_SUPPORTED:        break;
		case HPI_RESP_TRANSACTION_FAILED:   break;
		case HPI_RESP_PD_COMMAND_FAILED:    break;
		case HPI_RESP_UNDEFINED_ERROR:      break;
		case HPI_RESP_READ_PDO_DATA: 
			break;
		case HPI_RESP_CMD_ABORTED:          break;
		case HPI_RESP_PORT_BUSY:            break;
		case HPI_RESP_MIN_MAX_CURRENT:      break;
		case HPI_RESP_EXT_SRC_CAP:          break;
		default:
			break;
	}

	if (!ack) {
		LOG_DBG("Skip to handle DEV command response [%02X]", resp.f.msgCode);
	}

	/**
	 * return TRUE means the response of previous command are read 
	 * and the according INT BITS in g_hpi_Regx0006 can be cleared.
	 * for now, if none can do for a message, just leave a log
	 * and return TRUE.
	 */
	return true;
}

/**
 * amd_crb_drivers_hpi_clearInterrupts
 *
 * @param [in]   ms;           timeout
 * @param [in]   chipID;       chip index
 *
 * @return if clear interrupt successful
 *
 * @note
 *  clear the interrupt
 */
bool amd_crb_drivers_hpi_clearInterrupts (uint32_t ms, uint8_t chipID)
{
	bool ret = false;
	uint32_t attempts = 0;

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (ms) {
		do {
			if (amd_crb_drivers_hpi_intAck(0xFF, chipID)) {
				ret = true;
				break;
			}
			k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
			attempts = attempts + ACD_HPI_RETRY_PERIOD;
		} while (attempts < ms);

	} else {
		while (1) {
			if (amd_crb_drivers_hpi_intAck(0xFF, chipID)) {
				ret = true;
				break;
			}
		}
	}
	return ret;
}

/**
 * amd_crb_drivers_hpi_wait4DevResp
 * 
 * @param [in] respCode;     HPI spec defined message ID or response code.
 * @param [in]       ms;     The maximal delays, 0 means never timeout.
 * @param [in]   chipID;     chip index
 * @return
 *  1 - received 'respCode' before timeout (ms).
 *  0 - otherwise
 *
 * @note
 *  The response will be discarded if it is not the one addressed by 'respCode'.
 *
 *  Use function amd_crb_drivers_hpi_wait4DevRespArray() if caller is interested in more than
 *  one code.
 */
bool amd_crb_drivers_hpi_wait4DevResp (uint8_t respCode, uint32_t ms, uint8_t chipID)
{
	DRV_HPI_DEV_RESPONSE resp = {0};
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	bool ret = false;
	if (ms) {
		do {
			resp = amd_crb_drivers_hpi_getDeviceResponse(chipID);
			amd_crb_drivers_hpi_intAck(0x01, chipID);
			if (resp.u8Response[0] == respCode) {
				ret = true;
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip dev resp %02X when wait for %02X", resp.u8Response[0], respCode);
			}
			k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
			attempts = attempts + ACD_HPI_RETRY_PERIOD;
		} while (attempts < ms);
	} else {
		while (1) {
			resp = amd_crb_drivers_hpi_getDeviceResponse(chipID);
			/* Clear DEV_INTR (bit 0) */
			amd_crb_drivers_hpi_intAck(0x01, chipID);
			if (resp.u8Response[0] == respCode) {
				ret = true;
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip dev resp %02X when wait for %02X", resp.u8Response[0], respCode);
			}
		}
	}
	return ret;
}

/**
 * amd_crb_drivers_hpi_wait4DevRespArray
 * 
 * @param [in] pExpectedCodeArray;     HPI spec defined message ID or response code pointer
 * @param [in]                num;     HPI spec defined message ID or response code length
 * @param [in]                 ms;     The maximal delays, 0 means never timeout.
 * @param [in]             chipID;     chip index
 * @return
 *  1 - received 'respCode' before timeout (ms).
 *  0 - otherwise
 *
 * @note
 *  similar as amd_crb_drivers_hpi_wait4DevResp but provides ability to wait for more than one code.
 */
uint8_t amd_crb_drivers_hpi_wait4DevRespArray (uint8_t * pExpectedCodeArray, uint8_t num, uint32_t ms, uint8_t chipID)
{
	DRV_HPI_DEV_RESPONSE resp = {0};
	uint8_t ret = HPI_INVALID_RESPONSE_CODE;
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (ms) {
		do {
			resp = amd_crb_drivers_hpi_getDeviceResponse(chipID);
			amd_crb_drivers_hpi_intAck(0x01, chipID);
			for (uint8_t i = 0; i < num; i++) {
				if (resp.u8Response[0] == pExpectedCodeArray[i]) {
					ret = pExpectedCodeArray[i];
					break;
				}
			}

			if (ret == HPI_INVALID_RESPONSE_CODE) {
				if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
					LOG_DBG("Skip dev resp %02X when wait for code array", resp.u8Response[0]);
				}
			} else {
				break;
			}
			k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
			attempts = attempts + ACD_HPI_RETRY_PERIOD;
		} while (attempts < ms);
	} else {
		while (1) {
			resp = amd_crb_drivers_hpi_getDeviceResponse(chipID);
			/* Clear DEV_INTR (bit 0) */
			amd_crb_drivers_hpi_intAck(0x01, chipID);
			for (uint8_t i = 0; i < num; i++) {
				if (resp.u8Response[0] == pExpectedCodeArray[i]) {
					ret = pExpectedCodeArray[i];
					break;
				}
			}

			if (ret == HPI_INVALID_RESPONSE_CODE) {
				if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
					LOG_DBG("Skip dev resp %02X when wait for code array", resp.u8Response[0]);
				}
			} else {
				break;
			}
		}
	}

	return ret;
}

/**
 * amd_crb_drivers_hpi_wait4PortResp
 *
 * @param [in] respCode;     HPI spec defined message ID or response code.
 * @param [in]     port;     port number
 * @param [in]       ms;     The maximal delays, 0 means never timeout.
 * @param [in]   chipID;     chip index
 * @return
 *  1 - received 'respCode' before timeout (ms).
 *  0 - otherwise
 *
 * @note
 *  similar as amd_crb_drivers_hpi_wait4PortResp but it's for port exclusive events,
 *  hence caller should pass in 'portIdx' (0 or 1) to address the special port.
 */
bool amd_crb_drivers_hpi_wait4PortResp (uint8_t respCode, uint8_t port, uint32_t ms, uint8_t chipID)
{
	DRV_HPI_PORT_RESPONSE resp = {0};
	bool ret = false;
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (ms) {
		do {
			resp = amd_crb_drivers_hpi_getPortResponse(port, chipID);
			/* Clear PORTx_INTR (bit 1/2 for port0/1) */
			amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
			if (resp.u8Response[0] == respCode) {
				ret = true;
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip port%d resp %02X when wait for %02X", port, resp.u8Response[0], respCode);
			}
			k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
			attempts = attempts + ACD_HPI_RETRY_PERIOD;
		} while (attempts < ms);
	} else {
		while (1) {
			resp = amd_crb_drivers_hpi_getPortResponse(port, chipID);
			/* Clear PORTx_INTR (bit 1/2 for port0/1) */
			amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
			if (resp.u8Response[0] == respCode) {
				ret = true;
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip port%d resp %02X when wait for %02X", port, resp.u8Response[0], respCode);
			}
		}
	}

	return ret;
}

/**
 * amd_crb_drivers_hpi_wait4PortResp
 *
 * @param [in] respCode;     HPI spec defined message ID or response code.
 * @param [in]     data;     data buffer pointer
 * @param [in]     port;     port number
 * @param [in]       ms;     The maximal delays, 0 means never timeout.
 * @param [in]   chipID;     chip index
 * @return
 *  1 - received 'respCode' before timeout (ms).
 *  0 - otherwise
 *
 * @note
 *  similar as amd_crb_drivers_hpi_wait4PortResp but it's for port exclusive events,
 *  hence caller should pass in 'portIdx' (0 or 1) to address the special port.
 *  return the data buffer when get the response match
 */
bool amd_crb_drivers_hpi_wait4PortRespReturn (uint8_t respCode, uint8_t* data, uint8_t port, uint32_t ms, uint8_t chipID)
{
	DRV_HPI_PORT_RESPONSE resp = {0};
	bool ret = false;
	uint32_t attempts = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (ms) {
		do {
			resp = amd_crb_drivers_hpi_getPortResponse(port, chipID);
			
			/* response match and read buffer */
			if (resp.u8Response[0] == respCode) {
				ret = true;
			
				/* return data is not empty */
				if (resp.u8Response[2] + (resp.u8Response[3] << 8)) {
					amd_crb_drivers_hpi_readDatafRDataBuf(port, data, resp.u8Response[2] + (resp.u8Response[3] << 8), false, chipID);
				}
				/* Clear PORTx_INTR (bit 1/2 for port0/1) */
				amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip port%d resp %02X when wait for %02X", port, resp.u8Response[0], respCode);
			}

			/* Clear PORTx_INTR (bit 1/2 for port0/1) */
			amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
			
			k_msleep(ACD_HPI_RETRY_PERIOD); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
			attempts = attempts + ACD_HPI_RETRY_PERIOD;
		} while (attempts < ms);
	} else {
		while (1) {
			resp = amd_crb_drivers_hpi_getPortResponse(port, chipID);

			/* response match and read buffer */
			if (resp.u8Response[0] == respCode) {
				ret = true;

				/* return data is not empty */
				if (resp.u8Response[2] + (resp.u8Response[3] << 8)) {
					amd_crb_drivers_hpi_readDatafRDataBuf(port, data, resp.u8Response[2] + (resp.u8Response[3] << 8), false, chipID);
				}
				/* Clear PORTx_INTR (bit 1/2 for port0/1) */
				amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip port%d resp %02X when wait for %02X", port, resp.u8Response[0], respCode);
			}

			/* Clear PORTx_INTR (bit 1/2 for port0/1) */
			amd_crb_drivers_hpi_intAck(0x01 << (port + 1), chipID);
			if (resp.u8Response[0] == respCode) {
				ret = true;
				break;
			} else if (resp.u8Response[0] != HPI_RESP_NO_RESPONSE) {
				LOG_DBG("Skip port%d resp %02X when wait for %02X", port, resp.u8Response[0], respCode);
			}
		}
	}

	return ret;
}

/**
 * amd_crb_drivers_hpi_intTopHalf
 * 
 * @param [in]    pinSt;     the interrupt input pin status (1 - high, 0 - low)
 * @param [in]   chipID;     chip index
 * @return void
 *
 * @note
 *  CCGx interrupt handling routine. This function is executed 
 *  in the MSP context.
 *
 *  GPIO USBC_I2C_INTn low indicates interrupt active from CCGx.
 *  The handler is splited to top and bottom half to minimize
 *  EC's turn around time.
 *
 *  The function only records the interrupt had occured but don't
 *  handle it.
 */
void amd_crb_drivers_hpi_intTopHalf(uint8_t pinSt, uint8_t chipID)
{
	/* PD INT# low active */
	if (!pinSt) {
		amd_crb_drivers_hpi_intTrigger(chipID);
	}
}

/**
 * amd_crb_drivers_hpi_intTrigger
 *
 * @param [in]    chipID;     chip index
 * @return void
 *
 * @note
 *	assert interrupt flag
 */
void amd_crb_drivers_hpi_intTrigger(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	_s_hpi_intBottomFlag[chipID] = 1;
}

/**
 * amd_crb_drivers_hpi_intPending
 *
 *
 * @note
 *	there are one or two hpi interrupt (physical) pending to process
 */
bool amd_crb_drivers_hpi_intPending(void)
{
	if (_s_hpi_intBottomFlag[0] || _s_hpi_intBottomFlag[1]) {
		return true;
	}
	else
		return false;
}

/**
 * amd_crb_drivers_hpi_intAck
 * 
 * @param [in]      mask;     to clear the bits indicated by 'mask'
 * @return
 *  1 - the bits addressed by mask is not pending any more.
 *  0 - otherwise
 */
bool amd_crb_drivers_hpi_intAck (uint8_t mask, uint8_t chipID)
{
	DRV_HPI_Rx0006_INT_STATE intSt;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	if (mask) {
		if ( 0 == amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0006, &intSt, 1, chipID) ) {
			if (intSt.u8IntSt & mask) {
				mask &= intSt.u8IntSt;
				amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0006, &mask, 1, chipID);

				if (0 == amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0006, &intSt, 1, chipID) && !(intSt.u8IntSt & mask)) {
					return true;
				}
				return false;
			}
			return true;
		}
		return false;
	}
	return true;
}

/**
 * amd_crb_drivers_hpi_intBottomHalf
 * 
 * @param    chipID:        chip index
 * @return void
 *
 * @note
 *  This is the bottom half for CCGx interrupt handling.
 */
void amd_crb_drivers_hpi_intBottomHalf(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (!_s_hpi_intBottomFlag[chipID])
		return;

	/**
	 * clear the INT pending indicator
	 */
	_s_hpi_intBottomFlag[chipID] = 0;
	/**The INT pending indicator may be set up
	 * again by the following proc if the events
	 * indicated by g_hpi_Regx0006 cannot be well handled
	 * immediately.
	 * (the reason could be by limited resource or
	 * any precondition is not meet)
	 */

	uint8_t loopCount = 0;
	while (1) {
		DRV_HPI_Rx0006_INT_STATE intSt = {0};
		DRV_HPI_Rx0006_INT_STATE clearMask = {0};
		int ret = 0;
		ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0006, &intSt, 1, chipID);
		
		/* Assert int flag back if read packet error (no ack) */
		if (ret != 0) {
			_s_hpi_intBottomFlag[chipID] = 1;
		}

		/***********************************************
		 * INTn from CCGx is level trigger;
		 * Do interrupt handler until INTn goes to high.
		 * *********************************************/
		if (intSt.u8IntSt == 0) { /* Use '|' for mutiply PD controllers interrupt pins */
			break;
		}
#if CONFIG_EC_UCSI_ENABLE
		if ((intSt.f.ucsiInt || intSt.f.ucsiInt0) && app_ucsi_tunnel_intHandler(chipID)) {
			clearMask.f.ucsiInt0 = 1;
			clearMask.f.ucsiInt = 1;
		}
#endif
		/* P0 */
		if (intSt.f.port0Int)
		{
			DRV_HPI_PORT_RESPONSE resp = amd_crb_drivers_hpi_getPortResponse(TYPEC_PORT_0_IDX, chipID);
			if (resp.f.msgType) {
				/* 1 - Event or Asynchronous Message */
				if (amd_crb_drivers_hpi_PortEventMsgHandler(resp, TYPEC_PORT_0_IDX, chipID))
					clearMask.f.port0Int = 1;

				LOG_DBG("Port 0 message:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			} else {
				/* 0 - Response to command issued by EC */
				if (amd_crb_drivers_hpi_PortEcCmdHandler(resp, TYPEC_PORT_0_IDX, chipID))
					clearMask.f.port0Int = 1;

				LOG_DBG("Port 0 event:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			}
		}
		/* P1 */
		if (intSt.f.port1Int) /* if no P1 in chip#1, intSt.f.Port1Int will not fire. No side effect(not check chipID here) */
		{
			DRV_HPI_PORT_RESPONSE resp = amd_crb_drivers_hpi_getPortResponse(TYPEC_PORT_1_IDX, chipID);
			if (resp.f.msgType) {
				/* 1 - Event or Asynchronous Message */
				if (amd_crb_drivers_hpi_PortEventMsgHandler(resp, TYPEC_PORT_1_IDX, chipID))
					clearMask.f.port1Int = 1;

				LOG_DBG("Port 1 message:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			} else {
				/* 0 - Response to command issued by EC */
				if (amd_crb_drivers_hpi_PortEcCmdHandler(resp, TYPEC_PORT_1_IDX, chipID))
					clearMask.f.port1Int = 1;

				LOG_DBG("Port 1 event:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			}
		}
		if (intSt.f.devInt) {
			DRV_HPI_DEV_RESPONSE resp = amd_crb_drivers_hpi_getDeviceResponse(chipID);
			if (resp.f.msgType) {
				/* 1 - Event or Asynchronous Message */
				if (amd_crb_drivers_hpi_DevEventMsgHandler(resp, chipID))
					clearMask.f.devInt = 1;

				LOG_DBG("Device message:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			} else {
				/* 0 - Response to command issued by EC */
				if (amd_crb_drivers_hpi_DevEcCmdHandler(resp, chipID))
					clearMask.f.devInt = 1;

				LOG_DBG("Device event:");
				amd_crb_drivers_hpi_dumpEventMessage(resp.u8Response[0]);
			}
		}

		/**
		 * 1s in 'clearMask' indicate they are well handled by current loop.
		 * So we can write them back to g_hpi_Regx0006 to clear them.
		 */
		if (clearMask.u8IntSt) {
			ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0006, &clearMask, 1, chipID);
		}
		
		/**
		 * if handled bits (1s in 'clearMask') are less than the bits read 
		 * from g_hpi_Regx0006 at the beginning of this loop (1s in 'intSt'), it means
		 * something cannot be handled well at current point.
		 *
		 */
		if ( (intSt.u8IntSt ^ clearMask.u8IntSt ) &&
			 (intSt.u8IntSt & ~clearMask.u8IntSt) ) {

			loopCount++;

			/*
			 * if we had tried several times, set the INT pending indicator to 1
			 * hence leave it be handled later (i.e next time when runs to this handler)
			 */
			if (loopCount > 3) {
				LOG_DBG("Something cannot be handled by current interrupt loop, tried %d times.", loopCount);
				_s_hpi_intBottomFlag[chipID] = 1;
				break;
			}
		}
		/**
		 * Note that CCGx will quening the pending bits by their side.
		 * So if clearMask equal to intSt, it means the top event of each bits are
		 * well handled, clear the top one by write clearMask to g_hpi_Regx0006 and check
		 * if there are any quened events.
		 * Hence loopCount will not counting in this case.
		 */
	}
}

/**
 * amd_crb_drivers_hpi_curPdo
 *
 * @param [in]     port;     port number
 * @param [in]   chipID;     chip index
 * @return current PDO
 *
 * @note
 *  CCGx read current buffer PDO
 */
uint32_t amd_crb_drivers_hpi_curPdo (uint8_t port, uint8_t chipID)
{
	uint32_t ret = 0;
	
	switch (port) {
		case TYPEC_PORT_0_IDX:

			if (chipID == 0)
				ret = _s_hpi_port0Pdo.val;
			else 
				ret = _s_hpi_port2Pdo.val;
			break;
		case TYPEC_PORT_1_IDX:
			if (chipID == 0)
				ret = _s_hpi_port1Pdo.val;
			break;
		default:
			break;
	}
	return ret;
}

/**
 * amd_crb_drivers_hpi_updatePdo
 *
 * @param [in]     port;     port number
 * @param [in]   chipID;     chip index
 * @param [in]  pr_swap;     if trigger pr swap
 * @return latest PDO
 *
 * @note
 *  CCGx read latest buffer PDO
 */
uint32_t amd_crb_drivers_hpi_updatePdo (uint8_t port, uint8_t chipID, bool pr_swap)
{
	uint16_t rdoAddr = 0;
	uint16_t pdStatusAddr = 0;
	uint32_t * pRDO = NULL;
	uint32_t ret = 0;

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 0;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 0;
#endif	

	switch (port) {
		case TYPEC_PORT_0_IDX:
			rdoAddr = 0x1010;
			pdStatusAddr = 0x1008;
			if (chipID == 0)
				pRDO = &_s_hpi_port0Pdo.val;
			else 
				pRDO = &_s_hpi_port2Pdo.val;
			break;
		case TYPEC_PORT_1_IDX:
			rdoAddr = 0x2010;
			pdStatusAddr = 0x2008;
			if (chipID == 0)
				pRDO = &_s_hpi_port1Pdo.val;
			break;
		default:
			break;
	}

	if (rdoAddr) {
		uint32_t curRdo = 0;
		DRV_HPI_PD_STATUS pdSt = {0};
		int ack = amd_crb_drivers_hpi_regAccess(HPI_READ, pdStatusAddr, (uint8_t *) &pdSt, sizeof(pdSt), chipID);

		if (pdSt.f.contractStatus && !pdSt.f.crtPortPwrRole) {
			/* if 'PD contract exists with port partner' and 'the port power pole is Source' */
			ack = amd_crb_drivers_hpi_regAccess(HPI_READ, rdoAddr, (uint8_t *) &curRdo, 4, chipID);
			if (ack != 0) // 2 bytes address and 4 bytes data
				curRdo = 0;
		} else {
			curRdo = 0;
		}
		ret = curRdo;

		k_mutex_lock(&_s_hpi_RdoLock, K_FOREVER);
		if (pRDO != NULL)
			*pRDO = curRdo;
		k_mutex_unlock(&_s_hpi_RdoLock);
		LOG_DBG("Type-C port %d PDO is updated to %08X, chipID:%d", port, curRdo, chipID);
		
		if (pr_swap && (dpm_set_status(port)->cur_port_role == PRT_ROLE_SINK)) {
			if ((port == TYPEC_PORT_0_IDX) && (chipID == 0)) {
				if(_s_hpi_port0Pdo.fixed_snk.voltage == 100)
					k_timer_start(&g_hpi_prSwapTimerP0, K_MSEC(3000), K_NO_WAIT);
			}
			else if ((port == TYPEC_PORT_1_IDX) && (chipID == 0)) {
				if(_s_hpi_port1Pdo.fixed_snk.voltage == 100)
					k_timer_start(&g_hpi_prSwapTimerP1, K_MSEC(3000), K_NO_WAIT);
			}
#if PD_TRIPPORT_ENABLE
			else if ((port == TYPEC_PORT_0_IDX) && (chipID == 1)) {
				if(_s_hpi_port2Pdo.fixed_snk.voltage == 100)
					k_timer_start(&g_hpi_prSwapTimerP2, K_MSEC(3000), K_NO_WAIT);
			}
#endif
		}
	}
	return ret;
}


/**
 * amd_crb_drivers_hpi_updateAcLimit
 *
 * @param [in]   chipID;     chip index
 * @return current ac limit
 *
 * @note
 *  CCGx read ac limit
 */
uint32_t amd_crb_drivers_hpi_updateAcLimit (uint8_t chipID)
{	
	extern volatile uint8_t g_ccgxSyncUpFlag;
	uint8_t retry = 3;
	uint32_t ret = 0;

	k_mutex_lock(&_s_hpi_RdoLock, K_FOREVER);

	while (retry--) {
		int ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0046, (uint8_t *) &_s_hpi_curRdo.val, 4, chipID);
		if (ret != 0) { // 2 bytes address and 4 bytes data
			_s_hpi_curRdo.val = 0;
		} else {
			break;
		}
	}

	g_ccgxSyncUpFlag = 1;
	ret = _s_hpi_curRdo.val;

	k_mutex_unlock(&_s_hpi_RdoLock);
	LOG_DBG("*************** CCGx0046 AC Limit is updated to %08X", _s_hpi_curRdo.val);
	
	return ret;
}

/**
 * amd_crb_drivers_hpi_lastAcLimit
 *
 * @param [in]   chipID;     chip index
 * @return current ac limit
 *
 * @note
 *  get latest ac limit
 */
uint32_t amd_crb_drivers_hpi_lastAcLimit (uint8_t chipID)
{
	uint32_t ret = 0;

	k_mutex_lock(&_s_hpi_RdoLock, K_FOREVER);
	ret = _s_hpi_curRdo.val;
	k_mutex_unlock(&_s_hpi_RdoLock);

	return ret;
}

/**
 * amd_crb_drivers_hpi_rowDataCheck
 *
 * @param [in]   pRow;     raw data pointer
 * @return if checksum valid
 *
 * @note
 *  check the raw data checksum if it is valid
 */
bool amd_crb_drivers_hpi_rowDataCheck (DRV_HPI_FW_ONE_ROW * pRow)
{
	if (pRow->f.signature != ':' || pRow->f.rowSize != 0x0001 /* FIXME: 0x0100 (256) */ || pRow->f.ending != 0x0A0D)
		return false;

	uint8_t checkSum = 0;
	for (uint32_t i = 1; i < sizeof(DRV_HPI_FW_ONE_ROW) - 2; i++) {
		checkSum += pRow->thisRow[i];
	}
	if (checkSum != 0)
		return false;

	return true;
}

/**
 * amd_crb_drivers_hpi_rowDataCheckCcgx
 *
 * @param [in]   pRow;     raw data pointer
 * @return if checksum valid
 *
 * @note
 *  check the raw data checksum if it is valid for ccgx
 */
bool amd_crb_drivers_hpi_rowDataCheckCcgx (DRV_HPI_FW_ONE_ROW_CCGx * pRow)
{
	if (pRow->f.signature != ':' || pRow->f.rowSize != 0x8000 /* FIXME: 0x0080 (128) */ || pRow->f.ending != 0x0A0D)
		return false;

	uint8_t checkSum = 0;
	for (uint32_t i = 1; i < sizeof(DRV_HPI_FW_ONE_ROW_CCGx) - 2; i++) {
		checkSum += pRow->thisRow[i];
	}
	if (checkSum != 0)
		return false;

	return true;
}

/**
 * amd_crb_drivers_hpi_writePayLoad2DataBuf
 *
 * @param [in]   pBuf;     data pointer
 * @param [in]    len;     data length
 * @param [in] chipID;     chip index
 * @return successful or not
 *
 * @note
 *  write data to pd payload data buffer
 */
bool amd_crb_drivers_hpi_writePayLoad2DataBuf(uint8_t * pBuf, uint32_t len, uint8_t chipID)
{
#define _DRV_HPI_BLOCK  64
	int ret = 0;

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif	
	
	for(uint32_t i = 0; i < len; i += _DRV_HPI_BLOCK) {
		uint8_t segLen = (len - i) > _DRV_HPI_BLOCK ? _DRV_HPI_BLOCK : (len - i);
		ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0200 + i, &pBuf[i], segLen, chipID);
		amd_crb_drivers_hpi_clearInterrupts(200, chipID);  /* 200ms */

		if (ret != 0) return false;
	}

	return true;
}

/**
 * amd_crb_drivers_hpi_ReadDatafRDataBuf
 *
 * @param [in]   port;     port number
 * @param [in]   pBuf;     data pointer
 * @param [in]    len;     data length
 * @param [in]   page;     page index
 * @param [in] chipID;     chip index
 * @return successful or not
 *
 * @note
 *  read data to pd Rdata buffer
 */
bool amd_crb_drivers_hpi_readDatafRDataBuf(uint8_t port, uint8_t *pBuf, uint32_t len, bool page, uint8_t chipID)
{
	int ret = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		if (page == false) {
			ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1404, pBuf, len, chipID);
			if (ret != 0) return false;
		}
		else {
			ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1500, pBuf, len, chipID);
			if (ret != 0) return false;
		}
	}
	else if (port == TYPEC_PORT_1_IDX) {
		if (page == false) {
			ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2404, pBuf, len, chipID);
			if (ret != 0) return false;
		}
		else {
			ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2500, pBuf, len, chipID);
			if (ret != 0) return false;
		}
	}
	return true;
}

/**
 * amd_crb_drivers_hpi_writeData2WDataBuf
 *
 * @param [in]   port;     port number
 * @param [in]   pBuf;     data pointer
 * @param [in]    len;     data length
 * @param [in]   page;     page index
 * @param [in] chipID;     chip index
 * @return successful or not
 *
 * @note
 *  write data to pd Wdata buffer
 */
bool amd_crb_drivers_hpi_writeData2WDataBuf(uint8_t port, uint8_t *pBuf, uint32_t len, bool page, uint8_t chipID)
{
	int ret = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		if (page == false) {
			ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1800, pBuf, len, chipID);
			amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20,chipID); /* 20ms */

			if (ret != 0) return false;
		}
		else {
			ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1900, pBuf, len, chipID);
			amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */

			if (ret != 0) return false;
		}
	}
	else if (port == TYPEC_PORT_1_IDX) {
		if (page == false) {
			ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2800, pBuf, len, chipID);
			amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */

			if (ret != 0) return false;
		}
		else {
			ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2900, pBuf, len, chipID);
			amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */

			if (ret != 0) return false;
		}
	}
	
	return true;
}

/**
 * amd_crb_drivers_hpi_reconfigPortRole
 *
 * @param [in]       port;     port number
 * @param [in]  port_role;     port role
 * @param [in]     chipID;     chip index
 *
 * @note
 *  reconfig the CCGx port status
 */
void amd_crb_drivers_hpi_reconfigPortRole(uint8_t port, uint8_t port_role, uint8_t chipID)
{
	uint8_t cmd = 0x14;
	uint8_t port_setting[4];
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		/* register 0x1006 + commnad 0x14 + settings [port_role, dft_port_role, drp_toggle_en, try_src_en]*/
		if (port_role == PRT_ROLE_SINK) {
			/* load the setting into the write_mem */
			port_setting[0] = PRT_ROLE_SINK;
			port_setting[1] = PRT_ROLE_SINK;
			port_setting[2] = 0; /* disable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		else if (port_role == PRT_ROLE_SOURCE) {
			/* load the setting into the write_mem */
			port_setting[0] = PRT_ROLE_SOURCE;
			port_setting[1] = PRT_ROLE_SOURCE;
			port_setting[2] = 0; /* disable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		else {  /* PRT_DUAL */
			/* load the setting into the write_mem */
			port_setting[0] = PRT_DUAL;
			port_setting[1] = PRT_ROLE_SINK;
			port_setting[2] = 1; /* enable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		amd_crb_drivers_hpi_writeData2WDataBuf(TYPEC_PORT_0_IDX, port_setting, 4, false, chipID);
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
	}
	else if (port == TYPEC_PORT_1_IDX) {
		/* register 0x2006 + commnad 0x14 + settings [port_role, dft_port_role, drp_toggle_en, try_src_en]*/
		if (port_role == PRT_ROLE_SINK) {
			/* load the setting into the write_mem */
			port_setting[0] = PRT_ROLE_SINK;
			port_setting[1] = PRT_ROLE_SINK;
			port_setting[2] = 0; /* disable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		else if (port_role == PRT_ROLE_SOURCE) {
			/* load the setting into the write_mem */
			port_setting[0] = PRT_ROLE_SOURCE;
			port_setting[1] = PRT_ROLE_SOURCE;
			port_setting[2] = 0; /* disable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		else {  /* PRT_DUAL */
			/* load the setting into the write_mem */
			port_setting[0] = PRT_DUAL;
			port_setting[1] = PRT_ROLE_SINK;
			port_setting[2] = 1; /* enable toggle */
			port_setting[3] = 0; /* disable try.src */
		}
		amd_crb_drivers_hpi_writeData2WDataBuf(TYPEC_PORT_1_IDX, port_setting, 4, false, chipID);
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */
	}
}

/**
 * amd_crb_drivers_hpi_reconfigTypecRp
 *
 * @param [in]       port;     port number
 * @param [in]  typec_cmd;     port type
 * @param [in]     chipID;     chip index
 *
 * @note
 *  reconfig the CCGx port type
 */
void amd_crb_drivers_hpi_reconfigTypecRp(uint8_t port, dpm_typec_cmd_t typec_cmd, uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif

	if (port == TYPEC_PORT_0_IDX) {
		switch (typec_cmd) {
			case DPM_CMD_SET_RP_DFLT:
			case DPM_CMD_SET_RP_1_5A:
			case DPM_CMD_SET_RP_3A:
				/* trigger command */
				amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &typec_cmd, 1, chipID);
				amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
				break;
			default:
				break;
		}
	}
	else if (port == TYPEC_PORT_1_IDX) {
		switch (typec_cmd) {
			case DPM_CMD_SET_RP_DFLT:
			case DPM_CMD_SET_RP_1_5A:
			case DPM_CMD_SET_RP_3A:
				/* trigger command */
				amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &typec_cmd, 1, chipID);
				amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */
				break;
			default:
				break;
		}
	}
}

/**
 * amd_crb_drivers_hpi_allPortsSinkOnly
 *
 * @param [in]     chipID;     chip index
 *
 * @note
 *  all ports sink only
 */
void amd_crb_drivers_hpi_allPortsSinkOnly(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_1_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_1_IDX, chipID);

	if (((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached)) ||
		((g_hpi_Regx2008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx200C[chipID].f.typeDeviceAttached))) {

		if (chipID == 0) {
			if (_s_hpi_port0Pdo.fixed_snk.voltage > 100 || _s_hpi_port1Pdo.fixed_snk.voltage > 100) {
				_s_hpi_DrpUfpStatusPending[chipID] = HPI_UFP_ONLY; /* pending sink */
				LOG_DBG("Pending CCGx UFP:%d",chipID);
				return;
			}
		}
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1) {
			if (_s_hpi_port2Pdo.fixed_snk.voltage > 100) {
				_s_hpi_DrpUfpStatusPending[chipID] = HPI_UFP_ONLY; /* pending sink */
				LOG_DBG("Pending CCGx UFP:%d",chipID);
				return;
			}
		}
#endif
	}
	
	LOG_DBG("CCGx UFP effect:%d", chipID);

	/* Clear the pending status */
	_s_hpi_DrpUfpStatusPending[chipID] = 0;

#if CONFIG_EC_UCSI_ENABLE
	/* Stop UCSI */
	app_ucsi_tunnel_end(chipID);
#endif

	/* Disable Port0/1 */
	DRV_HPI_Rx002C_PORT_ENABLE HPIx002C = {0xFF};
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if (HPIx002C.f.port0En || HPIx002C.f.port1En) {
		HPIx002C.f.port0En = 0;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 0;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("CCGx:%d disable port", chipID);
	}
		
	if (chipID == 0) {  /* chip 0 for two ports */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_ROLE_SINK, chipID);
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_1_IDX, PRT_ROLE_SINK, chipID);
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) {  /* chip 1 for one port */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_ROLE_SINK, chipID);
	}
#endif
	/* Enable Port0/1 */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if ((HPIx002C.f.port0En == 0) || (HPIx002C.f.port1En == 0)) {
		HPIx002C.f.port0En = 1;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 1;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("CCGx:%d enable port", chipID);
	}

#if CONFIG_EC_UCSI_ENABLE
	/* Enable UCSI */
	app_ucsi_tunnel_start(chipID);
#endif
}

/**
 * amd_crb_drivers_hpi_allPortsSourceOnly
 *
 * @param [in]     chipID;     chip index
 *
 * @note
 *  all ports sink only
 */
void amd_crb_drivers_hpi_allPortsSourceOnly(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif

	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_1_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_1_IDX, chipID);

	LOG_DBG("CCGx DFP effect:%d", chipID);

	/* Clear the pending status */
	_s_hpi_DrpUfpStatusPending[chipID] = 0;

#if CONFIG_EC_UCSI_ENABLE
	/* Stop UCSI */
	app_ucsi_tunnel_end(chipID);
#endif

	/* Disable Port0/1 */
	DRV_HPI_Rx002C_PORT_ENABLE HPIx002C = {0xFF};
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if (HPIx002C.f.port0En || HPIx002C.f.port1En) {
		HPIx002C.f.port0En = 0;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 0;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("CCGx:%d disable port", chipID);
	}

	if (chipID == 0) {  /* chip 0 for two ports */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_ROLE_SOURCE, chipID);
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_1_IDX, PRT_ROLE_SOURCE, chipID);
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) {  /* chip 1 for one port */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_ROLE_SOURCE, chipID);
	}
#endif
	/* Enable Port0/1 */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if ((HPIx002C.f.port0En == 0) || (HPIx002C.f.port1En == 0)) {
		HPIx002C.f.port0En = 1;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 1;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("CCGx:%d enable port", chipID);
	}

#if CONFIG_EC_UCSI_ENABLE
	/* Enable UCSI */
	app_ucsi_tunnel_start(chipID);
#endif
}

/**
 * amd_crb_drivers_hpi_allPortsDrp
 *
 * @param [in]     chipID;     chip index
 *
 * @note
 *  all ports drp
 */
void amd_crb_drivers_hpi_allPortsDrp(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_1_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
	amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_1_IDX, chipID);

	if (((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached)) ||
		((g_hpi_Regx2008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx200C[chipID].f.typeDeviceAttached))) {

		if (chipID == 0) {
			if (_s_hpi_port0Pdo.fixed_snk.voltage > 100 || _s_hpi_port1Pdo.fixed_snk.voltage > 100) {
				_s_hpi_DrpUfpStatusPending[chipID] = HPI_DRP; /* pending drp */
				LOG_DBG("Pending CCGx UFP:%d",chipID);
				return;
			}
		}
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1) {
			if (_s_hpi_port2Pdo.fixed_snk.voltage > 100) {
				_s_hpi_DrpUfpStatusPending[chipID] = HPI_DRP; /* pending drp */
				LOG_DBG("Pending CCGx UFP:%d",chipID);
				return;
			}
		}
#endif
	}
	
	LOG_DBG("CCGx UFP effect:%d", chipID);
	
	/* Clear the pending status */	
	_s_hpi_DrpUfpStatusPending[chipID] = 0;

#if CONFIG_EC_UCSI_ENABLE
	/* Stop UCSI */
	app_ucsi_tunnel_end(chipID);
#endif

	/* Disable Port0/1 */
	DRV_HPI_Rx002C_PORT_ENABLE HPIx002C = {0xFF};
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if (HPIx002C.f.port0En || HPIx002C.f.port1En) {
		HPIx002C.f.port0En = 0;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 0;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("CCGx:%d disable port", chipID);
	}
	
	if (chipID == 0)  { /* chip 0 for two ports */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_DUAL, chipID);
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_1_IDX, PRT_DUAL, chipID);
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) {  /* chip 1 for one port */
		amd_crb_drivers_hpi_reconfigPortRole(TYPEC_PORT_0_IDX, PRT_DUAL, chipID);
	}
#endif
	/* Enable Port0/1 */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &HPIx002C, 1, chipID);
	if ((HPIx002C.f.port0En == 0) || (HPIx002C.f.port1En == 0)) {
		HPIx002C.f.port0En = 1;
		if (chipID == 0)
			/* only PD0 has second port */
			HPIx002C.f.port1En = 1;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &HPIx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID); /* 200ms */
		LOG_DBG("CCGx:%d enable port", chipID);
	}

#if CONFIG_EC_UCSI_ENABLE
	/* Enable UCSI */
	app_ucsi_tunnel_start(chipID);
#endif
}

/**
 * amd_crb_drivers_hpi_getHigherPortSnkPdo
 *
 * @return pd_do_t
 *
 * @note
 *  Return the higher sink Rdo.
 */
pd_do_t amd_crb_drivers_hpi_getHigherPortSnkPdo(void)
{
	_s_hpi_xSysPdo = _s_hpi_port0Pdo;
#if PD_DUALPORT_ENABLE
	if ((_s_hpi_xSysPdo.fixed_snk.op_current * _s_hpi_xSysPdo.fixed_snk.voltage) <
	(_s_hpi_port1Pdo.fixed_snk.op_current * _s_hpi_port1Pdo.fixed_snk.voltage)) {
		_s_hpi_xSysPdo = _s_hpi_port1Pdo;
	}
#endif
#if PD_TRIPPORT_ENABLE
	if ((_s_hpi_xSysPdo.fixed_snk.op_current * _s_hpi_xSysPdo.fixed_snk.voltage) <
			 _s_hpi_port2Pdo.fixed_snk.op_current * _s_hpi_port2Pdo.fixed_snk.voltage) {
		_s_hpi_xSysPdo = _s_hpi_port2Pdo;
	}
#endif
	
	return _s_hpi_xSysPdo;
}

/**
 * amd_crb_drivers_hpi_getHigherPortNumSnk
 *
 * @return sink number
 *
 * @note
 *  Return the higher sink number.
 */
uint8_t amd_crb_drivers_hpi_getHigherPortNumSnk(void)
{
	uint8_t port = TYPEC_PORT_0_IDX;
	
	_s_hpi_xSysPdo = _s_hpi_port0Pdo;
#if PD_DUALPORT_ENABLE
	if ((_s_hpi_xSysPdo.fixed_snk.op_current * _s_hpi_xSysPdo.fixed_snk.voltage) <
	(_s_hpi_port1Pdo.fixed_snk.op_current * _s_hpi_port1Pdo.fixed_snk.voltage)) {
		_s_hpi_xSysPdo = _s_hpi_port1Pdo;
		port = TYPEC_PORT_1_IDX;
	}
#endif
#if PD_TRIPPORT_ENABLE
	if ((_s_hpi_xSysPdo.fixed_snk.op_current * _s_hpi_xSysPdo.fixed_snk.voltage) <
			 _s_hpi_port2Pdo.fixed_snk.op_current * _s_hpi_port2Pdo.fixed_snk.voltage) {
		_s_hpi_xSysPdo = _s_hpi_port2Pdo;
		port = TYPEC_PORT_2_IDX;
	}
#endif
	
	/* return 0xFF if all PDO empty */
	if (_s_hpi_xSysPdo.val)
		return port;
	else
		return 0xFF;
}

/**
 * amd_crb_drivers_hpi_sinkPathEnable
 *
 * @param [in]     port;     port number
 *
 * @note
 *  enable port sink path
 */
void amd_crb_drivers_hpi_sinkPathEnable(uint8_t port)
{
	uint8_t inputBuffer;
	uint8_t chipID = 0;
	
	if (port == TYPEC_PORT_0_IDX) {
		/* Enable port0 */
		chipID = 0;
		inputBuffer = 0x03;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		
		/* Disable port1 */
		chipID = 0;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */
		
		/* Disable port2 */
		chipID = 1;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		LOG_DBG("CYPD Enable:P0, Disable:P1/P2");
	}
	else if (port == TYPEC_PORT_1_IDX) {
		/* Disable port0 */
		chipID = 0;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		
		/* Enable port1 */
		chipID = 0;
		inputBuffer = 0x03;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */
		
		/* Disable port2 */
		chipID = 1;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		LOG_DBG("CYPD Enable:P1, Disable:P0/P2");
	}
	else if (port == TYPEC_PORT_2_IDX) {
		/* Disable port0 */
		chipID = 0;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		
		/* Disable port1 */
		chipID = 0;
		inputBuffer = 0x01;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */
		
		/* Enable port2 */
		chipID = 1;
		inputBuffer = 0x03;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
		LOG_DBG("CYPD Enable:P2, Disable:P0/P1");
	}
}

/**
 * amd_crb_drivers_hpi_crossbarMailBox
 *
 * @param [in]     isRead;     read or write
 * @param [in]       port;     port number
 * @param [in]       data;     data pointer
 * @param [in]     chipID;     chip index
 * @return successful or not
 *
 * @note
 *  use to test the hpi crossbar mailbox feature
 */
uint8_t amd_crb_drivers_hpi_crossbarMailBox(bool isRead, uint8_t port, uint8_t* data, uint8_t chipID)
{
	uint8_t cmd = 0x3C;
	uint8_t i2c_packet[24];
	uint8_t address = 0;
	
	if (port == TYPEC_PORT_0_IDX && chipID == 0) {
		address = 0x54;
	}
	else if (port == TYPEC_PORT_1_IDX && chipID == 0) {
		address = 0x58;
	}
#if PD_TRIPPORT_ENABLE
	else if (port == TYPEC_PORT_0_IDX && chipID == 1) {
		address = 0x5C;
	}
#endif
	/* load the setting into the write_mem */
	i2c_packet[0] = (address << 1) + isRead;
	i2c_packet[1] = 0;         /* CCGx SCB index */ 
	i2c_packet[2] = 0;         /* Reserved */
	i2c_packet[3] = 1;         /* Custom command code: 
	                              0x00 -> AMD re-timer tunneling command
                                0x01 -> AMD mailbox tunneling command	*/
	i2c_packet[4] = 4;         /* Number of bytes */
	if (isRead)
		i2c_packet[5] = 0xA0;      /* I2C register offset */
	else
		i2c_packet[5] = 0x43;      /* I2C register offset */
	
	memcpy(&i2c_packet[6], data, 4);
	
	/* Full the write data buf first */
	amd_crb_drivers_hpi_writeData2WDataBuf(port, i2c_packet, (6 + 4), false, chipID);
	
	/* trigger command */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
	}
	else
		return 0;
	
	/* Wait response 0x1D */
	amd_crb_drivers_hpi_wait4PortRespReturn(HPI_RESP_I2C_REG, data, port, 20, chipID);  /* 20ms */

	return 1;
}

/**
 * amd_crb_drivers_hpi_prSwap
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 *  send the pr swap command
 */
void amd_crb_drivers_hpi_prSwap(uint8_t port, uint8_t chipID)
{
	uint8_t cmd = 0x06;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 5, chipID);  /* 5ms */
	}
	else if (port == TYPEC_PORT_1_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_portDisable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 *  disable port
 */
void amd_crb_drivers_hpi_portDisable(uint8_t port, uint8_t chipID)
{
	uint8_t cmd = 0x11;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 5, chipID); /* 5ms */
	}
	else if (port == TYPEC_PORT_1_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_portEnable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 *  enable port
 */
void amd_crb_drivers_hpi_portEnable(uint8_t port, uint8_t chipID)
{	
	uint8_t data = 1 << port;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	/* trigger command */
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &data, 1, chipID);
	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
}

/**
 * amd_crb_drivers_hpi_resetChip
 *
 * @param [in]     chipID;     chip index
 *
 * @note
 *  reset pd controller chip
 */
void amd_crb_drivers_hpi_resetChip(uint8_t chipID)
{
	uint8_t cmd[2] = {'R', 0x01};
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0008, &cmd, 2, chipID);
	/* no need to wait for response as pd is reseted */
}

/**
 * amd_crb_drivers_hpi_dataReset
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 *  send the data reset command
 */
void amd_crb_drivers_hpi_dataReset(uint8_t port, uint8_t chipID)
{
	uint8_t cmd = 0x19;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (port == TYPEC_PORT_0_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 5, chipID);  /* 5ms */
	}
	else if (port == TYPEC_PORT_1_IDX) {
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_usb4Tbt3Enable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 * enable the usb4 and tbt3 function
 */
void amd_crb_drivers_hpi_usb4Tbt3Enable(uint8_t port, uint8_t chipID)
{
	uint8_t data = 0;
	
	/* enable usb4 */
	if(port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x105C, &data, 1, chipID);
		
	  /* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x60;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x105C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if(port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x205C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x60;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x205C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_usb4Tbt3Enable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 * disable the usb4 and tbt3 function
 */
void amd_crb_drivers_hpi_usb4Tbt3Disable(uint8_t port, uint8_t chipID)
{
	uint8_t data = 0;
	
	/* disable usb4 */
	if(port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x105C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data &= 0x9F;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x105C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if(port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x205C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data &= 0x9F;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x205C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_usb4EnableTbt3Disable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 * enable the usb4 and disable the tbt3 function
 */
void amd_crb_drivers_hpi_usb4EnableTbt3Disable(uint8_t port, uint8_t chipID)
{
	uint8_t data = 0;
	
	/* enable usb4 */
	if(port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x105C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x20;
		data &= 0xBF;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x105C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if(port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x205C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x20;
		data &= 0xBF;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x205C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_usb4DisableTbt3Enable
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 * disable the usb4 and enable the tbt3 function
 */
void amd_crb_drivers_hpi_usb4DisableTbt3Enable(uint8_t port, uint8_t chipID)
{
	uint8_t data = 0;
	
	/* disable usb4 */
	if(port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x105C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x40;
		data &= 0xDF;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x105C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if(port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x205C, &data, 1, chipID);
		
		/* bit5 (0~7) enable/disable USB4 */
		/* bit6 (0~7) enable/disable TBT3 */
		data |= 0x40;
		data &= 0xDF;
		
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x205C, &data, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_readSrcPdo
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 *
 * @note
 * read source pdo
 */
void amd_crb_drivers_hpi_readSrcPdo(uint8_t port, uint8_t chipID)
{
	uint8_t cmd = 0x20;

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif
	
	if (port == TYPEC_PORT_0_IDX)
	{
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if (port == TYPEC_PORT_1_IDX)
	{
		/* trigger command */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
}

/**
 * amd_crb_drivers_hpi_writeSrcPdo
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 * @param [in]       lock;     lock source pdo to 5V/1.5A
 *
 * @note
 * write source pdo
 */
void amd_crb_drivers_hpi_writeSrcPdo(uint8_t port, uint8_t chipID, bool lock)
{
	uint8_t cmd = 0x01;
	uint8_t inputBuffer[32];
	
	/* load the setting into the write_mem */
	inputBuffer[0] = 'P';
	inputBuffer[1] = 'C';         
	inputBuffer[2] = 'R';       
	inputBuffer[3] = 'S';     

	if (lock == true) {
		dpm_set_status(port)->src_pdo[0].fixed_src.max_current = 0x96;
	}
	else  {
		dpm_set_status(port)->src_pdo[0].fixed_src.max_current = 0x12C;
	}

	memcpy (&inputBuffer[4], &dpm_set_status(port)->src_pdo[0], 28);
	
	/* Full the write data buf first */
	amd_crb_drivers_hpi_writeData2WDataBuf(port, inputBuffer, 32, false, chipID);
	
	/* trigger command */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1004, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2004, &cmd, 1, chipID);
		amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 5, chipID);  /* 5ms */
	}
	else
		return;
	
	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 20, chipID);  /* 20ms */

	return;
}

/**
 * amd_crb_drivers_hpi_recoveryPorts
 *
 *
 * @note
 *  recovery all port setting
 */
void amd_crb_drivers_hpi_recoveryPorts(void)
{
	if (dpm_get_info(TYPEC_PORT_0_IDX)->port_disable) {
		amd_crb_drivers_hpi_portEnable(TYPEC_PORT_0_IDX, 0);
		dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = false;
	}

	if (dpm_get_info(TYPEC_PORT_1_IDX)->port_disable) {
		amd_crb_drivers_hpi_portEnable(TYPEC_PORT_1_IDX, 0);
		dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = false;
	}

	if (dpm_get_info(TYPEC_PORT_2_IDX)->port_disable) {
		amd_crb_drivers_hpi_portEnable(TYPEC_PORT_0_IDX, 1);
		dpm_set_status(TYPEC_PORT_2_IDX)->port_disable = false;
	}
}

/**
 * amd_crb_drivers_hpi_recoveryPorts
 *
 * @note
 *  enable or disable the hpi EC sink path control
 */
void amd_crb_drivers_hpi_sinkPathCtrl(bool en)
{
    uint8_t inputBuffer;
    uint8_t chipID = 0;

    if (en) {
        /* Enable port0 */
        chipID = 0;
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1032, &inputBuffer, 1, chipID);
        inputBuffer |= 0x01;
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */

        /* Enable port1 */
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2032, &inputBuffer, 1, chipID);
        inputBuffer |= 0x01;
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */

        /* Enable port2 */
        chipID = 1;
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1032, &inputBuffer, 1, chipID);
        inputBuffer |= 0x01;
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
        LOG_DBG("CYPD enable sink ctrl:P0_1_2");
    }
    else {
        /* Disable port0 */
        chipID = 0;
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1032, &inputBuffer, 1, chipID);
        inputBuffer &= ~(0x01);
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */

        /* Disable port1 */
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2032, &inputBuffer, 1, chipID);
        inputBuffer &= ~(0x01);
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_1_IDX, 20, chipID);  /* 20ms */

        /* Disable port2 */
        chipID = 1;
        amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1032, &inputBuffer, 1, chipID);
        inputBuffer &= ~(0x01);
        amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1032, &inputBuffer, 1, chipID);
        amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, TYPEC_PORT_0_IDX, 20, chipID);  /* 20ms */
        LOG_DBG("CYPD disable sink ctrl:P0_1_2");
    }
}

/**
 * amd_crb_drivers_hpi_snkCapUnconstrainedPower
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 * @param [in]     enable;     enable unconstrained bit or not
 *
 * @note
 * change sink cap unconstrained power bit
 */
void amd_crb_drivers_hpi_snkCapUnconstrainedPower(uint8_t port, uint8_t chipID, bool enable)
{
	/* Mask two sink pdo. 5V/3A and 20V/5A */
	uint8_t cmd = 0x03;

	if (enable) {
		cmd |= 0x80;
	}

	/* trigger command */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1005, &cmd, 1, chipID);
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2005, &cmd, 1, chipID);
	}
	else
		return;

	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 20, chipID);  /* 20ms */

	return;
}

/**
 * amd_crb_drivers_hpi_srcCapUnconstrainedPower
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     chip index
 * @param [in]     enable;     enable unconstrained bit or not
 *
 * @note
 * change source cap unconstrained power bit
 */
void amd_crb_drivers_hpi_srcCapUnconstrainedPower(uint8_t port, uint8_t chipID, bool enable)
{
	/* Mask two sink pdo. 5V/3A and 20V/5A */
	uint8_t cmd = 0x01;

	if (enable) {
		cmd |= 0x80;
	}

	/* trigger command */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1004, &cmd, 1, chipID);
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2004, &cmd, 1, chipID);
	}
	else
		return;

	amd_crb_drivers_hpi_wait4PortResp(HPI_RESP_SUCCESS, port, 20, chipID);  /* 20ms */
}

/**
 * amd_crb_drivers_hpi_systemStatus
 *
 * @note
 *  System status: G3/S5/S3/S0
 */
void amd_crb_drivers_hpi_systemStatus(uint8_t chipID, enum system_power_state st)
{
	uint8_t sysStatus;

	switch (st) {
		case SYSTEM_G3_STATE:
			sysStatus = 0x02;
			break;
		case SYSTEM_S0_STATE:
			sysStatus = 0x00;
			break;
		case SYSTEM_S3_STATE:
			sysStatus = 0x01;
			break;
		case SYSTEM_S5_STATE:
			sysStatus = 0x03;
			break;
		default:
			return;
	}

	/* trigger command */
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x003B, &sysStatus, 1, chipID);
	amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
}

/**
 * amd_crb_drivers_hpi_drSwapResponse
 *
 * @note
 *  change the dr swap response with accept/reject/wait
 */
void amd_crb_drivers_hpi_drSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp)
{
	uint8_t curResp = 0;

	/* read current response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2028, &curResp, 1, chipID);
	}

	curResp &= ~0x03;  /* 2 bits for status */
	curResp |= ((uint8_t)resp << DPM_DR_SWAP_RESP_POS) & DPM_DR_SWAP_RESP_MASK;

	/* update new response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2028, &curResp, 1, chipID);
	}
}

/**
 * amd_crb_drivers_hpi_prSwapResponse
 *
 * @note
 *  change the pr swap response with accept/reject/wait
 */
void amd_crb_drivers_hpi_prSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp)
{
	uint8_t curResp = 0;

	/* read current response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2028, &curResp, 1, chipID);
	}

	curResp &= ~0x03;   /* 2 bits for status */
	curResp |= ((uint8_t)resp << DPM_PR_SWAP_RESP_POS) & DPM_PR_SWAP_RESP_MASK;

	/* update new response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2028, &curResp, 1, chipID);
	}
}

/**
 * amd_crb_drivers_hpi_vconnSwapResponse
 *
 * @note
 *  change the vconn swap response with accept/reject/wait
 */
void amd_crb_drivers_hpi_vconnSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp)
{
	uint8_t curResp = 0;

	/* read current response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x2028, &curResp, 1, chipID);
	}

	curResp &= ~0x03;   /* 2 bits for status */
	curResp |= ((uint8_t)resp << DPM_VCONN_SWAP_RESP_POS) & DPM_VCONN_SWAP_RESP_MASK;

	/* update new response */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1028, &curResp, 1, chipID);
	} else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2028, &curResp, 1, chipID);
	}
}

#if (CONFIG_LOG)
void amd_crb_drivers_hpi_dumpEventMessage(uint8_t id)
{
	switch (id) {
		case HPI_RESP_NO_RESPONSE:
			LOG_DBG("  HPI_RESP_NO_RESPONSE [%02X]", id);
			break;
		case HPI_RESP_SUCCESS:
			LOG_DBG("  HPI_RESP_SUCCESS [%02X]", id);
			break;
		case HPI_RESP_FLASH_DATA_AVAILABLE:
			LOG_DBG("  HPI_RESP_FLASH_DATA_AVAILABLE [%02X]", id);
			break;
		case HPI_RESP_INVALID_COMMAND:
			LOG_DBG("  HPI_RESP_INVALID_COMMAND [%02X]", id);
			break;
		case HPI_RESP_FLASH_UPDATE_FAILED:
			LOG_DBG("  HPI_RESP_FLASH_UPDATE_FAILED [%02X]", id);
			break;
		case HPI_RESP_INVALID_FW:
			LOG_DBG("  HPI_RESP_INVALID_FW [%02X]", id);
			break;
		case HPI_RESP_INVALID_ARGUMENTS:
			LOG_DBG("  HPI_RESP_INVALID_ARGUMENTS [%02X]", id);
			break;
		case HPI_RESP_NOT_SUPPORTED:
			LOG_DBG("  HPI_RESP_NOT_SUPPORTED [%02X]", id);
			break;
		case HPI_RESP_TRANSACTION_FAILED:
			LOG_DBG("  HPI_RESP_TRANSACTION_FAILED [%02X]", id);
			break;
		case HPI_RESP_PD_COMMAND_FAILED:
			LOG_DBG("  HPI_RESP_PD_COMMAND_FAILED [%02X]", id);
			break;
		case HPI_RESP_UNDEFINED_ERROR:
			LOG_DBG("  HPI_RESP_UNDEFINED_ERROR [%02X]", id);
			break;
		case HPI_RESP_READ_PDO_DATA:
			LOG_DBG("  HPI_RESP_READ_PDO_DATA [%02X]", id);
			break;
		case HPI_RESP_CMD_ABORTED:
			LOG_DBG("  HPI_RESP_CMD_ABORTED [%02X]", id);
			break;
		case HPI_RESP_PORT_BUSY:
			LOG_DBG("  HPI_RESP_PORT_BUSY [%02X]", id);
			break;
		case HPI_RESP_MIN_MAX_CURRENT:
			LOG_DBG("  HPI_RESP_MIN_MAX_CURRENT [%02X]", id);
			break;
		case HPI_RESP_EXT_SRC_CAP:
			LOG_DBG("  HPI_RESP_EXT_SRC_CAP [%02X]", id);
			break;
		case HPI_EVENT_RESET_COMPLETE:
			LOG_DBG("  HPI_EVENT_RESET_COMPLETE [%02X]", id);
			break;
		case HPI_EVENT_MSG_QUEUE_OVERFLOW:
			LOG_DBG("  HPI_EVENT_MSG_QUEUE_OVERFLOW [%02X]", id);
			break;
		case HPI_EVENT_OVER_CURRENT_DETECTED:
			LOG_DBG("  HPI_EVENT_OVER_CURRENT_DETECTED [%02X]", id);
			break;
		case HPI_EVENT_OVER_VOLTAGE_DETECTED:
			LOG_DBG("  HPI_EVENT_OVER_VOLTAGE_DETECTED [%02X]", id);
			break;
		case HPI_EVENT_TYPE_C_PORT_CONNECT:
			LOG_DBG("  HPI_EVENT_TYPE_C_PORT_CONNECT [%02X]", id);
			break;
		case HPI_EVENT_TYPE_C_PORT_DISCONNECT:
			LOG_DBG("  HPI_EVENT_TYPE_C_PORT_DISCONNECT [%02X]", id);
			break;
		case HPI_EVENT_PD_CONTRACT_NEGOTIATION_COMPLETE:
			LOG_DBG("  HPI_EVENT_PD_CONTRACT_NEGOTIATION_COMPLETE [%02X]", id);
			break;
		case HPI_RESP_UPDATE_AC_LIMIT:
			LOG_DBG("  HPI_RESP_UPDATE_AC_LIMIT [%02X]", id);
			break;
		default:
			LOG_DBG("  Unknown HPI event/msg [%02X]", id);
			break;
	}
}
#endif

