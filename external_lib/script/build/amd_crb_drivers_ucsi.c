/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#if CONFIG_USBC_TIPD_TPS6599X
#include "amd_crb_drivers_tps6599x.h"
#endif
#if CONFIG_USBC_TIPD_TPS6699X
#include "amd_crb_drivers_tps6699x.h"
#endif
#include "amd_crb_drivers_ucsi.h"
#include "amd_crb_drivers_ucsi_internal.h"
#include "dev_utility.h"
#include "board_config.h"
#include "system.h"

LOG_MODULE_REGISTER(EcUcsi, CONFIG_EC_UCSI_LOG_LEVEL);

#if CONFIG_EC_UCSI_ENABLE
#define UCSI_DEBUG             (1)
/* 
 * Check PPM command for completion 
 */
#define amd_crb_drivers_ppmCmdReqCmplt(cmdcode)    ((cmdcode != UCSI_CMD_PPM_RESET) && (cmdcode != UCSI_CMD_SET_NOTIFICATION_ENABLE))

/* timer interval */
#define UCSI_PPM_REST_TIMEOUT_INTERVAL             (800)
#define UCSI_SET_UOM_TIMEOUT_INTERVAL               (2000)
#define UCSI_SET_POWER_LEVEL_TIMEOUT_INTERVAL       (3000)
    
/**
 * Enum to Hold status of DPM command
**/
typedef enum 
{ 
    NO_CMD,             /* 0 */
    CMD_QUEUED,         /* 1 */
    CMD_IN_PROGRESS,    /* 2 */
    CMD_SUCCESS,        /* 3 */
    CMD_ERROR           /* 4 */
    
} dpmCmdStatus_t;

/* Map PD command to desired Rp level. */
const dpm_typec_cmd_t ucsi_pdcmd_rp_map[] =
{
    DPM_CMD_SET_RP_3A,         /* PPM Defined Default */
    DPM_CMD_SET_RP_3A,         /* UCSI_PD_CMD_SET_RP_3A0 */
    DPM_CMD_SET_RP_1_5A,       /* UCSI_PD_CMD_SET_RP_1A5 */
    DPM_CMD_SET_RP_DFLT        /* UCSI_PD_CMD_SET_RP_DFLT */
};

/* PPM state enumeration. */
typedef enum {
    PPM_IDLE,                           /* PPM is waiting for a new command or notification */
    PPM_BUSY,                           /* PPM is waiting for a DPM command to complete */
    PPM_WAIT_FOR_ACK,                   /* PPM is waiting for the OPM to ACK the previous command */
    PPM_READ_PENDING,                   /* PPM is waiting for OPM to read the CCI register */
} ucsi_ppm_state_t;

/* struct to hold UCSI status */
static struct {
    struct {
        uint32_t    notifications;      /* All notifications raised so far */
        uint32_t    eventFilter;        /* Which notifications to not send to the OPM */
        bool        isCharging;         /* Whether the port is charging the system's battery */
    } ports[NO_OF_TYPEC_PORTS];
    uint32_t        eventMask;          /* Which notifications are enabled currently */
    union {                             /* Union to hold UCSI command parameters */
        struct {
            uint8_t start;              /* Start PDO index for GET_PDOS */
            uint8_t num;                /* Number of PDOs to ask in GET_PDOS */
        } get_pdos;
    } cmd_params;
    uint16_t         errorInfo;         /* UCSI error status */
    uint8_t          dpmPort;           /* The port in which to execute the command */
    dpmCmdStatus_t   dpmCmdStatus;      /* Status of the current DPM command */
    ucsi_cmd_t       activeCmd;         /* The command being processed now */
} g_ec_ucsi;

/* Global variable */
ucsi_reg_space ucsi_regs __attribute__ ((aligned (4))); /*UCSI Register space*/
bool g_ec_ucsi_readPending = false;              /* Checks for command read pending */
volatile bool g_ec_ucsi_cmdAvailable = false;    /* Checks for any command recevied from OPM */
uint16_t g_ec_ucsi_response =  UCSI_STS_IGNORE;   /* Response to a command */

/* Static variable */
static ucsi_ppm_state_t gl_ppm_state = PPM_IDLE;
/* data struct to device capability. */
static ucsi_capability_t gl_dev_capability = {
    .bmAttributes   = UCSI_TYPEC_DISABLED_SUPPORTED,
    .bNumConnectors = NO_OF_TYPEC_PORTS,
    .bmOptionalFeatures = UCSI_OPTIONAL_FEATURES,
#if UCSI_ALT_MODE_ENABLED
    .bNumAltModes = 2,
#else
		.bNumAltModes = 0,
#endif
    .reserved = 0,
#if UCSI_BATTERY_CHARGING_ENABLE
    .bcdBCVersion = UCSI_BC_1_2_VERSION,
#else
    .bcdBCVersion = 0x0000,    
#endif /* UCSI_BATTERY_CHARGING_ENABLE */
#if PD_REV3_ENABLE    
    .bcdPDVersion = UCSI_USB_PD_3_0_VERSION,
#else
    .bcdPDVersion = UCSI_USB_PD_2_0_VERSION,    
#endif /*PD_REV3_ENABLE*/    
    .bcdUSBTypeCVersion = UCSI_USB_TYPE_C_1_3_VERSION
};

const static uint16_t connector_capability_no_alt_mode = 
 (UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
  UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
  UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
  UCSI_CONN_CAP_SWAP_TO_SNK);

/* To hold connector capability*/
const static uint16_t connector_capability[NO_OF_TYPEC_PORTS] = {
#if UCSI_ALT_MODE_ENABLED
    /* Port 0 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
				 UCSI_CONN_CAP_ALT_MODE_SUPP | UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#if PD_DUALPORT_ENABLE    
    /* Port 1 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
			 	 UCSI_CONN_CAP_ALT_MODE_SUPP | UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#endif 
#if PD_TRIPPORT_ENABLE    
    /* Port 2 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
				 UCSI_CONN_CAP_ALT_MODE_SUPP | UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#endif
#else
    /* Port 0 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
				 UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#if PD_DUALPORT_ENABLE    
    /* Port 1 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
				 UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#endif 
#if PD_TRIPPORT_ENABLE    
    /* Port 2 */ UCSI_CONN_CAP_DFP_ONLY | UCSI_CONN_CAP_USB_2_0_SUPP | UCSI_CONN_CAP_USB_3_0_SUPP |\
				 UCSI_CONN_CAP_PWR_SRC_SUPP | UCSI_CONN_CAP_PWR_SNK_SUPP |\
				 UCSI_CONN_CAP_SWAP_TO_DFP | UCSI_CONN_CAP_SWAP_TO_SRC |\
				 UCSI_CONN_CAP_SWAP_TO_SNK,
#endif 
#endif
};

/* Mask used to  track the reception of the SET_CCOM command. */
static uint8_t _S_ccomCmdRcvdMask = 0;
static uint8_t _s_typecResetCmdCompletion = 0;
static uint8_t _s_resetPending = false;

static uint8_t _s_ucsiStatus;
static uint8_t _s_ucsiControl;

static struct k_timer _s_ppmResetTimerId;
static struct k_timer _s_setUomTimerId[NO_OF_TYPEC_PORTS];
static struct k_timer _s_setPowerLevelTimerId[NO_OF_TYPEC_PORTS];

extern uint8_t g_4cc_pendingCmdType[NO_OF_TYPEC_PORTS];

/* External functions */
extern bool app_ucsi_tunnel_intHandler (uint8_t chipID); /* for ucsi soft interrupt handler */

/* ucsi debug log functions */
void amd_crb_drivers_ucsi_ecRegLog(uint8_t isRead, uint8_t reg, uint8_t* pBuf, uint32_t len);
void amd_crb_drivers_ucsi_ecStringLog(char * str);
void amd_crb_drivers_ucsi_ecDataLog(uint8_t port, uint32_t data);
void amd_crb_drivers_ucsi_ecLogCtrlCmd(uint8_t command);
void amd_crb_drivers_ucsi_ecLogCtrlDataLen(uint8_t data_length);
void amd_crb_drivers_ucsi_ecLogCtrlDetails(uint8_t* details, uint8_t len);
void amd_crb_drivers_ucsi_ecLogMsgOut(uint8_t* out, uint8_t len);
void amd_crb_drivers_ucsi_ecLogMsgIn(uint8_t* in, uint8_t len);
void amd_crb_drivers_ucsi_ecLogCciConnectorNum(uint8_t num);
void amd_crb_drivers_ucsi_ecLogCciDataLen(uint8_t data_length);
void amd_crb_drivers_ucsi_ecLogCciIndicators(uint8_t indicators);

/**
 * amd_crb_drivers_ec_ucsi_acpiReadDone
 *
 * @note
 *  check if acpi read done for ucsi buffer
 */
void amd_crb_drivers_ec_ucsi_acpiReadDone(void)
{
	/* Notify the UCSI layer that the read is done */
	g_ec_ucsi_readPending = false;
}

/**
 * amd_crb_drivers_ec_ucsi_regAccess
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]      reg;     registers' offset (see TI spec.)
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/register width
 *
 * @note
 *  Read/write the UCSI defined registers
 */
void amd_crb_drivers_ec_ucsi_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len)
{
	uint8_t  regIdx = reg;

	/* also need to check the len to avoid overflow each area */
	if (isRead) {
	  /* read the msgin and cci from ucsi addr */
		if ((regIdx >= UCSI_REG_MESSAGE_IN) && 
			(regIdx < UCSI_REG_MESSAGE_OUT)) {
			if (len > (sizeof(ucsi_regs.message_in) - reg + UCSI_REG_MESSAGE_IN)) {
				len = (sizeof(ucsi_regs.message_in) - reg + UCSI_REG_MESSAGE_IN);
			}
			memcpy(pBuf, ucsi_regs.message_in + (reg - UCSI_REG_MESSAGE_IN), len);
		}
		else if (regIdx == UCSI_REG_CCI) {
			if (len > sizeof(ucsi_regs.cci))
				len = sizeof(ucsi_regs.cci);

			memcpy(pBuf, &ucsi_regs.cci, len);
			amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_CMD_IN_PROGRESS);
		}
		else if (regIdx == UCSI_REG_VERSION) {
			if (len > sizeof(ucsi_regs.version))
				len = sizeof(ucsi_regs.version);

			memcpy(pBuf, &ucsi_regs.version, len);
		}
	} 
	else {
		/* write the msgout and control to ucsi addr */
		if (regIdx >= UCSI_REG_MESSAGE_OUT) {
			if (len > (sizeof(ucsi_regs.message_out) - reg + UCSI_REG_MESSAGE_OUT))
				len = (sizeof(ucsi_regs.message_out) - reg + UCSI_REG_MESSAGE_OUT);

			memcpy(ucsi_regs.message_out + (reg - UCSI_REG_MESSAGE_OUT), pBuf, len);
			
			/*Deassert the ucsi command avaliable */
			g_ec_ucsi_cmdAvailable = false;
		}
		else if (regIdx == UCSI_REG_CONTROL) {
			if (len > sizeof(ucsi_regs.control))
				len = sizeof(ucsi_regs.control);

			memcpy(&ucsi_regs.control, pBuf, len);
			
			/* Assert the ucsi command avaliable */
			g_ec_ucsi_cmdAvailable = true;
		}
	}
#if UCSI_DEBUG
	amd_crb_drivers_ucsi_ecRegLog(isRead, reg, pBuf, len);
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_ppmRestTimeout
 *
 * @note
 *  ppm reset timeout call back
 */
void amd_crb_drivers_ec_ucsi_ppmRestTimeout(struct k_timer *timer)
{
  /* Report PPM pass when timeout */
	ucsi_regs.cci.data_length = 0;
	memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));

	_s_typecResetCmdCompletion = 0;
	k_timer_stop(&_s_ppmResetTimerId);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("PPM reset timeout and force pass");
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_setUomTimeoutP0
 *
 * @note
 *  port0 set uom command time out call back
 */
