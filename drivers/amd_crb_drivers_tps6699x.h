/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_TPS6699X_H_
#define AMD_CRB_DRIVERS_TPS6699X_H_

#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_ucsi.h"
#include "amd_crb_drivers_ucsi_internal.h"
#include "system.h"

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

#define PACK_STRUCT                      __attribute__((packed, aligned(1)))

/*
 * TI PD controller registers define
 * Reserve regs will not in this list
 */
#define TIPD_REG_VID                        (0x00)
#define TIPD_REG_DID                        (0x01)
#define TIPD_REG_PROTO_VER                  (0x02)
#define TIPD_REG_MODE                       (0x03)
#define TIPD_REG_TYPE                       (0x04)
#define TIPD_REG_UID                        (0x05)
#define TIPD_REG_CUSTOMER_USER              (0x06)
 
#define TIPD_REG_CMD1                       (0x08)
#define TIPD_REG_DATA1                      (0x09)

#define TIPD_REG_DEVICE_CAP                 (0x0D)
#define TIPD_REG_VERSION                    (0x0F)
#define TIPD_REG_CMD2                       (0x10)
#define TIPD_REG_DATA2                      (0x11)

#define TIPD_REG_INT_EVENT1                 (0x14)
#define TIPD_REG_INT_EVENT2                 (0x15)
#define TIPD_REG_INT_MASK1                  (0x16)
#define TIPD_REG_INT_MASK2                  (0x17)
#define TIPD_REG_INT_CLEAR1                 (0x18)
#define TIPD_REG_INT_CLEAR2                 (0x19)
#define TIPD_REG_STATUS                     (0x1A)

#define TIPD_REG_SX_CONFIG                  (0x1F)

#define TIPD_REG_SX_APP_CONFIG              (0x20)
#define TIPD_REG_DIS_SVID                   (0x21)
#define TIPD_REG_CONN_M_STS                 (0x22)
#define TIPD_REG_USB_CONFIG                 (0x23)
#define TIPD_REG_USB_STS                    (0x24)
#define TIPD_REG_CONN_M_CTRL                (0x25)
#define TIPD_REG_PWR_PATH_STATUS            (0x26)
#define TIPD_REG_GL_SYS_CONFIG              (0x27)
#define TIPD_REG_PORT_CONFIG                (0x28)
#define TIPD_REG_PORT_CTL                   (0x29)

#define TIPD_REG_BOOT                       (0x2D)
#define TIPD_REG_BUILD_DES                  (0x2E)
#define TIPD_REG_DEV_INFO                   (0x2F)
#define TIPD_REG_RX_SRC_CAP                 (0x30)
#define TIPD_REG_RX_SINK_CAP                (0x31)
#define TIPD_REG_TX_SRC_CAP                 (0x32)
#define TIPD_REG_TX_SNK_CAP                 (0x33)
#define TIPD_REG_ACTIVE_PDO                 (0x34)
#define TIPD_REG_ACTIVE_RDO                 (0x35)
#define TIPD_REG_AUTO_NEG_SINK              (0x37)
#define TIPD_REG_SPM_CLIENT_CTRL            (0x3C)
#define TIPD_REG_SPM_CLIENT_STS             (0x3D)

#define TIPD_REG_PD_STATUS                  (0x40)
#define TIPD_REG_PD_30_STATUS               (0x41)
#define TIPD_REG_PD_30_CONFIG               (0x42)
#define TIPD_REG_DELAY_CONFIG               (0x43)

#define TIPD_REG_TX_IDENTITY                (0x47)
#define TIPD_REG_RX_IDENTITY_SOP            (0x48)
#define TIPD_REG_RX_IDENTITY_SOPP           (0x49)
#define TIPD_REG_USER_VID_CONFIG            (0x4A)

#define TIPD_REG_RX_ATT                     (0x4E)
#define TIPD_REG_RX_VDM                     (0x4F)
#define TIPD_REG_DP_SID_CONFIG              (0x51)
#define TIPD_REG_INTEL_VID_CONFIG           (0x52)

#define TIPD_REG_USER_VID_STATUS            (0x57)
#define TIPD_REG_DP_SID_STATUS              (0x58)
#define TIPD_REG_INTEL_VID_STATUS           (0x59)

#define TIPD_REG_RET_DEBUG_MODE             (0x5D)

#define TIPD_REG_DATA_STATUS                (0x5F)
#define TIPD_REG_RX_USER_VID_ATT_VDM        (0x60)
#define TIPD_REG_RX_USER_VID_OTS_VDM        (0x61)
#define TIPD_REG_BIN_INDICES                (0x62) 
#define TIPD_REG_MIPI_VID_STATUS            (0x63)
#define TIPD_REG_I2C_MASTER_CONFIG          (0x64)

#define TIPD_REG_TYPEC_STATE                (0x69)
#define TIPD_REG_ADC_RESULTS                (0x6A)

#define TIPD_REG_EVENT_CTL                  (0x6C)

#define TIPD_REG_SLEEP_CONFIG               (0x70)
#define TIPD_REG_GPIO_STATUS                (0x72)
#define TIPD_REG_TX_MIDB_SOP                (0x73)
#define TIPD_REG_RX_ADO                     (0x74)
#define TIPD_REG_TX_ADO                     (0x75)
#define TIPD_REG_TX_SCEDB                   (0x77)
#define TIPD_REG_TX_SDB                     (0x79)
#define TIPD_REG_TX_BSDO                    (0x7B)
#define TIPD_REG_TX_BCDB                    (0x7D)
#define TIPD_REG_TX_MIDB_DSOP               (0x7F)

#define TIPD_REG_UUID                       (0x80)

#define TIPD_REG_EPR_CONFIG                 (0x97)

#define TIPD_REG_GPIO_P0                    (0xA0)
#define TIPD_REG_GPIO_P1                    (0xA1)
#define TIPD_REG_GPIO_EVT_CONFIG            (0xA3)

/*
 * TI PD controller registers define
 * Reserve regs will not in this list
 */
#define TIPD_REG_VID_LEN                    (4)  /* 0x00 */
#define TIPD_REG_DID_LEN                    (4)  /* 0x01 */
#define TIPD_REG_PROTO_VER_LEN              (4)  /* 0x02 */
#define TIPD_REG_MODE_LEN                   (4)  /* 0x03 */
#define TIPD_REG_TYPE_LEN                   (4)  /* 0x04 */
#define TIPD_REG_UID_LEN                    (16) /* 0x05 */
#define TIPD_REG_CUSTOMER_USER_LEN          (8)  /* 0x06 */

#define TIPD_REG_CMD1_LEN                   (4)  /* 0x08 */
#define TIPD_REG_DATA1_LEN                  (64) /* 0x09 */

#define TIPD_REG_DEVICE_CAP_LAN             (4)  /* 0x0D */
#define TIPD_REG_VERSION_LEN                (4)  /* 0x0F */
#define TIPD_REG_CMD2_LEN                   (4)  /* 0x10 */
#define TIPD_REG_DATA2_LEN                  (64) /* 0x11 */

#define TIPD_REG_INT_EVENT1_LEN             (11) /* 0x14 */
#define TIPD_REG_INT_EVENT2_LEN             (11) /* 0x15 */
#define TIPD_REG_INT_MASK1_LEN              (11) /* 0x16 */
#define TIPD_REG_INT_MASK2_LEN              (11) /* 0x17 */
#define TIPD_REG_INT_CLEAR1_LEN             (11) /* 0x18 */
#define TIPD_REG_INT_CLEAR2_LEN             (11) /* 0x19 */
#define TIPD_REG_STATUS_LEN                 (5)  /* 0x1A */

#define TIPD_REG_SX_CONFIG_LEN              (24) /* 0x1F */

#define TIPD_REG_SX_APP_CONFIG_LEN          (2)  /* 0x20 */
#define TIPD_REG_DIS_SVID_LEN               (33) /* 0x21 */
#define TIPD_REG_CONN_M_STS_LEN             (1)  /* 0x22 */
#define TIPD_REG_USB_CONFIG_LEN             (4)  /* 0x23 */
#define TIPD_REG_USB_STS_LEN                (9)  /* 0x24 */
#define TIPD_REG_CONN_M_CTRL_LEN            (8)  /* 0x25 */
#define TIPD_REG_PWR_PATH_STATUS_LEN        (5)  /* 0x26 */
#define TIPD_REG_GL_SYS_CONFIG_LEN          (15) /* 0x27 */
#define TIPD_REG_PORT_CONFIG_LEN            (3)  /* 0x28 */
#define TIPD_REG_PORT_CTL_LEN               (6)  /* 0x29 */

#define TIPD_REG_BOOT_STATUS_LEN            (5)  /* 0x2D */
#define TIPD_REG_BUILD_DES_LEN              (49) /* 0x2E */
#define TIPD_REG_DEV_INFO_LEN               (40) /* 0x2F */
#define TIPD_REG_RX_SRC_CAP_LEN             (53) /* 0x30 */
#define TIPD_REG_RX_SINK_CAP_LEN            (53) /* 0x31 */
#define TIPD_REG_TX_SRC_CAP_LEN             (63) /* 0x32 */
#define TIPD_REG_TX_SINK_CAP_LEN            (53) /* 0x33 */
#define TIPD_REG_ACTIVE_PDO_LEN             (6)  /* 0x34 */
#define TIPD_REG_ACTIVE_RDO_LEN             (12) /* 0x35 */
#define TIPD_REG_AUTO_NEG_SINK_LEN          (24) /* 0x37 */

#define TIPD_REG_PWR_STATUS_LEN             (2)  /* 0x3F */
#define TIPD_REG_PD_STATUS_LEN              (4)  /* 0x40 */
#define TIPD_REG_PD_30_STATUS_LEN           (8)  /* 0x41 */
#define TIPD_REG_PD_30_CONFIG_LEN           (4)  /* 0x42 */
#define TIPD_REG_DELAY_CONFIG_LEN           (28) /* 0x43 */

