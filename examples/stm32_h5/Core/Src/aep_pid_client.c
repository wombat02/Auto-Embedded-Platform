/*
 * aep_pid_client.c — board UDS client: session / tester present / RDBI → ext sigs.
 *
 * Peer is host tools/uds_pid_ecu.py on 0x7E0/0x7E8.
 */
#include "aep_pid_client.h"
#include "aep_app.h"


#include "aep_demo_pt.h"
#include "diag_link.h"
#include "log.h"

#ifndef UDS_SYS
#define UDS_SYS 0
#endif
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#include "iso14229.h"

#include <string.h>

#define AEP_PID_RDBI_PERIOD_MS 200u
#define AEP_PID_TP_PERIOD_MS   2000u

static aep_pid_client_ctx_t *s_active_ctx;

static void *unpack_memcpy(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}

static int client_fn(UDSClient_t *client, UDSEvent_t evt, void *ev_data)
{
	aep_pid_client_ctx_t *ctx = s_active_ctx;
	uint8_t raw[2];
	UDSRDBIVar_t var;
	uint16_t did;
	uint16_t sig_id;
	uint32_t now;

	(void)ev_data;
	if ((ctx == NULL) || (client == NULL))
	{
		return UDS_OK;
	}

	if (evt == UDS_EVT_ResponseReceived)
	{
		uint8_t op = ctx->last_op;

		ctx->pending = 0u;
		ctx->last_op = 0u;
		if (op == 1u)
		{
			ctx->phase = 1u;
			return UDS_OK;
		}
		if (op == 3u)
		{
			did = (ctx->did_idx == 0u) ? (uint16_t)AEP_DID_BOOST : (uint16_t)AEP_DID_FUEL_PRESS;
			sig_id =
				(ctx->did_idx == 0u) ? (uint16_t)AEP_SIG_EXT_BOOST : (uint16_t)AEP_SIG_EXT_FUEL_PRESS;
			var.did = did;
			var.len = 2u;
			var.data = raw;
			var.UnpackFn = unpack_memcpy;
			if (UDSUnpackRDBIResponse(client, &var, 1u) == UDS_OK)
			{
				uint16_t val = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);

				now = (ctx->cfg.now_ms != NULL) ? ctx->cfg.now_ms() : 0u;
				(void)sig_table_write(ctx->cfg.table, sig_id, val, 1u, now);
				ctx->did_idx = (uint8_t)((ctx->did_idx + 1u) & 1u);
			}
		}
	}
	else if (evt == UDS_EVT_Err)
	{
		ctx->pending = 0u;
		ctx->last_op = 0u;
	}
	return UDS_OK;
}

static int pid_start(void *ctx_v, const void *cfg, size_t cfg_len)
{
	aep_pid_client_ctx_t *ctx = (aep_pid_client_ctx_t *)ctx_v;
	diag_link_cfg_t lcfg;
	diag_link_t *link;
	UDSClient_t *client;

	if ((ctx == NULL) || (cfg == NULL) || (cfg_len < sizeof(aep_pid_client_cfg_t)))
	{
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfg = *(const aep_pid_client_cfg_t *)cfg;
	if ((ctx->cfg.core == NULL) || (ctx->cfg.table == NULL) || (ctx->cfg.now_ms == NULL))
	{
		return -1;
	}

	link = diag_link_instance();
	memset(&lcfg, 0, sizeof(lcfg));
	lcfg.core = ctx->cfg.core;
	lcfg.bus = ctx->cfg.bus;
	lcfg.phys_req_id = AEP_PT_UDS_REQ_ID;
	lcfg.phys_resp_id = AEP_PT_UDS_RESP_ID;
	lcfg.func_req_id = AEP_PT_UDS_FUNC_ID;
	lcfg.now_ms = ctx->cfg.now_ms;
	if (!diag_link_init(link, &lcfg) || !diag_link_subscribe_rx(link))
	{
		return -1;
	}
	client = (UDSClient_t *)diag_link_client(link);
	if (client == NULL)
	{
		diag_link_deinit(link);
		return -1;
	}
	client->fn = client_fn;
	client->fn_data = ctx;
	s_active_ctx = ctx;
	ctx->started = 1u;
	ctx->phase = 0u;
	ctx->next_req_ms = ctx->cfg.now_ms();
	ctx->next_tp_ms = ctx->cfg.now_ms() + AEP_PID_TP_PERIOD_MS;
	logi("PID client on bus %u (0x7E0/0x7E8)", (unsigned)ctx->cfg.bus);
	return 0;
}

static void pid_stop(void *ctx_v)
{
	aep_pid_client_ctx_t *ctx = (aep_pid_client_ctx_t *)ctx_v;

	if ((ctx == NULL) || (ctx->started == 0u))
	{
		return;
	}
	diag_link_deinit(diag_link_instance());
	if (s_active_ctx == ctx)
	{
		s_active_ctx = NULL;
	}
	ctx->started = 0u;
}

static void pid_poll(void *ctx_v, uint32_t now_ms)
{
	aep_pid_client_ctx_t *ctx = (aep_pid_client_ctx_t *)ctx_v;
	diag_link_t *link;
	uint16_t did;

	if ((ctx == NULL) || (ctx->started == 0u))
	{
		return;
	}
	s_active_ctx = ctx;
	link = diag_link_instance();
	(void)diag_link_poll(link);

	if (ctx->pending != 0u)
	{
		return;
	}

	if (ctx->phase == 0u)
	{
		if ((int32_t)(now_ms - ctx->next_req_ms) >= 0)
		{
			if (diag_link_session_default(link))
			{
				ctx->pending = 1u;
				ctx->last_op = 1u;
			}
			ctx->next_req_ms = now_ms + AEP_PID_RDBI_PERIOD_MS;
		}
		return;
	}

	if ((int32_t)(now_ms - ctx->next_tp_ms) >= 0)
	{
		if (diag_link_tester_present(link))
		{
			ctx->pending = 1u;
			ctx->last_op = 2u;
			ctx->next_tp_ms = now_ms + AEP_PID_TP_PERIOD_MS;
			return;
		}
		ctx->next_tp_ms = now_ms + AEP_PID_TP_PERIOD_MS;
	}

	if ((int32_t)(now_ms - ctx->next_req_ms) >= 0)
	{
		did = (ctx->did_idx == 0u) ? (uint16_t)AEP_DID_BOOST : (uint16_t)AEP_DID_FUEL_PRESS;
		if (diag_link_read_did(link, &did, 1u))
		{
			ctx->pending = 1u;
			ctx->last_op = 3u;
		}
		ctx->next_req_ms = now_ms + AEP_PID_RDBI_PERIOD_MS;
	}
}

static const aep_comp_ops_t s_ops = {
	.start = pid_start,
	.stop = pid_stop,
	.poll = pid_poll,
	.on_can_rx = NULL,
};

static const aep_comp_desc_t s_desc = {
	.id = AEP_COMP_ID_PID_CLIENT,
	.kind = AEP_COMP_PID,
	.ops = &s_ops,
	.ctx_size = sizeof(aep_pid_client_ctx_t),
	.max_cfg_len = sizeof(aep_bus_nvm_cfg_t),
	.name = "pid_client",
};

const aep_comp_desc_t *aep_pid_client_desc(void)
{
	return &s_desc;
}

