/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "nuvoton_espi.h"
#include <soc.h>
#include <zephyr/kernel.h>
extern uint8_t shm_win3_mmap[CONFIG_ESPI_NPCX_PERIPHERAL_SHAW3_SIZE];
extern void npcx_host_init_shaw3(void);
extern void npcx_host_disable_shaw3(void);
extern void npcx_host_init_sp_base(void);
extern void npcx_host_disable_sp(void);

/**
 * @brief Set UDC port configuration based on eSPI UDC status
 *
 * @param dev Pointer to the device structure
 * @param status UDC status to configure
 * @return int true on success
 */
int espi_xec_set_udc_port(const struct device *dev, espi_udc_status status)
{

	switch(status) {
	case ESPI_UDC_UDCPRESENT:
		npcx_host_disable_sp();
		npcx_host_init_shaw3();
		break;
	case ESPI_UDC_UARTDONGLE_PRESENT:
		npcx_host_disable_shaw3();
		npcx_host_init_sp_base();
		break;
	case ESPI_UDC_NONE:
		npcx_host_disable_shaw3();
		npcx_host_disable_sp();
		break;
	default:
		break;
	}
		return true;
}

/**
 * @brief Mimic UART TX buffer empty status in shared memory window
 *
 * @param dev Pointer to the device structure
 * @return int true on success
 */
int espi_xec_mimic_tx_buffer_empty(const struct device *dev)
{
	/* IOx3F8 ~ IOx3FB */
	shm_win3_mmap[0] = 0x06;
	shm_win3_mmap[1] = 0x03;
	shm_win3_mmap[2] = 0xC2;
	shm_win3_mmap[3] = 0x03;

	/* IOx3FD */
	shm_win3_mmap[5] = 0x60;

	/* Leave IOx3FE and IOx3FF as 0 */
	shm_win3_mmap[6] = 0x0;
	shm_win3_mmap[7] = 0x0;

	/* IOx3FC */
	shm_win3_mmap[4] = 0x20;
	
	return true;


}