void amd_crb_drivers_ec_ucsi_setUomTimeoutP0(struct k_timer *timer)
{
	k_timer_stop(&_s_setUomTimerId[TYPEC_PORT_0_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_uom timeout p0");
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_setUomTimeoutP1
 *
 * @note
 *  port1 set uom command time out call back
 */
#if PD_DUALPORT_ENABLE
void amd_crb_drivers_ec_ucsi_setUomTimeoutP1(struct k_timer *timer)
{
	k_timer_stop(&_s_setUomTimerId[TYPEC_PORT_1_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_uom timeout p1");
#endif
}
#endif

/**
 * amd_crb_drivers_ec_ucsi_setUomTimeoutP2
 *
 * @note
 *  port2 set uom command time out call back
 */
#if PD_TRIPPORT_ENABLE
void amd_crb_drivers_ec_ucsi_setUomTimeoutP2(struct k_timer *timer)
{
	k_timer_stop(&_s_setUomTimerId[TYPEC_PORT_2_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_uom timeout p1");
#endif
}
#endif

/**
 * amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP0
 *
 * @note
 *  port0 set power level command time out call back
 */
void amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP0(struct k_timer *timer)
{
	k_timer_stop(&_s_setPowerLevelTimerId[TYPEC_PORT_0_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_power_level timeout p0");
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP1
 *
 * @note
 *  port1 set power level command time out call back
 */
#if PD_DUALPORT_ENABLE
void amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP1(struct k_timer *timer)
{
	k_timer_stop(&_s_setPowerLevelTimerId[TYPEC_PORT_1_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_power_level timeout p1");
#endif
}
#endif

/**
 * amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP2
 *
 * @note
 *  port2 set power level command time out call back
 */
#if PD_TRIPPORT_ENABLE
void amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP2(struct k_timer *timer)
{
	k_timer_stop(&_s_setPowerLevelTimerId[TYPEC_PORT_2_IDX]);
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;  /* Set success when ppm reset time out for now */
#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("set_power_level timeout p2");
#endif
}
#endif

/**
 * amd_crb_drivers_ec_ucsi_init
 *
 * @note
 *  ec ucsi driver init
 */
void amd_crb_drivers_ec_ucsi_init(void)
{
	memset((uint8_t*)&g_ec_ucsi, 0x00, sizeof(g_ec_ucsi));

	g_ec_ucsi.dpmCmdStatus = NO_CMD;
	g_ec_ucsi.eventMask = 0x00;
	amd_crb_drivers_ec_ucsi_regReset();
	g_ec_ucsi_cmdAvailable = false;

	memset(&ucsi_regs, 0x00, sizeof(ucsi_regs));
	ucsi_regs.version = UCSI_VERSION;
	g_ec_ucsi.activeCmd = UCSI_CMD_RESERVED;
	gl_ppm_state = PPM_IDLE;

	_s_typecResetCmdCompletion = 0;
	_s_resetPending = false;

	k_timer_init(&_s_ppmResetTimerId, amd_crb_drivers_ec_ucsi_ppmRestTimeout, NULL); /* Interval 500ms */

	k_timer_init(&_s_setUomTimerId[TYPEC_PORT_0_IDX], amd_crb_drivers_ec_ucsi_setUomTimeoutP0, NULL); /* Interval 2000ms */
	k_timer_init(&_s_setPowerLevelTimerId[TYPEC_PORT_0_IDX], amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP0, NULL); /* Interval 3000ms */
#if PD_DUALPORT_ENABLE
	k_timer_init(&_s_setUomTimerId[TYPEC_PORT_1_IDX], amd_crb_drivers_ec_ucsi_setUomTimeoutP0, NULL); /* Interval 2000ms */
	k_timer_init(&_s_setPowerLevelTimerId[TYPEC_PORT_1_IDX], amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP0, NULL); /* Interval 3000ms */
#endif

#if PD_TRIPPORT_ENABLE
	k_timer_init(&_s_setUomTimerId[TYPEC_PORT_2_IDX], amd_crb_drivers_ec_ucsi_setUomTimeoutP0, NULL); /* Interval 2000ms */
	k_timer_init(&_s_setPowerLevelTimerId[TYPEC_PORT_2_IDX], amd_crb_drivers_ec_ucsi_setPowerLevelTimeoutP0, NULL); /* Interval 3000ms */
#endif

#if UCSI_DEBUG	
	amd_crb_drivers_ucsi_ecStringLog("Ucsi Init");
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_notify
 *
 * @param [in]          port;    port number
 * @param [in]  notification;    ucsi notification
 *
 * @note
 * update the ucsi notification
 */
void amd_crb_drivers_ec_ucsi_notify(uint8_t port, uint32_t notification)
{
if((g_ec_ucsi.eventMask & ~(g_ec_ucsi.ports[port].eventFilter)) & notification)
	g_ec_ucsi.ports[port].notifications |= notification;
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("Notification:");
	amd_crb_drivers_ucsi_ecDataLog(port, notification);
#endif
}

/**
 * amd_crb_drivers_ec_ucsi_error
 *
 * @param [in]  errorInfo;    error information
 * @return 'ucsi_status_indicator_t' ERROR
 *
 * @note
 * update the ucsi error information
 */
static ucsi_status_indicator_t amd_crb_drivers_ec_ucsi_error(uint16_t errorInfo)
{
    g_ec_ucsi.errorInfo |= errorInfo;
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("Error:");
	amd_crb_drivers_ucsi_ecDataLog(0, errorInfo);
#endif
    return UCSI_STS_ERROR;
}

/**
 *  @brief Internal function to update the UCSI Command Status.
 *  @param status UCSI Command Status
 *  @return None
 */
#if UCSI_REV_1_x_ENABLE
void amd_crb_drivers_ec_ucsi_setStatus(uint8_t status)
{
    ucsi_regs.cci.indicators = status;
	
	if(ucsi_regs.cci.indicators == UCSI_STS_BUSY) {
		ucsi_regs.cci.data_length = 0;
		ucsi_regs.cci.connector_change = 0;
	}
	
    g_ec_ucsi_readPending = true;
	/* Trigger pass through to read the package. Simulate the hardware interrupt */
    app_ucsi_tunnel_intHandler(0); /* chipID only used for Cypress PD */
	
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("SetStatus:");
	amd_crb_drivers_ucsi_ecDataLog(0, status);
#endif
}
#elif UCSI_REV_2_x_ENABLE
void amd_crb_drivers_ec_ucsi_setStatus(uint16_t status)
{
    ucsi_regs.cci.indicators = status;

	if(ucsi_regs.cci.indicators == UCSI_STS_BUSY) {
		ucsi_regs.cci.data_length = 0;
		ucsi_regs.cci.connector_change = 0;
	}

    g_ec_ucsi_readPending = true;
	/* Trigger pass through to read the package. Simulate the hardware interrupt */
    app_ucsi_tunnel_intHandler(0); /* chipID only used for Cypress PD */

#if UCSI_DEBUG
	amd_crb_drivers_ucsi_ecStringLog("SetStatus:");
	amd_crb_drivers_ucsi_ecDataLog(0, status);
#endif
}
#endif

/**
 * get_battery_charging_status
 *
 * @param [in]  port;    port number
 * @return Value of battery charging status.
 *
 * @note
 * calculate the charging speed based on report sink PDO
 */
static uint8_t get_battery_charging_status(uint8_t port)
{
    const dpm_status_t *dpm_stat;
    uint8_t power_rate;
    dpm_stat = dpm_get_info(port);
	pd_do_t pdo;
	
	pdo.val = amd_crb_drivers_tps_readSnkPdo(port);

    /*fixed_src.max_current is in 10mA units*/
    /*fixed_src.voltage is in 50mV units*/	
    power_rate = ((pdo.fixed_src.voltage) * (pdo.fixed_src.max_current)) / 2000;
    if(power_rate >= 45)
        return UCSI_BATT_STS_NOMINAL_CHARGING;

    else if (power_rate >= 27) 
        return UCSI_BATT_STS_SLOW_CHARGING;

    else if (power_rate >= 15)
        return UCSI_BATT_STS_VERY_SLOW_CHARGING;

    else
        return UCSI_BATT_STS_NOT_CHARGING;
}

/**
 * get_conn_status
 *
 * @param [in]              port;    port number
 * @param [in]  connector_status;    ucsi connector status
 * @return _s_ucsiStatus_indicator_t return CCI status indicator
 *
 * @note
 * Function to get connector status of the connector
 */
static ucsi_status_indicator_t get_conn_status(uint8_t port, uint8_t * connector_status)
{
    const dpm_status_t *dpm_stat;
    volatile ucsi_connector_status_t *conn_status = (ucsi_connector_status_t *)connector_status;
    
    dpm_stat = dpm_get_info (port);
    
    memset(connector_status, 0x00, sizeof(ucsi_connector_status_t));  
    
    /* Push notifications to the OPM and clear it locally */
    conn_status->connector_status_change = g_ec_ucsi.ports[port].notifications;
    g_ec_ucsi.ports[port].notifications = 0;
    
    if (dpm_stat->attach == true) {
        conn_status->connect_status = 1;
        if(dpm_stat->contract_exist) {
            conn_status->power_op_mode = UCSI_POM_PD;
            if (dpm_stat->cur_port_role == PRT_ROLE_SINK) {
                conn_status->battery_charging_status = UCSI_BATT_STS_NOMINAL_CHARGING;
                conn_status->rdo = dpm_stat->snk_rdo.val;
            }
            else {
                conn_status->rdo = dpm_stat->src_rdo.val;
            }
        }
        else {
            if(dpm_stat->src_cur_level == UCSI_SRC_CUR_LEVEL_DEF) {
                conn_status->power_op_mode = UCSI_POM_CUR_LEVEL_DEF;
            }
            else if(dpm_stat->src_cur_level == UCSI_SRC_CUR_LEVEL_1_5A) {
                conn_status->power_op_mode = UCSI_POM_CUR_LEVEL_1_5A;
            }
            else if(dpm_stat->src_cur_level == UCSI_SRC_CUR_LEVEL_3A) {
                conn_status->power_op_mode = UCSI_POM_CUR_LEVEL_3A;
            }            
            if(dpm_stat->cur_port_role == PRT_ROLE_SINK)
                conn_status->battery_charging_status = get_battery_charging_status(port);                 
        }  
        
        if(dpm_stat->cur_port_role == PRT_ROLE_SOURCE)
            conn_status->power_direction = 1;
        
        /* Get partner type. */
        if (dpm_stat->attached_dev == DEV_DBG_ACC) {
            /* Debug acc */
            conn_status->connector_partner_type = UCSI_CONN_PARTNER_DBG_ACC;
        }
        else if (dpm_stat->attached_dev == DEV_AUD_ACC) {
            /* Audio acc */
            conn_status->connector_partner_type = UCSI_CONN_PARTNER_AUD_ACC;
        }
        else {
            /* DFP or UFP */
            if(dpm_stat->cur_port_type == PRT_TYPE_UFP)
                conn_status->connector_partner_type = UCSI_CONN_PARTNER_DFP;
            else
                conn_status->connector_partner_type = UCSI_CONN_PARTNER_UFP;
        }        
        
        conn_status->connector_partner_flags = dpm_stat->alt_mode_entered ? 
        									   UCSI_CONN_PARTNER_FLAG_ALT_MODE : UCSI_CONN_PARTNER_FLAG_USB;
    }
    amd_crb_drivers_ec_ucsi_clearStatusBit (UCSI_EVENT_PENDING);
    return UCSI_STS_CMD_COMPLETED;    
}

/**
 * amd_crb_drivers_ec_ucsi_pdEventHandler
 *
 * @param [in]              port;    port number
 * @param [in]               evt;    Event that is being notified.
 * @param [in]    alt_mode_event;    alt mode event
 *
 * @note
 * Internal function used to receive PD events from the stack and to update the UCSI registers.
 */
void amd_crb_drivers_ec_ucsi_pdEventHandler(uint8_t port, app_evt_t evt, alt_mode_app_evt_t alt_mode_event)
{
    dpm_status_t        *dpm_stat = (dpm_status_t *)dpm_get_info (port);
    static bool         prev_charging_status = false;
    bool                system_isCharging = false;
    static bool         pr_swap_completed   = false;
	
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("pd_event_hd:");
	amd_crb_drivers_ucsi_ecDataLog(port, evt);
	amd_crb_drivers_ucsi_ecDataLog(port, alt_mode_event);
#endif
    
    switch (evt) {
        case APP_EVT_PD_CONTRACT_NEGOTIATION_COMPLETE:
        	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_PD_CONTRACT_CHANGED);
        	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_CAP_CHANGED);
    
            if(dpm_stat->cur_port_role == PRT_ROLE_SINK) {
                g_ec_ucsi.ports[port].isCharging = true;
                amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_BATT_CHARGING_STATUS_CHANGED);
            }
            /* PR SWAP notification should be sent only after the PS_RDY for contract completion happens. */           
            if (pr_swap_completed == true)
            	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_POWER_DIRECTION_CHANGED);
            pr_swap_completed = false;              
            break;
            
        case APP_EVT_CONNECT:
            pr_swap_completed = false;

            amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_CONNECT_CHANGED | UCSI_EVT_PARTNER_STATUS_CHANGED);
            amd_crb_drivers_ec_ucsi_setStatusBit(UCSI_EVENT_PENDING);
            break;
            
        case APP_EVT_DISCONNECT:
            g_ec_ucsi.ports[port].isCharging = false;
        
            pr_swap_completed = false;
				
            /* Swap caused by SET PDR and UOR should be reset after a disconnect */
            /* Resst the swap */
            dpm_set_status(port)->swap_response = 0;
            /* Reset DR SWAP to accept. */
            dpm_set_status(port)->swap_response &= ~UCSI_DPM_DR_SWAP_RESP_MASK;
            dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_DR_SWAP_RESP_POS;
            /* Reset PR SWAP to accept. */
            dpm_set_status(port)->swap_response &= ~UCSI_DPM_PR_SWAP_RESP_MASK;
            dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_PR_SWAP_RESP_POS;
             
            amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_CONNECT_CHANGED | UCSI_EVT_PARTNER_STATUS_CHANGED);
            amd_crb_drivers_ec_ucsi_setStatusBit(UCSI_EVENT_PENDING);
				
            amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_ACCEPT);
            amd_crb_drivers_tps_portCtrlDrSwapResp(port, APP_RESP_ACCEPT);
				
            if (_S_ccomCmdRcvdMask & (0x10 << port)) {
            	_S_ccomCmdRcvdMask &= ~(0x10 << port);
            	amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
#if UCSI_DEBUG			
            	amd_crb_drivers_ucsi_ecStringLog("Disconn->> set DRP");
#endif
            }
				
            break;
            
        case APP_EVT_HARD_RESET_COMPLETE:
            pr_swap_completed = false;
            amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_HARD_RESET_COMPLETE);
            break;

        case APP_EVT_ALT_MODE:
#if UCSI_ALT_MODE_ENABLED
            switch(alt_mode_event) {
                case AM_EVT_ALT_MODE_SUPP:
                case AM_EVT_SUPP_ALT_MODE_CHNG:
                	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_SUPPORTED_CAM_CHANGED);
#if PD_DUALPORT_ENABLE
#if UCSI_RS5_HLK_FIX    
                    app_ec_ucsi_notify(TYPEC_PORT_1_IDX, UCSI_EVT_SUPPORTED_CAM_CHANGED);
#endif /*UCSI_RS5_HLK_FIX*/                    
#endif /*PD_DUALPORT_ENABLE*/  
#if PD_TRIPPORT_ENABLE
#if UCSI_RS5_HLK_FIX    
                    app_ec_ucsi_notify(TYPEC_PORT_2_IDX, UCSI_EVT_SUPPORTED_CAM_CHANGED);
#endif /*UCSI_RS5_HLK_FIX*/                    
#endif /*PD_TRIPPORT_ENABLE*/  								
                    break;
                case AM_EVT_ALT_MODE_ENTERED:
                case AM_EVT_ALT_MODE_EXITED:
                	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_PARTNER_STATUS_CHANGED);
                    break;
                default:
                    break;
            }
#endif
            break;
                
        case APP_EVT_PR_SWAP_COMPLETE:
						/* If need to check swap accept or not */
            pr_swap_completed = true;
            amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_POWER_DIRECTION_CHANGED);
            break;

        case APP_EVT_DR_SWAP_COMPLETE:
            /* If need to check swap accept or not */
        	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_PARTNER_STATUS_CHANGED);
            break;
            
        default:
            break;
    }  
    
    /* If either port is charging or both stop charging, notify the OPM */
#if PD_TRIPPORT_ENABLE
		system_isCharging = g_ec_ucsi.ports[TYPEC_PORT_0_IDX].isCharging ||
												 g_ec_ucsi.ports[TYPEC_PORT_1_IDX].isCharging ||
												 g_ec_ucsi.ports[TYPEC_PORT_2_IDX].isCharging;
#elif PD_DUALPORT_ENABLE
    system_isCharging = g_ec_ucsi.ports[TYPEC_PORT_0_IDX].isCharging ||
												 g_ec_ucsi.ports[TYPEC_PORT_1_IDX].isCharging;
#else
    system_isCharging = g_ec_ucsi.ports[TYPEC_PORT_0_IDX].isCharging;
#endif
    if(system_isCharging != prev_charging_status)
    	amd_crb_drivers_ec_ucsi_notify(port, UCSI_EVT_EXT_SUPPLY_CHANGED);
    prev_charging_status = system_isCharging;
}

/**
 * amd_crb_drivers_ec_ucsi_handleAltModes
 *
 * @param [in]              port;    port number
 * @param [in]           pkt_ptr;    Struct to hold PD packet bit-0 resp_status bit-1 length and bit2~ data
 * @param [in]            status;    response status true-> success and false-> fail
 *
 * @note
 * Function to handle UCSI Get Alternate Mode Command.
 */
static void amd_crb_drivers_ec_ucsi_handleAltModes(uint8_t port, uint8_t* pkt_ptr, bool status)
{
    uint8_t imsg_count = 0;

	/* First check the ti pd controller response */
	if (status)	{
		for (imsg_count = 0; imsg_count < pkt_ptr[0]; imsg_count++) {
			/* Copy the svid and mod info into messagein buffer */
			ucsi_regs.message_in[imsg_count] = pkt_ptr[1 + imsg_count];
			if (imsg_count >= (UCSI_MAX_DATA_LENGTH - 1))
				break;
		}

		ucsi_regs.cci.data_length = imsg_count;
		g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
	}
	/* "Rejected" by pd controller maybe because wrong offset, too long DataIn or others */
	else {
		/* Data response error and there will no data */
		ucsi_regs.cci.data_length = 0;
		g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
	}
}

/**
 * amd_crb_drivers_ec_ucsi_cmdCb
 *
 * @param [in]        port;    port number
 * @param [in]       pData;     ucsi command send to TI PD controller previously
 *
 * @note
 * Handler for ucsi response reported from the controller.
 */
void amd_crb_drivers_ec_ucsi_cmdCb(uint8_t port, uint8_t* pData)  /*byte-0 resp code, byte1~ data*/
{
	uint8_t pdo_requested = 0;
	ucsi_ctrl_details_t * ctrl_reg = &(ucsi_regs.control.details);

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		ucsi_regs.cci.data_length = 0;
		memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));
		g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
		return;
	}

	ucsi_regs.cci.data_length = 0;
	memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));

