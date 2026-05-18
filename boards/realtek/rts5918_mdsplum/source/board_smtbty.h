/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_SMTBTY_H__
#define __BOARD_SMTBTY_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/sys/util_macro.h>
#include "dev_utility.h"
#include "app_smtbty.h"

extern uint8_t g_batPresentFlag;

struct acpi_reg {
	uint32_t val;
} __packed;
BUILD_ASSERT(sizeof(struct acpi_reg) == 4);

#define ACPI_REG_COUNT 8

struct acpi_regDump {
	uint8_t en_bits;
	struct acpi_reg regs[ACPI_REG_COUNT];
} __packed;
BUILD_ASSERT(sizeof(struct acpi_regDump) == 33);

enum acpi_reg_idx {
	ACPI_REG_IDX_AC_LIMIT1_1 = 0,
	ACPI_REG_IDX_AC_LIMIT1_2,
	ACPI_REG_IDX_AC_LIMIT2_1,
	ACPI_REG_IDX_AC_LIMIT2_2,
	ACPI_REG_IDX_AC_PROCHOT_1,
	ACPI_REG_IDX_AC_PROCHOT_2,
	ACPI_REG_IDX_DC_PROCHOT_LEVEL,
	ACPI_REG_IDX_NUM,
};
#define ACPI_REG_IDX_COUNT  ACPI_REG_IDX_NUM

void     board_battery_notify(uint8_t u8BtyId, uint32_t u32ChangedBits);
uint32_t board_battery_alartStatusGetAndClear(void);
bool     board_battery_isPresent(uint8_t u8BtyId);
bool     board_smtbty_setAcLimit(uint8_t chgId, uint32_t acLimit1, uint32_t acLimit2);
uint16_t board_battery_fullChargeCapacity(void);
uint16_t board_battery_remainingCapacity(void);
uint16_t board_smtbty_calculateProchotLevel(void);
uint32_t board_smtbty_calculateP3tLimit(void);
void     board_smtbty_setSlotPwr(uint32_t pin, uint32_t val);
void     board_smtbty_setProchotGate(void);
bool     board_smtbty_taskInit(void);
void     board_smtbty_chgPwrSavingS0i3Enter(void);
void     board_smtbty_chgPwrSavingS0i3Exit(void);
void     board_smtbty_breath_led(void);
bool     GET_BAT_LED(void);
void     SET_BAT_LED(bool mode);
uint8_t  board_smtbty_info_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);

#endif /* __BOARD_SMTBTY_H__ */
