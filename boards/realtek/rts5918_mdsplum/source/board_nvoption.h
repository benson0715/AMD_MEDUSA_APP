/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVRAM-backed option flags that AMD's Plum stores in BBRAM.
 * Re-validate offsets/layout once the RTS5918 BBRAM partition map is
 * decided (BBRAM size is the same 256 B as NPCX4).
 */

#ifndef __BOARD_NVOPTION_H__
#define __BOARD_NVOPTION_H__

#include <stdint.h>
#include <stdbool.h>

/*
 * NV_OPTIONS_DECLARATION expands inside the EC_NV_OPTIONS union/struct
 * in misc/f_nv_options.h. Each entry is a single-bit (or small-width)
 * field that app code reads through GET_NV_OPTIONS / SET_NV_OPTIONS.
 *
 * The list below mirrors the NPCX4 Plum surface so app/acpi, app/saf,
 * app/power_management etc. resolve. Add new fields here when more
 * AMD subsystems are re-enabled.
 *
 * TODO(realtek-schematic): re-confirm bit allocation against AMD's
 * loader once the RTS5918 boot loader format is decided. The actual
 * NV storage backing is currently a static buffer in
 * board_sys_config.c, not real BBRAM/flash.
 */
#define NV_OPTIONS_DECLARATION                                          \
	DEF_NV_OPTIONS(f_s0i3KbcWake,        1);                        \
	DEF_NV_OPTIONS(espi_ResetInS0i3,     1);                        \
	DEF_NV_OPTIONS(f_TurnOnPostCode,     1);                        \
	DEF_NV_OPTIONS(f_wirelessManagability, 1);                      \
	DEF_NV_OPTIONS(chg_AcDcSwitch,       1);                        \
	DEF_NV_OPTIONS(chg_AcOnlyProchot,    1);                        \
	DEF_NV_OPTIONS(chg_AcTime,           8);                        \
	DEF_NV_OPTIONS(chg_DcTime,           8);                        \
	DEF_NV_OPTIONS(chg_DcOnlyProchot,    1);                        \
	DEF_NV_OPTIONS(chg_DcProchotLevel,   8);                        \
	DEF_NV_OPTIONS(chg_FakeDcLevel,      8);                        \
	DEF_NV_OPTIONS(chg_ProchotAcDebounce, 8);                       \
	DEF_NV_OPTIONS(chg_ProchotDcDebounce, 8);                       \
	DEF_NV_OPTIONS(chg_ProchotDuration,  8);                        \
	DEF_NV_OPTIONS(chg_UseNvdcMode,      1);                        \
	DEF_NV_OPTIONS(pwr_SSD0D3Cold,       1);                        \
	DEF_NV_OPTIONS(pwr_SSD1D3Cold,       1);                        \
	DEF_NV_OPTIONS(pwr_keepWlanPwrInS3,  1);                        \
	DEF_NV_OPTIONS(pwr_keepWlanPwrInS4,  1);                        \
	DEF_NV_OPTIONS(spi_ba_Type,          4);                        \
	DEF_NV_OPTIONS(thermtrip_force_g3,   1);                        \
	DEF_NV_OPTIONS(thm_p3tLimitEn_AC,    1);                        \
	DEF_NV_OPTIONS(thm_p3tLimitEn_DC,    1);                        \
	DEF_NV_OPTIONS(thm_uploadEvalCardTemp, 1);                      \
	DEF_NV_OPTIONS(thm_uploadOnboardTemp, 1);                       \
	DEF_NV_OPTIONS(cpu_open_type,        4)

/* SET_NV_OPTIONS_TO_DEFAULT is called once by misc/f_nv_options.c when
 * the NV magic check fails on cold boot. NPCX4 Plum populates every
 * bitfield to its expected default; for the RTS5918 scaffold the
 * default-zero-initialisation is acceptable until the bring-up
 * persistence story is decided.
 */
#define SET_NV_OPTIONS_TO_DEFAULT  do { (void)0; } while (0)

bool board_nvopt_get(uint16_t id, uint32_t *value);
bool board_nvopt_set(uint16_t id, uint32_t value);

#endif /* __BOARD_NVOPTION_H__ */