#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("ucsi_cb:");
	amd_crb_drivers_ucsi_ecDataLog(port, g_ec_ucsi.activeCmd);
#endif

	/* Check resposne successfully already */
	switch (g_ec_ucsi.activeCmd) {
		case UCSI_CMD_PPM_RESET:
			/* PPM_RESET complete */
			if ((*pData & 0x7F) == 0) {
				if (port == TYPEC_PORT_0_IDX) {
					/* Reset local notification list */
					g_ec_ucsi.ports[TYPEC_PORT_1_IDX].notifications = 0;
#if PD_DUALPORT_ENABLE							
					/* Send the PPM reset command to TI pd controller */
					g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
					amd_crb_drivers_tps_cmdUcsi(0, TYPEC_PORT_1_IDX, UCSI_CMD_PPM_RESET, 0,  (&ucsi_regs.control.details));
					g_ec_ucsi.dpmPort = TYPEC_PORT_1_IDX;
					g_ec_ucsi.activeCmd = UCSI_CMD_PPM_RESET;
					g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
#else
					/* Success */
					_s_typecResetCmdCompletion = 0; /* clear the flag */
					md_timer_disable(_s_ppmResetTimerId);
					g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
#endif							
				}
#if PD_DUALPORT_ENABLE
				else if (port == TYPEC_PORT_1_IDX) {
					/* Success */
					if (_s_typecResetCmdCompletion > 1) {
						_s_typecResetCmdCompletion--;
					}
					else {
						_s_typecResetCmdCompletion = 0; /* clear the flag */
						k_timer_stop(&_s_ppmResetTimerId);
						g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
					}
				}
#endif
#if PD_TRIPPORT_ENABLE
				else if (port == TYPEC_PORT_2_IDX) {
					/* Success */
					if (_s_typecResetCmdCompletion > 1) {
						_s_typecResetCmdCompletion--;
					}
					else {
						_s_typecResetCmdCompletion = 0; /* clear the flag */
						k_timer_stop(&_s_ppmResetTimerId);
						g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
					}
				}
#endif

				/* Resst the swap */
				dpm_set_status(port)->swap_response = 0;
				/* Reset DR SWAP to accept. */
				dpm_set_status(port)->swap_response &= ~UCSI_DPM_DR_SWAP_RESP_MASK;
				dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_DR_SWAP_RESP_POS;
				/* Reset PR SWAP to accept. */
				dpm_set_status(port)->swap_response &= ~UCSI_DPM_PR_SWAP_RESP_MASK;
				dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_PR_SWAP_RESP_POS;

				amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_ACCEPT);
				amd_crb_drivers_tps_portCtrlDrSwapResp(port, APP_RESP_ACCEPT);

				amd_crb_drivers_tps_devSrcCapDeft(port);
				amd_crb_drivers_tps_devSnkCapDeft(port);

				amd_crb_drivers_tps_portCtrlTypecCurrent(port, DPM_CMD_SET_RP_3A);
				amd_crb_drivers_tps_portConfigPdDisable(port, 0);
			}
			else {
				/* Error */
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

	case UCSI_CMD_SET_CCOM:
			if((*pData & 0x7F) == 0) {
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				/* Error */
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_CONNECTOR_RESET:
			/* CONNECTOR_RESET complete */
			if ((*pData & 0x7F) == 0) {
				/* Success */
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;

				/* Resst the swap */
				dpm_set_status(port)->swap_response = 0;
				/* Reset DR SWAP to accept. */
				dpm_set_status(port)->swap_response &= ~UCSI_DPM_DR_SWAP_RESP_MASK;
				dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_DR_SWAP_RESP_POS;
				/* Reset PR SWAP to accept. */
				dpm_set_status(port)->swap_response &= ~UCSI_DPM_PR_SWAP_RESP_MASK;
				dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_PR_SWAP_RESP_POS;
				
				amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_ACCEPT);
				amd_crb_drivers_tps_portCtrlDrSwapResp(port, APP_RESP_ACCEPT);
				
				amd_crb_drivers_tps_devSrcCapDeft(port);
				amd_crb_drivers_tps_devSnkCapDeft(port);
				
				amd_crb_drivers_tps_portCtrlTypecCurrent(port, DPM_CMD_SET_RP_3A);
				amd_crb_drivers_tps_portConfigPdDisable(port, 0);
			}
			else {
				/* Error */
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_GET_CAPABILITY:
			/* CONNECTOR_RESET complete */
			if ((*pData & 0x7F) == 0) {
				/* Success */
				ucsi_regs.cci.data_length = UCSI_GET_CAPABILITY_STATUS_DATA_LENGTH;
				memcpy(ucsi_regs.message_in, &pData[1], UCSI_GET_CAPABILITY_STATUS_DATA_LENGTH);
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				/* Error */
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

	case UCSI_CMD_GET_CONNECTOR_CAPABILITY:
			break;

	case UCSI_CMD_SET_UOR:
	case UCSI_CMD_SET_PDR:
			if((*pData & 0x7F) == 0) {
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				g_ec_ucsi.errorInfo |= UCSI_ERR_PORT_PARTNER_REJECT_SWAP;
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_GET_CONNECTOR_STATUS:

			if((*pData & 0x7F) == 0) {
				if((dpm_get_info(port)->contract_exist) &&
					 (dpm_get_info(port)->cur_port_role == PRT_ROLE_SINK)) {
					/* Override the charging status */
					pData[9] &= ~(0x03);
					pData[9] |= (0x03 & get_battery_charging_status(port));
				}
				else {
					pData[9] &= ~(0x03);
				}
				
#if UCSI_DEBUG			
				amd_crb_drivers_ucsi_ecStringLog("override:");
				amd_crb_drivers_ucsi_ecDataLog(0, pData[9]);
#endif

				ucsi_regs.cci.data_length = UCSI_GET_CONNECTOR_STATUS_DATA_LENGTH;
				memcpy(ucsi_regs.message_in, &pData[1], UCSI_GET_CONNECTOR_STATUS_DATA_LENGTH);
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_GET_ALTERNATE_MODES:
			if((*pData & 0x7F) == 0) {
				amd_crb_drivers_ec_ucsi_handleAltModes(port, &pData[1], true); /* First bit is alt mode length */
			}
			else {
				/* return empty packet for error case */
				//ucsi_regs.message_in[0] = 0x01;
				//ucsi_regs.message_in[1] = 0xFF;
				//ucsi_regs.message_in[2] = 0x46;
				//ucsi_regs.message_in[3] = 0x1C;
				/* support two atl mode: TBT and DP */
				ucsi_regs.message_in[0] = 0x87;
				ucsi_regs.message_in[1] = 0x80;
				ucsi_regs.message_in[2] = 0x01;
				ucsi_regs.message_in[3] = 0x00;
				ucsi_regs.message_in[4] = 0x01;
				ucsi_regs.message_in[5] = 0xFF;
				ucsi_regs.message_in[6] = 0x46;
				ucsi_regs.message_in[7] = 0x1C;

				ucsi_regs.cci.data_length = 0x06;
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			break;

		case UCSI_CMD_GET_CAM_SUPPORTED:
			if((*pData & 0x7F) == 0) {
				/* Only copy one byte for now. Need to check with TI why there is no length */
				/* Normally the length should be ((Number of Alternate Modes + 7)/8) */
				ucsi_regs.cci.data_length = (get_alt_mode_number(port) >> 3) + 1;;
				memcpy(ucsi_regs.message_in, &pData[1], ucsi_regs.cci.data_length);
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_GET_PDOS:
			if((*pData & 0x7F) == 0) {
				pdo_requested  = ctrl_reg->get_pdos.num_pdos + 1;
				ucsi_regs.cci.data_length = 0;
				
#if UCSI_READ_GET_PDO_REGISTER
				/* Read the current partner pdos */
				if(ctrl_reg->get_pdos.partner_pdo) {
					if(ctrl_reg->get_pdos.is_source_pdo) {
						amd_crb_drivers_tps_partnerSrcCapInfo(port);
						/* Check the response data length */
						if (dpm_get_info (port)->partner_src_pdo_count > ctrl_reg->get_pdos.pdo_offset) {
							ucsi_regs.cci.data_length = UCSI_BOUND(sizeof(pd_do_t) * pdo_requested,
							sizeof(pd_do_t) * (dpm_get_info (port)->partner_src_pdo_count - ctrl_reg->get_pdos.pdo_offset));
						}

						memcpy(ucsi_regs.message_in,
					   (uint8_t *)&(dpm_get_info(port)->partner_src_pdo[ctrl_reg->get_pdos.pdo_offset]),
					   ucsi_regs.cci.data_length);
					}
					else {
						amd_crb_drivers_tps_partnerSnkCapInfo(port);
						/* Check the response data length */
						if (dpm_get_info (port)->partner_snk_pdo_count > ctrl_reg->get_pdos.pdo_offset) {
							ucsi_regs.cci.data_length = UCSI_BOUND(sizeof(pd_do_t) * pdo_requested,
							sizeof(pd_do_t) * (dpm_get_info (port)->partner_snk_pdo_count - ctrl_reg->get_pdos.pdo_offset));
						}

						memcpy(ucsi_regs.message_in,
					   (uint8_t *)&(dpm_get_info(port)->partner_snk_pdo[ctrl_reg->get_pdos.pdo_offset]),
					   ucsi_regs.cci.data_length);
					}
				}
				else {
						
					memcpy(ucsi_regs.message_in, (uint8_t *)&pData[1], ucsi_regs.cci.data_length);
				}
#else						
				memcpy(ucsi_regs.message_in, (uint8_t *)&pData[1], ucsi_regs.cci.data_length);
#endif
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				ucsi_regs.cci.data_length = 0;
				memcpy(ucsi_regs.message_in, (uint8_t *)&pData[1], ucsi_regs.cci.data_length);
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			break;

		case UCSI_CMD_SET_POWER_LEVEL:
			if((*pData & 0x7F) == 0) {
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		case UCSI_CMD_SET_SINK_PATH:
			if((*pData & 0x7F) == 0) {
				g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
			}
			else {
				g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
			}
			break;

		default:
			break;
	}
}

/**
 * amd_crb_drivers_ec_ucsi_altModeCmdCb
 *
 * @param [in]        port;    port number
 * @param [in]       pData;     ucsi command send to TI PD controller previously
 *
 * @note
 * Handler for ucsi response reported from the controller for alt mode command.
 */
void amd_crb_drivers_ec_ucsi_altModeCmdCb(uint8_t port, uint8_t* pData)  /*byte-0 resp code, byte1~ data*/
{
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("alt_mode_cb:");
#endif
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		g_ec_ucsi.dpmCmdStatus = CMD_ERROR;
		return;
	}
	/* Success */
	ucsi_regs.cci.data_length = 0;
	memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));
	g_ec_ucsi.dpmCmdStatus = CMD_SUCCESS;
}

/**
 * amd_crb_drivers_ec_ucsi_handleAckCmds
 * @return _s_ucsiStatus_indicator_t return CCI status indicator
 *
 * @note
 * Function to handle UCSI Acknowledge Command
 */
static ucsi_status_indicator_t amd_crb_drivers_ec_ucsi_handleAckCmds (void)
{
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("ack_hd:");
	amd_crb_drivers_ucsi_ecDataLog(0, g_ec_ucsi.activeCmd);
#endif
	
    switch(ucsi_regs.control.command) {
        case UCSI_CMD_CANCEL:
            g_ec_ucsi.activeCmd = UCSI_CMD_CANCEL;
            gl_ppm_state =PPM_IDLE;
            /**
            Once the control is transferred to PD/Type-C stack there is 
            no way to cancel it. So the current behavior of cancel command is 
            either it is completed before execution of cancel command or it is dropped.
            TBD : Need to address this in new event based architecture.
            Note : RS5 HLK doesn't implement test for this commands
            */
            return UCSI_STS_CMD_COMPLETED;

        case UCSI_CMD_ACK_CC_CI:
            g_ec_ucsi.activeCmd = UCSI_CMD_RESERVED;
            if (ucsi_regs.control.details.ack_cc_ci.connector_change_ack) {
            	ucsi_regs.cci.connector_change = 0;
            }
            return UCSI_STS_ACK_CMD;
    }
    return UCSI_STS_IGNORE;
}

/**
 * amd_crb_drivers_ec_ucsi_cmdPpmRstHandler
 *
 * @note
 * handle with the PPM reset command
 */
static void amd_crb_drivers_ec_ucsi_cmdPpmRstHandler (void)
{
    uint8_t port = 0;
	
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("PPMreset_hd:");
#endif
	
	amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_EVENT_PENDING);
    memset((uint8_t*)&g_ec_ucsi, 0x00, sizeof(g_ec_ucsi));
    memset(&ucsi_regs, 0x00, sizeof(ucsi_regs));
    ucsi_regs.version = UCSI_VERSION;
    amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_CMD_IN_PROGRESS);
	
	_s_typecResetCmdCompletion = 0;

	for(port = 0; port < NO_OF_TYPEC_PORTS; port++) {
		if (_S_ccomCmdRcvdMask & (1 << port)) {
			_S_ccomCmdRcvdMask &= ~(1 << port);

			/* If there is no connect in this port after PPM reset then reset the port role */
			if (dpm_get_info(port)->connect == false) {
				amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
				/* delay 80ms */
				k_busy_wait(80 * 1000);

#if UCSI_DEBUG			
				amd_crb_drivers_ucsi_ecStringLog("PPM_reset->> set DRP");
#endif
			}
			/* Otherwise set the pending flag and will do the port role reset after disconnect */
			else
			{
#if UCSI_DEBUG			
				amd_crb_drivers_ucsi_ecStringLog("PPM_reset->> set DRP pending");
#endif
				_S_ccomCmdRcvdMask |= (0x10 << port);
			}
		}
	}
	
	/* Enable the ppm reset time out */
	k_timer_start(&_s_ppmResetTimerId, K_MSEC(UCSI_PPM_REST_TIMEOUT_INTERVAL), K_NO_WAIT);
	
	port = TYPEC_PORT_0_IDX; /* reset the port0 first */
	g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;

	/* Reset local notification list */
	g_ec_ucsi.ports[TYPEC_PORT_0_IDX].notifications = 0;
	/* Send the PPM reset command to TI pd controller 1 */
	amd_crb_drivers_tps_cmdUcsi(0, TYPEC_PORT_0_IDX, UCSI_CMD_PPM_RESET, 0,  (&ucsi_regs.control.details));
	/* set the flag for ucsi command */
	_s_typecResetCmdCompletion = 1;
#if PD_TRIPPORT_ENABLE
	/* Reset local notification list */
	g_ec_ucsi.ports[TYPEC_PORT_2_IDX].notifications = 0;
	/* Send the PPM reset command to TI pd controller 2 */
	amd_crb_drivers_tps_cmdUcsi(0, TYPEC_PORT_2_IDX, UCSI_CMD_PPM_RESET, 0,  (&ucsi_regs.control.details));
	/* set the flag for ucsi command */
	_s_typecResetCmdCompletion = 2;
#endif
	g_ec_ucsi.dpmPort = port;
	g_ec_ucsi.activeCmd = UCSI_CMD_PPM_RESET;
	g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;

	_s_resetPending = true;
    g_ec_ucsi_readPending = false;
}

/**
 * amd_crb_drivers_ec_ucsi_handleRst
 * @return _s_ucsiStatus_indicator_t return CCI status indicator
 *
 * @note
 * Function to handle UCSI Reset Commands
 */
static ucsi_status_indicator_t amd_crb_drivers_ec_ucsi_handleRst (void)
{
	ucsi_status_indicator_t ucsi_cmd_status = UCSI_STS_IGNORE;
    ucsi_ctrl_details_t * ctrl_reg = &(ucsi_regs.control.details);

    g_ec_ucsi.activeCmd = (ucsi_cmd_t)ucsi_regs.control.command;
	
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("reset_hd:");
	amd_crb_drivers_ucsi_ecDataLog(0, g_ec_ucsi.activeCmd);
#endif

    switch(ucsi_regs.control.command) {
        case UCSI_CMD_CONNECTOR_RESET: {
			const dpm_status_t *dpm_stat;
			uint8_t port = ctrl_reg->get_connector_capability.connector_number - 1;
			if (port >= NO_OF_TYPEC_PORTS) {
				ucsi_cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
				break;
			}

			dpm_stat = dpm_get_info (port);
			if (!(dpm_stat->contract_exist)) {
				/*TODO : UCSI - HLK Bug Failed if error bit is set.
				ucsi_cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
				*/
				ucsi_cmd_status = UCSI_STS_CMD_COMPLETED;
			}
			else {
				if (_S_ccomCmdRcvdMask & (1 << port)) {
					_S_ccomCmdRcvdMask &= ~(1 << port);

					/* If there is no connect in this port after PPM reset then reset the port role */
					if (dpm_get_info(port)->connect == false) {
						amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
						/* delay 80ms */
						k_busy_wait(80 * 1000);
#if UCSI_DEBUG			
						amd_crb_drivers_ucsi_ecStringLog("CONN_reset->> set DRP");
#endif
					}
					/* Otherwise set the pending flag and will do the port role reset after disconnect */
					else {
#if UCSI_DEBUG			
						amd_crb_drivers_ucsi_ecStringLog("CONN_reset->> set DRP pending");
#endif
						_S_ccomCmdRcvdMask |= (0x10 << port);
					}
				}

				/* Send the connector reset command to pd controller */
				g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
				amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
				g_ec_ucsi.dpmPort = port;
				g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
				/* Connector reset need to set the busy and inform pd controller */
				ucsi_cmd_status = UCSI_STS_BUSY;//UCSI_STS_CMD_COMPLETED;
			}
			break;
		}
    }
    return ucsi_cmd_status;
}

/**
 * amd_crb_drivers_ec_ucsi_handlePpmCmds
 * @return _s_ucsiStatus_indicator_t return CCI status indicator
 *
 * @note
 * Function to handle UCSI PPM Commands
 */
static ucsi_status_indicator_t amd_crb_drivers_ec_ucsi_handlePpmCmds (void)
{
    ucsi_ctrl_details_t * ctrl_reg = &(ucsi_regs.control.details);
    ucsi_status_indicator_t ucsi_cmd_status = UCSI_STS_IGNORE;

    g_ec_ucsi.activeCmd = (ucsi_cmd_t)ucsi_regs.control.command;
	
#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("ppm_cmd_hd:");
	amd_crb_drivers_ucsi_ecDataLog(0, g_ec_ucsi.activeCmd);
#endif

    switch(ucsi_regs.control.command) {
        case UCSI_CMD_SET_NOTIFICATION_ENABLE: {
#if UCSI_HP_DRIVER            
                prev_eventMask = ucsi.eventMask;
#endif /* UCSI_HP_DRIVER */    
                g_ec_ucsi.eventMask = ctrl_reg->set_notification_enable.notifications;

                /* If the event mask was updated, clear any pending notifications
                 * associated with the event */
                g_ec_ucsi.ports[TYPEC_PORT_0_IDX].notifications &= g_ec_ucsi.eventMask;
#if PD_DUALPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_1_IDX].notifications &= g_ec_ucsi.eventMask;
#endif
#if PD_TRIPPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_2_IDX].notifications &= g_ec_ucsi.eventMask;
#endif
                /* If the "Command Completed" notification was cleared, drop the response */
                if(g_ec_ucsi.eventMask & UCSI_EVT_CMD_COMPLETED) {
                    ucsi_cmd_status = UCSI_STS_CMD_COMPLETED;
                }
                else {
                    g_ec_ucsi.activeCmd = UCSI_CMD_RESERVED;
                    ucsi_regs.cci.connector_change = 0;
                    amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_EVENT_PENDING);
                }
                break;
            }
        
        case UCSI_CMD_GET_CAPABILITY: {
#if 1
                uint8_t temp1 = 0;
#if PD_DUALPORT_ENABLE
                uint8_t temp2 = 0;
#endif /* PD_DUALPORT_ENABLE */
#if PD_TRIPPORT_ENABLE
                uint8_t temp3 = 0;
#endif /* PD_TRIPPORT_ENABLE */
                ucsi_regs.cci.data_length = UCSI_GET_CAPABILITY_STATUS_DATA_LENGTH;
				/* Need to get the alt mode number */
                temp1 = get_alt_mode_number(TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
                temp2 = get_alt_mode_number(TYPEC_PORT_1_IDX);
                if (temp2 > temp1)
                    temp1 = temp2;
#endif /* PD_DUALPORT_ENABLE */
#if PD_TRIPPORT_ENABLE
                temp3 = get_alt_mode_number(TYPEC_PORT_2_IDX);
                if (temp3 > temp1)
                    temp1 = temp3;
#endif /* PD_TRIPPORT_ENABLE */
#if UCSI_ALT_MODE_ENABLED
                gl_dev_capability.bNumAltModes = temp1;
#else
                gl_dev_capability.bNumAltModes = 0;
#endif
                gl_dev_capability.bNumConnectors = NO_OF_TYPEC_PORTS;

                if ((dpm_get_info (TYPEC_PORT_0_IDX)->cur_port_type != PRT_TYPE_DFP)
#if PD_DUALPORT_ENABLE 
                 || (dpm_get_info (TYPEC_PORT_1_IDX)->cur_port_type != PRT_TYPE_DFP)
#endif /* PD_DUALPORT_ENABLE */
#if PD_TRIPPORT_ENABLE
                 || (dpm_get_info (TYPEC_PORT_2_IDX)->cur_port_type != PRT_TYPE_DFP)
#endif
                ) {
                    gl_dev_capability.bmAttributes |= UCSI_PD_SUPPLY_SUPPORTED;
                }                

#if APP_AC_BARREL_CONNECTOR_SUPPPRTED
                gl_dev_capability.bmAttributes |= AC_SUPPLY_SUPPORTED;
#endif
                memcpy(ucsi_regs.message_in, &gl_dev_capability, sizeof(ucsi_capability_t));
                ucsi_cmd_status = UCSI_STS_CMD_COMPLETED;
#else
                ucsi_regs.cci.data_length = GET_CAPABILITY_STATUS_DATA_LENGTH;
                /* Only read the port 0 capibility because capability should not point to ports */
                g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                dev_tps_cmd_ucsi(0, TYPEC_PORT_0_IDX, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                g_ec_ucsi.dpmPort = TYPEC_PORT_0_IDX;
                g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                ucsi_cmd_status = UCSI_STS_BUSY;
#endif
                break;
            }
         
        case UCSI_CMD_GET_CONNECTOR_CAPABILITY:
            {
                uint8_t port = ctrl_reg->get_connector_capability.connector_number - 1;
                if (port >= NO_OF_TYPEC_PORTS) {
                    ucsi_cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_STS_CMD_COMPLETED | UCSI_ERR_UNKNOWN_CONNECTOR);
                }
                else {
                    ucsi_regs.cci.data_length = UCSI_GET_CONNECTOR_CAPABILITY_DATA_LENGTH;
                    if (dpm_get_info(port)->alt_mode_enable)
	                    /* Regular connector cap if alt mode enable in this port */
	                    memcpy(ucsi_regs.message_in, &connector_capability[port], 2);
                    else
	                    /* remove the alt mode supp from the conenctor cap if not support alt mode */
	                    memcpy(ucsi_regs.message_in, &connector_capability_no_alt_mode, 2);
                    ucsi_cmd_status = UCSI_STS_CMD_COMPLETED;
                }

                break;
            }

        case UCSI_CMD_GET_ERROR_STATUS: {
                memcpy(ucsi_regs.message_in, &g_ec_ucsi.errorInfo, 2);
                g_ec_ucsi.errorInfo = 0x00;
                ucsi_regs.cci.data_length = UCSI_GET_ERROR_STATUS_DATA_LENGTH;
                ucsi_cmd_status = UCSI_STS_CMD_COMPLETED;

                break;
			}
    }

    return ucsi_cmd_status;
}

/**
 * amd_crb_drivers_ec_ucsi_handlePortCmd
 * @return _s_ucsiStatus_indicator_t return CCI status indicator
 *
 * @note
 * Function to handle UCSI Port Commands
 */
static ucsi_status_indicator_t amd_crb_drivers_ec_ucsi_handlePortCmd (void)
{
	ucsi_status_indicator_t cmd_status = UCSI_STS_CMD_COMPLETED;
    
    ucsi_ctrl_details_t * ctrl_reg = &(ucsi_regs.control.details);
    uint8_t port = 0;
  
    const dpm_status_t *dpm_stat; 
	
#if UCSI_ALT_MODE_ENABLED
    uint8_t alt_mode_idx;
#endif /*UCSI_ALT_MODE_ENABLED*/  

	port_role_t port_role_map[5] = {PRT_NULL, PRT_ROLE_SOURCE, PRT_ROLE_SINK, PRT_NULL, PRT_DUAL };
    
    g_ec_ucsi.activeCmd = (ucsi_cmd_t)ucsi_regs.control.command;

#if UCSI_DEBUG			
	amd_crb_drivers_ucsi_ecStringLog("port_cmd_hd:");
	port = (ctrl_reg->set_uom.connector_number) - 1;
	amd_crb_drivers_ucsi_ecDataLog(port, g_ec_ucsi.activeCmd);
#endif		
 
    switch(ucsi_regs.control.command) {
        case UCSI_CMD_RESERVED:
            cmd_status = UCSI_STS_NOT_SUPPORTED;
            break;            

        case UCSI_CMD_SET_CCOM:
			port = (ctrl_reg->set_uom.connector_number) - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
            dpm_stat = dpm_get_info (port);
						
            if(dpm_stat->attach)
                cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
            else {
	            port_role_t req_port_role = port_role_map[ctrl_reg->set_uom.usb_operating_mode];
							
	            if (!amd_crb_drivers_tps_portConfigStateMachineMatch(port, req_port_role)) {
		            g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
		            /* Send the command to pd controller */
		            amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
		            g_ec_ucsi.dpmPort = port;
		            g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
		            cmd_status = UCSI_STS_BUSY;	
	            }
	            else {
	            	cmd_status = UCSI_STS_CMD_COMPLETED;
	            }
            }
            break;
        
        case UCSI_CMD_SET_UOR:
            
            port = (ctrl_reg->set_uor.connector_number) - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
            dpm_stat = dpm_get_info (port);

            if(!(dpm_stat->contract_exist)) {
                cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
            }
            else {
                port_type_t req_port_role;
                if (ctrl_reg->set_uor.operate_as_dfp)
                    req_port_role = PRT_TYPE_DFP;
                else if (ctrl_reg->set_uor.operate_as_ufp)
                    req_port_role = PRT_TYPE_UFP;
                else if (ctrl_reg->set_uor.accept_dr_swap)
                    req_port_role = PRT_TYPE_DRP;
                else {
                    cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
                    break;
                }
								
                if(req_port_role == PRT_TYPE_DRP) {
                    if ((dpm_get_info(port)->swap_response & UCSI_DPM_DR_SWAP_RESP_MASK) != (APP_RESP_ACCEPT << UCSI_DPM_DR_SWAP_RESP_POS)) {
                        g_ec_ucsi.ports[port].eventFilter = UCSI_EVT_PARTNER_STATUS_CHANGED;
                        g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                        /* Send the command to pd controller */
                        amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                        g_ec_ucsi.dpmPort = port;
                        g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY;
                    }
                    /* set Accept DR SWAP flag. */
                    dpm_set_status(port)->swap_response &= ~UCSI_DPM_DR_SWAP_RESP_MASK;
                    dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_DR_SWAP_RESP_POS;
                }
                else if(dpm_stat->cur_port_type != req_port_role) {
                    if (dpm_stat->alt_mode_entered) {
                        cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
                    }
                    else {
                        g_ec_ucsi.ports[port].eventFilter = UCSI_EVT_PARTNER_STATUS_CHANGED;
						g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
						/* Send the command to pd controller */
						amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
						g_ec_ucsi.dpmPort = port;
						g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY;
                    }
                }
                else {
                    cmd_status = UCSI_STS_CMD_COMPLETED;
                }
            }
            break;

#if UCSI_SET_PWR_LEVEL_ENABLE
         /* Use the 4CC command set_power_level or not */
        case UCSI_CMD_SET_POWER_LEVEL: {
				/* Power for each port */
				if (ctrl_reg->set_power_level.connector_number != 0)  {
					port = ctrl_reg->set_power_level.connector_number - 1;

					if(port >= NO_OF_TYPEC_PORTS)
						return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
				}
				/* Power for all ports */
				else {
					/* Need to implement a strategy to controll total power cnosumption  */
					if (dpm_get_info(TYPEC_PORT_0_IDX)->attach)
						port = TYPEC_PORT_0_IDX;
#if PD_DUALPORT_ENABLE
					else if (dpm_get_info(TYPEC_PORT_1_IDX)->attach)
						port = TYPEC_PORT_1_IDX;
#endif
#if PD_TRIPPORT_ENABLE
					else if (dpm_get_info(TYPEC_PORT_2_IDX)->attach)
						port = TYPEC_PORT_2_IDX;
#endif									
				}

				g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
				/* Send the command to pd controller */
				amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
				g_ec_ucsi.dpmPort = port;
				g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
				cmd_status = UCSI_STS_BUSY;
				break;
			}
#endif /* UCSI_SET_PWR_LEVEL_ENABLE */

        case UCSI_CMD_SET_SINK_PATH:
        	break;

        case UCSI_CMD_SET_PDR:           
            port = ctrl_reg->set_pdr.connector_number - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
            dpm_stat = dpm_get_info (port);    
            if(!(dpm_stat->contract_exist))
                cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
            else {
                /* workaround to force the pr swap response as accept before send the command */
            	amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_ACCEPT);
							
                if((ctrl_reg->set_pdr.operate_as_snk && (dpm_stat->cur_port_role == PRT_ROLE_SOURCE)) ||
                   (ctrl_reg->set_pdr.operate_as_src && (dpm_stat->cur_port_role == PRT_ROLE_SINK))) {
                	g_ec_ucsi.ports[port].eventFilter = UCSI_EVT_PD_CONTRACT_CHANGED |
                                                       UCSI_EVT_EXT_SUPPLY_CHANGED | 
                	                                   UCSI_EVT_POM_CHANGED | 
                	                                   UCSI_EVT_PARTNER_STATUS_CHANGED;
                	/* send the pr swap to partner */
                	if (ctrl_reg->set_pdr.operate_as_snk) 
                		amd_crb_drivers_tps_cmdx(0, port, NULL, 0, "SWSk");
                	else if (ctrl_reg->set_pdr.operate_as_src)
                		amd_crb_drivers_tps_cmdx(0, port, NULL, 0, "SWSr");
										
                	g_4cc_pendingCmdType[port] = CMD_TYPE_UCSI;
                	g_ec_ucsi.dpmPort = port;
                	g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                	cmd_status = UCSI_STS_BUSY;
                }
                /* Seems thtat if set the status to reject the pr swap commnad will be failed to initiate. So configure it after the 4CC cmd */
                if(ctrl_reg->set_pdr.accept_pr_swap) {
                	amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_ACCEPT);
                }
                else {
                	amd_crb_drivers_tps_portCtrlPrSwapResp(port, APP_RESP_REJECT);
                }

                /* Need to change the accept */
                if(ctrl_reg->set_pdr.accept_pr_swap) {
                	/* Accept PR SWAP. */
                	dpm_set_status(port)->swap_response &= ~UCSI_DPM_PR_SWAP_RESP_MASK;
                	dpm_set_status(port)->swap_response |= APP_RESP_ACCEPT << UCSI_DPM_PR_SWAP_RESP_POS;
                }
                else {
                	/* Reject PR SWAP. */
                	dpm_set_status(port)->swap_response &= ~UCSI_DPM_PR_SWAP_RESP_MASK;
                	dpm_set_status(port)->swap_response |= APP_RESP_REJECT << UCSI_DPM_PR_SWAP_RESP_POS;
                }									
            }

            break;
#if UCSI_ALT_MODE_ENABLED            
        case UCSI_CMD_GET_ALTERNATE_MODES: {
                uint8_t req_am_offset = ctrl_reg->get_alternate_modes.alterate_mode_offset; 
                uint8_t req_am_num = ctrl_reg->get_alternate_modes.num_alternate_modes;

				/* Set the alt mode to 2 for TBT and DP */
                uint8_t num_am_supp = 2;
                    
                port = ctrl_reg->get_alternate_modes.connector_number - 1;                           
                dpm_stat = dpm_get_info (port);

                if(port >= NO_OF_TYPEC_PORTS)
                    return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
                    
                if(ctrl_reg->get_alternate_modes.recipient > 3)
                    return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_INVALID_PARAMS);

                if(ctrl_reg->get_alternate_modes.recipient == UCSI_ALT_MODES_RECIPIENT_CONNECTOR) {
                    /* Check the num, supp and offset correct before comm with device */
                    if((req_am_num < 2) && 
                       (num_am_supp >= 1) && 
                       (req_am_offset < num_am_supp)) {
                        g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                        /* Send the command to pd controller */
                        amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                        g_ec_ucsi.dpmPort = port;
                        g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY;
                    }
                    else {
                        ucsi_regs.cci.data_length = 0;
                        cmd_status = UCSI_STS_CMD_COMPLETED;
                    }									
				}
                else if (ctrl_reg->get_alternate_modes.recipient == UCSI_ALT_MODES_RECIPIENT_SOP) {
					/* Need to confirm device sop alt mode entered and supported */
                    if((num_am_supp >= 1) && 
                       (req_am_offset <= (num_am_supp-1)) &&
                       ((dpm_get_info(port)->alt_mode_entered) || (dpm_stat->modal_op_support))) { /* neither into alt mode or mode support can trigger it */
                        g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                        /* Send the command to pd controller */
                        amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                        g_ec_ucsi.dpmPort = port;
                        g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY;
                    }
                    else {
                        ucsi_regs.cci.data_length = 0;
                        cmd_status = UCSI_STS_CMD_COMPLETED;
                    }
                }
                else if (ctrl_reg->get_alternate_modes.recipient == UCSI_ALT_MODES_RECIPIENT_SOP_PRIME) {
										/* Need to confirm emca attached and cable alt mode supported */
                    if((num_am_supp >= 1) && 
                       (req_am_offset <= (num_am_supp-1)) &&
                       (dpm_stat->emca_present != false) &&
                       (dpm_stat->cbl_mode_en != false)) {
                        g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                        /* Send the command to pd controller */
                        amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                        g_ec_ucsi.dpmPort = port;
                        g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY;
                    }
                    else {
                        ucsi_regs.cci.data_length = 0;
                        cmd_status = UCSI_STS_CMD_COMPLETED;
                    }                    
                }
                else if (ctrl_reg->get_alternate_modes.recipient == UCSI_ALT_MODES_RECIPIENT_SOP_DPRIME) {
					/* Need to confirm emca attached and cable alt mode supported */
                    if((num_am_supp >= 1) && 
                       (req_am_offset <= (num_am_supp-1)) &&
                       (dpm_stat->emca_present != false) &&
                       (dpm_stat->cbl_mode_en != false)) {
                        g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                        /* Send the command to pd controller */
                        amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                        g_ec_ucsi.dpmPort = port;
                        g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                        cmd_status = UCSI_STS_BUSY; 
                    }
                    else {
                        ucsi_regs.cci.data_length = 0;
                        cmd_status = UCSI_STS_CMD_COMPLETED;
                    }                    
                }
                else {
                    ucsi_regs.cci.data_length = 0;
                    cmd_status = UCSI_STS_CMD_COMPLETED;
                }                               
            }
         
            break;
            
        case UCSI_CMD_GET_CAM_SUPPORTED:
            port = ctrl_reg->get_cam_supported.connector_number - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);

            g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
            /* Send the command to pd controller */
            amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
            g_ec_ucsi.dpmPort = port;
            g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
            cmd_status = UCSI_STS_BUSY;                          
            break;
            
        case UCSI_CMD_GET_CURRENT_CAM:
            port = ctrl_reg->get_current_cam.connector_number - 1;
            dpm_stat = dpm_get_info (port);
				
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);

            if((get_alt_mode_number(port) != 0) && (dpm_stat->alt_mode_entered)) {
                for(alt_mode_idx = 0; alt_mode_idx < get_alt_mode_number(port); alt_mode_idx++) {
                	if(get_alt_mode_svids_supp(port, alt_mode_idx) == get_cur_alt_mode_id(port)) {
                		/* Loop check the current alt mode id from 1~max */
                		ucsi_regs.message_in[0] = alt_mode_idx;
                		break;
                	}
                }
												
                /* Don't know how to get the alt mode number and current alt mode id. Need to check with TI */
                ucsi_regs.message_in[0] = DP_ALT_MODE_ID;
                
                ucsi_regs.cci.data_length = 0x01;
                cmd_status = UCSI_STS_CMD_COMPLETED;    
            }
            else {
                ucsi_regs.message_in[0] = 0xFF;
                ucsi_regs.cci.data_length = 0x01;
                cmd_status = UCSI_STS_CMD_COMPLETED;
            }  						
            break;                     
                
        case UCSI_CMD_SET_NEW_CAM:
            port = ctrl_reg->set_new_cam.connector_number - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
            dpm_stat = dpm_get_info (port);
            /* Cannot execute if port is not enabled. */                    
            if(!(dpm_stat->contract_exist)) {
                cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
            }
            else {
            	/* Set alt mode number to 1 for now. Need to check with TI */
                for(alt_mode_idx = 0; alt_mode_idx < 1 ; alt_mode_idx++) {
                	/* Don't know how to get the alt mode number. Assume there is only one now. Need to check with TI */
                    if(alt_mode_idx == ctrl_reg->set_new_cam.new_cam) {
                    	/* Assume the SVID is 0xff01 DP. Need to check with TI */
                        if(DP_SVID == (uint32_t) ctrl_reg->set_new_cam.alt_mode_specific) {
                            if(ctrl_reg->set_new_cam.enter_or_exit == 0x01) {
                            	/* Initiate alt mode enter. Input the svid */
                            	amd_crb_drivers_tps_cmdAltMode(0, port, ctrl_reg->set_new_cam.alt_mode_specific, (ctrl_reg->set_new_cam.new_cam + 1), "AMEn");
                            }
                            else {
                            	/* Initiate alt mode exit. Input the svid */
                            	amd_crb_drivers_tps_cmdAltMode(0, port, ctrl_reg->set_new_cam.alt_mode_specific, (ctrl_reg->set_new_cam.new_cam + 1), "AMEx");
                            }
                        }
                    }
                    cmd_status = UCSI_STS_BUSY;//UCSI_STS_CMD_COMPLETED;
                }                                    
            }
            break;
