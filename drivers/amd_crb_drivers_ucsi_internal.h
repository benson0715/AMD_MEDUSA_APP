/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_UCSI_INTERNAL_H_
#define AMD_CRB_DRIVERS_UCSI_INTERNAL_H_

#include "amd_crb_drivers_pd.h"

#define PACK_STRUCT                      __attribute__((packed, aligned(1)))

#define UCSI_MAX_DATA_LENGTH            (16)

/* UCSI Spec Version Info. */
#if UCSI_REV_1_x_ENABLE
#define UCSI_MAJOR_VERSION              (1)
#define UCSI_MINOR_VERSION              (2)
#elif UCSI_REV_2_x_ENABLE
#define UCSI_MAJOR_VERSION              (2)
#define UCSI_MINOR_VERSION              (0)
#elif UCSI_REV_3_x_ENABLE
#define UCSI_MAJOR_VERSION              (3)
#define UCSI_MINOR_VERSION              (0)
#endif
#define UCSI_SUBMINOR_VERSION           (0)
#define UCSI_VERSION                    (UCSI_SUBMINOR_VERSION | (UCSI_MINOR_VERSION << 4) | (UCSI_MAJOR_VERSION << 8))

#define UCSI_BOUND(bound, lenval)       GET_MIN(GET_MIN(UCSI_MAX_DATA_LENGTH, lenval), bound)

/*****************************************************************************
 * Data Struct Definition
 ****************************************************************************/
/* Union to hold the Control Data Structure. */
typedef union {
    uint8_t data[6];
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1        : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT generic;
#if UCSI_REV_1_x_ENABLE
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT connector_reset;
#elif UCSI_REV_2_x_ENABLE
    struct {
        uint8_t connector_number : 7;
        uint8_t reset_type : 1;    /* 0: hard reset; 1: data reset */
        uint8_t reserved2[5];
    } PACK_STRUCT connector_reset;
#endif
    struct {
        uint8_t connector_change_ack  : 1;
        uint8_t command_completed_ack : 1;
        uint8_t reserved1             : 6;
        uint8_t reserved2[5];
    } PACK_STRUCT ack_cc_ci;
#if UCSI_REV_1_x_ENABLE
    struct {
        uint32_t notifications;
        uint8_t  reserved[2];
    } PACK_STRUCT set_notification_enable;
#elif UCSI_REV_2_x_ENABLE
    struct {
        uint32_t notifications : 17;
        uint32_t reserved1 : 15;
        uint16_t reserved2;
    } PACK_STRUCT set_notification_enable;
#endif
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_connector_capability;
    struct {
        uint16_t connector_number   : 7;
        uint16_t usb_operating_mode : 3;
        uint16_t reserved1          : 6;
        uint8_t reserved2[4];
    } PACK_STRUCT set_uom;
    struct {
        uint16_t connector_number   : 7;
        uint16_t operate_as_dfp     : 1;
        uint16_t operate_as_ufp     : 1;
        uint16_t accept_dr_swap     : 1;
        uint16_t reserved1          : 6;
        uint8_t reserved2[4];
    } PACK_STRUCT set_uor;
    struct {
        uint16_t connector_number     : 7;
        uint16_t operate_as_src     : 1;
        uint16_t operate_as_snk     : 1;
        uint16_t accept_pr_swap     : 1;
        uint16_t reserved1            : 6;
        uint8_t reserved2[4];
    } PACK_STRUCT set_pdr;
    struct {
        uint8_t recipient;
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t alterate_mode_offset;
        uint8_t num_alternate_modes;
        uint8_t reserved2[2];
    } PACK_STRUCT get_alternate_modes;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_cam_supported;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_current_cam;
    struct {
        uint8_t connector_number : 7;
        uint8_t enter_or_exit : 1;
        uint8_t new_cam;
        uint8_t alt_mode_specific[4];
    } PACK_STRUCT set_new_cam;
    struct {
        uint8_t connector_number : 7;
        uint8_t partner_pdo : 1;
        uint8_t pdo_offset;
        uint8_t num_pdos : 2;
        uint8_t is_source_pdo : 1;
        uint8_t src_cap_type : 2;
        uint8_t reserved8 : 3;
        uint8_t reserved[3];
    } PACK_STRUCT get_pdos;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_cable_property;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_connector_status;
    struct {
        uint8_t connector_number : 7;
        uint8_t src_snk : 1;
        uint8_t usb_pd_max_pwr;
        uint8_t usb_typec_current : 3;
        uint8_t reserved1 : 5;
        uint8_t reserved2[3];
    } PACK_STRUCT set_power_level;
#if UCSI_REV_2_x_ENABLE
    struct {
        uint32_t connector_number : 7;
        uint32_t recipient : 3;
        uint32_t message_offset : 8;
        uint32_t number_of_bytes : 8;
        uint32_t response_message_type : 6;
        uint8_t reserved[2];
    } PACK_STRUCT get_pd_message;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t reserved2[5];
    } PACK_STRUCT get_attention_vdo;
    struct {
        uint8_t connector_number : 7;
        uint8_t reserved1 : 1;
        uint8_t current_alt_mode;
        uint8_t reserved2[4];
    } PACK_STRUCT get_current_alt_mode_config;
    struct {
        uint32_t connector_number : 7;
        uint32_t direction : 2;
        uint32_t fw_udpate_request;
        uint32_t data_index : 7;
        uint32_t end_of_message : 1;
        uint32_t reserved1 : 7;
        uint8_t reserved2[2];
    } PACK_STRUCT lpm_firmware_udpate;
    struct {
        uint32_t connector_number : 7;
        uint32_t direction : 2;
        uint32_t security_request : 1;
        uint32_t auth_protocol_revision;
        uint32_t auth_message;
        uint32_t data_index1 : 6;
        uint8_t data_index2 : 1;
        uint8_t end_of_message : 1;
        uint8_t reserved1 : 6;
        uint8_t reserved2;
    } PACK_STRUCT security_request;
    struct {
		uint16_t connector_number : 7;
		uint16_t re_timer_number : 2;
		uint16_t state : 3;
		uint16_t functional_mode : 4;
		uint32_t dp_source_sink :1;
		uint32_t gain : 8;
		uint32_t orientation : 1;
		uint32_t reserved1 : 4;
		uint32_t data_index : 7;
		uint32_t end_of_message : 1;
		uint32_t reserved2: 2;
		uint32_t reserved3 : 8;
	} PACK_STRUCT set_retimer_mode;
	struct {
        uint8_t connector_number : 7;
        uint8_t sink_path : 1;
        uint8_t reserved[5];
	} PACK_STRUCT set_sink_path;