#define TIPD_REG_TX_IDENTITY_LEN            (49) /* 0x47 */
#define TIPD_REG_RX_IDENTITY_SOP_LEN        (25) /* 0x48 */
#define TIPD_REG_RX_IDENTITY_SOPP_LEN       (25) /* 0x49 */
#define TIPD_REG_USER_VID_CONFIG_LEN        (63) /* 0x4A */

#define TIPD_REG_RX_ATT_LEN                 (9)  /* 0x4E */
#define TIPD_REG_RX_VDM_LEN                 (29) /* 0x4F */
#define TIPD_REG_DP_SID_CONFIG_LEN          (6)  /* 0x51 */
#define TIPD_REG_INTEL_VID_CONFIG_LEN       (8)  /* 0x52 */

#define TIPD_REG_USER_VID_STATUS_LEN        (2)  /* 0x57 */
#define TIPD_REG_DP_SID_STATUS_LEN          (37) /* 0x58 */
#define TIPD_REG_INTEL_VID_STATUS_LEN       (11) /* 0x59 */

#define TIPD_REG_RET_DEBUG_MODE_LEN         (4)  /* 0x5D */

#define TIPD_REG_DATA_STATUS_LEN            (5)  /* 0x5F */
#define TIPD_REG_RX_USER_VID_ATT_VDM_LEN    (9)  /* 0x60 */
#define TIPD_REG_RX_USER_VID_OTS_VDM_LEN    (29) /* 0x61 */
#define TIPD_REG_BIN_INDICES_LEN            (8)  /* 0x62 */ 
#define TIPD_REG_I2C_MASTER_CONFIG_LEN      (12) /* 0x64 */

#define TIPD_REG_CCN_PIN_STATUS_LEN         (4)  /* 0x69 */

#define TIPD_REG_EVENT_CTL_LEN              (60) /* 0x6C */

#define TIPD_REG_SLEEP_CONFIG_LEN           (1)  /* 0x70 */
#define TIPD_REG_GPIO_STATUS_LEN            (12) /* 0x72 */
#define TIPD_REG_TX_MIDB_SOP_LEN            (22) /* 0x73 */
#define TIPD_REG_RX_ADO_LEN                 (4)  /* 0x74 */
#define TIPD_REG_TX_ADO_LEN                 (4)  /* 0x75 */
#define TIPD_REG_TX_SCEDB_LEN               (15) /* 0x77 */
#define TIPD_REG_TX_SDB_LEN                 (7)  /* 0x79 */
#define TIPD_REG_TX_BSDO_LEN                (16) /* 0x7B */
#define TIPD_REG_TX_BCDB_LEN                (36) /* 0x7D */

#define TIPD_REG_UUID_LEN                   (15) /* 0x80 */

#define TIPD_REG_EPR_CONFIG_LEN             (5)  /* 0x97 */

#define TIPD_REG_GPIO_P0_LEN                (51) /* 0xA0 */
#define TIPD_REG_GPIO_P1_LEN                (30) /* 0xA1 */
#define TIPD_REG_GPIO_EVT_CONFIG_LEN        (45) /* 0xA3 */


/* Event byte 1~4 */
#define TIPD_EVENT0_PD_HARD_RESET           (1 << 1)
#define TIPD_EVENT0_PLUG_INSERT_OR_REMOVAL  (1 << 3)
#define TIPD_EVENT0_PR_SWAP_CPLT            (1 << 4)
#define TIPD_EVENT0_DR_SWAP_CPLT            (1 << 5)
#define TIPD_EVENT0_FR_SWAP_CPLT            (1 << 6)
#define TIPD_EVENT0_SRC_CAP_UPDATE          (1 << 7)
#define TIPD_EVENT0_OVERCURRENT             (1 << 9)
#define TIPD_EVENT0_ATT_RECV                (1 << 10)
#define TIPD_EVENT0_VDM_RECV                (1 << 11)
#define TIPD_EVENT0_NEW_CONTRACT_AS_CONS    (1 << 12)
#define TIPD_EVENT0_NEW_CONTRACT_AS_PROV    (1 << 13)
#define TIPD_EVENT0_SRC_CAP_MSG_RDY         (1 << 14)
#define TIPD_EVENT0_SNK_CAP_MSG_RDY         (1 << 15)
#define TIPD_EVENT0_PR_SWAP_REQ             (1 << 17)
#define TIPD_EVENT0_DR_SWAP_REQ             (1 << 18)
#define TIPD_EVENT0_USB_HOST_PRES           (1 << 20)
#define TIPD_EVENT0_USB_HOST_PRES_NL        (1 << 21)
#define TIPD_EVENT0_PP_SWITCH_CHANGE        (1 << 23)
#define TIPD_EVENT0_PWR_STATUS_UPDATE       (1 << 24)
#define TIPD_EVENT0_DATA_STATUS_UPDATE      (1 << 25)
#define TIPD_EVENT0_STATUS_UPDATE           (1 << 26)
#define TIPD_EVENT0_PD_STATUS_UPDATE        (1 << 27)
#define TIPD_EVENT0_CMD1_CPLT               (1 << 30)
#define TIPD_EVENT0_CMD2_CPLT               (1 << 31)
/* Event byte 5~8 */
#define TIPD_EVENT1_ERROR_DEV_CPT           (1 << 0)
#define TIPD_EVENT1_ERROR_NO_V_O_C          (1 << 1)
#define TIPD_EVENT1_ERROR_PROV_V_O_C_LATER  (1 << 2)
#define TIPD_EVENT1_ERROR_PWR_EVENT         (1 << 3)
#define TIPD_EVENT1_ERROR_MS_GCAP_MSG       (1 << 4)
#define TIPD_EVENT1_ERROR_PROTOCOL_ERROR    (1 << 6)
#define TIPD_EVENT1_ERROR_MSG_DATA          (1 << 7)
#define TIPD_EVENT1_SNK_TRANS_CPLT          (1 << 10)
#define TIPD_EVENT1_PLUG_EARLY_NOTF         (1 << 11)
#define TIPD_EVENT1_PROC_HOT                (1 << 12)
#define TIPD_EVENT1_ERROR_UNABLE_TO_SRC     (1 << 14)
#define TIPD_EVENT1_AM_ENTEY_FAIL           (1 << 16)
#define TIPD_EVENT1_AM_ENTERED_MODE         (1 << 17)  
#define TIPD_EVENT1_DISC_MODE_CPLT          (1 << 19)
#define TIPD_EVENT1_EXIT_MODE_CPLT          (1 << 20)
#define TIPD_EVENT1_DATA_RESET_START        (1 << 21)
#define TIPD_EVENT1_USB_STATUS_UPDATE       (1 << 22)
#define TIPD_EVENT1_CONN_M_UPDATE           (1 << 23)
#define TIPD_EVENT1_USER_VID_ENTERED        (1 << 24)
#define TIPD_EVENT1_USER_VID_EXITED         (1 << 25)
#define TIPD_EVENT1_USER_VID_ATT            (1 << 26)
#define TIPD_EVENT1_USER_VID_OTS            (1 << 27)
#define TIPD_EVENT1_DP_SID_STATUS_UPDATE    (1 << 30)
#define TIPD_EVENT1_INTEL_VID_STATUS_UPDATE (1 << 31)
/* Event byte 9~11 */
#define TIPD_EVENT2_PD3_STATUS_UPDATE       (1 << 0)
#define TIPD_EVENT2_TX_MEM_BUFFER_EMPTY     (1 << 1)
#define TIPD_EVENT2_MB_RD_BUFFER_READY      (1 << 2)
#define TIPD_EVENT2_ACK_TIMEOUT             (1 << 6)
#define TIPD_EVENT2_NOT_SUPP_RECV           (1 << 7)
#define TIPD_EVENT2_AMD_IOMUX_ERROR         (1 << 8)
#define TIPD_EVENT2_MAILBOX_UPDATED         (1 << 9)
#define TIPD_EVENT2_FR_SIGNAL_RECV          (1 << 12)
#define TIPD_EVENT2_CHUNK_RESP_RECV         (1 << 13)
#define TIPD_EVENT2_CHUNK_REQ_RECV          (1 << 14)
#define TIPD_EVENT2_ALERT_MSG_RECV          (1 << 15)
#define TIPD_EVENT2_PATCH_LOADED            (1 << 16)
#define TIPD_EVENT2_RDY_FOR_PATCH           (1 << 17)

/* Register 0x1A status */
#define TIPD_REG_STATUS_CONNSTATUS_NO_CONN           (0)
#define TIPD_REG_STATUS_CONNSTATUS_PORT_DISABLED     (1)
#define TIPD_REG_STATUS_CONNSTATUS_AUDIO_CONN        (2)
#define TIPD_REG_STATUS_CONNSTATUS_DEBUG_CONN        (3)
#define TIPD_REG_STATUS_CONNSTATUS_NO_CONN_RA        (4)
#define TIPD_REG_STATUS_CONNSTATUS_RSVD              (5)
#define TIPD_REG_STATUS_CONNSTATUS_CONN_WO_RA        (6)
#define TIPD_REG_STATUS_CONNSTATUS_CONN_W_RA         (7)
#define TIPD_REG_STATUS_PLUG_ORIEN_CC1               (0)
#define TIPD_REG_STATUS_PLUG_ORIEN_CC2               (1)
#define TIPD_REG_STATUS_PORT_ROLE_SNK                (0)
#define TIPD_REG_STATUS_PORT_ROLE_SRC                (1)
#define TIPD_REG_STATUS_PORT_TYPE_UFP                (0)
#define TIPD_REG_STATUS_PORT_TYPE_DFP                (1)
#define TIPD_REG_STATUS_VBUS_VSF0                    (0)                     
#define TIPD_REG_STATUS_VBUS_VSF5                    (1)
#define TIPD_REG_STATUS_VBUS_VSF_OTS                 (2)
#define TIPD_REG_STATUS_VBUS_OUT_RANGE               (3)
#define TIPD_REG_STATUS_AM_NO                        (0)
#define TIPD_REG_STATUS_AM_SUCCESS                   (1)
#define TIPD_REG_STATUS_AM_UNSUCCESS                 (2)
#define TIPD_REG_STATUS_AM_BOTH                      (3)

