/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#include <stdint.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include "oob_msg_mngr.h"

LOG_MODULE_REGISTER(oob_msg, CONFIG_OOB_MSG_LOG_LEVEL);

struct oob_msg_mngr_context {
	uint8_t in_buffer[MSG_MNGR_BUFFER_LENGTH];
	uint16_t in_pos;

	uint8_t out_buffer[MSG_MNGR_BUFFER_LENGTH];
	uint16_t out_pos;
	uint16_t out_buffer_len;
	uint16_t out_remain_len;
	uint8_t out_seq;
};

static struct oob_msg_mngr_context g_oob_msg_mngr_ctx = {0};

/*****************************************************************************
 * Function name:   _smbus_from_soc_header_check
 * Description:     Validate SMBus header from SoC for correctness
 * @param           head - SMBus header structure to validate
 * @return          0 if valid, negative on validation failure
 * @note            Checks access type, address, command, and byte count
 *****************************************************************************/
static int _smbus_from_soc_header_check(struct smbus_header head)
{
	if (head.access != SMBUS_ACCESS_W) {
		return -1;
	}

	if (head.dst_addr != SMBUS_EC_SLAVE_ADDRESS) {
		return -2;
	}

	if (head.cmd != SMBUS_CMD_MCTP) {
		return -3;
	}

	if (head.bytes < SMBUS_BYTES) {
		return -4;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   _mctp_from_soc_header_check
 * Description:     Validate MCTP header from SoC for protocol compliance
 * @param           head - MCTP header structure to validate
 * @return          0 if valid, negative on validation failure
 * @note            Checks MCTP enable, addresses, tags, and endpoint IDs
 *****************************************************************************/
static int _mctp_from_soc_header_check(struct mctp_header head)
{
	if (head.mctp_en != MCTP_ENABLED) {
		return -1;
	}

	if (head.src_addr != SMBUS_SOC_SLAVE_ADDRESS) {
		return -2;
	}

	if (head.msg_tag != MCTP_MSG_TAG) {
		return -3;
	}

	if (head.owner_tag != MCTP_OWNER_TAG) {
		return -4;
	}

	if (head.src_eid != MCTP_SOC_EID) {
		return -5;
	}

	if (head.dst_eid != MCTP_EC_EID) {
		return -6;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   _msg_from_soc_header_check
 * Description:     Validate complete message header (SMBus + MCTP) from SoC
 * @param           head - Complete message header structure
 * @return          0 if valid, negative on validation failure
 * @note            Combines SMBus and MCTP header validation
 *****************************************************************************/
static int _msg_from_soc_header_check(struct msg_header head)
{
	int ret = 0;

	ret = _smbus_from_soc_header_check(head.smbus);
	if (ret != 0) {
		LOG_ERR("SMBUS header content check fail: %d", ret);
		return -1;
	}
	ret = _mctp_from_soc_header_check(head.mctp);
	if (ret != 0) {
		LOG_ERR("MCTP header content check fail: %d", ret);
		return -2;
	}
	return ret;
}

/*****************************************************************************
 * Function name:   _msg_to_soc_header_pack
 * Description:     Pack message header for transmission to SoC
 * @param           pHeader - Pointer to message header structure to populate
 * @param           length  - Payload length for SMBus byte count
 * @param           esom    - MCTP packet fragmentation state
 * @param           seq     - MCTP packet sequence number
 * @return          None
 * @note            Sets up SMBus and MCTP headers for EC-to-SoC communication
 *****************************************************************************/
static void _msg_to_soc_header_pack(struct msg_header *pHeader, uint8_t length, uint8_t esom, uint8_t seq)
{
	pHeader->smbus.access = SMBUS_ACCESS_W;
	pHeader->smbus.dst_addr = SMBUS_SOC_SLAVE_ADDRESS;
	pHeader->smbus.cmd = SMBUS_CMD_MCTP;
	pHeader->smbus.bytes = length;

	pHeader->mctp.mctp_en = MCTP_ENABLED;
	pHeader->mctp.src_addr = SMBUS_EC_SLAVE_ADDRESS;
	pHeader->mctp.msg_tag = MCTP_MSG_TAG;
	pHeader->mctp.owner_tag = MCTP_OWNER_TAG;
	pHeader->mctp.src_eid = MCTP_EC_EID;
	pHeader->mctp.dst_eid = MCTP_SOC_EID;
	pHeader->mctp.version = MCTP_HDR_VERSION;
	pHeader->mctp.rsvd_4 = MCTP_RSVD;

	if (esom == ESOM_START) {
		pHeader->mctp.som = 1;
		pHeader->mctp.eom = 0;
		pHeader->mctp.pkt_seq = 0;
	} else if (esom == ESOM_ONGOING) {
		pHeader->mctp.som = 0;
		pHeader->mctp.eom = 0;
		pHeader->mctp.pkt_seq = seq;
	} else if (esom == ESOM_END) {
		pHeader->mctp.som = 0;
		pHeader->mctp.eom = 1;
		pHeader->mctp.pkt_seq = seq;
	} else {
		pHeader->mctp.som = 1;
		pHeader->mctp.eom = 1;
		pHeader->mctp.pkt_seq = 0;
	}
}

/*****************************************************************************
 * Function name:   _esom_identify
 * Description:     Identify MCTP packet fragmentation state (SOM/EOM flags)
 * @param           head - MCTP header to analyze
 * @return          ESOM_* constant indicating fragmentation state
 * @note            Determines if packet is single, start, middle, or end fragment
 *****************************************************************************/
static int _esom_identify(struct mctp_header head)
{
	if (head.som && !head.eom) {
		return ESOM_START;
	} else if (!head.som && !head.eom) {
		return ESOM_ONGOING;
	} else if (!head.som && head.eom) {
		return ESOM_END;
	}

	return ESOM_NOMORE;
}

/*****************************************************************************
 * Function name:   _payload_length
 * Description:     Calculate payload length from SMBus header
 * @param           head - SMBus header containing byte count
 * @return          Payload length (total bytes minus SMBus+MCTP overhead)
 * @note            Subtracts protocol overhead to get actual data length
 *****************************************************************************/
static int _payload_length(struct smbus_header head)
{
	return head.bytes - SMBUS_MCTP_BYTES;
}

/*****************************************************************************
 * Function name:   _broken_up_msg_len
 * Description:     Calculate remaining message length after fragmentation
 * @param           length - Total message length
 * @return          Remaining message length (0 if no fragmentation needed)
 * @note            If the length is less than or equal to max bytes, return 0.
 *                  Otherwise, return the difference (remaining bytes to send).
 *****************************************************************************/
static uint16_t _broken_up_msg_len(uint16_t length)
{
	if (length <= SMBUS_MSG_MAX_BYTES) {
		return 0;
	}
	return length - SMBUS_MSG_MAX_BYTES;
}

/*****************************************************************************
 * Function name:   _msg_info_check
 * Description:     Validate message info structure for message type
 * @param           info - Message info structure to validate
 * @return          0 if valid message type, negative on validation failure
 * @note            Checks integrity checking flag and message type
 *****************************************************************************/
static int _msg_info_check(struct msg_info info)
{
	if (info.ic_en) {
		LOG_ERR("Doesn't support the integrity checking.");
	}

	switch (info.type) {
		case MSG_RPMC_TYPE:
		case MSG_MEMORY_INFO_TYPE:
		case MSG_I2C_TUNNE_TYPE: {
			break;
		}
		default: {
			LOG_ERR("Doesn't support the mctp message type 0x%02x.", info.type);
			return -1;
		}
	}

	return 0;
}

/*****************************************************************************
 * Function name:   _msg_info_pack
 * Description:     Pack message info structure for RPMC response
 * @param           pInfo - Pointer to message info structure to populate
 * @param           type  - Message type to set
 * @param           pLen  - Pointer to length variable to update
 * @return          None
 * @note            Sets RPMC message type and updates total message length
 *****************************************************************************/
static void _msg_info_pack(struct msg_info *pInfo, uint8_t type, uint16_t *pLen)
{
	pInfo->ic_en = 0;
	pInfo->type = type;
	
	*pLen += sizeof(struct msg_info);
}

/*****************************************************************************
 * Function name:   _handle_input_fragmentation
 * Description:     Handle input message fragmentation based on ESOM state
 * @param           ctx    - Message manager context
 * @param           pInBuf - Input message buffer
 * @return          0 = continue processing, 1, 2 = early return (incomplete message),
 *                  negative = error
 * @note            Manages message reassembly for fragmented MCTP packets
 *****************************************************************************/
static int _handle_input_fragmentation(struct oob_msg_mngr_context *ctx, oob_msg_buf_t *pInBuf)
{
	int ret = _esom_identify(pInBuf->head.mctp);
	uint8_t len = _payload_length(pInBuf->head.smbus);
	switch (ret) {
		case ESOM_NOMORE: {
			memset(ctx->in_buffer, 0x0, MSG_MNGR_BUFFER_LENGTH);
			memcpy(ctx->in_buffer, (uint8_t *)&pInBuf->in, len);
			ctx->in_pos = 0;
			break;
		}
		case ESOM_START: {
			memset(ctx->in_buffer, 0x0, MSG_MNGR_BUFFER_LENGTH);
			memcpy(ctx->in_buffer, (uint8_t *)&pInBuf->in, len);
			ctx->in_pos = len;
			return 1;
		}
		case ESOM_ONGOING: {
			memcpy((uint8_t *)(ctx->in_buffer + ctx->in_pos), (uint8_t *)&pInBuf->in, len);
			ctx->in_pos += len;
			return 2;
		}
		case ESOM_END: {
			memcpy((uint8_t *)(ctx->in_buffer + ctx->in_pos), (uint8_t *)&pInBuf->in, len);
			ctx->in_pos = 0;
			break;
		}
		default: {
			LOG_ERR("Message esom check fail: %d", ret);
			return -3;
		}
	}
	return 0;
}

/*****************************************************************************
 * Function name:   _prepare_output_message
 * Description:     Prepare output message header and calculate final length
 * @param           ctx     - Message manager context
 * @param           pOutBuf - Output message buffer
 * @param           pOutLen - Pointer to output length (input/output parameter)
 * @return          None
 * @note            Handles message fragmentation and adds protocol overhead
 *****************************************************************************/
static void _prepare_output_message(struct oob_msg_mngr_context *ctx, oob_msg_buf_t *pOutBuf, uint16_t *pOutLen)
{
	uint16_t payload_len = 0;
	uint8_t esom = ESOM_NOMORE;
	
	if (ctx->out_remain_len == 0) { // first packet
		ctx->out_remain_len = _broken_up_msg_len(ctx->out_buffer_len);
		ctx->out_seq = 0;

		if (ctx->out_remain_len == 0) {
			payload_len = ctx->out_buffer_len;
			esom = ESOM_NOMORE;
		} else {
			payload_len = SMBUS_MSG_MAX_BYTES;
			esom = ESOM_START;
		}
	} else { // remaining packets
		payload_len = ctx->out_remain_len;
		ctx->out_remain_len = _broken_up_msg_len(payload_len);

		ctx->out_seq = (ctx->out_seq + 1) & 0x3; // Wrap around at 4 (0-3)

		if (ctx->out_remain_len == 0) {
			esom = ESOM_END;
		} else {
			payload_len = SMBUS_MSG_MAX_BYTES;
			esom = ESOM_ONGOING;
		}
	}

	// Copy payload data to output buffer
	memcpy(pOutBuf->trans_buf, (uint8_t *)(ctx->out_buffer + ctx->out_pos), payload_len);
	ctx->out_pos += payload_len;

	// Pack header with length including MCTP header
	uint16_t header_len = payload_len + SMBUS_MCTP_BYTES;
	_msg_to_soc_header_pack(&pOutBuf->head, header_len, esom, ctx->out_seq);

	// Calculate total packet length (SMBus header + MCTP header + payload + SMBus PEC)
	*pOutLen = SMBUS_BYTES + SMBUS_MCTP_BYTES + payload_len + SMBUS_PEC_BYTES;
}

/*****************************************************************************
 * Function name:   _service_proc
 * Description:     Route messages to appropriate service processors
 * @param           pIn_msg  - Input message
 * @param           pOut_msg - Output response message
 * @param           pOut_len - Pointer to output message length
 * @return          0 on success, negative on error
 * @note            Currently supports RPMC message type only
 *****************************************************************************/
static int _service_proc(msg_in_t *pIn_msg, msg_out_t *pOut_msg, uint16_t *pOut_len)
{
	int ret = -1;
	uint8_t type = pIn_msg->info.type;
	
	ret = _msg_info_check(pIn_msg->info);
	if (ret != 0) {
		LOG_ERR("Message info check fail: %d", ret);
		return ret;
	}
	
	switch (type) {
		case MSG_RPMC_TYPE: {
			ret = oob_rc_msg_proc(&pIn_msg->rpmc, &pOut_msg->rpmc, pOut_len);
			if (ret != 0) {
				LOG_ERR("RPMC service process fail: %d", ret);
			}
			break;
		}
		case MSG_MEMORY_INFO_TYPE: {
			ret = oob_misc_memory_info_proc(&pIn_msg->misc, &pOut_msg->misc, pOut_len);
			if (ret != 0) {
				LOG_ERR("Memory info service process fail: %d", ret);
			}
			break;
		}
		case MSG_I2C_TUNNE_TYPE: {
#if CONFIG_ACPI_DAC_PWR
			ret = oob_misc_i2c_tunnel_proc(&pIn_msg->misc, &pOut_msg->misc, pOut_len);
			if (ret != 0) {
				LOG_ERR("I2C tunnel service process fail: %d", ret);
			}
#else
			LOG_ERR("I2C tunnel service process is not supported");
#endif
			break;
		}
		default:
			break;
	}

	_msg_info_pack(&pOut_msg->info, type, pOut_len);
	return ret;
}

/*****************************************************************************
 * Function name:   oob_msg_mngr_proc
 * Description:     Main OOB message processing function with fragmentation support
 * @param           pInData  - Input data buffer containing OOB message
 * @param           InLen    - Input data buffer size
 * @param           pOutData - Output data buffer for response
 * @param           pOutLen  - Pointer to output data buffer size
 * @return          0 on success, negative on error
 * @note            Handles MCTP fragmentation and routes to service processors
 *****************************************************************************/
int oob_msg_mngr_proc(void *pInData, uint8_t InLen, void *pOutData, uint16_t *pOutLen)
{
	struct oob_msg_mngr_context *ctx = &g_oob_msg_mngr_ctx;

	int ret = 0;
	oob_msg_buf_t *pInBuf = (oob_msg_buf_t *)pInData;
	oob_msg_buf_t *pOutBuf = (oob_msg_buf_t *)pOutData;

	// Clear it.
	*pOutLen = 0;

	if (InLen < SMBUS_BYTES) {
		LOG_ERR("Invalid SMbus bytes %d", InLen);
		return -1;
	}

	ret = _msg_from_soc_header_check(pInBuf->head);
	if (ret != 0) {
		LOG_ERR("Message header content check fail: %d", ret);
		return -2;
	}

	ret = _handle_input_fragmentation(ctx, pInBuf);
	if (ret != 0) {
		// ret > 0 means message is incomplete (early return needed)
		// ret < 0 means error (already logged in the function)
		return (ret > 0) ? 0 : ret;
	}

	ctx->out_pos = 0;
	ctx->out_seq = 0;
	ctx->out_remain_len = 0;
	ctx->out_buffer_len = 0;
	memset(ctx->out_buffer, 0x0, MSG_MNGR_BUFFER_LENGTH);
	ret = _service_proc((msg_in_t *)ctx->in_buffer, (msg_out_t *)ctx->out_buffer, &ctx->out_buffer_len);
	if (ret != 0) {
		LOG_ERR("Message service process fail: %d", ret);
		return -4;
	}

	_prepare_output_message(ctx, pOutBuf, pOutLen);
	return 0;
}

/*****************************************************************************
 * Function name:   oob_msg_mngr_proc_remainning
 * Description:     Process remaining fragmented OOB message packets
 * @param           pOutBuf - Output data buffer for remaining message fragment
 * @param           pOutLen - Pointer to output data buffer size
 * @return          0 on success, negative on error
 * @note            Handles subsequent MCTP packet fragments after initial message
 *****************************************************************************/
int oob_msg_mngr_proc_remainning(void *pOutBuf, uint16_t *pOutLen)
{
	struct oob_msg_mngr_context *ctx = &g_oob_msg_mngr_ctx;

	*pOutLen = 0;
	if (ctx->out_remain_len == 0) {
		return 0;
	}

	_prepare_output_message(ctx, pOutBuf, pOutLen);
	return 0;
}

/*****************************************************************************
 * Function name:   oob_msg_mngr_postpne_proc
 * Description:     Handle postponed OOB message operations
 * @param           None
 * @return          None
 * @note            Processes deferred RPMC ROM write operations
 *****************************************************************************/
void oob_msg_mngr_postpne_proc(void)
{
	oob_rc_postpone_service();
}

/*****************************************************************************
 * Function name:   oob_msg_mngr_init_proc
 * Description:     Initialize OOB message manager and related services
 * @param           None
 * @return          None
 * @note            Sets up RPMC services and underlying hardware
 *****************************************************************************/
void oob_msg_mngr_init_proc(void)
{
	int ret = oob_rc_init();
	if (ret != 0) {
		LOG_ERR("RPMC init fail: %d", ret);
		return;
	}
}