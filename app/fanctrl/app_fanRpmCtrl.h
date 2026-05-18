/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_FANRPMCTRL_H__
#define __APP_FANRPMCTRL_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include "board_fanRpmCtrl.h"

#define FAN_DBG_PWM_DIS   (0xFFu)
#define FAN_DBG_TEMP_DIS  (0xFFu)
#define FAN_DBG_PID_DIS   (0u)
#define FAN_DBG_RAMP_DIS  (0u)
#define FAN_DBG_TABLE_DIS (0u)

#define FAN_DBG_PID_FLAG_DUMP		 (0u)
#define FAN_DBG_PID_FLAG_SET_ONGOING (1u)
#define FAN_DBG_PID_FLAG_SET_DONE	 (2u)

#define FAN_DBG_RAMP_FLAG_DUMP		  (0u)
#define FAN_DBG_RAMP_FLAG_SET_ONGOING (1u)
#define FAN_DBG_RAMP_FLAG_SET_DONE	  (2u)

#define FAN_DBG_TABLE_FLAG_DUMP		   (0u)
#define FAN_DBG_TABLE_FLAG_SET_ONGOING (1u)
#define FAN_DBG_TABLE_FLAG_SET_DONE	   (2u)

/**
 * struct fan_dbg_sys_pid - Debug structure for PID parameters
 * @rpm_wd:     PID write RPM value (address offset 0)
 * @rpm_rd:     PID read RPM value (address offset 2)
 * @multi:      PID multiplier (address offset 4)
 * @kp:         PID proportional gain (address offset 6)
 * @ki:         PID integral gain (address offset 8)
 * @kd:         PID derivative gain (address offset 10)
 * @tolerance:  PID tolerance (address offset 12)
 * @ajust:      PID adjustment (address offset 13)
 */
struct fan_dbg_sys_pid {
	uint16_t rpm_wd;     /* 0x00: PID write RPM value */
	uint16_t rpm_rd;     /* 0x02: PID read RPM value */

	uint16_t multi;      /* 0x04: PID multiplier */
	uint16_t kp;         /* 0x06: PID proportional gain */
	uint16_t ki;         /* 0x08: PID integral gain */
	uint16_t kd;         /* 0x0A: PID derivative gain */

	uint8_t tolerance;   /* 0x0C: PID tolerance */
	uint8_t ajust;       /* 0x0D: PID adjustment */
} __packed;

struct fan_dbg_sys_ramp {
	uint16_t rpm_wd;     /* 0x00: write RPM value */
	uint16_t rpm_rd;     /* 0x02: read RPM value */
} __packed;


/**
 * struct fan_dbg_sys_context - Debug context for system fan control
 * @pwm_w:      PWM write value (address offset 0x00)
 * @temp_w:     Temperature write value (address offset 0x01)
 * @temp_r:     Temperature read value (address offset 0x02)
 * @table_rw:   Table read/write selector (address offset 0x03)
 * @rpm_r1:     Fan 1 RPM read value (address offset 0x04)
 * @rpm_r2:     Fan 2 RPM read value (address offset 0x06)
 * @rpm_r3:     Fan 3 RPM read value (address offset 0x08)
 * @rpm_r4:     Fan 4 RPM read value (address offset 0x0A)
 * @rpm_r5:     Fan 5 RPM read value (address offset 0x0C)
 * @rpm_r6:     Fan 6 RPM read value (address offset 0x0E)
 * @pid_flag:   PID debug flag (address offset 0x10)
 * @pid_sel:    PID selection (address offset 0x11)
 * @pid:        PID debug structure (address offset 0x12, size 14 bytes)
 */
