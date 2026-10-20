/*
 * Unit tests for ucp_xfer pull + push (byte-pipe via port ops).
 */
#include "ucp_parser.h"
#include "ucp_port.h"
#include "ucp_xfer.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static uint32_t g_now_ms;

static uint8_t g_rx_blob[UCP_XFER_MAX_SDU];
static uint16_t g_rx_len;
static uint16_t g_rx_token;
static int g_rx_hits;

static uint8_t g_cap_frames[32][72];
static uint16_t g_cap_lens[32];
static int g_cap_n;
static int g_loopback;

static void expect_true(const char *name, int cond)
{
	if (!cond)
	{
		printf("FAIL: %s\n", name);
		g_failures++;
	}
	else
	{
		printf("ok: %s\n", name);
	}
}

typedef struct
{
	ucp_parser_ctx_t parser;
	ucp_port_ops_t ops;
} xfer_peer_t;

static xfer_peer_t g_peer;

static uint32_t peer_time(void *user)
{
	(void)user;
	return g_now_ms;
}

static bool peer_send_frame(const uint8_t *frame, uint16_t len, void *user)
{
	xfer_peer_t *p = (xfer_peer_t *)user;
	ucp_pkt_t pkt;
	uint16_t i;

	if ((frame == NULL) || (len < 5u))
	{
		return false;
	}

	if (!g_loopback)
	{
		if (g_cap_n < 32)
		{
			memcpy(g_cap_frames[g_cap_n], frame, len);
			g_cap_lens[g_cap_n] = len;
			g_cap_n++;
		}
		return true;
	}

	if (p == NULL)
	{
		return false;
	}
	ucp_parser_ctx_init(&p->parser);
	for (i = 0u; i < len; i++)
	{
		if (ucp_parse_byte_ctx(&p->parser, frame[i], &pkt))
		{
			g_ucp_xfer_app_vtable.on_rx(&pkt);
		}
	}
	return true;
}

static void on_push_rx(uint16_t token, const uint8_t *data, uint16_t len, void *user)
{
	(void)user;
	g_rx_token = token;
	g_rx_len = len;
	g_rx_hits++;
	if ((data != NULL) && (len > 0u) && (len <= UCP_XFER_MAX_SDU))
	{
		memcpy(g_rx_blob, data, len);
	}
}

static void setup(int loopback)
{
	g_now_ms = 1000u;
	g_rx_hits = 0;
	g_rx_len = 0;
	g_rx_token = 0;
	g_cap_n = 0;
	g_loopback = loopback;
	memset(g_rx_blob, 0, sizeof(g_rx_blob));
	memset(&g_peer, 0, sizeof(g_peer));
	g_peer.ops.get_time_ms = peer_time;
	g_peer.ops.send_frame = peer_send_frame;
	g_peer.ops.user = &g_peer;
	ucp_port_set_ops(&g_peer.ops);
	ucp_xfer_init();
	ucp_xfer_set_push_rx_cb(on_push_rx, NULL);
}

static void inject(uint8_t pkt_id, const uint8_t *payload, uint8_t len)
{
	ucp_pkt_t pkt;
	pkt.app_id = (uint8_t)UCP_APP_ID_XFER;
	pkt.pkt_id = pkt_id;
	pkt.payload_len = len;
	if ((payload != NULL) && (len > 0u))
	{
		memcpy(pkt.payload, payload, len);
	}
	g_ucp_xfer_app_vtable.on_rx(&pkt);
}

