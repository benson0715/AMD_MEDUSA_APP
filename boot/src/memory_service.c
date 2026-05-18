/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/flash.h>
#include <stdlib.h>

#define EXT_FLASH           DT_NODELABEL(ext_flash0)
static const struct device *extflash_dev;
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
 * @brief Shell command handler to read memory at specified address
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing address and length
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
 * @brief Shell command handler to read external flash at specified address
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing address and length
 * @return 0 on success
 */
static int cmd_read_flash(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);


	shell_print(shell, "Reading %d bytes from memory, addr %x...", len,
		    addr);
	extflash_dev =  DEVICE_DT_GET(EXT_FLASH);
	

	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);

		flash_read(extflash_dev,addr,data,pending);
		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}

	shell_print(shell, "");
	return 0;
}

/**
 * @brief Check if memory address range is accessible
 *
 * @param addr Starting address to check
 * @param length Length of memory range
 * @return 1 if access is permitted, 0 otherwise
 */
unsigned char  Mem_AccessOK(unsigned int addr, unsigned int  length)
{
    
    return 1;
}

/**
 * @brief Shell command handler to write data to memory
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing address, value, length, and optional type
 * @return 0 on success, -1 on argument error
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
	SHELL_CMD_ARG(read, NULL, "<addr> <length>", cmd_read, 3, 0),
	SHELL_CMD_ARG(readflash, NULL, "<addr> <length>", cmd_read_flash, 3, 0),
	SHELL_CMD_ARG(write, NULL,
		      "<addr> [byte0] <byte1> .. <byteN>", cmd_write,
		      3, 10),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(memory, &memory_cmds, "Memory shell commands", NULL);