#endif
} PACK_STRUCT ucsi_ctrl_details_t;

/**
 * The structure of the memory locations used to pass information between LPM/OPM/PPM.
 * See Sec 3 of UCSI specification.
 */
#if UCSI_REV_1_x_ENABLE
typedef struct {
    uint16_t version;
    uint16_t reserved;
    struct {
        uint8_t reserved1              : 1;
        uint8_t connector_change       : 7;
        uint8_t data_length;
        uint8_t reserved2;
        uint8_t indicators;
    } PACK_STRUCT cci;
    struct {
        uint8_t command;
        uint8_t data_length;
        ucsi_ctrl_details_t details;
    } control;
    uint8_t message_in[16];
    uint8_t message_out[16];
} ucsi_reg_space;
#elif UCSI_REV_2_x_ENABLE
typedef struct {
    uint16_t version;
    uint16_t reserved;
    struct {
        uint8_t reserved1              : 1;
        uint8_t connector_change       : 7;
        uint8_t data_length;
        uint16_t indicators;
    } PACK_STRUCT cci;
    struct {
        uint8_t command;
        uint8_t data_length;
        ucsi_ctrl_details_t details;
    } control;
    uint8_t message_in[256];
    uint8_t message_out[256];
} ucsi_reg_space;
#endif

/* UCSI Register */
#define UCSI_REG_VERSION                           (0x00)
#define UCSI_REG_CCI                               (0x04)
#define UCSI_REG_CONTROL                           (0x08)
#define UCSI_REG_MESSAGE_IN                        (0x10)
#if UCSI_REV_1_x_ENABLE
#define UCSI_REG_MESSAGE_OUT                       (0x20)
#elif UCSI_REV_2_x_ENABLE
#define UCSI_REG_MESSAGE_OUT                       (0x120)
#endif

