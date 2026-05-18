/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************/


#ifndef __FWST_H__
#define __FWST_H__

#include <zephyr/kernel.h>
#include "amd_crb_drivers_pd.h"

/* PDFW version for two chips */
extern PD_VR_CTRL_STATUS g_pdCtrlSt[2];

void fwSt_init( void );

#endif // end of __FWST_H__
