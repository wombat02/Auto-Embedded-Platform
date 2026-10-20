/*
 * Host byte-pipe: hello + capability with demo telemetry roles.
 * Producer: TX|REQ_RSP, Consumer: RX|REQ_RSP => producer negotiates TX|REQ_RSP.
 */
#include "ucp_parser.h"
#include "ucp_port.h"
#include "ucp_session.h"

#include <stdio.h>
#include <string.h>

#define APP_ID_DEMO 0x20u
#define APP_VER_DEMO 0x0100u

static uint32_t g_now_ms;
static int g_failures;

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

typedef struct peer peer_t;

struct peer {
	ucp_session_t session;
	ucp_parser_ctx_t parser;
	ucp_port_ops_t ops;
	ucp_hw_id_t hw_id;
	uint8_t role_flags;
	peer_t *other;
};

static uint32_t peer_time(void *user)
{
	(void)user;
	return g_now_ms;
}

static uint8_t peer_descriptors(ucp_app_version_entry_t *out, uint8_t max, void *user)
{
	peer_t *p = (peer_t *)user;

	if ((out == NULL) || (max == 0u) || (p == NULL))
	{
		return 0u;
	}
	out[0].app_id = APP_ID_DEMO;
	out[0].version = APP_VER_DEMO;
	out[0].role_flags = p->role_flags;
	return 1u;
}

static ucp_hw_id_t peer_hw(void *user)
{
	peer_t *p = (peer_t *)user;
	return (p != NULL) ? p->hw_id : UCP_HW_ID_UNKNOWN;
}

static bool peer_send_frame(const uint8_t *frame, uint16_t len, void *user)
{
	peer_t *me = (peer_t *)user;
	peer_t *other;
	const ucp_port_ops_t *saved;
	uint16_t i;

	if ((me == NULL) || (me->other == NULL) || (frame == NULL))
	{
		return false;
	}
	other = me->other;
	saved = ucp_port_get_ops();
	ucp_port_set_ops(&other->ops);

	for (i = 0u; i < len; i++)
	{
		ucp_pkt_t pkt;

		if (ucp_parse_byte_ctx(&other->parser, frame[i], &pkt))
		{
			ucp_session_on_packet(&other->session, &pkt);
		}
	}

	ucp_port_set_ops(saved);
	return true;
}

static void peer_init(peer_t *p, peer_t *other, ucp_hw_id_t hw, uint8_t roles)
{
	memset(p, 0, sizeof(*p));
	p->other = other;
	p->hw_id = hw;
	p->role_flags = roles;
	p->ops.get_time_ms = peer_time;
	p->ops.send_frame = peer_send_frame;
	p->ops.get_hw_id = peer_hw;
	p->ops.get_app_descriptors = peer_descriptors;
	p->ops.user = p;
	ucp_parser_ctx_init(&p->parser);
	ucp_port_set_ops(&p->ops);
	ucp_session_init(&p->session);
}

static uint8_t negotiated_roles_for(const ucp_session_t *s, uint8_t app_id)
{
	uint8_t i;

	for (i = 0u; i < s->negotiated_app_count; i++)
	{
		if (s->negotiated_apps[i].app_id == app_id)
		{
			return s->negotiated_apps[i].role_flags;
		}
	}
	return 0u;
}

static void test_roles_unit(void)
{
	uint8_t neg = 0u;
	bool warn = false;
	const uint8_t prod = (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP);
	const uint8_t cons = (uint8_t)(UCP_APP_ROLE_RX | UCP_APP_ROLE_REQ_RSP);

	expect_true("negotiate ok",
				ucp_app_roles_negotiate(prod, cons, &neg, &warn));
	expect_true("TX|REQ_RSP",
				neg == (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP));
	expect_true("no warn on complementary+req", !warn);

	neg = 0u;
	warn = true;
	expect_true("consumer view",
				ucp_app_roles_negotiate(cons, prod, &neg, &warn));
	expect_true("RX|REQ_RSP",
				neg == (uint8_t)(UCP_APP_ROLE_RX | UCP_APP_ROLE_REQ_RSP));
	expect_true("no warn consumer", !warn);

	/* Same-direction only -> warn */
	neg = 0u;
	warn = false;
	expect_true("dup TX negotiates",
				ucp_app_roles_negotiate(UCP_APP_ROLE_TX, UCP_APP_ROLE_TX, &neg, &warn));
	expect_true("dup TX warn", warn && (neg == (uint8_t)UCP_APP_ROLE_TX));

	/* REQ_RSP both, also same-direction TX: no warn (REQ_RSP already negotiated) */
	neg = 0u;
	warn = false;
	expect_true("req+dup tx",
				ucp_app_roles_negotiate((uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP),
										(uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP),
										&neg, &warn));
	expect_true("req only no warn",
				!warn && (neg == (uint8_t)UCP_APP_ROLE_REQ_RSP));
}

static void test_byte_pipe_session(void)
{
	peer_t producer;
	peer_t consumer;
	uint32_t t;
	uint8_t prod_roles;
	uint8_t cons_roles;

	g_now_ms = 1000u;
	peer_init(&producer, &consumer, UCP_HW_ID_DEVBOARD,
			  (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP));
	peer_init(&consumer, &producer, UCP_HW_ID_HOST,
			  (uint8_t)(UCP_APP_ROLE_RX | UCP_APP_ROLE_REQ_RSP));

	for (t = 0u; t < 80u; t++)
	{
		g_now_ms += 50u;
		ucp_port_set_ops(&producer.ops);
		ucp_session_tick(&producer.session, g_now_ms);
		ucp_port_set_ops(&consumer.ops);
		ucp_session_tick(&consumer.session, g_now_ms);

		if (ucp_session_is_ready(&producer.session) &&
			ucp_session_is_ready(&consumer.session))
		{
			break;
		}
	}

	expect_true("producer READY", ucp_session_is_ready(&producer.session));
	expect_true("consumer READY", ucp_session_is_ready(&consumer.session));

	prod_roles = negotiated_roles_for(&producer.session, APP_ID_DEMO);
	cons_roles = negotiated_roles_for(&consumer.session, APP_ID_DEMO);
	expect_true("producer TX|REQ_RSP",
				prod_roles == (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP));
	expect_true("consumer RX|REQ_RSP",
				cons_roles == (uint8_t)(UCP_APP_ROLE_RX | UCP_APP_ROLE_REQ_RSP));
}

int main(void)
{
	g_failures = 0;
	test_roles_unit();
	test_byte_pipe_session();
	if (g_failures != 0)
	{
		printf("%d failure(s)\n", g_failures);
		return 1;
	}
	printf("all ucp tests passed\n");
	return 0;
}