/* UCSI Data length for different commands */
#if UCSI_REV_1_x_ENABLE
#define MAX_DATA_LENGTH                            (0x10)
#define UCSI_GET_CAPABILITY_STATUS_DATA_LENGTH     (0x10)
#define UCSI_GET_CONNECTOR_CAPABILITY_DATA_LENGTH  (0x02)
#define UCSI_GET_CABLE_PROPERTY_DATA_LENGTH        (0x05)
#define UCSI_GET_CONNECTOR_STATUS_DATA_LENGTH      (0x09)
#define UCSI_GET_ERROR_STATUS_DATA_LENGTH          (0x10)
#elif UCSI_REV_2_x_ENABLE
#define MAX_DATA_LENGTH                            (0x100)
#define UCSI_GET_CAPABILITY_STATUS_DATA_LENGTH     (0x10)
#define UCSI_GET_CONNECTOR_CAPABILITY_DATA_LENGTH  (0x04)
#define UCSI_GET_CABLE_PROPERTY_DATA_LENGTH        (0x05)
#define UCSI_GET_CONNECTOR_STATUS_DATA_LENGTH      (0x0C)
#define UCSI_GET_ERROR_STATUS_DATA_LENGTH          (0x10)
#define UCSI_GET_PDO_LENGTH                        (0x10)
#define UCSI_GET_PD_MESSAGE_LENGTH                 (0x06)
#define UCSI_GET_ATTENTION_VDO_LENGTH              (0x0B)
#endif

/**
 * Set of UCSI Connector Status Register. The structure of the memory locations
 * used to pass information between LPM/OPM/PPM.
 * See Table 4.42 of UCSI specification.
 */
#if UCSI_REV_1_x_ENABLE
typedef struct {
    uint16_t connector_status_change;
    uint16_t power_op_mode : 3;
    uint16_t connect_status : 1;
    uint16_t power_direction : 1;
    uint16_t connector_partner_flags : 8;
    uint16_t connector_partner_type : 3;
    uint32_t rdo;
    uint8_t  battery_charging_status : 2;
    uint8_t  provider_caps_limited_reason : 4;
    uint8_t  reserved : 2;
} PACK_STRUCT ucsi_connector_status_t;
#elif UCSI_REV_2_x_ENABLE
typedef struct {
    uint16_t connector_status_change;
    uint16_t power_op_mode : 3;
    uint16_t connect_status : 1;
    uint16_t power_direction : 1;
    uint16_t connector_partner_flags : 8;
    uint16_t connector_partner_type : 3;
    uint32_t rdo;
    uint8_t  battery_charging_status : 2;
    uint8_t  provider_caps_limited_reason : 4;
    uint16_t bdcPDVersion_OpMOde;
    uint8_t orentation : 1;
    uint8_t sink_path_status : 1;
    uint8_t rcp_status : 1;
    uint8_t reserve : 5;
} PACK_STRUCT ucsi_connector_status_t;
#endif
/**
 * UCSI notification mask definitions.
 * This enumeration lists the various mask values that control the reporting of UCSI events.
 */
#if UCSI_REV_1_x_ENABLE
typedef enum {
    UCSI_EVT_CMD_COMPLETED                  = 0x0001u,
    UCSI_EVT_EXT_SUPPLY_CHANGED             = 0x0002u,
    UCSI_EVT_POM_CHANGED                    = 0x0004u,
    UCSI_EVT_RSVD1                          = 0x0008u,
    UCSI_EVT_RSVD2                          = 0x0010u,
    UCSI_EVT_CAP_CHANGED                    = 0x0020u,
    UCSI_EVT_PD_CONTRACT_CHANGED            = 0x0040u,
    UCSI_EVT_HARD_RESET_COMPLETE            = 0x0080u,
    UCSI_EVT_SUPPORTED_CAM_CHANGED          = 0x0100u,
    UCSI_EVT_BATT_CHARGING_STATUS_CHANGED   = 0x0200u,
    UCSI_EVT_RSVD3                          = 0x0400u,
    UCSI_EVT_PARTNER_STATUS_CHANGED         = 0x0800u,
    UCSI_EVT_POWER_DIRECTION_CHANGED        = 0x1000u,
    UCSI_EVT_RSVD4                          = 0x2000u,
    UCSI_EVT_CONNECT_CHANGED                = 0x4000u,
    UCSI_EVT_ERROR                          = 0x8000u
} ucsi_evmask_t;
#elif UCSI_REV_2_x_ENABLE
typedef enum {
    UCSI_EVT_CMD_COMPLETED                  = 0x00001u,
    UCSI_EVT_EXT_SUPPLY_CHANGED             = 0x00002u,
    UCSI_EVT_POM_CHANGED                    = 0x00004u,
    UCSI_EVT_ATTENTION                      = 0x00008u,
    UCSI_EVT_FW_UPDATE_REQUEST              = 0x00010u,
    UCSI_EVT_CAP_CHANGED                    = 0x00020u,
    UCSI_EVT_PD_CONTRACT_CHANGED            = 0x00040u,
    UCSI_EVT_HARD_RESET_COMPLETE            = 0x00080u,
    UCSI_EVT_SUPPORTED_CAM_CHANGED          = 0x00100u,
    UCSI_EVT_BATT_CHARGING_STATUS_CHANGED   = 0x00200u,
    UCSI_EVT_RSVD3                          = 0x00400u,
    UCSI_EVT_PARTNER_STATUS_CHANGED         = 0x00800u,
    UCSI_EVT_POWER_DIRECTION_CHANGED        = 0x01000u,
    UCSI_EVT_RSVD4                          = 0x02000u,
    UCSI_EVT_CONNECT_CHANGED                = 0x04000u,
    UCSI_EVT_ERROR                          = 0x08000u,
	UCSI_EVT_SINK_PATH_STATUS_CHANGE        = 0x10000u
} ucsi_evmask_t;
#endif

