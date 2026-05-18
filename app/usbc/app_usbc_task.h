/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_USBC_TASK_H_
#define APP_USBC_TASK_H_

#include "system.h"

#define APP_USBC_RETIMER_PS8828      (1)
#define APP_USBC_RETIMER_PS8830      (2)
#define APP_USBC_RETIMER_KB8002      (3)
#define APP_USBC_RETIMER_KB8001      (4)
#define APP_USBC_RETIMER_KB8010      (5)
#define APP_USBC_RETIMER_PS8833      (6)
#define APP_USBC_RETIMER_PS8835      (7)

typedef union {
	struct {
		uint8_t silicon        : 2;    // Silicon: 00 (not initiate), 01 (TV), 10 (A0 Silicon), 11 (B0 SIlicon)
		uint8_t retimer        : 3;    // Retimer: 000 (not initiate), 001 (PS8828A), 010 (PS8830), 011 (KB8002), 100 (KB8001)
		uint8_t resv      	   : 3;    // Resv
	} field;
	uint8_t ui8Reg;
} APP_USBC_STATUS_REG;

extern APP_USBC_STATUS_REG app_usbc_status;

/* write the USBC device id */
void app_usbc_writeDevID(uint8_t id);
/* read the USBC device id */
uint8_t app_usbc_readDevID(void);
/* retimer0 acpi handler */
uint8_t app_usbc_retimer0_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* retimer1 acpi handler */
uint8_t app_usbc_retimer1_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* crossbar mail box acpi handler */
uint8_t app_usbc_CrossbarMailbox_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* read the re-timer type from PD register */
void app_usbc_getRetimerType(void);
/* signal the usbcSem */
void app_usbc_giveSem(void);
/* return the highest sink RDO from all three typec ports */
uint32_t app_usbc_getAcLimitOfHighestPdPort(void);
/* app usbc task init */
int app_usbc_task_init(void);
/**
 * @brief Routine that handles usbc tasks.
 *
 * This routines also handles usbc and power delivery related tasks
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void app_usbc_thread(void *p1, void *p2, void *p3);

#endif // end of APP_USBC_TASK_H_







