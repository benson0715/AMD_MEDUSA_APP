/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_REALTEK_PD_H_
#define AMD_CRB_DRIVERS_REALTEK_PD_H_

#include "amd_crb_drivers_pd.h"
#include "system.h"

#define RTKPD_IC_TYPE_RTS5453P          (1)
#define RTKPD_IC_TYPE_RTS5453P_VB       (2)
#define RTKPD_IC_TYPE_RTS5453P_28       (3)

/*
 * realtek PD controller registers (cmd + sub cmd) define
 * Reserve regs will not in this list
 * For example: 0x01DA means cmd 0x01 and sub cmd 0xDA
 * If it is 1 byte, we only have cmd. If it is 2 bytes, we have cmd and sub cmd.
 */
#define RTKPD_REG_VENDOR_CMD_EN                (0x01DA)
#define RTKPD_REG_TCPM_RST                     (0x0800)
#define RTKPD_REG_SET_NOTIFICATION_EN          (0x0801)
#define RTKPD_REG_SET_PDO                      (0x0803)
#define RTKPD_REG_SET_RDO                      (0x0804)
#define RTKPD_REG_AUTO_SET_RDO                 (0x0844)
#define RTKPD_REG_SET_TPC_RP                   (0x0805)
#define RTKPD_REG_SET_VDM                      (0x0819)
#define RTKPD_REG_SET_VDO                      (0x081A)
#define RTKPD_REG_SET_TPC_CSD_OP_MODE          (0x081D)
#define RTKPD_REG_SET_TPC_RECONNECT            (0x081F)
#define RTKPD_REG_SET_INIT_PD_AMS              (0x0820)
#define RTKPD_REG_FORCE_SET_POWER_SW           (0x0821)
#define RTKPD_REG_SET_TPC_DISCONNECT           (0x0823)
#define RTKPD_REG_ACK_POWER_CTRL_REQ           (0x0824)
#define RTKPD_REG_SET_BYPASS_SVID              (0x0826)
#define RTKPD_REG_SET_BBR_CTS                  (0x0827)
#define RTKPD_REG_SET_BC12                     (0x0828)
#define RTKPD_REG_SET_SYS_PWR_STATE            (0x082B)
#define RTKPD_REG_GET_PDO                      (0x0883)
#define RTKPD_REG_GET_RDO                      (0x0884)
#define RTKPD_REG_GET_TPC_RP                   (0x0885)
#define RTKPD_REG_GET_VDM                      (0x0899)
#define RTKPD_REG_GET_VDO                      (0x089A)
#define RTKPD_REG_GET_TPC_CSD_OP_MODE          (0x089D)
#define RTKPD_REG_GET_AMS_STATUS               (0x08A2)
#define RTKPD_REG_GET_CSD                      (0x08F0)
#define RTKPD_REG_GET_BYPASS_SVID              (0x08A6)
#define RTKPD_REG_GET_CURR_PARTNER_SRC_PDO     (0x08A7)
#define RTKPD_REG_GET_DATA_MESSAGE             (0x08A8)
#define RTKPD_REG_GET_PWR_SW_STATUS            (0x08A9)
#define RTKPD_REG_GET_EPR_PDO                  (0x08AA)
#define RTKPD_REG_CLEAR_DEAD_BAT_STATE         (0x0856)
#define RTKPD_REG_GET_IC_TYPE                  (0x08DF)
#define RTKPD_REG_GET_RTK_STATE                (0x0900)  /* sub-cmd field is used as offset: change from 0x00 to 0x12 */
#define RTKPD_REG_ACK_CC_CI                    (0x0A00)  /* sub-cmd field is reserved or zero */
/* UCSI related command */
#define RTKPD_REG_PPM_RSEST                    (0x0E01)
#define RTKPD_REG_CONNECTOR_RESET              (0x0E03)
#define RTKPD_REG_GET_CAPABILITY               (0x0E06)
#define RTKPD_REG_GET_CONN_CAPABILITY          (0x0E07)
#define RTKPD_REG_SET_UOR                      (0x0E09)
#define RTKPD_REG_SET_PDR                      (0x0E0B)
#define RTKPD_REG_GET_ALT_MODE                 (0x0E0C)
#define RTKPD_REG_GET_CONN_ALT_MODE_SUPP       (0x0E0D)
#define RTKPD_REG_GET_CURR_CONN_ALT_MODE       (0x0E0E)
#define RTKPD_REG_SET_NEW_CONN_ALT_MODE        (0x0E0F)
#define RTKPD_REG_UCSI_GET_PDO                 (0x0E10)
#define RTKPD_REG_GET_CABLE_PROPERTY           (0x0E11)
#define RTKPD_REG_UCSI_GET_CONN_STATUS         (0x0E12)
#define RTKPD_REG_UCSI_GET_ERROR_STATUS        (0x0E13)
#define RTKPD_REG_UCSI_READ_PWR_LEVEL          (0x0E1E)
#define RTKPD_REG_UCSI_SET_USB                 (0x0E21)
#define RTKPD_REG_UCSI_GET_LPM_PPM_INFO        (0x0E22)
#define RTKPD_REG_SET_FTS_STATUS               (0x1201)
#define RTKPD_REG_GET_FTS_STATUS               (0x1202)
#define RTKPD_REG_SET_RT_FW_UPDATE_MODE        (0x2000)
#define RTKPD_REG_GET_IC_STATUS                (0x3A00)  /* sub-cmd field is used as offset: change from 0x00 to 0x1E */

/* firmware update registers */
#define RTKPD_REG_SET_FLASH_64K                (0x04)
#define RTKPD_REG_SET_FLASH_128K               (0x06)
#define RTKPD_REG_SET_FLASH_192K               (0x13)
#define RTKPD_REG_SET_FLASH_256K               (0x14)
#define RTKPD_REG_SET_RESET_FLASH              (0x05)
#define RTKPD_REG_ISP_VALID                    (0x16)
#define RTKPD_REG_SET_SPI_PROTECTION           (0x07)
#define RTKPD_REG_GET_SPI_PROTECTION           (0x36)
#define RTKPD_REG_ERASE_FLASH                  (0x03)
#define RTKPD_REG_GET_FLASH_64K                (0x24)
#define RTKPD_REG_GET_FLASH_128K               (0x26)
#define RTKPD_REG_GET_FLASH_192K               (0x33)
#define RTKPD_REG_GET_FLASH_256K               (0x34)

