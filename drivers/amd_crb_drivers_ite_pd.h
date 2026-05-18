/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_ITE_PD_H_
#define AMD_CRB_DRIVERS_ITE_PD_H_

#include "amd_crb_drivers_pd.h"
#include "system.h"

#pragma pack(push,1)
#define PACK_STRUCT                      __attribute__((packed, aligned(1)))

/*
 * ITE PD controller registers define
 * Reserve regs will not in this list
 */
#define ITEPD_REG_UCSI_VRESION              (0x80)
#define ITEPD_REG_UCSI_CCI                  (0x84)
#define ITEPD_REG_UCSI_MSGIN                (0x88)
#define ITEPD_REG_UCSI_CTRL                 (0x98)
#define ITEPD_REG_UCSI_MSGOUT               (0xA0)
 
#define ITEPD_REG_VCMD_STATUS1_PA           (0xB0)
#define ITEPD_REG_VCMD_STATUS1_PB           (0xB1)
#define ITEPD_REG_VCMD_STATUS2_PA           (0xB2)
#define ITEPD_REG_VCMD_STATUS2_PB           (0xB3)
#define ITEPD_REG_VCMD_STATUS3_PA           (0xB4)
#define ITEPD_REG_VCMD_STATUS3_PB           (0xB5)

#define ITEPD_REG_FW_VER                    (0xB6)
#define ITEPD_REG_PID                       (0xB8)
#define ITEPD_REG_VID                       (0xBA)

#define ITEPD_CLEAR_ALERT_STATUS            (0xBC)
#define ITEPD_ALERT_STATUS                  (0xBD)
#define ITEPD_EXIT_TCPC_MODE                (0xBE)

#define ITEPD_RDO_PA                        (0xC0)
#define ITEPD_RDO_PB                        (0xC4)
#define ITEPD_PDO_PA                        (0xC8)
#define ITEPD_PDO_PB                        (0xCC)
#define ITEPD_ATT_PA                        (0xD0)
#define ITEPD_ATT_PB                        (0xD1)

#define ITEPD_SLEEP                         (0xD2)
#define ITEPD_TIMER_TICK                    (0xD3)
#define ITEPD_TIMER_FREQ                    (0xD4)

#define ITEPD_CHIP_VER                      (0xD5)
#define ITEPD_CHIP_ID                       (0xD6)

#define ITEPD_ENTER_SX                      (0xD8)
#define ITEPD_SRC_PORT_INF                  (0xD9)

#define ITEPD_CURRENT_PA                    (0xDA)
#define ITEPD_CURRENT_PB                    (0xDC)
#define ITEPD_VOLTAGE_PA                    (0xDE)
#define ITEPD_VOLTAGE_PB                    (0xE0)

#define ITEPD_ACPI_ENABLE                   (0xE5)

/*
 * ITE PD controller registers length of byte
 */
#define ITEPD_REG_UCSI_VRESION_LEN          (2)
#define ITEPD_REG_UCSI_CCI_LEN              (4)
#define ITEPD_REG_UCSI_MSGIN_LEN            (16)
#define ITEPD_REG_UCSI_CTRL_LEN             (8)
#define ITEPD_REG_UCSI_MSGOUT_LEN           (16)

#define ITEPD_REG_VCMD_STATUS1_PA_LEN       (1)
#define ITEPD_REG_VCMD_STATUS1_PB_LEN       (1)
#define ITEPD_REG_VCMD_STATUS2_PA_LEN       (1)
#define ITEPD_REG_VCMD_STATUS2_PB_LEN       (1)
#define ITEPD_REG_VCMD_STATUS3_PA_LEN       (1)
#define ITEPD_REG_VCMD_STATUS3_PB_LEN       (1)

#define ITEPD_REG_FW_VER_LEN                (2)
#define ITEPD_REG_PID_LEN                   (2)
#define ITEPD_REG_VID_LEN                   (2)

#define ITEPD_CLEAR_ALERT_STATUS_LEN        (1)
#define ITEPD_ALERT_STATUS_LEN              (1)
#define ITEPD_EXIT_TCPC_MODE_LEN            (1)

#define ITEPD_RDO_PA_LEN                    (4)
#define ITEPD_RDO_PB_LEN                    (4)
#define ITEPD_PDO_PA_LEN                    (4)
#define ITEPD_PDO_PB_LEN                    (4)
#define ITEPD_ATT_PA_LEN                    (1)
#define ITEPD_ATT_PB_LEN                    (1)

