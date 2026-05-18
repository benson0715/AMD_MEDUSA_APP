/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_PD_H_
#define AMD_CRB_DRIVERS_PD_H_

#include <zephyr/toolchain/gcc.h>
#include "board_config.h"

/* Support 3 USBC ports */
#ifndef PD_PORT_3_ENABLE
#define PD_PORT_3_ENABLE                     (1u)
#endif

/* Define the port number */
#define PD_DUALPORT_ENABLE                   (1u) 
#if PD_PORT_3_ENABLE                  
#define PD_TRIPPORT_ENABLE                   (1u) 
#else
#define PD_TRIPPORT_ENABLE                   (0u)
#endif

#if PD_TRIPPORT_ENABLE
#define NO_OF_TYPEC_PORTS                    (3u)
#elif PD_DUALPORT_ENABLE
#define NO_OF_TYPEC_PORTS                    (2u)
#else
#define NO_OF_TYPEC_PORTS                    (1u)
#endif

#define TYPEC_PORT_0_IDX                     (0u)
#define TYPEC_PORT_1_IDX                     (1u)
#define TYPEC_PORT_2_IDX                     (2u)

/* UCSI Revision upport is enabled. */
#define UCSI_REV_1_x_ENABLE                  (1u)
#if UCSI_REV_1_x_ENABLE
BUILD_ASSERT((CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE==256),"eSPI MMIO length needs to be 256 bytes for UCSI 1.x");
#define UCSI_REV_2_x_ENABLE                  (0u)
#define UCSI_REV_3_x_ENABLE                  (0u)
#else
#define UCSI_REV_2_x_ENABLE                  (1u)
#if UCSI_REV_2_x_ENABLE
BUILD_ASSERT((CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE==1024),"eSPI MMIO length needs to be 1024 bytes for UCSI 2.x");
#define UCSI_REV_3_x_ENABLE                  (0u)
#else
#define UCSI_REV_3_x_ENABLE                  (1u)
BUILD_ASSERT((CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE==1024),"eSPI MMIO length needs to be 1024 bytes for UCSI 3.x");
#endif  /* UCSI_REV_2_x_ENABLE */
#endif  /* UCSI_REV_1_x_ENABLE */

    
#if CONFIG_EC_UCSI_ENABLE
/* Enable UCSI Alt mode commands */
#define UCSI_ALT_MODE_ENABLED                (1u)

/* Enable Start / Stop UCSI Command */
#define ENABLE_ST_SP_UCSI_CMD                (1u)
    
/* Enable Set Power Level UCSI Command */    
#define UCSI_SET_PWR_LEVEL_ENABLE            (1u)

/* Enable the BC 1.2 */
#define UCSI_BATTERY_CHARGING_ENABLE         (0u)

#endif /*CONFIG_EC_UCSI_ENABLE*/

#define PD_REV3_ENABLE                       (1u)

/* Standard SVID defined by USB-PD specification */
#define STD_SVID                             (0xFF00u)

/* Displayport SVID defined by VESA specification */
#define DP_SVID                              (0xFF01u)

/* Intel (Thunderbolt 3) SVID. */
#define INTEL_VID                            (0x8087u)

/* Maximum number of DOs in a packet. Limited by PD message definition. */
#define MAX_NO_OF_DO                         (7u)

/* Maximum number of PDOs in a packet. Limited by PD message definition. */
#define MAX_NO_OF_PDO                        (MAX_NO_OF_DO)

#define PD_VOLT_PER_UNIT                     (50u)

#define CBL_CAP_0A                           (0u)        /* Cable current capability: 0 A. */
#define CBL_CAP_3A                           (300u)      /* Cable current capability: 3 A. */
#define CBL_CAP_5A                           (500u)      /* Cable current capability: 5 A. */

/* Unique ID assigned to Thunderbolt mode */
#define DP_ALT_MODE_ID                       (0u)
/* Unique ID assigned to DP */
#define TBT_ALT_MODE_ID                      (1u)
/* mode not supported */
#define MODE_NOT_SUPPORTED                   (0xFFu)
/* Get the maximum from among two numbers */
#define GET_MAX(a,b)    (((a) > (b)) ? (a) : (b))
/* Get the minimum from among two numbers. */
#define GET_MIN(a,b)    (((a) > (b)) ? (b) : (a))

/********************************* PD macros **********************************/

/* Externally powered bit position in Source PDO mask. */
#define PD_EXTERNALLY_POWERED_BIT_POS               (7u)

/* Mask to be applied on Fixed Supply Source PDO for PD Rev 2.0 */
#define PD_FIX_SRC_PDO_MASK_REV2                    (0xFE3FFFFFu)

/* Mask to be applied on Fixed Supply Source PDO for PD Rev 3.0 */
#define PD_FIX_SRC_PDO_MASK_REV3                    (0xFF3FFFFFu)

/* Device Policy Manager swap command macros.*/
#define DPM_DR_SWAP_RESP_MASK                       (0x03)
#define DPM_DR_SWAP_RESP_POS                        (0x00)
#define DPM_PR_SWAP_RESP_MASK                       (0x0C)
#define DPM_PR_SWAP_RESP_POS                        (0x02)
#define DPM_VCONN_SWAP_RESP_MASK                    (0x30)
#define DPM_VCONN_SWAP_RESP_POS                     (0x04)

/* Macro to extract the default DR_SWAP command response from the swap_response field
     in the configuration table. */
#define GET_DR_SWAP_RESP(resp)                      ((resp) & 0x3u)

/* Macro to extract the default PR_SWAP command response from the swap_response field in the configuration table. */
#define GET_PR_SWAP_RESP(resp)                      (((resp) & 0xCu) >> 2u)

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

#pragma pack(push,1)

/* Enum of the VDM types.*/
typedef enum
{
    VDM_TYPE_UNSTRUCTURED = 0,          /* Unstructured VDM. */
    VDM_TYPE_STRUCTURED                 /* Structured VDM. */
} vdm_type_t;

/* Enum of the standard VDM command types. */
typedef enum
{
    CMD_TYPE_INITIATOR = 0,             /* VDM sent by command initiator. */
    CMD_TYPE_RESP_ACK,                  /* ACK response. */
    CMD_TYPE_RESP_NAK,                  /* NAK response. */
    CMD_TYPE_RESP_BUSY                  /* BUSY response. */
} std_vdm_cmd_type_t;

/* Enum of the response status to DPM commands. */
typedef enum
{
    SEQ_ABORTED = 0,                    /* PD AMS aborted. */
    CMD_FAILED,                         /* PD AMS failed. */
    RES_TIMEOUT,                        /* No response received. */
    CMD_SENT,                           /* PD command has been sent. Response wait may be in progress. */
    RES_RCVD                            /* Response received. */
} resp_status_t;

/* Enum of the SOP (Start Of Frame) types. */
typedef enum
{
    SOP = 0,                            /* SOP: Used for communication with port partner. */
    SOP_PRIME,                          /* SOP': Cable marker communication. */
    SOP_DPRIME,                         /* SOP'': Cable marker communication. */
    SOP_P_DEBUG,                        /* SOP'_Debug */
    SOP_DP_DEBUG,                       /* SOP''_Debug */
    HARD_RESET,                         /* Hard Reset */
    CABLE_RESET,                        /* Cable Reset */
    SOP_INVALID                         /* Undefined ordered set. */
} sop_t;

