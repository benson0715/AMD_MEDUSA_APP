/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __PSL_H__
#define __PSL_H__
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
int psl_configure(void);
void psl_out_inactive(void);
uint8_t psl_get_input( void );
#endif /* __PSL_H__*/