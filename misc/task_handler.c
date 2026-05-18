/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/arch/cpu.h>
/* Zephyr 3.7.1 reorganised the cmsis header layout. The legacy
 * <zephyr/arch/arm/aarch32/cortex_m/cmsis.h> moved to <cmsis_core.h>
 * (provided by the cmsis module). Keep the old include for the
 * NPCX4/3.2 tree and switch to the new path everywhere else.
 */
#if defined(CONFIG_SOC_SERIES_NPCX4)
#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#else
#include <cmsis_core.h>
#endif
#include <zephyr/logging/log.h>
#include "app_pseq.h"
#include "espioob_mngr.h"
#include "smchost.h"
#include "periphmgmt.h"
#include "kbchost.h"
#include "task_handler.h"
#include "board_config.h"
#if CONFIG_BATTERY_MANAGEMENT
#include "app_smtbty.h"
#endif
#include "app_udc.h"
#if CONFIG_APP_THERMAL
#include "app_thermal.h"
#endif
#if CONFIG_ALS
#include "app_opt4048_thread.h"
#endif
#if CONFIG_USBC_POWER_DELIVERY
#include "app_usbc_task.h"
#endif
#if CONFIG_POWER_SOURCE_MANAGEMENT
#include "board_pwrSrc.h"
#endif
#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif
#include "app_sleep.h"
#include "app_pseq.h"
LOG_MODULE_DECLARE(pwrmgmt, CONFIG_PWRMGT_LOG_LEVEL);

#define EC_TASK_STACK_SIZE	1024

/* K_FOREVER can no longer be used with K_THREAD_DEFINE
 * Zephyr community is using user-defined macros instead to achieve
 * the same behavior than with K_FOREVER.
 * For EC define macro until the new flag K_THREAD_NO_START is available.
 */
#define EC_WAIT_FOREVER (-1)

const uint32_t periph_thrd_period = 1;
const uint32_t app_pseq_thrd_period = 1;
const uint32_t smchost_thrd_period = 10;

uint8_t periph_thrd_sleep_ready = 0;
uint8_t app_pseq_thrd_sleep_ready = 0;
uint8_t thermal_thrd_sleep_ready = 0;
uint8_t opt4048_thrd_sleep_ready = 0;
uint8_t usbc_thrd_sleep_ready = 0;
uint8_t udc_thrd_sleep_ready = 0;
uint8_t psrc_thrd_sleep_ready = 0;
uint8_t smtbty_thrd_sleep_ready = 0;
uint8_t fanctrl_thrd_sleep_ready = 0;
uint8_t smchost_thrd_sleep_ready = 0;

int OSIdleCtr =0;
void cpu_usage_thread(void *p1, void *p2, void *p3);

#if defined(CONFIG_ESPI_PERIPHERAL_8042_KBC) && \
	(defined(CONFIG_PS2_KEYBOARD) || defined(CONFIG_PS2_MOUSE) || \
	defined(CONFIG_KSCAN_EC))

#define KBC_TASK_STACK_SIZE	1024
#define KB_TASK_STACK_SIZE	1024

K_THREAD_DEFINE(kbc_thrd_id, KBC_TASK_STACK_SIZE, to_from_host_thread,
		NULL, NULL, NULL, EC_TASK_PRIORITY_1, 0, EC_WAIT_FOREVER);
K_THREAD_DEFINE(kb_thrd_id, KB_TASK_STACK_SIZE, to_host_kb_thread,
		NULL, NULL, NULL, EC_TASK_PRIORITY_1, 0, EC_WAIT_FOREVER);
#endif