/* Enum of the control message types. */
typedef enum
{
    CTRL_MSG_RSRVD = 0,                 /* 0x00: Reserved message code. */
    CTRL_MSG_GOOD_CRC,                  /* 0x01: GoodCRC message. */
    CTRL_MSG_GO_TO_MIN,                 /* 0x02: GotoMin message. */
    CTRL_MSG_ACCEPT,                    /* 0x03: Accept message. */
    CTRL_MSG_REJECT,                    /* 0x04: Reject message. */
    CTRL_MSG_PING,                      /* 0x05: Ping message. */
    CTRL_MSG_PS_RDY,                    /* 0x06: PS_RDY message. */
    CTRL_MSG_GET_SOURCE_CAP,            /* 0x07: Get_Source_Cap message. */
    CTRL_MSG_GET_SINK_CAP,              /* 0x08: Get_Sink_Cap message. */
    CTRL_MSG_DR_SWAP,                   /* 0x09: DR_Swap message. */
    CTRL_MSG_PR_SWAP,                   /* 0x0A: PR_Swap message. */
    CTRL_MSG_VCONN_SWAP,                /* 0x0B: VCONN_Swap message. */
    CTRL_MSG_WAIT,                      /* 0x0C: Wait message. */
    CTRL_MSG_SOFT_RESET,                /* 0x0D: Soft_Reset message. */
    CTRL_MSG_NOT_SUPPORTED = 16,        /* 0x10: Not_Supported message. */
    CTRL_MSG_GET_SRC_CAP_EXTD,          /* 0x11: Get_Source_Cap_Extended message. */
    CTRL_MSG_GET_STATUS,                /* 0x12: Get_Status message . */
    CTRL_MSG_FR_SWAP,                   /* 0x13: FR_Swap message. */
    CTRL_MSG_GET_PPS_STATUS,            /* 0x14: Get_PPS_Status message. */
    CTRL_MSG_GET_COUNTRY_CODES          /* 0x15: Get_Country_Codes message. */
} ctrl_msg_t;

/* Enum of the DPM (Device Policy Manager) response types. */
typedef enum
{
    DPM_RESP_FAIL = 0,                  /* Command failed. */
    DPM_RESP_SUCCESS                    /* Command succeeded. */
} dpm_typec_cmd_resp_t;

/* Enum of events that are signalled to the application. */
typedef enum
{
    APP_EVT_UNEXPECTED_VOLTAGE_ON_VBUS,         /* 0x00: Unexpected high voltage seen on VBus. */
    APP_EVT_TYPE_C_ERROR_RECOVERY,              /* 0x01: Type-C error recovery initiated. */
    APP_EVT_CONNECT,                            /* 0x02: Type-C connect detected. */
    APP_EVT_DISCONNECT,                         /* 0x03: Type-C disconnect(detach) detected. */
    APP_EVT_EMCA_DETECTED,                      /* 0x04: Cable (EMCA) discovery successful. */
    APP_EVT_EMCA_NOT_DETECTED,                  /* 0x05: Cable (EMCA) discovery timed out. */
    APP_EVT_ALT_MODE,                           /* 0x06: Alternate mode related event. */
    APP_EVT_APP_HW,                             /* 0x07: MUX control related event. */
    APP_EVT_BB,                                 /* 0x08: Billboard status change. */
    APP_EVT_RP_CHANGE,                          /* 0x09: Rp termination change detected. */
    APP_EVT_HARD_RESET_RCVD,                    /* 0x0A: Hard Reset received. */
    APP_EVT_HARD_RESET_COMPLETE,                /* 0x0B: Hard Reset processing completed. */
    APP_EVT_PKT_RCVD,                           /* 0x0C: New PD message received. */
    APP_EVT_PR_SWAP_COMPLETE,                   /* 0x0D: PR_SWAP process completed. */
    APP_EVT_DR_SWAP_COMPLETE,                   /* 0x0E: DR_SWAP process completed. */
    APP_EVT_VCONN_SWAP_COMPLETE,                /* 0x0F: VConn_SWAP process completed. */
    APP_EVT_SENDER_RESPONSE_TIMEOUT,            /* 0x10: Sender response timeout occurred. */
    APP_EVT_VENDOR_RESPONSE_TIMEOUT,            /* 0x11: Vendor message response timeout occurred. */
    APP_EVT_HARD_RESET_SENT,                    /* 0x12: Hard Reset sent by device. */
    APP_EVT_SOFT_RESET_SENT,                    /* 0x13: Soft Reset sent by device. */
    APP_EVT_CBL_RESET_SENT,                     /* 0x14: Cable Reset sent by device. */
    APP_EVT_PE_DISABLED,                        /* 0x15: PE.Disabled state entered. */
    APP_EVT_PD_CONTRACT_NEGOTIATION_COMPLETE,   /* 0x16: Contract negotiation completed. */
    APP_EVT_VBUS_OVP_FAULT,                     /* 0x17: VBus Over Voltage fault detected. */
    APP_EVT_VBUS_OCP_FAULT,                     /* 0x18: VBus Over Current fault detected. */
    APP_EVT_VCONN_OCP_FAULT,                    /* 0x19: VConn Over Current fault detected. */
    APP_EVT_VBUS_PORT_DISABLE,                  /* 0x1A: PD port disable completed. */
    APP_EVT_TYPEC_STARTED,                      /* 0x1B: PD port enable (start) completed. */
    APP_EVT_FR_SWAP_COMPLETE,                   /* 0x1C: FR_SWAP process completed. */
    APP_EVT_TEMPERATURE_FAULT,                  /* 0x1D: Over Temperature fault detected. */
    APP_EVT_HANDLE_EXTENDED_MSG,                /* 0x1E: Extended message received and needs to be handled. */
    APP_EVT_VBUS_UVP_FAULT,                     /* 0x1F: VBus Under Voltage fault detected. */
    APP_EVT_VBUS_SCP_FAULT,                     /* 0x20: VBus Short Circuit fault detected. */
    APP_EVT_TYPEC_ATTACH_WAIT,                  /* 0x21: Type-C AttachWait state entered. For internal use only. */
    APP_EVT_TYPEC_ATTACH_WAIT_TO_UNATTACHED,    /* 0x22: Type-C transition from AttachWait to Unattached. For
                                                           internal use only. */
    APP_EVT_TYPEC_ATTACH,                       /* 0x23: Type-C attach event. */
    APP_EVT_CC_OVP,                             /* 0x24: Over Voltage on CC/VConn line detected. */
    APP_EVT_SBU_OVP,                            /* 0x25: Over Voltage on SBU1/SBU2 line detected. */
    APP_EVT_ALERT_RECEIVED,                     /* 0x26: Alert message received. For internal use only. */
    APP_EVT_SRC_CAP_TRIED_WITH_NO_RESPONSE,     /* 0x27: Src Cap tried with no response. For internal use only. */
    APP_EVT_PD_SINK_DEVICE_CONNECTED,           /* 0x28: Sink device connected. For internal use only. */
    APP_EVT_VBUS_RCP_FAULT,                     /* 0x29: VBus Reverse Current fault detected. */
    APP_EVT_STANDBY_CURRENT,                    /* 0x2A: Standby Current. */
		
	APP_EVT_NULL

} app_evt_t;

