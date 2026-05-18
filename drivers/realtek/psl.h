/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSL (Power Switch Logic) hibernate shim for RTS5918. NPCX4 exposes
 * a dedicated PSL block; RTS5918 reaches the same outcome through
 * BBRAM wake + SLW timer. For framework compile pass these are
 * intentionally no-ops; hibernate flows must be re-implemented before
 * S5/G3 features are enabled.
 *
 * TODO(realtek-schematic): once the GPIO that latches the PSL output
 * on Plum is known we can wire psl_out_inactive() to drive it directly
 * via gpio_write_pin().
 */

#ifndef __REALTEK_PSL_H__
#define __REALTEK_PSL_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>

int psl_configure(void);
void psl_out_inactive(void);
uint8_t psl_get_input(void);

#endif /* __REALTEK_PSL_H__ */