#if CONFIG_PERIPHERAL_MANAGEMENT
K_THREAD_DEFINE(periph_thrd_id, EC_TASK_STACK_SIZE, periph_thread,
		&periph_thrd_period, &periph_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

K_THREAD_DEFINE(app_pseq_thrd_id, EC_TASK_STACK_SIZE, app_pseq_thread,
		&app_pseq_thrd_period, &app_pseq_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);

#if CONFIG_SOC_SERIES_NPCX4
#define OOBMNGR_TASK_STACK_SIZE		2048U
K_THREAD_DEFINE(oobmngr_thrd_id, OOBMNGR_TASK_STACK_SIZE, oobmngr_thread,
		NULL, NULL, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

K_THREAD_DEFINE(smchost_thrd_id, EC_TASK_STACK_SIZE, smchost_thread,
		&smchost_thrd_period, &smchost_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_1,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);

#if CONFIG_APP_THERMAL
const uint32_t thermal_thrd_period = 800;
K_THREAD_DEFINE(thermal_thrd_id, EC_TASK_STACK_SIZE, thermalmgmt_thread,
		&thermal_thrd_period, &thermal_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_ALS
const uint32_t opt4048_thrd_period = 1000;
K_THREAD_DEFINE(opt4048_thrd_id, EC_TASK_STACK_SIZE, opt4048_thread,
		&opt4048_thrd_period, &opt4048_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_USBC_POWER_DELIVERY
#define USBC_TASK_STACK_SIZE	2048
const uint32_t usbc_thrd_period = 200;
K_THREAD_DEFINE(usbc_thrd_id, USBC_TASK_STACK_SIZE, app_usbc_thread,
		&usbc_thrd_period, &usbc_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_BATTERY_MANAGEMENT
const uint32_t battery_thrd_period = 500;
K_THREAD_DEFINE(battery_thrd_id, EC_TASK_STACK_SIZE, batterymgmt_thread,
		&battery_thrd_period, &smtbty_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_UDC_MANAGEMENT
const uint32_t udc_thrd_period = 125;
K_THREAD_DEFINE(udc_thrd_id, EC_TASK_STACK_SIZE, udc_thread,
		&udc_thrd_period, &udc_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_8,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_POWER_SOURCE_MANAGEMENT
const uint32_t psrc_thrd_period = 10;
K_THREAD_DEFINE(psrc_thrd_id, EC_TASK_STACK_SIZE, board_pwrSrc_thread,
		&psrc_thrd_period, &psrc_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
K_THREAD_DEFINE(psrc_delay_thrd_id, EC_TASK_STACK_SIZE, board_pwrSrc_thread_delay_callback,
		&psrc_thrd_period,NULL, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_FAN_RPM_CONTROL
const uint32_t fanctrl_thrd_period = 0;
K_THREAD_DEFINE(fanctrl_thrd_id, EC_TASK_STACK_SIZE, fanctrl_thread,
		&fanctrl_thrd_period, &fanctrl_thrd_sleep_ready, NULL, EC_TASK_PRIORITY_5,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_CPU_USEAGE_RATE
const uint32_t cpu_usage_period = 1000;
K_THREAD_DEFINE(cpu_usage_thrd_id, EC_TASK_STACK_SIZE, cpu_usage_thread,
		&cpu_usage_period, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO-1,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

#if CONFIG_SLEEP
const uint32_t idle_thrd_period = 0;
K_THREAD_DEFINE(idle_thrd_id, EC_TASK_STACK_SIZE, idle_thread,
		&idle_thrd_period, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO,
		K_INHERIT_PERMS, EC_WAIT_FOREVER);
#endif

struct task_info {
	k_tid_t thread_id;
	bool can_suspend;
	bool started;
	const char *tagname;
	uint8_t *sleep_ready;
};

static struct task_info tasks[] = {

#if defined(CONFIG_ESPI_PERIPHERAL_8042_KBC) && \
	(defined(CONFIG_PS2_KEYBOARD) || defined(CONFIG_PS2_MOUSE) || \
	defined(CONFIG_KSCAN_EC))

	{ .thread_id = kbc_thrd_id, .can_suspend = false,
	  .tagname = "KBC", .sleep_ready = NULL },

	{ .thread_id = kb_thrd_id, .can_suspend = false,
	  .tagname = "KB", .sleep_ready = NULL },
#endif

#if CONFIG_PERIPHERAL_MANAGEMENT
	{ .thread_id = periph_thrd_id, .can_suspend = false,
	  .tagname = "PERIPH", .sleep_ready = &periph_thrd_sleep_ready  },
#endif

	{ .thread_id = app_pseq_thrd_id, .can_suspend = false,
	  .tagname = "PWR", .sleep_ready = &app_pseq_thrd_sleep_ready  },

#if CONFIG_SOC_SERIES_NPCX4
	{ .thread_id = oobmngr_thrd_id, .can_suspend = false,
	  .tagname = "OOB", .sleep_ready = NULL  },
#endif

	{ .thread_id = smchost_thrd_id, .can_suspend = false,
	  .tagname = "SMC", .sleep_ready = &smchost_thrd_sleep_ready  },

#if CONFIG_APP_THERMAL
	{ .thread_id = thermal_thrd_id, .can_suspend = false,
	  .tagname = THRML_MGMT_TASK_NAME, .sleep_ready = &thermal_thrd_sleep_ready  },
#endif
#if CONFIG_ALS
	{ .thread_id = opt4048_thrd_id, .can_suspend = false,
	  .tagname = "OPT4048", .sleep_ready = &opt4048_thrd_sleep_ready  },
#endif
#if CONFIG_BATTERY_MANAGEMENT
	{ .thread_id = battery_thrd_id, .can_suspend = false,
	  .tagname = "BATTMGMT", .sleep_ready = &smtbty_thrd_sleep_ready  },
#endif
#if CONFIG_USBC_POWER_DELIVERY
	{ .thread_id = usbc_thrd_id, .can_suspend = false,
	  .tagname = USBC_TASK_NAME, .sleep_ready =&usbc_thrd_sleep_ready },
#endif
#if CONFIG_POWER_SOURCE_MANAGEMENT
	{ .thread_id = psrc_thrd_id, .can_suspend = false,
	  .tagname = PSRC_TASK_NAME, .sleep_ready = &psrc_thrd_sleep_ready  },
	{ .thread_id = psrc_delay_thrd_id, .can_suspend = false,
	  .tagname = "PWRSRC_DELAY", .sleep_ready = NULL  },
#endif
#if CONFIG_FAN_RPM_CONTROL
	{ .thread_id = fanctrl_thrd_id, .can_suspend = false,
	  .tagname = "FANCTRL", .sleep_ready = &fanctrl_thrd_sleep_ready  },
#endif

#if CONFIG_CPU_USEAGE_RATE
	{ .thread_id = cpu_usage_thrd_id, .can_suspend = false,
	  .tagname = "CPU_RATE", .sleep_ready = NULL  },
#endif

#if CONFIG_SLEEP
	{ .thread_id = idle_thrd_id, .can_suspend = false,
	  .tagname = "IDLE_TASK", .sleep_ready = NULL  },
#endif
#if CONFIG_UDC_MANAGEMENT
	{ .thread_id = udc_thrd_id, .can_suspend = false,
	  .tagname = "UDCMGMT", .sleep_ready = NULL  },
#endif
};

/**
 * @brief Start all registered tasks
 *
 * Iterates through all tasks in the task array and starts each thread.
 * Also sets thread names if CONFIG_THREAD_NAME is enabled.
 */
void start_all_tasks(void)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (tasks[i].thread_id) {
#if CONFIG_THREAD_NAME
			k_thread_name_set(tasks[i].thread_id, tasks[i].tagname);
#endif
			k_thread_start(tasks[i].thread_id);
		}
	}
}

/**
 * @brief Suspend all suspendable tasks
 *
 * Iterates through all tasks and suspends those marked as can_suspend.
 */
void suspend_all_tasks(void)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (tasks[i].can_suspend) {
			k_thread_suspend(tasks[i].thread_id);
			LOG_DBG("%p suspended", tasks[i].thread_id);
		}
	}
}

/**
 * @brief Resume all suspended tasks
 *
 * Iterates through all tasks and resumes those marked as can_suspend.
 */
void resume_all_tasks(void)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (tasks[i].can_suspend) {
			k_thread_resume(tasks[i].thread_id);
			LOG_DBG("%p resumed", tasks[i].thread_id);
		}
	}
}

/**
 * @brief Wake a specific task by tagname
 *
 * @param tagname The tag name of the task to wake
 */
void wake_task(const char *tagname)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (strcmp(tasks[i].tagname, tagname) == 0) {
			k_wakeup(tasks[i].thread_id);
			break;
		}
	}
}

/**
 * @brief Suspend a specific task by tagname
 *
 * @param tagname The tag name of the task to suspend
 */
void suspend_tasks(const char *tagname)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (strcmp(tasks[i].tagname, tagname) == 0) {
			k_thread_suspend(tasks[i].thread_id);
			break;
		}
	}
}

/**
 * @brief Resume a specific task by tagname
 *
 * @param tagname The tag name of the task to resume
 */
void resume_tasks(const char *tagname)
{
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (strcmp(tasks[i].tagname, tagname) == 0) {
			k_thread_resume(tasks[i].thread_id);
			break;
		}
	}
}

