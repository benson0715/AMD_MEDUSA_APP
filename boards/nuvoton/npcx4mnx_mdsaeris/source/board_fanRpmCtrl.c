/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/mft_tach_npcx.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "board_fanRpmCtrl.h"

LOG_MODULE_REGISTER(fan_board, CONFIG_FANCTRL_LOG_LEVEL);

/* Function prototypes */
static int board_fan_pwm_tach_init(uint8_t pwmCh, uint8_t tachCh);
static int board_fan_pwm_speed_set(uint8_t ch, uint16_t value);
static int board_fan_tach_rpm_get(uint8_t tachCh);
// static int board_fan_thermal_highest_val_get(void); not used
static int board_fan_thermal_cpu_tj_val_get(void);
static int board_fan_thermal_pcb_sensor_val_get(void);
static int board_fan_power_off(uint8_t logicId);
static int board_fan_power_on(uint8_t logicId);


/* variables shared with fan control */
extern uint16_t _s_u16pcbTmp, _s_u16pcbTmpQ1, _s_u16pcbTmpQ2;
extern uint16_t _s_u16dieTmp;
extern uint16_t _s_u16evalSensor;

/* fan pwm and tach device */
static const struct device *pwm_0_dev;
static const struct device *tach_0_dev;

static fan_ctrl_context_t g_board_fan_ctrl_ctx;

static fan_speed_table_t g_fan_speed_table_init[BOARD_FAN_DEVICE_NUM] = {
	[BOARD_FAN_DEVICE_1] = {					        // For fan device 1
		.profile[FAN_SPEED_TABLE_15W_20W_28W_MODE] = {  // For fan speed table mode
			[FAN_TEMP_TYPE_CPU_TJ] = {					// For thermal sensor CPU_TJ
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{56, 50, 2950}, // {Enable, Disable, RPM}
						{62, 56, 3200},
						{68, 62, 3400},
						{74, 68, 3700},
						{80, 74, 3900},
						{0, 0, 0} /* sentinel */
					},
				},
			},
			[FAN_TEMP_TYPE_PCB_SENSOR] = {  // For thermal sensor PCB_SENSOR
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{47, 44, 2950},
						{50, 47, 3200},
						{54, 51, 3400},
						{57, 54, 3700},
						{59, 56, 3900},
						{0, 0, 0} /* sentinel */
					},
				},
			},
		},
		.profile[FAN_SPEED_TABLE_45W_MODE] = {
			[FAN_TEMP_TYPE_CPU_TJ] = {
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{55, 50, 3200},
						{60, 55, 3500},
						{65, 60, 3800},
						{70, 65, 4300},
						{75, 70, 4600},
						{80, 75, 5150},
						{85, 80, 5850},
						{0, 0, 0} /* sentinel */
					},
				},
			},
			[FAN_TEMP_TYPE_PCB_SENSOR] = {
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{47, 44, 3200},
						{50, 47, 3500},
						{54, 51, 3800},
						{57, 54, 4300},
						{59, 56, 4600},
						{61, 59, 5150},
						{63, 60, 5850},
						{0, 0, 0} /* sentinel */
					},
				},
			},
		},
		.profile[FAN_SPEED_TABLE_54W_MODE] = {
			[FAN_TEMP_TYPE_CPU_TJ] = {
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{55, 50, 3200},
						{60, 55, 3500},
						{65, 60, 4300},
						{70, 65, 4800},
						{75, 70, 5350},
						{80, 75, 5850},
						{85, 80, 6300},
						{0, 0, 0} /* sentinel */
					},
				},
			},
			[FAN_TEMP_TYPE_PCB_SENSOR] = {
				.tb_type = FAN_TABLE_TYPE_STEPWISE,
				.tb_item = {
					.stepwise = {
						{47, 44, 3200},
						{50, 47, 3500},
						{54, 51, 4300},
						{57, 54, 4800},
						{59, 56, 5350},
						{61, 59, 5850},
						{63, 60, 6300},
						{0, 0, 0} /* sentinel */
					},
				},
			},
		},
	}
};

