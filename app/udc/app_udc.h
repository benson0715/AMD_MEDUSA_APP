/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_UDC_H__
#define __APP_UDC_H__

#include <stdint.h>
#include <stdbool.h>

#define POSTCODE_PORT80    0
#define POSTCODE_PORT81    1
#define POSTCODE_PORT82    2
#define POSTCODE_PORT83    3

extern bool g_isUartDonglePresent;
extern bool g_isUdcPresent;


/**
 * @brief Refresh udc status.
 */
void app_udc_refreshUdcStatus(void);

/**
 * @brief Refresh udc status.
 */
void app_udc_refreshUdcStatus(void);

/**
 * @brief turn off postcode led.
 */
void app_postcode_turnOff( void );

/**
 * @brief turn on postcode led.
 */
void app_postcode_turnOn( void );

/**
 * @brief reset postcode led to "0.0.0.0.0.0.0.0."
 */
void app_postcode_OnReset (void);

/**
 * @brief set postcode message.
 *
 * @param msgId warn message index.
 */
void app_postcode_setWarnMsg (uint8_t msgId);

/**
 * @brief set postcode led display wrapper.
 *
 * @param val dispaly value.
 * @param limit scan limit.
 * @param points digital point.
 */
void app_udc_displayWrapper(uint32_t val, uint8_t limit, uint8_t points);

/**
 * @brief udc display when press WwanRadio switch.
 */
void app_udc_onWwanRadioClick(void);

/**
 * @brief udc display when press WlanRadio switch.
 */
void app_udc_onWlanRadioClick(void);

/**
 * @brief udc display when long press display switch.
 */
void app_udc_longPress(void);

/**
 * @brief udc display when short press display switch.
 */
void app_udc_shortClick(void);

/**
 * @brief init udc application environment.
 *
 * @retval true if success.
 */
bool app_udc_init( void );

/**
 * @brief Routine that handles udc application.
 * 
 * udc_thread must run after 1.8V EC alw power enabled in amd crb, or init error will 
 * happen. 
 *
 * @param p1 thread run period.
 * @param p2 thread sleep ready.
 * @param p3 reserved.
 */
void udc_thread(void *p1, void *p2, void *p3);

#endif // end of __APP_UDC_H__

