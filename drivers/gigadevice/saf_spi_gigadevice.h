/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#ifndef __SAF_SPI_GIGADEVICE_H__
#define __SAF_SPI_GIGADEVICE_H__

#include "spi_gigadevice_opcodes.h"


#define FAST_READ_IO_OPCODE            FAST_READ_QUAD_IO_OPCODE


/**
 * @brief Return SAF configuration structure for Winbond SPI devices.
 *
 * @return espi saf configuration. Refer to eSPI SAF driver header.
 */
const struct espi_saf_cfg *gigadevice_saf_cfg(void);

/**
 * @brief Return SPI transaction details for Winbond SPI devices.
 *
 * @param the desired logical command.
 *
 * @return spi_transaction item. Refer to spi_transaction.h.
 */
struct saf_spi_transaction *gigadevice_qspi_cmd(enum saf_command_index command);

#endif /* __SAF_SPI_GIGADEVICE_H__ */