/* Patch info */
#define TOTAL_4kBSECTORS_FOR_PATCH                   (4)
#define PATCH_BUNDLE_SIZE                            (64)


/* TI PD command string */
#define TIPD_CMD_ST_ERROR                            ("!CMD")
#define TIPD_CMD_ST_UCSI                             ("UCSI")

#define TIPD_CMD_ST_UCSI_LEN                         (8)
#define TIPD_CMD_ST_ALT_MODE_LEN                     (3)

/* TI PD response */
#define TIPD_RESP_SUCCESS                            (0x00)
#define TIPD_RESP_TIMEOUT                            (0x01)
#define TIPD_RESP_REJECTED                           (0x03)

/* 4CC command type */
#define CMD_TYPE_NULL                                (0)
#define CMD_TYPE_UCSI                                (1)
#define CMD_TYPE_ALT_MODE                            (2)
#define CMD_TYPE_PROGRAM                             (3)
#define CMD_TYPE_RECOVER_EX                          (4)
#define CMD_TYPE_RECOVER_EN                          (5)

/* Other Macros */
#define TIPD_MAX_PACKAGE_LENGTH                      (64)  /* package buffer length */
#define TI_REG_ACCESS_TRTRY_TIME                     (5)   /* retry time for I2C comm */

#define TI_REG_READ                                  (1)
#define TI_REG_WRITE                                 (0)

#define TIPD_4CCPTR_TO_U32(p)                        ( *((uint32_t *)(p)) )
#define TIPD_4CC_nCMD                                TIPD_4CCPTR_TO_U32("!CMD")

#define TIPD_4CC_PTCd_DATA_LENGTH                    (64)

/*** 3.1 0x14 - 0x19 IntEventX, IntMaskX, IntClearX Registers ***/
#define DRV_TPS6699X_INT_EVENT_REGISTER_LENGTH          (11)  /* 0x14, 0x15, 0x16, 0x17 */
#define DRV_TPS6699X_STATUS_REGISTER_LENGTH             (8)   /* 0x1A */
#define DRV_TPS6699X_SET_SX_APP_REGISTER_LENGTH         (2)   /* 0x20 */
#define DRV_TPS6699X_RX_SVID_SOP_REGISTER_LENGTH        (17)  /* 0x21 only read 17 bytes */
#define DRV_TPS6699x_CONN_M_STS_REGISTER_LENGTH         (1)   /* 0x22 */
#define DRV_TPS6699X_USB_CONFIG_REGISTER_LENGTH         (4)   /* 0x23 */
#define DRV_TPS6699x_USB_STATUS_REGISTER_LENGTH         (9)   /* 0x24 */
#define DRV_TPS6699x_USB_M_CTRL_REGISTER_LENGTH         (1)   /* 0x25 */
#define DRV_TPS6699X_POWER_PATH_REGISTER_LENGTH         (5)   /* 0x26 */
#define DRV_TPS6699X_GL_SYS_CONFIG_REGISTER_LENGTH      (14)  /* 0x27 */
#define DRV_TPS6699X_PORT_CONFIG_REGISTER_LENGTH        (6)   /* 0x28 only read first 6 of 17*/
#define DRV_TPS6699X_PORT_CONTROL_REGISTER_LENGTH       (6)   /* 0x29 only read first 4 of 6*/
#define DRV_TPS6699X_BOOT_FLAGS_REGISTER_LENGTH         (4)   /* 0x2D only read first 4 of 52 */
#define DRV_TPS6699X_DEV_INFO_REGISTER_LENGTH           (40)  /* 0x2F */
#define DRV_TPS6699X_RX_SRC_CAP_REGISTER_LENGTH         (29)  /* 0x30 only read first 29 of 53 */
#define DRV_TPS6699X_RX_SNK_CAP_REGISTER_LENGTH         (29)  /* 0x31 only read first 29 of 53 */
#define DRV_TPS6699X_TX_SRC_CAP_REGISTER_LENGTH         (31)  /* 0x32 only read first 31 of 63 */
#define DRV_TPS6699X_TX_SNK_CAP_REGISTER_LENGTH         (29)  /* 0x33 only read first 29 of 53 */
#define DRV_TPS6699X_PD_STATUS_REGISTER_LENGTH          (4)   /* 0x40 */
#define DRV_TPS6699X_TX_VID_REGISTER_LENGTH             (25)  /* 0x47 only read first 25 of 49 */
#define DRV_TPS6699X_RX_VID_SOP_REGISTER_LENGTH         (25)  /* 0x48 */
#define DRV_TPS6699X_RX_VID_SOPP_REGISTER_LENGTH        (25)  /* 0x49 */
#define DRV_TPS6699X_RX_VDM_REGISTER_LENGTH             (29)  /* 0x4F */
#define DRV_TPS6699X_INTEL_VID_CF_REGISTER_LENGTH       (5)   /* 0x52 only read first 5 of 8 */
#define DRV_TPS6699x_DP_STATUS_REGISTER_LENGTH          (1)   /* 0x58 only read first 1 of 40 */
#define DRV_TPS6699x_INTEL_VID_STATUS_REGISTER_LENGTH   (11)  /* 0x59 */
#define DRV_TPS6699X_DATA_STATUS_REGISTER_LENGTH        (5)   /* 0x5F */
#define DRV_TPS6699X_SLEEP_CONFIG_REGISTER_LENGTH       (1)   /* 0x70 */
#define DRV_TPS6699X_GPIO_STATUS_REGISTER_LENGTH        (12)  /* 0x72 */
#define DRV_TPS6699X_GPIO_P0_REGISTER_LENGTH            (51)  /* 0xA0 */
#define DRV_TPS6699X_GPIO_P1_REGISTER_LENGTH            (30)  /* 0xA1 */

#define DRV_TPS_INT_EVENT              DRV_TPS6699X_INT_EVENT

/* length->11 address->0x14, 0x15, 0x16, 0x17 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t Rsvd0                                 :1;
            uint32_t PdHardReset                           :1;
            uint32_t Rsvd2                                 :1;
            uint32_t PlugInsertOrRemoval                   :1;
            uint32_t PrSwapComplete                        :1;
            uint32_t DrSwapComplete                        :1;
            uint32_t FrSwapComplete                        :1;
            uint32_t SourceCapUpdated                      :1;
            uint32_t Rsvd7                                 :1;
            uint32_t Overcurrent                           :1;
            uint32_t AttentionReceived                     :1;
            uint32_t VdmReceived                           :1;
            uint32_t NewContractAsCons                     :1;
            uint32_t NewContractAsProv                     :1;
            uint32_t SourceCapMsgReceived                  :1;
            uint32_t SinkCapMsgReceived                    :1;
            uint32_t Rsvd16                                :1;
            uint32_t PrSwapRequested                       :1;
            uint32_t DrSwapRequested                       :1;
            uint32_t Rsvd19                                :1;
            uint32_t UsbHostPresent                        :1;
            uint32_t UsbHostPresentNoLonger                :1;
            uint32_t Rsvd22                                :1;
            uint32_t PPswitchChanged                       :1;
            uint32_t PowerStatusUpdate                     :1;
            uint32_t DataStatusUpdate                      :1;
            uint32_t StatusUpdate                          :1;
            uint32_t PdStatusUpdate                        :1;
            uint32_t Rsvd28                                :1;
            uint32_t Rsvd29                                :1;
            uint32_t Cmd1Complete                          :1;
            uint32_t Cmd2Complete                          :1;

            /* bytes 5-8 */
            uint32_t Error_DeviceIncompatible              :1;
            uint32_t Error_CannotProvideVoltageOrCurrent   :1;
            uint32_t Error_CanProvideVoltageOrCurrentLater :1;
            uint32_t Error_PowerEventOccurred              :1;
            uint32_t Error_MissingGetCapMessage            :1;
            uint32_t Rsvd5                                 :1;
            uint32_t Error_ProtocolError                   :1;
            uint32_t Rsvd17                                :1;
            uint32_t Rsvd8                                 :1;
            uint32_t Rsvd9                                 :1;
            uint32_t SnkTransitionComplete                 :1;
            uint32_t Plug_Early_Notification               :1;
            uint32_t ProcHot                               :1;
            uint32_t Rsvd213                               :1;
            uint32_t Error_UnableToSource                  :1;
            uint32_t Rsvd215                               :1;
            uint32_t AmEntryFail                           :1;
            uint32_t AmEnteredMode                         :1;
            uint32_t Rsvd218                               :1;
            uint32_t DiscoverModesComplete                 :1;
            uint32_t ExitModeComplete                      :1;
            uint32_t DataResetStart                        :1;
            uint32_t UsbStatusUpdate                       :1;
            uint32_t ConnManagerUpdate                     :1;
            uint32_t UserVidAltModeEntered                 :1;
            uint32_t UserVidAltModeExited                  :1;
            uint32_t UserVidAltModeAttnVDM                 :1;
            uint32_t UserVidAltModeOtherVDM                :1;
            uint32_t Rsvd2829                              :2;
            uint32_t Dp_Sid_StatusUpdate                   :1;
            uint32_t Intel_Vid_StatusUpdate                :1;

            /* bytes 9-10 */
            uint16_t Pd3StatusUpdate                       :1;
            uint16_t TxMemBufferEmpty                      :1;
            uint16_t MbRdBufferReady                       :1;
            uint16_t Rsvd35                                :3;
            uint16_t EventSocAckTimeout                    :1;
            uint16_t NotSupportedReceived                  :1;
            uint16_t Amd_I2cIomuxError                   :1;
            uint16_t MailboxUpdate                         :1;
						
            uint16_t Rsvd811                               :2;
            uint16_t FrSsignalReceived                     :1;
            uint16_t ChunkResponseReceived                 :1;
            uint16_t ChunkRequestReceived                  :1;
            uint16_t AlertMessageReceived                  :1;

            /* byte 11 */
            uint8_t  PatchLoaded                           :1;
            uint8_t  ReadyForPatch                         :1;
            uint8_t  Rsvd27                                :6;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_INT_EVENT_REGISTER_LENGTH];
    };
} DRV_TPS6699X_INT_EVENT;

#define DRV_TPS_STATUS     DRV_TPS6699X_STATUS

