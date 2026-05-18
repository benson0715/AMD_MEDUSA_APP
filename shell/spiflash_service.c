/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "espi_hub.h"
#include "espioob_mngr.h"
#if CONFIG_ESPI_SAF
#include "saf_config.h"
#endif
#include "system.h"
#include "app_pseq.h"
#include "board_config.h"
#include "app_udc.h"
#include "sci.h"
#include "scicodes.h"
#include "errcodes.h"
#include "amd_crb_drivers_spiFlash.h"
#include <zephyr/drivers/flash.h>

struct args_index {
	uint8_t device;
	uint8_t offset;
	uint8_t length;
	uint8_t data;
	uint8_t pattern;
};

static const struct args_index args_indx = {
	.offset = 1,
	.length = 2,
	.data = 2,
	.pattern = 3,
};

#if 0
/**
 *  SFDP parameter header structure
 */
typedef struct
{
    uint8_t id;                                  /**< Parameter ID LSB */
    uint8_t minor_rev;                           /**< Parameter minor revision */
    uint8_t major_rev;                           /**< Parameter major revision */
    uint8_t len;                                 /**< Parameter table length(in double words) */
    uint32_t ptp;                                /**< Parameter table 24bit pointer (byte address) */
} sfdp_para_header;

typedef struct
{
    uint8_t write_enable;
    uint8_t suspend;
    uint8_t resume;
    uint8_t status_1;

    uint8_t erase_4kb;
    uint8_t erase_32kb;
    uint8_t erase_64kb;
    uint8_t page_program;

    uint8_t quad_read;
    uint8_t non_continuous_mode;
    uint8_t continuous_mode;
    uint8_t status_2;

    uint8_t op1;
    uint8_t op2;
    uint8_t power_up;
    uint8_t power_down;

    uint8_t status_3;
    uint8_t rpmc_counter_num;
    uint8_t busy_bit_pos;
    bool is_non_vola;
    bool is_support_dpd;
    bool is_using_extended_status;
} flash_opcode_t;

static flash_opcode_t g_flash_opcode[2] = {0x0u};





inline static void erase_opcode_set(flash_opcode_t *op, uint8_t *pBase)
{
    if (pBase[0] == 0x0Cu)
    {
        op->erase_4kb = pBase[1];
    }
    else if (pBase[0] == 0x0Fu)
    {
        op->erase_32kb = pBase[1];
    }
    else if (pBase[0] == 0x10u)
    {
        op->erase_64kb = pBase[1];
    }
}
#if 0
/**
 * @brief     advance read the SFDP table
 * @param[in]
 * @return    status code
 *            - 0 success
 *            - 1 set burst with wrap failed
 * @note      none
 */