const static control_profile_t s_fan_dev_calibration_init[BOARD_FAN_DEVICE_NUM] =
{
	[BOARD_FAN_DEVICE_1] = {
#if (FAN_CTRL_MODE == FAN_RPM_NO_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_OFFSET_CONTROL)
		.mal_offset = {
			.length = FAN_CTRL_PROFILE_MANUAL_WITH_OFFSET_NUMBER,
			.curve_ofst = {
				{0, 0}, /* sentinel */
			},
		},
#elif (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL)
		.mal_curve = {
			.scale = 10,
			.length = FAN_CTRL_PROFILE_MANUAL_WITH_CURVE_OFFSET_NUMBER,
			// You should enable the macro FAN_CTRL_CALIBRATION_DEBUG, and then dump fan device capbilities.
			.curve_map = {
				{0, 0}, /* sentinel */
			},
		},
#elif (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL)
		.pid_curve = {
			.kp = 17,
			.ki = 3,
			.kd = 1,
			.k_multi = 10,

			.tolerant_err = 25,
		},
#elif (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
		.pid_curve = {
			.kp = 4,
			.ki = 2,
			.kd = 0,
			.k_multi = 100, // Convert the kp, ki and kd value from float to int, (k_multi = 100, kp = 4 ==> kp = 0.04)

			.tolerant_err = 25,
		},
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
		.ramp_rate = {
			.min_start_pwm = 200,
			.up = {
				{500, 40},
				{250, 20},
				{60, 10},
				{30, 5},
				{0, 0}, /* sentinel */
			},
			.down = {
				{250, 20},
				{120, 10},
				{60, 5},
				{30, 2},
				{0, 0}, /* sentinel */
			},
		},
#endif
	}
};

static fan_ctrl_config_init_t g_fan_ctrl_data_init[BOARD_FAN_DEVICE_NUM] =
{
	[BOARD_FAN_DEVICE_1] = {
		.header = {
			.pNext = &g_fan_ctrl_data_init[1].header,
			.logicId = 1,         // Logic device id, often illustrate in the DOC and ECRAM
			.pwmChannel = 0,      // PWM channel
			.tachChannel = 0,     // TACH channel
			.name = "J5801",      // Header name refer to SCH
			.type = FAN_PWM_TACH, // PWM and TACH control type
			.isInit = false,      // Controled by system itself
			.isDisable = false,   // Manually disable this fan
		},
		.profile = {
			.location = FAN_LOCATION_LEFT,                                                          // Fan device for CPU, Liquid, VR, board...
			.tb_select = FAN_SPEED_TABLE_15W_20W_28W_MODE,                                          // Fan speed table select
			.pTable = (fan_speed_table_t *)&g_fan_speed_table_init[BOARD_FAN_DEVICE_1],             // Fan speed table
#if (FAN_CTRL_MODE == FAN_RPM_NO_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_OFFSET_CONTROL || FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL)
			.maxSpeed = 6300,                                                                       // Fan maximum RPM speed value for pwm control
#endif
			.resolution = FAN_CTRL_PWM_RESOLUTION_L2,                                               // PWM control resolution, value = 100 (1/100)
			.ctrl_type = FAN_CTRL_MODE,                                           					// Fan control type
			.pCtrl_profile = (control_profile_t *)&s_fan_dev_calibration_init[BOARD_FAN_DEVICE_1],  // Fan control tuning or configuration data
		},

		/* For debug */
		.overrideTemp = 0,                          // Set a fixed temperature for debug
		.overrideTargetSpeed = 0,                   // Set a fixed fan RPM speed for debug
		.overridePercentage = 0,                    // Set a fixed fan pwm percentage, maximum value = 100

		/* Defined fan control function APIs */
		.pfInit = board_fan_pwm_tach_init,
		.pfSetSpeed = board_fan_pwm_speed_set,
		.pfGetSpeed = board_fan_tach_rpm_get,
		.pfReadTemp[FAN_TEMP_TYPE_CPU_TJ] = board_fan_thermal_cpu_tj_val_get,
		.pfReadTemp[FAN_TEMP_TYPE_PCB_SENSOR] = board_fan_thermal_pcb_sensor_val_get,
		// .pfReadTemp[FAN_TEMP_TYPE_MAIN_SENSOR] = board_fan_thermal_main_sensor_val_get, not used for this project, reserved for future use
		.pfPowerOff = board_fan_power_off,
		.pfPowerOn = board_fan_power_on,
	}
};

/*****************************************************************************
 * Function name:   board_fan_pwm_tach_init
 * Description:     Initialize PWM and tachometer devices for fan control
 * @param           pwmCh  - PWM channel (0 or n)
 * @param           tachCh - Tachometer channel (0 or n)
 * @return          0 on success, negative on error
 *****************************************************************************/
static int board_fan_pwm_tach_init(uint8_t pwmCh, uint8_t tachCh)
{
	if (pwmCh == 0) {
		pwm_0_dev =  DEVICE_DT_GET(PWM_0);
		if (!device_is_ready(pwm_0_dev)) {
			LOG_ERR("pwm_0_dev device not ready");
			return -ENODEV;
		};
	} else {
		LOG_ERR("pwm_%d_dev device doesn't support", pwmCh);
		return -ENODEV;
	}

	if (tachCh == 0) {
		tach_0_dev =  DEVICE_DT_GET(TACH_n);
		if (!device_is_ready(tach_0_dev)) {
			LOG_ERR("tach_0_dev device not ready");
			return -ENODEV;
		};
	} else {
		LOG_ERR("tach_%d_dev device doesn't support", tachCh);
		return -ENODEV;
	}
		
	return 0;
}