#endif /*UCSI_ALT_MODE_ENABLED*/            
        
        case UCSI_CMD_GET_PDOS:
            port = ctrl_reg->get_pdos.connector_number - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
            dpm_stat = dpm_get_info (port);
            if(ctrl_reg->get_pdos.partner_pdo) {
               if(dpm_stat->contract_exist) {
                    g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                    /* Send the command to pd controller */
                    amd_crb_drivers_tps_cmdUcsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                    g_ec_ucsi.dpmPort = port;
                    g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                    g_ec_ucsi.ports[port].eventFilter = UCSI_EVT_PD_CONTRACT_CHANGED | UCSI_EVT_EXT_SUPPLY_CHANGED;
                    cmd_status = UCSI_STS_BUSY;
                }
                else {
                    ucsi_regs.cci.data_length = 0;
                    cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_PD_COMM_ERROR);
                }
            }
            else /* Connector's PDOs */
            {
#if UCSI_EC_HANDLE_GET_PDO_CONN
                if(ctrl_reg->get_pdos.is_source_pdo) {
                    switch(ctrl_reg->get_pdos.src_cap_type) {
                        case UCSI_CUR_SUPPORTED_SRC_CAPS:
                        {      
                            uint8_t i = 0, pdo_num = 0, valid_pdo_offset = 0;
                            for(i = 0; i < dpm_stat->src_pdo_count; i++) {
                                if ((dpm_stat->src_pdo_mask & (0x01 << i)) != 0) {
                                    if(valid_pdo_offset >= ctrl_reg->get_pdos.pdo_offset)
                                        memcpy(&ucsi_regs.message_in[(pdo_num++) * 4], &(dpm_stat->src_pdo[i]), sizeof(pd_do_t));
                                    valid_pdo_offset++;
                                    if (pdo_num >= (ctrl_reg->get_pdos.num_pdos + 1))
                                        break;
                                }
                            }
                            ucsi_regs.cci.data_length = sizeof(pd_do_t) * pdo_num;
                            break;                                                                                    
                        }

                        case UCSI_ADVERTISED_SRC_CAPS: {
                            /* Get the number of PDOs requested by EC. */                            
                            uint8_t pdo_requested = ctrl_reg->get_pdos.num_pdos + 1;
                            uint8_t pdo_count = 0;
                              
                                
                            /* Ensure PDO offset doesn't cross the number of PDOs. */
                            if (dpm_stat->cur_src_pdo_count > ctrl_reg->get_pdos.pdo_offset) {
                                /* Number of PDOs that can be sent in response. */
                                pdo_count = dpm_stat->cur_src_pdo_count - ctrl_reg->get_pdos.pdo_offset;
                                pdo_count = GET_MIN(pdo_count, pdo_requested);
                                ucsi_regs.cci.data_length = pdo_count << 2;
                                memcpy(ucsi_regs.message_in, &(dpm_stat->cur_src_pdo[ctrl_reg->get_pdos.pdo_offset]),
                                        ucsi_regs.cci.data_length);
                            }
                            else {
                                ucsi_regs.cci.data_length = 0;
                            }
                            break;
                        }
                        
                        case UCSI_MAX_SUPPORTED_SRC_CAPS: {
                            /* Get the number of PDOs requested by EC. */                            
                            uint8_t pdo_requested = ctrl_reg->get_pdos.num_pdos + 1; 
                            if (dpm_stat->src_pdo_count > ctrl_reg->get_pdos.pdo_offset) {
                                ucsi_regs.cci.data_length = UCSI_BOUND(sizeof(pd_do_t) * pdo_requested,
                                                            sizeof(pd_do_t) * (dpm_stat->src_pdo_count - ctrl_reg->get_pdos.pdo_offset));
                                memcpy(ucsi_regs.message_in, &(dpm_stat->src_pdo[ctrl_reg->get_pdos.pdo_offset]),
                                        ucsi_regs.cci.data_length);
                            }
                            else {
                                ucsi_regs.cci.data_length = 0;
                            }
                            break;
                        }
                        default:
                            cmd_status = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNRECOGNIZED_CMD);
                            ucsi_regs.cci.data_length = 0;
                            break;
                    }
                }
                else {
                    /* Get the number of PDOs requested by EC. */                            
                    uint8_t pdo_requested = ctrl_reg->get_pdos.num_pdos + 1;                    
                    if ((dpm_stat->snk_pdo_count > ctrl_reg->get_pdos.pdo_offset) && (pdo_requested != 0)) {
                        ucsi_regs.cci.data_length = UCSI_BOUND(sizeof(pd_do_t) * pdo_requested,
                                                    sizeof(pd_do_t) * (dpm_stat->snk_pdo_count - ctrl_reg->get_pdos.pdo_offset));
                        memcpy(ucsi_regs.message_in, &(dpm_stat->snk_pdo[ctrl_reg->get_pdos.pdo_offset]),
                                            ucsi_regs.cci.data_length);
                    }
                }
