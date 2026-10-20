/*
 * Unbuilt sketch: minimal UCP REQ_RSP application (APP 0x60, version V1.0).
 *
 * Not linked into the STM32 example. Copy into product glue and register the
 * vtable next to ucp_tlm / ucp_cfg. See ucp/docs/UCP_APPLICATION_LAYER.md and
 * examples/stm32_h5/GLUE.md.
 *
 * Wire layout (illustrative):
 *   REQ payload: corr[2 LE] | opcode | arg…
 *   RSP payload: corr[2 LE] | opcode | status | data…
 */
#include "ucp_port.h"
#include "ucp_protocol.h"

#include <string.h>

#define DEMO_MIN_APP_ID 0x60u
#define DEMO_MIN_APP_VER 0x0100u /* V1.0 — high byte major, low byte minor */
#define DEMO_MIN_OP_ECHO 0x01u

static void demo_min_on_rx(const ucp_pkt_t *pkt)
{
	uint16_t corr;
	uint8_t opcode;
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t rsp_len;

	if ((pkt == NULL) || (pkt->pkt_id != (uint8_t)UCP_SERVICE_PKT_ID_REQ) ||
		(pkt->payload_len < 3u))
	{
		return;
	}

	corr = (uint16_t)pkt->payload[0] | ((uint16_t)pkt->payload[1] << 8);
	opcode = pkt->payload[2];
	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = opcode;

	if (opcode != DEMO_MIN_OP_ECHO)
	{
		rsp[3] = (uint8_t)UCP_SERVICE_STATUS_UNSUPPORTED_OPCODE;
		(void)ucp_send_pkt(DEMO_MIN_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, 4u);
		return;
	}

	rsp[3] = (uint8_t)UCP_SERVICE_STATUS_OK;
	rsp_len = 4u;
	if (pkt->payload_len > 3u)
	{
		uint8_t n = (uint8_t)(pkt->payload_len - 3u);
		if ((uint16_t)rsp_len + n > sizeof(rsp))
		{
			n = (uint8_t)(sizeof(rsp) - rsp_len);
		}
		memcpy(&rsp[4], &pkt->payload[3], n);
		rsp_len = (uint8_t)(rsp_len + n);
	}
	(void)ucp_send_pkt(DEMO_MIN_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, rsp_len);
}

void demo_min_ucp_app_fill_vtable(ucp_app_vtable_t *out)
{
	if (out == NULL)
	{
		return;
	}
	out->app_id = DEMO_MIN_APP_ID;
	out->version = DEMO_MIN_APP_VER;
	out->role_flags = (uint8_t)UCP_APP_ROLE_REQ_RSP;
	out->init = NULL;
	out->on_rx = demo_min_on_rx;
	out->on_tick = NULL;
	out->on_link_event = NULL;
}
