/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_INIT_H__
#define __BOARD_INIT_H__

#include <stdint.h>
#include <stdbool.h>

extern uint16_t g_sysAcLimit1;

#define LED_OFF				(0)
#define LED_ON				(1)
#define LED_BLINK_ERR 		(2)
#define LED_BLINK_PD_UPDATE (3)
#define LED_BLINK_S0		(5)
#define LED_BLINK_G3_       (10)
#define LED_BLINK_S5		(5)
#define LED_BLINK_G3_S3S4	(20)

/**
 * board_status_led_set
 *
 * @param [in]   val: 0: always turn off
 *                    1: always turn on
 *                    2 ~ 255: blinking speed
 *
 * @note
 *  board status indicated by the blinked LED
 */
void board_status_led_set(uint8_t val);
/* led statue unlock. */
void board_status_led_lock(void);
/* led statue unlock. */
void board_status_led_unlock(void);
/* board platform init */
void board_init_platformInit(void);
/* apu reset handler */
void board_init_apuResetHandler (void);
/* apu sleep enter handler */
void board_init_apuSleepEnterHandler (void);
/* spu sleep exit handler */
void board_init_apuSleepExitHandler (void);
/* if need to force upgrade PD FW */
bool board_init_ifNeedForcePdFwUpdate(void);
/* inform user if PD FW is upgrading */
void board_init_informUserForPdUpdate(void);
/* trigger PMIC tunnel service */
void board_init_pmicTunnelTrigger(void);
/* enable the apu reset timer */
void board_init_apuRstTimerEnable (uint32_t ms);
/* handle the kbc reset */
void board_init_kbcRst(void);
/* smart mux setting V0 */
void board_init_smartMux_V0(void);
/* smart mux setting V1 */
void board_init_smartMux_V1(void);
/* smart mux setting V2 */
void board_init_smartMux_V2(void);
/* smart mux setting V3 */
void board_init_smartMux_V3(void);
/* trigger EC information report */
void board_init_updateEcInfoEventTrigger(void);

#endif // end of __BOARD_INIT_H__