/*
 * RTK PD controller registers length of byte
 */
#define RTKPD_REG_GET_PDO_LEN                  (21) /* 1 + 4*N (N is PDO number), assume there is five */
#define RTKPD_REG_GET_RDO_LEN                  (5)
#define RTKPD_REG_GET_TPC_RP_LEN               (2)
#define RTKPD_REG_GET_VDM_LEN                  (21) /* 1 + 4*N (N is VDM number), assume there is five */
#define RTKPD_REG_GET_VDO_LEN                  (21) /* 1 + 4*N (N is VDO number), assume there is five */
#define RTKPD_REG_GET_TPC_CSD_OP_MODE_LEN      (5)
#define RTKPD_REG_GET_AMS_STATUS_LEN           (3)
#define RTKPD_REG_GET_CSD_LEN                  (2)
#define RTKPD_REG_GET_BYPASS_SVID_LEN          (11) /* 1 + 2*N (N is SVID number), assume there is five */
#define RTKPD_REG_GET_CURR_PARTNER_SRC_PDO_LEN (5)
#define RTKPD_REG_GET_DATA_MESSAGE_LEN         (6)  /* 1 +  N (N is data number), assume there is five */
#define RTKPD_REG_GET_PWR_SW_STATUS_LEN        (1)
#define RTKPD_REG_GET_EPR_PDO_LEN              (21) /* 1 + 4*N (N is PDO number), assume there is five */
#define RTKPD_REG_GET_RTK_STATE_LEN            (19)
#define RTKPD_REG_GET_CAPABILITY_LEN           (17)
#define RTKPD_REG_GET_CONN_CAPABILITY_LEN      (3)
#define RTKPD_REG_SET_UOR_LEN                  (3)
#define RTKPD_REG_GET_ALT_MODE_LEN             (31) /* 1 + 6*N (N is SVID number), assume there is five */
#define RTKPD_REG_GET_CONN_ALT_MODE_SUPP_LEN   (2)
#define RTKPD_REG_GET_CURR_CONN_ALT_MODE_LEN   (2)
#define RTKPD_REG_UCSI_GET_PDO_LEN             (21) /* 1 + 4*N (N is PDO number), assume there is five */
#define RTKPD_REG_GET_CABLE_PROPERTY_LEN       (6)
#define RTKPD_REG_UCSI_GET_CONN_STATUS_LEN     (10)
#define RTKPD_REG_UCSI_GET_ERROR_STATUS_LEN    (2)
#define RTKPD_REG_UCSI_READ_PWR_LEVEL_LEN      (3)
#define RTKPD_REG_UCSI_SET_USB_LEN             (7)
#define RTKPD_REG_UCSI_GET_LPM_PPM_INFO_LEN    (21)
#define RTKPD_REG_GET_FTS_STATUS_LEN           (4)
#define RTKPD_REG_GET_IC_STATUS_LEN            (32)

#define RTKPD_ACPI_ENABLE_LEN                  (1)

#define RTK_REG_READ                           (1)
#define RTK_REG_WRITE                          (0)

#define RTK_REG_ACCESS_TRTRY_TIME              (1)   /* retry time for I2C comm */
#define RTK_PING_RETRY_TIME                    (200) /* ping status retry 200 times */
#define RTK_PING_RETRY_DELAY                   (10)  /* ping status delay 10ms */

#define RTK_ARA_ADDRESS                        (0x0C)

#define RTK_REG_READ                           (1)
#define RTK_REG_WRITE                          (0)

#pragma pack(push, 1)

/* Realtek PD command response status */
enum DRV_RTK_CMD_STS {
	RTK_STS_PROCESSING,
	RTK_STS_COMPLETE,
	RTK_STS_DEFFERED,
	RTK_STS_ERROR,
	RTK_STS_NONE
};

/* Realtek PD vdo types */
enum DRV_RTK_VDO_TYPE {
	Resv = 0,
	ID_HEADER_VDO,
	Cert_Stat_VDO,
	Product_VDO,
	Cable_VDO,
	Alternate_Mode_Adapter_VDO,
	SVID_Response_VDO1,
	SVID_Response_VDO2,
	SVID_Response_VDO3,
	SVID_Response_VDO4,
	SVID_Response_VDO5,
	SVID_Response_VDO6,
	PD_VDO_DP_CAPS,
	PD_VDO_DP_STATUS,
	PD_VDO_DP_CFG,
	Vendor_Defined
};

/* Realtek PD AMS command */
enum DRV_RTK_AMS_CMD {
	RTK_PR_SWAP = 1,
	RTK_DR_SWAP,
	RTK_VCONN_SWAP,
	RTK_SRC_CAP,
	RTK_REQUEST,
	RTK_SOFT_RST,
	RTK_HARD_RST,
	RTK_GOTOMIN,
	RTK_GET_SNK_CAP,
	RTK_GET_SRC_CAP,
	RTK_DATA_RST,
	RTK_ENTER_USB,
	RTK_CABLE_RST,
	RTK_CABLE_SOFT_RST,
	RTK_GET_SRC_CAP_EXTD,
	RTK_GET_STATUS,
	RTK_GET_BAT_CAP,
	RTK_GET_BAT_STATUS,
	RTK_GET_MF_INFO,
	RTK_SECURITY_REQ,
	RTK_GET_PPS_STATUS,
	RTK_GET_COUNTRY_CODE,
	RTK_GET_COUNTRY_INFO,
	RTK_GET_SNK_CAP_EXTD,
	RTK_EPR_MODE,
	RTK_ALERT,
	RTK_FW_UPDATE_REQ,
	RTK_BIST_CARRIER2,
	RTK_BIST_TEST_DATA
};

