/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef _F_BOARD_FANRPMCTRL_H__
#define _F_BOARD_FANRPMCTRL_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include <soc.h>
#include "board_config.h"

/* Able has one fan */
enum fan_dev {
	BOARD_FAN_DEVICE_1 = 0,
	
	BOARD_FAN_DEVICE_NUM,
};

#define FAN_RPM_NO_CONTROL                     (0)
#define FAN_RPM_MANUAL_WITH_OFFSET_CONTROL     (1)
#define FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL  (2)
#define FAN_RPM_PID_DISCRETIZE_CONTROL         (3)
#define FAN_RPM_PID_INCREMENTAL_CONTROL        (4)
#define FAN_RPM_RAMP_RATE_CONTROL              (5)

#ifndef FAN_PWM_DEV_CLK_KHZ
#define FAN_PWM_DEV_CLK_KHZ	(25u) // Pls follow the fan device spec
#endif

/* No control */
// #define FAN_CTRL_MODE                                    FAN_RPM_NO_CONTROL

/* Manual with offset */
// #define FAN_CTRL_MODE                                    FAN_RPM_MANUAL_WITH_OFFSET_CONTROL

/* Manual with curve map */
// #define FAN_CTRL_MODE                                    FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL

/* PID Discretize */
// #define FAN_CTRL_MODE                                    FAN_RPM_PID_DISCRETIZE_CONTROL

/* PID Incremental */
// #define FAN_CTRL_MODE                                    FAN_RPM_PID_INCREMENTAL_CONTROL

/* Ramp Rate Control */
#define FAN_CTRL_MODE                                    FAN_RPM_RAMP_RATE_CONTROL

#define FAN_CTRL_CALIBRATION_DEBUG                       (0) // To dump fan device capabilities
#define FAN_CTRL_PARAM_DEBUG                             (0) // To dump fan control parameters

#define FAN_CTRL_CALIBRACTION_TIMEOUT_MS                 (10000u) // 10s
#define FAN_CTRL_HEADER_NAME_LENGTH                      (7u)
#define FAN_CTRL_DUMP_CALIBRATION_DATA_SCALE             (5u)
#define FAN_CTRL_OVERRIDE_PERCENTAGE_SCALE               (100u)
#define FAN_CTRL_OVERRIDE_SPEED_ENABLE_MASK              (0x80u)
#define FAN_CTRL_THERMAL_DECREASE_SENSITIVITY            (1u)
#define FAN_CTRL_OVERRIDE_SPEED_VALID_MASK               (0x7Fu)
#define FAN_CTRL_SPEED_TABLE_ITEM_NUM                    (10u)
#define FAN_CTRL_PROFILE_MANUAL_WITH_OFFSET_NUMBER       (10u)
#define FAN_CTRL_PROFILE_MANUAL_WITH_CURVE_OFFSET_NUMBER (20u)
#define FAN_CTRL_PWM_RESOLUTION_L1                       (100u)
#define FAN_CTRL_PWM_RESOLUTION_L2                       (1000u)
#define FAN_CTRL_PWM_RESOLUTION_L3                       (10000u)

#if (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
#define FAN_CTRL_PROC_TIMEOUT_MS                         (1000u) // Thermal sample time period (1s)
#define FAN_CTRL_PROC_SUB_TIMEOUT_MS                     (1000u) // Fan control time period (1s)
#else
#define FAN_CTRL_PROC_TIMEOUT_MS                         (1250u) // Thermal sample time period (1.25s)
#define FAN_CTRL_PROC_SUB_TIMEOUT_MS                     (125u)  // Fan control time period (0.125s)
#endif

#define FAN_ABS(a, b)                                            ((a) >= (b) ? ((a) - (b)) : ((b) - (a)))
#define FAN_CONVERT_TARGETRPM_TO_TARGETCNT(val, mini, max, rang) (((val) >= ((max) - (mini))) ? (rang) : (((val)*(rang))/((max) - (mini))))