static int cmd_sdf(const struct shell *shell, size_t argc, char **argv)
{
    sfdp_para_header basic_header = {0u};
    sfdp_para_header rpmc_header = {0u};
    uint8_t sfdp_major_rev = 0u;
    uint8_t sfdp_minor_rev = 0u;
    uint8_t sfdp_NPN = 0u;
    uint32_t header_addr = 0u;
    uint8_t header[2 * 4] = {0u};
    uint8_t table[32 * 4] = {0u};
    uint8_t i = 0u;
    flash_opcode_t *pCurContext = &g_flash_opcode[0]; // TODO now only support one flash ROM setting


    amd_crb_drivers_flashSfdpTableRead(0,header_addr,8,header);
    if (!(header[3] == 0x50 &&
          header[2] == 0x44 &&
          header[1] == 0x46 &&
          header[0] == 0x53))
    {
        shell_print(shell, "Flash SFDP warning: Check SFDP signature %x%x%x%xh error. It's must be 50444653h('S' 'F' 'D' 'P').\n", header[3],
            header[2], header[1], header[0]);
        return 0;
    }
    else
    {
        sfdp_NPN = header[6];         // Number of Parameter Headers
        sfdp_major_rev = header[5];   // SFDP Major Revision Number
        sfdp_minor_rev = header[4];   // SFDP Minor Revision Number

        shell_print(shell, "Flash SFDP header checking: The reversion is V%d.%d, NPN is %d\n", sfdp_major_rev, sfdp_minor_rev, sfdp_NPN);
    }

    /* The basic parameter header is mandatory, is defined by this standard, and starts at byte offset 08h. */
    header_addr = 8u;
    memset(header, 0x0u, (2 * 4));

    amd_crb_drivers_flashSfdpTableRead(0,header_addr,8,header);
    basic_header.id        = header[0];
    basic_header.minor_rev = header[1];
    basic_header.major_rev = header[2];
    basic_header.len       = header[3];
    basic_header.ptp       = (long)header[4] | (long)header[5] << 8 | (long)header[6] << 16;

    shell_print(shell, "Check JEDEC basic flash parameter header is OK.\n");
    shell_print(shell, "The table id is %d, reversion is V%d.%d, length is %d, parameter table pointer is 0x%06lX.\n",
        basic_header.id, basic_header.major_rev, basic_header.minor_rev, basic_header.len, basic_header.ptp);

    amd_crb_drivers_flashSfdpTableRead(0,basic_header.ptp,(basic_header.len * 4),table);
    shell_print(shell, "JEDEC basic flash parameter table info:\n");
    shell_print(shell, "MSB-LSB  3    2    1    0\n");
    for (i = 0; i < basic_header.len; i++)
    {
        shell_print(shell, "[%04d] 0x%02X 0x%02X 0x%02X 0x%02X\n", i + 1, table[i * 4 + 3], table[i * 4 + 2], table[i * 4 + 1], table[i * 4]);
    }

    // See: 6.4.11 JEDEC Basic Flash Parameter Table: 8th DWORD
    // See: 6.4.12 JEDEC Basic Flash Parameter Table: 9th DWORD
    erase_opcode_set(pCurContext, &table[(8 - 1) * 4 + 0]);
    erase_opcode_set(pCurContext, &table[(8 - 1) * 4 + 2]);
    erase_opcode_set(pCurContext, &table[(9 - 1) * 4 + 0]);
    erase_opcode_set(pCurContext, &table[(9 - 1) * 4 + 2]);

    // See: 6.4.16 JEDEC Basic Flash Parameter Table: 13th DWORD
    pCurContext->resume = table[(13 - 1) * 4 + 2];
    pCurContext->suspend = table[(13 - 1) * 4 + 3];

    /* volatile status register block protect bits */
    switch ((table[0] & (0x01 << 3)) >> 3)
    {
        case 0:
            /* Block Protect bits in device's status register are solely non-volatile or may be
             * programmed either as volatile using the 50h instruction for write enable or non-volatile
             * using the 06h instruction for write enable.
             */
            pCurContext->is_non_vola = true;
            pCurContext->write_enable = 0x06u;
            break;
        case 1:
            /* block protect bits in device's status register are solely volatile. */
            pCurContext->is_non_vola = false;
            /* write enable instruction select for writing to volatile status register */
            switch ((table[0] & (0x01 << 4)) >> 4)
            {
                case 0:
                    pCurContext->write_enable = 0x05u;
                    break;
                case 1:
                    pCurContext->write_enable = 0x06u;
                    break;
            }
            break;
    }

    pCurContext->page_program = 0x02u;
    pCurContext->non_continuous_mode = 0xFFu;
    pCurContext->continuous_mode = 0xA5u;

    pCurContext->status_1 = 0x05u;

#if 0
	/* Winbound*/
    pCurContext->status_2 = 0x35u;
    pCurContext->status_3 = 0x15u;
#else
	/* GigaDevice */
    pCurContext->status_2 = 0x70u;
    pCurContext->status_3 = 0x0u;
#endif

    if (table[(14 - 1) * 4 + 0] & 0x01)
    {
        pCurContext->status_1 = 0x05u;
    }
    else if (table[(14 - 1) * 4 + 1] & 0x02)
    {
        pCurContext->status_1 = 0x70u;
    }

    if (table[(14 - 1) * 4 + 0] & BIT(3))
    {
        /* Bit 7 of the Flag Status Register may be polled any time a Program, Erase,
           Suspend/Resume command is issued, or after a Reset command while the
           device is busy. The read instruction is 70h.
           Flag Status Register bit definitions:
           bit[7]: Program or erase controller status (0=busy; 1=ready) */
        pCurContext->busy_bit_pos = 0x07u;
    }
    else if (table[(14 - 1) * 4 + 0] & BIT(2))
    {
        /* Use of legacy polling is supported by reading the Status Register with 05h
           instruction and checking WIP bit[0] (0=ready; 1=busy). */
        pCurContext->busy_bit_pos = 0x00u;
    }

    if (!(table[(14 - 1) * 4 + 3] & 0x80))
    {
        pCurContext->is_support_dpd = 1;
        pCurContext->power_down = ((table[(14 - 1) * 4 + 3] & 0x7F) << 1);
        pCurContext->power_down |= ((table[(14 - 1) * 4 + 2] & 0x80) >> 7);
        pCurContext->power_up = ((table[(14 - 1) * 4 + 2] & 0x7F) << 1);
        pCurContext->power_up |= ((table[(14 - 1) * 4 + 1] & 0x80) >> 7);
    }

    pCurContext->quad_read = table[(7 - 1) * 4 + 3];

    /* The RPMC parameter header*/
    header_addr = 8u;
    for (i = 1; i <= sfdp_NPN; i++)
    {
        amd_crb_drivers_flashSfdpTableRead(0,(header_addr * (i + 1)),8,header);
        /* The RPMC table id */
        if (header[0] == 0x03u)
        {
            rpmc_header.id        = header[0];
            rpmc_header.minor_rev = header[1];
            rpmc_header.major_rev = header[2];
            rpmc_header.len       = header[3];
            rpmc_header.ptp       = (uint32_t)header[4] | (long)header[5] << 8 | (long)header[6] << 16;
            break;
        }
        else
        {
			shell_print(shell, "Regardless of table ID 0x%02x supported in this device.\n", header[0]);
        }
    }

    if (rpmc_header.id != 0x03u)
    {
        shell_print(shell, "Check JEDEC RPMC flash parameter header is Error.\n");
        return 0;
    }
    else
    {
        shell_print(shell, "Check JEDEC RPMC flash parameter header is OK.\n");
    }
    shell_print(shell, "The table id is %d, reversion is V%d.%d, length is %d, parameter table pointer is 0x%06lX.\n",
        rpmc_header.id, rpmc_header.major_rev, rpmc_header.minor_rev, rpmc_header.len, rpmc_header.ptp);

    amd_crb_drivers_flashSfdpTableRead(0,rpmc_header.ptp,(rpmc_header.len * 4),table);
    shell_print(shell, "JEDEC RPMC flash parameter table info:\n");
    shell_print(shell, "MSB-LSB  3    2    1    0\n");
    for (i = 0; i < rpmc_header.len; i++)
    {
        shell_print(shell, "[%04d] 0x%02X 0x%02X 0x%02X 0x%02X\n", i + 1, table[i * 4 + 3], table[i * 4 + 2], table[i * 4 + 1], table[i * 4]);
    }

    pCurContext->op1 = table[(1 - 1) * 4 + 1];
    pCurContext->op2 = table[(1 - 1) * 4 + 2];

    // 0b: Poll for OP1 BUSY using OP2 Extended Status[0]. No OP1 Suspend State support.
    // 1b: Poll for OP1 BUSY using Read Status (05h). Suspend State is supported.
    pCurContext->is_using_extended_status = !(table[(1 - 1) * 4 + 0] & BIT(2));

    // Num Counters – 1 = manufacturer specified
    // Number of supported counters minus one. Suggested value is 3 (4 counters supported).
    pCurContext->rpmc_counter_num = ((table[(1 - 1) * 4 + 0] & 0xF0) >> 4);

    shell_print(shell, "The parse resule of JEDEC Table:\n");
    shell_print(shell, "Write_Enable: 0x%02X, Suspend:             0x%02X, Resume:                  0x%02X, Status_1:     0x%02X\n",
        pCurContext->write_enable,
        pCurContext->suspend,
        pCurContext->resume,
        pCurContext->status_1);

    shell_print(shell, "Erase_4kb:    0x%02X, Erase_32kb:          0x%02X, Erase_64kb:              0x%02X, Page_program: 0x%02X\n",
        pCurContext->erase_4kb,
        pCurContext->erase_32kb,
        pCurContext->erase_64kb,
        pCurContext->page_program);

    shell_print(shell, "Quad_read:    0x%02X, Non_continuous_mode: 0x%02X, continuous_mode:         0x%02X, status_2:     0x%02X\n",
        pCurContext->quad_read,
        pCurContext->non_continuous_mode,
        pCurContext->continuous_mode,
        pCurContext->status_2);

    shell_print(shell, "RPMC_Op1:     0x%02X, RPMC_Op2:            0x%02X, Power_up:                0x%02X, Power_down:   0x%02X\n",
        pCurContext->op1,
        pCurContext->op2,
        pCurContext->power_up,
        pCurContext->power_down);

    shell_print(shell, "Status_3:     0x%02X, Rpmc_counter_num:    0x%02X, Status_busy_bit_pos:     0x%02X\n",
        pCurContext->status_3,
        pCurContext->rpmc_counter_num,
        pCurContext->busy_bit_pos);

    shell_print(shell, "Is_w_non_vola: 0x%02X, Is_support_dpd:     0x%02X, Is_RPMC_extended_status: 0x%02X\n",
        pCurContext->is_non_vola,
        pCurContext->is_support_dpd,
        pCurContext->is_using_extended_status);

    return 0;
}
#endif
#endif