/* Realtek system power state */
enum DRV_RTK_SYS_PWR {
	RTK_SYS_S0 = 1,
	RTK_SYS_S3 = 2,  /* S3 or S0i3 */
	RTK_SYS_S5 = 3,  /* S4 and S5 are same to EC. So only send S5 */
	RTK_SYS_S4 = 4
};

/* Realtek power partner type */
enum DRV_RTK_PORT_PARTNER_TYPE {
	DFP_ATT = 1,
	UFP_ATT = 2,
	PWR_CABLE_UFP_ATT = 4,
	DBG_ACC_ATT = 5,
	AUDIO_ACC_ATT = 6
};

/*
 * tPingStatus-> 10ms: Master can wait this delay time for TCPM processing command
 * nRetryCnt-> 200 times: for commands need more time to process. Need to retry for ping status.
 * */
typedef union {
	uint8_t u8PingSt;
	struct {
		/* When get defer, it means controller get the command but need more time to process it. Need to poll
		 * the Ping Status again */
		uint8_t pingSt          : 2; /* 0: Not process/processing; 1: Complete; 2: Deferred; 3: Error  */
		uint8_t length          : 6;
	} f;
} DRV_RTK_PING_STATUS;

typedef union {
	uint8_t buf[4];
	struct {
		uint8_t cmd             : 8;
		uint8_t length          : 8;
		uint8_t sub_cmd         : 8;
		uint8_t port_number     : 8;
	} f;
} DRV_RTK_CMD_WRITE;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf;
		struct {
			/* 0: Resv | 1: default | 2: 1.5A | 3: 3A */
			uint8_t cur_rp          : 2; /* Current Rp exposed on CC */
			uint8_t tcp_rp          : 2; /* Rp when type-c not attached before pd contract */
			uint8_t pd_rp           : 2; /* Rp after contract */
			uint8_t Resv            : 2;
		} f;
	};
} DRV_RTK_OUTPUT_GET_TCP_RP;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[26];
		struct {
			uint8_t Origin          : 2; /* 0:SOP 1:SOP' 2:SOP'' */
			uint8_t Resv            : 6;
			uint32_t VDM_Header     : 32; /* VDM Header */
			uint32_t VDO1           : 32; /* VDO1 */
			uint32_t VDO2           : 32; /* VDO2 */
			uint32_t VDO3           : 32; /* VDO3 */
			uint32_t VDO4           : 32; /* VDO4 */
			uint32_t VDO5           : 32; /* VDO5 */
		} f;
	};
} DRV_RTK_OUTPUT_GET_VDM;

typedef union {
	uint8_t buf[7];
	struct {
		uint8_t NumOfVdos       : 3; /* number of the vdo */
		uint8_t Origin          : 1; /* 0:Port_VDO 1:Partner_VDO */
		uint8_t Resv            : 4;
		uint8_t VDO1_Type       : 8; /* VDO1 */
		uint8_t VDO2_Type       : 8; /* VDO2 */
		uint8_t VDO3_Type       : 8; /* VDO3 */
		uint8_t VDO4_Type       : 8; /* VDO4 */
		uint8_t VDO5_Type       : 8; /* VDO3 */
		uint8_t VDO6_Type       : 8; /* VDO4 */
	} f;
} DRV_RTK_INPUT_GET_VDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[20];
		struct {
			uint32_t VDO1           : 32; /* VDO1 */
			uint32_t VDO2           : 32; /* VDO2 */
			uint32_t VDO3           : 32; /* VDO3 */
			uint32_t VDO4           : 32; /* VDO4 */
			uint32_t VDO5           : 32; /* VDO5 */
		} f;
	};
} DRV_RTK_OUTPUT_GET_VDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf;
		struct {
			uint8_t TPC_MODE           : 2; /* 0:Snk 1:DRP 2:Src */
			uint8_t ACC_SUPP           : 1; /* Accessory support. 0:not 1:yes */
			uint8_t Drp_MODE           : 2; /* Drp Mode. 0:drp_normal 1:try.src 2:try.snk */
			uint8_t Resv               : 3;
		} f;
	};
} DRV_RTK_OUTPUT_GET_TCP_CS_OP_MODE;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[2];
		struct {
			uint8_t AMS                : 4; /* Indicate the previous AMS */
			uint8_t AMS_Status         : 1; /* 0:AMS_Complete 1:AMD_InProcess */
			uint8_t Resv               : 3;
			uint8_t AMS_Result         : 8; /* 9:success others:fail */
		} f;
	};
} DRV_RTK_OUTPUT_GET_AMS_STATUS;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf;
		struct {
			uint8_t CSD                : 8; /* type-c state diagram */
		} f;
	};
} DRV_RTK_OUTPUT_GET_CSD;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[4];
		struct {
			uint32_t PDO1              : 32; /* current request pdo from port partner */
		} f;
	};
} DRV_RTK_OUTPUT_GET_CURR_PARTNER_SRC_PDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf;
		struct {
			uint8_t VBIN_ENABLE        : 2; /* 0:VBSIN_OFF 1:VBSIN_DIODE 3:VBSIN_ON */
			uint8_t Resv               : 5;
			uint8_t DeadBatteryStatus  : 1; /* 0:Power_On_From_Dead_Battery 1:Not_Power_On_From_Dead_Battery */
		} f;
	};
} DRV_RTK_OUTPUT_GET_PWR_SW_STATUS;

typedef union {
	uint8_t buf;
	struct {
		uint8_t PDO_Type        : 1; /* 0:Sink 1:Source */
		uint8_t PDO             : 1; /* 0:TCPM_PDO 1:TCPM_Port_Partner_PDO */
		uint8_t PDO_Offset      : 3; /* offset of the PDO */
		uint8_t MumOfPdos       : 3; /* Number of PDOs to return starting from the PDO offset */
	} f;
} DRV_RTK_INPUT_GET_EPR_PDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[20];
		struct {
			uint32_t PDO1           : 32; /* PDO1 */
			uint32_t PDO2           : 32; /* PDO2 */
			uint32_t PDO3           : 32; /* PDO3 */
			uint32_t PDO4           : 32; /* PDO4 */
			uint32_t PDO5           : 32; /* PDO5 */
		} f;
	};
} DRV_RTK_OUTPUT_GET_EPR_PDO;