#define ITEPD_SLEEP_LEN                     (1)
#define ITEPD_TIMER_TICK_LEN                (1)
#define ITEPD_TIMER_FREQ_LEN                (1)

#define ITEPD_CHIP_VER_LEN                  (1)
#define ITEPD_CHIP_ID_LEN                   (2)

#define ITEPD_ENTER_SX_LEN                  (1)
#define ITEPD_SRC_PORT_INF_LEN              (1)

#define ITEPD_CURRENT_PA_LEN                (2)
#define ITEPD_CURRENT_PB_LEN                (2)
#define ITEPD_VOLTAGE_PA_LEN                (2)
#define ITEPD_VOLTAGE_PB_LEN                (2)

#define ITEPD_ACPI_ENABLE_LEN               (1)

#define ITE_REG_READ                        (1)
#define ITE_REG_WRITE                       (0)

#define ITE_REG_ACCESS_TRTRY_TIME           (1)   /* retry time for I2C comm */

/* length->1 address->0xB0 0xB1 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t portConnected                         :1;
            uint8_t pdNegotiatedDone                      :1;
            uint8_t connectionOrientation                 :1;  /* 0: Normal and 1: Flip */
            uint8_t dataRole                              :1;
            uint8_t powerRole                             :1;
            uint8_t vdmModeEnted                          :1;
            uint8_t connectedUsb3Dev                      :1;
            uint8_t Revd                                  :1;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_REG_VCMD_STATUS1_PA_LEN];
    };
} DEV_ITEX_VCMD_STATUS1;

/* length->1 address->0xB2 0xB3 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t Rsvd                                  :3;
            uint8_t forceTbtMode                          :1;  /* 1: force tbt mode and 0: EC should set as ACK */
            uint8_t bbComplianceMode                      :1;
            uint8_t Rsvd2                                 :3;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_REG_VCMD_STATUS2_PA_LEN];
    };
} DEV_ITEX_VCMD_STATUS2;

/* length->1 address->0xB4 0xB5 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t Rsvd                                  :2;
            uint8_t forceTbtMode                          :1;  /* 1: force tbt mode and 0: EC should set as ACK */
            uint8_t bbComplianceMode                      :1;
            uint8_t Rsvd2                                 :4;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_REG_VCMD_STATUS3_PA_LEN];
    };
} DEV_ITEX_VCMD_STATUS3;

/* length->1 address->0xBC */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t vendorFunctionAlert                   :1;
            uint8_t ucsiAlert                             :1;
            uint8_t Rsvd                                  :6;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_CLEAR_ALERT_STATUS_LEN];
    };
} DEV_ITEX_CLEAR_ALERT_STATUS;

/* length->1 address->0xBD */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t vendorFunctionAlert                   :1;
            uint8_t ucsiAlert                             :1;
            uint8_t Rsvd                                  :6;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_ALERT_STATUS_LEN];
    };
} DEV_ITEX_ALERT_STATUS;

/* length->1 address->0xD8 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t sleep                                 :1;  /* 0: leave Sx and 1: enter Sx */
            uint8_t Rsvd                                  :7;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_ENTER_SX_LEN];
    };
} DEV_ITEX_ENTER_SX;

/* length->1 address->0xD9 */
typedef struct
{    
    uint8_t u8Length;
    union {
        struct {
            /* bytes 1 */
            uint8_t Rsvd                                  :4;
            uint8_t pmcIntPA                              :1;
            uint8_t pmcIntPB                              :1;
            uint8_t Rsvd2                                 :2;
        } PACK_STRUCT f;
        uint8_t buf[ITEPD_SRC_PORT_INF_LEN];
    };
} DEV_ITEX_SRC_PORT_INF;
#pragma pack(pop)


/* Access I2C regs in ITE PD controller */
int amd_crb_drivers_itex_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t port);
/* read sink rdo */
uint32_t amd_crb_drivers_itex_readSnkRdo(uint8_t port);
/* read source pdo */
uint32_t amd_crb_drivers_itex_readSrcPdo(uint8_t port);
/* update the system status */
void amd_crb_drivers_itex_sysStatus(uint8_t port, enum system_power_state sys_status);
/* check interrupt source is UCSI */
bool amd_crb_drivers_itex_alterStatusIsUcsi(uint8_t port);
/* check interrupt source is vendor function alert */
bool amd_crb_drivers_itex_alterStatusIsVendorAlert(uint8_t port);
/* clear the interrupt source */
void amd_crb_drivers_itex_clearAlterStatus(uint8_t port, DEV_ITEX_ALERT_STATUS status);

#endif // end of AMD_CRB_DRIVERS_ITE_PD_H_

