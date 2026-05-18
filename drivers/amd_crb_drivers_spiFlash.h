/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_SPI_FLASH_H_
#define AMD_CRB_DRIVERS_SPI_FLASH_H_

#include "system.h"

/**
 * Definitions of the commands for Winbond SPI Flash as defalut data.
 */
#define	SPIF_COMMAND_WRITE_ENABLE		          0x06 /* Write Enable command */
#define	SPIF_COMMAND_VOLATILE_SR_WRITE_ENABLE     0x50 /* Voltaile SR Write Enable command */
#define	SPIF_COMMAND_WRITE_DISABLE		          0x04 /* Write Disable command */
#define SPIF_COMMAND_READ_STATUS_REG1             0x05 /* Read Status Register-1 command */
#define SPIF_COMMAND_WRITE_STATUS_REG1            0x01 /* Write Status Register-1 command */
#define SPIF_COMMAND_READ_STATUS_REG2             0x70 /* Read Status Register-2 command */
#define SPIF_COMMAND_WRITE_STATUS_REG2            0x31 /* Write Status Register-2 command */
#define SPIF_COMMAND_READ_STATUS_REG3             0x15 /* Read Status Register-3 command */
#define SPIF_COMMAND_WRITE_STATUS_REG3            0x11 /* Write Status Register-3 command */
#define SPIF_COMMAND_CHIP_ERASE                   0xC7 /* Chip erase C7h/60h command */
#define SPIF_COMMAND_ERASE_PROG_SUSPEND           0x75 /* Erase / Program Suspend command */
#define SPIF_COMMAND_ERASE_PROG_RESUME            0x7A /* Erase / Program Suspend command */
#define SPIF_COMMAND_POWER_DOWN                   0xB9 /* Power down command */
#define SPIF_COMMAND_RELEASE_POWER_DOWN_DEVID     0xAB /* Release Power-down / ID command */
#define SPIF_COMMAND_DEVICE_ID                    0x90 /* Manufacturer/Device ID command */
#define SPIF_COMMAND_JEDEC_ID                     0x9F /* JEDEC ID command */
#define SPIF_COMMAND_GLOBAL_BK_LOCK               0x7E /* Global Block Lock command */
#define SPIF_COMMAND_GLOBAL_BK_UNLOCK             0x98 /* Global Block Unlock command */
#define SPIF_COMMAND_ENTER_QPI_MODE               0x38 /* Enter QPI Mode command */
#define SPIF_COMMAND_ENABLE_RESET                 0x66 /* Enable Reset command */
#define SPIF_COMMAND_RESET_DEVICE                 0x99 /* Reset Device command */
#define SPIF_COMMAND_READ_UNIQUE_ID               0x4B /* Read Unique ID command */
#define SPIF_COMMAND_BLOCK_ERASE_32KB		      0x52 /* Block Erase (32KB) command */
#define SPIF_COMMAND_READ_DATA  		          0x03 /* Read Data command */
#define SPIF_COMMAND_FAST_READ       		      0x0B /* Fast Read command */
#define SPIF_COMMAND_DUAL_READ		              0x3B /* Dual Output Fast Read command */
#define SPIF_COMMAND_QUAD_READ		              0x6B /* Quad Output Fast Read */
#define SPIF_COMMAND_READ_SFDP_REG	              0x5A /* Read SFDP Register */
#define SPIF_COMMAND_DEVICE_ID_DUAL               0x92 /* Manufacturer/Device ID in dual mode command */
#define SPIF_COMMAND_DEVICE_ID_QUAD               0x94 /* Manufacturer/Device ID in quad mode command */
#define SPIF_COMMAND_ENABLE_RST_OPCODE            0x66 /* enable the reset opcode */
#define SPIF_COMMAND_RESET_OPCODE                 0x99 /* reset the opcode */
#define SPIF_COMMAND_CONTINUOUS_MODE_OPCODE       0xA5 /* enable the continuous mode opcode */
#define SPIF_COMMAND_4_BYTE_ADDR_ENTER_OPCODE     0xB7 /* enable 4 bytes address enter opcode */
#define SPIF_COMMAND_EXIT_QPI_OPCODE              0xFF /* exit the QPI */
#define SPIF_COMMAND_READ_VOLATILE_CFG_OPCODE     0x85 /* read the volatile cfg */
#define SPIF_COMMAND_CONTINUOUS_MODE_OPCODE_GD    0xB1 /* enable the continuous mode opcode gigadevice*/
#define SPIF_COMMAND_CONTINUOUS_MODE_GET_GD       0xB5 /* get the continuous mode opcode gigadevice*/


