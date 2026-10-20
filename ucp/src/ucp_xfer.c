#include "ucp_xfer.h"

#include "ucp_protocol.h"

#include <string.h>

#define XFER_CHUNK_HDR 4u /* token u16 + index u8 + chunk_count u8 */
#define XFER_PUSH_HDR 5u  /* token u16 + seq u16 + flags u8 */

typedef struct
{
	uint8_t active;
	uint16_t token;
	uint16_t len;
	uint8_t data[UCP_XFER_MAX_SDU];
} xfer_offer_t;

typedef struct
{
	uint8_t active;
	uint8_t waiting_ack;
	uint8_t done;
	uint16_t token;
	uint16_t len;
	uint16_t offset;
	uint16_t next_seq;
	uint16_t last_sent_seq;
	uint16_t acked_seq; /* last contiguous seq_acked; 0xFFFF = none */
	uint8_t in_flight;
	uint32_t ack_deadline_ms;
	uint8_t data[UCP_XFER_MAX_SDU];
} xfer_push_tx_t;

typedef struct
{
	uint8_t active;
	uint16_t token;
	uint16_t expected_seq;
	uint16_t len;
	uint8_t since_ack;
	uint8_t data[UCP_XFER_MAX_SDU];
} xfer_push_rx_t;

static xfer_offer_t s_offer;
static xfer_push_tx_t s_push_tx;
static xfer_push_rx_t s_push_rx;
static uint16_t s_next_token = 1u;
static uint16_t s_last_push_done_token;
static ucp_xfer_push_rx_cb_t s_push_rx_cb;
static void *s_push_rx_user;

uint8_t ucp_xfer_chunk_data_cap(void)
{
	return (uint8_t)(UCP_PKT_MAX_PAYLOAD_LEN_BYTES - XFER_CHUNK_HDR);
}

uint8_t ucp_xfer_push_data_cap(void)
{
	return (uint8_t)(UCP_PKT_MAX_PAYLOAD_LEN_BYTES - XFER_PUSH_HDR);
}

static uint16_t alloc_token(void)
{
	uint16_t t = s_next_token++;
	if (s_next_token == 0u)
	{
		s_next_token = 1u;
	}
	return t;
}

static void clear_offer(void)
{
	memset(&s_offer, 0, sizeof(s_offer));
}

static void clear_push_tx(void)
{
	memset(&s_push_tx, 0, sizeof(s_push_tx));
}

static void clear_push_rx(void)
{
	memset(&s_push_rx, 0, sizeof(s_push_rx));
}

static void send_abort_local(uint16_t token)
{
	uint8_t p[2];
	p[0] = (uint8_t)(token & 0xFFu);
	p[1] = (uint8_t)((token >> 8) & 0xFFu);
	(void)ucp_send_pkt(UCP_APP_ID_XFER, UCP_XFER_PKT_ABORT, p, 2u);
}

static void send_push_ack(uint16_t token, uint16_t seq_acked, uint8_t status)
{
	uint8_t p[5];
	p[0] = (uint8_t)(token & 0xFFu);
	p[1] = (uint8_t)((token >> 8) & 0xFFu);
	p[2] = (uint8_t)(seq_acked & 0xFFu);
	p[3] = (uint8_t)((seq_acked >> 8) & 0xFFu);
	p[4] = status;
	(void)ucp_send_pkt(UCP_APP_ID_XFER, UCP_XFER_PKT_PUSH_ACK, p, 5u);
}

static uint8_t offer_chunk_count(void)
{
	uint8_t cap = ucp_xfer_chunk_data_cap();
	uint16_t n;

	if ((s_offer.active == 0u) || (cap == 0u))
	{
		return 0u;
	}
	if (s_offer.len == 0u)
	{
		return 1u;
	}
	n = (uint16_t)((s_offer.len + (uint16_t)cap - 1u) / (uint16_t)cap);
	return (n > 255u) ? 255u : (uint8_t)n;
}