/**
 * @brief Shell command handler to read data from SPI flash
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing offset and length
 * @return 0 on success
 */
static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;
        const struct device *extflash_dev;
	extflash_dev =  DEVICE_DT_GET(EXT_FLASH);
	
	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);


	shell_print(shell, "Reading %d bytes from SPIFLASH, offset %d...", len,
		    addr);

	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		flash_read(extflash_dev ,addr,data,pending);
	      //  amd_crb_drivers_sFlashRead(0, addr,pending,data);

		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}

	shell_print(shell, "");
	return 0;
}
#if 0
static int cmd_getid(const struct shell *shell, size_t argc, char **argv)
{

	uint8_t rxBuffer[SHELL_HEXDUMP_BYTES_IN_LINE];
	amd_crb_drivers_sFlashJedecId(0,rxBuffer);
	shell_print(shell, "Get Flash JedecId %x ",  ((uint32_t)((rxBuffer[2] << 16u) | (rxBuffer[1] << 8u) | rxBuffer[0])));

	
	return 0;
}



static int cmd_setsaf(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t rxBuffer[32];
	amd_crb_drivers_SetQuaMode(0,0,1,rxBuffer);
	shell_print(shell, "set to 4 Line mode");

        amd_crb_drivers_sFlashQspiReadStatus(0, rxBuffer);
	shell_print(shell, "get status1");
	shell_hexdump_line(shell, 0, rxBuffer, 1);
 	amd_crb_drivers_GetStatus2(0,0,1,rxBuffer);
	shell_print(shell, "get status2 Line mode");
	shell_hexdump_line(shell, 0, rxBuffer, 1);
#if CONFIG_ESPI_SAF
	int ret = initialize_saf_bridge();
	shell_print(shell, "initialize_saf_bridge return ret= %d",  ret);
#endif

	return 0;
}