typedef union {
	uint8_t buf[20];
	struct {
		uint32_t PDO1           : 32; /* PDO1 */
		uint32_t PDO2           : 32; /* PDO2 */
		uint32_t PDO3           : 32; /* PDO3 */
		uint32_t PDO4           : 32; /* PDO4 */
		uint32_t PDO5           : 32; /* PDO5 */
	} f;
}DRV_RTK_INPUT_V_VMD_EN;

typedef union {
	uint8_t buf;
	struct {
		uint8_t Param           : 8; /* 0:nothing 1:clear_dead_battery */
	} f;
} DRV_RTK_INPUT_CLEAR_DB_STATE;

typedef union {
	uint8_t buf;
	struct {
		uint8_t RstType         : 2; /* 0:Soft_Rst 1:Hard_Rst */
		uint8_t Resv            : 6;
	} f;
} DRV_RTK_INPUT_RCPM_RST;

typedef union {
	uint32_t buf32;
	uint8_t buf[4];
	struct {
		uint32_t cmdCmplt                 : 1; /* command complete */
		uint32_t externalSupplyChange     : 1; /* external supply change */
		uint32_t pwrOpModeChange          : 1; /* power operation mode change */
		uint32_t Resv0                    : 1;
		uint32_t Resv1                    : 1;
		uint32_t suppProvdierCapChange    : 1; /* supported provider capabilities change */
		uint32_t negPwrLevelChange        : 1; /* negotiated power level change */
		uint32_t pdRstCmplt               : 1; /* PD reset complete */
		uint32_t suppCamChange            : 1; /* supported CAM change */\
		uint32_t battChargingStChange     : 1; /* battery charging status change */
		uint32_t Resv2                    : 1;
		uint32_t portPartnerChanged       : 1; /* port partner changed */
		uint32_t pwrDirchanged            : 1; /* power direction changed */
		uint32_t Resv3                    : 1;
		uint32_t connectChange            : 1; /* connect change */
		uint32_t error                    : 1; /* error */
		uint32_t na1                      : 1;
		uint32_t na2                      : 1;
		uint32_t na3                      : 1;
		uint32_t na4                      : 1;
		uint32_t alternateFLowChange      : 1; /* alternate flow change */
		uint32_t dpStatusChange           : 1; /* dp status change */
		uint32_t dfpOcpChange             : 1; /* dfp ocp change */
		uint32_t portOpModeChange         : 1; /* port operation mode change */
		uint32_t pwrCtrlRequest           : 1; /* power control request */
		uint32_t vdmRecv                  : 1; /* vdm received */
		uint32_t srcSnkCapRecv            : 1; /* source sink cap received */
		uint32_t dataMsgRecv              : 1; /* data message received */
		uint32_t Resv4                    : 1;
		uint32_t na5                      : 1;
		uint32_t Resv5                    : 1;
		uint32_t na6                      : 1;
	} f;
}DRV_RTK_INPUT_SET_NOTIFY;

typedef union {
	uint8_t buf[21];
	struct {
		uint8_t sprPdoNum                 : 3; /* spr pdo number 1~7 */
		uint8_t pdoTyle                   : 1; /* 0:Sink_PDO 1:Src_PDO */
		uint8_t eprPdoNum                 : 3; /* epr pdo number 1~7 */
		uint8_t Resv                      : 1;
		uint32_t PDO1                     : 32; /* PDO1 */
		uint32_t PDO2                     : 32; /* PDO2 */
		uint32_t PDO3                     : 32; /* PDO3 */
		uint32_t PDO4                     : 32; /* PDO4 */
		uint32_t PDO5                     : 32; /* PDO5 */
	} f;
}DRV_RTK_INPUT_SET_PDO;

typedef union {
	uint8_t buf[4];
	struct {
		uint32_t RDO                     : 32; /* PDO */
	} f;
}DRV_RTK_INPUT_SET_RDO;

typedef union {
	uint8_t buf;
	struct {
		uint8_t Resv                     : 2;
		uint8_t tpcRp                    : 2; /* 0:Resv 1:Default 2:1.5A 3:3A */
		uint8_t pdRp                     : 2; /* 0:Resv 1:Default 2:1.5A 3:3A */
		uint8_t Resv1                    : 2;
	} f;
}DRV_RTK_INPUT_SET_TPC_RP;

typedef union {
	uint8_t buf[24];
	struct {
		uint32_t VDM_Header              : 32;
		uint32_t VDO1                    : 32;
		uint32_t VDO2                    : 32;
		uint32_t VDO3                    : 32;
		uint32_t VDO4                    : 32;
		uint32_t VDO5                    : 32;
	} f;
}DRV_RTK_INPUT_SET_VDM;

typedef union {
	uint8_t buf[26];
	struct {
		uint8_t VDO_Num                  : 3; /* vdo number */
		uint8_t VDO_Origin               : 1; /* 0:Original VDO 1:Port Partner */
		uint8_t Resv                     : 4;
		uint8_t VDO1Type                 : 8;
		uint32_t VDO1                    : 32;
		uint8_t VDO2Type                 : 8;
		uint32_t VDO2                    : 32;
		uint8_t VDO3Type                 : 8;
		uint32_t VDO3                    : 32;
		uint8_t VDO4Type                 : 8;
		uint32_t VDO4                    : 32;
		uint8_t VDO5Type                 : 8;
		uint32_t VDO5                    : 32;
	} f;
}DRV_RTK_INPUT_SET_VDO;

