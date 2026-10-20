/*
 * aep_dash_mod.c — Tier B dash: steering / stalks on CAN1 → 0xF010+ signals.
 */
#include "aep_dash_mod.h"
#include "aep_app.h"

#include "log.h"
#include "sig_catalog.h"

#include <string.h>

static void dash_on_can_rx(void *ctx_v, uint8_t bus, const vcom_can_frame_t *fr)
{
	aep_dash_mod_ctx_t *ctx = (aep_dash_mod_ctx_t *)ctx_v;
	uint32_t now;

	if ((ctx == NULL) || (fr == NULL) || (ctx->started == 0u) || (ctx->cfg.table == NULL))
	{
		return;
	}
	if (bus != ctx->cfg.bus)
	{
		return;
	}
	now = (ctx->cfg.core != NULL && ctx->cfg.core->time != NULL &&
		   ctx->cfg.core->time->now_ms != NULL)
			  ? ctx->cfg.core->time->now_ms()
			  : 0u;

	if ((fr->id == AEP_DASH_FRAME_STEER) && (fr->dlc >= 2u))
	{
		int16_t deg = (int16_t)(((uint16_t)fr->data[0] << 8) | fr->data[1]);

		(void)sig_table_write(ctx->cfg.table, AEP_SIG_DASH_STEER_DEG, (uint32_t)(uint16_t)deg, 1u,
							  now);
	}
	else if ((fr->id == AEP_DASH_FRAME_STALKS) && (fr->dlc >= 1u))
	{
		uint8_t f = fr->data[0];

		(void)sig_table_write(ctx->cfg.table, AEP_SIG_DASH_TURN_L, (f & 1u) ? 1u : 0u, 1u, now);
		(void)sig_table_write(ctx->cfg.table, AEP_SIG_DASH_TURN_R, (f & 2u) ? 1u : 0u, 1u, now);
		(void)sig_table_write(ctx->cfg.table, AEP_SIG_DASH_WIPER, (f & 4u) ? 1u : 0u, 1u, now);
		(void)sig_table_write(ctx->cfg.table, AEP_SIG_DASH_HIGH_BEAM, (f & 8u) ? 1u : 0u, 1u, now);
	}
}

static void demux_cb(uint8_t bus, const vcom_can_frame_t *fr, void *user)
{
	dash_on_can_rx(user, bus, fr);
}

static int dash_start(void *ctx_v, const void *cfg, size_t cfg_len)
{
	aep_dash_mod_ctx_t *ctx = (aep_dash_mod_ctx_t *)ctx_v;

	if ((ctx == NULL) || (cfg == NULL) || (cfg_len < sizeof(aep_dash_mod_cfg_t)))
	{
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfg = *(const aep_dash_mod_cfg_t *)cfg;
	if ((ctx->cfg.core == NULL) || (ctx->cfg.table == NULL) || (ctx->cfg.demux == NULL))
	{
		return -1;
	}
	if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_DASH_FRAME_STEER, 0x7FFu, demux_cb,
							   ctx) < 0)
	{
		return -1;
	}
	if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_DASH_FRAME_STALKS, 0x7FFu, demux_cb,
							   ctx) < 0)
	{
		vcom_rx_demux_unregister_user(ctx->cfg.demux, ctx);
		return -1;
	}
	ctx->started = 1u;
	logi("dash mod on bus %u (0x401/0x402)", (unsigned)ctx->cfg.bus);
	return 0;
}

static void dash_stop(void *ctx_v)
{
	aep_dash_mod_ctx_t *ctx = (aep_dash_mod_ctx_t *)ctx_v;

	if ((ctx == NULL) || (ctx->started == 0u))
	{
		return;
	}
	if (ctx->cfg.demux != NULL)
	{
		vcom_rx_demux_unregister_user(ctx->cfg.demux, ctx);
	}
	ctx->started = 0u;
}

static void dash_poll(void *ctx_v, uint32_t now_ms)
{
	(void)ctx_v;
	(void)now_ms;
}

static const aep_comp_ops_t s_ops = {
	.start = dash_start,
	.stop = dash_stop,
	.poll = dash_poll,
	.on_can_rx = dash_on_can_rx,
};

static const aep_comp_desc_t s_desc = {
	.id = AEP_COMP_ID_DASH,
	.kind = AEP_COMP_MODULE,
	.ops = &s_ops,
	.ctx_size = sizeof(aep_dash_mod_ctx_t),
	.max_cfg_len = sizeof(aep_bus_nvm_cfg_t),
	.name = "dash_mod",
};

const aep_comp_desc_t *aep_dash_mod_desc(void)
{
	return &s_desc;
}

