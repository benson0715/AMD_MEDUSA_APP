/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_DAC_H__
#define __BOARD_DAC_H__

#include <stdint.h>
#include <stdbool.h>

bool board_dac_init(void);
bool board_dac_setValue(uint8_t channel, uint16_t value);
uint16_t board_dac_getValue(uint8_t channel);

#endif /* __BOARD_DAC_H__ */