typedef union {
	uint8_t buf;
	struct {
		uint8_t csdMode                  : 2; /* 0:sink 1:drp 2:source */
		uint8_t accessory                : 1; /* 0:not_support 1:support */
		uint8_t drpMode                  : 2; /* 0:normal 1:try.src 2:try.snk */
		uint8_t Resv                     : 3;
	} f;
}DRV_RTK_INPUT_SET_OP_MODE;

typedef union {
	uint8_t buf;
	struct {
		uint8_t VBSIN_EN                 : 2; /* 0:VBSIN_EN_OFF 1:VBSIN_EN_Diodo 2:VBSIN_EN_ON */
		uint8_t LP_EN                    : 2; /* 0:LP_EN_OFF 1:LP_EN_Diodo 2:LP_EN_ON */
		uint8_t Resv                     : 2;
		uint8_t VBSIN_EN_CTRL            : 1; /* 0:TCPM ignore field[1:0] 1:TCPM set VBSIN_EN as [1:0] */
		uint8_t LP_EN_CTRL               : 1; /* 0:TCPM ignore field[3:2] 1:TCPM set VBSIN_EN as [3:2] */
	} f;
}DRV_RTK_INPUT_SET_PWR_SW;

typedef union {
	uint8_t buf[2];
	struct {
		uint8_t PD_AMS                   : 8; /* refer: DRV_RTK_AMS_CMD */
		uint8_t ACK                      : 2; /* 0:invalid 1:do_no_operation 2:executed_ok 3:executed_fail */
		uint8_t Resv                     : 6;
	} f;
}DRV_RTK_INPUT_SET_ACK_PWR_CTRL_REQ;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[18];
		struct {
			uint32_t PD_AMS                  : 32; /* DRV_RTK_INPUT_SET_NOTIFY */

			uint8_t supply                   : 1; /* 0:no supply 1:supply present */
			uint8_t portOpMode               : 3; /* 0:no consumer
													 1:USBC default
													 2:BC
													 3:PD
													 4:USBC 1.5A
													 5:USBC 3A
													 6-7:Resv */
			uint8_t insertionDetect          : 1; /* 0:no cable 1:cable detected */
			uint8_t pdCapableCable           : 1; /* 0:pd capable cable not detect 1:pd capable cable detected */
			uint8_t pwrDirection             : 1; /* 0:consumer 1:provider */
			uint8_t connectStatus            : 1; /* 0:unattached 1:attached */

			uint8_t portPartnerFlags         : 8; /* 0: USB 1:AltMode 2:TBT 3:USB4 4-7:Resv */
			uint32_t reqDataOb               : 32; /* only valid when connect_status 1 and PwrOpMode is PD.
													Return current PD negotiated level */
			uint8_t portPartnerType          : 3; /* 0:Resv
													 1:DFP attached
													 2:UFP attached
													 3:Resv
													 4:Powered cable/UFP attached
													 5:Debug Accessory attached
													 6:Audio Adapter Accessory attached
													 7:Resv */
			uint8_t batChargingStatus         : 2;
			uint8_t pdSourcingVconn           : 1; /* 0:no VCONN 1:source Vconn */
			uint8_t pdRespVconn               : 1; /* port is responsible to source Vconn */
			uint8_t pdAmsInProcess            : 1; /* 0:PE_xx_Ready 1:PD AMS in processing */

			uint8_t lastOrCurPdAms            : 4; /* 1:Power role swap
													  2:Data role swap
													  3:Vconn swap
													  4:Power negotiation
													  5:Resv
													  6:Soft reset
													  7:Hard reset
													  8:GotoMin
													  9:Get sink Cap
													  10:Get source cap
													  11:VDM
													  Others:Resv */
			uint8_t portPartnerNotSuppPd      : 1; /* 0:Port Partner may support PD
													  1:Port Partner may not support PD */
			uint8_t plugDirection             : 1; /* 0:Reverse 1:Obverse */
			uint8_t dpRole                    : 1; /* 0:DP sink 1:DP source */
			uint8_t pdConnected               : 1; /* 0:PD has not exchanged pd message and goodcrc
													  1:PD has exchagned pd message and goodcrc */

			uint8_t VBSIN_EN                  : 2; /* 0:off 1:Diode 3:on */
			uint8_t LP_EN                     : 2; /* 0:off 1:Diode 3:on */
			uint8_t cableVer                  : 2; /* 0:pd version1.0 1:pd version2.0 2:pd version3.0 */
			uint8_t portPartnerVer            : 2; /* 0:pd version1.0 1:pd version2.0 2:pd version3.0 */

			uint8_t altModeStatus             : 3; /* 0:Discover Identity Not
													  1:Discover Identity Done
													  2:Discover SVID Done
													  3:Discover Mode Done
													  4:Enter mode Done
													  5:DP Status Update Command Done
													  6:DP Configure Command Done */
			uint8_t dpLaneSwapSetting         : 1; /* 0:not Swap 1:lane Swap */
			uint8_t contractValid             : 1; /* 0:contract invalid 1:contract valid */
			uint8_t uncheckedExtMsgSupp       : 1; /* 0:unchunked support 1:only support chunked msg */
			uint8_t frSwapSupp                : 1; /* 0:fr_swap not support 1:fr_swap support */
			uint8_t Resv                      : 1;

			uint8_t averageCurLow             : 8;
			uint8_t averageCurHigh            : 8;
			uint8_t voltageReadLow            : 8;
			uint8_t voltageReadHigh           : 8;

		} f;
	};
}DRV_RTK_OUTPUT_GET_RTK_STATUS;