/*** 3.2 0x1A Status Register ***/
typedef struct {
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            // 0 PlugPresent 
            //       - 0b No plug present.
            //       - 1b Plug present, see ConnState for details.
            uint32_t PlugPresent : 1;
            
            // 3:1 ConnState
            //       - 000b No connection
            //       - 001b Port is disabled
            //       - 010b Audio connection (Ra/Ra)
            //       - 011b Debug connection (Rd/Rd)
            //       - 100b No connection, Ra detected (Ra but no Rd)
            //       - 101b Reserved (may be used for Rp/Rp Debug connection)
            //       - 110b Connection present, no Ra detected (Rd but no Ra) or Rp detected with no previous Ra
            //              detection, includes PD Controller that connected in Attached.SNK.
            //       - 111b Connection present, Ra detected (Rd and Ra detected) or Rp detected with previous
            //              Ra detection (assumes PD Controller started as Source and later swapped to Sink).
            uint32_t ConnState : 3;
            
            // 4 PlugOrientation Indicates port orientation when known (requires connection).
            //       - 0b Upside-up orientation (plug CC on C_CC1) or orientation unknown or port is
            //            disabled/disconnected.
            //       - 1b Upside-down orientation (plug CC on C_CC2).
            uint32_t PlugOrientation : 1;
            
            // 5 PortRole Indicates current state of PD Controller C_CCx pulls, and therefore PD 
            //     Controller Power Role, once connected. This bit does not toggle during
            //     Unattached .* state transitions.
            //       - 0b PD Controller is Sink (C_CCx pull-down active) or port is disabled/disconnected.
            //       - 1b PD Controller is Source (C_CCx pull-up active).
            uint32_t PortRole : 1;
            
            // 6 DataRole Indicates current state of PD Controller Data Role once connected.
            //       - 0b PD Controller is UFP or port is disabled/disconnected.
            //       - 1b PD Controller is DFP.
            uint32_t DataRole : 1;
						
            // 7 if EPR mode is active or not
            uint32_t EprModeIsActive : 1;
            
            // 19:8 Reserved
            uint32_t Rsvd819 : 12;
            
            // 21:20 VbusStatus 00b VBUS is at vSafe0V (less than 0.8V)
            //       - 01b VBUS is at vSafe5V (4.75V to 5.5V).
            //       - 10b VBUS is at other PD-negotiated power level and within expected limits.
            //       - 11b VBUS is not within any of the above ranges.
            uint32_t VbusStatus : 2;
            
            // 23:22 UsbHostPresent
            //       - 00b No far-end device present providing VBUS or PD Controller power role is Source.
            //       - 01b VBUS is being provided by a far-end device that is a PD device not capable of USB
            //             communications.
            //       - 10b VBUS is being provided by a far-end device that is not a PD device.
            //       - 11b VBUS is being provided by a far-end device that is a PD device capable of USB
            //             communications.
            uint32_t UsbHostPresent : 2;
            
            // 25:24 ActingAsLegacy Indicates when PD Controller has gone into a mode where it is acting
            //         like a legacy (non PD) device.
            //       - 00b PD Controller is not in a legacy (non PD mode)
            //       - 01b PD Controller is acting like a legacy sink. It will not respond to USB PD message traffic.
            //       - 10b PD Controller is acting like a legacy source. It will not respond to USB PD message
            //             traffic.
            uint32_t ActingAsLegacy : 2;
            
            // 26 Reserved
            uint32_t Rsvd26 : 1;
            
            // 27 BIST
            //       - 0b No BIST in progress.
            //       - 1b BIST in progress (also indicated by Mode register, 0x03, reading ��BIST��).
            uint32_t BIST : 1;
            
            // 28 Reserved
            uint32_t Rsvd28 : 1;
            
            // 29 Reserved
            uint32_t Rsvd29 : 1;
            
            // 30 Ack_Timeout
            //       - 0b SoC is responding timely
            //       - 1b Soc has not responded to recent Data Status Update
            uint32_t Ack_Timeout : 1;
            
            // 31 Reserved
            uint32_t Rsvd31 : 1;
            
            /* bytes 5-6 */
            // 1:0 AMStatus
            //       - 00b No Alternate Modes attempted.
            //       - 01b At least one Alternate Mode entry successful.
            //       - 10b At least one Alternate Mode entry unsuccessful.
            //       - 11b At least one Alternate Mode entry successful and at least one mode entry unsuccessful.
            uint16_t AMStatus : 2;
            
            // 15:2 Reserved
            uint16_t Rsvd215 : 6;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_STATUS;

/* length->2 address->0x20 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-2 */
            uint16_t SleepState                             :3;
            uint16_t Rsvd                                   :13;
						
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_SET_SX_APP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_SET_SX_APP_CONFIG;

/* length->33 address->0x21 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t NumberSvidsSop                          :4;
            uint8_t NumberSvidsSopPrime                     :4;
            /* bytes 2-5 */
            uint32_t SvidsSop0                              :32;
            /* bytes 6-9 */
            uint32_t SvidsSop1                              :32;
            /* bytes 10~13 */
            uint32_t SvidsSop2                              :32;
            /* bytes 14~17 */
            uint32_t SvidsSop3                              :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_SVID_SOP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_SVID_SOP;

/* length->1 address->0x22 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
			uint8_t Rsvd                                    :1;
            uint8_t Usb2HostConnected                       :1;
            uint8_t Usb3HostConnected                       :1;
			uint8_t DpHostConnected                         :1;
			uint8_t TbtHostConnected                        :1;
			uint8_t Usb4HostConnected                       :1;
			uint8_t PcieHostConnected                       :1;
			uint8_t Rsvd1                      				:1;
		}PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699x_CONN_M_STS_REGISTER_LENGTH];
    };
} DRV_TPS6699x_CONN_M_STS;

/* length->4 address->0x23 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t Rsvd                                   :13;
            uint32_t HostPresent                            :1;
            uint32_t Tbt3_Supported                         :1;
            uint32_t DP_Supported                           :1;
            uint32_t PCIe_Supported                         :1;
            uint32_t Rsvd1                                  :8;
            uint32_t Usb3Drd                                :1;
            uint32_t Usb4Drd                                :1;
            uint32_t Rsvd2                                  :5;
						
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_USB_CONFIG_REGISTER_LENGTH];
    };
} DRV_TPS6699X_USB_CONFIG;


/* length->9 address->0x24 */
typedef struct
{
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t eudoSopSendRecv                         :2;
            uint8_t usb4ReqPlugMode                         :2;
            uint8_t usbModeActiveOnPlug                     :1;
            uint8_t Resv                                    :1;
            uint8_t usbRentryNeeded                         :1;
            uint8_t Resv2                                   :1;
            /* bytes 2~5 */
            uint32_t usb4EnterUsbRxTx                       :32;

            /* bytes 6~9 */
            uint32_t Resv3                                  :32;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699x_USB_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_USB_STATUS;

/* length->5 address->0x26 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t Pp1_CableSw                            :2;
            uint32_t Pp2_CabelSw                            :2;
            uint32_t Rsvd54                                 :2;
            uint32_t Pp1_SW                                 :3;
            uint32_t Pp2_SW                                 :3;
            uint32_t Pp3_SW                                 :3;
            uint32_t Pp4_SW                                 :3;
            uint32_t Rsvd2718                               :10;
            uint32_t Pp1_Overcurrent                        :1;
            uint32_t Pp2_Overcurrent                        :1;
            uint32_t Rsvd76                                 :2;
		
            /* bytes 5 */
            uint32_t Rsvd98                                 :2;
            uint32_t PP1_CableOvercurrent                   :1;
            uint32_t PP2_CableOvercurrent                   :1;
            uint32_t Rsvd1312                               :2;
            uint32_t PowerSource                            :2;
						
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_POWER_PATH_REGISTER_LENGTH];
    };
} DRV_TPS6699X_POWER_PATH;


/* length->14 address->0x27 */
typedef struct
{
    uint8_t u8Length;
    union {
        struct {
            /* byte 1 */
            uint8_t PaVconnConfig                          :1;
            uint8_t Rsvd1                                  :1;
            uint8_t PbVconnConfig                          :1;
            uint8_t Rsvd37                                 :5;
            /* byte 2 */
            uint8_t PaPp5vVbusSwConfig                     :3;
            uint8_t PbPp5vVbusSwConfig                     :3;
            uint8_t IlimOverShoot                          :2;
            /* byte 3 */
            uint8_t PaPpextVbusSwConfig                    :3;
            uint8_t PbPpextVbusSwConfig                    :3;
            uint8_t Rsvd2223                               :2;
            /* bytes 4-5 */
            uint16_t MultiPortSinkPolicy                   :1;
            uint16_t Rsvd25                                :1;
            uint16_t TbtControllerType                     :3;
            uint16_t EnableOneUFPPolicy                    :1;
            uint16_t EnableSpm                             :1;
            uint16_t MultiPortSinkNonOverlapTime           :2;
            uint16_t EnableI2cMultiControllerMode          :1;
            uint16_t I2cTimeout                            :3;
            uint16_t Rsvd372                               :1;
            uint16_t EmulateSinglePort                     :1;
            uint16_t MinimumCurrentAdvertisement           :1;
            /* byte 6 */
            uint8_t Rsvd4042                               :3;
            uint8_t UsbDefaultCurrent                      :2;
            uint8_t Rsvd4546                               :2;
            uint8_t EnableAmEntryExitOnLpModeExitEntry     :1;
            /* byte 7 */
            uint8_t Rsvd4853                               :6;
            uint8_t CrossbarPollingMode                    :1;
            uint8_t CrossbarConfigType1Extended            :1;
            /* byte 8 */
            uint8_t ExternalDcdcStatusPollingInterval      :8;
            /* byte 9 */
            uint8_t Port1I2c2TargetAddress                 :8;
            /* byte 10 */
            uint8_t Port2I2c2TargetAddress                 :8;
            /* byte 11 */
            uint8_t VsysPreventsHighPower                  :1;
            uint8_t WaitForVin3v3                          :1;
            uint8_t WaitForMinimumPower                    :1;
            uint8_t Rsvd8386                               :4;
            uint8_t EnableDesktopSpm                       :1;
            /* byte 12 */
            uint8_t Rsvd8895                               :8;
            /* byte 13-14 */
            uint16_t Rsvd96102                             :7;
            uint16_t SourcePolicyMode                      :2;
            uint16_t EnableHbretimerStartup                :1;
            uint16_t S4orS5RetimerPowerSaving              :2;
            uint16_t AllowI2c4Access                       :1;
            uint16_t Rsvd109111                            :3;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_GL_SYS_CONFIG_REGISTER_LENGTH];
    };
} DRV_TPS6699X_GL_SYS_CONFIG;