/* This enumeration holds CCI status indicator.*/
#if UCSI_REV_1_x_ENABLE
typedef enum {
    UCSI_STS_IGNORE             = 0x00,
    UCSI_STS_RESERVED           = 0x01,
    UCSI_STS_NOT_SUPPORTED      = 0x02,
    UCSI_STS_CANCEL_COMPLETED   = 0x04,
    UCSI_STS_RESET_COMPLETED    = 0x08,
    UCSI_STS_BUSY               = 0x10,
    UCSI_STS_ACK_CMD            = 0x20,
    UCSI_STS_ERROR              = 0x40,
    UCSI_STS_CMD_COMPLETED      = 0x80,
} ucsi_status_indicator_t;
#elif UCSI_REV_2_x_ENABLE
typedef enum {
    UCSI_STS_IGNORE              = 0x0000,
    UCSI_STS_RESERVED1           = 0x0001,
	UCSI_STS_RESERVED2           = 0x0002,
	UCSI_STS_RESERVED3           = 0x0004,
	UCSI_STS_RESERVED4           = 0x0008,
	UCSI_STS_RESERVED5           = 0x0010,
	UCSI_STS_RESERVED6           = 0x0020,
	UCSI_STS_RESERVED7           = 0x0040,
	UCSI_STS_SECURITY_REQUEST    = 0x0080,
	UCSI_TST_FW_UPDATE_REQUEST   = 0x0100,
    UCSI_STS_NOT_SUPPORTED       = 0x0200,
    UCSI_STS_CANCEL_COMPLETED    = 0x0400,
    UCSI_STS_RESET_COMPLETED     = 0x0800,
    UCSI_STS_BUSY                = 0x1000,
    UCSI_STS_ACK_CMD             = 0x2000,
    UCSI_STS_ERROR               = 0x4000,
    UCSI_STS_CMD_COMPLETED       = 0x8000,
} ucsi_status_indicator_t;
#endif

