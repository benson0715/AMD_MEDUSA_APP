/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_UCSI_H_
#define AMD_CRB_DRIVERS_UCSI_H_

#include "amd_crb_drivers_pd.h"

/* Maximum number of SVIDs that the module stores and returns */
#define UCSI_MAX_SVIDS                       (4u)

/* EC to handle the get_pdos conn but not by TI controller command/response. */
#define UCSI_EC_HANDLE_GET_PDO_CONN          (1)

/* Read the ucsi get_pdos partner response from TI PD controller 0x30/0x31 but not from data1. */
#define UCSI_READ_GET_PDO_REGISTER           (1)

/* UCSI Control Register Start value. This commands starts the UCSI Interface. */
#define UCSI_START                           (0x01)

/* UCSI Control Register Stop value. This command stops the UCSI interface. */
#define UCSI_STOP                            (0x02)

/* UCSI Control Register Silence value. This command silences the UCSI Port. */
#define UCSI_SILENCE                         (0x03)


/* Initialize the UCSI interface. */
void amd_crb_drivers_ec_ucsi_init(void);
/* Run the UCSI state machine */
/*
 * This function handles the commands from the EC through the UCSI registers. UCSI writes from the EC
 * are handled in interrupt context, all UCSI commands are sent in sequence by OPM. There is no event
 * queue maintained for UCSI interface.
 */
void amd_crb_drivers_ec_ucsi_task(void);
/* Handler for PD events reported. Internal function used to receive PD events from the stack and to update the UCSI registers. */
void amd_crb_drivers_ec_ucsi_pdEventHandler(uint8_t port, app_evt_t evt, alt_mode_app_evt_t alt_mode_event);
/* Function to update UCSI notifications */
void amd_crb_drivers_ec_ucsi_notify(uint8_t port, uint32_t notification);
/* Clear status & control register.*/
void amd_crb_drivers_ec_ucsi_regReset(void);
/* Clear appropriate bit_idx to 0 in the status register. */
void amd_crb_drivers_ec_ucsi_clearStatusBit(uint8_t bit_idx);
/* Set appropriate bit_idx to 1 in the status register. */
void amd_crb_drivers_ec_ucsi_setStatusBit(uint8_t bit_idx);
/* read or write the ucsi reg */
void amd_crb_drivers_ec_ucsi_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len);
/* ucsi callback for ucsi commnad to pd controller.*/
void amd_crb_drivers_ec_ucsi_cmdCb(uint8_t port, uint8_t* pData);
/* ucsi callback for alt mode related command to pd controller. */
void amd_crb_drivers_ec_ucsi_altModeCmdCb(uint8_t port, uint8_t* pData);
/* APCI read only feedback to UCSI. */
void amd_crb_drivers_ec_ucsi_acpiReadDone(void);
/* ucsi ccgx chip selector based on message */
uint8_t amd_crb_drivers_ec_ucsi_cypdSelector(volatile uint8_t *Ctrl, uint8_t PrevCommPort);
/* adjust the port number. */
void amd_crb_drivers_ec_ucsi_cypdAdjustPort(volatile uint8_t *Ctrl);
/* set ucsi status */
#if UCSI_REV_1_x_ENABLE
void amd_crb_drivers_ec_ucsi_setStatus(uint8_t status);
#elif UCSI_REV_2_x_ENABLE
void amd_crb_drivers_ec_ucsi_setStatus(uint16_t status);
#endif

#endif /* AMD_CRB_DRIVERS_UCSI_H_ */

/* [] END OF FILE */