/* length->6 address->0x28 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-2 */
            uint16_t TypeCStateMachine                      :2;
            uint16_t CrossbarType                           :1;
            uint16_t Rsvd36                                	:4;
            uint16_t PpExtActiveLow                         :1;
            uint16_t SupportTypeCOptions                    :2;
            uint16_t DisablePD                              :1;
            uint16_t UsbCommCapable                         :1;
            uint16_t DebugAccessorySupp                     :1;
            uint16_t USB3rate                               :2;
            uint16_t AmdI2CMuxEnable                        :1;
					
			/* bytes 3-6 */
			uint32_t VbusOvpUsage                           :2;
			uint32_t SoftStart                              :2;
			uint32_t OvpForPp5V                             :2;
			uint32_t CrossbarConfigType1Extended            :1;
			uint32_t RemoveSafeStateBetweenU3ToDp           :1;
			uint32_t VbusSinkUvpTripHv                      :3;
			uint32_t ApdoVbusUvpThreshold                   :2;
			uint32_t ApdoIlimOverShoot                      :2;
			uint32_t Rsvd31                                 :1;
			uint32_t ApdoVbusUvpTripPointOffset             :16;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_PORT_CONFIG_REGISTER_LENGTH];
    };
} DRV_TPS6699X_PORT_CONFIG;

#define DRV_TPS_PORT_CONTROL DRV_TPS6699X_PORT_CONTROL

/* length->6 address->0x29 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t TypeCCurrent                           :2;
            uint32_t Rsvd32                                 :2;
            uint32_t ProcessSwapToSink                      :1;
            uint32_t InitiateSwapToSink                     :1;
            uint32_t ProcessSwapToSource                    :1;
            uint32_t InitiateSwapToSource                   :1;
            uint32_t AutoCapRequest                         :1;
            uint32_t AutoAlertEnable                        :1;
            uint32_t AutoPpsStsEnable                       :1;
            uint32_t RetimerFWUpdate                        :1;
            uint32_t ProcessSwapToUFP                       :1;
            uint32_t InitiateSwapToUFP                      :1;
            uint32_t ProcessSwapToDFP                       :1;
            uint32_t InitiateSwapToDFP                      :1;
            uint32_t AutomaticIDRequest                     :1;
            uint32_t AmIntrusiveMode                        :1;
            uint32_t ForceUSB3Gen1                          :1;
            uint32_t UnconstrainedPower                     :1;
            uint32_t EnableCurrentMonitor                   :1;
            uint32_t SinkControlBit                         :1;
            uint32_t FRSwap_Enabled                         :1;
            uint32_t Rsvd33                                 :4;	
            uint32_t UsbDisable                             :1;		
            uint32_t Rsvd34                                 :2;									
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_PORT_CONTROL_REGISTER_LENGTH];
    };
} DRV_TPS6699X_PORT_CONTROL;

#define DRV_TPS_BOOT     DRV_TPS6699X_BOOT_FLAGS

/* length->52 address->0x2D */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t BootStage                              :4;
            uint32_t Rsvd                                   :28;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_BOOT_FLAGS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_BOOT_FLAGS;

/* length->40 address->0x2F */
typedef struct
{    
    uint8_t u8Length;
    uint8_t buf[DRV_TPS6699X_DEV_INFO_REGISTER_LENGTH];
	
} DRV_TPS6699X_DEV_INFO;

/* length->53 address->0x30 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t RxSourceNumValidPdos                    :3;
            uint8_t NumberOfValidEprPdos                    :3;
            uint8_t LastSrcCapRecvIsEpr                     :1;
            uint8_t Rsvd7                                   :1;
            /* bytes 2-5 */
            uint32_t RxSourcePdo1                           :32;
            /* bytes 6-9 */
            uint32_t RxSourcePdo2                           :32;
            /* bytes 10-13 */
            uint32_t RxSourcePdo3                           :32;
            /* bytes 14-17 */
            uint32_t RxSourcePdo4                           :32;
            /* bytes 18-21 */
            uint32_t RxSourcePdo5                           :32;
            /* bytes 22-25 */
            uint32_t RxSourcePdo6                           :32;
            /* bytes 26-29 */
            uint32_t RxSourcePdo7                           :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_SRC_CAP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_SRC_CAP;

/* length->53 address->0x31 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t numValidPdos                            :3;
			uint8_t NumberOfValidEprPdos                    :3;
			uint8_t LastSnkCapRecvIsEpr                     :1;
            uint8_t Rsvd7                                   :1;
            /* bytes 2-5 */
            uint32_t RxSinkPdo1                             :32;
            /* bytes 6-9 */
            uint32_t RxSinkPdo2                             :32;
            /* bytes 10-13 */
            uint32_t RxSinkPdo3                             :32;
            /* bytes 14-17 */
            uint32_t RxSinkPdo4                             :32;
            /* bytes 18-21 */ 
            uint32_t RxSinkPdo5                             :32;
            /* bytes 22-25 */
            uint32_t RxSinkPdo6                             :32;
            /* bytes 26-29 */
            uint32_t RxSinkPdo7                             :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_SNK_CAP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_SNK_CAP;

/* length->63 address->0x32 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t numValidPdos                            :3;
			uint8_t NumberOfValidEprPdos                    :3;
            uint8_t Rsvd67                                  :2;
            /* bytes 2-3 */
            uint16_t PowerPathForPdo1                       :2;
            uint16_t Rsvd215                                :14;
            /* bytes 4-7 */
            uint32_t TXSourcePdo1                           :32;
            /* bytes 8-11 */
            uint32_t TXSourcePdo2                           :32;
            /* bytes 12-15 */
            uint32_t TXSourcePdo3                           :32;
            /* bytes 16-19 */
            uint32_t TXSourcePdo4                           :32;
            /* bytes 20-23 */
            uint32_t TXSourcePdo5                           :32;
            /* bytes 24-27 */
            uint32_t TXSourcePdo6                           :32;
            /* bytes 28-31 */
            uint32_t TXSourcePdo7                           :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_TX_SRC_CAP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_TX_SRC_CAP;

/* length->53 address->0x33 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t numValidPdos                            :3;
			uint8_t NumberOfValidEprPDOs                    :3;
            uint8_t Rsvd67                                  :2;
            /* bytes 2-5 */
            uint32_t TxSinkPdo1                             :32;
            /* bytes 6-9 */
            uint32_t TxSinkPdo2                             :32;
            /* bytes 10-13 */
            uint32_t TxSinkPdo3                             :32;
            /* bytes 14-17 */
            uint32_t TxSinkPdo4                             :32;
            /* bytes 18-21 */
            uint32_t TxSinkPdo5                             :32;
            /* bytes 22-25 */
            uint32_t TxSinkPdo6                             :32;
            /* bytes 26-29 */
            uint32_t TxSinkPdo7                             :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_TX_SNK_CAP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_TX_SNK_CAP;

/* length->4 address->0x40 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1-4 */
            uint32_t Rsvd10                                 :2;
            uint32_t CcPullUp                              	:2;
            uint32_t PortType                               :2;
            uint32_t PresentRole                            :1;
            uint32_t Rsvd7                                  :1;
            uint32_t SoftResetType                          :5;
            uint32_t Rsvd1315                               :3;
            uint32_t HardResetDetails                       :6;
            uint32_t ErrorRecoveryDetails                   :6;
			uint32_t DataResetDetails                       :3;
            uint32_t Rsvd31                                 :1;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_PD_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_PD_STATUS;

/* length->49 address->0x47 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t numValidVDOs                            :3;
            uint8_t Rsvd37                                  :5;
            /* bytes 2-5 */
            uint32_t VendorID                               :16;
            uint32_t Rsvd1622                               :7;
            uint32_t ProductType_DFP                        :3;
            uint32_t ModalOperationSupported                :1;
            uint32_t ProductType_UFP                        :3;
            uint32_t UsbCommCapableAsDevice                 :1;
            uint32_t UsbCommCapableAsHost                   :1;
            /* bytes 6-9 */
            uint32_t CertStatVdo                            :32;
            /* bytes 10-13 */
            uint32_t DcdDevice                              :16;
            uint32_t ProductId                              :16;
            /* bytes 14-17 */
            uint32_t Ufp1Vdo                                :32;
			/* bytes 18-21 */
			uint32_t Rsvd1                                  :32;
            /* bytes 22-25 */
            uint32_t Dfp1Vdo                                :32;
						
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_TX_VID_REGISTER_LENGTH];
    };
} DRV_TPS6699X_TX_VID;

/* length->25 address->0x48 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t RxIdSoppNumValid                        :3;
            uint8_t Rsvd35                                  :3;
            uint8_t RxIdSoppResponse                        :2;
            /* bytes 2-5 */
            uint32_t RxIdSoppVdo1                           :32;
            /* bytes 6-9 */
            uint32_t RxIdSoppVdo2                           :32;
            /* bytes 10-13 */
            uint32_t RxIdSoppVdo3                           :32;
            /* bytes 14-17 */
            uint32_t RxIdSoppVdo4                           :32;
            /* bytes 18-21 */
            uint32_t RxIdSoppVdo5                           :32;
            /* bytes 22-25 */
            uint32_t RxIdSoppVdo6                           :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_VID_SOP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_VID_SOP;