static int cmd_getmod(const struct shell *shell, size_t argc, char **argv)
{

	uint8_t rxBuffer[32];
	amd_crb_drivers_GetContinueMode(0,0,1,rxBuffer);
	shell_hexdump_line(shell, 0, rxBuffer, 1);
	
	return 0;
}

static int cmd_getstatus(const struct shell *shell, size_t argc, char **argv)
{

	uint8_t rxBuffer[32];
	
         amd_crb_drivers_GetStatus2(0,0,1,rxBuffer);
	shell_hexdump_line(shell, 0, rxBuffer, 1);
	
	return 0;
}

static int cmd_setact(const struct shell *shell, size_t argc, char **argv)
{
     int8_t rxBuffer[32];
     amd_crb_drivers_SetContinueMode(0,0,1,rxBuffer);
     shell_print(shell, "set to continue mode");
     return 0;
}


/*
 * SAF hardware limited to 1 to 64 byte read requests.
 */
static uint32_t saf_read(const struct shell *shell,uint32_t spi_addr, uint8_t *dest, int len)
{
	int rc, chunk_len, n;
	struct espi_saf_packet saf_pkt = { 0 };

	if ((dest == NULL) || (len < 0)) {
		return 1;
	}

	saf_pkt.flash_addr = spi_addr;
	saf_pkt.buf = dest;

	n = len;
	while (n) {
		chunk_len = 64;
		if (n < 64) {
			chunk_len = n;
		}

		saf_pkt.len = chunk_len;

		rc = saf_read_flash(&saf_pkt);
		if (rc != 0) {
			shell_print(shell, "saf_read_flash error %d",rc);
		}

		saf_pkt.flash_addr += chunk_len;
		saf_pkt.buf += chunk_len;
		n -= chunk_len;
	}

	return len;
}


