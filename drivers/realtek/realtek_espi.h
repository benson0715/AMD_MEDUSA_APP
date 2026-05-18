/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Realtek RTS5918 eSPI vendor shim. Mirrors drivers/nuvoton/nuvoton_espi.h
 * but exposes the same espi_udc_status enum and helper names so
 * drivers/espi_hub.h compiles unchanged when CONFIG_SOC_SERIES_RTS5918
 * is selected.
 *
 * The two helper functions used by the eSPI hub (set_udc_port,
 * mimic_tx_buffer_empty) ship as no-op stubs that return 0. They were
 * originally Microchip XEC quirks routed through the Nuvoton wrapper;
 * RTS5918 silicon does not need either, so leaving them as no-ops is
 * the correct semantic. Revisit if a Plum-specific UDC dongle handshake
 * surfaces during bring-up.
 */

#ifndef __REALTEK_ESPI_H__
#define __REALTEK_ESPI_H__

#include <zephyr/device.h>

typedef enum {
	ESPI_UDC_UDCPRESENT = 0,
	ESPI_UDC_UARTDONGLE_PRESENT = 1,
	ESPI_UDC_NONE = 2,
} espi_udc_status;

int espi_xec_set_udc_port(const struct device *dev, espi_udc_status status);
int espi_xec_mimic_tx_buffer_empty(const struct device *dev);

/*
 * AMD-custom eSPI virtual-wire signals. Upstream Zephyr 3.7.1's
 * `enum espi_vwire_signal` does not include the AMD-specific signals
 * for CPU thermal-trip read and RTC read. NPCX4 Plum gets these via
 * the AMD kernel patch (0001-AMD-kernel-V3_2.patch line 78 of espi.h).
 * For RTS5918 we define them as integer aliases above
 * ESPI_VWIRE_SIGNAL_COUNT so the espi_send_vwire() call compiles. The
 * RTS5918 eSPI driver will silently drop signals outside its switch
 * table — that matches AMD's intent on platforms without the signals
 * wired.
 *
 * TODO(realtek-schematic): once the AMD VW number assignment is in
 * lenovo_p's zephyr eSPI driver (mirror the AMD kernel patch), replace
 * these with the real enum values.
 */
#ifndef ESPI_VWIRE_SIGNAL_CPU_TEMP_READ
#define ESPI_VWIRE_SIGNAL_CPU_TEMP_READ  ((enum espi_vwire_signal)0x80)
#endif
#ifndef ESPI_VWIRE_SIGNAL_RTC_READ
#define ESPI_VWIRE_SIGNAL_RTC_READ       ((enum espi_vwire_signal)0x81)
#endif

#endif /* __REALTEK_ESPI_H__ */