/* Enum of the DPM (Device Policy Manager) command types that can be initiated */
typedef enum
{
    DPM_CMD_SET_RP_DFLT = 0,            /* Command to select Default Rp. */
    DPM_CMD_SET_RP_1_5A,                /* Command to select 1.5 A Rp. */
    DPM_CMD_SET_RP_3A,                  /* Command to select 3 A Rp. */
	DPM_CMD_SET_PD,                     /* Command to select 3 A Rp. */
    DPM_CMD_PORT_DISABLE,               /* Command to disable the USB-PD port. */
    DPM_CMD_TYPEC_ERR_RECOVERY,         /* Command to initiate Type-C error recovery. */
    DPM_CMD_TYPEC_INVALID               /* Invalid command type. */
} dpm_typec_cmd_t;

/* This enumeration holds all possible application events related to Alt modes handling. */
typedef enum
{
    AM_NO_EVT = 0,                      /* Empty event. */
    AM_EVT_SVID_NOT_FOUND,              /* Sends to EC if UFP doesn't support any SVID. */
    AM_EVT_ALT_MODE_ENTERED,            /* Alternate mode entered. */
    AM_EVT_ALT_MODE_EXITED,             /* Alternate mode exited. */
    AM_EVT_DISC_FINISHED,               /* Discovery process was finished. */
    AM_EVT_SVID_NOT_SUPP,               /* It doesn't support received SVID. */
    AM_EVT_SVID_SUPP,                   /* It supports received SVID. */
    AM_EVT_ALT_MODE_SUPP,               /* It supports alternate mode. */
    AM_EVT_SOP_RESP_FAILED,             /* UFP VDM response failed. */
    AM_EVT_CBL_RESP_FAILED,             /* Cable response failed. */
    AM_EVT_CBL_NOT_SUPP_ALT_MODE,       /* Cable capabilities couldn't provide alternate mode hanling. */
    AM_EVT_NOT_SUPP_PARTNER_CAP,        /* device and UFP capabilities not consistent. */
    AM_EVT_DATA_EVT,                    /* Specific alternate mode event with data. */
    AM_EVT_SUPP_ALT_MODE_CHNG,          /* Same functionality as ALT_MODE_SUPP; new one for UCSI */
}alt_mode_app_evt_t;

/* Enum of the PD port roles  */
typedef enum
{
    PRT_ROLE_SINK = 0,                  /* Power sink */
    PRT_ROLE_SOURCE,                    /* Power source */
    PRT_DUAL,                           /* Dual Role Power device: can be source or sink. */
	PRT_NULL
} port_role_t;

/* Enum of the PD port types.*/
typedef enum
{
    PRT_TYPE_UFP = 0,                   /* Upstream facing port. USB device or Alternate mode accessory. */
    PRT_TYPE_DFP,                       /* Downstream facing port. USB host or Alternate mode controller. */
    PRT_TYPE_DRP                        /* Dual Role data device: can be UFP or DFP. */
} port_type_t;

/* Enum of the attached device type. */
typedef enum
{
    DEV_SNK = 1,                        /* Power sink device is attached. */
    DEV_SRC,                            /* Power source device is attached. */
    DEV_DBG_ACC,                        /* Debug accessory is attached. */
    DEV_AUD_ACC,                        /* Audio accessory is attached. */
    DEV_PWRD_ACC,                       /* Powered accessory is attached. */
    DEV_VPD,                            /* Vconn powered device is attached. */
    DEV_UNSUPORTED_ACC                  /* Unsupported device type is attached. */
} pd_devtype_t;

/**
 *  Union to hold a PD data object. All USB-PD data objects are 4-byte values which are interpreted
 * according to the message type, length and object position. This union represents all possible interpretations
 * of a USB-PD data object.
 */