static int cmd_saf_read(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);
	shell_print(shell, "EC Op SAF bridge Reading %d bytes from SPIFLASH, offset %d...", len,
		    addr);
	
	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
	        saf_read(0, addr,data,pending);

		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}


	shell_print(shell, "");
	return 0;
}

static int cmd_saf_erase(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);
	shell_print(shell, "EC Op SAF bridge erase %d bytes from SPIFLASH, offset %d...", len,
		    addr);
	struct espi_saf_packet saf_pkt = { 0 };

	uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

	saf_pkt.flash_addr = addr ;
	saf_pkt.buf = data;
        saf_pkt.len = 4096;
       saf_erase_flash(&saf_pkt );



	shell_print(shell, "");
	return 0;
}

static int cmd_set4linemod(const struct shell *shell, size_t argc, char **argv)
{

	uint8_t rxBuffer[32];
	amd_crb_drivers_SetQuaMode(0,0,1,rxBuffer);
	shell_print(shell, "set to 4 Line mode");
	
	return 0;
}

#endif

/**
 * @brief Shell command handler to get flash unique ID via JEDEC read
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success
 */
static int cmd_getUqid(const struct shell *shell, size_t argc, char **argv)
{
        const struct device *extflash_dev;
	uint8_t rxBuffer[32];
	extflash_dev =  DEVICE_DT_GET(EXT_FLASH);
	flash_read_jedec_id(extflash_dev ,rxBuffer);
	//amd_crb_drivers_sUniqId(0,0,3,rxBuffer);
	shell_hexdump_line(shell, 0, rxBuffer, 3);
	
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(spiflash_cmds,
	SHELL_CMD_ARG(read, NULL, "<offset> <length>", cmd_read, 3, 0),
	//SHELL_CMD_ARG(saf_read, NULL, "<offset> <length>", cmd_saf_read, 3, 0),
	//SHELL_CMD_ARG(saf_erase, NULL, "<offset> <length>", cmd_saf_erase, 3, 0),
	//SHELL_CMD(getid, NULL, NULL, cmd_getid),
	//SHELL_CMD(getsdf, NULL, NULL, cmd_sdf),
        SHELL_CMD(getuqid, NULL, NULL, cmd_getUqid),
	//SHELL_CMD(getmod, NULL, NULL, cmd_getmod),
	//SHELL_CMD(getstatus, NULL, NULL, cmd_getstatus),
	//SHELL_CMD(setact, NULL, NULL, cmd_setact),
	//SHELL_CMD(set4linemod, NULL, NULL, cmd_set4linemod),
	//SHELL_CMD(setsaf, NULL, NULL, cmd_setsaf),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(spiflash, &spiflash_cmds, "SPI FLASH shell commands", NULL);