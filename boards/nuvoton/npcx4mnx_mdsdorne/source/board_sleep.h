/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_SLEEP_H__
#define __BOARD_SLEEP_H__

#include <zephyr/kernel.h>
//#include <mchp_sleep.h>
#define APP_WAIT_BEFORE_EC_LOW_PWR  90
typedef enum
{
    SLP_SYSTEM_LIGHT_SLEEP = 0,
    SLP_SYSTEM_HEAVY_SLEEP = 1,
} SLEEP_MODE;
#define _GIRQ_ENCODE(x)     (0xA0 | ((x) - 8))
#define _isGIRQ_IDX(x)      (0xA0 == (0xA0 & (x)))
#define _GIRQ_IDX(x)        (0x1F & (x))
#define KSI_GIRQ_INIT_LIST                                                       \
/*     GIRQ group     ,    bit ,    NET NAME,            Interrupt handler    */ \
    { _GIRQ_ENCODE(11),    15 }, /* SMSC_KSI_0           (none)               */ \
    { _GIRQ_ENCODE(11),    16 }, /* SMSC_KSI_1           (none)               */ \
    { _GIRQ_ENCODE(11),    17 }, /* SMSC_KSI_2           (none)               */ \
    { _GIRQ_ENCODE(11),    22 }, /* SMSC_KSI_3           (none)               */ \
    { _GIRQ_ENCODE(11),    23 }, /* SMSC_KSI_4           (none)               */ \
    { _GIRQ_ENCODE(11),    24 }, /* SMSC_KSI_5           (none)               */ \
    { _GIRQ_ENCODE(11),    25 }, /* SMSC_KSI_6           (none)               */ \
    { _GIRQ_ENCODE(11),    26 }, /* SMSC_KSI_7           (none)               */ \
    {0,                     0 }
/* shut down the EC blocks and prepare to enter sleep mode */
void board_slp_Sleep(uint8_t mode, uint16_t duration);
/* resume the EC blocks and prepare to wake up from sleep mode */
bool board_slp_Wakeup(void);
/* disable the KBC wakeup */
void board_slp_disableKSxWakeup(void);
/* enable the KBC wakeup */
void board_slp_enableKSxWakeup(void);
/* KBC wakeup KSI monitor debounce */
void board_slp_wake_up_from_kb(void);
void npcx_power_enter_system_sleep2(int slp_mode, int wk_mode);
#endif // end of __BOARD_SLEEP_H__