typedef union
{

    uint32_t val;                                   /* Data object interpreted as an unsigned integer value. */

    /**  Structure of a BIST data object. */
    struct BIST_DO
    {
        uint32_t rsvd1                      : 16;   /* Reserved field. */
        uint32_t rsvd2                      : 12;   /* Reserved field. */
        uint32_t mode                       : 4;    /* BIST mode. */
    } bist_do;                                      /* DO interpreted as a BIST data object. */

    /**  Structure representing a Fixed Supply PDO - Source. */
    struct FIXED_SRC
    {
        uint32_t max_current                : 10;   /* Maximum current in 100mA units. */
        uint32_t voltage                    : 10;   /* Voltage in 50mV units. */
        uint32_t pk_current                 : 2;    /* Peak current. */
        uint32_t reserved                   : 1;    /* Reserved field. */
        uint32_t epr_mode_cap               : 1;    /* EPR Mode capable. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t dr_swap                    : 1;    /* Data Role Swap supported. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t ext_powered                : 1;    /* Externally powered. */
        uint32_t usb_suspend_sup            : 1;    /* USB suspend supported. */
        uint32_t dual_role_power            : 1;    /* Dual role power support. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b00. */
    } fixed_src;                                    /* DO interpreted as a Fixed Supply PDO - Source. */

    /**  Structure representing a Variable Supply PDO - Source. */
    struct VAR_SRC
    {
        uint32_t max_current                : 10;   /* Maximum current in 10mA units. */
        uint32_t min_voltage                : 10;   /* Minimum voltage in 50mV units. */
        uint32_t max_voltage                : 10;   /* Maximum voltage in 50mV units. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b10. */
    } var_src;                                      /* DO interpreted as a Variable Supply PDO - Source. */

    /**  Structure representing a Battery Supply PDO - Source. */
    struct BAT_SRC
    {
        uint32_t max_power                  : 10;   /* Maximum power in 250mW units. */
        uint32_t min_voltage                : 10;   /* Minimum voltage in 50mV units. */
        uint32_t max_voltage                : 10;   /* Maximum voltage in 50mV units. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b01. */
    } bat_src;                                      /* DO interpreted as a Battery Supply PDO - Source. */

    /**  Structure representing a generic source PDO. */
    struct SRC_GEN
    {
        uint32_t max_cur_power              : 10;   /* Maximum current in 10 mA or power in 250 mW units. */
        uint32_t min_voltage                : 10;   /* Minimum voltage in 50mV units. */
        uint32_t max_voltage                : 10;   /* Maximum voltage in 50mV units. */
        uint32_t supply_type                : 2;    /* Supply type. */
    } src_gen;                                      /* DO interpreted as a generic PDO - Source. */

    /**  Structure representing a Fixed Supply PDO - Sink. */
    struct FIXED_SNK
    {
        uint32_t op_current                 : 10;   /* Operational current in 10mA units. */
        uint32_t voltage                    : 10;   /* Voltage in 50mV units. */
        uint32_t rsrvd                      : 3;    /* Reserved field. */
        uint32_t fr_swap                    : 2;    /* FR swap support. */
        uint32_t dr_swap                    : 1;    /* Data Role Swap supported. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t ext_powered                : 1;    /* Externally powered. */
        uint32_t high_cap                   : 1;    /* Higher capability possible. */
        uint32_t dual_role_power            : 1;    /* Dual role power support. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b00. */
    } fixed_snk;                                    /* DO interpreted as a Fixed Supply PDO - Sink. */

    /**  Structure representing a Variable Supply PDO - Sink. */
    struct VAR_SNK
    {
        uint32_t op_current                 : 10;   /* Operational current in 10mA units. */
        uint32_t min_voltage                : 10;   /* Minimum voltage in 50mV units. */
        uint32_t max_voltage                : 10;   /* Maximum voltage in 50mV units. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b10. */
    } var_snk;                                      /* DO interpreted as a Variable Supply PDO - Sink. */

    /**  Structure representing a Battery Supply PDO - Sink. */
    struct BAT_SNK
    {
        uint32_t op_power                   : 10;   /* Maximum power in 250mW units. */
        uint32_t min_voltage                : 10;   /* Minimum voltage in 50mV units. */
        uint32_t max_voltage                : 10;   /* Maximum voltage in 50mV units. */
        uint32_t supply_type                : 2;    /* Supply type - should be 'b01. */
    } bat_snk;                                      /* DO interpreted as a Battery Supply PDO - Sink. */

    /**  Structure representing a Fixed or Variable Request Data Object. */
    struct RDO_FIXED_VAR
    {
        uint32_t max_op_current             : 10;   /* Maximum operating current in 10mA units. */
        uint32_t op_current                 : 10;   /* Operating current in 10mA units. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack flag = 0. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_fix_var;                                  /* DO interpreted as a fixed/variable request. */

    /**  Structure representing a Fixed or Variable Request Data Object with GiveBack. */
    struct RDO_FIXED_VAR_GIVEBACK
    {
        uint32_t min_op_current             : 10;   /* Minimum operating current in 10mA units. */
        uint32_t op_current                 : 10;   /* Operating current in 10mA units. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack flag = 1. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_fix_var_gvb;                              /* DO interpreted as a fixed/variable request with giveback. */

    /**  Structure representing a Battery Request Data Object. */
    struct RDO_BAT
    {
        uint32_t max_op_power               : 10;   /* Maximum operating power in 250mW units. */
        uint32_t op_power                   : 10;   /* Operating power in 250mW units. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack flag = 0. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_bat;                                      /* DO interpreted as a Battery request. */

    /**  Structure representing a Battery Request Data Object with GiveBack. */
    struct RDO_BAT_GIVEBACK
    {
        uint32_t min_op_power               : 10;   /* Minimum operating power in 250mW units. */
        uint32_t op_power                   : 10;   /* Operating power in 250mW units. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack flag = 1. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_bat_gvb;                                  /* DO interpreted as a Battery request with giveback. */

    /**  Structure representing a generic Request Data Object. */
    struct RDO_GEN
    {
        uint32_t min_max_power_cur          : 10;   /* Min/Max power or current requirement. */
        uint32_t op_power_cur               : 10;   /* Operating power or current requirement. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack supported flag = 0. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_gen;                                      /* DO interpreted as a generic request message. */

    /**  Structure representing a Generic Request Data Object with GiveBack. */
    struct RDO_GEN_GVB
    {
        uint32_t max_power_cur              : 10;   /* Min/Max power or current requirement. */
        uint32_t op_power_cur               : 10;   /* Operating power or current requirement. */
        uint32_t rsrvd1                     : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Unchunked extended messages supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend. */
        uint32_t usb_comm_cap               : 1;    /* USB communication capability. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch. */
        uint32_t give_back_flag             : 1;    /* GiveBack supported flag = 1. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsrvd2                     : 1;    /* Reserved field. */
    } rdo_gen_gvb;                                  /* DO interpreted as a generic request with giveback. */

    /**  Structure representing a Structured VDM Header Data Object. */
    struct STD_VDM_HDR
    {
        uint32_t cmd                        : 5;    /* VDM command id. */
        uint32_t rsvd1                      : 1;    /* Reserved field. */
        uint32_t cmd_type                   : 2;    /* VDM command type. */
        uint32_t obj_pos                    : 3;    /* Object position. */
        uint32_t rsvd2                      : 2;    /* Reserved field. */
        uint32_t st_ver                     : 2;    /* Structured VDM version. */
        uint32_t vdm_type                   : 1;    /* VDM type = Structured. */
        uint32_t svid                       : 16;   /* SVID associated with VDM. */
    } std_vdm_hdr;                                  /* DO interpreted as a Structured VDM header. */

    /**  Structure representing an Unstructured VDM header data object as defined. */
    struct USTD_VDM_HDR
    {
        uint32_t cmd                        : 5;    /* Command id. */
        uint32_t seq_num                    : 3;    /* Sequence number. */
        uint32_t rsvd1                      : 3;    /* Reserved field. */
        uint32_t cmd_type                   : 2;    /* Command type. */
        uint32_t vdm_ver                    : 2;    /* VDM version. */
        uint32_t vdm_type                   : 1;    /* VDM type = Unstructured. */
        uint32_t svid                       : 16;   /* SVID associated with VDM. */
    } ustd_vdm_hdr;                                 /* DO interpreted as unstructured VDM header. */

    /**  Structure representing an Unstructured VDM header data object as defined by QC 4.0 spec. */
    struct USTD_QC_4_0_HDR
    {
        uint32_t cmd_0                      : 8;    /* Command code #0. */
        uint32_t cmd_1                      : 7;    /* Command code #1. */
        uint32_t vdm_type                   : 1;    /* VDM type = Unstructured. */
        uint32_t svid                       : 16;   /* SVID associated with message. */
    } ustd_qc_4_0_hdr;                              /* DO interpreted as a QC 4.0 Unstructured VDM header. */

    /**  Structure representing an Unstructured VDM data object as defined by QC 4.0 spec. */
    struct QC_4_0_DATA_VDO
    {
        uint32_t data_0                     : 8;    /* Command data #0. */
        uint32_t data_1                     : 8;    /* Command data #1. */
        uint32_t data_2                     : 8;    /* Command data #2. */
        uint32_t data_3                     : 8;    /* Command data #3. */
    } qc_4_0_data_vdo;                              /* DO interpreted as a QC 4.0 Unstructured VDM data object. */

    /**  Structure representing a Standard ID_HEADER VDO. */
    struct STD_VDM_ID_HDR
    {
        uint32_t usb_vid                    : 16;   /* 16-bit vendor ID. */
#if PD_REV3_ENABLE
        uint32_t rsvd1                      : 7;    /* Reserved field. */
        uint32_t prod_type_dfp              : 3;    /* Product type as DFP. */
#else
        uint32_t rsvd1                      : 10;   /* Reserved field. */
#endif /* PD_REV3_ENABLE */
        uint32_t mod_support                : 1;    /* Whether alternate modes are supported. */
        uint32_t prod_type                  : 3;    /* Product type as UFP. */
        uint32_t usb_dev                    : 1;    /* USB device supported. */
        uint32_t usb_host                   : 1;    /* USB host supported. */
    } std_id_hdr;                                   /* DO interpreted as a Standard ID_HEADER VDO. */

    /**  Cert Stat VDO structure. */
    struct STD_CERT_VDO
    {
        uint32_t usb_xid                    : 32;   /* 32-bit XID value. */
    } std_cert_vdo;                                 /* DO interpreted as a Cert Stat VDO. */

    /**  Product VDO structure. */
    struct STD_PROD_VDO
    {
        uint32_t bcd_dev                    : 16;   /* 16-bit bcdDevice value. */
        uint32_t usb_pid                    : 16;   /* 16-bit product ID. */
    } std_prod_vdo;                                 /* DO interpreted as a Product VDO. */

    /**  Cable VDO structure as defined in USB-PD r2.0. */
    struct STD_CBL_VDO
    {
        uint32_t usb_ss_sup                 : 3;    /* USB signalling supported by the cable. */
        uint32_t sop_dp                     : 1;    /* Whether SOP'' controller is present. */
        uint32_t vbus_thru_cbl              : 1;    /* Whether cable allows VBus power through. */
        uint32_t vbus_cur                   : 2;    /* VBus current supported by the cable. */
        uint32_t ssrx2                      : 1;    /* Whether SSRX2 has configurable direction. */
        uint32_t ssrx1                      : 1;    /* Whether SSRX1 has configurable direction. */
        uint32_t sstx2                      : 1;    /* Whether SSTX2 has configurable direction. */
        uint32_t sstx1                      : 1;    /* Whether SSTX1 has configurable direction. */
        uint32_t cbl_term                   : 2;    /* Cable termination and VConn power requirement. */
        uint32_t cbl_latency                : 4;    /* Cable latency. */
        uint32_t typec_plug                 : 1;    /* Whether cable has a plug: Should be 0. */
        uint32_t typec_abc                  : 2;    /* Cable plug type. */
        uint32_t rsvd1                      : 4;    /* Reserved field. */
        uint32_t cbl_fw_ver                 : 4;    /* Cable firmware version. */
        uint32_t cbl_hw_ver                 : 4;    /* Cable hardware version. */
    } std_cbl_vdo;                                  /* DO interpreted as a PD 2.0 cable VDO. */

    /**  Passive cable VDO structure as defined by PD 3.0. */
    struct PAS_CBL_VDO
    {
        uint32_t usb_ss_sup                 : 3;    /* USB signalling supported by the cable. */
        uint32_t rsvd1                      : 2;    /* Reserved field. */
        uint32_t vbus_cur                   : 2;    /* VBus current supported by the cable. */
        uint32_t rsvd2                      : 2;    /* Reserved field. */
        uint32_t max_vbus_volt              : 2;    /* Max. VBus voltage supported. */
        uint32_t cbl_term                   : 2;    /* Cable termination and VConn power requirement. */
        uint32_t cbl_latency                : 4;    /* Cable latency. */
        uint32_t typec_plug                 : 1;    /* Reserved field. */
        uint32_t typec_abc                  : 2;    /* Cable plug type. */
        uint32_t rsvd3                      : 1;    /* Reserved field. */
        uint32_t vdo_version                : 3;    /* VDO version. */
        uint32_t cbl_fw_ver                 : 4;    /* Cable firmware version. */
        uint32_t cbl_hw_ver                 : 4;    /* Cable hardware version. */
    } pas_cbl_vdo;                                  /* DO interpreted as a PD 3.0 passive cable VDO. */

    /**  Active cable VDO structure as defined by PD 3.0. */
    struct ACT_CBL_VDO
    {
        uint32_t usb_ss_sup                 : 3;    /* USB signalling supported by the cable. */
        uint32_t sop_dp                     : 1;    /* Whether SOP'' controller is present. */
        uint32_t vbus_thru_cbl              : 1;    /* Whether cable conducts VBus through. */
        uint32_t vbus_cur                   : 2;    /* VBus current supported by the cable. */
        uint32_t rsvd1                      : 2;    /* Reserved field. */
        uint32_t max_vbus_volt              : 2;    /* Max. VBus voltage supported. */
        uint32_t cbl_term                   : 2;    /* Cable termination and VConn power requirement. */
        uint32_t cbl_latency                : 4;    /* Cable latency. */
        uint32_t typec_plug                 : 1;    /* Reserved field. */
        uint32_t typec_abc                  : 2;    /* Cable plug type. */
        uint32_t rsvd2                      : 1;    /* Reserved field. */
        uint32_t vdo_version                : 3;    /* VDO version. */
        uint32_t cbl_fw_ver                 : 4;    /* Cable firmware version. */
        uint32_t cbl_hw_ver                 : 4;    /* Cable hardware version. */
    } act_cbl_vdo;                                  /* DO interpreted as a PD 3.0 active cable VDO. */

    /**  AMA VDO structure as defined by PD 2.0. */
    struct STD_AMA_VDO
    {
        uint32_t usb_ss_sup                 : 3;    /* USB signalling supported. */
        uint32_t vbus_req                   : 1;    /* Whether device requires VBus. */
        uint32_t vcon_req                   : 1;    /* Whether device requires VConn. */
        uint32_t vcon_pwr                   : 3;    /* VConn power required. */
        uint32_t ssrx2                      : 1;    /* Whether SSRX2 has configurable direction. */
        uint32_t ssrx1                      : 1;    /* Whether SSRX1 has configurable direction. */
        uint32_t sstx2                      : 1;    /* Whether SSTX2 has configurable direction. */
        uint32_t sstx1                      : 1;    /* Whether SSTX1 has configurable direction. */
        uint32_t rsvd1                      : 12;   /* Reserved field. */
        uint32_t ama_fw_ver                 : 4;    /* AMA firmware version. */
        uint32_t ama_hw_ver                 : 4;    /* AMA hardware version. */
    } std_ama_vdo;                                  /* DO interpreted as a PD 2.0 AMA VDO. */
		
		/**  UFP VDO. */
    struct STD_UFP_VDO
    {
        uint32_t usb_highest_speed          : 3;    /* USB highest signalling supported. */
        uint32_t alt_modes                  : 3;    /* ALT MODE supported. */
        uint32_t vbus_req                   : 1;    /* Whether device requires VBus. */
        uint32_t vcon_req                   : 1;    /* Whether device requires VConn. */
        uint32_t vcon_pwr                   : 3;    /* VConn power required. */
        uint32_t rsvd                       : 11;   /* Reserved field. */
        uint32_t conn_type                  : 2;    /* Connector type and shal be 00b. */
        uint32_t dev_cap                    : 4;    /* The USB device capable. */
        uint32_t rsvd1                      : 1;    /* Reserved field. */
        uint32_t ufp_vdo_ver                : 3;    /* UFP VDO version. */
    } std_ufp_vdo; 

    /**  AMA VDO structure as defined by PD 3.0. */
    struct STD_AMA_VDO_PD3
    {
        uint32_t usb_ss_sup                 : 3;    /* USB signalling supported. */
        uint32_t vbus_req                   : 1;    /* Whether device requires VBus. */
        uint32_t vcon_req                   : 1;    /* Whether device requires VConn. */
        uint32_t vcon_pwr                   : 3;    /* VConn power required. */
        uint32_t rsvd1                      : 13;   /* Reserved field. */
        uint32_t vdo_version                : 3;    /* VDO version. */
        uint32_t ama_fw_ver                 : 4;    /* AMA firmware version. */
        uint32_t ama_hw_ver                 : 4;    /* AMA hardware version. */
    } std_ama_vdo_pd3;                              /* DO interpreted as a PD 3.0 AMA VDO. */

    /**  Discover_SVID response structure. */
    struct STD_SVID_RESP_VDO
    {
        uint32_t svid_n1                    : 16;   /* SVID #1 */
        uint32_t svid_n                     : 16;   /* SVID #2 */
    } std_svid_res;                                 /* DO interpreted as a DISCOVER_SVID response. */

    /**  DisplayPort Mode VDO as defined by VESA spec. */
    struct STD_DP_VDO
    {
        uint32_t port_cap                   : 2;    /* Port capability. */
        uint32_t signal                     : 4;    /* Signalling supported. */
        uint32_t recep                      : 1;    /* Whether Type-C connector is plug or receptacle. */
        uint32_t usb2_0                     : 1;    /* USB 2.0 signalling required. */
        uint32_t dfp_d_pin                  : 8;    /* DFP_D pin assignments supported. */
        uint32_t ufp_d_pin                  : 8;    /* UFP_D pin assignments supported. */
        uint32_t rsvd                       : 8;    /* Reserved field. */
    } std_dp_vdo;                                   /* DO interpreted as a DisplayPort Mode response. */

    /**  DisplayPort status update VDO as defined by VESA spec. */
    struct DP_STATUS_VDO
    {
        uint32_t dfp_ufp_conn               : 2;    /* Whether DFP_D/UFP_D is connected. */
        uint32_t pwr_low                    : 1;    /* Low power mode. */
        uint32_t en                         : 1;    /* DP functionality enabled. */
        uint32_t mult_fun                   : 1;    /* Multi-function mode preferred. */
        uint32_t usb_cfg                    : 1;    /* Request switch to USB configuration. */
        uint32_t exit                       : 1;    /* Exit DP mode request. */
        uint32_t hpd_state                  : 1;    /* Current HPD state. */
        uint32_t hpd_irq                    : 1;    /* HPD IRQ status. */
        uint32_t rsvd                       : 23;   /* Reserved field. */
    } dp_stat_vdo;                                  /* DO interpreted as a DisplayPort status update. */

    /**  DisplayPort configure VDO as defined by VESA spec. */
    struct DP_CONFIG_VDO
    {
        uint32_t sel_conf                   : 2;    /* Select configuration. */
        uint32_t sign                       : 4;    /* Signalling for DP protocol. */
        uint32_t rsvd1                      : 2;    /* Reserved. */
        uint32_t dfp_asgmnt                 : 8;    /* DFP_D pin assignment. */
        uint32_t ufp_asgmnt                 : 8;    /* UFP_D pin assignment. */
        uint32_t rsvd2                      : 8;    /* Reserved field. */
    } dp_cfg_vdo;                                   /* DO interpreted as a DisplayPort Configure command. */

    /**  Programmable Power Supply Source PDO. */
    struct PPS_SRC
    {
        uint32_t max_cur                    : 7;    /* Maximum current in 50 mA units. */
        uint32_t rsvd1                      : 1;    /* Reserved field. */
        uint32_t min_volt                   : 8;    /* Minimum voltage in 100 mV units. */
        uint32_t rsvd2                      : 1;    /* Reserved field. */
        uint32_t max_volt                   : 8;    /* Maximum voltage in 100 mV units. */
        uint32_t rsvd3                      : 2;    /* Reserved field. */
        uint32_t pps_pwr_limited            : 1;    /* Whether PPS power has been limited. */
        uint32_t apdo_type                  : 2;    /* APDO type: Should be 0 for PPS. */
        uint32_t supply_type                : 2;    /* PDO type: Should be 3 for PPS APDO. */
    } pps_src;                                      /* DO interpreted as a Programmable Power Supply - Source. */

    /**  Programmable Power Supply Sink PDO. */
    struct PPS_SNK
    {
        uint32_t op_cur                     : 7;    /* Operating current in 50 mA units. */
        uint32_t rsvd1                      : 1;    /* Reserved field. */
        uint32_t min_volt                   : 8;    /* Minimum voltage in 100 mV units. */
        uint32_t rsvd2                      : 1;    /* Reserved field. */
        uint32_t max_volt                   : 8;    /* Maximum voltage in 100 mV units. */
        uint32_t rsvd3                      : 1;    /* Reserved field. */
        uint32_t cur_fb                     : 1;    /* Whether current foldback is required. */
        uint32_t rsvd4                      : 1;    /* Reserved field. */
        uint32_t apdo_type                  : 2;    /* APDO type: Should be 0 for PPS. */
        uint32_t supply_type                : 2;    /* PDO type: Should be 3 for PPS APDO. */
    } pps_snk;                                      /* DO interpreted as a Programmable Power Supply - Sink. */

    /**  Programmable Request Data Object. */
    struct RDO_PPS
    {
        uint32_t op_cur                     : 7;    /* Operating current in 50 mA units. */
        uint32_t rsvd1                      : 2;    /* Reserved field. */
        uint32_t out_volt                   : 11;   /* Requested output voltage in 20 mV units. */
        uint32_t rsvd2                      : 3;    /* Reserved field. */
        uint32_t unchunk_sup                : 1;    /* Whether unchunked extended messages are supported. */
        uint32_t no_usb_suspend             : 1;    /* No USB suspend flag. */
        uint32_t usb_comm_cap               : 1;    /* Whether sink supports USB communication. */
        uint32_t cap_mismatch               : 1;    /* Capability mismatch flag. */
        uint32_t rsvd3                      : 1;    /* Reserved field. */
        uint32_t obj_pos                    : 3;    /* Object position - index to source PDO. */
        uint32_t rsvd4                      : 1;    /* Reserved field. */
    } rdo_pps;                                      /* DO interpreted as a PPD Request. */

    /**  PD 3.0 Alert Data Object. */
    struct ADO_ALERT
    {
        uint32_t rsvd1                      :16;    /* Reserved field. */
        uint32_t hot_swap_bats              :4;     /* Identifies hot-swappable batteries whose status has changed. */
        uint32_t fixed_bats                 :4;     /* Identifies fixed batteries whose status has changed. */
        uint32_t rsvd2                      :1;     /* Reserved field. */
        uint32_t bat_status_change          :1;     /* Battery status changed. */
        uint32_t ocp                        :1;     /* Over-Current event status. */
        uint32_t otp                        :1;     /* Over-Temperature event status. */
        uint32_t op_cond_change             :1;     /* Operating conditions changed. */
        uint32_t src_input_change           :1;     /* Power source input changed. */
        uint32_t ovp                        :1;     /* Over-Voltage event status. */
    } ado_alert;                                    /* DO interpreted as a PD 3.0 alert message. */

    /**  Thunderbolt Discover Modes Response Data Object. */
    struct TBT_VDO
    {
        uint32_t intel_mode                 : 16;   /* Thunderbolt (Intel) modes identifier. */
        uint32_t cbl_speed                  : 3;    /* Data bandwidth supported by the Type-C cable. */
        uint32_t cbl_gen                    : 2;    /* Thunderbolt cable generation. */
        uint32_t cbl_type                   : 1;    /* Whether cable is non-optical or optical. */
        uint32_t cbl                        : 1;    /* Type of Type-C cable: Passive or Active. */
        uint32_t link_training              : 1;    /* Type of link training supported by active cable. */
        uint32_t leg_adpt                   : 1;    /* Whether this is a legacy Thunderbolt adapter. */
        uint32_t rsvd0                      : 1;    /* Reserved field. */
        uint32_t vpro_dock_host             : 1;    /* Whether the device supports VPRO feature. */
        uint32_t rsvd1                      : 5;    /* Reserved field. */
    } tbt_vdo;                                      /* DO interpreted as a Thunderbolt Discovery response. */

    /** @cond DOXYGEN_HIDE */
    struct SLICE_VDO
    {
        uint32_t slice_mode                 : 16;
        uint32_t module_type                : 2;
        uint32_t rsvd                       : 14;
    } slice_vdo;

    struct SLICE_SUBHDR
    {
        uint32_t am_addr                    : 20;
        uint32_t vdo_cnt                    : 3;
        uint32_t multi_part                 : 1;
        uint32_t data_cnt                   : 8;
    } slice_subhdr;
    /** @endcond */

} pd_do_t;

/* Union to hold the PD header defined by the USB-PD specification. Lower 16 bits hold the message
 * header and the upper 16 bits hold the extended message header (where applicable).
 */
typedef union
{
    uint32_t val;                               /* Header expressed as a 32-bit word. */

    /**  PD message header broken down into component fields. Includes 2-byte extended message header. */
    struct PD_HDR
    {
        uint32_t msg_type   : 5;                /* Bits 04:00 - Message type. */
        uint32_t data_role  : 1;                /* Bit     05 - Data role. */
        uint32_t spec_rev   : 2;                /* Bits 07:06 - Spec revision. */
        uint32_t pwr_role   : 1;                /* Bit     08 - Power role. */
        uint32_t msg_id     : 3;                /* Bits 11:09 - Message ID. */
        uint32_t len        : 3;                /* Bits 14:12 - Number of data objects. */
        uint32_t extd       : 1;                /* Bit     15 - Extended message. */
        uint32_t data_size  : 9;                /* Bits 24:16 - Extended message size in bytes. */
        uint32_t rsvd1      : 1;                /* Bit     25 - Reserved. */
        uint32_t request    : 1;                /* Bit     26 - Chunk request. */
        uint32_t chunk_no   : 4;                /* Bits 30:27 - Chunk number. */
        uint32_t chunked    : 1;                /* Bit     31 - Chunked message. */
    } hdr;                                      /* PD message header split into component fields. */
} pd_hdr_t;

/* Enum of the PDO types. */
typedef enum
{
    PDO_FIXED_SUPPLY = 0,               /* Fixed (voltage) supply power data object. */
    PDO_BATTERY,                        /* Battery based power data object. */
    PDO_VARIABLE_SUPPLY,                /* Variable (voltage) supply power data object. */
    PDO_AUGMENTED                       /* Augmented power data object. */
} pdo_t;

/**
 *  PD Device Policy Status structure. This structure holds all of the configuration and status
 * information associated with a port on the device. Members of this structure should not be
 * directly modified by any of the application code.
 */
typedef struct
{
    uint8_t port_role;          /* Port role: Sink, Source or Dual. */
    uint8_t dflt_port_role;     /* Default port role: Sink or Source. */
    uint8_t src_cur_level;      /* Type C current level in the source role. */
    uint8_t swap_response;      /* Response to be sent for each USB-PD SWAP command.
                                     - Bits 1:0 => DR_SWAP response.
                                     - Bits 3:2 => PR_SWAP response.
                                     - Bits 5:4 => VCONN_SWAP response.
                                     Allowed values are: 0=ACCEPT, 1=REJECT, 2=WAIT, 3=NOT_SUPPORTED.
                                 */

    uint8_t src_pdo_count;      /* Source PDO count from the config table or updated at runtime by the EC. */
    uint8_t src_pdo_mask;       /* Source PDO mask from the config table or updated at runtime by the EC. */
    uint8_t snk_pdo_count;      /* Sink PDO count from the config table or updated at runtime by the EC. */
    uint8_t snk_pdo_mask;       /* Sink PDO mask from the config table or updated at runtime by the EC. */

    uint8_t port_disable;       /* PD port disable flag. */
    uint8_t vconn_retain;       /* Whether VConn should be retained in ON state. */

    pd_do_t src_pdo[MAX_NO_OF_PDO];          /* Source PDO loaded from the config table or updated at runtime by the EC. */
    pd_do_t snk_pdo[MAX_NO_OF_PDO];          /* Sink PDO loaded from the config table or updated at runtime by the EC. */
	pd_do_t src_pdo_deft[MAX_NO_OF_PDO];     /* Source PDO loaded from the config table or updated at runtime by the EC. */
    pd_do_t snk_pdo_deft[MAX_NO_OF_PDO];     /* Sink PDO loaded from the config table or updated at runtime by the EC. */
    uint16_t snk_max_min[MAX_NO_OF_PDO];     /* Max min current from the config table or updated at runtime by the EC. */

    port_type_t cur_port_type;          /* Current port type: UFP or DFP. */
    port_role_t cur_port_role;          /* Current Port role: Sink or Source. */
    uint8_t volatile snk_cur_level;     /* Type C current level in sink role. */
    uint8_t volatile dead_bat;          /* Flag to indicate dead battery operation. */
    uint8_t modal_op_support;

    uint8_t polarity;                   /* CC channel polarity (CC1=0, CC2=1) */
    uint8_t volatile connect;           /* Port connected but not debounced yet. */
    uint8_t volatile attach;            /* Port attached indication. */
    pd_devtype_t attached_dev;          /* Type of device attached. */
    uint8_t volatile contract_exist;    /* PD contract exists indication. */
    uint8_t volatile pd_connected;      /* Ports are PD connected indication. */
    uint8_t volatile pd_disabled;       /* PD disabled indication. */
    uint8_t volatile ra_present;        /* Ra present indication. */
    uint8_t volatile emca_present;      /* EMCA cable present indication. */
    uint8_t volatile VbusStatus;        /* VBUS status as safe, 5V, other or error */

    uint8_t cur_src_pdo_count;          /* Source PDO count in the last sent source cap. */
    uint8_t cur_snk_pdo_count;          /* Sink PDO count in the last sent sink cap. */
	uint8_t partner_src_pdo_count;      /* Source PDO count in the last sent source cap. */
    uint8_t partner_snk_pdo_count;      /* Sink PDO count in the last sent sink cap. */

    pd_do_t cbl_vdo;                    /* Stores the last received cable VDO. */
    bool cbl_mode_en;                   /* Whether cable supports alternate modes. */

    pd_do_t cur_src_pdo[MAX_NO_OF_PDO];         /* Current source PDOs sent in source cap messages. */
    pd_do_t cur_snk_pdo[MAX_NO_OF_PDO];         /* Current sink PDOs sent in sink cap messages. */
	pd_do_t partner_src_pdo[MAX_NO_OF_PDO];     /* Current source PDOs sent in source cap messages. */
    pd_do_t partner_snk_pdo[MAX_NO_OF_PDO];     /* Current sink PDOs sent in sink cap messages. */
    pd_do_t src_rdo;                            /* Last RDO received (when operating as a source) that resulted in a contract. */
    pd_do_t snk_rdo;                            /* Last RDO sent (when operating as a sink) that resulted in a contract. */
    pd_do_t snk_sel_pdo;                        /* Selected PDO which resulted in contract (when sink). */
    pd_do_t src_sel_pdo;                        /* Selected PDO which resulted in contract (when source). */

    bool alt_mode_entered;                      /* alt mode is entered */
    bool alt_mode_enable;                       /* alt mode is enabled */
    bool usb4_entered;                          /* usb4 is entered */
    bool tbt3_entered;                          /* tbt3 is entered */
    bool dp_entered;                            /* dp alt mode if entered */
    bool usb4_supp_vid;                         /* discover vid check if it support USB4 device mode */
    bool tbt3_supp_svid;                        /* discover vid check if it support TBT3 device mode */
    uint16_t partner_vid;                       /* discover vid partner vendor id */
    uint16_t partner_pid;                       /* discover vid partner product id */
} dpm_status_t;

/* Enum of the PD Request results. Enum fields map to the control
 * message field in the PD spec.
 */
typedef enum
{
    REQ_SEND_HARD_RESET = 1,            /* Invalid message. Send Hard Reset. */
    REQ_ACCEPT = 3,                     /* Send Accept message. */
    REQ_REJECT = 4,                     /* Send Reject message. */
    REQ_WAIT = 12,                      /* Send Wait message. */
    REQ_NOT_SUPPORTED = 16              /* Send Not_Supported message. Will translate to Reject message under PD 2.0 */
} app_req_status_t;

/* Enum of the standard VDM commands. */
typedef enum
{
    VDM_CMD_DSC_IDENTITY = 1,           /* Discover Identity command. */
    VDM_CMD_DSC_SVIDS,                  /* Discover SVIDs command. */
    VDM_CMD_DSC_MODES,                  /* Discover Modes command. */
    VDM_CMD_ENTER_MODE,                 /* Enter Mode command. */
    VDM_CMD_EXIT_MODE,                  /* Exit Mode command. */
    VDM_CMD_ATTENTION,                  /* Attention message. */
    VDM_CMD_DP_STATUS_UPDT = 16,        /* DisplayPort Status Update message. */
    VDM_CMD_DP_CONFIGURE = 17           /* DisplayPort Configure command. */
} std_vdm_cmd_t;

/* Alt modes manager application event/command structure. */
typedef union
{
    uint32_t val;                        /* Integer field used for direct manipulation of reason code. */

    /**  Struct containing alternate modes manager event/command . */
    struct ALT_MODE_EVT
    {
        uint32_t data_role       : 1;    /* Current event sender data role. */
        uint32_t alt_mode_evt    : 7;    /* Alt mode event index from alt_mode_app_evt_t structure. */
        uint32_t alt_mode        : 8;    /* Alt mode ID. */
        uint32_t svid            : 16;   /* Alt mode related SVID. */
    }alt_mode_event;                     /* Union containing the alt mode event value. */

    /**  Struct containing alternate modes manager event/command data. */
    struct ALT_MODE_EVT_DATA
    {
        uint32_t evt_data        : 24;   /* Alt mode event's data. */
        uint32_t evt_type        : 8;    /* Alt mode event's type. */
    }alt_mode_event_data;                /* Union containing the alt mode event's data value. */

}alt_mode_evt_t;

/* Enum of the standard VDM product types. */
typedef enum
{
    PROD_TYPE_UNDEF = 0,                /**< Undefined device type. */
    PROD_TYPE_HUB,                      /**< Hub device type. */
    PROD_TYPE_PERI,                     /**< Peripheral device type. */
    PROD_TYPE_PAS_CBL,                  /**< Passive Cable. */
    PROD_TYPE_ACT_CBL,                  /**< Active Cable. */
    PROD_TYPE_AMA,                      /**< Alternate Mode Accessory. */
    PROD_TYPE_VPD                       /**< Vconn powered device. */
} std_vdm_prod_t;

/* Union to hold the PD extended header */
typedef union
{
    uint16_t val;                               /* Extended header expressed as 2-byte integer value. */

    /* Extended header broken down into respective fields. */
    struct EXTD_HDR_T
    {
        uint16_t data_size  : 9;                /* Bits 08:00 - Extended message size in bytes. */
        uint16_t rsvd1      : 1;                /* Bit     09 - Reserved. */
        uint16_t request    : 1;                /* Bit     10 - Chunk request. */
        uint16_t chunk_no   : 4;                /* Bits 14:11 - Chunk number. */
        uint16_t chunked    : 1;                /* Bit     15 - Chunked message. */
    } extd;                                     /* Extended header broken down into respective fields. */
} pd_extd_hdr_t;

/* brief Possible responses to various USB-PD swap requests from the application layer */
typedef enum {
    APP_RESP_ACCEPT         = 0u,       /* Swap request accepted. */
    APP_RESP_REJECT,                    /* Swap request rejected. */
    APP_RESP_WAIT,                      /* Swap request send wait response */
    APP_RESP_NOT_SUPPORTED              /* Swap request not supported. */
} app_swap_resp_t;

typedef struct
{
    uint8_t nBytes;
    uint8_t RR;
    uint8_t MM;
    uint8_t VVVV;
    uint8_t EE;
} PD_FW_VERSION;

typedef  struct
{
	/* FW version */
	uint8_t RR;
	uint8_t MM;
	uint8_t VVVV;
    uint8_t EE;
	uint8_t Custom[4];
	/* controller mode */
	uint8_t mode[4];
	/* Load PDFW is skipped or not after EC POR */
	bool isFwLoadSkipped;
	bool isFwLoadSuccess;
	/* controller mini-type */
	uint8_t type[4];
	uint8_t retimer[8];
	uint8_t ParadeVer[8];
	uint8_t sku[2];
} PD_VR_CTRL_STATUS;

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

extern PD_VR_CTRL_STATUS g_pdCtrlSt[2];

/* get pd info */
const dpm_status_t* dpm_get_info(uint8_t port);
/* set pd status */
dpm_status_t* dpm_set_status(uint8_t port);
/* get pd alt mode number */
uint8_t get_alt_mode_number(uint8_t port);
/* set pd alt mode number */
void set_alt_mode_number(uint8_t port, uint8_t num);
/* set pd alt mode info */
void set_alt_mode_info(uint8_t port, uint8_t index, uint16_t svid, uint32_t mode);
/* get pd alt mode svid support */
uint16_t get_alt_mode_svids_supp(uint8_t port, uint8_t index);
/* get pd alt mode modes support */
uint32_t get_alt_mode_modes_supp(uint8_t port, uint8_t index);
/* get current alt mode id */
uint32_t get_cur_alt_mode_id(uint8_t port);
/* set current alt mode id */
void set_cur_alt_mode_id(uint8_t port, uint32_t mode_id);
/* divide two unsigned integers and return the quotient rounded up to the nearest integer */
uint32_t div_round_up(uint32_t x, uint32_t y);
#endif /* END of AMD_CRB_DRIVERS_PD_H_ */
