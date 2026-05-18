/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_HPI_H_
#define AMD_CRB_DRIVERS_HPI_H_

#include "amd_crb_drivers_pd.h"
#include "system.h"

/* HPI response */
#define HPI_RESP_NO_RESPONSE                         0x00
#define HPI_RESP_SUCCESS                             0x02
#define HPI_RESP_FLASH_DATA_AVAILABLE                0x03
#define HPI_RESP_INVALID_COMMAND                     0x05
#define HPI_RESP_FLASH_UPDATE_FAILED                 0x07
#define HPI_RESP_INVALID_FW                          0x08
#define HPI_RESP_INVALID_ARGUMENTS                   0x09
#define HPI_RESP_NOT_SUPPORTED                       0x0A
#define HPI_RESP_TRANSACTION_FAILED                  0x0C
#define HPI_RESP_PD_COMMAND_FAILED                   0x0D
#define HPI_RESP_UNDEFINED_ERROR                     0x0F
#define HPI_RESP_READ_PDO_DATA                       0x10
#define HPI_RESP_CMD_ABORTED                         0x11
#define HPI_RESP_PORT_BUSY                           0x12
#define HPI_RESP_MIN_MAX_CURRENT                     0x13
#define HPI_RESP_EXT_SRC_CAP                         0x14
#define HPI_RESP_UCSI_IS_ALREADY_STARTED             0x15
#define HPI_RESP_I2C_REG                             0x1D
#define HPI_RESP_UPDATE_AC_LIMIT                     0x34
#define HPI_EVENT_RESET_COMPLETE                     0x80
#define HPI_EVENT_MSG_QUEUE_OVERFLOW                 0x81
/* Type-C Specific Events */
#define HPI_EVENT_OVER_CURRENT_DETECTED              0x82
#define HPI_EVENT_OVER_VOLTAGE_DETECTED              0x83
#define HPI_EVENT_TYPE_C_PORT_CONNECT                0x84
#define HPI_EVENT_TYPE_C_PORT_DISCONNECT             0x85
#define HPI_EVENT_PD_CONTRACT_NEGOTIATION_COMPLETE   0x86
#define HPI_INVALID_RESPONSE_CODE                    0xFF

/* Cypress firmware mode */
#define CYPRESS_HPI_FwMODE_BOOT                      (0)
#define CYPRESS_HPI_FwMODE_FW1                       (1)
#define CYPRESS_HPI_FwMODE_FW2                       (2)

#define HPI_READ                                     (1)
#define HPI_WRITE                                    (0)

#define _BYTEORDER16(val)   ((uint16_t)((((uint16_t)(val) & 0xFF00) >> 8) | (((uint16_t)(val) & 0x00FF) << 8)))
#define _BYTEORDER32(val)   ((uint32_t)((_BYTEORDER16((uint16_t)(val) & 0xFFFF) << 16) | (_BYTEORDER16((uint16_t)((val) >> 16 & 0xFFFF)))))

#if defined(__CC_ARM)
  #pragma push
  #pragma anon_unions
#elif defined(__ICCARM__)
  #pragma language=extended
#elif defined(__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined(__TMS470__)
/* anonymous unions are enabled by default */
#elif defined(__TASKING__)
  #pragma warning 586
#else
  #warning Not supported compiler type
#endif

#pragma pack(push, 1)

typedef union {
	uint8_t u8Mode;
	struct {
		uint8_t firmwareMode          : 2;
		uint8_t numOfPdPortsSupported : 2;
		uint8_t flashRomSize          : 2;
		uint8_t reserved_6            : 1;
		uint8_t hpiVersion            : 1;
	} f;
} DRV_HPI_Rx0000_DEVICE_MODE;

typedef union {
	uint8_t u8IntSt;
	struct {
		uint8_t devInt          : 1;
		uint8_t port0Int        : 1;
		uint8_t port1Int        : 1;
		uint8_t ucsiInt0        : 1;   // On CRB, bit7 indicates UCSI interrupt, but TED2.x not (it's bit 3).
		uint8_t reserved_4_6    : 3;
		uint8_t ucsiInt         : 1;   // Hence either bit 3 or bit 7 asserted will be treated as UCSI interrupt.
	} f;
} DRV_HPI_Rx0006_INT_STATE;

typedef union {
	uint8_t u8PortsEn;
	struct {
		uint8_t port0En         : 1;
		uint8_t port1En         : 1;
		uint8_t reserved_2_7    : 6;
	} f;
} DRV_HPI_Rx002C_PORT_ENABLE;