/**
 * @brief Check if all tasks are ready for sleep
 *
 * @return true if all tasks are sleep ready, false otherwise
 */
bool all_tasks_sleep_ready(void)
{
	bool ret = true;
	for (int i = 0; i < ARRAY_SIZE(tasks); i++) {
		if (tasks[i].sleep_ready) {
			if (*(uint8_t*)(tasks[i].sleep_ready) & TASK_STASTUS_BUSY_SHORT) {// sleep get update
				*(uint8_t*)(tasks[i].sleep_ready) &= ~TASK_STASTUS_BUSY_SHORT;// this bit is enabled by each tasks and clear it here
				return false;
			} 
			if((*(uint8_t*)(tasks[i].sleep_ready)  & TASK_STASTUS_SLEEP_READY) == false){
				LOG_DBG("%s not ready=%d", tasks[i].tagname, *(uint8_t *)(tasks[i].sleep_ready));
				return false;// sleep is not ready
			}
		} 
	}
	return ret;
}

/**
 * @brief Set or clear task status flags
 *
 * @param p Pointer to the status variable
 * @param value The value to set or clear
 * @param clr_set If non-zero, set the bits; otherwise clear them
 */
void task_status_set(void *p, uint32_t value, uint32_t clr_set)
{
	if (p != NULL) {
		if (clr_set){
			*(uint32_t*)p |= value;
		} else {
			*(uint32_t*)p &= ~value;
		}
	}
}

/**
 * @brief CPU usage monitoring thread
 *
 * Periodically reports the idle counter value for CPU usage analysis.
 *
 * @param p1 Pointer to the thread period in milliseconds
 * @param p2 Unused parameter
 * @param p3 Unused parameter
 */
void cpu_usage_thread(void *p1, void *p2, void *p3)
{
	uint32_t period = *(uint32_t *)p1;
	uint32_t run =0;
	unsigned int key;
	while(1) {
		k_msleep(period);
		key = irq_lock();
		run = OSIdleCtr;
		OSIdleCtr = 0;
		printk("idle count = %d\r\n\r\n",run);
		irq_unlock(key);
	}
}