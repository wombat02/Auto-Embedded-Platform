/*
 * File: ucp_parser.c
 * Brief: Streaming UCP deframer with per-link context support.
 */

#include "ucp_parser.h"

#include <string.h>

static ucp_parser_ctx_t s_legacy_ctx;
static ucp_post_rx_handler_t s_post_rx_handler;
static ucp_parser_stats_t s_parser_stats = {0};
static bool s_legacy_inited = false;

void ucp_parser_ctx_init(ucp_parser_ctx_t *ctx)
{
	if (ctx == NULL)
	{
		return;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->state = UCP_RX_WAIT_SOF;
}

void ucp_parser_ctx_reset(ucp_parser_ctx_t *ctx)
{
	ucp_parser_ctx_init(ctx);
}

bool ucp_parse_byte_ctx(ucp_parser_ctx_t *ctx, uint8_t byte, ucp_pkt_t *out_pkt)
{
	if ((ctx == NULL) || (out_pkt == NULL))
	{
		return false;
	}

	switch (ctx->state)
	{
	case UCP_RX_WAIT_SOF:
		if (byte == UCP_PKT_SOF)
		{
			ctx->checksum = 0u;
			ctx->state = UCP_RX_WAIT_APP_ID;
		}
		break;

	case UCP_RX_WAIT_APP_ID:
		ctx->app_id = byte;
		ctx->checksum ^= byte;
		ctx->state = UCP_RX_WAIT_PKT_ID;
		break;

	case UCP_RX_WAIT_PKT_ID:
		ctx->pkt_id = byte;
		ctx->checksum ^= byte;
		ctx->state = UCP_RX_WAIT_LEN;
		break;

	case UCP_RX_WAIT_LEN:
		ctx->len = byte;
		ctx->checksum ^= byte;
		ctx->index = 0u;
		if (ctx->len > sizeof(ctx->payload))
		{
			s_parser_stats.oversize_length++;
			s_parser_stats.last_oversize_len = ctx->len;
			ctx->state = UCP_RX_WAIT_SOF;
		}
		else if (ctx->len == 0u)
		{
			ctx->state = UCP_RX_WAIT_CHECKSUM;
		}
		else
		{
			ctx->state = UCP_RX_WAIT_PAYLOAD;
		}
		break;

	case UCP_RX_WAIT_PAYLOAD:
		ctx->payload[ctx->index++] = byte;
		ctx->checksum ^= byte;
		if (ctx->index >= ctx->len)
		{
			ctx->state = UCP_RX_WAIT_CHECKSUM;
		}
		break;

	case UCP_RX_WAIT_CHECKSUM:
		if (byte != ctx->checksum)
		{
			s_parser_stats.checksum_failures++;
			s_parser_stats.last_chk_fail_pkt_id = ctx->pkt_id;
			s_parser_stats.last_chk_fail_len = ctx->len;
			s_parser_stats.last_chk_fail_expected = ctx->checksum;
			s_parser_stats.last_chk_fail_got = byte;
			for (uint8_t i = 0u; i < 4u; i++)
			{
				s_parser_stats.last_chk_fail_payload_head[i] =
					(i < ctx->len) ? ctx->payload[i] : 0u;
			}
			ctx->state = UCP_RX_WAIT_SOF;
			break;
		}

		out_pkt->app_id = ctx->app_id;
		out_pkt->pkt_id = ctx->pkt_id;
		out_pkt->payload_len = ctx->len;
		if (ctx->len > 0u)
		{
			memcpy(out_pkt->payload, ctx->payload, ctx->len);
		}
		ctx->state = UCP_RX_WAIT_SOF;

		if (s_post_rx_handler != NULL)
		{
			s_post_rx_handler(out_pkt);
		}
		return true;
	}

	return false;
}

void ucp_parser_get_stats(ucp_parser_stats_t *out_stats)
{
	if (out_stats != NULL)
	{
		*out_stats = s_parser_stats;
	}
}

void ucp_parser_reset_stats(void)
{
	memset(&s_parser_stats, 0, sizeof(s_parser_stats));
}

void ucp_parser_reset(void)
{
	ucp_parser_ctx_reset(&s_legacy_ctx);
}

bool ucp_parse_byte(const uint8_t byte, ucp_pkt_t *out_pkt)
{
	if (!s_legacy_inited)
	{
		ucp_parser_ctx_init(&s_legacy_ctx);
		s_legacy_inited = true;
	}
	return ucp_parse_byte_ctx(&s_legacy_ctx, byte, out_pkt);
}

void ucp_set_post_rx_handler(ucp_post_rx_handler_t handler)
{
	s_post_rx_handler = handler;
}
