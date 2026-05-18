/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "board_ioexp.h"
#include "board_id.h"
#include "dev_utility.h"
#include "f_nv_options.h"
#include "dev_pca9535.h"
#include "board_config.h"
#include "board_smtbty.h"

/**************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_S5AlwEnFlag = 0;

/**
 * @brief Init the IOEXP when it is powered on. (power rail in ALW_3V3)
 *        Need to call in the power sequence after S5 power rail ready
 */
void board_ioexp_initIoExp(void){}

/**
 * @brief ACPI handler for IO Expander 0
 */
void board_ioexp_AcpiHandler_IOExp0 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 1
 */
void board_ioexp_AcpiHandler_IOExp1 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 2
 */
void board_ioexp_AcpiHandler_IOExp2 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 3
 */
void board_ioexp_AcpiHandler_IOExp3 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 4
 */
void board_ioexp_AcpiHandler_IOExp4 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 5
 */
void board_ioexp_AcpiHandler_IOExp5 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 6
 */
void board_ioexp_AcpiHandler_IOExp6 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 7
 */
void board_ioexp_AcpiHandler_IOExp7 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 8
 */
void board_ioexp_AcpiHandler_IOExp8 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 9
 */
void board_ioexp_AcpiHandler_IOExp9 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 10
 */
void board_ioexp_AcpiHandler_IOExp10 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 11
 */
void board_ioexp_AcpiHandler_IOExp11 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 12
 */
void board_ioexp_AcpiHandler_IOExp12 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI handler for IO Expander 13
 */
void board_ioexp_AcpiHandler_IOExp13 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}