/* This enumeration holds PPM Controller Commands. */
typedef enum
{
    UCSI_CMD_RESERVED = 0x00u,          /* Command Code = 0x00 */
    UCSI_CMD_PPM_RESET,                 /* Command Code = 0x01 */
    UCSI_CMD_CANCEL,                    /* Command Code = 0x02 */ 
    UCSI_CMD_CONNECTOR_RESET,           /* Command Code = 0x03 */
    UCSI_CMD_ACK_CC_CI,                 /* Command Code = 0x04 */
    UCSI_CMD_SET_NOTIFICATION_ENABLE,   /* Command Code = 0x05 */
    UCSI_CMD_GET_CAPABILITY,            /* Command Code = 0x06 */
    UCSI_CMD_GET_CONNECTOR_CAPABILITY,  /* Command Code = 0x07 */
    UCSI_CMD_SET_CCOM,                  /* Command Code = 0x08 */
    UCSI_CMD_SET_UOR,                   /* Command Code = 0x09 */
    UCSI_CMD_SET_PDM,                   /* Command Code = 0x0A */
    UCSI_CMD_SET_PDR,                   /* Command Code = 0x0B */
    UCSI_CMD_GET_ALTERNATE_MODES,       /* Command Code = 0x0C */
    UCSI_CMD_GET_CAM_SUPPORTED,         /* Command Code = 0x0D */
    UCSI_CMD_GET_CURRENT_CAM,           /* Command Code = 0x0E */
    UCSI_CMD_SET_NEW_CAM,               /* Command Code = 0x0F */
    UCSI_CMD_GET_PDOS,                  /* Command Code = 0x10 */
    UCSI_CMD_GET_CABLE_PROPERTY,        /* Command Code = 0x11 */
    UCSI_CMD_GET_CONNECTOR_STATUS,      /* Command Code = 0x12 */
    UCSI_CMD_GET_ERROR_STATUS,          /* Command Code = 0x13 */
    UCSI_CMD_SET_POWER_LEVEL,           /* Command Code = 0x14 */
	UCSI_CMD_GET_PD_MESSAGE,            /* Command Code = 0x15 */
	UCSI_CMD_GET_ATTENTION_VDO,         /* Command Code = 0x16 */
	UCSI_CMD_GET_CAM_CS,                /* Command Code = 0x18 */
	UCSI_CMD_LPM_FW_UPDATE_REQUEST,     /* Command Code = 0x19 */
	UCSI_CMD_SECURITY_REQUEST,          /* Command Code = 0x1A */
	UCSI_CMD_SET_RETIMER_MODE,          /* Command Code = 0x1B */
	UCSI_CMD_SET_SINK_PATH,             /* Command Code = 0x1C */
	UCSI_CMD_SET_PDO,                   /* Command Code = 0x1D */
	UCSI_CMD__READ_POWER_LEVEL,         /* Command Code = 0x1E */
	UCSI_CMD_CHUNKING_SUPPORT           /* Command Code = 0x1F */
} ucsi_cmd_t;

/* Struct to hold UCSI Get Capability Data. */
typedef struct {
    uint32_t bmAttributes;
    uint8_t  bNumConnectors;
    uint32_t bmOptionalFeatures : 24;
    uint32_t bNumAltModes : 8;
    uint8_t  reserved;
    uint16_t bcdBCVersion;
    uint16_t bcdPDVersion;
    uint16_t bcdUSBTypeCVersion;
} ucsi_capability_t;

/* Struct to hold UCSI Get Cable Property Data. */
typedef struct {
    uint16_t bmSpeedSupported;
    uint8_t  bCurrentCapability;
    uint8_t  VBUSInCable : 1;
    uint8_t  CableType : 1;
    uint8_t  Directionality : 1;
    uint8_t  PlugEndType : 2;
    uint8_t  ModeSupport : 1;
    uint8_t  Reserved : 2;
    uint8_t  Latency : 4;
    uint8_t  Reserved2 : 4;
} ucsi_cable_property_t;

/* Struct to hold UCSI Get Connector Capability Data. */
typedef struct {
    uint8_t opertaionMode;
    uint8_t  provider : 1;
    uint8_t  consumer : 1;
    uint8_t  swap_to_dfp : 1;
    uint8_t  swap_to_ufp : 1;
    uint8_t  swap_to_src : 1;
    uint8_t  swap_to_snk : 1;
    uint8_t  extended_opmode;
    uint8_t  misc_capabilities : 4;
    uint8_t  rcp_support : 1;
    uint8_t  Reserved : 5;
} ucsi_connector_capability_t;

/* The Battery Charging Specification Release Number supported by the PPM in BCD format. */
#define UCSI_BC_1_2_VERSION                        (0x0120)

/* The USB Power Delivery Specification 3.0 Release Number in BCD format. */
#define UCSI_USB_PD_3_0_VERSION                    (0x0300)

/* The USB Power Delivery Specification 2.0 Release Number in BCD format. */
#define UCSI_USB_PD_2_0_VERSION                    (0x0200)

/* The USB Type C 1.3 Release Number in BCD format. */
#define UCSI_USB_TYPE_C_1_3_VERSION                (0x0130)

/* The USB Type C 1.2 Release Number in BCD format. */
#define UCSI_USB_TYPE_C_1_2_VERSION                (0x0120)