static void handle_info_req(const uint8_t *payload, uint8_t len)
{
	uint16_t token;
	uint8_t rsp[7];
	uint8_t chunks;

	if (len < 2u)
	{
		return;
	}
	token = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	if ((s_offer.active == 0u) || (s_offer.token != token))
	{
		return;
	}
	chunks = offer_chunk_count();
	rsp[0] = (uint8_t)(token & 0xFFu);
	rsp[1] = (uint8_t)((token >> 8) & 0xFFu);
	rsp[2] = (uint8_t)(s_offer.len & 0xFFu);
	rsp[3] = (uint8_t)((s_offer.len >> 8) & 0xFFu);
	rsp[4] = ucp_xfer_chunk_data_cap();
	rsp[5] = chunks;
	rsp[6] = 0u;
	(void)ucp_send_pkt(UCP_APP_ID_XFER, UCP_XFER_PKT_INFO_RSP, rsp, 7u);
}

static void handle_chunk_req(const uint8_t *payload, uint8_t len)
{
	uint16_t token;
	uint8_t index;
	uint8_t chunks;
	uint8_t cap;
	uint16_t off;
	uint16_t n;
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];

	if (len < 3u)
	{
		return;
	}
	token = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	index = payload[2];
	if ((s_offer.active == 0u) || (s_offer.token != token))
	{
		return;
	}
	chunks = offer_chunk_count();
	if (index >= chunks)
	{
		return;
	}
	cap = ucp_xfer_chunk_data_cap();
	off = (uint16_t)index * (uint16_t)cap;
	if (off >= s_offer.len)
	{
		n = 0u;
	}
	else
	{
		n = (uint16_t)(s_offer.len - off);
		if (n > (uint16_t)cap)
		{
			n = cap;
		}
	}
	rsp[0] = (uint8_t)(token & 0xFFu);
	rsp[1] = (uint8_t)((token >> 8) & 0xFFu);
	rsp[2] = index;
	rsp[3] = chunks;
	if (n > 0u)
	{
		memcpy(&rsp[4], &s_offer.data[off], n);
	}
	(void)ucp_send_pkt(UCP_APP_ID_XFER, UCP_XFER_PKT_CHUNK_RSP, rsp, (uint8_t)(4u + n));
}

static void handle_abort(const uint8_t *payload, uint8_t len)
{
	uint16_t token;

	if (len < 2u)
	{
		return;
	}
	token = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	if ((s_offer.active != 0u) && (s_offer.token == token))
	{
		clear_offer();
	}
	if ((s_push_tx.active != 0u) && (s_push_tx.token == token))
	{
		clear_push_tx();
	}
	if ((s_push_rx.active != 0u) && (s_push_rx.token == token))
	{
		clear_push_rx();
	}
}

static void handle_push_ack(const uint8_t *payload, uint8_t len)
{
	uint16_t token;
	uint16_t seq_acked;
	uint8_t status;

	if (len < 5u)
	{
		return;
	}
	token = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	seq_acked = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
	status = payload[4];
	if ((s_push_tx.active == 0u) || (s_push_tx.token != token))
	{
		return;
	}
	if (status != UCP_XFER_ACK_OK)
	{
		clear_push_tx();
		return;
	}
	if (seq_acked < s_push_tx.last_sent_seq)
	{
		return;
	}
	s_push_tx.acked_seq = seq_acked;
	s_push_tx.waiting_ack = 0u;
	s_push_tx.in_flight = 0u;
	if (s_push_tx.offset >= s_push_tx.len)
	{
		s_last_push_done_token = s_push_tx.token;
		s_push_tx.done = 1u;
		s_push_tx.active = 0u;
	}
}

