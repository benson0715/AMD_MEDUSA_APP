/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_CCGx_H_
#define APP_CCGx_H_

#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_hpi.h"
#include "system.h"

extern uint8_t g_ccgxUpdateSinkPath;

/* detect IFX PD based on I2C address */
bool app_ccgx_vonderIdentify(void);
/* ccgx init */
void app_ccgx_init(void);
/* ccgx report amd customized data */
void app_ccgx_xyncUpCustomizedByte (void);
/* ccgx interrupt event handler */
void app_ccgx_eventHandler(void);
/* ccgx firmware update */
bool app_ccgx_firmwareUpdate(DRV_HPI_PROGRESS_INDICATOR pStatusCallback,
		                     uint32_t fwOffset, uint32_t fwSize, uint8_t chipID);
/* ccg8 firmware update block */
bool app_ccgx_firmwareUpdateCcg8(DRV_HPI_PROGRESS_INDICATOR pStatusCallback,
							 uint32_t fwOffset, uint32_t fwSize, uint8_t chipID);
/* ccgx firmware update block */
bool app_ccgx_fwxUpdate (uint8_t fwIdx, DRV_HPI_PROGRESS_INDICATOR pStatusCallback,
		                 uint32_t fwOffset, uint32_t fwSize, uint8_t chipID);
bool app_ccgx_fwxUpdateCcg8 (uint8_t fwIdx, DRV_HPI_PROGRESS_INDICATOR pStatusCallback,
		                 uint32_t fwOffset, uint32_t fwSize, uint8_t chipID);
/* compare two version */
bool app_ccgx_fwVerCompare(DRV_HPI_FW_VERSION * pVer1, DRV_HPI_FW_VERSION * pVer2);
/* get Cypress firmware version */
bool app_ccgx_getCypressFwVersion(uint8_t chipID);
/* get Cypress firmware version for ccgx */
bool app_ccgx_getCypressFwVersionCcgx(uint8_t chipID);
/* i2c tunnel to control pd i2c master */
uint8_t app_ccgx_i2cTunnel(bool isRead, uint8_t port, uint8_t address, uint8_t reg, uint8_t dataLen, uint8_t* data, uint8_t chipID);
/* update ccgx system status register */
void app_ccgx_updatePwrStatus(enum system_power_state state);
/* force power on RT */
void app_ccgx_forceRtPower (uint8_t port, uint8_t chipID, bool en);
/* force power on RT and enter USB4/TBT3 mode */
void app_ccgx_forceUsb4Tbt3(uint8_t chipID, bool en);
/* clean the ccgx power on flag in power sequence thread */
void app_ccgx_systemPowerOn(void);
#endif // end of APP_CCGx_H_
