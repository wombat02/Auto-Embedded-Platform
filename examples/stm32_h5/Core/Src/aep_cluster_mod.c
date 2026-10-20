/*
 * aep_cluster_mod.c — instrument-cluster dummy: sig table → CAN1 0x301.
 *
 * Scaling differs from PT raw (shows module remap). Host: tools/cluster_sniff.py.
 */
#include "aep_cluster_mod.h"
#include "aep_app.h"


#include "aep_demo_pt.h"
#include "sig_catalog.h"
#include "log.h"

#include <string.h>

static aep_cluster_mod_ctx_t *s_fill_ctx;

static void cluster_fill(vcom_can_frame_t *fr, void *user)
{
	aep_cluster_mod_ctx_t *ctx = (aep_cluster_mod_ctx_t *)user;
	uint32_t rpm_raw = 0u;
	uint32_t cool_raw = 0u;
	uint32_t tq_raw = 0u;
	uint8_t v = 0u;
	int32_t rpm_eng = 0;
	int32_t cool_eng = 0;
	int32_t tq_eng = 0;

	(void)user;
	if ((ctx == NULL) || (fr == NULL) || (ctx->cfg.table == NULL))
	{
		return;
	}
	memset(fr->data, 0, sizeof(fr->data));
	fr->id = AEP_CLUSTER_FRAME_ID;
	fr->dlc = 5u;
	fr->ide = 0u;

	(void)sig_table_read(ctx->cfg.table, SIG_ENGINE_RPM, &rpm_raw, &v, NULL);
	(void)sig_table_read(ctx->cfg.table, SIG_COOLANT_TEMP, &cool_raw, &v, NULL);
	(void)sig_table_read(ctx->cfg.table, SIG_ENGINE_TORQUE, &tq_raw, &v, NULL);

	(void)sig_decode_eng(SIG_ENGINE_RPM, SIG_T_U16, rpm_raw, &rpm_eng);
	(void)sig_decode_eng(SIG_COOLANT_TEMP, SIG_T_I16, cool_raw, &cool_eng);
	(void)sig_decode_eng(SIG_ENGINE_TORQUE, SIG_T_I16, tq_raw, &tq_eng);

	/* Cluster encoding: RPM as integer u16, coolant as i8 °C, torque as i16 N·m. */
	if (rpm_eng < 0)
	{
		rpm_eng = 0;
	}
	if (rpm_eng > 65535)
	{
		rpm_eng = 65535;
	}
	fr->data[0] = (uint8_t)(((uint16_t)rpm_eng >> 8) & 0xFFu);
	fr->data[1] = (uint8_t)((uint16_t)rpm_eng & 0xFFu);
	if (cool_eng < -128)
	{
		cool_eng = -128;
	}
	if (cool_eng > 127)
	{
		cool_eng = 127;
	}
	fr->data[2] = (uint8_t)(int8_t)cool_eng;
	{
		int16_t tq = (int16_t)tq_eng;

		fr->data[3] = (uint8_t)(((uint16_t)tq >> 8) & 0xFFu);
		fr->data[4] = (uint8_t)((uint16_t)tq & 0xFFu);
	}
}

static int cluster_start(void *ctx_v, const void *cfg, size_t cfg_len)
{
	aep_cluster_mod_ctx_t *ctx = (aep_cluster_mod_ctx_t *)ctx_v;
	vcom_can_frame_t tmpl;

	if ((ctx == NULL) || (cfg == NULL) || (cfg_len < sizeof(aep_cluster_mod_cfg_t)))
	{
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfg = *(const aep_cluster_mod_cfg_t *)cfg;
	ctx->tx_slot = -1;
	if ((ctx->cfg.core == NULL) || (ctx->cfg.table == NULL))
	{
		return -1;
	}

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.id = AEP_CLUSTER_FRAME_ID;
	tmpl.dlc = 5u;
	s_fill_ctx = ctx;
	ctx->tx_slot = vcom_tx_periodic(ctx->cfg.core, ctx->cfg.bus, 50u, &tmpl, cluster_fill, ctx);
	if (ctx->tx_slot < 0)
	{
		return -1;
	}
	ctx->started = 1u;
	logi("cluster mod TX 0x301 on bus %u", (unsigned)ctx->cfg.bus);
	return 0;
}

static void cluster_stop(void *ctx_v)
{
	aep_cluster_mod_ctx_t *ctx = (aep_cluster_mod_ctx_t *)ctx_v;

	if ((ctx == NULL) || (ctx->started == 0u))
	{
		return;
	}
	if (ctx->tx_slot >= 0)
	{
		vcom_tx_cancel(ctx->cfg.core, ctx->tx_slot);
		ctx->tx_slot = -1;
	}
	if (s_fill_ctx == ctx)
	{
		s_fill_ctx = NULL;
	}
	ctx->started = 0u;
}

static void cluster_poll(void *ctx_v, uint32_t now_ms)
{
	(void)ctx_v;
	(void)now_ms;
	/* TX driven by vcom_tx_periodic fill. */
}

static const aep_comp_ops_t s_ops = {
	.start = cluster_start,
	.stop = cluster_stop,
	.poll = cluster_poll,
	.on_can_rx = NULL,
};

static const aep_comp_desc_t s_desc = {
	.id = AEP_COMP_ID_CLUSTER,
	.kind = AEP_COMP_MODULE,
	.ops = &s_ops,
	.ctx_size = sizeof(aep_cluster_mod_ctx_t),
	.max_cfg_len = sizeof(aep_bus_nvm_cfg_t),
	.name = "cluster_mod",
};

const aep_comp_desc_t *aep_cluster_mod_desc(void)
{
	return &s_desc;
}