static void handle_push_data(const uint8_t *payload, uint8_t len)
{
	uint16_t token;
	uint16_t seq;
	uint8_t flags;
	uint8_t data_len;
	uint8_t is_first;
	uint8_t is_last;

	if (len < XFER_PUSH_HDR)
	{
		return;
	}
	token = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	seq = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
	flags = payload[4];
	data_len = (uint8_t)(len - XFER_PUSH_HDR);
	is_first = (uint8_t)((flags & UCP_XFER_FLAG_FIRST) != 0u);
	is_last = (uint8_t)((flags & UCP_XFER_FLAG_LAST) != 0u);

	if (is_first != 0u)
	{
		if ((s_push_rx.active != 0u) && (s_push_rx.token != token))
		{
			send_push_ack(token, 0u, UCP_XFER_ACK_BUSY);
			return;
		}
		clear_push_rx();
		s_push_rx.active = 1u;
		s_push_rx.token = token;
		s_push_rx.expected_seq = 0u;
		s_push_rx.len = 0u;
		s_push_rx.since_ack = 0u;
	}
	else if ((s_push_rx.active == 0u) || (s_push_rx.token != token))
	{
		send_push_ack(token, 0u, UCP_XFER_ACK_BAD_SEQ);
		return;
	}

	if (seq != s_push_rx.expected_seq)
	{
		send_push_ack(token, s_push_rx.expected_seq, UCP_XFER_ACK_BAD_SEQ);
		clear_push_rx();
		return;
	}

	if ((uint16_t)s_push_rx.len + (uint16_t)data_len > UCP_XFER_MAX_SDU)
	{
		send_push_ack(token, seq, UCP_XFER_ACK_OVERFLOW);
		clear_push_rx();
		return;
	}
	if (data_len > 0u)
	{
		memcpy(&s_push_rx.data[s_push_rx.len], &payload[XFER_PUSH_HDR], data_len);
		s_push_rx.len = (uint16_t)(s_push_rx.len + data_len);
	}
	s_push_rx.expected_seq = (uint16_t)(seq + 1u);
	s_push_rx.since_ack++;

	if ((is_last != 0u) || (s_push_rx.since_ack >= UCP_XFER_PUSH_WINDOW))
	{
		send_push_ack(token, seq, UCP_XFER_ACK_OK);
		s_push_rx.since_ack = 0u;
	}

	if (is_last != 0u)
	{
		if (s_push_rx_cb != NULL)
		{
			s_push_rx_cb(token, s_push_rx.data, s_push_rx.len, s_push_rx_user);
		}
		clear_push_rx();
	}
}

static void xfer_on_rx(const ucp_pkt_t *pkt)
{
	if ((pkt == NULL) || (pkt->app_id != UCP_APP_ID_XFER))
	{
		return;
	}
	switch (pkt->pkt_id)
	{
	case UCP_XFER_PKT_INFO_REQ:
		handle_info_req(pkt->payload, pkt->payload_len);
		break;
	case UCP_XFER_PKT_CHUNK_REQ:
		handle_chunk_req(pkt->payload, pkt->payload_len);
		break;
	case UCP_XFER_PKT_ABORT:
		handle_abort(pkt->payload, pkt->payload_len);
		break;
	case UCP_XFER_PKT_PUSH_DATA:
		handle_push_data(pkt->payload, pkt->payload_len);
		break;
	case UCP_XFER_PKT_PUSH_ACK:
		handle_push_ack(pkt->payload, pkt->payload_len);
		break;
	default:
		break;
	}
}

