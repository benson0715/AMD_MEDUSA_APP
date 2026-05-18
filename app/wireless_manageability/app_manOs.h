/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_MANOS_H__
#define __APP_MANOS_H__

#define MANOS_EC_OFFSET_PLANE_SELECTOR   0x31
#define   MANOS_EC_PLANE_ID              0xC0

/* get the manOS enable flag */
bool app_manOs_getEnFlag(void);
/* set the manOS enable flag */
void app_manOs_setEnFlag(bool isEn);
/* manOS init */
void app_manOs_init(void);
/* manOS hook in S5 enter */
void app_manOs_EnterS5Hook (void);
/* manOS hook in S5 exit */
void app_manOs_ExitS5Hook  (void);
/* manOS hook in S3 enter */
void app_manOs_EnterS3Hook (void);
/* manOS hook in S0 enter */
void app_manOs_EnterS0Hook (void);
/* manOS hook in S0 exit */
void app_manOs_ExitS0Hook  (void);
/* manOS hook in power button handler */
void app_manOs_PwrBtnHook  (void);
/* manOS hook in APU reset handler */
void app_manOs_ApuRstHook  (void);
/* manOS hook in power source change */
void app_manOs_PwrSourceChange(void);

#endif // end of __APP_MANOS_H__

