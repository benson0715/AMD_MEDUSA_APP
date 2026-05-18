/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * No-op implementations of the Nuvoton-named eSPI helper hooks for
 * RTS5918 builds. See realtek_espi.h for the rationale.
 */

#include "realtek_espi.h"

int espi_xec_set_udc_port(const struct device *dev, espi_udc_status status)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(status);
	return 0;
}

int espi_xec_mimic_tx_buffer_empty(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}