/* length->25 address->0x49 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t RxIdSoppNumValid                        :3;
            uint8_t Rsvd35                                  :3;
            uint8_t RxIdSoppResponse                        :2;
            /* bytes 2-5 */
            uint32_t RxIdSoppVdo1                           :32;
            /* bytes 6-9 */
            uint32_t RxIdSoppVdo2                           :32;
            /* bytes 10-13 */
            uint32_t RxIdSoppVdo3                           :32;
            /* bytes 14-17 */
            uint32_t RxIdSoppVdo4                           :32;
            /* bytes 18-21 */
            uint32_t RxIdSoppVdo5                           :32;
            /* bytes 22-25 */
            uint32_t RxIdSoppVdo6                           :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_VID_SOPP_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_VID_SOPP;

/* length->29 address->0x4F */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t RxVdmNumValid                           :3;
            uint8_t RxVdmSource                             :2;
            uint8_t RxVdmSequenceNum                        :2;
            /* bytes 2-5 */
            uint32_t RxVdmdo1                               :32;
            /* bytes 6-9 */
            uint32_t RxVdmdo2                               :32;
            /* bytes 10-13 */
            uint32_t RxVdmdo3                               :32;
            /* bytes 14-17 */
            uint32_t RxVdmdo4                               :32;
            /* bytes 18-21 */
            uint32_t RxVdmdo5                               :32;
            /* bytes 22-25 */ 
            uint32_t RxVdmdo6                               :32;
            /* bytes 26-29 */
            uint32_t RxVdmdo7                               :32;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_RX_VDM_REGISTER_LENGTH];
    };
} DRV_TPS6699X_RX_VDM;

#define DRV_TPS_INTEL_VID_CONFIG DRV_TPS6699X_INTEL_VID_CONFIG
/* length->8 address->0x52 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
						/* bytes 1 */
            uint8_t IntelVidEnabled                         :1;
            uint8_t thunderBoltModeEnabled                  :1;
            uint8_t Resv27                                  :6;
            /* bytes 2 */
            uint8_t Resv11                                  :1;
            uint8_t TbtEMarkerOverride                      :1;
            uint8_t AnMinPowerRequired                      :1;
            uint8_t Resv12                                  :1;
            uint8_t DualTBTRetimerPresent                   :1;
            uint8_t TbtRetimerPresent                       :1;
            uint8_t DataStatusHPDEvents                     :1;
            uint8_t RetimerComplianceSupport                :1;
            /* bytes 3~4 */
            uint16_t LegacyTbtAdapter                       :1;
            uint16_t Resv19                                 :9;
            uint16_t VproSupported                          :1;
            uint16_t Resv1115                               :5;
            /* bytes 3~4 */
            uint16_t Resv015                                :16;
			/* byte 5 */
			uint8_t Resv0                                   :1;
            uint8_t ThunderBoltAutoEntryAllowed             :1;
            uint8_t Resv271                                 :6;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_INTEL_VID_CF_REGISTER_LENGTH];
    };
} DRV_TPS6699X_INTEL_VID_CONFIG;

#define DRV_TPS_DP_STATUS DRV_TPS6699X_DP_STATUS
/* length->1 address->0x58 */
typedef struct
{
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t dpSidDetected                           :1;
            uint8_t dpModeActive                            :1;
            uint8_t Resv                                    :6;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699x_DP_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_DP_STATUS;

#define DRV_TPS_INTEL_VID_STATUS DRV_TPS6699X_INTEL_VID_STATUS
/* length->11 address->0x59 */
typedef struct
{
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t intelVidDetected                        :1;
            uint8_t tbtModeActive                           :1;
						uint8_t forcedTbtMode                           :1;
            uint8_t Resv                                    :5;

            /* bytes 2~5 */
            uint32_t tbtAttData                             :32;

            /* bytes 6~7 */
            uint16_t tbtEnterModeData                       :16;

            /* bytes 8~9 */
            uint32_t tbtModeDataRxOnSop                      :16;

            /* bytes 10~11 */
            uint32_t tbtModeDataRxOnSopPrime                 :16;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699x_INTEL_VID_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_INTEL_VID_STATUS;

/* length->5 address->0x5F */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* byte 1 */
            uint8_t DataConnection                           :1;
            uint8_t ConnectionOrientation                    :1;
            uint8_t RetimerOrRedriver                        :1;
            uint8_t OvercurrentOrTemperature                 :1;
            uint8_t Usb2Connection                           :1;
            uint8_t Usb3Connection                           :1;
            uint8_t Usb3Speed                                :1;
            uint8_t DataRole                                 :1;
					  
            /* byte 2 */
            uint8_t DpConnection                             :1;
            uint8_t DpSourceSink                             :1;
            uint8_t DpPinAssignment                          :2;
            uint8_t DebugAccessoryMode                       :1;
            uint8_t Resv                                     :1;
            uint8_t HpdIrqSticky                             :1;
            uint8_t HpdLevel                                 :1;
					
            /* byte 3 */
            uint8_t TbtConnection                            :1;
            uint8_t TbtType                                  :1;
            uint8_t CableType                                :1;
            uint8_t Resv1                                    :1;
            uint8_t ActiveLinkTraining                       :1;
            uint8_t DebugAlternateMode                       :1;
            uint8_t ActiveCable                              :1;
            uint8_t Usb4Connection                           :1;
						
            /* byte 4 */
            uint8_t Resv2                                    :1;
            uint8_t TbtCableSpeedSupport                     :3;
            uint8_t TbtCableGEN                              :2;
            uint8_t Resv3                                    :2;
						
            /* byte 5 */
            uint8_t DebugAlternateModeID                     :8;
						
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_DATA_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_DATA_STATUS;

/* length->1 address->0x70 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t SleepModeAllowed                        :1;
            uint8_t SleepTime                               :2;
            uint8_t Resv                                    :5;
        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_SLEEP_CONFIG_REGISTER_LENGTH];
    };
} DRV_TPS6699X_SLEEP_CONFIG;

/* length->12 address->0x72 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 ~ 4 */
            uint32_t GPIO0_DATA                              :1;
            uint32_t GPIO1_DATA                              :1;
            uint32_t GPIO2_DATA                              :1;
            uint32_t GPIO3_DATA                              :1;
            uint32_t GPIO4_DATA                              :1;
            uint32_t GPIO5_DATA                              :1;
            uint32_t GPIO6_DATA                              :1;
            uint32_t GPIO7_DATA                              :1;
            uint32_t GPIO8_DATA                              :1;
			uint32_t GPIO9_DATA                              :1;
            uint32_t GPIO10_DATA                             :1;
            uint32_t GPIO11_DATA                             :1;
            uint32_t GPIO12_DATA                             :1;
            uint32_t GPIO13_DATA                             :1;
			uint32_t GPIO14_DATA                             :1;
            uint32_t Resv1                                   :17;
					  
            /* bytes 5 ~ 8 */
            uint32_t GPIO0_DIR                               :1;
            uint32_t GPIO1_DIR                               :1;
            uint32_t GPIO2_DIR                               :1;
            uint32_t GPIO3_DIR                               :1;
            uint32_t GPIO4_DIR                               :1;
            uint32_t GPIO5_DIR                               :1;
            uint32_t GPIO6_DIR                               :1;
            uint32_t GPIO7_DIR                               :1;
            uint32_t GPIO8_DIR                               :1;
			uint32_t GPIO9_DIR                               :1;
            uint32_t GPIO10_DIR                              :1;
            uint32_t GPIO11_DIR                              :1;
            uint32_t GPIO12_DIR                              :1;
            uint32_t GPIO13_DIR                              :1;
			uint32_t GPIO14_DIR                              :1;
            uint32_t Resv2                                   :17;
						
			/* bytes 9 ~ 12 */
            uint32_t GPIO0_OutputEn                          :1;
            uint32_t GPIO1_OutputEn                          :1;
            uint32_t GPIO2_OutputEn                          :1;
            uint32_t GPIO3_OutputEn                          :1;
            uint32_t GPIO4_OutputEn                          :1;
            uint32_t GPIO5_OutputEn                          :1;
            uint32_t GPIO6_OutputEn                          :1;
            uint32_t GPIO7_OutputEn                          :1;
            uint32_t GPIO8_OutputEn                          :1;
			uint32_t GPIO9_OutputEn                          :1;
            uint32_t GPIO10_OutputEn                         :1;
            uint32_t GPIO11_OutputEn                         :1;
            uint32_t GPIO12_OutputEn                         :1;
            uint32_t GPIO13_OutputEn                         :1;
			uint32_t GPIO14_OutputEn                         :1;
            uint32_t Resv3                                   :17;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_GPIO_STATUS_REGISTER_LENGTH];
    };
} DRV_TPS6699X_GPIO_STATUS;

#define DRV_TPS_GPIO_STATUS     DRV_TPS6699X_GPIO_STATUS

/* length->51 address->0xA0 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 ~ 4 */
            uint32_t OutputEnable                            :15;
            uint32_t Resv1                                   :17;
						/* bytes 5 ~ 8 */
            uint32_t InterruptEnable                         :15;
            uint32_t Resv2                                   :17;
						/* bytes 9 ~ 12 */
            uint32_t Output_Data                             :15;
            uint32_t Resv3                                   :17;
						/* bytes 13 ~ 16 */
            uint32_t OutputOdEnable                          :15;
            uint32_t Resv4                                   :17;
						/* bytes 17 ~ 20 */
            uint32_t PullDownEnable                          :15;
			uint32_t Resv5                                   :17;
			/* bytes 21 ~ 24 */
            uint32_t PullUpEnable                            :15;
            uint32_t Resv6                                   :17;
			/* bytes 25 ~ 28 */
            uint32_t InputDigitalEnable                      :15;
            uint32_t Resv7                                   :17;
			/* bytes 29 ~ 32 */
			uint32_t InvertData                              :15;
            uint32_t Resv8                                   :17;
			/* bytes 33 ~ 36 */
			uint32_t SetVioTiLdo3V3                          :15;
            uint32_t Resv9                                   :17;
			/* bytes 37 */
			uint8_t GPIO0_MUX                                :8;
			/* bytes 38 */
			uint8_t GPIO1_MUX                                :8;
			/* bytes 39 */
			uint8_t GPIO2_MUX                                :8;
			/* bytes 40 */
			uint8_t GPIO3_MUX                                :8;
			/* bytes 41 */
			uint8_t GPIO4_MUX                                :8;
			/* bytes 42 */
			uint8_t GPIO5_MUX                                :8;
			/* bytes 43 */
			uint8_t GPIO6_MUX                                :8;
			/* bytes 44 */
			uint8_t GPIO7_MUX                                :8;
			/* bytes 45 */
			uint8_t GPIO8_MUX                                :8;
			/* bytes 46 */
			uint8_t GPIO9_MUX                                :8;
			/* bytes 47 */
			uint8_t GPIO10_MUX                               :8;
			/* bytes 48 */
			uint8_t GPIO11_MUX                               :8;
			/* bytes 49 */
			uint8_t GPIO12_MUX                               :8;
			/* bytes 50 */
			uint8_t GPIO13_MUX                               :8;
			/* bytes 51 */
			uint8_t GPIO14_MUX                               :8;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_GPIO_P0_REGISTER_LENGTH];
    };
} DRV_TPS6699X_GPIO_P0;

