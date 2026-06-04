/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_PERIPH_H__
#define __BOARD_PERIPH_H__



/* inputBuffer Initial positions */
#define PWR_BTN_INIT_POS		           (1)
#define AC_PRSENT_INIT_POS                 (1)
#define UART_DONGLE_PRSENT_INIT_POS        (1)
#define UART0_INTn_INIT_POS                (1)
#define SMIn_INTn_INIT_POS                 (1)
#define BAT_IN_INIT_POS                    (1)
#define CHG_OK_INIT_POS                    (0)
#define DOCK_IN_INIT_POS                   (1)
#define LID_INIT_POS                       (1)
#define DISP_SW_INIT_POS                   (0)
#define DBG_CARD_INIT_POS                  (1)
#define AC_330W_PRSNT_INIT_POS             (0)
#define CHG_PROCHOTn1_INIT_POS             (1)
#define CHG_PROCHOTn2_INIT_POS             (1)
#define DC_S5_ALW_INIT_POS                 (0)
#define PD_SRC_ON_INIT_POS                 (1)
#define CHG_PROCHOT_INIT_POS               (1)
#define APU_RST_INIT_POS                   (1)
#define APU_PWROK_INIT_POS                 (0)
#define APU_THERMALTRIP_INIT_POS           (0)
#define APU_ALERT_INIT_POS                 (1)
#define SLP_S3_INIT_POS                    (1)
#define SLP_S5_INIT_POS                    (1)
#define USB1_INT_INIT_POS                  (1)
#define USB2_INT_INIT_POS                  (1)
#define IOEXP0_INIT_POS                    (1)
#define ALW_GD_INIT_POS                    (0)
#define MEM_PMIC_PWR_GOOD_INIT_POS         (0)

int board_periph_init(void);
void board_periph_IoExp_IntDispatcher(void);

#endif /* __BOARD_PERIPH_H__ */