/* bmPowerSource (Get Capability cmd) macros */
#define UCSI_TYPEC_DISABLED_SUPPORTED              (0x0001)
#define UCSI_BATTERY_CHARGING                      (0x0002)
#if UCSI_BATTERY_CHARGING_ENABLE
#define UCSI_BC_SUPPORTED                          (0x0002)
#else
#define UCSI_BC_SUPPORTED                          (0x0000)
#endif /* UCSI_BATTERY_CHARGING_ENABLE */
#define UCSI_USB_PD_SUPPORTED                      (0x0004)
#define UCSI_TYPE_C_RP_SUPPORTED                   (0x0040)
#define UCSI_AC_SUPPLY_SUPPORTED                   (0x0100)
#define UCSI_OTHER_SUPPLY_SUPPORTED                (0x0400)
#define UCSI_PD_SUPPLY_SUPPORTED                   (0x4000)
 
#define ALL_ATTRIBUTES_SUPPORTED                   (UCSI_TYPEC_DISABLED_SUPPORTED | UCSI_BC_SUPPORTED | UCSI_USB_PD_SUPPORTED |\
                                                   UCSI_TYPE_C_RP_SUPPORTED | UCSI_AC_SUPPLY_SUPPORTED | UCSI_OTHER_SUPPLY_SUPPORTED )

/* Optional features macros.*/
#define UCSI_SET_CCOM_SUPPORTED                    (0x0001)
#define UCSI_SET_PWR_LEVEL_SUPPORTED               (0x0002)
#define UCSI_ALT_MODE_DETAILS_SUPPORTED            (0x0004)
#define UCSI_ALT_MODE_OVERRIDE_SUPPORTED           (0x0008)
#define UCSI_PDO_DETAILS_SUPPORTED                 (0x0010)
#define UCSI_CABLE_DETAILS_SUPPORTED               (0x0020)
#define UCSI_EXT_SUPPLY_NOTFN_SUPPORTED            (0x0040)
#define UCSI_HARD_RESET_NOTFN_SUPPORTED            (0x0080)
#define UCSI_GET_PD_MESSAGE_SUPPORTED              (0x0100)
#if UCSI_REV_2_x_ENABLE
#define UCSI_GET_ATT_VDO_SUPPORTED                 (0x0200)
#define UCSI_FW_UPDATE_REQUEST_SUPPORTED           (0x0400)
#define UCSI_NEGO_PWR_LEVEL_SUPPORTED              (0x0800)
#define UCSI_SECURITY_REQUEST_SUPPORTED            (0x1000)
#define UCSI_SET_RETIMER_MODE_SUPPORTED            (0x2000)
#endif

#if UCSI_ALT_MODE_ENABLED
#define UCSI_OPTIONAL_FEATURES                    (UCSI_ALT_MODE_DETAILS_SUPPORTED | UCSI_ALT_MODE_OVERRIDE_SUPPORTED|\
											       UCSI_PDO_DETAILS_SUPPORTED | UCSI_CABLE_DETAILS_SUPPORTED |\
												   UCSI_CABLE_DETAILS_SUPPORTED | UCSI_EXT_SUPPLY_NOTFN_SUPPORTED |\
												   UCSI_HARD_RESET_NOTFN_SUPPORTED)  /* Not support UOM for now "SET_UOM_SUPPORTED" */
#else
/* remove the alt mode supp */
#define UCSI_OPTIONAL_FEATURES                    (UCSI_PDO_DETAILS_SUPPORTED | UCSI_CABLE_DETAILS_SUPPORTED | UCSI_CABLE_DETAILS_SUPPORTED |\
												   UCSI_EXT_SUPPLY_NOTFN_SUPPORTED | UCSI_HARD_RESET_NOTFN_SUPPORTED | UCSI_SET_CCOM_SUPPORTED)
#endif

/* Error Information macros.*/
#define UCSI_ERR_UNRECOGNIZED_CMD                 (0x0001)
#define UCSI_ERR_UNKNOWN_CONNECTOR                (0x0002)
#define UCSI_ERR_INVALID_PARAMS                   (0x0004)
#define UCSI_ERR_INCOMPAT_PARTNER                 (0x0008)
#define UCSI_ERR_PD_COMM_ERROR                    (0x0010)
#define UCSI_ERR_BATTERY_DEAD                     (0x0020)
#define UCSI_ERR_CONTRACT_NEG_FAILURE             (0x0040)
#define UCSI_ERR_OVERCURRENT                      (0x0080)
#define UCSI_ERR_UNDEFINED                        (0x0100)
#define UCSI_ERR_PORT_PARTNER_REJECT_SWAP         (0x0200)
#define UCSI_ERR_HARD_RESET                       (0x0400)
#define UCSI_ERR_PPM_POLICY_CONFLICT              (0x0800)
#define UCSI_ERR_SWAP_REJECTED                    (0x1000)
#if UCSI_REV_2_x_ENABLE
#define UCSI_ERR_RCP                              (0x2000)
#define UCSI_ERR_SET_SINK_PATH_REJECTED           (0x4000)
#define UCSI_ERR_RESERVED_AND_SHALL_BE_SET2ZERO   (0x8000)
#endif

