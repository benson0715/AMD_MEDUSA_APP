/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Build-time generated version strings. AMD's buildVersion.sh produces
 * this file under boards/nuvoton/<board>/source/; here we provide a
 * static placeholder so misc/f_nv_options.h's `#include "ec_version.h"`
 * resolves. The real bring-up flow should regenerate this at build
 * time from git describe / Plum SKU.
 */

#ifndef __EC_VERSION_H__
#define __EC_VERSION_H__

#define EC_VERSION_PROJECT "Plum-RTS5918"
#define EC_VERSION_MAJOR   0
#define EC_VERSION_MINOR   1
#define EC_VERSION_PATCH   0
#define EC_VERSION_BUILD   "scaffold"

/* Loader-verified NVRAM magic. NPCX4 Plum uses a SHA1SUM-derived 32-bit
 * constant; for the RTS5918 scaffold we use a recognisable placeholder
 * until the bring-up loader is wired up. The exact value is irrelevant
 * for compile but must stay stable per-board once NV storage is live.
 */
#define EC_VERSION_U32_MAGIC  0x52545453ul  /* 'RTTS' = Realtek TODO */

extern const char ec_version_string[];

#endif /* __EC_VERSION_H__ */
