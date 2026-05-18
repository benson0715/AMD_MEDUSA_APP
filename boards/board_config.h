/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "board_ioexp.h"
#if defined(CONFIG_BOARD_NPCX4MNX_MDSPLUM)
#include "mdsplum_npcx4mnx.h"
#endif
#if defined(CONFIG_BOARD_NPCX4MNX_MDSPLUM_ECMODULE)
#include "mdsplum_npcx4mnx.h"
#endif
#if defined(CONFIG_BOARD_NPCX4MNX_MDSDORNE)
#include "mdsdorne_npcx4mnx.h"
#endif
#if defined(CONFIG_BOARD_NPCX4MNX_MDSAERIS)
#include "mdsaeris_npcx4mnx.h"
#endif
#if defined(CONFIG_BOARD_RTS5918_MDSPLUM)
#include "mdsplum_rts5918.h"
#endif
#include "board_init.h"
#include "app_pseq.h"

/**
 * @brief This global variable helps to configure variable gpios.
 */
extern uint8_t boot_mode_maf;

/* define the PD update hook */
#define CHECK_FOR_FORCE_PD_UPDATE board_init_ifNeedForcePdFwUpdate
#define INFORM_USER_FOR_PD_UPDATE board_init_informUserForPdUpdate

/**
 * @brief Perform platform configuration depending on the board.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_init(void);

/**
 * @brief Perform platform configuration during suspend depending on the board.
 *
 * Note: Allows to optimize power consumption while the system is in S3/S4/S5.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_suspend(void);

/**
 * @brief Perform platform configuration during resume depending on the board.
 *
 * Note: Allows to restore pin functionality when the system exits S3/S4/S5.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_resume(void);

/**
 * @brief re-configure the pnis status during APU Power OK event.
 *
 * Note: called in the interrupt handler
 *
 * @retval null
 */
void board_onApuPwrOK(void);

/**
 * @brief re-configure the pnis status when system status change from G3 to Sx or APU_RST assert
 *
 * Note: called in the power sequence management
 *
 * @retval null
 */
void board_evalCardCtrlPins(void);

/**
 * @brief turn off the jtag interface in case of current leakage
 *
 * @param void
 * @retval void
 */
void board_turnOffJtagInterface (void);

/**
 * @brief if need to force upgarde PD FW
 *
 * @param void
 * @retval void
 */
bool board_ifNeedForcePdFwUpdate(void);


/**
 * @brief turn on the eSPO
 *
 * @param void
 * @retval void
 */
void board_turnOn_espi(void);
#endif /* __BOARD_CONFIG_H__ */