typedef union {
	uint8_t u8UcsiStatus;
	struct {
		uint8_t interfaceIsEnabled: 1;
		uint8_t commandIsPending: 1;
		uint8_t reserved_2_7    : 6;
	} f;
} DRV_HPI_Rx0030_UCSI_STATUS;

typedef union {
	uint8_t ui8Reg;
	struct {
		uint8_t b2PwSt		   : 2;    // EC --> CCGx
		uint8_t b2Retimer      : 2;    // CCGx --> EC   0:PS8828A 1:PS8830 2:KB8001 3:KB8002
		uint8_t b4FWSKU    	   : 4;    // EC --> CCGx
	} f;
} DRV_HPI_Rx0040_REG;

typedef union {
	uint32_t u32PdSt;
	struct {
		/* dft -> default; crt -> current */
		uint32_t dftPortDataRole                  : 2;   // [1:0]   00 - UFP; 01: DFP; 10 - DRP
		uint32_t dftPortDataRoleInCaseOfDRP       : 1;   // [2]     0 - UFP; 1 - DFP
		uint32_t dftPortPwrRole                   : 2;   // [4:3]   00 - Sink; 01 - Source; 10 - Dual Role
		uint32_t dftPortPwrRoleInCaseOfDualRole   : 1;   // [5]     0 - Sink; 1 - Source
		uint32_t crtPortDataRole                  : 1;   // [6]     0 - UFP; 1 - DFP
		uint32_t                                  : 1;
		uint32_t crtPortPwrRole                   : 1;   // [8]     0 - Sink; 1 - Source
		uint32_t                                  : 1;
		uint32_t contractStatus                   : 1;   // [10]    0 - No Contract exists with port partner; 1 - Contract Exists with port partner
		uint32_t emcaPresent                      : 1;   // [11]    0 - EMCA is present; 1 - EMCA is not detected
		uint32_t vconnSupplier                    : 1;   // [12]    0 - CCG is not the current supplier of VConn; 1 - It is.
		uint32_t vconnStatus                      : 1;   // [13]    0 - CCG is not providing VConn supply; 1 - It has VConn suppply enabled
		uint32_t pd30RpStatus                     : 1;   // [14]    0 - Rp is SinkTxOk; 1 - Rp is SinkTxNG
		uint32_t peReadyState                     : 1;   // [15]    0 - Port is not in PE_READY state; 1 - It is.
		uint32_t pdSpecRevisionSupported          : 2;   // [17:16] 00 - USB-PD Revision 2.0; 01 - USB-PD Revision 3.0
		uint32_t pd30SupportedByPortPartner       : 1;   // [18]
		uint32_t unchunkedExtendedMsgSupportedByPortPartner : 1; // [19]
		uint32_t emcaPdSpecRevision               : 2;   // [21:20] 00 - No EMCA detected; 01 - EMCA supports PD Rev 2.0; 10 - EMCA supports PD Rev 3.0
		uint32_t                                  : 10;
	} f;
} DRV_HPI_PD_STATUS;

typedef union {
	uint32_t u8Status;
	struct {
		uint8_t portPartnerConnectionStatus       : 1;
		uint8_t ccPolarity                        : 1;
		uint8_t typeDeviceAttached                : 3;
		uint8_t raStatus                          : 1;
		uint8_t typecCurrentLevel                 : 2;
	} f;
} DRV_HPI_TYPEC_STATUS;

/* HPIx007E, size 2 */
typedef union {
	uint16_t u16Response;
	uint8_t  u8Response[2];
	struct {
		uint16_t msgCode        : 7;
		uint16_t msgType        : 1;
		uint16_t msgLength      : 8;
	} f;
} DRV_HPI_DEV_RESPONSE;

typedef union {
	uint32_t u32Response;
	uint8_t  u8Response[4];
	struct {
		uint32_t msgCode        : 7;
		uint32_t msgType        : 1;
		uint32_t msgLength      : 24;
	} f;
} DRV_HPI_PORT_RESPONSE;

typedef union {
	uint16_t version[2];
	struct {
		uint16_t build;
		uint8_t  patch;
		uint8_t  minor : 4;
		uint8_t  major : 4;
	} f;
} DRV_HPI_BASE_VERSION;