typedef union {
	uint8_t buf[5];
	struct {
		uint32_t Data                    : 8; /* Change the indication Mask to clear it until PPM set_ack_cci */
		uint8_t cmdCmpltAck              : 1; /* set to one to ack that a cmd complete */
		uint8_t Resv                     : 7;
	} f;
}DRV_RTK_INPUT_ACK_CC_CI;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[32];
		struct {
			uint8_t romFlash        : 8;  /* 0:RomCode 1:FlashCode */
			uint16_t Resv           : 16; /* Not Care */

			uint32_t fwMainVer      : 8;  /* firmware main version */
			uint32_t fwSubVer1      : 8;  /* firmware sub version1 */
			uint32_t fwSubVer2      : 8;  /* firmware sub version2 */
			uint32_t Resv1          : 8;  /* Not Care */

			uint8_t Resv2           : 8;  /* Not Care */
			uint8_t ready           : 1;  /* 0:not ready 1:ready */
			uint8_t Resv3           : 2;  /* Not Care */
			uint8_t connected       : 1;  /* 0:not connected 1:connected */
			uint8_t Resv4           : 4;  /* Not Care */

			uint32_t VID            : 16; /* VID L:H */
			uint32_t PID            : 16; /* PID L:H */

			uint8_t Resv5           : 8;  /* Not Care */
			uint8_t FlashBank       : 8;  /* Running Flash Bank Offset */
			uint8_t Resv6           : 8;  /* Not Care */
			uint16_t Resv7          : 16; /* Not Care */
			uint32_t Resv8          : 32; /* Not Care */

			uint32_t pdMajorRev     : 8;  /* pd major revision */
			uint32_t pdMinorRev     : 8;  /* pd minor revision */
			uint32_t pdMajorVer     : 8;  /* pd major version */
			uint32_t pdMinorVer     : 8;  /* pd minor version */

			uint16_t Resv9          : 16; /* Not Care */
			uint32_t Resv10         : 32; /* Not Care */
		} f;
	};
} DRV_RTK_OUTPUT_GET_IC_STATUS;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[16];
		struct {
			uint32_t bmAttributes        : 32; /* bitmap of PPM feature */
			uint32_t bNumConnectors      : 8;  /* The number of Connectors that PPM support. Zero is illegal */
			uint32_t bmOptionalFeatures  : 24; /* bitmap of PPM feature */
			uint32_t bNUmAltModes        : 8;  /* this field indicates the number of alt mode supported */
			uint32_t Rsev                : 8;  /* Reserve */
			uint32_t bcdBCVersion        : 16; /* Battery Charging Spec Release Number supported */
			uint32_t bcdPDVersion        : 16; /* USB Power Delivery Spec Release Number supported */
			uint32_t bcdUSBTypeCVersion  : 16; /* USB Type-C Spec Release Number supported */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CAPABILITY;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[4];
		struct {
			uint32_t opMode        : 8; /* port operational mode */
			uint32_t provider      : 1; /* This bit shall be set to one if the connector is capable of providing power on this connector. */
			uint32_t consumer      : 1; /* This bit shall be set to one if the connector is capable of consuming power on this connector. */
			uint32_t swapToDfp     : 1; /* This bit shall be set to one if the connector is capable of accepting swap to DFP. */
			uint32_t swapToUfp     : 1; /* This bit shall be set to one if the connector is capable of accepting swap to UFP. */
			uint32_t swapToSrc     : 1; /* This bit shall be set to one if the connector is capable of accepting swap to SRC. */
			uint32_t swapToSnk     : 1; /* This bit shall be set to one if the connector is capable of accepting swap to SNK. */
			uint32_t extOpMode     : 8; /* extend operational mode */
			uint32_t miscCap       : 4; /* Miscellaneous Capabilities */
			uint32_t reverseCurPro : 1; /* reverse current protection support */
			uint32_t partnerPdVer  : 2; /* partner pd revision */
			uint32_t Resv          : 3; /* Reserve */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CONN_CAPABILITY;

typedef struct {
	union {
		uint8_t buf[3];
		struct {
			uint8_t Recipient     : 3; /* TCPM/SOP/SOP'/SOP'' */
			uint8_t Resv          : 5; /* Reserve */
			uint8_t altModeOffset : 8; /* Alt Mode offset */
			uint8_t numOfAltMode  : 2; /* Number of alt mode */
			uint8_t Resv1         : 6; /* Reserve */
		} f;
	};
}DRV_RTK_INPUT_GET_ATL_MODE;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[12];
		struct {
			uint16_t svid0               : 16; /* standard or vendor id0 */
			uint32_t mode0               : 32; /* mode id for associated with the above svid */
			uint16_t svid1               : 16; /* standard or vendor id1 */
			uint32_t mode1               : 32; /* mode id for associated with the above svid */
		} f;
	};
}DRV_RTK_OUTPUT_GET_ATL_MODE;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[1];
		struct {
			uint8_t bmAltModeSupp         : 8; /* If alt mode is supported then the bit shall be set to one */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CUR_ATL_MODE_SUPP;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[1];
		struct {
			uint8_t curAltMode            : 8; /* offset into the list of alt mode supported that connector is currently operating in */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CUR_CONN_ATL_MODE;

typedef struct {
	union {
		uint8_t buf[6];
		struct {
			uint8_t enterOrExit            : 1;  /* set one if the PPM wants to enter this alt mode, set zero to exit */
			uint8_t Resv                   : 7;  /* Reserve */
			uint8_t NewCam                 : 8;  /* offset into the list of alt mode that PPM wants to operate */
			uint32_t AMSpecific            : 32; /* standard or vendor specific alt mode */
		} f;
	};
}DRV_RTK_INPUT_SET_NEW_CAM;

typedef struct {
	union {
		uint8_t buf[1];
		struct {
			uint8_t pdoType                : 1; /*  */
			uint8_t pdo                    : 1; /* Reserve */
			uint8_t pdoOffset              : 3; /* offset into the list of alt mode that PPM wants to operate */
			uint32_t numOfPdo              : 3; /* standard or vendor specific alt mode */
		} f;
	};
}DRV_RTK_INPUT_GET_PDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[20];
		struct {
			uint32_t pdo1            : 32; /* pdo1 */
			uint32_t pdo2            : 32; /* pdo2 */
			uint32_t pdo3            : 32; /* pdo3 */
			uint32_t pdo4            : 32; /* pdo4 */
			uint32_t pdo5            : 32; /* pdo5 */
		} f;
	};
}DRV_RTK_OUTPUT_GET_PDO;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[5];
		struct {
			uint32_t bmSpeedSupp     : 16; /* 0: bit per second; 1: kb/s; 2: Mb/s; 3 Bb/s */
			uint32_t bCurrentCap     : 8;  /* amount of current cable is designed for in 50ma units */
			uint32_t vbusInCable     : 1;  /* set to one if cable has a VBUS connection from end to end */
			uint32_t cableType       : 1;  /* set to one if it is active cable */
			uint32_t Directionality  : 1;  /* set to one of the lane directinality is configurable */
			uint32_t plugEndType     : 2;  /* 0:USBA / 1:USBB / 2:USBC / 3:other */
			uint32_t modeSupp        : 1;  /* cable support the alt mode */
			uint32_t Resv            : 2;  /* Reserve */
			uint8_t latency          : 4;  /* latency info */
			uint8_t Resv1            : 4;  /* Reserve */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CABLE_PROPERTY;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[9];
		struct {
			uint32_t pdStatusChange  : 16; /* port status change */
			uint32_t portOpMode      : 3;  /* port operation mode */
			uint32_t connectStatus   : 1;  /* set to one for attached */
			uint32_t pwrDir          : 1;  /* set to one as provider */
			uint32_t portPartnerF    : 8;  /* 0:USB / 1:AltMode */
			uint32_t portPartnerT    : 3;  /* port partner device type */
			uint32_t requestDataObj  : 32; /* request data object */
			uint8_t batChargCapSt    : 2;  /* Battery charging capability status */
			uint8_t pCapLimitReason  : 4;  /* provider capability limited reason */
			uint8_t Resv             : 2;  /* Reserve */
		} f;
	};
}DRV_RTK_OUTPUT_GET_CONN_STATUS;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[1];
		struct {
			uint8_t errorInfo        : 7; /* error information */
			uint8_t Resv             : 1;  /* Reserve */
		} f;
	};
}DRV_RTK_OUTPUT_GET_ERROR_STATUS;

