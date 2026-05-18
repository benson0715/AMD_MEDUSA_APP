/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __MCHP_ESPI_H__
#define __MCHP_ESPI_H__

#include <zephyr/device.h>

typedef enum {
	ESPI_UDC_UDCPRESENT = 0,
	ESPI_UDC_UARTDONGLE_PRESENT = 1,
	ESPI_UDC_NONE = 2,
} espi_udc_status;

/**
 * @brief Set udc status, so espi peripheral can be set correctly.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param status UDC status.
 * 
 * @retval True if successful.
 */
int espi_xec_set_udc_port(const struct device *dev, espi_udc_status status);

/**
 * @brief Mimic tx buffer to keep host sending tx data.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * 
 * @retval True if successful.
 */
int espi_xec_mimic_tx_buffer_empty(const struct device *dev);
#endif /* __ESPI_HUB_H__ */