#else
                g_ec_ucsi.dpmCmdStatus = CMD_QUEUED;
                /* Send the command to pd controller */
                dev_tps_cmd_ucsi(0, port, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                g_ec_ucsi.dpmPort = port;
                g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                cmd_status = UCSI_STS_BUSY;
#endif
            }
            break;
            
        case UCSI_CMD_GET_CABLE_PROPERTY: {
                port = ctrl_reg->get_cable_property.connector_number - 1;            
                if(port >= NO_OF_TYPEC_PORTS)
                    return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);
                    
                ucsi_cable_property_t * cable_prop = (ucsi_cable_property_t *)&(ucsi_regs.message_in[0]);
                pd_do_t cbl_vdo;
                /* Current Map 3A = 50*60, 5A = 50*100*/
                uint8_t current_map[4] = {0, UCSI_CABLE_CURR_3A, UCSI_CABLE_CURR_5A, 0};
                /* Speed Mapping 
                 * bmSpeedSupported = M * 10^(3*e)
                 * M = Mantissa, e = Exponent
                 * 
                 * 00 - USB2.0 only, 480 Mbps     = 0000 0111 1000 00 10
                 * 01 - USB 3.1 Gen 1, 5Gbps      = 0000 0000 0001 00 11
                 * 10 - USB 3.1 Gen 1 & 2, 10Gbps = 0000 0000 0010 10 11
                */
                uint16_t speed_map[3] = {UCSI_CABLE_SPEED_480MBPS, UCSI_CABLE_SPEED_5GBPS, UCSI_CABLE_SPEED_10GBPS};
								
                /* read cbl vdo from the reg directly */
                cbl_vdo.val = amd_crb_drivers_tps_readCableVdo(port);

                cable_prop->bmSpeedSupported = speed_map[GET_MIN(cbl_vdo.std_cbl_vdo.usb_ss_sup, 0x02)];
                cable_prop->bCurrentCapability = current_map[cbl_vdo.std_cbl_vdo.vbus_cur];
                cable_prop->VBUSInCable = cbl_vdo.std_cbl_vdo.vbus_thru_cbl;
                cable_prop->CableType = cbl_vdo.std_cbl_vdo.cbl_term >> 1;
                if(cbl_vdo.val & UCSI_CABLE_VDO_DIRECTION)
                    cable_prop->Directionality = 1;
                cable_prop->PlugEndType = cbl_vdo.std_cbl_vdo.typec_abc;
                cable_prop->ModeSupport = 0;
                cable_prop->Latency = cbl_vdo.std_cbl_vdo.cbl_latency;
                ucsi_regs.cci.data_length = UCSI_GET_CABLE_PROPERTY_DATA_LENGTH;
            }
            break;
            
        case UCSI_CMD_GET_CONNECTOR_STATUS:
            port = ctrl_reg->get_connector_status.connector_number - 1;
            if(port >= NO_OF_TYPEC_PORTS)
                return amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNKNOWN_CONNECTOR);

            cmd_status = get_conn_status(port, ucsi_regs.message_in);
            if(cmd_status == UCSI_STS_CMD_COMPLETED)
                ucsi_regs.cci.data_length = UCSI_GET_CONNECTOR_STATUS_DATA_LENGTH;

            break;

        default:
            cmd_status = UCSI_STS_IGNORE;
            break;
    }  
    return cmd_status;
}

