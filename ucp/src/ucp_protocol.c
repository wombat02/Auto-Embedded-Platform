/*
 * File: ucp_protocol.c
 * Brief: Portable frame helpers (checksum and static payload sizing).
 *
 * Author:
 * GitHub:
 * Email:
 * License:
 */

#include "ucp_protocol.h"

uint8_t ucp_calc_checksum(const uint8_t *data, uint8_t len)
{
	uint8_t cs = 0u;
	for (uint8_t i = 0u; i < len; i++)
	{
		cs ^= data[i];
	}
	return cs;
}

uint8_t ucp_payload_len_for_app_pkt(uint8_t app_id, uint8_t pkt_id)
{
	if (app_id == (uint8_t)UCP_APP_ID_CONTROL)
	{
		switch (pkt_id)
		{
		case UCP_CTRL_PKT_ID_DEVICE_HELLO_REQ:
		case UCP_CTRL_PKT_ID_DEVICE_HELLO_RSP:
			return (uint8_t)sizeof(ucp_ctrl_hello_payload_t);
		case UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ:
		case UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_RSP:
			return (uint8_t)sizeof(ucp_ctrl_capability_payload_t);
		case UCP_CTRL_PKT_ID_APP_VERSION_QUERY_REQ:
		case UCP_CTRL_PKT_ID_APP_VERSION_QUERY_RSP:
			return (uint8_t)sizeof(ucp_ctrl_capability_payload_t);
		case UCP_CTRL_PKT_ID_KEEPALIVE:
			return 0u;
		case UCP_CTRL_PKT_ID_LINK_READY_REQ:
		case UCP_CTRL_PKT_ID_LINK_READY_RSP:
			return (uint8_t)sizeof(ucp_ctrl_link_ready_payload_t);
		case UCP_CTRL_PKT_ID_NOT_SUPPORTED:
		case UCP_CTRL_PKT_ID_WARNING:
		case UCP_CTRL_PKT_ID_ERROR:
			return (uint8_t)sizeof(ucp_ctrl_status_payload_t);
		default:
			return 0u;
		}
	}

	(void)app_id;
	(void)pkt_id;
	return 0u;
}