#define SPIF_COMMAND_WRITE_VOLATILE_CFG_OPCODE    0x81 /* write the volatile cfg */
/* Devices of 32MB/256Mbit must also support a command to enter 4-byte address mode */
#define SPIF_COMMAND_PAGE_PROGRAM		          0x12 /* Page Program command */
#define SPIF_COMMAND_SECTOR_ERASE_4KB		      0x21 /* Sector Erase (4KB) command */
#define SPIF_COMMAND_BLOCK_ERASE_64KB		      0xDC /* Block Erase (64KB) command */
#define SPIF_COMMAND_DUAL_IO_READ		          0xBC /* Dual IO Fast Read command */
#define SPIF_COMMAND_QUAD_IO_READ		          0xEC /* Quad IO Fast Read */
#define SPIF_COMMAND_QUAD_PAGE_PROGRAM		      0x34 /* Quad Page Program command */

/**
 * @brief Describes a SPI transaction.
 */
struct amd_crb_drivers_spi_transaction {
	uint8_t buf[16];
	uint8_t tx_len;
	uint8_t rx_len;
	uint8_t mode;
	uint8_t dummyCycle;
};

/**
 * @brief Describes a SPI read.
 */
struct amd_crb_drivers_spi_read {
	uint8_t buf[256];  /* max read buffer as 256 bytes */
	uint8_t rx_len;
	uint8_t mode;
};

/* spi flash is ready */
bool amd_crb_drivers_sFlashReady(void);
/* spi flash init */
int amd_crb_drivers_sFlashInit(void);
/* spi flash send command */
int amd_crb_drivers_sFlashSendCmd(uint8_t slave_index, struct amd_crb_drivers_spi_transaction cmd, uint8_t *resp, uint32_t mode);
/* spi flash single read */
int amd_crb_drivers_sFlashRead(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);
/* spi flash read status */
int amd_crb_drivers_sFlashQspiReadStatus(uint8_t slave_index, uint8_t *resp);
/* spi flash reset device */
int amd_crb_drivers_sFlashQspiRestDev(uint8_t slave_index);
/* spi flash read jedec id */
int amd_crb_drivers_sFlashJedecId(uint8_t slave_index, uint8_t *resp);

int amd_crb_drivers_flashSfdpTableRead(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);

int amd_crb_drivers_sUniqId(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);

int amd_crb_drivers_GetContinueMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);

int amd_crb_drivers_SetContinueMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);

int amd_crb_drivers_GetStatus2(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);
int amd_crb_drivers_SetQuaMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp);

int amd_crb_drivers_spi_reset(void);
int amd_crb_drivers_spi_rom_config(uint8_t *resp);


int amd_crb_drivers_sFlash_int_read(uint32_t offset, uint32_t size, uint8_t *resp);
int amd_crb_drivers_sFlash_int_write(uint32_t offset, uint32_t size, uint8_t *resp);
int amd_crb_drivers_sFlash_int_erase(uint32_t offset, uint32_t size);
int amd_crb_drivers_sFlash_int_sUniqId(uint32_t size, uint8_t *resp);
int amd_crb_flash_byte_write(uint32_t offset, uint32_t len, uint8_t *resp);



#endif // end of AMD_CRB_DRIVERS_SPI_FLASH_H_