/*****************************************************************************
 * Function name:   board_fan_pwm_speed_set
 * Description:     Set PWM duty cycle for fan speed control
 * @param           ch    - PWM channel (0 or n)
 * @param           value - PWM duty cycle value (0 to FAN_CTRL_HW_RANGE_V2)
 * @return          0 on success, negative on error
 *****************************************************************************/
static int board_fan_pwm_speed_set(uint8_t ch, uint16_t value)
{
	const struct device *pwm_dev = NULL;
	uint8_t level = gpio_read_pin(CHG_EC_PROCHOT_N);

	/* do not change PWM when prochot is asserted */
	if (level == 0) {
		return 0;
	}

	if (ch == 0) {
		pwm_dev = pwm_0_dev;
	} else {
		LOG_ERR("PWM channel %d doesn't support", ch);
		return -ENODEV;
	}

	if (value > FAN_CTRL_PWM_RESOLUTION_L2) {
		LOG_ERR("PWM channel %d doesn't support this HW range %d", ch, value);
		return -1;
	}
	uint32_t period = PWM_KHZ(FAN_PWM_DEV_CLK_KHZ);
	uint32_t pulse = (period / FAN_CTRL_PWM_RESOLUTION_L2) * value;

	int ret = pwm_set(pwm_dev, ch, period, pulse, PWM_POLARITY_NORMAL); // ch: not use in nv chip.
	if (ret != 0) {
		LOG_ERR("pwm_%d_dev device channel set fail", ch);
		return ret;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   board_fan_tach_rpm_get
 * Description:     Read fan RPM from tachometer sensor
 * @param           tachCh - Tachometer channel (0 or n)
 * @return          RPM value on success, negative on error
 *****************************************************************************/
static int board_fan_tach_rpm_get(uint8_t tachCh)
{
	struct sensor_value val = {0};
	const struct device *tach_dev = tach_0_dev;

	if (tachCh > 0) {
		LOG_ERR("tach_%d_dev device doesn't support", tachCh);
		return -ENODEV;
	}

	uint8_t ch = (tachCh == 0) ? NPCX_TACH_CHAN_RPM_A : NPCX_TACH_CHAN_RPM_B;
	int ret = sensor_sample_fetch_chan(tach_dev, ch);
	if (ret != 0) {
		LOG_ERR("tach_%d_dev device sample fail", tachCh);
		return ret;
	}

	ret = sensor_channel_get(tach_dev, ch, &val);
	if (ret != 0) {
		LOG_ERR("tach_%d_dev device channel get fail", ch);
		return ret;
	}
	
	return val.val1;
}

#if 0 /* not used */
/*****************************************************************************
 * Function name:   board_fan_thermal_highest_val_get
 * Description:     Get highest temperature from all available thermal sensors
 * @param           None
 * @return          Temperature in Celsius, 80 if no valid reading
 *****************************************************************************/
static int board_fan_thermal_highest_val_get(void)
{
	uint16_t temp = 0;

	if (SYSTEM_S0_STATE != app_pseq_systemState()) {
		return 0;
	}

	if (temp < _s_u16dieTmp) {
		temp = _s_u16dieTmp;
	}

	if (temp < _s_u16pcbTmp) {
		temp = _s_u16pcbTmp;
	}

	if (temp < _s_u16pcbTmpQ1) {
		temp = _s_u16pcbTmpQ1;
	}

	if (temp < _s_u16pcbTmpQ2) {
		temp = _s_u16pcbTmpQ2;
	}

	if (temp < _s_u16evalSensor) {
		temp = _s_u16evalSensor;
	}

	if (temp == 0) {
		return 80;
	}

	return ((temp) >> 8);
}
#endif

/*****************************************************************************
 * Function name:   board_fan_thermal_cpu_tj_val_get
 * Description:     Get CPU TJ temperature from thermal sensor
 * @param           None
 * @return          Temperature in Celsius
 *****************************************************************************/
static int board_fan_thermal_cpu_tj_val_get(void)
{
	if (SYSTEM_S0_STATE != app_pseq_systemState()) {
		return 0;
	}

	return ((_s_u16dieTmp) >> 8); // CPU internal temperature
}

/*****************************************************************************
 * Function name:   board_fan_thermal_pcb_sensor_val_get
 * Description:     Get PCB sensor temperature from thermal sensor
 * @param           None
 * @return          Temperature in Celsius
 *****************************************************************************/
static int board_fan_thermal_pcb_sensor_val_get(void)
{
	if (SYSTEM_S0_STATE != app_pseq_systemState()) {
		return 0;
	}

	return ((_s_u16pcbTmpQ1) >> 8); // Near APU
}

/*****************************************************************************
 * Function name:   board_fan_power_off
 * Description:     Power off fan
 * @param           logicId -  logic id of the fan
 * @return          0 on success
 *****************************************************************************/
static int board_fan_power_off(uint8_t logicId)
{
    // TODO: Implement fan power off， a few fan device can't be power off by setting pwm to 0.
	return 0;
}

/*****************************************************************************
 * Function name:   board_fan_power_on
 * Description:     Power on fan
 * @param           logicId - logic id of the fan
 * @return          0 on success
 *****************************************************************************/
static int board_fan_power_on(uint8_t logicId)
{
	// TODO: Implement fan power on
	return 0;
}

/*****************************************************************************
 * Function name:   board_fan_dev_init
 * Description:     Initialize individual fan device with configuration
 * @param           pCtrlContext - Fan control context
 * @param           pDevContext  - Device context to initialize
 * @param           pConfgInit   - Initial configuration data
 * @return          None
 *****************************************************************************/
static void board_fan_dev_init(fan_ctrl_context_t *pCtrlContext, fan_dev_ctrl_context_t *pDevContext, const fan_ctrl_config_init_t *pConfgInit)
{
	if (pConfgInit == NULL) {
		return ;
	}

	pDevContext->header = pConfgInit->header;
	pDevContext->header.pNext = pCtrlContext->pManagerHeader;
	pCtrlContext->pManagerHeader = &pDevContext->header;

	pDevContext->profile = pConfgInit->profile;
	pDevContext->overrideTemp = pConfgInit->overrideTemp;
	pDevContext->overrideTargetSpeed = pConfgInit->overrideTargetSpeed;
	pDevContext->overridePercentage = pConfgInit->overridePercentage;
	pDevContext->pfInit =  pConfgInit->pfInit;
	pDevContext->pfGetSpeed = pConfgInit->pfGetSpeed;
	pDevContext->pfSetSpeed = pConfgInit->pfSetSpeed;
	pDevContext->pfPowerOff = pConfgInit->pfPowerOff;
	pDevContext->pfPowerOn = pConfgInit->pfPowerOn;
	for (uint8_t i = 0; i < FAN_TEMP_TYPE_NUMBER; i++) {
		pDevContext->pfReadTemp[i] = pConfgInit->pfReadTemp[i];
	}

	if ((pDevContext->pfInit != NULL) && (!pDevContext->header.isDisable)) {
		if (pDevContext->pfInit(pDevContext->header.pwmChannel, pDevContext->header.tachChannel) != 0) {
			pDevContext->header.isInit = false;
		} else {
			pDevContext->header.isInit = true;
		}
	}

	LOG_INF("fan dev %d inited %d, disabled %d.", pDevContext->header.logicId, pDevContext->header.isInit, pDevContext->header.isDisable);
}

/*****************************************************************************
 * Function name:   board_fan_ctrl_init
 * Description:     Initialize all fan control devices on the board
 * @param           None
 * @return          None
 *****************************************************************************/
void board_fan_ctrl_init(void)
{
	uint8_t i = 0;
	for (i = 0; i < BOARD_FAN_DEVICE_NUM; i++) {
		board_fan_dev_init(&g_board_fan_ctrl_ctx, &g_board_fan_ctrl_ctx.dev_ctrl_context[i], &g_fan_ctrl_data_init[i]);
	}
}

/*****************************************************************************
 * Function name:   board_fan_ctrl_mngrHeaderGet
 * Description:     Get pointer to fan control manager header
 * @param           None
 * @return          Pointer to fan manager header
 *****************************************************************************/
fan_header_t *board_fan_ctrl_mngrHeaderGet(void)
{
	return (fan_header_t *)g_board_fan_ctrl_ctx.pManagerHeader;
}

/*****************************************************************************
 * Function name:   board_fan_ctrl_ctxGet
 * Description:     Get pointer to fan control context
 * @param           None
 * @return          Pointer to fan control context
 *****************************************************************************/
fan_ctrl_context_t *board_fan_ctrl_ctxGet(void)
{
	return (fan_ctrl_context_t *)&g_board_fan_ctrl_ctx;
}

/*****************************************************************************
 * Function name:   board_fan_ctrl_numGet
 * Description:     Get number of fan devices on the board
 * @param           None
 * @return          Number of fan devices
 *****************************************************************************/
uint8_t board_fan_ctrl_numGet(void)
{
	return BOARD_FAN_DEVICE_NUM;
}