enum temp_type {
	FAN_TEMP_TYPE_CPU_TJ = (0u),
	FAN_TEMP_TYPE_PCB_SENSOR,
	// FAN_TEMP_TYPE_MAIN_SENSOR, not used for this project, reserved for future use
	FAN_TEMP_TYPE_NUMBER,
};

enum speed_mode {
	FAN_SPEED_TABLE_15W_20W_28W_MODE = (0u),
	FAN_SPEED_TABLE_45W_MODE,
	FAN_SPEED_TABLE_54W_MODE,

	FAN_SPEED_TABLE_MODE_NUM,
};

enum table_type {
	FAN_TABLE_TYPE_LINED = (0u),
	FAN_TABLE_TYPE_STEPWISE,
};

struct tbi_common {
	uint8_t temp_1;
	uint8_t temp_2;
	uint16_t speed;
};

struct tbi_line {
	uint8_t temp;
	uint8_t hysteresis;
	uint16_t speed;
};

struct tbi_stepwise {
	uint8_t up_temp; 
	uint8_t down_temp; 
	uint16_t speed;
};

struct table_item {
	union {
		struct tbi_common common[FAN_CTRL_SPEED_TABLE_ITEM_NUM];
		struct tbi_line line[FAN_CTRL_SPEED_TABLE_ITEM_NUM];
		struct tbi_stepwise stepwise[FAN_CTRL_SPEED_TABLE_ITEM_NUM];
	};
};

struct profile_table {
	uint8_t spike_temp;
	uint8_t tb_type;
	struct table_item tb_item;
};

typedef struct {
	struct profile_table profile[FAN_SPEED_TABLE_MODE_NUM][FAN_TEMP_TYPE_NUMBER];
} fan_speed_table_t;

typedef enum {
	FAN_RPM_PWM  = (0u), // Pure hardware mode
	FAN_PWM, // Only PWM control
	FAN_PWM_TACH, // Has PWM and TACH

	FAN_TYPE_MAX_NUMBER,
} fan_type_t;

typedef enum {
	FAN_LOCATION_CPU  = (0u),
	FAN_LOCATION_LIQUID,
	FAN_LOCATION_VR,
	FAN_LOCATION_BOARD,
	FAN_LOCATION_LEFT,
	FAN_LOCATION_RIGHT,
} fan_location_t;

#define FAN_RPM_MAX_SPEED_SUPPORT_CONTROL_MSK (BIT(FAN_RPM_NO_CONTROL) | \
												BIT(FAN_RPM_MANUAL_WITH_OFFSET_CONTROL) | \
												BIT(FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL) | \
												BIT(FAN_RPM_PID_DISCRETIZE_CONTROL))

#define FAN_CTRL_RAMP_RATE_TABLE_SIZE (10u)

struct ramp_rate_table {
	uint16_t rpm;
	uint16_t pwm;
};

struct ramp_rate {
	uint16_t min_start_pwm;
	struct ramp_rate_table up[FAN_CTRL_RAMP_RATE_TABLE_SIZE];
	struct ramp_rate_table down[FAN_CTRL_RAMP_RATE_TABLE_SIZE];
};

struct manual_offset {
	int tolerance;
	int offset;
};

typedef struct {
	uint8_t length;
	struct manual_offset curve_ofst[FAN_CTRL_PROFILE_MANUAL_WITH_OFFSET_NUMBER];
} fan_mal_ofst_t;

struct manual_curve {
	uint16_t setSpeed;	   // setSpeed
	uint16_t expectSpeed;  // expect target
};

typedef struct {
	uint8_t scale;
	uint8_t length;

	struct manual_curve curve_map[FAN_CTRL_PROFILE_MANUAL_WITH_CURVE_OFFSET_NUMBER];
} fan_mal_curve_t;

typedef struct {
	bool started;
	int target;
	int actual;
	int out;
	int pre_err;
	int ppre_err; // For Incremental
	int inter_err; // For Discretize

	uint32_t k_multi;
	int kp;
	int ki;
	int kd;

	int tolerant_err;
	int ajust_cnt;
} fan_pid_curve_t;