/* Source Capabilities Type macros*/
#define UCSI_CUR_SUPPORTED_SRC_CAPS                (0x00)
#define UCSI_ADVERTISED_SRC_CAPS                   (0x01)
#define UCSI_MAX_SUPPORTED_SRC_CAPS                (0x02)
#define UCSI_UNKNOWN_SRC_CAPS                      (0x03)

/* Battery Charging Status macros*/
#define UCSI_BATT_STS_NOT_CHARGING                 (0)
#define UCSI_BATT_STS_NOMINAL_CHARGING             (1)
#define UCSI_BATT_STS_SLOW_CHARGING                (2)
#define UCSI_BATT_STS_VERY_SLOW_CHARGING           (3)

/* GET_CONNECTOR_STATUS 'Power Operation Mode' types */
#define UCSI_PWR_OP_MODE_NO_CONSUMER               (0x00)
#define UCSI_PWR_OP_MODE_TYPE_C_DEFAULT            (0x01)
#define UCSI_PWR_OP_MODE_BC_1_2                    (0x02)
#define UCSI_PWR_OP_MODE_USB_PD                    (0x03)
#define UCSI_PWR_OP_MODE_TYPE_C_1_5A               (0x04)
#define UCSI_PWR_OP_MODE_TYPE_C_3_0A               (0x05)
#if UCSI_REV_2_x_ENABLE
#define UCSI_PWR_OP_MODE_TYPE_C_5_0A               (0x06)
#endif

/* Operation Mode (Get Connector Capability) macros*/
#define UCSI_CONN_CAP_DFP_ONLY                     (0x0001)
#define UCSI_CONN_CAP_UFP_ONLY                     (0x0002)
#define UCSI_CONN_CAP_DRP                          (0x0004)
#define UCSI_CONN_CAP_ANALOG_ACC_SUPP              (0x0008)
#define UCSI_CONN_CAP_DEBUG_ACC_SUPP               (0x0010)
#define UCSI_CONN_CAP_USB_2_0_SUPP                 (0x0020)
#define UCSI_CONN_CAP_USB_3_0_SUPP                 (0x0040)
#define UCSI_CONN_CAP_ALT_MODE_SUPP                (0x0080)
#define UCSI_CONN_CAP_PWR_SRC_SUPP                 (0x0100)
#define UCSI_CONN_CAP_PWR_SNK_SUPP                 (0x0200)
#define UCSI_CONN_CAP_SWAP_TO_DFP                  (0x0400)
#define UCSI_CONN_CAP_SWAP_TO_UFP                  (0x0800)
#define UCSI_CONN_CAP_SWAP_TO_SRC                  (0x1000)
#define UCSI_CONN_CAP_SWAP_TO_SNK                  (0x2000)

#define UCSI_ALT_MODE_SVID_SIZE                    (2u)
#define UCSI_ALT_MODE_MID_SIZE                     (4u)

/* Connector Partner Type (Get Connector Status) macros*/
#define UCSI_CONN_PARTNER_DFP                      (0x01)
#define UCSI_CONN_PARTNER_UFP                      (0x02)
#define UCSI_CONN_PARTNER_PWR_CBL_NO_UFP           (0x03)
#define UCSI_CONN_PARTNER_PWR_CBL_UFP              (0x04)
#define UCSI_CONN_PARTNER_DBG_ACC                  (0x05)
#define UCSI_CONN_PARTNER_AUD_ACC                  (0x06)

/* Connector Partner Flags macros*/
#define UCSI_CONN_PARTNER_FLAG_USB                 (0x01)
#define UCSI_CONN_PARTNER_FLAG_ALT_MODE            (0x02)

/* Device Policy Manager swap command macros.*/
#define UCSI_DPM_DR_SWAP_RESP_MASK                 (0x03)
#define UCSI_DPM_DR_SWAP_RESP_POS                  (0x00)
#define UCSI_DPM_PR_SWAP_RESP_MASK                 (0x0C)
#define UCSI_DPM_PR_SWAP_RESP_POS                  (0x02)

