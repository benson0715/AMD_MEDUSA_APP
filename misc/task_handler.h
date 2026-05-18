/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
 
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#define EC_TASK_PRIORITY_1	K_PRIO_COOP(1)
#define EC_TASK_PRIORITY_2	K_PRIO_COOP(2)
#define EC_TASK_PRIORITY_3	K_PRIO_COOP(3)
#define EC_TASK_PRIORITY_4	K_PRIO_COOP(4)
#define EC_TASK_PRIORITY_5	K_PRIO_COOP(5)
#define EC_TASK_PRIORITY_6	K_PRIO_COOP(6)
#define EC_TASK_PRIORITY_7	K_PRIO_COOP(7)
#define EC_TASK_PRIORITY_8	K_PRIO_COOP(8)

#define THRML_MGMT_TASK_NAME    "THRMLMGMT"
#define USBC_TASK_NAME          "USBC"
#define PSRC_TASK_NAME          "PWRSRC"

#define TASK_STASTUS_BUSY		0
#define TASK_STASTUS_BUSY_SHORT		BIT(1)
#define TASK_STASTUS_SLEEP_READY	BIT(0)

/**
 * @brief Set names for all tasks in the app.
 *
 */
void start_all_tasks(void);

/**
 * @brief Suspends all tasks currently running in the app.
 *
 */
void suspend_all_tasks(void);

/**
 * @brief Resumes all tasks currently suspend in the app.
 *
 */
void resume_all_tasks(void);

/**
 * @brief wake specified task from sleep in the app.
 *
 */
void wake_task(const char *tagname);

/**
 * @brief suspend specified task  in the app.
 *
 */

void suspend_tasks(const char *tagname);
/**
 * @brief resume specified task in the app.
 *
 */
void resume_tasks(const char *tagname);

bool all_tasks_sleep_ready(void);

void task_status_set(void *p, uint32_t value, uint32_t clr_set);

#if CONFIG_UDC_MANAGEMENT
/**
 * @brief start udctask in the app.
 *
 */
void start_udctask(void);
/**
 * @brief wake udctask in the app.
 *
 */
void wake_udctask(void);
#endif /* CONFIG_UDC_MANAGEMENT */

#endif /* __TASK_HANDLER_H__ */
