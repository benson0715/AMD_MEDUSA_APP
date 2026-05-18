/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <stdlib.h>
#include "app_fanRpmCtrl.h"
#include "app_pseq.h"
#include "app_acpi.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(fanctrl, CONFIG_FANCTRL_LOG_LEVEL);

uint8_t *task_fanctrlSlpReady;
static bool g_fan_ctrl_enabled = false;
static struct fan_dbg_sys_context g_fan_sys_dbg_ctx = {0};
static struct fan_dbg_table_context g_fan_tb_dbg_ctx = {0};

/*****************************************************************************
 * Function name:   app_fan_ctrl_line_speed
 * Description:     Calculate target fan speed based on temperature lookup table
 * @param           pItem       - Temperature/speed lookup table
 * @param           size        - Available table size
 * @param           temp        - Current temperature
 * @param           pHys_temp   - Last temperature threshold for hysteresis
 * @return          Target fan speed
 *****************************************************************************/
static uint16_t app_fan_ctrl_line_speed(struct tbi_line* pItem, uint8_t size, uint16_t temp, uint8_t* pHys_temp)
{
	uint8_t last_idx = size - 1;
	uint8_t threshold = pItem[0].hysteresis;

	if (temp > *pHys_temp) {
		*pHys_temp = temp; 
	} else if (temp < *pHys_temp) {
		if ((*pHys_temp - temp) > threshold) {
			*pHys_temp = temp;
		} else {
			temp = *pHys_temp;
		}
	}

	if (temp < pItem[0].temp) {
		return 0;
	}
	if (temp >= pItem[last_idx].temp) {
		return pItem[last_idx].speed;
	}
	
	// Optimized binary-like search for temperature range
	for (uint8_t i = 0; i < last_idx; i++) {
		if (temp >= pItem[i].temp && temp < pItem[i + 1].temp) {
			// Cache values for linear interpolation
			uint16_t tempLow = pItem[i].temp;
			uint16_t tempHigh = pItem[i + 1].temp;
			uint16_t speedLow = pItem[i].speed;
			uint16_t speedHigh = pItem[i + 1].speed;
			
			// Linear interpolation with single division
			return speedLow + ((speedHigh - speedLow) * (temp - tempLow)) / (tempHigh - tempLow);
		}
	}
	
	// Fallback to last entry
	return pItem[last_idx].speed;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_stepwise_speed
 * Description:     Calculate target fan speed based on temperature lookup table
 * @param           pItem       - Temperature/speed lookup table
 * @param           size        - Available table size
 * @param           temp        - Current temperature
 * @param           pHys_temp   - Last temperature threshold for hysteresis
 * @return          Target fan speed
 *****************************************************************************/
static uint16_t app_fan_ctrl_stepwise_speed(struct tbi_stepwise* pItem, uint8_t size, uint16_t temp, uint8_t* pHys_temp)
{
	uint8_t last_idx = size - 1;

	// Fast path: below minimum or above maximum
	if (temp <= pItem[0].down_temp) {
		*pHys_temp = 0;
		return 0;
	}
	if (temp >= pItem[last_idx].up_temp) {
		*pHys_temp = pItem[last_idx].up_temp;
		return pItem[last_idx].speed;
	}

	// Find the current step index (fd) based on hysteresis or temperature
	int fd = -1;
	for (uint8_t i = 0; i < size; i++) {
		if (*pHys_temp == pItem[i].up_temp) {
			fd = i;
			break;
		}
	}

	if (fd == -1) {
		// Not started yet, find the correct step for the current temperature
		for (fd = 0; fd < size; fd++) {
			if (temp < pItem[fd].up_temp) {
				break;
			}
		}
		if (fd == 0) {
			*pHys_temp = 0;
			return 0;
		}
		fd--;
		*pHys_temp = pItem[fd].up_temp;
		return pItem[fd].speed;
	}

	uint16_t tempLow = pItem[fd].down_temp;
	uint16_t tempHigh = (fd < last_idx) ? pItem[fd + 1].up_temp : 0xFFFFu;

	// If temperature is within the current step, return its speed
	if (temp > tempLow && temp < tempHigh) {
		return pItem[fd].speed;
	}

	// If temperature is above the current step, move up
	if (temp >= tempHigh) {
		for (; fd < last_idx; fd++) {
			if (temp < pItem[fd + 1].up_temp) {
				break;
			}
		}
		*pHys_temp = pItem[fd].up_temp;
		return pItem[fd].speed;
	}

	// If temperature is below the current step, move down
	while (fd > 0 && temp <= pItem[fd].down_temp) {
		fd--;
	}
	*pHys_temp = pItem[fd].up_temp;
	return pItem[fd].speed;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_target_speed_calculate
 * Description:     Calculate target fan speed based on temperature lookup table
 * @param           tableItem   - Temperature/speed lookup table
 * @param           pHys_temp   - Last temperature threshold for hysteresis
 * @param           temp        - Current temperature
 * @param           type        - Fan table type
 * @return          Target fan speed
 *****************************************************************************/
static uint16_t app_fan_ctrl_target_speed_calculate(struct table_item tableItem, uint8_t* pHys_temp, uint16_t temp, bool type)
{
	uint8_t sz = 0;
	const struct tbi_common *p = tableItem.common;
	while (sz < FAN_CTRL_SPEED_TABLE_ITEM_NUM && p[sz].temp_1) {
		sz++;
	}
	if (!sz) {
		return 0;
	}

	if (type == FAN_TABLE_TYPE_LINED) {
	    return app_fan_ctrl_line_speed(&tableItem.line[0], sz, temp, pHys_temp);
	} else if (type == FAN_TABLE_TYPE_STEPWISE) {
		return app_fan_ctrl_stepwise_speed(&tableItem.stepwise[0], sz, temp, pHys_temp);
	}

	LOG_WRN("Fan speed table type %d is invalid", type);
	return 0;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_tempGet
 * Description:     Read temperature from associated sensor
 * @param           pDevContext - Fan device context
 * @param           type        - Temperature type
 * @return          Temperature value or -1 on error
 *****************************************************************************/
static inline int app_fan_ctrl_tempGet(fan_dev_ctrl_context_t *pDevContext, uint8_t type)
{
	if (!pDevContext || !pDevContext->pfReadTemp[type]) {
	    return -1;
	}

	return pDevContext->pfReadTemp[type]();
}

/*****************************************************************************
 * Function name:   app_fan_pwm_data_update
 * Description:     Update fan PWM value
 * @param           pDevContext - Fan device context
 * @param           value       - PWM value
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_pwm_data_update(fan_dev_ctrl_context_t *pDevContext, uint16_t value)
{
	if (!pDevContext) {
		return -1;
	}

	pDevContext->pwm_value = value;
	return 0;
}

/*****************************************************************************
 * Function name:   app_fan_pwm_data_get
 * Description:     Get fan PWM value
 * @param           pDevContext - Fan device context
 * @param           value       - PWM value
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_pwm_data_get(fan_dev_ctrl_context_t *pDevContext, uint16_t* value)
{
	if (!pDevContext) {
		return -1;
	}

	*value = pDevContext->pwm_value;
	return 0;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_rpmSet
 * Description:     Set fan speed based on control type
 * @param           pDevContext - Fan device context
 * @param           targetSpeed - Target speed value
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_ctrl_rpmSet(fan_dev_ctrl_context_t *pDevContext, uint16_t targetSpeed)
{
	uint16_t scale = 0;
	uint16_t speed = 0;
	if (!pDevContext || !pDevContext->pfSetSpeed) {
	    return -1;
	}

	switch (pDevContext->profile.ctrl_type) {
		case FAN_RPM_NO_CONTROL:
		case FAN_RPM_MANUAL_WITH_OFFSET_CONTROL:
		case FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL:
		case FAN_RPM_PID_DISCRETIZE_CONTROL: {
			scale = pDevContext->profile.resolution;
			speed = FAN_CONVERT_TARGETRPM_TO_TARGETCNT(targetSpeed, 0x0, pDevContext->profile.maxSpeed, scale);
		} break;

		case FAN_RPM_PID_INCREMENTAL_CONTROL:
		case FAN_RPM_RAMP_RATE_CONTROL:
			speed = targetSpeed;
			break;
		default:
			return -2;
			break;
	}

	if (speed > pDevContext->profile.resolution) {
		speed = pDevContext->profile.resolution;
		LOG_WRN("PWM out exceed resolution %d", speed);
	}
	app_fan_pwm_data_update(pDevContext, speed);
	return pDevContext->pfSetSpeed(pDevContext->header.pwmChannel, speed);
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_pwmSet
 * Description:     Set fan PWM duty cycle directly
 * @param           pDevContext - Fan device context
 * @param           targetPwm   - Target PWM percentage (0-100)
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_ctrl_pwmSet(fan_dev_ctrl_context_t *pDevContext, uint8_t targetPwm)
{
	if (!pDevContext || !pDevContext->pfSetSpeed) {
	    return -1;
	}

	uint16_t speed = (uint16_t)targetPwm * (pDevContext->profile.resolution / 100);
	if (speed > pDevContext->profile.resolution) {
		speed = pDevContext->profile.resolution;
		LOG_WRN("PWM out exceed resolution %d", speed);
	}
	app_fan_pwm_data_update(pDevContext, speed);
	return pDevContext->pfSetSpeed(pDevContext->header.pwmChannel, speed);
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_rpmGet
 * Description:     Read current fan RPM from tachometer
 * @param           pDevContext - Fan device context
 * @return          Current RPM value
 *****************************************************************************/
static inline int app_fan_ctrl_rpmGet(fan_dev_ctrl_context_t *pDevContext)
{
	int tachRpm = 0;
	uint8_t retry = 3;
	if (!pDevContext && !pDevContext->pfGetSpeed) {
		return 0;
	}

	if (pDevContext->header.type == FAN_PWM_TACH) {
		do {
			retry--;
			// k_msleep(20); // adequate to minimum 50 rpm
			tachRpm = pDevContext->pfGetSpeed(pDevContext->header.tachChannel);
		} while ((tachRpm == 0) && (retry > 0));
	} else {
		tachRpm = pDevContext->pfGetSpeed(pDevContext->header.tachChannel);
	}
	
	return tachRpm;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_powerOff
 * Description:     Power off the fan
 * @param           pDevContext - Fan device context
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_ctrl_powerOff(fan_dev_ctrl_context_t *pDevContext)
{
	if (!pDevContext || !pDevContext->pfPowerOff) {
		return -1;
	}

	return pDevContext->pfPowerOff(pDevContext->header.logicId);
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_powerOn
 * Description:     Power on the fan
 * @param           pDevContext - Fan device context
 * @return          0 on success, negative on error
 *****************************************************************************/
static inline int app_fan_ctrl_powerOn(fan_dev_ctrl_context_t *pDevContext)
{
	if (!pDevContext || !pDevContext->pfPowerOn) {
		return -1;
	}

	return pDevContext->pfPowerOn(pDevContext->header.logicId);
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_data_flush
 * Description:     Reset PID controller internal state
 * @param           pProfile - Fan profile containing PID parameters
 * @return          None
 *****************************************************************************/
static void app_fan_ctrl_data_flush(fan_profile_t *pProfile)
{
#if (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL)
	if (pProfile->ctrl_type == FAN_RPM_PID_INCREMENTAL_CONTROL || pProfile->ctrl_type == FAN_RPM_PID_DISCRETIZE_CONTROL) {
		fan_pid_curve_t *pFanPid = &pProfile->pCtrl_profile->pid_curve;
		pFanPid->target = 0;
		pFanPid->actual = 0;
		pFanPid->out = 0;
		pFanPid->pre_err = 0;
		pFanPid->ppre_err = 0;
		pFanPid->inter_err = 0;
		pFanPid->ajust_cnt = 0;
		pFanPid->started = false;
	}
#endif
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_rpmDataCalculate
 * Description:     Calculate fan control output using various control algorithms
 * @param           id        - Fan device ID
 * @param           devCtx    - Fan device context
 * @param           inputRpm  - Current RPM reading
 * @return          Calculated control output
 *****************************************************************************/
static int app_fan_ctrl_rpmDataCalculate(uint8_t id, fan_dev_ctrl_context_t *devCtx, uint16_t inputRpm)
{
	uint16_t targetRpm = devCtx->reportSpeed;
	uint8_t percentage = devCtx->overridePercentage;
	fan_profile_t *profile = &devCtx->profile;
	control_profile_t *pCtrlProfile = profile->pCtrl_profile;
	uint8_t type = profile->ctrl_type;
	uint16_t maxSpeed = profile->maxSpeed;
	uint16_t miniSpeed = profile->miniSpeed;
	int cal_speed = 0;

	if (type != FAN_RPM_RAMP_RATE_CONTROL) {
		if (!targetRpm) {
			app_fan_ctrl_data_flush(profile);
		    return targetRpm;
		}
	}

#if (FAN_CTRL_CALIBRATION_DEBUG)
	if (type & FAN_RPM_MAX_SPEED_SUPPORT_CONTROL_MSK) {
		return targetRpm;
	}
#endif

	if ((type == FAN_RPM_PID_INCREMENTAL_CONTROL) && (percentage > 0)) {
		// Percentage value
		return targetRpm;
	}

#if (FAN_CTRL_MODE == FAN_RPM_NO_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_OFFSET_CONTROL)
	if (type == FAN_RPM_NO_CONTROL) {
		cal_speed = targetRpm;
	} else if (type == FAN_RPM_MANUAL_WITH_OFFSET_CONTROL) {
		const fan_mal_ofst_t *pMalProfile = &pCtrlProfile->mal_offset;

		if ((FAN_ABS(inputRpm, targetRpm) <= pMalProfile->curve_ofst->tolerance) ||
			(targetRpm <= (pMalProfile->curve_ofst->tolerance * 2))) {
			return targetRpm;
		} else {
			uint8_t i = pMalProfile->length;
			do {
				i--;
				if (inputRpm > (targetRpm + pMalProfile->curve_ofst[i].tolerance)) {
					cal_speed =  targetRpm - pMalProfile->curve_ofst[i].offset;
				} else if (inputRpm < (targetRpm - pMalProfile->curve_ofst[i].tolerance)) {
					cal_speed = targetRpm + pMalProfile->curve_ofst[i].offset;
				}
			} while ((i > 0) && (cal_speed == 0));
		}
	}
#elif (FAN_CTRL_MODE == FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL)
    if (type == FAN_RPM_MANUAL_WITH_CURVE_MAP_CONTROL) {
		uint8_t i = 0;
		uint16_t expectLow = 0;
		uint16_t expectHigh = 0;
		uint16_t TargetLow = 0;
		uint16_t TargetHigh = 0;
		uint8_t sz = 0;
		const fan_mal_curve_t *pMapCurve = &pCtrlProfile->mal_curve;
		uint8_t scale = pMapCurve->scale;

		while ((pMapCurve->curve_map[sz].expectSpeed != 0) && (sz < pMapCurve->length)) {
			sz++;
		}

		while (((i + 1) < sz) && (expectLow == 0)) {
			if ((targetRpm >= (pMapCurve->curve_map[i].expectSpeed * scale)) &&
				(targetRpm < (pMapCurve->curve_map[i + 1].expectSpeed * scale))) {
				expectLow = pMapCurve->curve_map[i].expectSpeed * scale;
				expectHigh = pMapCurve->curve_map[i + 1].expectSpeed * scale;
				TargetLow = pMapCurve->curve_map[i].setSpeed * scale;
				TargetHigh = pMapCurve->curve_map[i + 1].setSpeed * scale;
			}
			i++;
		}

		if (expectLow == 0) {
			if (targetRpm >= (pMapCurve->curve_map[i].expectSpeed * scale) && (targetRpm <= profile->maxSpeed)) {
				cal_speed = targetRpm;
			} else if (targetRpm < (pMapCurve->curve_map[0].expectSpeed * scale)) {
				cal_speed = targetRpm;
			}
		} else {
			cal_speed = ((TargetHigh - TargetLow) * (targetRpm - expectLow)) / (expectHigh - expectLow) + TargetLow;
		}
	}
#elif (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL)
	if (type == FAN_RPM_PID_DISCRETIZE_CONTROL) {
		bool limited = false;
		int calVal = 0;
		int calOut = 0;
		int err = 0;
		int preErr = 0;
		int InterErr = 0;

		fan_pid_curve_t *pFanPid = &pCtrlProfile->pid_curve;
		pFanPid->ajust_cnt++;
		if (FAN_ABS(targetRpm, inputRpm) <= pFanPid->tolerant_err) {
#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("pid %d: target %d actual %d close, last count %d and time %dms", id, targetRpm, inputRpm, pFanPid->ajust_cnt, 
			(pFanPid->ajust_cnt * FAN_CTRL_PROC_SUB_TIMEOUT_MS));
#endif
			pFanPid->ajust_cnt = 0;
			// calVal = pFanPid->out;
		}

		pFanPid->target = targetRpm;
		pFanPid->actual = inputRpm;
		err = pFanPid->target - pFanPid->actual;
		preErr = err - pFanPid->pre_err;
		InterErr = pFanPid->inter_err + err;
		calVal = (pFanPid->kp * err) + (pFanPid->ki * InterErr) + (pFanPid->kd * preErr);
		calOut = abs(calVal) / pFanPid->k_multi;
		if (calVal < 0) {
			calOut = -calVal;
		}
		if ((calOut >= maxSpeed) && (pFanPid->ki > 0)) {
			limited = true;
		} else if ((calOut <= miniSpeed) && (pFanPid->ki > 0)) {
			limited = true;
		}

		if (limited) {
			InterErr = ((calVal - (pFanPid->kp * err) - (pFanPid->kd * preErr)) / pFanPid->ki) - err;
		}
#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("pid %d: target %d actual %d err %d preErr %d ppreErr %d calVal %d out %d", id, targetRpm, inputRpm, err, pFanPid->pre_err, 
			pFanPid->ppre_err, calOut, pFanPid->out);
#endif
		pFanPid->inter_err = InterErr;
		pFanPid->pre_err = err;
		pFanPid->out = calOut;
		cal_speed = pFanPid->out;
	}
#elif (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	if (type == FAN_RPM_PID_INCREMENTAL_CONTROL) {
		uint16_t resolution = profile->resolution;
		fan_pid_curve_t *pFanPid = &pCtrlProfile->pid_curve;
		int err, proportional, integral, derivative, cal_val, cal_out = 0;
		uint16_t shift = resolution / 100;
		pFanPid->ajust_cnt++;
		if (FAN_ABS(targetRpm, inputRpm) <= pFanPid->tolerant_err) {
#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("pid %d: target %d actual %d close, last tuning count %d and time %dms", id, targetRpm, inputRpm, pFanPid->ajust_cnt, 
			(pFanPid->ajust_cnt * FAN_CTRL_PROC_SUB_TIMEOUT_MS));
#endif
			pFanPid->ajust_cnt = 0;
			// calVal = pFanPid->out;
		}

		pFanPid->target = targetRpm;
		pFanPid->actual = inputRpm;
		err = pFanPid->target - pFanPid->actual;

		if (!pFanPid->started) {
			// a. initialize "preErr" and ''ppreErr" equal to "Err"; 
			//    (so that minimize the impact of P and D items at time zero in PID control)
			pFanPid->pre_err = err;
			pFanPid->ppre_err = err;

			// b. initilaize PWM =20%
			pFanPid->out = shift * 20;
			cal_val = 0;
			cal_out = 0;

			pFanPid->started = true;
			return pFanPid->out;
		}

		proportional = err - pFanPid->pre_err;
		integral = err;
		derivative = err + pFanPid->ppre_err - (2 * pFanPid->pre_err);
		cal_val = (pFanPid->kp * proportional) + (pFanPid->ki * integral) + (pFanPid->kd * derivative);
		cal_out = abs(cal_val) / pFanPid->k_multi;
		if (cal_val < 0) {
			cal_out = -cal_out;
		}
		pFanPid->out += cal_out;

		/* c. restrict PWM within 10% ~ 100% range */
		if (pFanPid->out < (shift * 10))  {
			pFanPid->out = shift * 10;
		} else if (pFanPid->out > (shift * 100)) {
			pFanPid->out = shift * 100;
		}

#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("pid %d: target %d actual %d err %d preErr %d ppreErr %d calVal %d calOut %d out %d", id, targetRpm, inputRpm, err, pFanPid->pre_err, 
			pFanPid->ppre_err, cal_val, cal_out, pFanPid->out);
#endif
		pFanPid->ppre_err = pFanPid->pre_err;
		pFanPid->pre_err = err;
		cal_speed = pFanPid->out;

		return cal_speed;
	}
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
	if (type == FAN_RPM_RAMP_RATE_CONTROL) {
		uint16_t pwm_old = 0;
		uint16_t delta_rpm = 0;
		struct ramp_rate *pFanRampRate = &pCtrlProfile->ramp_rate;

		app_fan_pwm_data_get(devCtx, &pwm_old);
		if (pwm_old == 0 && targetRpm != 0) {
			LOG_INF("RAMP control min pwm start %d", pFanRampRate->min_start_pwm);
			return pFanRampRate->min_start_pwm;
		}

		if (targetRpm > inputRpm) {
			delta_rpm = targetRpm - inputRpm;
			for (uint8_t i = 0;; i++) {
				if (pFanRampRate->up[i].rpm == 0) {
					cal_speed = pwm_old;
					break;
				} else if (delta_rpm >= pFanRampRate->up[i].rpm) {
					cal_speed = pwm_old + pFanRampRate->up[i].pwm;
					break;
				}
			}
		} else {
			delta_rpm = inputRpm - targetRpm;
			
			for (uint8_t i = 0;; i++) {
				if (pFanRampRate->down[i].rpm == 0) {
					cal_speed = pwm_old;
					break;
				} else if (delta_rpm >= pFanRampRate->down[i].rpm) {
					if (pwm_old > pFanRampRate->down[i].pwm) {
						cal_speed = pwm_old - pFanRampRate->down[i].pwm;
					} else {
						cal_speed = 0;
					}
					break;
				}
			}

			if (targetRpm == 0) {
				if (cal_speed < pFanRampRate->min_start_pwm) {
					// If target RPM is 0 and ramp-down completes (PWM < min_start_pwm), set PWM to 0 immediately for efficiency
					cal_speed = 0;
				}
			} else {
				// Ensure calculated speed is not below minimum start PWM
				if (cal_speed < pFanRampRate->min_start_pwm) {
					cal_speed = pFanRampRate->min_start_pwm;
				}
			}
		}

#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("ramp %d: target %d actual %d delta %d out %d", id, targetRpm, inputRpm, delta_rpm, cal_speed);
#endif
		return cal_speed;
	}
#endif

	if (cal_speed > maxSpeed) {
		cal_speed = maxSpeed;
#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("Output data %d: High than %d, correct %d", id, maxSpeed, cal_speed);
#endif
	} else if (cal_speed < miniSpeed) {
		cal_speed = miniSpeed;
#if (FAN_CTRL_PARAM_DEBUG)
		LOG_INF("Output data %d: Low than %d, correct %d", id, maxSpeed, cal_speed);
#endif
	}
	
	return cal_speed;
}

#if (FAN_CTRL_CALIBRATION_DEBUG)
/*****************************************************************************
 * Function name:   app_fan_ctrl_dump_calibration_data
 * Description:     Dump calibration data for fan control debugging
 * @param           pfan_ctrl_ctx - Fan control context
 * @return          None
 *****************************************************************************/
static void app_fan_ctrl_dump_calibration_data(fan_ctrl_context_t *pfan_ctrl_ctx)
{
	const fan_header_t *pCur_item_header = (fan_header_t *)pfan_ctrl_ctx->pManagerHeader;
	fan_dev_ctrl_context_t *pCur_dev_ctx = NULL;
	
	while (pCur_item_header != NULL) {
		if (!pCur_item_header->isInit || pCur_item_header->isDisable) {
			continue;
		}
		
		if ((pCur_item_header->type == FAN_RPM_PWM) || (pCur_item_header->type == FAN_PWM) ||
			(pCur_item_header->type == FAN_PWM_TACH)) {
			pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			
			uint8_t speed = pCur_dev_ctx->overridePercentage & FAN_CTRL_OVERRIDE_SPEED_VALID_MASK;
			if (speed < FAN_CTRL_OVERRIDE_PERCENTAGE_SCALE) {
				pCur_dev_ctx->overridePercentage = FAN_CTRL_DUMP_CALIBRATION_DATA_SCALE + speed;
				pCur_dev_ctx->overridePercentage |= FAN_CTRL_OVERRIDE_SPEED_ENABLE_MASK;
			} else {
				pCur_dev_ctx->overridePercentage = 0;
			}
			pCur_item_header = pCur_item_header->pNext;
		}
	}
}
#endif

/*****************************************************************************
 * Function name:   app_fan_ctrl_temp_calculate
 * Description:     Calculate the temperature for the fan
 * @param           pDevContext - Fan control context
 * @param           type        - Temperature type
 * @return          Temperature
 *****************************************************************************/
static uint16_t app_fan_ctrl_temp_calculate(fan_dev_ctrl_context_t *pDevContext, uint8_t type)
{
	uint16_t temp = 0;

	/* The temperature from ECRAM has highest priority */
	if (g_fan_sys_dbg_ctx.temp_w != FAN_DBG_TEMP_DIS) {
		temp = g_fan_sys_dbg_ctx.temp_w;
	} else if (pDevContext->overrideTemp > 0) {
		temp = pDevContext->overrideTemp;
	} else {
		temp = app_fan_ctrl_tempGet(pDevContext, type);
	}

	return temp;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_speed_calculate
 * Description:     Calculate the target speed for the fan
 * @param           pDevContext - Fan control context
 * @param           inputTemp   - Input temperature
 * @param           type        - Temperature type
 * @return          Target speed
 *****************************************************************************/
static uint32_t app_fan_ctrl_speed_calculate(fan_dev_ctrl_context_t *pDevContext, uint16_t inputTemp, uint8_t type)
{
	uint32_t target_speed = 0;

	/* The target speed from ECRAM has highest priority */
	if (pDevContext->overrideTargetSpeed > 0) {
		target_speed = pDevContext->overrideTargetSpeed;
	} else if (pDevContext->overridePercentage > 0) {
		if (pDevContext->profile.ctrl_type & FAN_RPM_MAX_SPEED_SUPPORT_CONTROL_MSK) {
			target_speed = ((pDevContext->overridePercentage & FAN_CTRL_OVERRIDE_SPEED_VALID_MASK) *
				pDevContext->profile.maxSpeed) / FAN_CTRL_OVERRIDE_PERCENTAGE_SCALE;
		} else {
			target_speed = 0;
		}
	} else {
		uint8_t sel = pDevContext->profile.tb_select;
		fan_speed_table_t *pSpeedTable =  pDevContext->profile.pTable;
		struct profile_table *pProfileTable = &pSpeedTable->profile[sel][type];
		target_speed = app_fan_ctrl_target_speed_calculate(pProfileTable->tb_item, &pProfileTable->spike_temp, inputTemp, pProfileTable->tb_type);
	}

	return target_speed;
}

/*****************************************************************************
 * Function name:   app_fan_data_update
 * Description:     Update temperature readings and calculate target speeds for all fans
 * @param           pfan_ctrl_ctx - Fan control context
 * @return          None
 *****************************************************************************/
static void app_fan_data_update(fan_ctrl_context_t *pfan_ctrl_ctx)
{
	const fan_header_t *pCur_item_header = (fan_header_t *)pfan_ctrl_ctx->pManagerHeader;

	if (!pfan_ctrl_ctx) {
		return;
	}

#if (FAN_CTRL_CALIBRATION_DEBUG)
	 app_fan_ctrl_dump_calibration_data(pfan_ctrl_ctx);
#endif

	 while (pCur_item_header != NULL) {
		if ((pCur_item_header->isInit) && (!pCur_item_header->isDisable)) {
			fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			uint16_t inputTemp = 0;
			uint32_t targetSpeed = 0;

			/* Find the highest target speed from all temperature types */
			uint8_t active_tp = 0;
			for (uint8_t i = 0; i < FAN_TEMP_TYPE_NUMBER; i++) {
				uint16_t temp = app_fan_ctrl_temp_calculate(pCur_dev_ctx, i);
				uint32_t speed = app_fan_ctrl_speed_calculate(pCur_dev_ctx, temp, i);
				if (speed >= targetSpeed) {
					targetSpeed = speed;
					inputTemp = temp;
					active_tp = i;
				}
				LOG_DBG("num %d: temp %d speed %d", i, temp, speed);
			}

			g_fan_sys_dbg_ctx.temp_r = inputTemp;
			pCur_dev_ctx->reportInputTemp = inputTemp;
			pCur_dev_ctx->reportSpeed = targetSpeed;

			if (pCur_dev_ctx->overridePercentage & FAN_CTRL_OVERRIDE_SPEED_ENABLE_MASK) {
				LOG_INF("Calibration Fan %d: Precentage: %d inputTemp: %d targetRpm %d", pCur_item_header->logicId, 
					(pCur_dev_ctx->overridePercentage & FAN_CTRL_OVERRIDE_SPEED_VALID_MASK), inputTemp, targetSpeed);
			} else {
	            LOG_INF("Fan %d: acitve %d inputTemp %d targetRpm %d", pCur_item_header->logicId, active_tp, inputTemp, targetSpeed);
			}
		} else {
			LOG_INF("Fan %d is disabled", pCur_item_header->logicId);
		}
		
		pCur_item_header = pCur_item_header->pNext;
	}
}

/*****************************************************************************
 * Function name:   app_fan_debug_sys_report_speed
 * Description:     Report fan device(s) RPM speed
 * @param           id    - fan device logic id
 * @param           speed - current speed
 * @return          None
 *****************************************************************************/
static void app_fan_debug_sys_report_speed(uint8_t id, uint16_t speed)
{
	if (id == 1u) {
		g_fan_sys_dbg_ctx.rpm_r1 = speed;
	} else if (id == 2u) {
		g_fan_sys_dbg_ctx.rpm_r2 = speed;
	} else if (id == 3u) {
		g_fan_sys_dbg_ctx.rpm_r3 = speed;
	} else if (id == 4u) {
		g_fan_sys_dbg_ctx.rpm_r4 = speed;
	} else if (id == 5u) {
		g_fan_sys_dbg_ctx.rpm_r5 = speed;
	} else if (id == 6u) {
		g_fan_sys_dbg_ctx.rpm_r6 = speed;
	}
}

/*****************************************************************************
 * Function name:   app_fan_sys_power_control
 * Description:     Control fan power on/off
 * @param           pfan_ctrl_ctx - Fan control context
 * @param           on_off        - On/off flag
 * @return          None
 *****************************************************************************/
static void app_fan_sys_power_control(fan_ctrl_context_t *pfan_ctrl_ctx, bool on_off)
{
	fan_dev_ctrl_context_t *pCur_dev_ctx = NULL;
	const fan_header_t *pCur_item_header = (fan_header_t *)pfan_ctrl_ctx->pManagerHeader;

	if (!pfan_ctrl_ctx) {
		return;
	}

	while (pCur_item_header != NULL) {
		if ((pCur_item_header->isInit) && (!pCur_item_header->isDisable)) {
			pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			if (on_off) {
				app_fan_ctrl_powerOn(pCur_dev_ctx);
			} else {
				app_fan_ctrl_data_flush(&pCur_dev_ctx->profile);
				app_fan_ctrl_pwmSet(pCur_dev_ctx, 0);
				app_fan_ctrl_powerOff(pCur_dev_ctx);
			}
		}
		pCur_item_header = pCur_item_header->pNext;
	}
}

/*****************************************************************************
 * Function name:   app_fan_sys_control
 * Description:     Execute fan control algorithms and apply PWM outputs
 * @param           pfan_ctrl_ctx - Fan control context
 * @return          None
 *****************************************************************************/
static void app_fan_sys_control(fan_ctrl_context_t *pfan_ctrl_ctx)
{
	uint16_t readSpeed = 0;
	int cal_speed = 0;
	fan_dev_ctrl_context_t *pCur_dev_ctx = NULL;
	const fan_header_t *pCur_item_header = (fan_header_t *)pfan_ctrl_ctx->pManagerHeader;
	
	if (!pfan_ctrl_ctx) {
		return;
	}
	
	while (pCur_item_header != NULL) {
		if ((pCur_item_header->isInit) && (!pCur_item_header->isDisable))
		{
			pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			readSpeed = app_fan_ctrl_rpmGet(pCur_dev_ctx);

			/* ECRAM has highest priority */
			if (g_fan_sys_dbg_ctx.pwm_w != FAN_DBG_PWM_DIS) {
				app_fan_ctrl_pwmSet(pCur_dev_ctx, g_fan_sys_dbg_ctx.pwm_w);
				pCur_dev_ctx->reporCalculatedSpeed = 0xFFFFu; // A fake value for resuming case
			} else if (pCur_dev_ctx->overridePercentage > 0) {
				app_fan_ctrl_pwmSet(pCur_dev_ctx, pCur_dev_ctx->overridePercentage > 100 ? 100 : pCur_dev_ctx->overridePercentage);
				pCur_dev_ctx->reporCalculatedSpeed = 0xFFFFu; // A fake value for resuming case
			} else {
				cal_speed = app_fan_ctrl_rpmDataCalculate(pCur_item_header->logicId, pCur_dev_ctx, readSpeed);

				if (pCur_dev_ctx->reporCalculatedSpeed != cal_speed) {
					app_fan_ctrl_rpmSet(pCur_dev_ctx, cal_speed);
					pCur_dev_ctx->reporCalculatedSpeed = cal_speed;
				}
			}

			pCur_dev_ctx->reportRealRpm = readSpeed;
			app_fan_debug_sys_report_speed(pCur_item_header->logicId, readSpeed);

			LOG_INF("Fan %d: TargetRpm %d RealRpm %d calSpeed %d", pCur_item_header->logicId,  pCur_dev_ctx->reportSpeed, readSpeed, cal_speed);
		}
		
		pCur_item_header = pCur_item_header->pNext;
	}
}

/*****************************************************************************
 * Function name:   app_fan_debug_speed_table_control
 * Description:     Set or get a fan speed table type
 * @param           mode - fan speed table select
 * @param           type - 0: read, 1: write
 * @return          None
 *****************************************************************************/
static void app_fan_debug_speed_table_control(uint8_t *mode, uint8_t type)
{
	fan_header_t *pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
	 
	while (pCur_item_header != NULL) {
		fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;

		if (type) {
			if (*mode < FAN_SPEED_TABLE_MODE_NUM) {
				pCur_dev_ctx->profile.tb_select = *mode;
			} else {
				LOG_WRN("Set fan table ctrl mode %d failed", *mode);
			}
		} else {
			*mode = pCur_dev_ctx->profile.tb_select; // no need to check all devices
			return;
		}

		pCur_item_header = pCur_item_header->pNext;
	}
}

/*****************************************************************************
 * Function name:   app_fan_debug_sys_acpiHandler
 * Description:     ACPI interface for fan system debug and PID tuning
 * @param           isRead   - Read/write operation flag
 * @param           ui8Idx   - Data index
 * @param           pui8Data - Data pointer
 * @return          0 on success
 *****************************************************************************/
static uint8_t app_fan_debug_sys_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{	
	uint8_t ui8Index = ui8Idx;
	uint8_t *pData = (uint8_t*)&g_fan_sys_dbg_ctx;

	if (ui8Index >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET) {
		return 0;
	}

	if (ui8Index >= sizeof(struct fan_dbg_sys_context)) {
		return 0;
	}

	if (isRead) {
#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
		if (g_fan_sys_dbg_ctx.pid_sel != FAN_DBG_PID_DIS && g_fan_sys_dbg_ctx.pid_sel <= BOARD_FAN_DEVICE_NUM) {
			fan_header_t *pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
			while (pCur_item_header != NULL) {
				fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
				if (pCur_dev_ctx->profile.ctrl_type != FAN_RPM_PID_DISCRETIZE_CONTROL && 
						pCur_dev_ctx->profile.ctrl_type != FAN_RPM_PID_INCREMENTAL_CONTROL) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				if (pCur_dev_ctx->header.logicId == g_fan_sys_dbg_ctx.pid_sel) {
					if (g_fan_sys_dbg_ctx.pid_flag == FAN_DBG_PID_FLAG_DUMP) {
						struct fan_dbg_sys_pid *pid = &g_fan_sys_dbg_ctx.pid;
						pid->rpm_wd = pCur_dev_ctx->overrideTargetSpeed;

						fan_pid_curve_t *pid_curve = &pCur_dev_ctx->profile.pCtrl_profile->pid_curve;
						pid->multi = pid_curve->k_multi;
						pid->kp = pid_curve->kp;
						pid->ki = pid_curve->ki;
						pid->kd = pid_curve->kd;
						pid->tolerance = pid_curve->tolerant_err;
						pid->ajust = pid_curve->ajust_cnt;
						pid->rpm_rd = pCur_dev_ctx->reportRealRpm;
					}
				}
				pCur_item_header = pCur_item_header->pNext;
			}
		}
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
		if (g_fan_sys_dbg_ctx.ramp_sel != FAN_DBG_RAMP_DIS && g_fan_sys_dbg_ctx.ramp_sel <= BOARD_FAN_DEVICE_NUM) {
			fan_header_t *pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
			while (pCur_item_header != NULL) {
				fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
				if (pCur_dev_ctx->profile.ctrl_type != FAN_RPM_RAMP_RATE_CONTROL) {
					pCur_item_header = pCur_item_header->pNext;
					continue;
				}
			
				if (pCur_dev_ctx->header.logicId == g_fan_sys_dbg_ctx.ramp_sel) {
					if (g_fan_sys_dbg_ctx.ramp_flag == FAN_DBG_RAMP_FLAG_DUMP) {
						struct fan_dbg_sys_ramp *ramp = &g_fan_sys_dbg_ctx.ramp;
						ramp->rpm_wd = pCur_dev_ctx->overrideTargetSpeed;
						ramp->rpm_rd = pCur_dev_ctx->reportRealRpm;
					}
				}
				pCur_item_header = pCur_item_header->pNext;
			}
		}
#endif

		app_fan_debug_speed_table_control(&g_fan_sys_dbg_ctx.table_rw, 0);
		*pui8Data = *(pData + ui8Index);
		return 0;
	}

#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	if (ui8Index < (sizeof(struct fan_dbg_sys_context) - sizeof(struct fan_dbg_sys_pid))) {
		*(pData + ui8Index) = *pui8Data;
	}
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
	if (ui8Index < (sizeof(struct fan_dbg_sys_context) - sizeof(struct fan_dbg_sys_ramp))) {
		*(pData + ui8Index) = *pui8Data;
	}
#endif

	if (g_fan_sys_dbg_ctx.pwm_w == 0xFF) {
		g_fan_sys_dbg_ctx.pwm_w = FAN_DBG_PWM_DIS;
	} else {
		if (g_fan_sys_dbg_ctx.pwm_w > 100) {
			g_fan_sys_dbg_ctx.pwm_w = 100;
		}
	}

	if (g_fan_sys_dbg_ctx.temp_w == 0xFF) {
		g_fan_sys_dbg_ctx.temp_w = FAN_DBG_TEMP_DIS;
	}

	if (g_fan_sys_dbg_ctx.table_rw < FAN_SPEED_TABLE_MODE_NUM) {
		app_fan_debug_speed_table_control(&g_fan_sys_dbg_ctx.table_rw, 1);
	}

#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	if (g_fan_sys_dbg_ctx.pid_flag == FAN_DBG_PID_FLAG_DUMP) {
		return 0;
	}
	*(pData + ui8Index) = *pui8Data;

	if (g_fan_sys_dbg_ctx.pid_sel != FAN_DBG_PID_DIS && g_fan_sys_dbg_ctx.pid_sel <= BOARD_FAN_DEVICE_NUM) {
		fan_header_t *pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
		while (pCur_item_header != NULL) {
			fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			if (pCur_dev_ctx->header.logicId == g_fan_sys_dbg_ctx.pid_sel) {
				if (pCur_dev_ctx->profile.ctrl_type != FAN_RPM_PID_DISCRETIZE_CONTROL &&
						pCur_dev_ctx->profile.ctrl_type != FAN_RPM_PID_INCREMENTAL_CONTROL) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				if (g_fan_sys_dbg_ctx.pid_flag != FAN_DBG_PID_FLAG_SET_DONE) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				struct fan_dbg_sys_pid *pid = &g_fan_sys_dbg_ctx.pid;
				fan_pid_curve_t *pid_curve = &pCur_dev_ctx->profile.pCtrl_profile->pid_curve;
				pCur_dev_ctx->overrideTargetSpeed = pid->rpm_wd;
				pid_curve->k_multi = pid->multi;
				pid_curve->kp = pid->kp;
				pid_curve->ki = pid->ki;
				pid_curve->kd = pid->kd;
				pid_curve->tolerant_err = pid->tolerance;
			}
			pCur_item_header = pCur_item_header->pNext;
		}
	}
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
	if (g_fan_sys_dbg_ctx.ramp_flag == FAN_DBG_RAMP_FLAG_DUMP) {
		return 0;
	}
	*(pData + ui8Index) = *pui8Data;

	if (g_fan_sys_dbg_ctx.ramp_sel != FAN_DBG_RAMP_DIS && g_fan_sys_dbg_ctx.ramp_sel <= BOARD_FAN_DEVICE_NUM) {
		fan_header_t *pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
		while (pCur_item_header != NULL) {
			fan_dev_ctrl_context_t *pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;
			if (pCur_dev_ctx->header.logicId == g_fan_sys_dbg_ctx.ramp_sel) {
				if (pCur_dev_ctx->profile.ctrl_type != FAN_RPM_RAMP_RATE_CONTROL) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				if (g_fan_sys_dbg_ctx.ramp_flag != FAN_DBG_RAMP_FLAG_SET_DONE) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				struct fan_dbg_sys_ramp *ramp = &g_fan_sys_dbg_ctx.ramp;
				pCur_dev_ctx->overrideTargetSpeed = ramp->rpm_wd;
			}
			pCur_item_header = pCur_item_header->pNext;
		}
	}
#endif

	return 0;
}

/*****************************************************************************
 * Function name:   app_fan_debug_speed_table_acpiHandler
 * Description:     ACPI interface for fan speed table debugging
 * @param           isRead   - Read/write operation flag
 * @param           ui8Idx   - Data index
 * @param           pui8Data - Data pointer
 * @return          0 on success
 *****************************************************************************/
static uint8_t app_fan_debug_speed_table_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{	
	uint8_t ui8Index = ui8Idx;
	fan_header_t *pCur_item_header = NULL;
	fan_dev_ctrl_context_t *pCur_dev_ctx = NULL;
	struct tbi_common *pTable = NULL;
	uint8_t *pData = (uint8_t*)&g_fan_tb_dbg_ctx;

	if (ui8Index >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET) {
		return 0;
	}

	if (ui8Index >= sizeof(struct fan_dbg_table_context)) {
		return 0;
	}

	if (isRead) {
		if (g_fan_tb_dbg_ctx.dev_sel != FAN_DBG_TABLE_DIS && g_fan_tb_dbg_ctx.dev_sel <= BOARD_FAN_DEVICE_NUM) {
			pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
			while (pCur_item_header != NULL) {
				pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;

				if (pCur_dev_ctx->header.logicId == g_fan_tb_dbg_ctx.dev_sel) {
					if (g_fan_tb_dbg_ctx.table_flag == FAN_DBG_TABLE_FLAG_DUMP) {
						uint8_t i = FAN_CTRL_SPEED_TABLE_ITEM_NUM;
						uint8_t speed_mode = g_fan_tb_dbg_ctx.table.active_mode;
						uint8_t temp_type = g_fan_tb_dbg_ctx.table.active_temp;

						while (i-- > 0) {
							pTable = &pCur_dev_ctx->profile.pTable->profile[speed_mode][temp_type].tb_item.common[i];
							struct fan_dbg_table_item *item = &g_fan_tb_dbg_ctx.table.item[i];

							item->temp_1 = pTable->temp_1;
							item->temp_2 = pTable->temp_2;
							item->speed = pTable->speed;
						}
					}
				}
				pCur_item_header = pCur_item_header->pNext;
			}
		}

		*pui8Data = *(pData + ui8Index);
		return 0;
	}

	if (ui8Index < (sizeof(struct fan_dbg_table_context) - sizeof(struct fan_dbg_table))) {
		*(pData + ui8Index) = *pui8Data;
	}

	if (g_fan_tb_dbg_ctx.table_flag == FAN_DBG_TABLE_FLAG_DUMP) {
		return 0;
	}
	*(pData + ui8Index) = *pui8Data;

	if (g_fan_tb_dbg_ctx.dev_sel != FAN_DBG_TABLE_DIS && g_fan_tb_dbg_ctx.dev_sel <= BOARD_FAN_DEVICE_NUM) {
		pCur_item_header = (fan_header_t *)board_fan_ctrl_mngrHeaderGet();
		while (pCur_item_header != NULL) {
			pCur_dev_ctx = (fan_dev_ctrl_context_t *)pCur_item_header;

			if (pCur_dev_ctx->header.logicId == g_fan_tb_dbg_ctx.dev_sel) {
				uint8_t i = FAN_CTRL_SPEED_TABLE_ITEM_NUM;
				uint8_t speed_mode = g_fan_tb_dbg_ctx.table.active_mode;
				uint8_t temp_type = g_fan_tb_dbg_ctx.table.active_temp;
			
				if (g_fan_tb_dbg_ctx.table_flag != FAN_DBG_PID_FLAG_SET_DONE) {
					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				if (speed_mode >= FAN_SPEED_TABLE_MODE_NUM) {

					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				if (temp_type >= FAN_TEMP_TYPE_NUMBER) {
					pCur_item_header = pCur_item_header->pNext;
					continue;
				}

				while (i-- > 0) {
					pTable = &pCur_dev_ctx->profile.pTable->profile[speed_mode][temp_type].tb_item.common[i];
					struct fan_dbg_table_item *item = &g_fan_tb_dbg_ctx.table.item[i];

					pTable->temp_1 = item->temp_1;
					pTable->temp_2 = item->temp_2;
					pTable->speed = item->speed;
				}
			}
			pCur_item_header = pCur_item_header->pNext;
		}
	}

	return 0;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_speed_table_select
 * Description:     Set or get a fan speed table type
 * @param           value - fan speed table select
 * @param           code  - 0: read, 1: write
 * @return          None
 *****************************************************************************/
void app_fan_ctrl_sys_speed_table_select(uint8_t *value, uint8_t code)
{
	app_fan_debug_speed_table_control(value, code);
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_disable
 * Description:     Disable fan control system
 * @param           None
 * @return          None
 *****************************************************************************/
void app_fan_ctrl_sys_disable(void)
{
	/* Need to power off fan pwm output before EC_SLP_S5_N and EC_SLP_S3_S0A3_N pin release to avoid power current leakage. */
	fan_ctrl_context_t *p_fan_ctx = board_fan_ctrl_ctxGet();
	app_fan_sys_power_control(p_fan_ctx, false);
	g_fan_ctrl_enabled = false;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_enable
 * Description:     Enable fan control system
 * @param           None
 * @return          None
 *****************************************************************************/
void app_fan_ctrl_sys_enable(void)
{
	g_fan_ctrl_enabled = true;
}

/*****************************************************************************
 * Function name:   app_fan_ctrl_sys_enabled
 * Description:     Check if fan control system is enabled
 * @param           None
 * @return          True if enabled, false otherwise
 *****************************************************************************/
bool app_fan_ctrl_sys_enabled(void)
{
	return g_fan_ctrl_enabled;
}

/*****************************************************************************
 * Function name:   app_fan_dbg_int
 * Description:     Initialize fan debug interface and register ACPI handlers
 * @param           None
 * @return          None
 *****************************************************************************/
static void app_fan_dbg_int(void)
{
	memset(&g_fan_sys_dbg_ctx, 0xFF, sizeof(struct fan_dbg_sys_context));
	g_fan_sys_dbg_ctx.pwm_w = FAN_DBG_PWM_DIS;
	g_fan_sys_dbg_ctx.temp_w = FAN_DBG_TEMP_DIS;
#if (FAN_CTRL_MODE == FAN_RPM_PID_DISCRETIZE_CONTROL) || (FAN_CTRL_MODE == FAN_RPM_PID_INCREMENTAL_CONTROL)
	g_fan_sys_dbg_ctx.pid_flag = FAN_DBG_PID_FLAG_DUMP;
	g_fan_sys_dbg_ctx.pid_sel = FAN_DBG_PID_DIS;
#elif (FAN_CTRL_MODE == FAN_RPM_RAMP_RATE_CONTROL)
	g_fan_sys_dbg_ctx.ramp_flag = FAN_DBG_RAMP_FLAG_DUMP;
	g_fan_sys_dbg_ctx.ramp_sel = FAN_DBG_RAMP_DIS;
#endif
	g_fan_tb_dbg_ctx.table_flag = FAN_DBG_TABLE_FLAG_DUMP;
	g_fan_tb_dbg_ctx.dev_sel = FAN_DBG_TABLE_DIS;

	md_acpiplanes_register_fun(app_fan_debug_sys_acpiHandler, 0xDC);
	md_acpiplanes_register_fun(app_fan_debug_speed_table_acpiHandler, 0xDD);
}

/*****************************************************************************
 * Function name:   fanctrl_thread
 * Description:     Main fan control thread - manages system fan operation
 * @param           p1, p2, p3 - Standard Zephyr thread parameters (unused)
 * @return          None (infinite loop)
 *****************************************************************************/
void fanctrl_thread(void *p1, void *p2, void *p3)
{
	uint32_t updated_cnt = 0;
	uint32_t updated_thd = 0;
	bool on_off_flag = false;

	task_fanctrlSlpReady = (uint8_t*) p2;
#if (FAN_CTRL_CALIBRATION_DEBUG)
	updated_thd = FAN_CTRL_CALIBRACTION_TIMEOUT_MS / FAN_CTRL_PROC_SUB_TIMEOUT_MS;
	LOG_INF("Fan calibraction start period time %dms", FAN_CTRL_CALIBRACTION_TIMEOUT_MS);
#else
	updated_thd = FAN_CTRL_PROC_TIMEOUT_MS / FAN_CTRL_PROC_SUB_TIMEOUT_MS;
#endif
	board_fan_ctrl_init();
	app_fan_dbg_int();
	
	fan_ctrl_context_t *p_fan_ctx = board_fan_ctrl_ctxGet();
	while(1) {
		k_msleep(FAN_CTRL_PROC_SUB_TIMEOUT_MS);
		task_status_set(task_fanctrlSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	
		if ((app_pseq_getPwrStatus() == SYSTEM_S0_STATE) && app_fan_ctrl_sys_enabled()) {
			if (!on_off_flag) {
				app_fan_sys_power_control(p_fan_ctx, true);
				on_off_flag = true;
			}

			updated_cnt++;
			if (updated_cnt >= updated_thd) {
				updated_cnt = 0;
				app_fan_data_update(p_fan_ctx);
			}
			app_fan_sys_control(p_fan_ctx);
		} else {
			if (on_off_flag) {
				app_fan_sys_power_control(p_fan_ctx, false);
				on_off_flag = false;
			}
		}
		task_status_set(task_fanctrlSlpReady, TASK_STASTUS_SLEEP_READY, 1);
	}
}