static void push_tx_emit(uint32_t now_ms)
{
	uint8_t cap = ucp_xfer_push_data_cap();
	uint8_t frame[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t emitted = 0u;

	if ((s_push_tx.active == 0u) || (s_push_tx.waiting_ack != 0u) || (s_push_tx.done != 0u))
	{
		return;
	}

	while ((emitted < UCP_XFER_PUSH_WINDOW) && (s_push_tx.offset < s_push_tx.len ||
												(s_push_tx.len == 0u && s_push_tx.next_seq == 0u)))
	{
		uint16_t remain;
		uint8_t n;
		uint8_t flags = 0u;
		uint8_t is_last;

		remain = (uint16_t)(s_push_tx.len - s_push_tx.offset);
		n = (remain > (uint16_t)cap) ? cap : (uint8_t)remain;
		if (s_push_tx.next_seq == 0u)
		{
			flags |= UCP_XFER_FLAG_FIRST;
		}
		is_last = (uint8_t)((s_push_tx.offset + n) >= s_push_tx.len);
		if (is_last != 0u)
		{
			flags |= UCP_XFER_FLAG_LAST;
		}
		else
		{
			flags |= UCP_XFER_FLAG_CONT;
		}

		frame[0] = (uint8_t)(s_push_tx.token & 0xFFu);
		frame[1] = (uint8_t)((s_push_tx.token >> 8) & 0xFFu);
		frame[2] = (uint8_t)(s_push_tx.next_seq & 0xFFu);
		frame[3] = (uint8_t)((s_push_tx.next_seq >> 8) & 0xFFu);
		frame[4] = flags;
		if (n > 0u)
		{
			memcpy(&frame[5], &s_push_tx.data[s_push_tx.offset], n);
		}
		if (!ucp_send_pkt(UCP_APP_ID_XFER, UCP_XFER_PKT_PUSH_DATA, frame, (uint8_t)(5u + n)))
		{
			break;
		}
		s_push_tx.last_sent_seq = s_push_tx.next_seq;
		s_push_tx.next_seq = (uint16_t)(s_push_tx.next_seq + 1u);
		s_push_tx.offset = (uint16_t)(s_push_tx.offset + n);
		emitted++;
		s_push_tx.in_flight++;

		if ((is_last != 0u) || (emitted >= UCP_XFER_PUSH_WINDOW))
		{
			/* Loopback may ACK inside ucp_send_pkt; only wait if not yet covered. */
			if ((s_push_tx.acked_seq == 0xFFFFu) ||
				(s_push_tx.acked_seq < s_push_tx.last_sent_seq))
			{
				s_push_tx.waiting_ack = 1u;
				s_push_tx.ack_deadline_ms = now_ms + UCP_XFER_PUSH_ACK_TIMEOUT_MS;
			}
			else if (s_push_tx.offset >= s_push_tx.len)
			{
				s_last_push_done_token = s_push_tx.token;
				s_push_tx.done = 1u;
				s_push_tx.active = 0u;
			}
			break;
		}
		if (s_push_tx.len == 0u)
		{
			break;
		}
	}
}

void ucp_xfer_init(void)
{
	clear_offer();
	clear_push_tx();
	clear_push_rx();
	s_next_token = 1u;
	s_push_rx_cb = NULL;
	s_push_rx_user = NULL;
}

void ucp_xfer_on_tick(uint32_t now_ms, bool link_ready)
{
	if (!link_ready)
	{
		return;
	}
	if ((s_push_tx.active != 0u) && (s_push_tx.waiting_ack != 0u))
	{
		if ((int32_t)(now_ms - s_push_tx.ack_deadline_ms) >= 0)
		{
			send_abort_local(s_push_tx.token);
			clear_push_tx();
			return;
		}
	}
	push_tx_emit(now_ms);
}

void ucp_xfer_on_link_event(ucp_app_link_event_t event)
{
	(void)event;
	clear_offer();
	clear_push_tx();
	clear_push_rx();
}

bool ucp_xfer_offer(uint16_t *token, const uint8_t *data, uint16_t len)
{
	if ((token == NULL) || (len > UCP_XFER_MAX_SDU) || ((len > 0u) && (data == NULL)))
	{
		return false;
	}
	if (s_offer.active != 0u)
	{
		return false;
	}
	s_offer.active = 1u;
	s_offer.token = alloc_token();
	s_offer.len = len;
	if (len > 0u)
	{
		memcpy(s_offer.data, data, len);
	}
	*token = s_offer.token;
	return true;
}

bool ucp_xfer_offer_active(void)
{
	return s_offer.active != 0u;
}

bool ucp_xfer_push_begin(uint16_t *token, const uint8_t *data, uint16_t len)
{
	if ((token == NULL) || (len > UCP_XFER_MAX_SDU) || ((len > 0u) && (data == NULL)))
	{
		return false;
	}
	if (s_push_tx.active != 0u)
	{
		return false;
	}
	memset(&s_push_tx, 0, sizeof(s_push_tx));
	s_push_tx.active = 1u;
	s_push_tx.acked_seq = 0xFFFFu;
	s_push_tx.token = alloc_token();
	s_push_tx.len = len;
	if (len > 0u)
	{
		memcpy(s_push_tx.data, data, len);
	}
	*token = s_push_tx.token;
	return true;
}

bool ucp_xfer_push_done(uint16_t token)
{
	return (s_last_push_done_token == token) && (token != 0u);
}

bool ucp_xfer_push_busy(void)
{
	return s_push_tx.active != 0u;
}

void ucp_xfer_set_push_rx_cb(ucp_xfer_push_rx_cb_t cb, void *user)
{
	s_push_rx_cb = cb;
	s_push_rx_user = user;
}

static void xfer_vtable_init(void)
{
	ucp_xfer_init();
}

static void xfer_vtable_tick(uint32_t now_ms, bool link_ready)
{
	ucp_xfer_on_tick(now_ms, link_ready);
}

static void xfer_vtable_link(ucp_app_link_event_t event)
{
	ucp_xfer_on_link_event(event);
}

const ucp_app_vtable_t g_ucp_xfer_app_vtable = {
	.app_id = UCP_APP_ID_XFER,
	.version = UCP_XFER_VERSION,
	.role_flags = (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_RX),
	.init = xfer_vtable_init,
	.on_rx = xfer_on_rx,
	.on_tick = xfer_vtable_tick,
	.on_link_event = xfer_vtable_link,
};