/**
 * amd_crb_drivers_ec_ucsi_task
 *
 * @note
 *  UCSI task handler.
 */
void amd_crb_drivers_ec_ucsi_task()
{
    uint8_t port_idx;

    if (_s_resetPending) {
			/* Both ports flag are cleared for PPM reset */
        if (_s_typecResetCmdCompletion == 0) {
            _s_resetPending = false;
            amd_crb_drivers_ec_ucsi_setStatus(UCSI_STS_RESET_COMPLETED);
            g_ec_ucsi.dpmCmdStatus = NO_CMD;
            g_ec_ucsi.activeCmd = UCSI_CMD_RESERVED;
            g_ec_ucsi_cmdAvailable = false; /* Clear all the pending commands during the reset period */
#if UCSI_DEBUG			
            amd_crb_drivers_ucsi_ecStringLog("reset complete");
#endif
        }
        return;
    }

    /* Check if the PPM Reset command has been issued by the OPM. */
    if (g_ec_ucsi_cmdAvailable && (ucsi_regs.control.command == UCSI_CMD_PPM_RESET)) {
        /* Handle the PPM Reset. */
    	amd_crb_drivers_ec_ucsi_cmdPpmRstHandler ();
			
        amd_crb_drivers_ec_ucsi_setStatus(UCSI_STS_BUSY);

        /* Reset the relevant variables to their default values. */
        g_ec_ucsi.dpmCmdStatus = NO_CMD;
        g_ec_ucsi.eventMask = 0x00;

        g_ec_ucsi_readPending = false;
        g_ec_ucsi_cmdAvailable = false;

        /* Transition to PPM Idle state. */
        gl_ppm_state = PPM_IDLE;

        if (_s_resetPending == false) {
            amd_crb_drivers_ec_ucsi_setStatus(UCSI_STS_RESET_COMPLETED);
            return;
        }
    }

    switch(gl_ppm_state) {
        case PPM_IDLE: {
                /* If previous read pending is not cleared. And there is no command input from OS. It will trigger logic back to idle to process the commnad.
                But we need to wait host to finish reading the previous packet to avoid overlay between two resposne packates */
                if(g_ec_ucsi_readPending) {
                	break;
                }
						
                /* If the OPM hasn't sent any command, then process notifications */ 
                if(g_ec_ucsi_cmdAvailable == false) {
                    /* Notify only if the command completed event is set */
                    if(!(g_ec_ucsi.eventMask & UCSI_EVT_CMD_COMPLETED))
                        break;

                    /* If a notification is pending, wait until it's cleared */
                    if(ucsi_regs.cci.connector_change)
                        break;

                    for(port_idx = 0; port_idx < NO_OF_TYPEC_PORTS; port_idx++) {
                        if(g_ec_ucsi.ports[port_idx].notifications) {
                            amd_crb_drivers_ucsi_ecStringLog("Trigger Notify");
                            /* If a transaction was just started, don't send the notification */
                            ucsi_regs.cci.data_length = 0;
	                            ucsi_regs.cci.connector_change = port_idx + 1;
                            gl_ppm_state = PPM_READ_PENDING;
                            /*Update UCSI Status Register*/
                            amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_CMD_IN_PROGRESS);
                            amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
                            break;
                        }
                    }
                    break;
                }

                g_ec_ucsi_cmdAvailable = false;

                /* If the command completed notification isn't set, then fail this command */
                if(amd_crb_drivers_ppmCmdReqCmplt(ucsi_regs.control.command) &&
                   !(g_ec_ucsi.eventMask & UCSI_EVT_CMD_COMPLETED)) {
                    g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNRECOGNIZED_CMD);
                    memset(&ucsi_regs.cci, 0x00, sizeof(ucsi_regs.cci));
                    break;
                }
                else {
                    /* Clear the data memory first */
                    memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));
                    ucsi_regs.cci.data_length = 0;
							
