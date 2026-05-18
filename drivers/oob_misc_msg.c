/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#include <zephyr/logging/log.h>
#include <string.h>
#include "oob_misc_msg.h"
#include "i2c_hub.h"
#include "board_config.h"

LOG_MODULE_REGISTER(misc_msg, CONFIG_OOB_MISC_LOG_LEVEL);

struct misc_data {
	uint8_t mem_info_type;
};

static struct misc_data g_misc_data;

/*****************************************************************************
 * Function name:   oob_misc_mem_info_type_set
 * Description:     Set the memory info type
 * @param           type - Memory info type (MEM_INFO_TYPE_DDR5 or MEM_INFO_TYPE_LPDDR5)
 * @return          0 successfully, negative on error
 *****************************************************************************/
int oob_misc_mem_info_type_set(uint8_t type)
{
	if ((type != MEM_INFO_TYPE_DDR5) && (type != MEM_INFO_TYPE_LPDDR5)) {
		LOG_ERR("Invalid memory info type %d.", type);
		return -1;
	}

	g_misc_data.mem_info_type = type;
	return 0;	
}

/*****************************************************************************
 * Function name:   oob_misc_memory_info_proc
 * Description:     Process memory info request
 * @param           req  - Input memory info request
 * @param           rsp  - Output memory info response
 * @param           len - Pointer to output message length
 * @return          0 on success, negative on error
 *****************************************************************************/
int oob_misc_memory_info_proc(struct misc_msg_req *req, struct misc_msg_rsp *rsp, uint16_t *len)
{
	struct mem_info_req *mem_req = &req->mem_info;
	struct mem_info_rsp *mem_rsp = &rsp->mem_info;

	memset((uint8_t*)mem_rsp, 0, sizeof(struct mem_info_rsp));
	if (mem_req->cmd_code != MEM_INFO_CMD_REQUEST) {
		LOG_ERR("Doesn't support the MISC memory info command %d.", mem_req->cmd_code);
		mem_rsp->rsp_ext_status.bit.err_occur = 1;
		goto exit;
	}

	mem_rsp->rsp_data = g_misc_data.mem_info_type;
	LOG_DBG("Memory info type: %d.", mem_rsp->rsp_data);

exit:
	*len = sizeof(struct mem_info_rsp);
	return 0;
}
#if CONFIG_ACPI_DAC_PWR
/*****************************************************************************
 * Function name:   oob_misc_i2c_tunnel_proc
 * Description:     Process I2C tunnel request
 * @param           req  - Input I2C tunnel request
 * @param           rsp  - Output I2C tunnel response
 * @param           len - Pointer to output message length
 * @return          0 on success, negative on error
 *****************************************************************************/
int oob_misc_i2c_tunnel_proc(struct misc_msg_req *req, struct misc_msg_rsp *rsp, uint16_t *len)
{
	int ret = 0;
	struct i2c_tunnel_req *i2c_req = &req->i2c_tunnel;
	struct i2c_tunnel_rsp *i2c_rsp = &rsp->i2c_tunnel;	

	memset((uint8_t*)i2c_rsp, 0, sizeof(struct i2c_tunnel_rsp));

	if ((i2c_req->cmd_code != I2C_TUNNEL_CMD_WRITE) && (i2c_req->cmd_code != I2C_TUNNEL_CMD_READ)) {
		LOG_ERR("Doesn't support the MISC I2C tunnel command %d.", i2c_req->cmd_code);
		i2c_rsp->rsp_ext_status.bit.invalid_transport = 1;
		goto exit;
	}

	if (i2c_req->rw) {  /* I2C read */
		ret = i2c_hub_write_read(I2C_DAC_PWR_PORT, i2c_req->address, &i2c_req->reg, sizeof(i2c_req->reg), &i2c_rsp->data[0], i2c_req->length);
		if (ret) {
			i2c_rsp->rsp_ext_status.bit.data_nak = 1;
		}
		LOG_DBG("Read  PMIC/VR on I2C [%d] Addr: %02X, Reg: %02X, len: %d, Val: %02X; ret = %d",
			I2C_DAC_PWR_PORT, i2c_req->address, i2c_req->reg, i2c_req->length, i2c_rsp->data[0], ret);
	} else {                    /* I2C write */
		ret = i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, i2c_req->address, &i2c_req->reg, 1, &i2c_req->data[0], i2c_req->length);
		if (ret) {
			i2c_rsp->rsp_ext_status.bit.data_nak = 1;
		}
		LOG_DBG("Write PMIC/VR on I2C [%d] Addr: %02X, Reg: %02X, len: %d, Val: %02X; ret = %d",
			I2C_DAC_PWR_PORT, i2c_req->address, i2c_req->reg, i2c_req->length, i2c_req->data[0], ret);
	}

exit:
	*len = sizeof(struct i2c_tunnel_rsp);
	return ret;
}
#endif