typedef struct {
    uint8_t u8Length;
	union {
		uint8_t buf[1];
		struct {
			uint8_t icType        : 8; /* IC Type 1:RTS5453P 2:RTS5453P-VB 3: RTS5453P-28 */
		} f;
	};
}DRV_RTK_OUTPUT_GET_IC_TYPE;

#pragma pack(pop)

/* read the ara address for interrupt handler */
int amd_crb_drivers_rtk_araAccess(uint8_t* pBuf, uint8_t chipID);
/* Access I2C regs in realtek PD controller */
int amd_crb_drivers_rtk_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t port);
/* common get realtek register */
int amd_crb_drivers_rtk_getRegisters(uint8_t port, uint16_t cmd, void* pDataIn, uint8_t dataInLen, void* pDataOut, uint8_t dataOutLen);
/* get the ping status */
enum DRV_RTK_CMD_STS amd_crb_drivers_rtk_getPingStatus(uint8_t port, uint8_t* length);
/* wait ping status ready */
int amd_crb_drivers_rtk_waitPingStatusDone (uint8_t port, uint8_t* length);
/* set the realtek pd registers */
int amd_crb_drivers_rtk_setRegisters(uint8_t port, uint16_t cmd, void* pDataIn, uint8_t dataInLen);

/* read the pdo */
int amd_crb_drivers_rtk_readPdo(uint8_t port, DRV_RTK_INPUT_GET_PDO DataIn, DRV_RTK_OUTPUT_GET_PDO* pDataOut);
/* read sink rdo */
uint32_t amd_crb_drivers_rtk_readSnkRdo(uint8_t port);
/* read the tcp rp */
int amd_crb_drivers_rtk_readTcpRp(uint8_t port, DRV_RTK_OUTPUT_GET_TCP_RP* pDataOut);
/* read the vdm */
int amd_crb_drivers_rtk_readVdm(uint8_t port, DRV_RTK_OUTPUT_GET_VDM* pDataOut);
/* read the vdo */
int amd_crb_drivers_rtk_readVdo(uint8_t port, DRV_RTK_INPUT_GET_VDO DataIn, DRV_RTK_OUTPUT_GET_VDO* pDataOut);
/* read the tpc csd operation mode */
int amd_crb_drivers_rtk_readTpcCsdOpMode(uint8_t port, DRV_RTK_OUTPUT_GET_TCP_CS_OP_MODE* pDataOut);
/* read the ams status */
int amd_crb_drivers_rtk_readAmsStatus(uint8_t port, DRV_RTK_OUTPUT_GET_AMS_STATUS* pDataOut);
/* read the csd */
int amd_crb_drivers_rtk_readCsd(uint8_t port, DRV_RTK_OUTPUT_GET_CSD* pDataOut);
/* read the current partner pdo */
int amd_crb_drivers_rtk_readCurPartnerSrcPdo(uint8_t port, DRV_RTK_OUTPUT_GET_CURR_PARTNER_SRC_PDO* pDataOut);
/* read the power switch status */
int amd_crb_drivers_rtk_readPwrSwState(uint8_t port, DRV_RTK_OUTPUT_GET_PWR_SW_STATUS* pDataOut);
/* read the EPR PDO */
int amd_crb_drivers_rtk_readEprPdo(uint8_t port, DRV_RTK_INPUT_GET_EPR_PDO DataIn, DRV_RTK_OUTPUT_GET_EPR_PDO* pDataOut);
/* read the IC Status */
int amd_crb_drivers_rtk_readIcStatus(uint8_t port, uint8_t StatusLen, DRV_RTK_OUTPUT_GET_IC_STATUS* pDataOut);