typedef struct {
	union {
#if (FAN_CTRL_MODE == FAN_RPM_NO_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_OFFSET_CONTROL)
	    fan_mal_ofst_t mal_offset;
#elif (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL)
	    fan_mal_curve_t mal_curve;
#elif (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	    fan_pid_curve_t pid_curve;
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
		struct ramp_rate ramp_rate;
#endif
	};
} control_profile_t;

typedef struct {
	fan_location_t location;
	uint8_t tb_select;
	fan_speed_table_t *pTable;
	uint16_t miniSpeed;
	uint16_t maxSpeed;
	uint16_t resolution;
	uint8_t ctrl_type;
	control_profile_t *pCtrl_profile;
} fan_profile_t;

typedef struct fan_header {
	struct fan_header *pNext;
	uint8_t logicId;
	uint8_t pwmChannel;
	uint8_t tachChannel;
	uint8_t name[FAN_CTRL_HEADER_NAME_LENGTH];
	fan_type_t type;
	bool isInit;
	bool isDisable;
} fan_header_t;

typedef struct {
	fan_header_t header;
	fan_profile_t profile;
	uint8_t overrideTemp;
	uint16_t overrideTargetSpeed;
	uint8_t overridePercentage;
	int (*pfInit)(uint8_t, uint8_t);
	int (*pfSetSpeed)(uint8_t, uint16_t);
	int (*pfGetSpeed)(uint8_t);
	int (*pfReadTemp[FAN_TEMP_TYPE_NUMBER])(void);
	int (*pfPowerOff)(uint8_t);
	int (*pfPowerOn)(uint8_t);
} fan_ctrl_config_init_t;

typedef struct {
	fan_header_t header;
	fan_profile_t profile;
	uint8_t overrideTemp;
	uint16_t overrideTargetSpeed;
	uint8_t overridePercentage;

	int (*pfInit)(uint8_t, uint8_t);
	int (*pfGetSpeed)(uint8_t);
	int (*pfSetSpeed)(uint8_t, uint16_t);
	int (*pfReadTemp[FAN_TEMP_TYPE_NUMBER])(void);
	int (*pfPowerOff)(uint8_t);
	int (*pfPowerOn)(uint8_t);

	uint16_t reportInputTemp;
	int reportSpeed;
	uint16_t reporCalculatedSpeed;
	uint16_t reportRealRpm;
	uint16_t pwm_value;
} fan_dev_ctrl_context_t;

typedef struct {
	fan_header_t *pManagerHeader;
	fan_dev_ctrl_context_t dev_ctrl_context[BOARD_FAN_DEVICE_NUM];
} fan_ctrl_context_t;

/*****************************************************************************
 * Function name:   board_fan_ctrl_init
 * Description:     Initialize all fan control devices on the board
 * @param           None
 * @return          None
 *****************************************************************************/
void board_fan_ctrl_init(void);

/*****************************************************************************
 * Function name:   board_fan_ctrl_mngrHeaderGet
 * Description:     Get pointer to fan control manager header
 * @param           None
 * @return          Pointer to fan manager header
 *****************************************************************************/
fan_header_t *board_fan_ctrl_mngrHeaderGet(void);

/*****************************************************************************
 * Function name:   board_fan_ctrl_ctxGet
 * Description:     Get pointer to fan control context
 * @param           None
 * @return          Pointer to fan control context
 *****************************************************************************/
fan_ctrl_context_t *board_fan_ctrl_ctxGet(void);

/*****************************************************************************
 * Function name:   board_fan_ctrl_numGet
 * Description:     Get number of fan devices on the board
 * @param           None
 * @return          Number of fan devices
 *****************************************************************************/
uint8_t board_fan_ctrl_numGet(void);

#endif // end of _F_BOARD_FANRPMCTRL_H__
