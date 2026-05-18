/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __APP_SLEEP_H__
#define __APP_SLEEP_H__
#include <zephyr/kernel.h>
/* when all task sleep, cpu enters idle thread */
void idle_thread(void *p1, void *p2, void *p3);
/* cpu enters sleep for power saving */
void sleep_PwrSaver();
#endif // end of __APP_SLEEP_H__
