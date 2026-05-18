/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_MAX695X_H__
#define __DEV_MAX695X_H__

#include <stdint.h>
#include <stdbool.h>

#define DEV_MAX695X_DECODE_REG      0x01
#define DEV_MAX695X_INTENSITY_REG   0x02
#define DEV_MAX695X_SCANLIMIT_REG   0x03  // The scan-limit register sets the number of digits displayed, from one to four (Table 14).
#define DEV_MAX695X_CONFIG_REG      0x04
#define DEV_MAX695X_TEST_REG        0x07
#define DEV_MAX695X_P0_REG          0x20
#define DEV_MAX695X_DP_REG          0x24

/***************************************
 *
 * MAX695x 32-bit mode
 *
 ***************************************/
/* max695x device init */
void dev_max695x_Init(uint8_t i2cPort);
/* max695x reset */
void dev_max695x_32bit_reset(void);
/* max695x reset 7-Seg LED to "0.0.0.0.0.0.0.0." */
void dev_max695x_32bit_DPonly(void);
/* max695x turn on postcode led */
void dev_max695x_32bit_turnOn(uint8_t brightness);
/* max695x turn off postcode led */
void dev_max695x_32bit_turnOff(void);
/* max695x show postcode */
void dev_max695x_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit);
/* max695x modify scan limit */
void dev_max695x_32bit_scanLimit(uint8_t limit);
/* max695x modify digital point */
void dev_max695x_32bit_DPs(uint8_t turnOnBits);
/* max695x show content by rawdata */
void dev_max695x_show(uint32_t rawH, uint32_t rawL, uint8_t dp, uint8_t limit, bool forceWrite);
/* max695x format print */
bool dev_max695x_print(const char * format, ...);
/* max695x set postcode led status */
void dev_max695x_set_sts(bool status);
/* max695x get postcode led status */
bool dev_max695x_get_sts(void);


#endif // end of __DEV_MAX695X_H__