struct fan_dbg_sys_context {
	uint8_t pwm_w;        /* 0x00: PWM write value */
	uint8_t temp_w;       /* 0x01: Temperature write value */
	uint8_t temp_r;       /* 0x02: Temperature read value */
	uint8_t table_rw;     /* 0x03: Table speed table selector */
	uint16_t rpm_r1;      /* 0x04: Fan 1 RPM read */
	uint16_t rpm_r2;      /* 0x06: Fan 2 RPM read */
	uint16_t rpm_r3;      /* 0x08: Fan 3 RPM read */
	uint16_t rpm_r4;      /* 0x0A: Fan 4 RPM read */
	uint16_t rpm_r5;      /* 0x0C: Fan 5 RPM read */
	uint16_t rpm_r6;      /* 0x0E: Fan 6 RPM read */

#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	uint8_t pid_flag;     /* 0x10: PID debug flag */
	uint8_t pid_sel;      /* 0x11: PID selection */
	struct fan_dbg_sys_pid pid; /* 0x12: PID debug structure (14 bytes) */
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
	uint8_t ramp_flag;
	uint8_t ramp_sel;
	struct fan_dbg_sys_ramp ramp;
#endif
} __packed;
#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
BUILD_ASSERT(sizeof(struct fan_dbg_sys_context) == 32, "Invalid fan_dbg_sys_context structure!");
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
BUILD_ASSERT(sizeof(struct fan_dbg_sys_context) == 22, "Invalid fan_dbg_sys_context structure!");
#endif
/**
 * struct fan_dbg_table_item - Debug item for fan speed table
 * @temp_1:   First temperature threshold (address offset 0)
 * @temp_2:   Second temperature threshold (address offset 1)
 * @speed:    Fan speed value (address offset 2, 2 bytes)
 */
struct fan_dbg_table_item {
	uint8_t temp_1;   /* 0x00: First temperature threshold */
	uint8_t temp_2;   /* 0x01: Second temperature threshold */
	uint16_t speed;   /* 0x02: Fan speed value */
} __packed;

/**
 * struct fan_dbg_table - Debug structure for a fan speed table
 * @active_mode:  Currently active fan mode (address offset 0)
 * @active_temp:  Currently active temperature type (address offset 1)
 * @item:         Array of table items (address offset 2, size: FAN_CTRL_SPEED_TABLE_ITEM_NUM)
 */
struct fan_dbg_table {
	uint8_t active_mode;   /* 0x00: Active fan mode */
	uint8_t active_temp;   /* 0x01: Active temperature type */
	struct fan_dbg_table_item item[FAN_CTRL_SPEED_TABLE_ITEM_NUM]; /* 0x02: Table items */
} __packed;

/**
 * struct fan_dbg_table_context - Debug context for fan speed table
 * @table_flag:   Table operation flag (address offset 0)
 * @dev_sel:      Device selection (address offset 1)
 * @table:        Fan debug table (address offset 2)
 */
struct fan_dbg_table_context {
	uint8_t table_flag;    /* 0x00: Table operation flag */
	uint8_t dev_sel;       /* 0x01: Device selection */
	struct fan_dbg_table table; /* 0x02: Fan debug table */
} __packed;
BUILD_ASSERT(sizeof(struct fan_dbg_table_context) == 44, "Invalid fan_dbg_table_context structure!");

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_speed_table_select
 * Description:     Set or get a fan speed table type
 * @param           *value - fan speed table select
 * @param           code  - 0: read, 1: write
 * @return          NONE
 *****************************************************************************/
void app_fan_ctrl_sys_speed_table_select(uint8_t *value, uint8_t code);

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_disable
 * Description:     Disable fan control system
 * @param           None
 * @return          None
 *****************************************************************************/
void app_fan_ctrl_sys_disable(void);

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_enable
 * Description:     Enable fan control system
 * @param           None
 * @return          None
 *****************************************************************************/
void app_fan_ctrl_sys_enable(void);

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_enabled
 * Description:     Check if fan control system is enabled
 * @param           None
 * @return          True if enabled, false otherwise
 *****************************************************************************/
bool app_fan_ctrl_sys_enabled(void);

/*****************************************************************************
 * Function name:   fanctrl_thread
 * Description:     Main fan control thread - manages system fan operation
 * @param           p1, p2, p3 - Standard Zephyr thread parameters (unused)
 * @return          None (infinite loop)
 *****************************************************************************/
void fanctrl_thread(void *p1, void *p2, void *p3);

#endif // end of __APP_FANRPMCTRL_H__