#define UCSI_PWR_LEVEL_SINK                        (0x00)
#define UCSI_PWR_LEVEL_SOURCE                      (0x01)

#define UCSI_PWR_LEVEL_CUR_PPM_DEFULT              (0x00)
#define UCSI_PWR_LEVEL_CUR_3_0_A                   (0x01)
#define UCSI_PWR_LEVEL_CUR_1_5_A                   (0x02)
#define UCSI_PWR_LEVEL_CUR_0_9_A                   (0x03)

#endif /* AMD_CRB_DRIVERS_UCSI_INTERNAL_H_ */

#define UCSI_CABLE_CURR_3A	                 (60u)           /* Type-C cable current capacity of 3A represented in 50 mA units. */
#define UCSI_CABLE_CURR_5A	                 (100u)          /* Type-C cable current capacity of 5A represented in 50 mA units. */

#define UCSI_CABLE_SPEED_480MBPS             (0x7820)        /* USB 2.0 Type-C cable data speed capability (480 Mbps) */
#define UCSI_CABLE_SPEED_5GBPS               (0x0017)        /* USB 3.0 Type-C cable data speed capability (5 Gbps). */
#define UCSI_CABLE_SPEED_10GBPS              (0x002B)        /* USB 3.1 Type-C cable data speed capability (10 Gbps). */

#define UCSI_CABLE_VDO_DIRECTION             (0x0780)        /* Mask for directionality flags in USB-PD Cable VDO. */

#define UCSI_ALT_MODES_RECIPIENT_CONNECTOR   (0u)            /* Connector value for Alternate mode recipient. */
#define UCSI_ALT_MODES_RECIPIENT_SOP         (1u)            /* SOP value for Alternate mode recipient. */
#define UCSI_ALT_MODES_RECIPIENT_SOP_PRIME   (2u)            /* SOP' value for Alternate mode recipient. */
#define UCSI_ALT_MODES_RECIPIENT_SOP_DPRIME  (3u)            /* SOP'' value for Alternate mode recipient. */

#define UCSI_VDM_DISCOVER_ID                 (1u)            /* VDM command value for Discover Identity. */
#define UCSI_VDM_DISCOVER_SVID               (2u)            /* VDM command value for Discover SVID */
#define UCSI_VDM_DISCOVER_MODES              (3u)            /* VDM command value for Discover Modes */

#define UCSI_SRC_CUR_LEVEL_DEF               (0x00)          /* Current source value: Type-C default (900 mA). */
#define UCSI_SRC_CUR_LEVEL_1_5A              (0x01)          /* Current source value: 1.5 A */
#define UCSI_SRC_CUR_LEVEL_3A                (0x02)          /* Current source value: 3 A */
#define UCSI_SRC_CUR_LEVEL_PD                (0x03)          /* Current source value: PD */

#define UCSI_SET_RP_PPM_DEFAULT              (0)             /* UCSI command to set PPM power level to default value. */
#define UCSI_SET_RP_3A                       (1)             /* UCSI command to set PPM power level to 3 A. */
#define UCSI_SET_RP_1_5A                     (2)             /* UCSI command to set PPM power level to 1.5 A. */
#define UCSI_SET_RP_DEF                      (3)             /* UCSI command to set PPM power level to Type-C default. */

#define UCSI_POM_NO_CONSUMER                 (0x00)          /* Power Operation Mode (POM) of connector: reserved. */
#define UCSI_POM_CUR_LEVEL_DEF               (0x01)          /* Power Operation Mode (POM) of connector: Type-C default. */
#define UCSI_POM_BC                          (0x02)          /* Power Operation Mode (POM) of connector: BC 1.2 */
#define UCSI_POM_PD                          (0x03)          /* Power Operation Mode (POM) of connector: USB-PD. */

#define UCSI_POM_CUR_LEVEL_1_5A              (0x04)          /* Power Operation Mode (POM) of connector: Type-C 1.5 A. */
#define UCSI_POM_CUR_LEVEL_3A                (0x05)          /* Power Operation Mode (POM) of connector: Type-C 3 A. */

#define UCSI_STARTED                         (0x00)          /* Status value to indicate UCSI is started. */
#define UCSI_CMD_IN_PROGRESS                 (0x01)          /* Status value to indicate UCSI Command in progress. */
#define UCSI_EVENT_PENDING                   (0x02)          /* Status value to indicate UCSI event pending. */

/* [] END OF FILE */