typedef union {
	uint16_t version[2];
	struct {
		uint16_t appName;
		uint8_t  exCricuit;
		uint8_t  minor : 4;
		uint8_t  major : 4;
	} f;
} DRV_HPI_APP_VERSION;

typedef union {
	uint8_t version[8];
	struct {
		DRV_HPI_BASE_VERSION baseVer;
		DRV_HPI_APP_VERSION  appVer;
	} f;
} DRV_HPI_FW_VERSION;

typedef union {
	uint8_t sign[16];
	struct {
		uint32_t ecFw1;
		uint32_t ecFw2;
		uint32_t ccgxFw1;
		uint32_t ccgxFw2;
	} f;
} DRV_HPI_SPI_SIGNATURE;

typedef union {
	uint8_t sign[8];
	struct {
		uint16_t siliconId;
		uint32_t magic;
		uint16_t ending;
	} f;
} DRV_HPI_FW_SIGNATURE;

typedef union {
	uint8_t rowIdx[3];
	struct {
		uint8_t leading;
		uint16_t index;
	} f;
} DRV_HPI_FW_ROW_INDEX;

typedef union {
	uint8_t rowIdx[4];
	struct {
		uint16_t leading;
		uint16_t index;
	} f;
} DRV_HPI_FW_ROW_INDEX_CCG8;

typedef union {
	uint8_t thisRow[1 + 3 + 2 + 256 + 1 + 2];
	struct {
		uint8_t signature;
		DRV_HPI_FW_ROW_INDEX idx;
		uint16_t rowSize;
		uint8_t payload[256];
		uint8_t checkSum;
		uint16_t ending;
	} f;
} DRV_HPI_FW_ONE_ROW;

typedef union {
	uint8_t thisRow[1 + 3 + 2 + 128 + 1 + 2];
	struct {
		uint8_t signature;
		DRV_HPI_FW_ROW_INDEX idx;
		uint16_t rowSize;
		uint8_t payload[128];
		uint8_t checkSum;
		uint16_t ending;
	} f;
} DRV_HPI_FW_ONE_ROW_CCGx;

typedef union {
	uint8_t thisRow[4 + 256];
	struct {
		DRV_HPI_FW_ROW_INDEX_CCG8 idx;
		uint8_t payload[256];
	} f;
} DRV_HPI_FW_ONE_ROW_CCG8;

typedef enum {
	DRV_HPI_FW_UPDATE_INIT = 0,
	DRV_HPI_FW_UPDATE_HAD_SKIPPED,
	DRV_HPI_FW_UPDATE_ONE_ROW,
	DRV_HPI_FW_UPDATE_CHECKSUM_ERROR,
	DRV_HPI_FW_UPDATE_FAILED,
	DRV_HPI_FW_UPDATE_SUCCESS
} DRV_HPI_FW_UPDATE_INDICATOR;

#pragma pack(pop)

#if defined(__CC_ARM)
  #pragma pop
#elif defined(__ICCARM__)
  /* leave anonymous unions enabled */
#elif defined(__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined(__TMS470__)
  /* anonymous unions are enabled by default */
#elif defined(__TASKING__)
  #pragma warning restore
#else
#warning Not supported compiler type
#endif

extern volatile DRV_HPI_Rx0040_REG g_hpi_Regx0040[2];
extern DRV_HPI_PD_STATUS g_hpi_Regx1008[2]; /* two PD chips */
extern DRV_HPI_PD_STATUS g_hpi_Regx2008[2]; /* two PD chips */
extern DRV_HPI_TYPEC_STATUS  g_hpi_Regx100C[2]; /* two PD chips */
extern DRV_HPI_TYPEC_STATUS  g_hpi_Regx200C[2]; /* two PD chips */
extern DRV_HPI_Rx0000_DEVICE_MODE g_hpi_Regx0000[2]; /* two PD chips */

typedef void (*DRV_HPI_PROGRESS_INDICATOR) (DRV_HPI_FW_UPDATE_INDICATOR st);

