/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "acpi_region.h"
extern struct acpi_tbl g_acpi_tbl;


struct args_index {
	uint8_t device;
	uint8_t offset;
	uint8_t length;
	uint8_t data;
	uint8_t pattern;
	uint8_t type;
};

static const struct args_index args_indx = {
	.offset = 1,
	.length = 2,
	.data = 2,
	.pattern = 3,
	.type = 4,
};

/**
 * @brief Shell command to read EC memory region
 *
 * @param shell Shell instance
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int cmd_readec(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);


	shell_print(shell, "Reading %d bytes from memory, offset %d...", len,
		    addr);

	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		for (int i=0;i<pending;i++) {
                    data[i] = *(uint8_t  *)(&g_acpi_tbl.ACPI_SPACE_RSV_00[0]+addr+i);
                }

		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}

	shell_print(shell, "");
	return 0;
}

/**
 * @brief Shell command to write EC memory region
 *
 * @param shell Shell instance
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, negative error code on failure
 */
static int cmd_writeec(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t wr_buf[CONFIG_AMD_MEMORY_SHELL_BUFFER_SIZE];
	unsigned long byte;
        size_t addr;
	size_t len;
	int i;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = argc - args_indx.data;

	if (len > sizeof(wr_buf)) {
		shell_error(shell, "Write buffer size (%d bytes) exceeded",
			    sizeof(wr_buf));
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		byte = strtoul(argv[args_indx.data + i], NULL, 0);
		if (byte > UINT8_MAX) {
			shell_error(shell, "Error parsing data byte %d", i);
			return -EINVAL;
		}
		wr_buf[i] = byte;
	}

	shell_print(shell, "Writing %d bytes to memory...", len);

	for (i=0;i<len;i++) {
            *(uint8_t  *)(&g_acpi_tbl.ACPI_SPACE_RSV_00[0]+addr+i) = wr_buf[i];
           }

	return 0;
}

/**
 * @brief Shell command to read raw memory
 *
 * @param shell Shell instance
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success
 */
static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);


	shell_print(shell, "Reading %d bytes from memory, addr %x...", len,
		    addr);

	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		for (int i=0;i<pending;i++) {
                    data[i] = *(uint8_t  *)(addr+i);
                }

		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}

	shell_print(shell, "");
	return 0;
}

/**
 * @brief Check if memory address is accessible
 *
 * @param addr Memory address to check
 * @param length Length of memory region
 * @return 1 if accessible, 0 otherwise
 */
unsigned char  Mem_AccessOK(unsigned int addr, unsigned int  length)
{
    
    return 1;
}

/**
 * @brief Shell command to write raw memory with configurable access width
 *
 * @param shell Shell instance
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, -1 on failure
 */
static int cmd_write(const struct shell *shell, size_t argc,char** argv)
{
  int i;
  unsigned int   w_addr, w_value,w_length;
  unsigned int   *mem_ptr32;
  unsigned short   *mem_ptr16;
  unsigned char    *mem_ptr8;
  char mode;

  if (argc < 4 || argc > 5)
  {
    shell_print(shell,"Error argument! Help with 'H/h w'");
    return -1;
  }
  /* translate the string into number */
  w_addr = strtoul(argv[args_indx.offset], NULL, 0);
  w_length = strtoul(argv[args_indx.pattern], NULL, 0);
  w_value = strtoul(argv[args_indx.data], NULL, 0);
 
  if (argc ==4) 
  {
     mode = 'W';
  } 
  else 
  {
     mode = argv[args_indx.type][0];
  }

  switch(mode)
  {
      case 'W':
        if(Mem_AccessOK(w_addr, 0))
        {
          
            mem_ptr32 = (unsigned int *)( w_addr & 0xfffffffc) ;
            for (i = 0; i < w_length; i++)
            {
              *mem_ptr32 = w_value;
              mem_ptr32++;
            }
         
          shell_print(shell,"SUCCESS for write memory!");
        }
        else
        {
          shell_print(shell,"Error address for write! Help 'H/h w'");
        }
        break;

    case 'H':
      if(Mem_AccessOK (w_addr, 0))
      {

          mem_ptr16 = (unsigned short *)( w_addr & 0xfffffffe) ;
          for (i = 0; i < w_length; i++)
          {
            *mem_ptr16 = w_value;
            mem_ptr16++;
          }
        shell_print(shell,"SUCCESS for write memory!");
      }
      else
      {
        shell_print(shell,"Error address for write! Help 'H/h w'");
      }
      break;
    case 'B':
      if(Mem_AccessOK (w_addr, 0))
      {
        
          mem_ptr8 = (unsigned char *) w_addr  ;
          for (i = 0; i < w_length; i++)
          {
            *mem_ptr8 = w_value;
            mem_ptr8++;
          }
        shell_print(shell,"SUCCESS for write memory!");
      }
      else
      {
        shell_print(shell,"Error address for write! Help 'H/h w'");
      }
      break;

    default:
      shell_print(shell,"Error argument!");

      return 0;
    }
    return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(memory_cmds,
	SHELL_CMD_ARG(ecread, NULL, "<addr> <length>", cmd_readec, 3, 0),
	SHELL_CMD_ARG(ecwrite, NULL,
		      "<addr> [byte0] <byte1> .. <byteN>", cmd_writeec,
		      3, CONFIG_AMD_MEMORY_SHELL_BUFFER_SIZE - 1),
	SHELL_CMD_ARG(read, NULL, "<addr> <length>", cmd_read, 3, 0),
	SHELL_CMD_ARG(write, NULL,
		      "<addr> [byte0] <byte1> .. <byteN>", cmd_write,
		      3, CONFIG_AMD_MEMORY_SHELL_BUFFER_SIZE - 1),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(memory, &memory_cmds, "Memory shell commands", NULL);