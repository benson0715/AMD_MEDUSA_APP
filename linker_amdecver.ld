/**
*Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
*/

MEMORY
    {
    /* AMD Added support EC version check */
    #if CONFIG_SOC_SERIES_MEC172X 
    AMDECVER  (rx) : ORIGIN = 0xCFF40,    LENGTH = 256
    #else
    AMDECVER  (rx) : ORIGIN = 0x10070000,    LENGTH = 256
    #endif
    }

#include <zephyr/arch/arm/aarch32/cortex_m/scripts/linker.ld>
