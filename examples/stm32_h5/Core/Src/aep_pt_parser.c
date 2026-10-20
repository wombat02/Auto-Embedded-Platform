/*
 * aep_pt_parser.c — mock PT-CAN decode on CAN0 → Tier A signals.
 * Profile A/B selected via cfg; RX via vcom_rx_demux + on_can_rx.
 */
#include "aep_pt_parser.h"
#include "aep_app.h"

#include "log.h"
#include "sig_catalog.h"

#include <string.h>

static void decode_a(aep_pt_parser_ctx_t *ctx, const vcom_can_frame_t *fr, uint32_t now)
{
	if ((fr->id == AEP_PT_FRAME_A_POWERTRAIN) && (fr->dlc >= 3u))
	{
		uint16_t rpm_raw = (uint16_t)(((uint16_t)fr->data[0] << 8) | fr->data[1]);
		uint16_t cool_raw = fr->data[2];

		(void)sig_table_write(ctx->cfg.table, SIG_ENGINE_RPM, rpm_raw, 1u, now);
		(void)sig_table_write(ctx->cfg.table, SIG_COOLANT_TEMP, cool_raw, 1u, now);
	}
	else if ((fr->id == AEP_PT_FRAME_A_TORQUE) && (fr->dlc >= 2u))
	{
		int16_t tq = (int16_t)(((uint16_t)fr->data[0] << 8) | fr->data[1]);

		(void)sig_table_write(ctx->cfg.table, SIG_ENGINE_TORQUE, (uint32_t)(uint16_t)tq, 1u, now);
	}
}

static void decode_b(aep_pt_parser_ctx_t *ctx, const vcom_can_frame_t *fr, uint32_t now)
{
	/* 0x211: rpm u16 LE, load u16 LE (raw=%/10), coolant u8 */
	if ((fr->id == AEP_PT_FRAME_B_POWERTRAIN) && (fr->dlc >= 5u))
	{
		uint16_t rpm_eng = (uint16_t)fr->data[0] | ((uint16_t)fr->data[1] << 8);
		uint16_t load_raw = (uint16_t)fr->data[2] | ((uint16_t)fr->data[3] << 8);
		uint16_t cool_raw = fr->data[4];
		uint16_t rpm_raw = (uint16_t)(rpm_eng * 4u);

		(void)sig_table_write(ctx->cfg.table, SIG_ENGINE_RPM, rpm_raw, 1u, now);
		(void)sig_table_write(ctx->cfg.table, SIG_ENGINE_LOAD, load_raw, 1u, now);
		(void)sig_table_write(ctx->cfg.table, SIG_COOLANT_TEMP, cool_raw, 1u, now);
	}
	/* 0x212: vss u16 LE (raw=kmh*100), gear i8, throttle u16 LE */
	else if ((fr->id == AEP_PT_FRAME_B_VEHICLE) && (fr->dlc >= 5u))
	{
		uint16_t vss = (uint16_t)fr->data[0] | ((uint16_t)fr->data[1] << 8);
		int8_t gear = (int8_t)fr->data[2];
		uint16_t thr = (uint16_t)fr->data[3] | ((uint16_t)fr->data[4] << 8);

		(void)sig_table_write(ctx->cfg.table, SIG_VEHICLE_SPEED, vss, 1u, now);
		(void)sig_table_write(ctx->cfg.table, SIG_GEAR, (uint32_t)(uint8_t)gear, 1u, now);
		(void)sig_table_write(ctx->cfg.table, SIG_THROTTLE_POS, thr, 1u, now);
	}
}

static void parser_on_can_rx(void *ctx_v, uint8_t bus, const vcom_can_frame_t *fr)
{
	aep_pt_parser_ctx_t *ctx = (aep_pt_parser_ctx_t *)ctx_v;
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

	if (ctx->cfg.profile == AEP_PT_PROFILE_B)
	{
		decode_b(ctx, fr, now);
	}
	else
	{
		decode_a(ctx, fr, now);
	}
}

static void demux_cb(uint8_t bus, const vcom_can_frame_t *fr, void *user)
{
	parser_on_can_rx(user, bus, fr);
}

static int parser_start(void *ctx_v, const void *cfg, size_t cfg_len)
{
	aep_pt_parser_ctx_t *ctx = (aep_pt_parser_ctx_t *)ctx_v;

	if ((ctx == NULL) || (cfg == NULL) || (cfg_len < sizeof(aep_pt_parser_cfg_t)))
	{
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfg = *(const aep_pt_parser_cfg_t *)cfg;
	if ((ctx->cfg.core == NULL) || (ctx->cfg.table == NULL) || (ctx->cfg.demux == NULL))
	{
		return -1;
	}

	if (ctx->cfg.profile == AEP_PT_PROFILE_B)
	{
		if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_PT_FRAME_B_POWERTRAIN,
								   0x7FFu, demux_cb, ctx) < 0)
		{
			return -1;
		}
		if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_PT_FRAME_B_VEHICLE, 0x7FFu,
								   demux_cb, ctx) < 0)
		{
			vcom_rx_demux_unregister_user(ctx->cfg.demux, ctx);
			return -1;
		}
		logi("PT parser B on bus %u (0x211/0x212)", (unsigned)ctx->cfg.bus);
	}
	else
	{
		if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_PT_FRAME_A_POWERTRAIN,
								   0x7FFu, demux_cb, ctx) < 0)
		{
			return -1;
		}
		if (vcom_rx_demux_register(ctx->cfg.demux, ctx->cfg.bus, AEP_PT_FRAME_A_TORQUE, 0x7FFu,
								   demux_cb, ctx) < 0)
		{
			vcom_rx_demux_unregister_user(ctx->cfg.demux, ctx);
			return -1;
		}
		logi("PT parser A on bus %u (0x201/0x202)", (unsigned)ctx->cfg.bus);
	}
	ctx->started = 1u;
	return 0;
}

static void parser_stop(void *ctx_v)
{
	aep_pt_parser_ctx_t *ctx = (aep_pt_parser_ctx_t *)ctx_v;

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

static void parser_poll(void *ctx_v, uint32_t now_ms)
{
	(void)ctx_v;
	(void)now_ms;
}

static const aep_comp_ops_t s_ops = {
	.start = parser_start,
	.stop = parser_stop,
	.poll = parser_poll,
	.on_can_rx = parser_on_can_rx,
};

static const aep_comp_desc_t s_desc = {
	.id = AEP_COMP_ID_PT_PARSER,
	.kind = AEP_COMP_PARSER,
	.ops = &s_ops,
	.ctx_size = sizeof(aep_pt_parser_ctx_t),
	.max_cfg_len = sizeof(aep_pt_nvm_cfg_t),
	.name = "pt_parser",
};

const aep_comp_desc_t *aep_pt_parser_desc(void)
{
	return &s_desc;
}

