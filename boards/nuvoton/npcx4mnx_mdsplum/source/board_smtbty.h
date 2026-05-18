/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_SMTBTY_H__
#define __BOARD_SMTBTY_H__

#include "dev_utility.h"
#include "app_smtbty.h"

extern uint8_t g_batPresentFlag;

struct acpi_reg {
	uint32_t val;
}__packed;
BUILD_ASSERT(sizeof(struct acpi_reg) == 4);

struct acpi_regDump {
	uint8_t en_bits;

#define ACPI_REG_COUNT 8
	struct acpi_reg regs[ACPI_REG_COUNT];
}__packed;
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

/* board battery status change notify */
void board_battery_notify(uint8_t u8BtyId, uint32_t u32ChangedBits);
/* get and clear the battery alart status */
uint32_t board_battery_alartStatusGetAndClear(void);
/* if battery is presented or not */
bool board_battery_isPresent(uint8_t u8BtyId);
/* set the charger ac limit */
bool board_smtbty_setAcLimit(uint8_t chgId, uint32_t acLimit1, uint32_t acLimit2);
/* return the battery full charge capability */
uint16_t board_battery_fullChargeCapacity (void);
/* return the battery remaining capability */
uint16_t board_battery_remainingCapacity (void);
/* calculate the prochot level */
uint16_t board_smtbty_calculateProchotLevel (void);
/* calculate the p3t limit */
uint32_t board_smtbty_calculateP3tLimit(void);
/* set the slot power */
void board_smtbty_setSlotPwr(uint32_t pin, uint32_t val);
/* set up the prochot threshold based on the power source fake or real AC/DC */
void board_smtbty_setProchotGate(void);
/* smart battery task init */
bool board_smtbty_taskInit(void);
/* charger power saving when enter S0i3 */
void board_smtbty_chgPwrSavingS0i3Enter(void);
/* charger power saving resume when exit S0i3 */
void board_smtbty_chgPwrSavingS0i3Exit(void);
/* charger status indicated by LED */
void board_smtbty_breath_led(void);
/* BAT LED status get */
bool GET_BAT_LED(void);
/* BAT LED status set */
void SET_BAT_LED(bool mode);
/* report smart battery information by ACPI handler */
uint8_t board_smtbty_info_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __BOARD_SMTBTY_H__