static void test_pull_roundtrip(void)
{
	uint16_t token = 0;
	uint8_t src[200];
	uint8_t out[200];
	uint16_t out_len = 0;
	uint16_t i;
	uint8_t req[3];
	uint8_t chunks;
	uint8_t cap;

	setup(0);
	for (i = 0; i < sizeof(src); i++)
	{
		src[i] = (uint8_t)(i ^ 0x5Au);
	}
	expect_true("offer", ucp_xfer_offer(&token, src, (uint16_t)sizeof(src)));
	req[0] = (uint8_t)(token & 0xFFu);
	req[1] = (uint8_t)((token >> 8) & 0xFFu);
	inject(UCP_XFER_PKT_INFO_REQ, req, 2u);
	expect_true("info rsp sent", g_cap_n >= 1);
	{
		uint8_t *f = g_cap_frames[0];
		uint8_t plen = f[3];
		expect_true("info pkt", f[2] == UCP_XFER_PKT_INFO_RSP);
		expect_true("info len", plen >= 7u);
		cap = f[4 + 4];
		chunks = f[4 + 5];
		expect_true("chunk cap", cap == ucp_xfer_chunk_data_cap());
		expect_true("chunks", chunks >= 1u);
	}
	g_cap_n = 0;
	for (i = 0; i < chunks; i++)
	{
		req[2] = (uint8_t)i;
		inject(UCP_XFER_PKT_CHUNK_REQ, req, 3u);
		expect_true("chunk rsp", g_cap_n > 0);
		{
			uint8_t *f = g_cap_frames[g_cap_n - 1];
			uint8_t plen = f[3];
			uint8_t dlen = (uint8_t)(plen - 4u);
			memcpy(&out[out_len], &f[4 + 4], dlen);
			out_len = (uint16_t)(out_len + dlen);
		}
	}
	expect_true("pull len", out_len == (uint16_t)sizeof(src));
	expect_true("pull data", memcmp(out, src, sizeof(src)) == 0);
}

static void test_push_roundtrip(void)
{
	uint16_t token = 0;
	uint8_t src[300];
	uint16_t i;

	setup(1);
	for (i = 0; i < sizeof(src); i++)
	{
		src[i] = (uint8_t)(i & 0xFFu);
	}
	expect_true("push begin", ucp_xfer_push_begin(&token, src, (uint16_t)sizeof(src)));
	for (i = 0; i < 64u; i++)
	{
		g_now_ms += 10u;
		ucp_xfer_on_tick(g_now_ms, true);
		if (ucp_xfer_push_done(token))
		{
			break;
		}
	}
	expect_true("push done", ucp_xfer_push_done(token));
	expect_true("push rx hit", g_rx_hits == 1);
	expect_true("push rx token", g_rx_token == token);
	expect_true("push rx len", g_rx_len == (uint16_t)sizeof(src));
	expect_true("push rx data", memcmp(g_rx_blob, src, sizeof(src)) == 0);
}

static void test_push_single_segment(void)
{
	uint16_t token = 0;
	uint8_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	uint16_t i;

	setup(1);
	expect_true("push tiny begin", ucp_xfer_push_begin(&token, src, 8u));
	for (i = 0; i < 8u; i++)
	{
		g_now_ms += 10u;
		ucp_xfer_on_tick(g_now_ms, true);
		if (ucp_xfer_push_done(token))
		{
			break;
		}
	}
	expect_true("push tiny done", ucp_xfer_push_done(token));
	expect_true("push tiny data", g_rx_len == 8u && memcmp(g_rx_blob, src, 8) == 0);
}

static void test_link_clear(void)
{
	uint16_t token = 0;
	uint8_t src[16];

	setup(0);
	memset(src, 0xAB, sizeof(src));
	expect_true("offer before clear", ucp_xfer_offer(&token, src, 16u));
	ucp_xfer_on_link_event(UCP_APP_LINK_EVENT_TIMEOUT);
	expect_true("offer cleared", !ucp_xfer_offer_active());
}

int main(void)
{
	test_pull_roundtrip();
	test_push_roundtrip();
	test_push_single_segment();
	test_link_clear();
	if (g_failures != 0)
	{
		printf("%d failure(s)\n", g_failures);
		return 1;
	}
	printf("all ucp_xfer tests passed\n");
	return 0;
}