#if (UCSI_REV_1_x_ENABLE)
                    if( (ucsi_regs.control.command > UCSI_CMD_SET_POWER_LEVEL)
#elif (UCSI_REV_2_x_ENABLE)
                    if( (ucsi_regs.control.command > UCSI_CMD_SET_SINK_PATH)
#endif
                            || (ucsi_regs.control.command == UCSI_CMD_SET_PDM)
#if (!UCSI_ALT_MODE_ENABLED)
                            || (ucsi_regs.control.command == UCSI_CMD_GET_ALTERNATE_MODES)
#endif /* UCSI_ALT_MODE_ENABLED*/

                      ) {
                        g_ec_ucsi_response = (ucsi_status_indicator_t)(UCSI_STS_NOT_SUPPORTED | UCSI_STS_CMD_COMPLETED);
                        amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
                        break;
                    }

                    g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handleRst();

                    if(g_ec_ucsi_response == UCSI_STS_IGNORE) {
                        g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handleAckCmds();
                    }

                    if(g_ec_ucsi_response == UCSI_STS_IGNORE)
                        g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handlePpmCmds();

                    if(g_ec_ucsi_response == UCSI_STS_IGNORE)
                        g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handlePortCmd();
                }

                /* If this command requires time to finish, then wait in the BUSY state */
                if(g_ec_ucsi_response == UCSI_STS_BUSY)
                    gl_ppm_state = PPM_BUSY;
                else if(g_ec_ucsi_response == UCSI_STS_IGNORE) {
                    /* If nobody processed the command, drop it and go back to the idle state */
                    return;
                }
                else if ((g_ec_ucsi_response == UCSI_STS_RESET_COMPLETED) || (g_ec_ucsi_response == UCSI_STS_ACK_CMD)) {
                    /* Update the response. */
                }
                else {
                    gl_ppm_state = PPM_READ_PENDING;
                    /* Always set the command completed notification */
                    if((g_ec_ucsi_response != UCSI_STS_RESET_COMPLETED)
                    && (g_ec_ucsi_response != UCSI_STS_ACK_CMD))
                        g_ec_ucsi_response |= UCSI_STS_CMD_COMPLETED;
                }

                amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
                break;
            }

        case PPM_READ_PENDING:
            
            /* If the EC/BIOS started to write data here, then we're out of sequence.
             * Reset the state machine back to IDLE */
            if(g_ec_ucsi_cmdAvailable && (ucsi_regs.control.command != UCSI_CMD_PPM_RESET)) {
                g_ec_ucsi.ports[TYPEC_PORT_0_IDX].eventFilter = 0;
#if PD_DUALPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_1_IDX].eventFilter = 0;
#endif
#if PD_TRIPPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_2_IDX].eventFilter = 0;
#endif
                amd_crb_drivers_ec_ucsi_clearStatusBit (UCSI_EVENT_PENDING);
                amd_crb_drivers_ec_ucsi_clearStatusBit(UCSI_CMD_IN_PROGRESS);
                gl_ppm_state = PPM_IDLE;
#if UCSI_DEBUG			
                amd_crb_drivers_ucsi_ecStringLog("read pending reset:");
#endif
                break;
            }

            if(g_ec_ucsi_readPending) {
            	amd_crb_drivers_ec_ucsi_setStatusBit(UCSI_CMD_IN_PROGRESS);
                break;
            }
						
#if UCSI_DEBUG			
            amd_crb_drivers_ucsi_ecStringLog("host read");
#endif

            /* ACK is required. Move to the respective state */
            if((g_ec_ucsi.activeCmd != UCSI_CMD_RESERVED) && (ucsi_regs.cci.indicators != UCSI_STS_BUSY)) {
                gl_ppm_state = PPM_WAIT_FOR_ACK;
#if UCSI_DEBUG			
                amd_crb_drivers_ucsi_ecStringLog("wait for ack");
#endif
            }
            else /* Command is done. Clear the notification fiter and move to IDLE */ {
                g_ec_ucsi.ports[TYPEC_PORT_0_IDX].eventFilter = 0;
#if PD_DUALPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_1_IDX].eventFilter = 0;
#endif
#if PD_TRIPPORT_ENABLE
                g_ec_ucsi.ports[TYPEC_PORT_2_IDX].eventFilter = 0;
#endif
                gl_ppm_state = PPM_IDLE;
            }            
            break;

        case PPM_BUSY:
            /* If the DPM command wasn't sent, retry */
            if(g_ec_ucsi.dpmCmdStatus == CMD_QUEUED) {
            	amd_crb_drivers_tps_cmdUcsi(0, g_ec_ucsi.dpmPort, ucsi_regs.control.command, ucsi_regs.control.data_length,  (&ucsi_regs.control.details));
                g_ec_ucsi.dpmCmdStatus = CMD_IN_PROGRESS;
                break;
            }
                       
            /* If OPM hasn't read the data or if the DPM command is in progress, wait */
            if(g_ec_ucsi_readPending || (g_ec_ucsi.dpmCmdStatus == CMD_IN_PROGRESS)) {
            
                /* If the command completed notification isn't set, then fail this command */
                if(amd_crb_drivers_ppmCmdReqCmplt(ucsi_regs.control.command) &&
                  !(g_ec_ucsi.eventMask & UCSI_EVT_CMD_COMPLETED)) {
                    g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNRECOGNIZED_CMD);
                }
                else {
                    
                    if( (ucsi_regs.cci.indicators == UCSI_STS_BUSY) && (ucsi_regs.control.command == UCSI_CMD_CANCEL) ) {
#if UCSI_DEBUG			
                        amd_crb_drivers_ucsi_ecStringLog("cancel in busy:");
#endif
                        /* Clear the data memory first */
                        memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));
                        ucsi_regs.cci.data_length = 0;
                        
                        g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handleAckCmds();
                    
                        /* If this command requires time to finish, then wait in the BUSY state */
                        if(g_ec_ucsi_response == UCSI_STS_BUSY)
                            gl_ppm_state = PPM_BUSY;
                        else if(g_ec_ucsi_response == UCSI_STS_IGNORE) {
                            /* If nobody processed the command, drop it and go back to the idle state */
                            return;
                        }
                        else {
                            gl_ppm_state = PPM_READ_PENDING;
                            /* Always set the command completed notification */
                            if((g_ec_ucsi_response != UCSI_STS_RESET_COMPLETED) && (g_ec_ucsi_response != UCSI_STS_ACK_CMD))
                                g_ec_ucsi_response |= UCSI_STS_CMD_COMPLETED;
                        }

                        amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
                    }
                    else if(ucsi_regs.cci.indicators == UCSI_STS_BUSY) {
                        /**
                            As per spec when PPM is busy / Busy Indicator field is set to one, 
                            then no other bits in the UCSI data structure shall be set by the PPM.
                            However, GET_PDO RS5 HLK test fails due to EC delayed reading of UCSI INT.
                            In this case, it will clear data data length and EC will read zero length.
                        */
											
                        /* If this command requires time to finish, then wait in the BUSY state */
                        gl_ppm_state = PPM_BUSY;                        
                    }  
                }
                break;
            }
						