/* length->30 address->0xA1 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 ~ 4 */
            uint32_t OutputEnable                            :12;
            uint32_t Resv1                                   :20;
						/* bytes 5 ~ 8 */
            uint32_t Resv2                                   :32;
						/* bytes 9 ~ 12 */
            uint32_t Output_Data                             :12;
            uint32_t Resv3                                   :20;
						/* bytes 13 ~ 16 */
            uint32_t OutputOdEnable                          :12;
            uint32_t Resv4                                   :20;
						/* bytes 17 ~ 20 */
            uint32_t PullDownEnable                          :7;
            uint32_t Resv5                                   :25;
						/* bytes 21 ~ 24 */
            uint32_t PullUpEnable                            :7;
			uint32_t Resv6                                   :25;
			/* bytes 25 ~ 28 */
            uint32_t AnalogInputEnable                       :4;
            uint32_t Resv7                                   :28;
			/* bytes 29 */
			uint8_t GPIO0_MUX                                :1;
			uint8_t GPIO1_MUX                                :1;
			uint8_t GPIO2_MUX                                :1;
			uint8_t GPIO3_MUX                                :1;
			uint8_t GPIO4_MUX                                :1;
			uint8_t GPIO5_MUX                                :1;
			uint8_t GPIO6_MUX                                :1;
			uint8_t GPIO7_MUX                                :1;
			/* bytes 30 */
			uint8_t GPIO8_MUX                                :1;
			uint8_t GPIO9_MUX                                :1;
			uint8_t GPIO10_MUX                               :1;
			uint8_t GPIO11_MUX                               :1;
			uint8_t Resv8                                    :4;

        } PACK_STRUCT f;
        uint8_t buf[DRV_TPS6699X_GPIO_P1_REGISTER_LENGTH];
    };
} DRV_TPS6699X_GPIO_P1;

/*** 3.13 0x34 Active Contract PDO ***/
/*** 3.14 0x35 Active Contract RDO ***/
/*** 3.15 0x36 Sink Request RDO    ***/
typedef union {
	uint32_t u32DatObj;
	union {
		struct {
			uint32_t c10mA  : 10;
			uint32_t v50mV  : 10;
			uint32_t rsvd   : 12;
		} base;
		struct {
			uint32_t c10mA  : 10;
			uint32_t v50mV  : 10;
			uint32_t rsvd   : 12;
		} fixed;
		struct {
			uint32_t c10mA  : 10;
			uint32_t v50mV  : 10;
			uint32_t misc   : 10;
			uint32_t rsvd   : 2;
		} variable;
	};
} ACD_PD30_PWR_DATA_OBJECT;

typedef struct 
{
	uint8_t u8Length;
	pd_do_t xDo;
	uint16_t FirstPdoCtrlBits : 10;
	uint16_t                  : 6;
} DRV_TPS6699X_ACTIVE_CONTRACT_PDO;    // 0x34, len 6 + 1

typedef struct 
{
	uint8_t u8Length;
	pd_do_t xDo;
} DRV_TPS6699X_ACTIVE_CONTRACT_RDO;    // 0x35, len 4 + 1

typedef struct 
{
	uint8_t u8Length;
	pd_do_t xDo;
} DRV_TPS6699X_SINK_REQUEST_RDO;       // 0x36, len 4 + 1

/* pd app mode patch status */
typedef struct
{
	uint8_t ActiveRegion;
	uint8_t InactiveRegion;
}PD_APP_CONTEXT;

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

/* Define the unified PD function name */
/* Access I2C regs in TI PD controller */
#define amd_crb_drivers_tps_regAccess                     amd_crb_drivers_tps6699x_regAccess
/* Access I2C burst write for patch */
#define amd_crb_drivers_tps_burst_patch_download          amd_crb_drivers_tps6699x_burst_patch_download
/* Check the cmd done */
#define amd_crb_drivers_tps_wait4CmdDone                  amd_crb_drivers_tps6699x_wait4CmdDone
/* PD cmd and data write */
#define amd_crb_drivers_tps_cmdExecutor                   amd_crb_drivers_tps6699x_cmdExecutor
/* read the source rdo */
#define amd_crb_drivers_tps_readSrcRdo                    amd_crb_drivers_tps6699x_readSrcRdo
/* read the sink rdo */
#define amd_crb_drivers_tps_readSnkRdo                    amd_crb_drivers_tps6699x_readSnkRdo
/* Read sink Pdo */
#define amd_crb_drivers_tps_readSnkPdo                    amd_crb_drivers_tps6699x_readSnkPdo
/* check if enter USB4 */
#define amd_crb_drivers_tps_isUsb4Enter                   amd_crb_drivers_tps6699x_isUsb4Enter
/* check if enter DP alt mode */
#define amd_crb_drivers_tps_isDpEnter                     amd_crb_drivers_tps6699x_isDpEnter
/* check if enter TBT */
#define amd_crb_drivers_tps_isTbtEnter                    amd_crb_drivers_tps6699x_isTbtEnter
/* update specific Tx VID for USB4 enable/disable */
#define amd_crb_drivers_tps_txVidUpdate                   amd_crb_drivers_tps6699x_txVidUpdate
/* change the model operation support bit */
#define amd_crb_drivers_tps_txVidChangeModelOpSupp        amd_crb_drivers_tps6699x_txVidChangeModelOpSupp
/* over write the tx vid register */
#define amd_crb_drivers_tps_txVidOverwrite                amd_crb_drivers_tps6699x_txVidOverwrite
/* set sys power status */
#define amd_crb_drivers_tps_setSxAppConfig                amd_crb_drivers_tps6699x_setSxAppConfig
/* config the type-c current */
#define amd_crb_drivers_tps_portCtrlTypecCurrent          amd_crb_drivers_tps6699x_portCtrlTypecCurrent
/* read the boot status */
#define amd_crb_drivers_tps_bootStatus                    amd_crb_drivers_tps6699x_bootStatus
/* config the sleep mode */
#define amd_crb_drivers_tps_sleepConfig                   amd_crb_drivers_tps6699x_sleepConfig
/* check the dead battery status */
#define amd_crb_drivers_tps_deadBattery                   amd_crb_drivers_tps6699x_deadBattery
/* enable or diable the pd */
#define amd_crb_drivers_tps_portConfigPdDisable           amd_crb_drivers_tps6699x_portConfigPdDisable
/* set and clear port config register */
#define amd_crb_drivers_tps_portConfig                    amd_crb_drivers_tps6699x_portConfig
/* config the port role */
#define amd_crb_drivers_tps_portConfigStateMachine        amd_crb_drivers_tps6699x_portConfigStateMachine
/* retry the failed 4cc commnad */
#define amd_crb_drivers_tps_cmdxRetry                     amd_crb_drivers_tps6699x_cmdxRetry
/* config the dr swap resposne */
#define amd_crb_drivers_tps_portCtrlDrSwapResp            amd_crb_drivers_tps6699x_portCtrlDrSwapResp
/* config the pr swap resposne */
#define amd_crb_drivers_tps_portCtrlPrSwapResp            amd_crb_drivers_tps6699x_portCtrlPrSwapResp
/* read the type-c current setting */
#define amd_crb_drivers_tps_readTypecCurrent              amd_crb_drivers_tps6699x_readTypecCurrent
/* set and clear port control register */
#define amd_crb_drivers_tps_portCtrl                      amd_crb_drivers_tps6699x_portCtrl
/* change GPIO for 6699x GPIO0 */
#define amd_crb_drivers_tps_gpio0Output                   amd_crb_drivers_tps6699x_gpio0Output
/* change GPIO for 6699x GPIO1 */
#define amd_crb_drivers_tps_gpio1Output                   amd_crb_drivers_tps6699x_gpio1Output
/* get device info for full image version */
#define amd_crb_drivers_tps_deviceInfo                    amd_crb_drivers_tps6699x_deviceInfo
/* enable or disable the auto sink policy */
#define amd_crb_drivers_tps_autoSink                      amd_crb_drivers_tps6699x_autoSink
/* lock the sink path */
#define amd_crb_drivers_tps_lockSinkPath                  amd_crb_drivers_tps6699x_lockSinkPath
/* force retimer power on and  reset for FW update */
#define amd_crb_drivers_portCtrlRetimerUpdate             amd_crb_drivers_tps6699x_portCtrlRetimerUpdate
#if CONFIG_EC_UCSI_ENABLE
/* Send 4CC command */
#define amd_crb_drivers_tps_cmdx                          amd_crb_drivers_tps6699x_cmdx
/* Send the 4CC commnad for alt mode */
#define amd_crb_drivers_tps_cmdAltMode                    amd_crb_drivers_tps6699x_cmdAltMode
/* send ucsi related command */
#define amd_crb_drivers_tps_cmdUcsi                       amd_crb_drivers_tps6699x_cmdUcsi
/* Read the pd controller sink cap */
#define amd_crb_drivers_tps_devSnkCapInfo                 amd_crb_drivers_tps6699x_devSnkCapInfo
/* Refreash current pdo */
#define amd_crb_drivers_tps_refreashDevSnkCapCurrent      amd_crb_drivers_tps6699x_refreashDevSnkCapCurrent
/* Read the pd controller source cap */
#define amd_crb_drivers_tps_devSrcCapInfo                 amd_crb_drivers_tps6699x_devSrcCapInfo
/* Refreash current pdo */
#define amd_crb_drivers_tps_refreashDevSrcCapCurrent      amd_crb_drivers_tps6699x_refreashDevSrcCapCurrent
/* Read the pd partner sink cap */
#define amd_crb_drivers_tps_partnerSnkCapInfo             amd_crb_drivers_tps6699x_partnerSnkCapInfo
/* Read the pd partner source cap */
#define amd_crb_drivers_tps_partnerSrcCapInfo             amd_crb_drivers_tps6699x_partnerSrcCapInfo
/* Read the sop vid */
#define amd_crb_drivers_tps_readRxVidSop                  amd_crb_drivers_tps6699x_readRxVidSop
/* Read the sopp vid */
#define amd_crb_drivers_tps_readRxVidSopp                 amd_crb_drivers_tps6699x_readRxVidSopp
/* Read the sop svid */
#define amd_crb_drivers_tps_readRxSvidSop                 amd_crb_drivers_tps6699x_readRxSvidSop
/* Read any receive vdm */
#define amd_crb_drivers_tps_readVdmInfo                   amd_crb_drivers_tps6699x_readVdmInfo
/* Read pd controller alt mode info */
#define amd_crb_drivers_tps_altModeInfo                   amd_crb_drivers_tps6699x_altModeInfo
/* check the port role */
#define amd_crb_drivers_tps_portConfigStateMachineMatch   amd_crb_drivers_tps6699x_portConfigStateMachineMatch
/* get the default source cap */
#define amd_crb_drivers_tps_devSrcCapDeft                 amd_crb_drivers_tps6699x_devSrcCapDeft
/* get the default sink cap */
#define amd_crb_drivers_tps_devSnkCapDeft                 amd_crb_drivers_tps6699x_devSnkCapDeft
/* read the cable vdo */
#define amd_crb_drivers_tps_readCableVdo                  amd_crb_drivers_tps6699x_readCableVdo
/* update the source cap */
#define amd_crb_drivers_tps_devSrcCapUpdate               amd_crb_drivers_tps6699x_devSrcCapUpdate
/* update the sink cap */
#define amd_crb_drivers_tps_devSnkCapUpdate               amd_crb_drivers_tps6699x_devSnkCapUpdate
#endif // end of CONFIG_EC_UCSI_ENABLE

