/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_RTK_UCSI_H_
#define AMD_CRB_DRIVERS_RTK_UCSI_H_

#include "amd_crb_drivers_pd.h"

/* Initialize the UCSI interface. */
void amd_crb_drivers_rtk_ucsi_init(void);
/* read or write the ucsi reg */
void amd_crb_drivers_rtk_ucsi_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len);
/* task the handle the rtk ucsi state machine */
void amd_crb_drivers_rtk_ucsi_task(void);
/* PD event callback */
void amd_crb_drivers_rtk_ucsi_pdEventHandler(uint8_t port, app_evt_t evt, alt_mode_app_evt_t alt_mode_event);
/* ucsi read done callback to clear the read pending flag */
void amd_crb_drivers_rtk_ucsi_acpiReadDone(void);

#endif /* AMD_CRB_DRIVERS_RTK_UCSI_H_ */

/* [] END OF FILE */