/* vendor command enable/disable */
int amd_crb_drivers_rtk_writeVendorCmdEn(uint8_t port, bool en, bool flashAccessEn);
/* reset the tcpm */
int amd_crb_drivers_rtk_writeTcpmRst(uint8_t port, DRV_RTK_INPUT_RCPM_RST DataIn);
/* set the pd notification */
int amd_crb_drivers_rtk_writeNotifyEn(uint8_t port, DRV_RTK_INPUT_SET_NOTIFY DataIn);
/* set the pdo */
int amd_crb_drivers_rtk_writePdo(uint8_t port, DRV_RTK_INPUT_SET_PDO DataIn);
/* set the rdo */
int amd_crb_drivers_rtk_writeRdo(uint8_t port, DRV_RTK_INPUT_SET_RDO DataIn);
/* set the auto rdo */
int amd_crb_drivers_rtk_writeAutoSetRdo(uint8_t port, bool en);
/* set the TPC RP */
int amd_crb_drivers_rtk_writeTpcRp(uint8_t port, DRV_RTK_INPUT_SET_TPC_RP DataIn);
/* set the VDM */
int amd_crb_drivers_rtk_writeVdm(uint8_t port, DRV_RTK_INPUT_SET_VDM DataIn);
/* set the VDO */
int amd_crb_drivers_rtk_writeVdo(uint8_t port, DRV_RTK_INPUT_SET_VDO DataIn);
/* set the TPC CSD operation mode */
int amd_crb_drivers_rtk_writeTpcCsdOpMode(uint8_t port, DRV_RTK_INPUT_SET_OP_MODE DataIn);
/* trigger the reconnect */
int amd_crb_drivers_rtk_writeReconnect(uint8_t port);
/* initiate the PD AMS command */
int amd_crb_drivers_rtk_writeInitPdAms(uint8_t port, enum DRV_RTK_AMS_CMD cmd);
/* force the power switch */
int amd_crb_drivers_rtk_writeForcePwrSw(uint8_t port, DRV_RTK_INPUT_SET_PWR_SW DataIn);
/* trigger the disconnect */
int amd_crb_drivers_rtk_writeDisconnect(uint8_t port);
/* ack the power control request */
int amd_crb_drivers_rtk_writeAckPwrCtrlReq(uint8_t port, DRV_RTK_INPUT_SET_ACK_PWR_CTRL_REQ DataIn);
/* send the system power state to PD */
int amd_crb_drivers_rtk_writeSysPwrState(uint8_t port, enum DRV_RTK_SYS_PWR DataIn);
/* clear the dead battery */
int amd_crb_drivers_rtk_writeClrDeadBatState(uint8_t port, DRV_RTK_INPUT_CLEAR_DB_STATE DataIn);

/* flash related */
int amd_crb_drivers_rtk_writeFlashProtect(uint8_t port, bool en);
int amd_crb_drivers_rtk_writeFlash(uint8_t port, uint32_t address, uint8_t* pDataIn, uint8_t DataInLen);
int amd_crb_drivers_rtk_resetFlash(uint8_t port);
int amd_crb_drivers_rtk_ispValid(uint8_t port);
int amd_crb_drivers_rtk_eraseFlash(uint8_t port);
int amd_crb_drivers_rtk_readFlash(uint8_t port, uint16_t address, uint8_t* pDataOut, uint8_t DataOutLen);
int amd_crb_drivers_rtk_readIcType(uint8_t port, DRV_RTK_OUTPUT_GET_IC_TYPE* pDataOut);

/* ucsi related */
int amd_crb_drivers_rtk_ucsiGetStatus(uint8_t port, DRV_RTK_OUTPUT_GET_RTK_STATUS* pDataOut);
int amd_crb_drivers_rtk_ucsiAckCcCi(uint8_t port,  DRV_RTK_INPUT_ACK_CC_CI DataIn);
int amd_crb_drivers_rtk_ucsiPpmRst(uint8_t port);
int amd_crb_drivers_rtk_ucsiSetNotificationEnable(uint8_t port, DRV_RTK_INPUT_SET_NOTIFY* eventMask);
int amd_crb_drivers_rtk_ucsiConnRst(uint8_t port);
int amd_crb_drivers_rtk_ucsiGetCapability(uint8_t port, DRV_RTK_OUTPUT_GET_CAPABILITY* pDataOut);
int amd_crb_drivers_rtk_ucsiGetConnectorCapability(uint8_t port, DRV_RTK_OUTPUT_GET_CONN_CAPABILITY* pDataOut);
int amd_crb_drivers_rtk_ucsiSetUor(uint8_t port, uint8_t portRole);
int amd_crb_drivers_rtk_ucsiSetPdr(uint8_t port, uint8_t portType);
int amd_crb_drivers_rtk_ucsiGetAltMode(uint8_t port, DRV_RTK_INPUT_GET_ATL_MODE* pDataIn, DRV_RTK_OUTPUT_GET_ATL_MODE* pDataOut);
int amd_crb_drivers_rtk_ucsiGetCamSupp(uint8_t port, DRV_RTK_OUTPUT_GET_CUR_ATL_MODE_SUPP* pDataOut);
int amd_crb_drivers_rtk_ucsiGetCurConnAltMode(uint8_t port, DRV_RTK_OUTPUT_GET_CUR_CONN_ATL_MODE* pDataOut);
int amd_crb_drivers_rtk_ucsiSetNewCam(uint8_t port, DRV_RTK_INPUT_SET_NEW_CAM* pDataIn);
int amd_crb_drivers_rtk_ucsiGetPdo(uint8_t port, DRV_RTK_INPUT_GET_PDO* pDataIn, DRV_RTK_OUTPUT_GET_PDO* pDataOut);
int amd_crb_drivers_rtk_ucsiGetCableProperty(uint8_t port, DRV_RTK_OUTPUT_GET_CABLE_PROPERTY* pDataOut);
int amd_crb_drivers_rtk_ucsiGetConnStatus(uint8_t port, DRV_RTK_OUTPUT_GET_CONN_STATUS* pDataOut);
int amd_crb_drivers_rtk_ucsiGetErrorStatus(uint8_t port, DRV_RTK_OUTPUT_GET_ERROR_STATUS* pDataOut);

#endif // end of AMD_CRB_DRIVERS_REALTEK_PD_H_