/* hpi i2c register access */
int amd_crb_drivers_hpi_regAccess(bool isRead, uint16_t reg, void * pBuf, uint32_t len, uint8_t chipID);
/* ucsi i2c register access */
int amd_crb_drivers_hpi_ucsiRegAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t chipID);
/* hpi interface init */
int amd_crb_drivers_hpi_init (void);
/* get device mode app or bootloader */
uint8_t amd_crb_drivers_hpi_getDeviceMode (uint8_t chipID);
/* get port status */
uint32_t amd_crb_drivers_hpi_getPdStatus(uint8_t port, uint8_t chipID);
/* get typec status */
uint32_t amd_crb_drivers_hpi_getTypecStatus(uint8_t port, uint8_t chipID);
/* send row read and write command after buffer write and before buffer read */
uint8_t amd_crb_drivers_hpi_triggerRowReadWrite(bool isRead, DRV_HPI_FW_ROW_INDEX rIdx, uint8_t chipID);
/* send row read and write command after buffer write and before buffer read. specific for CCG8 */
uint8_t amd_crb_drivers_hpi_triggerRowReadWriteCcg8(bool isRead, DRV_HPI_FW_ROW_INDEX_CCG8 rIdx, uint8_t chipID);
/* get device response */
DRV_HPI_DEV_RESPONSE amd_crb_drivers_hpi_getDeviceResponse (uint8_t chipID);
/* get port response */
DRV_HPI_PORT_RESPONSE amd_crb_drivers_hpi_getPortResponse (uint8_t port, uint8_t chipID);
/* check the device response and point the handler */
bool amd_crb_drivers_hpi_DevEventMsgHandler (DRV_HPI_DEV_RESPONSE resp, uint8_t chipID);
/* check the ec command response and point the handler */
bool amd_crb_drivers_hpi_DevEcCmdHandler (DRV_HPI_DEV_RESPONSE resp, uint8_t chipID);
/* check the port event response and point the handler */
bool amd_crb_drivers_hpi_PortEventMsgHandler (DRV_HPI_PORT_RESPONSE resp, uint8_t port, uint8_t chipID);
/* check the ec command event response and point the handler */
bool amd_crb_drivers_hpi_PortEcCmdHandler (DRV_HPI_PORT_RESPONSE resp, uint8_t port, uint8_t chipID);
/* wait and clear the interrupt */
bool amd_crb_drivers_hpi_clearInterrupts (uint32_t ms, uint8_t chipID);
/* wait for the device response */
bool amd_crb_drivers_hpi_wait4DevResp (uint8_t respCode, uint32_t ms, uint8_t chipID);
/* wait for the several device response */
uint8_t amd_crb_drivers_hpi_wait4DevRespArray (uint8_t * pExpectedCodeArray, uint8_t num, uint32_t ms, uint8_t chipID);
/* wait for port event response */
bool amd_crb_drivers_hpi_wait4PortResp (uint8_t respCode, uint8_t port, uint32_t ms, uint8_t chipID);
/* wait for port event response and read data buffer */
bool amd_crb_drivers_hpi_wait4PortRespReturn (uint8_t respCode, uint8_t* data, uint8_t port, uint32_t ms, uint8_t chipID);
/* hpi interrupt trigger */
void amd_crb_drivers_hpi_intTrigger(uint8_t chipID);
/* hpi interrupt handler top */
void amd_crb_drivers_hpi_intTopHalf(uint8_t pinSt, uint8_t chipID);
/* Interrupt pending */
bool amd_crb_drivers_hpi_intPending(void);
/* check interrupt ack */
bool amd_crb_drivers_hpi_intAck (uint8_t mask, uint8_t chipID);
/* hpi interrupt handler bottom */
void amd_crb_drivers_hpi_intBottomHalf(uint8_t chipID);
/* read current PDO */
uint32_t amd_crb_drivers_hpi_curPdo (uint8_t port, uint8_t chipID);
/* update the PDO */
uint32_t amd_crb_drivers_hpi_updatePdo (uint8_t port, uint8_t chipID, bool pr_swap);
/* update the ac limit */
uint32_t amd_crb_drivers_hpi_updateAcLimit(uint8_t chipID);
/* get last ac limit */
uint32_t amd_crb_drivers_hpi_lastAcLimit(uint8_t chipID);
/* check the raw data checksum if it is valid */
bool amd_crb_drivers_hpi_rowDataCheck (DRV_HPI_FW_ONE_ROW * pRow);
/* check the raw data checksum if it is valid for ccgx */
bool amd_crb_drivers_hpi_rowDataCheckCcgx (DRV_HPI_FW_ONE_ROW_CCGx * pRow);
/* write data to pd payload data buffer */
bool amd_crb_drivers_hpi_writePayLoad2DataBuf(uint8_t * pBuf, uint32_t len, uint8_t chipID);
/* read Rdata buffer */
bool amd_crb_drivers_hpi_readDatafRDataBuf(uint8_t port, uint8_t *pBuf, uint32_t len, bool page, uint8_t chipID);
/* write Wdata buffer */
bool amd_crb_drivers_hpi_writeData2WDataBuf(uint8_t port, uint8_t *pBuf, uint32_t len, bool page, uint8_t chipID);
/* higher sink PDO */
pd_do_t amd_crb_drivers_hpi_getHigherPortSnkPdo(void);
/* get higher PDO port number */
uint8_t amd_crb_drivers_hpi_getHigherPortNumSnk(void);
/* reconfigure port role */
void amd_crb_drivers_hpi_reconfigPortRole(uint8_t port, uint8_t port_role, uint8_t chipID);
/* reconfigure typec Rp */
void amd_crb_drivers_hpi_reconfigTypecRp(uint8_t port, dpm_typec_cmd_t typec_cmd, uint8_t chipID);
/* set all port to sink only */
void amd_crb_drivers_hpi_allPortsSinkOnly(uint8_t chipID);
/* set all port to source only */
void amd_crb_drivers_hpi_allPortsSourceOnly(uint8_t chipID);
/* set all port to drp */
void amd_crb_drivers_hpi_allPortsDrp(uint8_t chipID);
/* get higher sink pdo */
pd_do_t amd_crb_drivers_hpi_getHigherPortSnkPdo(void);
/* get higher sink number */
uint8_t amd_crb_drivers_hpi_getHigherPortNumSnk(void);
/* command to enable the sink path */
void amd_crb_drivers_hpi_sinkPathEnable(uint8_t port);
/* pd i2c crossbar mailbox */
uint8_t amd_crb_drivers_hpi_crossbarMailBox(bool isRead, uint8_t port, uint8_t* data, uint8_t chipID);
/* send PR SWAP */
void amd_crb_drivers_hpi_prSwap(uint8_t port, uint8_t chipID);
/* disable port command */
void amd_crb_drivers_hpi_portDisable(uint8_t port, uint8_t chipID);
/* enable port */
void amd_crb_drivers_hpi_portEnable(uint8_t port, uint8_t chipID);
/* reset pd chip */
void amd_crb_drivers_hpi_resetChip(uint8_t chipID);
/* send data reset */
void amd_crb_drivers_hpi_dataReset(uint8_t port, uint8_t chipID);
/* enable tbt and usb4 port */
void amd_crb_drivers_hpi_usb4Tbt3Enable(uint8_t port, uint8_t chipID);
/* disable usb4 and tbt port */
void amd_crb_drivers_hpi_usb4Tbt3Disable(uint8_t port, uint8_t chipID);
/* enable usb4 and disable tbt */
void amd_crb_drivers_hpi_usb4EnableTbt3Disable(uint8_t port, uint8_t chipID);
/* enable tbt and disable usb4 */
void amd_crb_drivers_hpi_usb4DisableTbt3Enable(uint8_t port, uint8_t chipID);
/* read source pdo */
void amd_crb_drivers_hpi_readSrcPdo(uint8_t port, uint8_t chipID);
/* write source pdo */
void amd_crb_drivers_hpi_writeSrcPdo(uint8_t port, uint8_t chipID, bool lock);
/* recovery ports setting */
void amd_crb_drivers_hpi_recoveryPorts(void);
/* enable or disable the hpi EC sink path control */
void amd_crb_drivers_hpi_sinkPathCtrl(bool en);
/* report system power status to IFX PD */
void amd_crb_drivers_hpi_systemStatus(uint8_t chipID, enum system_power_state st);
/* change pd unconstrained power bit in sink cap */
void amd_crb_drivers_hpi_snkCapUnconstrainedPower(uint8_t port, uint8_t chipID, bool enable);
/* change pd unconstrained power bit in src cap */
void amd_crb_drivers_hpi_srcCapUnconstrainedPower(uint8_t port, uint8_t chipID, bool enable);
/* change the dr swap response with accept/reject/wait */
void amd_crb_drivers_hpi_drSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp);
/* change the pr swap response with accept/reject/wait */
void amd_crb_drivers_hpi_prSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp);
/* change the vconn swap response with accept/reject/wait */
void amd_crb_drivers_hpi_vconnSwapResponse(uint8_t port, uint8_t chipID, app_swap_resp_t resp);
#endif // end of AMD_CRB_DRIVERS_HPI_H_