/* Access I2C regs in TI PD controller */
int amd_crb_drivers_tps6699x_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t port);
/* Access I2C burst write for patch */
int amd_crb_drivers_tps6699x_burst_patch_download(uint8_t addr, void * pBuf, uint32_t len);
/* Check the cmd done */
uint32_t amd_crb_drivers_tps6699x_wait4CmdDone (uint8_t port, uint32_t ms);
/* PD cmd and data write */
uint32_t amd_crb_drivers_tps6699x_cmdExecutor(uint32_t timeout, uint8_t port, uint8_t * pDataIn, uint8_t dataInLen, uint8_t * p4CC, uint8_t * pDataOut, uint8_t dataOutLen);
/* read the source rdo */
uint32_t amd_crb_drivers_tps6699x_readSrcRdo(uint8_t port);
/* read the sink rdo */
uint32_t amd_crb_drivers_tps6699x_readSnkRdo(uint8_t port);
/* Read sink Pdo */
uint32_t amd_crb_drivers_tps6699x_readSnkPdo(uint8_t port);
/* check if enter USB4 */
bool amd_crb_drivers_tps6699x_isUsb4Enter(uint8_t port);
/* check if enter DP ALT MODE */
bool amd_crb_drivers_tps6699x_isDpEnter(uint8_t port);
/* check if enter TBT */
bool amd_crb_drivers_tps6699x_isTbtEnter(uint8_t port);
/* update specific Tx VID for USB4 enable/disable */
int amd_crb_drivers_tps6699x_txVidUpdate(uint8_t port, uint8_t numValidVDOs, bool CapAsDev, uint32_t* Vdo);
/* change the model operation support bit */
int amd_crb_drivers_tps6699x_txVidChangeModelOpSupp(uint8_t port, bool ModelOpSupp);
/* over write the tx vid register */
int amd_crb_drivers_tps6699x_txVidOverwrite(uint8_t port);
/* set sys power status */
int amd_crb_drivers_tps6699x_setSxAppConfig(uint8_t port, enum system_power_state sys_status);
/* config the type-c current */
void amd_crb_drivers_tps6699x_portCtrlTypecCurrent(uint8_t port, dpm_typec_cmd_t cc_cur);
/* read the boot status */
void amd_crb_drivers_tps6699x_bootStatus(uint8_t port);
/* config the sleep mode */
void amd_crb_drivers_tps6699x_sleepConfig(uint8_t port, bool sleep_en, uint8_t sleep_time);
/* check the dead battery status */
bool amd_crb_drivers_tps6699x_deadBattery(uint8_t port);
/* enable or diable the pd */
void amd_crb_drivers_tps6699x_portConfigPdDisable(uint8_t port, uint8_t pd_disable);
/* set and clear port config register */
void amd_crb_drivers_tps6699x_portConfig(uint8_t port, DRV_TPS6699X_PORT_CONFIG status_set, DRV_TPS6699X_PORT_CONFIG status_clear);
/* config the port role */
void amd_crb_drivers_tps6699x_portConfigStateMachine(uint8_t port, port_role_t state_machine);
/* retry the failed 4cc commnad */
uint32_t amd_crb_drivers_tps6699x_cmdxRetry(uint32_t timeout, uint8_t port);
/* config the dr swap resposne */
void amd_crb_drivers_tps6699x_portCtrlDrSwapResp(uint8_t port, app_swap_resp_t resp);
/* config the pr swap resposne */
void amd_crb_drivers_tps6699x_portCtrlPrSwapResp(uint8_t port, app_swap_resp_t resp);
/* read the type-c current setting */
void amd_crb_drivers_tps6699x_readTypecCurrent(uint8_t port);
/* set and clear port control register */
void amd_crb_drivers_tps6699x_portCtrl(uint8_t port, DRV_TPS6699X_PORT_CONTROL status_set, DRV_TPS6699X_PORT_CONTROL status_clear);
/* change GPIO for 6699x GPIO0 */
int amd_crb_drivers_tps6699x_gpio0Output(uint8_t port, uint16_t pinMask, uint16_t output);
/* change GPIO for 6699x GPIO1 */
int amd_crb_drivers_tps6699x_gpio1Output(uint8_t port, uint16_t pinMask, uint16_t output);
/* get PD device information */
uint32_t amd_crb_drivers_tps6699x_deviceInfo(uint8_t port);
/* enable or disable the auto sink policy */
void amd_crb_drivers_tps6699x_autoSink(uint8_t chipID, bool enable);
/* lock the sink path */
void amd_crb_drivers_tps6699x_lockSinkPath(uint8_t port, bool enable);
/* force retimer power on and  reset for FW update */
void amd_crb_drivers_tps6699x_portCtrlRetimerUpdate(uint8_t port, bool enable);
#if CONFIG_EC_UCSI_ENABLE
/* Send 4CC command */
uint32_t amd_crb_drivers_tps6699x_cmdx(uint32_t timeout, uint8_t port, uint8_t * pDataIn, uint8_t dataInLen, uint8_t * p4CC);
/* Send the 4CC commnad for alt mode */
void amd_crb_drivers_tps6699x_cmdAltMode(uint32_t timeout, uint8_t port, uint8_t* svid, uint8_t ObjPos, uint8_t * p4CC);
/* send ucsi related command */
void amd_crb_drivers_tps6699x_cmdUcsi(uint32_t timeout, uint8_t port, uint8_t ucsi_cmd, uint8_t ucsi_length, ucsi_ctrl_details_t *detail);
/* Read the pd controller sink cap */
void amd_crb_drivers_tps6699x_devSnkCapInfo(uint8_t port, uint8_t deft);
/* Refreash current pdo */
void amd_crb_drivers_tps6699x_refreashDevSnkCapCurrent(uint8_t port);
/* Read the pd controller source cap */
void amd_crb_drivers_tps6699x_devSrcCapInfo(uint8_t port, uint8_t deft);
/* Refreash current pdo */
void amd_crb_drivers_tps6699x_refreashDevSrcCapCurrent(uint8_t port);
/* Read the pd partner sink cap */
void amd_crb_drivers_tps6699x_partnerSnkCapInfo(uint8_t port);
/* Read the pd partner source cap */
void amd_crb_drivers_tps6699x_partnerSrcCapInfo(uint8_t port);
/* Read the sop vid */
void amd_crb_drivers_tps6699x_readRxVidSop(uint8_t port);
/* Read the sopp vid */
void amd_crb_drivers_tps6699x_readRxVidSopp(uint8_t port);
/* Read the sop svid */
void amd_crb_drivers_tps6699x_readRxSvidSop(uint8_t port);
/* Read any receive vdm */
void amd_crb_drivers_tps6699x_readVdmInfo(uint8_t port);
/* Read pd controller alt mode info */
void amd_crb_drivers_tps6699x_altModeInfo(uint8_t port);
/* check the port role */
bool amd_crb_drivers_tps6699x_portConfigStateMachineMatch(uint8_t port, port_role_t state_machine);
/* get the default source cap */
void amd_crb_drivers_tps6699x_devSrcCapDeft(uint8_t port);
/* get the default sink cap */
void amd_crb_drivers_tps6699x_devSnkCapDeft(uint8_t port);
/* read the cable vdo */
uint32_t amd_crb_drivers_tps6699x_readCableVdo(uint8_t port);
/* update the source cap */
void amd_crb_drivers_tps6699x_devSrcCapUpdate(uint8_t port, uint8_t mask, pd_do_t* pdos);
/* update the sink cap */
void amd_crb_drivers_tps6699x_devSnkCapUpdate(uint8_t port, uint8_t mask, pd_do_t* pdos);
#endif // end of CONFIG_EC_UCSI_ENABLE
#endif // end of AMD_CRB_DRIVERS_TPS6699X_H_