#if UCSI_DEBUG			
            amd_crb_drivers_ucsi_ecStringLog("host read busy + device comm complete:");
#endif

            /* DPM command is done. Send the result to the OPM */
            if(g_ec_ucsi.dpmCmdStatus == CMD_ERROR) {
                amd_crb_drivers_ec_ucsi_setStatus(UCSI_STS_ERROR | UCSI_STS_CMD_COMPLETED);
            }
            else {
                amd_crb_drivers_ec_ucsi_setStatus(UCSI_STS_CMD_COMPLETED);
            }
            g_ec_ucsi.dpmCmdStatus = NO_CMD;
            gl_ppm_state = PPM_READ_PENDING;
            break;

        case PPM_WAIT_FOR_ACK:
            if(!g_ec_ucsi_cmdAvailable)
                break;
						
#if UCSI_DEBUG			
            amd_crb_drivers_ucsi_ecStringLog("recv ack cmd");
#endif

            g_ec_ucsi_cmdAvailable = false;

            /* Clear the data memory first */
            memset(ucsi_regs.message_in, 0x00, sizeof(ucsi_regs.message_in));
            ucsi_regs.cci.data_length = 0;

            g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handleAckCmds();
            if(g_ec_ucsi_response == UCSI_STS_IGNORE) /* Not an ACK or CANCEL */
                g_ec_ucsi_response = amd_crb_drivers_ec_ucsi_handleRst();
						
            /* connector reset will need to go to busy mode and wait for device response */
            if(g_ec_ucsi_response == UCSI_STS_BUSY) {
	            /* get in busy mode, inform os and break */
	            gl_ppm_state = PPM_BUSY;
	            amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
	            break;
            }
            
            if(g_ec_ucsi_response == UCSI_STS_IGNORE) /* Not a PPM_RESET */
                g_ec_ucsi_response = (amd_crb_drivers_ec_ucsi_error(UCSI_ERR_UNRECOGNIZED_CMD) | UCSI_STS_CMD_COMPLETED);

            amd_crb_drivers_ec_ucsi_setStatus(g_ec_ucsi_response);
            gl_ppm_state = PPM_READ_PENDING;
            break;
    }
}

/**
 * amd_crb_drivers_ec_ucsi_regReset
 *
 * @note
 *  Clear status & control register
 */
void amd_crb_drivers_ec_ucsi_regReset(void)
{
    _s_ucsiStatus = 0;
    _s_ucsiControl = 0;
}

/**
 * amd_crb_drivers_ec_ucsi_clearStatusBit
 *
 * @note
 *  Clear appropriate bit_idx to 0 in the status register.
 */
void amd_crb_drivers_ec_ucsi_clearStatusBit(uint8_t bit_idx)
{
    _s_ucsiStatus &= (~ (1 << ((uint8_t)(bit_idx))));
}

/**
 * amd_crb_drivers_ec_ucsi_setStatusBit
 *
 * @note
 *  Set appropriate bit_idx to 0 in the status register.
 */
void amd_crb_drivers_ec_ucsi_setStatusBit(uint8_t bit_idx)
{
    _s_ucsiStatus |=  (1 << ((uint8_t)(bit_idx)));
}

/**
 * amd_crb_drivers_ec_ucsi_cypdSelector
 *
 * @param [in]              Ctrl;     control message pointer
 * @param [in]      PrevCommPort;     previous communicate port number
 * @return command message or specific port message.
 *         0xCC for common message.
 *         0xEE for GET_ERROR_STATUS
 *         0x00/0x01/0x02 for specific port number.
 *
 * @note
 *  return the correct ccgx chip id for UCSI message
 */
uint8_t amd_crb_drivers_ec_ucsi_cypdSelector(volatile uint8_t *Ctrl, uint8_t PrevCommPort)
{
	uint8_t port = 0;
	
	/* Separate command message and specific port message */
	if ((Ctrl[0] == UCSI_CMD_PPM_RESET) ||
		(Ctrl[0] == UCSI_CMD_SET_NOTIFICATION_ENABLE) ||
		(Ctrl[0] == UCSI_CMD_GET_CAPABILITY)) {
		/* Command cmd */
		return 0xCC;
	}
	else if ((Ctrl[0] == UCSI_CMD_ACK_CC_CI) ||
			 (Ctrl[0] == UCSI_CMD_CANCEL)) {
		/* for ACK_CCI and CANCEL need to identify previous cmd */
		return PrevCommPort;
	}
	else if (Ctrl[0] == UCSI_CMD_GET_ERROR_STATUS) {
		return 0xEE;
	}
	else if (Ctrl[0] == UCSI_CMD_GET_ALTERNATE_MODES) {
		/* GET_ALTERNATE_MODES connecter number location is different to others */
		port = Ctrl[3] & 0x7F;
		if ((port > 0) && (port <= NO_OF_TYPEC_PORTS))
			return (port - 1);
		else
			return TYPEC_PORT_0_IDX;
	}
	else {
		/* UCSI port number start from 1 to further */
		port = Ctrl[2] & 0x7F;
		if ((port > 0) && (port <= NO_OF_TYPEC_PORTS))
			return (port - 1);
		else
			return TYPEC_PORT_0_IDX;
	}
}

/**
 * amd_crb_drivers_ec_ucsi_cypdAdjustPort
 *
 * @param [in]              Ctrl;     control message pointer
 *
 * @note
 *  adjust the port number based on different ctrl message
 */
void amd_crb_drivers_ec_ucsi_cypdAdjustPort(volatile uint8_t *Ctrl)
{
	uint8_t port = 0;
	
	if (Ctrl[0] == UCSI_CMD_GET_ALTERNATE_MODES) {
		LOG_DBG("Original CTRL:%x, %x, %x, %x", Ctrl[0], Ctrl[1], Ctrl[2], Ctrl[3]);
		/* GET_ALTERNATE_MODES connecter number location is different to others */
		port = Ctrl[3] & 0x7F;
		if (port == (TYPEC_PORT_2_IDX + 1)) {
			port = TYPEC_PORT_0_IDX + 1;
			Ctrl[3] &= ~0x7F;
			Ctrl[3] |= port;
		}
		
		LOG_DBG("Force CTRL:%x, %x, %x, %x", Ctrl[0], Ctrl[1], Ctrl[2], Ctrl[3]);
	}
	else if (Ctrl[0] == UCSI_CMD_ACK_CC_CI) {
		/* Not over-write ACK */
	}
	else {
		LOG_DBG("Original CTRL:%x, %x, %x, %x", Ctrl[0], Ctrl[1], Ctrl[2], Ctrl[3]);
		/* UCSI port number start from 1 to further */
		port = Ctrl[2] & 0x7F;
		if (port == (TYPEC_PORT_2_IDX + 1))
		{
			port = TYPEC_PORT_0_IDX + 1;
			Ctrl[2] &= ~0x7F;
			Ctrl[2] |= port;
		}
		
		LOG_DBG("Force CTRL:%x, %x, %x, %x", Ctrl[0], Ctrl[1], Ctrl[2], Ctrl[3]);
	}
}

/* Log functions */
void amd_crb_drivers_ucsi_ecRegLog(uint8_t isRead, uint8_t reg, uint8_t* pBuf, uint32_t len)
{
	LOG_CLEARBUF;
	for ( uint32_t i = 0; i < len; i++ ) {
		LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
	}

	LOG_DBG("ECUCSI Reg-->IsRead:%d, reg(%d), len(%d) data(%s) ",isRead, reg, len, LOG_BUF);

}
void amd_crb_drivers_ucsi_ecStringLog(char* str)
{
	LOG_DBG("ECUCSI String-->%s",str);
}
void amd_crb_drivers_ucsi_ecDataLog(uint8_t port, uint32_t data)
{
	LOG_DBG("ECUCSI p:%x data-->%x",port, data);
}
void amd_crb_drivers_ucsi_ecLogCtrlCmd(uint8_t command)
{
	LOG_DBG("ECUCSI control-commnad->%d", command);
}
void amd_crb_drivers_ucsi_ecLogCtrlDataLen(uint8_t data_length)
{
	LOG_DBG("ECUCSI control-data length->%d", data_length);
}
void amd_crb_drivers_ucsi_ecLogCtrlDetails(uint8_t* details, uint8_t len)
{
	LOG_CLEARBUF;
	for ( uint32_t i = 0; i < len; i++ ) {
		LOG_APPEND(" %02X", *((uint8_t *)details + i));
	}

	LOG_DBG("ECUCSI control-data length->%s", LOG_BUF);
}
void amd_crb_drivers_ucsi_ecLogMsgOut(uint8_t* out, uint8_t len)
{
	LOG_CLEARBUF;
	for ( uint32_t i = 0; i < len; i++ ) {
		LOG_APPEND(" %02X", *((uint8_t *)out + i));
	}

	LOG_DBG("ECUCSI message out->%s", LOG_BUF);
}
void amd_crb_drivers_ucsi_ecLogMsgIn(uint8_t* in, uint8_t len)
{
	LOG_CLEARBUF;
	for ( uint32_t i = 0; i < len; i++ ) {
		LOG_APPEND(" %02X", *((uint8_t *)in + i));
	}

	LOG_DBG("ECUCSI message in->%s", LOG_BUF);
}
void amd_crb_drivers_ucsi_ecLogCciConnectorNum(uint8_t num)
{
	LOG_DBG("ECUCSI cci-conn num->%d", num);
}
void amd_crb_drivers_ucsi_ecLogCciDataLen(uint8_t data_length)
{
	LOG_DBG("ECUCSI cci-data_length->%d", data_length);
}
#if UCSI_REV_1_x_ENABLE
void amd_crb_drivers_ucsi_ecLogCciIndicators(uint8_t indicators)
{
	LOG_DBG("ECUCSI cci-indicators->%d", indicators);
	if ((indicators >> 0)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_RESERVED");
	}
	if ((indicators >> 1)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_NOT_SUPPORTED");
	}
	if ((indicators >> 2)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_CANCEL_COMPLETED");
	}
	if ((indicators >> 3)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_RESET_COMPLETED");
	}
	if ((indicators >> 4)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_BUSY");
	}
	if ((indicators >> 5)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_ACK_CMD");
	}
	if ((indicators >> 6)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_ERROR");
	}
	if ((indicators >> 7)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_CMD_COMPLETED");
	}
}
#elif UCSI_REV_2_x_ENABLE
void amd_crb_drivers_ucsi_ecLogCciIndicators(uint8_t indicators)
{
	LOG_DBG("ECUCSI cci-indicators->%d", indicators);
	if ((indicators >> 7)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_SECURITY_REQUEST");
	}
	if ((indicators >> 8)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_TST_FW_UPDATE_REQUEST");
	}
	if ((indicators >> 9)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_NOT_SUPPORTED");
	}
	if ((indicators >> 10)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_CANCEL_COMPLETED");
	}
	if ((indicators >> 11)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_RESET_COMPLETED");
	}
	if ((indicators >> 12)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_BUSY");
	}
	if ((indicators >> 13)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_ACK_CMD");
	}
	if ((indicators >> 14)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_ERROR");
	}
	if ((indicators >> 15)&0x01) {
		LOG_DBG("ECUCSI cci-indicators->UCSI_STS_CMD_COMPLETED");
	}
}
#endif

#endif /* CONFIG_EC_UCSI_ENABLE */

/* [] END OF FILE */

