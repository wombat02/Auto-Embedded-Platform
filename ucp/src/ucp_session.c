/*
 * File: ucp_session.c
 * Brief: Portable UCP link/session state machine implementation.
 *
 * Author:
 * GitHub:
 * Email:
 * License:
 */

#include "ucp_session.h"

#include "ucp_log.h"
#include "ucp_port.h"
#include "ucp_session_debug.h"

#include <stdio.h>
#include <string.h>

static void session_clear_cap_negotiation(ucp_session_t *session);
static void session_start_handshaking(ucp_session_t *session, uint32_t now_ms, bool reset_cap_state);
static void enter_ready(ucp_session_t *session, uint16_t negotiated_version);
static bool session_try_complete_pending_link_ready(ucp_session_t *session);
static void request_capability_exchange(ucp_session_t *session, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void session_touch_rx(ucp_session_t *session, uint32_t now_ms)
{
	session->last_rx_ms = now_ms;
}

static void session_mark_progress(ucp_session_t *session, uint32_t now_ms)
{
	session->last_progress_ms = now_ms;
}

static uint16_t session_build_nonce(ucp_session_t *session, uint32_t now_ms)
{
	(void)now_ms;
	if (session->local_session_nonce == 0u)
	{
		/* Start from 1 so zero remains available as "unset/unknown". */
		return 1u;
	}

	/* Monotonic attempt counter with wrap-around, skipping zero. */
	return (uint16_t)(session->local_session_nonce + 1u);
}

static uint32_t retry_jitter_ms(uint32_t seed)
{
	seed ^= (seed << 13);
	seed ^= (seed >> 17);
	seed ^= (seed << 5);
	return seed % UCP_RETRY_JITTER_MAX_MS;
}

static uint32_t hello_retry_period_ms(const ucp_session_t *session)
{
	uint32_t seed = ((uint32_t)session->local_session_nonce << 16) |
					(uint32_t)session->peer_session_nonce;
	return UCP_HELLO_RETRY_PERIOD_MS + retry_jitter_ms(seed ^ session->last_hello_tx_ms);
}

static uint32_t handshake_retry_period_ms(const ucp_session_t *session)
{
	uint32_t seed = ((uint32_t)session->local_session_nonce << 16) | ( (uint32_t)session->peer_session_nonce ^ ((uint32_t)session->hs_step << 24) );
	return UCP_HANDSHAKE_RETRY_PERIOD_MS + retry_jitter_ms(seed ^ session->last_hs_tx_ms);
}

static void session_set_state(ucp_session_t *session, ucp_link_state_t new_state)
{
	ucp_link_state_t prev;

	if (session->state == new_state)
	{
		return;
	}

	prev = session->state;
	session->state = new_state;
	{
		char msg[64];
		(void)snprintf(msg,
					   sizeof(msg),
					   "state %u -> %u",
					   (unsigned)prev,
					   (unsigned)new_state);
		ucp_session_trace(session, "H-STATE", msg);
	}

	switch (new_state)
	{
	case UCP_LINK_STATE_DISCOVERING:
		ucp_port_on_link_event(UCP_APP_LINK_EVENT_DISCOVERING);
		UCP_LOGI("UCP: link DISCOVERING");
		break;
	case UCP_LINK_STATE_HANDSHAKING:
		UCP_LOGI("UCP: link HANDSHAKING");
		break;
	case UCP_LINK_STATE_READY:
		ucp_port_on_link_event(UCP_APP_LINK_EVENT_LINK_READY);
		UCP_LOGI("UCP: link READY");
		break;
	case UCP_LINK_STATE_TIMEOUT:
		ucp_port_on_link_event(UCP_APP_LINK_EVENT_TIMEOUT);
		UCP_LOGI("UCP: link TIMEOUT");
		break;
	case UCP_LINK_STATE_DOWN:
		UCP_LOGD("UCP: link DOWN");
		break;
	default:
		break;
	}
}

static bool ucp_versions_compatible(uint16_t local, uint16_t peer)
{
	return local == peer;
}

static void session_log_negotiated_app_entry(uint8_t app_id, uint16_t version, uint8_t roles)
{
	char msg[64];
	(void)snprintf(msg,
				   sizeof(msg),
				   "app=0x%02X ver=0x%04X roles=0x%02X",
				   (unsigned)app_id,
				   (unsigned)version,
				   (unsigned)roles);
	UCP_LOGI(msg);
}

static void session_log_negotiated_apps(const ucp_session_t *session)
{
	uint8_t i;

	if (session == NULL)
	{
		return;
	}

	{
		char hdr[48];
		(void)snprintf(hdr,
					   sizeof(hdr),
					   "negotiated_apps=%u",
					   (unsigned)session->negotiated_app_count);
		UCP_LOGI(hdr);
	}

	for (i = 0u; i < session->negotiated_app_count; i++)
	{
		const ucp_ctrl_app_entry_t *entry = &session->negotiated_apps[i];
		session_log_negotiated_app_entry(entry->app_id, entry->version, entry->role_flags);
	}
}

static bool send_control_pkt(uint8_t pkt_id, const void *payload, uint8_t payload_len)
{
	ucp_pkt_t pkt = {0};

	ucp_session_trace_tx_ctrl(pkt_id, payload_len);
	pkt.app_id = (uint8_t)UCP_APP_ID_CONTROL;
	pkt.pkt_id = pkt_id;
	pkt.payload_len = payload_len;
	if ((payload_len > 0u) && (payload != NULL))
	{
		memcpy(pkt.payload, payload, payload_len);
	}
	return ucp_port_send(&pkt);
}

static void send_status(uint8_t pkt_id, ucp_status_code_t code, uint8_t ref_app, uint8_t ref_pkt, uint16_t detail)
{
	ucp_ctrl_status_payload_t status = {0};

	status.status_code = (uint8_t)code;
	status.ref_app_id = ref_app;
	status.ref_pkt_id = ref_pkt;
	status.detail = detail;
	(void)send_control_pkt(pkt_id, &status, (uint8_t)sizeof(status));
}

static void send_hello(const ucp_session_t *session, uint8_t pkt_id)
{
	ucp_ctrl_hello_payload_t hello = {0};

	hello.HW_ID = (uint8_t)ucp_port_get_local_hw_id();
	hello.ucp_proto_version = ucp_port_get_protocol_version();
	hello.session_nonce = (session != NULL) ? session->local_session_nonce : 0u;
	(void)send_control_pkt(pkt_id, &hello, (uint8_t)sizeof(hello));
}

static uint8_t build_capability_payload(ucp_session_t *session, uint8_t *out_buf, uint8_t out_buf_len)
{
	ucp_app_version_entry_t local_apps[UCP_CTRL_CAP_MAX_ENTRIES];
	uint8_t count;
	uint8_t payload_len;
	ucp_ctrl_capability_payload_t *cap = (ucp_ctrl_capability_payload_t *)out_buf;

	if ((session == NULL) || (out_buf == NULL))
	{
		return 0u;
	}
	if (out_buf_len < 5u)
	{
		return 0u;
	}
	memset(out_buf, 0, out_buf_len);
	cap->ucp_proto_version = ucp_port_get_protocol_version();
	cap->session_nonce = session->local_session_nonce;
	count = ucp_port_get_app_descriptors(local_apps, UCP_CTRL_CAP_MAX_ENTRIES);
	if (count > UCP_CTRL_CAP_MAX_ENTRIES)
	{
		count = UCP_CTRL_CAP_MAX_ENTRIES;
	}
	payload_len = (uint8_t)(5u + (count * (uint8_t)sizeof(ucp_ctrl_app_entry_t)));
	if (payload_len > UCP_PKT_MAX_PAYLOAD_LEN_BYTES)
	{
		count = (uint8_t)((UCP_PKT_MAX_PAYLOAD_LEN_BYTES - 5u) / sizeof(ucp_ctrl_app_entry_t));
		payload_len = (uint8_t)(5u + (count * (uint8_t)sizeof(ucp_ctrl_app_entry_t)));
	}
	if (payload_len > out_buf_len)
	{
		count = (uint8_t)((out_buf_len - 5u) / sizeof(ucp_ctrl_app_entry_t));
		payload_len = (uint8_t)(5u + (count * (uint8_t)sizeof(ucp_ctrl_app_entry_t)));
	}
	cap->entry_count = count;
	for (uint8_t i = 0u; i < count; i++)
	{
		cap->entries[i].app_id = local_apps[i].app_id;
		cap->entries[i].version = local_apps[i].version;
		cap->entries[i].role_flags = local_apps[i].role_flags;
	}
	return payload_len;
}

static void send_capability(ucp_session_t *session, uint8_t pkt_id)
{
	uint8_t cap_buf[sizeof(ucp_ctrl_capability_payload_t)] = {0};
	const uint8_t payload_len = build_capability_payload(session, cap_buf, sizeof(cap_buf));

	if ((payload_len > 0u) && (payload_len <= UCP_PKT_MAX_PAYLOAD_LEN_BYTES))
	{
		(void)send_control_pkt(pkt_id, cap_buf, payload_len);
		if (pkt_id == (uint8_t)UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ)
		{
			ucp_port_cap_tx_guard();
		}
	}
}

static void session_send_cap_req(ucp_session_t *session, uint32_t now_ms)
{
	session->cap_exchange_in_flight = true;
	session->hs_step = UCP_HS_STEP_CAP_REQ;
	session->last_hs_tx_ms = now_ms;
	session->last_cap_req_tx_ms = now_ms;
	send_capability(session, (uint8_t)UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ);
}

static void send_link_ready(ucp_session_t *session, uint8_t pkt_id, uint16_t negotiated_version)
{
	ucp_ctrl_link_ready_payload_t ready = {0};

	ready.status = 0u;
	ready.negotiated_ucp_version = negotiated_version;
	ready.session_nonce = session->local_session_nonce;
	(void)send_control_pkt(pkt_id, &ready, (uint8_t)sizeof(ready));
}

static void send_keepalive(void)
{
	(void)send_control_pkt((uint8_t)UCP_CTRL_PKT_ID_KEEPALIVE, NULL, 0u);
}

static bool parse_capability(const ucp_pkt_t *pkt, ucp_ctrl_capability_payload_t *out_cap)
{
	const uint8_t min_len = 5u; /* ucp_proto_version + session_nonce + entry_count */
	uint8_t expected_len;

	if ((pkt == NULL) || (out_cap == NULL))
	{
		return false;
	}
	if (pkt->payload_len < min_len)
	{
		return false;
	}
	memset(out_cap, 0, sizeof(*out_cap));
	out_cap->ucp_proto_version = (uint16_t)pkt->payload[0] |
								 ((uint16_t)pkt->payload[1] << 8);
	out_cap->session_nonce = (uint16_t)pkt->payload[2] |
							 ((uint16_t)pkt->payload[3] << 8);
	out_cap->entry_count = pkt->payload[4];
	if (out_cap->entry_count > UCP_CTRL_CAP_MAX_ENTRIES)
	{
		return false;
	}
	expected_len = (uint8_t)(min_len + (out_cap->entry_count * (uint8_t)sizeof(ucp_ctrl_app_entry_t)));
	if (pkt->payload_len != expected_len)
	{
		return false;
	}
	for (uint8_t i = 0u; i < out_cap->entry_count; i++)
	{
		const uint8_t base = (uint8_t)(min_len + (i * (uint8_t)sizeof(ucp_ctrl_app_entry_t)));
		out_cap->entries[i].app_id = pkt->payload[base + 0u];
		out_cap->entries[i].version = (uint16_t)pkt->payload[base + 1u] |
									  ((uint16_t)pkt->payload[base + 2u] << 8);
		out_cap->entries[i].role_flags = pkt->payload[base + 3u];
	}
	return true;
}

static void session_log_cap_parse_fail(const ucp_pkt_t *pkt)
{
	char msg[56];

	if (pkt == NULL)
	{
		return;
	}
	(void)snprintf(msg,
				   sizeof(msg),
				   "UCP: cap parse fail pkt=0x%02X len=%u",
				   (unsigned)pkt->pkt_id,
				   (unsigned)pkt->payload_len);
	UCP_LOGI(msg);
}

static bool store_peer_capability(ucp_session_t *session, const ucp_ctrl_capability_payload_t *cap)
{
	ucp_app_version_entry_t local_apps[UCP_CTRL_CAP_MAX_ENTRIES];
	uint8_t local_count = 0u;

	if (!ucp_versions_compatible(ucp_port_get_protocol_version(), cap->ucp_proto_version))
	{
		send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
					UCP_STATUS_UCP_VERSION_MISMATCH,
					(uint8_t)UCP_APP_ID_CONTROL,
					(uint8_t)UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ,
					cap->ucp_proto_version);
		UCP_LOGI("UCP: UCP version mismatch");
		return false;
	}

	session->peer_protocol_version = cap->ucp_proto_version;
	session->negotiated_ucp_version = cap->ucp_proto_version;
	session->peer_app_count = cap->entry_count;
	session->negotiated_app_count = 0u;
	memset(session->peer_apps, 0, sizeof(session->peer_apps));
	memset(session->negotiated_apps, 0, sizeof(session->negotiated_apps));
	for (uint8_t i = 0u; i < cap->entry_count; i++)
	{
		session->peer_apps[i] = cap->entries[i];
	}
	local_count = ucp_port_get_app_descriptors(local_apps, UCP_CTRL_CAP_MAX_ENTRIES);
	if (local_count > UCP_CTRL_CAP_MAX_ENTRIES)
	{
		local_count = UCP_CTRL_CAP_MAX_ENTRIES;
	}
	bool duplicate_role_warning = false;

	for (uint8_t i = 0u; i < local_count; i++)
	{
		for (uint8_t j = 0u; j < cap->entry_count; j++)
		{
			uint8_t negotiated_roles = 0u;
			bool role_warn = false;

			if (local_apps[i].app_id != cap->entries[j].app_id)
			{
				continue;
			}
			if (local_apps[i].version != cap->entries[j].version)
			{
				continue;
			}
			if (!ucp_app_roles_negotiate(local_apps[i].role_flags,
										 cap->entries[j].role_flags,
										 &negotiated_roles,
										 &role_warn))
			{
				continue;
			}
			if (session->negotiated_app_count >= UCP_CTRL_CAP_MAX_ENTRIES)
			{
				break;
			}

			{
				ucp_ctrl_app_entry_t *entry = &session->negotiated_apps[session->negotiated_app_count++];
				entry->app_id = local_apps[i].app_id;
				entry->version = local_apps[i].version;
				entry->role_flags = negotiated_roles;
			}

			if (role_warn)
			{
				duplicate_role_warning = true;
				{
					char warn_msg[64];
					(void)snprintf(warn_msg,
								   sizeof(warn_msg),
								   "dup role app=0x%02X local=0x%02X peer=0x%02X",
								   (unsigned)local_apps[i].app_id,
								   (unsigned)local_apps[i].role_flags,
								   (unsigned)cap->entries[j].role_flags);
					UCP_LOGI(warn_msg);
				}
			}
			break;
		}
	}

	if (duplicate_role_warning)
	{
		send_status((uint8_t)UCP_CTRL_PKT_ID_WARNING,
					UCP_STATUS_APP_VERSION_MISMATCH,
					(uint8_t)UCP_APP_ID_CONTROL,
					0u,
					0u);
	}

	if ((session->negotiated_app_count == 0u) && (local_count > 0u) && (cap->entry_count > 0u))
	{
		char warn[72];
		(void)snprintf(warn,
					   sizeof(warn),
					   "UCP: no app matches local=%u peer=%u (check ver/roles)",
					   (unsigned)local_count,
					   (unsigned)cap->entry_count);
		UCP_LOGI(warn);
	}

	{
		char summary[72];
		(void)snprintf(summary,
					   sizeof(summary),
					   "UCP: cap exchange local=%u peer=%u negotiated=%u",
					   (unsigned)local_count,
					   (unsigned)cap->entry_count,
					   (unsigned)session->negotiated_app_count);
		UCP_LOGI(summary);
	}
	session->peer_capability_received = true;
	session->cap_exchange_in_flight = false;
	ucp_session_trace(session, "H-CAP", "store_peer_capability ok");

	if (!ucp_port_validate_peer_apps(cap->entries, cap->entry_count))
	{
		send_status((uint8_t)UCP_CTRL_PKT_ID_WARNING,
					UCP_STATUS_APP_VERSION_MISMATCH,
					(uint8_t)UCP_APP_ID_CONTROL,
					0u,
					0u);
		UCP_LOGI("UCP: app capability warning");
		/* Phase 1: continue to READY when UCP version matches. */
	}

	(void)session_try_complete_pending_link_ready(session);
	return true;
}

static void request_capability_exchange(ucp_session_t *session, uint32_t now_ms)
{
	if (session->state != UCP_LINK_STATE_HANDSHAKING)
	{
		session_start_handshaking(session, now_ms, true);
		return;
	}

	if (session->cap_exchange_in_flight)
	{
		return;
	}

	if (!ucp_port_handshake_tx_cap_allowed())
	{
		ucp_session_trace(session, "H-CAP", "defer CAP_REQ (RX listen window)");
		return;
	}

	ucp_session_trace(session, "H-CAP", "request_capability_exchange tx CAP_REQ");
	session_send_cap_req(session, now_ms);
}

static bool session_try_complete_pending_link_ready(ucp_session_t *session)
{
	uint16_t version;

	if (!session->link_ready_pending || !session->peer_capability_received)
	{
		return false;
	}

	version = session->pending_link_ready_version;
	session->link_ready_pending = false;

	if (session->link_ready_need_rsp)
	{
		session->link_ready_need_rsp = false;
		send_link_ready(session, (uint8_t)UCP_CTRL_PKT_ID_LINK_READY_RSP, version);
	}

	enter_ready(session, version);
	return true;
}

static void session_clear_cap_negotiation(ucp_session_t *session)
{
	session->peer_capability_received = false;
	session->cap_exchange_in_flight = false;
	session->link_ready_pending = false;
	session->link_ready_need_rsp = false;
	session->pending_link_ready_version = 0u;
	session->last_cap_req_tx_ms = 0u;
	session->peer_app_count = 0u;
	session->negotiated_app_count = 0u;
	memset(session->peer_apps, 0, sizeof(session->peer_apps));
	memset(session->negotiated_apps, 0, sizeof(session->negotiated_apps));
}

static void session_start_handshaking(ucp_session_t *session, uint32_t now_ms, bool reset_cap_state)
{
	if (reset_cap_state &&
		(session->state == UCP_LINK_STATE_HANDSHAKING) &&
		(session->cap_exchange_in_flight || session->link_ready_pending))
	{
		reset_cap_state = false;
	}

	if (reset_cap_state)
	{
		session_clear_cap_negotiation(session);
	}

	session->hs_step = UCP_HS_STEP_CAP_REQ;
	session->last_rx_ms = now_ms;
	session_mark_progress(session, now_ms);
	session->last_hs_tx_ms = now_ms;
	session->handshake_enter_ms = now_ms;
	session_set_state(session, UCP_LINK_STATE_HANDSHAKING);

	/*
	 * Listen for peer CAP_REQ before local CAP_REQ — avoids immediate TX/RX
	 * collision on shared TTL UART (peer CAP_RSP was seen as pkt=0x03 chk fail).
	 */
	ucp_session_trace(session, "H-CAP", "session_start_handshaking listen for peer CAP");
}

static void maybe_resync_from_hello(ucp_session_t *session, uint32_t now_ms, uint16_t peer_nonce, bool from_hello_req)
{
	const bool peer_epoch_changed = (peer_nonce != 0u) && (peer_nonce != session->peer_session_nonce);

	if (session->state == UCP_LINK_STATE_READY)
	{
		session->peer_session_nonce = peer_nonce;
		if (from_hello_req || peer_epoch_changed)
		{
			session_start_handshaking(session, now_ms, true);
			UCP_LOGI("UCP: peer restart detected, renegotiating");
		}
		return;
	}

	if (session->state == UCP_LINK_STATE_HANDSHAKING)
	{
		session->peer_session_nonce = peer_nonce;
		if (!peer_epoch_changed)
		{
			return;
		}
		if (session->peer_capability_received || session->cap_exchange_in_flight ||
			session->link_ready_pending)
		{
			ucp_session_trace(session, "H-HELLO", "ignore epoch change during active handshake");
			return;
		}
		ucp_session_trace(session, "H-HELLO", "handshake resync (epoch changed)");
		session_start_handshaking(session, now_ms, true);
		return;
	}

	session->peer_session_nonce = peer_nonce;

	if (session->state == UCP_LINK_STATE_DISCOVERING)
	{
		ucp_session_trace(session, "H-HELLO", "DISCOVERING -> start handshaking from HELLO");
		session_start_handshaking(session, now_ms, peer_epoch_changed || from_hello_req);
	}
}

static void enter_ready(ucp_session_t *session, uint16_t negotiated_version)
{
	uint32_t now_ms = ucp_port_get_time_ms();

	session->negotiated_ucp_version = negotiated_version;
	session->last_rx_ms = now_ms;
	session_mark_progress(session, now_ms);
	session->last_keepalive_tx_ms = now_ms;
	session_log_negotiated_apps(session);
	session_set_state(session, UCP_LINK_STATE_READY);
}

static bool hello_is_local_echo(const ucp_ctrl_hello_payload_t *hello)
{
	return (hello != NULL) &&
		   ((ucp_hw_id_t)hello->HW_ID == ucp_port_get_local_hw_id());
}

static void reset_to_discovering(ucp_session_t *session, uint32_t now_ms)
{
	const bool from_timeout = (session->state == UCP_LINK_STATE_TIMEOUT);

	if ((session->state == UCP_LINK_STATE_DOWN) || from_timeout)
	{
		session->local_session_nonce = session_build_nonce(session, now_ms);
	}
	session->hs_step = UCP_HS_STEP_CAP_REQ;
	session->peer_app_count = 0u;
	session->negotiated_app_count = 0u;
	session->peer_session_nonce = 0u;
	session_clear_cap_negotiation(session);
	session->last_rx_ms = now_ms;
	session_mark_progress(session, now_ms);
	session->last_hello_tx_ms = now_ms;
	session->last_keepalive_tx_ms = now_ms;
	/*
	 * Stay quiet after timeout so a peer still in READY can expire its own
	 * last_rx. HELLO_REQ would otherwise refresh the peer and stall rediscovery.
	 */
	session->discover_quiet_until_ms = from_timeout ? (now_ms + UCP_LINK_TIMEOUT_MS) : 0u;
	session_set_state(session, UCP_LINK_STATE_DISCOVERING);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void ucp_session_init(ucp_session_t *session)
{
	if (session == NULL)
	{
		return;
	}

	memset(session, 0, sizeof(*session));
	session->local_hw_id = ucp_port_get_local_hw_id();
	session->negotiated_ucp_version = ucp_port_get_protocol_version();
	reset_to_discovering(session, ucp_port_get_time_ms());
	UCP_LOGD("UCP: session init");
}

void ucp_session_tick(ucp_session_t *session, uint32_t now_ms)
{
	if (session == NULL)
	{
		return;
	}

	/* Timeout supervision for active link states. */
	if (session->state == UCP_LINK_STATE_HANDSHAKING)
	{
		const bool hs_timeout =
			ucp_elapsed_gt_ms(now_ms, session->last_progress_ms, UCP_HANDSHAKE_PROGRESS_TIMEOUT_MS);

		ucp_session_trace_timeout_check(session,
										now_ms,
										session->last_progress_ms,
										UCP_HANDSHAKE_PROGRESS_TIMEOUT_MS,
										hs_timeout);
		if (hs_timeout)
		{
			ucp_session_trace(session, "H-TMO", "HANDSHAKING progress timeout");
			session_clear_cap_negotiation(session);
			session_set_state(session, UCP_LINK_STATE_TIMEOUT);
		}
	}
	else if (session->state == UCP_LINK_STATE_READY)
	{
		if (ucp_elapsed_gt_ms(now_ms, session->last_rx_ms, UCP_LINK_TIMEOUT_MS))
		{
			{
				char msg[80];
				(void)snprintf(msg,
							   sizeof(msg),
							   "TIMEOUT now=%lu last_rx=%lu dt=%lu",
							   (unsigned long)now_ms,
							   (unsigned long)session->last_rx_ms,
							   (unsigned long)ucp_elapsed_ms(now_ms, session->last_rx_ms));
				UCP_LOGI(msg);
			}
			session_set_state(session, UCP_LINK_STATE_TIMEOUT);
		}
	}

	if (session->state == UCP_LINK_STATE_TIMEOUT)
	{
		reset_to_discovering(session, now_ms);
		return;
	}

	if (session->state == UCP_LINK_STATE_DISCOVERING)
	{
		if (session->discover_quiet_until_ms != 0u)
		{
			if (ucp_time_before_deadline_ms(now_ms, session->discover_quiet_until_ms))
			{
				return;
			}
			session->discover_quiet_until_ms = 0u;
		}
		if (ucp_elapsed_ge_ms(now_ms, session->last_hello_tx_ms, hello_retry_period_ms(session)))
		{
			session->last_hello_tx_ms = now_ms;
			send_hello(session, (uint8_t)UCP_CTRL_PKT_ID_DEVICE_HELLO_REQ);
			UCP_LOGD("UCP: tx HELLO_REQ");
		}
		return;
	}

	if (session->state == UCP_LINK_STATE_HANDSHAKING)
	{
		if (session->cap_exchange_in_flight && !session->peer_capability_received &&
			(session->last_cap_req_tx_ms != 0u) &&
			(ucp_elapsed_ge_ms(now_ms, session->last_cap_req_tx_ms, UCP_CAP_RSP_WINDOW_MS)))
		{
			session->cap_exchange_in_flight = false;
			ucp_session_trace(session, "H-CAP", "CAP_RSP window expired, allow retry");
		}

		if (ucp_elapsed_ge_ms(now_ms, session->last_hs_tx_ms, handshake_retry_period_ms(session)))
		{
			session->last_hs_tx_ms = now_ms;
			switch (session->hs_step)
			{
			case UCP_HS_STEP_CAP_REQ:
				if (!session->peer_capability_received && !session->cap_exchange_in_flight)
				{
					if (session->last_cap_req_tx_ms == 0u)
					{
						if (ucp_elapsed_ge_ms(now_ms, session->handshake_enter_ms, UCP_CAP_PEER_LISTEN_MS))
						{
							request_capability_exchange(session, now_ms);
							UCP_LOGD("UCP: tx CAP_REQ after peer listen");
						}
					}
					else if (ucp_elapsed_ge_ms(now_ms, session->last_cap_req_tx_ms, UCP_CAP_REQ_RETRY_MS))
					{
						request_capability_exchange(session, now_ms);
						UCP_LOGD("UCP: retry CAP_REQ");
					}
				}
				break;
			case UCP_HS_STEP_LINK_READY_REQ:
				send_link_ready(session, (uint8_t)UCP_CTRL_PKT_ID_LINK_READY_REQ, session->negotiated_ucp_version);
				UCP_LOGD("UCP: retry LINK_READY_REQ");
				break;
			default:
				break;
			}
		}
		return;
	}

	if (session->state == UCP_LINK_STATE_READY)
	{
		if (ucp_elapsed_ge_ms(now_ms, session->last_keepalive_tx_ms, UCP_KEEPALIVE_PERIOD_MS))
		{
			session->last_keepalive_tx_ms = now_ms;
			send_keepalive();
			UCP_LOGD("UCP: tx KEEPALIVE");
		}
	}
}

void ucp_session_on_packet(ucp_session_t *session, const ucp_pkt_t *pkt)
{
	ucp_ctrl_hello_payload_t hello;
	ucp_ctrl_capability_payload_t cap;
	ucp_ctrl_link_ready_payload_t link_ready;
	uint32_t now_ms;

	if ((session == NULL) || (pkt == NULL))
	{
		return;
	}

	now_ms = ucp_port_get_time_ms();

	UCP_LOGD("UCP: rx app/pkt");

	if (pkt->app_id != (uint8_t)UCP_APP_ID_CONTROL)
	{
		if (session->state == UCP_LINK_STATE_READY)
		{
			session_touch_rx(session, now_ms);
			ucp_port_route_application_packet(pkt);
		}
		return;
	}

	if ((pkt->pkt_id == (uint8_t)UCP_CTRL_PKT_ID_NOT_SUPPORTED) ||
		(pkt->pkt_id == (uint8_t)UCP_CTRL_PKT_ID_WARNING) ||
		(pkt->pkt_id == (uint8_t)UCP_CTRL_PKT_ID_ERROR))
	{
		return;
	}

	session_touch_rx(session, now_ms);

	switch (pkt->pkt_id)
	{
	case UCP_CTRL_PKT_ID_KEEPALIVE:
		UCP_LOGD("UCP: rx KEEPALIVE");
		return;

	case UCP_CTRL_PKT_ID_DEVICE_HELLO_REQ:
		if (pkt->payload_len != sizeof(ucp_ctrl_hello_payload_t))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		memcpy(&hello, pkt->payload, sizeof(hello));
		if (hello_is_local_echo(&hello))
		{
			UCP_LOGI("UCP: ignore HELLO_REQ from local HW_ID (UART echo)");
			return;
		}
		session->peer_hw_id = (ucp_hw_id_t)hello.HW_ID;
		session->peer_protocol_version = hello.ucp_proto_version;
		if (!ucp_versions_compatible(ucp_port_get_protocol_version(), hello.ucp_proto_version))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_UCP_VERSION_MISMATCH,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						hello.ucp_proto_version);
			UCP_LOGI("UCP: peer hello UCP version mismatch");
			return;
		}
		{
			/*
			 * maybe_resync_from_hello() moves DISCOVERING -> HANDSHAKING before we
			 * can test state. Always reply to HELLO_REQ so the peer learns our HW ID
			 * (host-initiated USB discovery depends on this).
			 */
			maybe_resync_from_hello(session, now_ms, hello.session_nonce, true);
			send_hello(session, (uint8_t)UCP_CTRL_PKT_ID_DEVICE_HELLO_RSP);
			UCP_LOGD("UCP: rx HELLO_REQ, tx HELLO_RSP");
		}
		session_mark_progress(session, now_ms);
		return;

	case UCP_CTRL_PKT_ID_DEVICE_HELLO_RSP:
		if (pkt->payload_len != sizeof(ucp_ctrl_hello_payload_t))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		memcpy(&hello, pkt->payload, sizeof(hello));
		if (hello_is_local_echo(&hello))
		{
			UCP_LOGI("UCP: ignore HELLO_RSP from local HW_ID (UART echo)");
			return;
		}
		session->peer_hw_id = (ucp_hw_id_t)hello.HW_ID;
		session->peer_protocol_version = hello.ucp_proto_version;
		if (!ucp_versions_compatible(ucp_port_get_protocol_version(), hello.ucp_proto_version))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_UCP_VERSION_MISMATCH,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						hello.ucp_proto_version);
			UCP_LOGI("UCP: peer hello rsp UCP version mismatch");
			return;
		}
		maybe_resync_from_hello(session, now_ms, hello.session_nonce, false);
		session_mark_progress(session, now_ms);
		UCP_LOGD("UCP: rx HELLO_RSP");
		return;

	case UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_REQ:
		if (!parse_capability(pkt, &cap))
		{
			session_log_cap_parse_fail(pkt);
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		if (!store_peer_capability(session, &cap))
		{
			return;
		}
		session->peer_session_nonce = cap.session_nonce;
		ucp_port_peer_turnaround_delay();
		send_capability(session, (uint8_t)UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_RSP);
		session_mark_progress(session, now_ms);
		if (session->state == UCP_LINK_STATE_DISCOVERING)
		{
			session_start_handshaking(session, now_ms, false);
		}
		if (session_try_complete_pending_link_ready(session))
		{
			UCP_LOGD("UCP: rx CAP_REQ, completed deferred READY");
			return;
		}
		if (session->state == UCP_LINK_STATE_READY)
		{
			UCP_LOGD("UCP: rx CAP_REQ, tx CAP_RSP");
			return;
		}
		if (session->hs_step == UCP_HS_STEP_LINK_READY_REQ)
		{
			UCP_LOGD("UCP: rx CAP_REQ, tx CAP_RSP only");
			return;
		}
		/* Progress rule: receiving a valid CAP_REQ is enough to move handshake forward. */
		session->hs_step = UCP_HS_STEP_LINK_READY_REQ;
		session->last_hs_tx_ms = now_ms;
		send_link_ready(session, (uint8_t)UCP_CTRL_PKT_ID_LINK_READY_REQ, session->negotiated_ucp_version);
		UCP_LOGD("UCP: rx CAP_REQ, tx CAP_RSP + LINK_READY_REQ");
		return;

	case UCP_CTRL_PKT_ID_CAPABILITY_DISCOVERY_RSP:
		UCP_LOGI("UCP: rx CAP_RSP");
		if (!parse_capability(pkt, &cap))
		{
			session_log_cap_parse_fail(pkt);
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		if (!store_peer_capability(session, &cap))
		{
			return;
		}
		session->peer_session_nonce = cap.session_nonce;
		session_mark_progress(session, now_ms);
		if (session_try_complete_pending_link_ready(session))
		{
			UCP_LOGD("UCP: rx CAP_RSP, completed deferred READY");
			return;
		}
		session->hs_step = UCP_HS_STEP_LINK_READY_REQ;
		session->last_hs_tx_ms = now_ms;
		send_link_ready(session, (uint8_t)UCP_CTRL_PKT_ID_LINK_READY_REQ, session->negotiated_ucp_version);
		UCP_LOGD("UCP: rx CAP_RSP, tx LINK_READY_REQ");
		return;

	case UCP_CTRL_PKT_ID_LINK_READY_REQ:
		if (pkt->payload_len != sizeof(ucp_ctrl_link_ready_payload_t))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		memcpy(&link_ready, pkt->payload, sizeof(link_ready));
		if (!ucp_versions_compatible(ucp_port_get_protocol_version(), link_ready.negotiated_ucp_version))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_UCP_VERSION_MISMATCH,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						link_ready.negotiated_ucp_version);
			return;
		}
		if ((link_ready.session_nonce != 0u) && (session->peer_session_nonce != 0u) &&
			(link_ready.session_nonce != session->peer_session_nonce))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_WARNING,
						UCP_STATUS_UNEXPECTED_STATE,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						link_ready.session_nonce);
			return;
		}
		if (!session->link_ready_pending)
		{
			session_mark_progress(session, now_ms);
		}
		if (!session->peer_capability_received)
		{
			const bool first_lr = !session->link_ready_pending;

			if (first_lr)
			{
				UCP_LOGI("UCP: defer READY until capability exchange");
				ucp_session_trace(session, "H-LR", "defer READY (LINK_READY_REQ, no peer cap)");
			}
			else
			{
				ucp_session_trace(session, "H-LR", "defer READY duplicate LINK_READY_REQ");
			}
			session->link_ready_pending = true;
			session->link_ready_need_rsp = true;
			session->pending_link_ready_version = link_ready.negotiated_ucp_version;
			if (first_lr && !session->cap_exchange_in_flight)
			{
				request_capability_exchange(session, now_ms);
			}
			else if (session->cap_exchange_in_flight)
			{
				ucp_session_trace(session, "H-LR", "defer READY, CAP already in flight");
			}
			return;
		}
		send_link_ready(session, (uint8_t)UCP_CTRL_PKT_ID_LINK_READY_RSP, link_ready.negotiated_ucp_version);
		enter_ready(session, link_ready.negotiated_ucp_version);
		UCP_LOGD("UCP: rx LINK_READY_REQ, tx LINK_READY_RSP");
		return;

	case UCP_CTRL_PKT_ID_LINK_READY_RSP:
		if (pkt->payload_len != sizeof(ucp_ctrl_link_ready_payload_t))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_ERROR,
						UCP_STATUS_INVALID_PACKET,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						pkt->payload_len);
			return;
		}
		memcpy(&link_ready, pkt->payload, sizeof(link_ready));
		if ((link_ready.session_nonce != 0u) && (session->peer_session_nonce != 0u) &&
			(link_ready.session_nonce != session->peer_session_nonce))
		{
			send_status((uint8_t)UCP_CTRL_PKT_ID_WARNING,
						UCP_STATUS_UNEXPECTED_STATE,
						(uint8_t)UCP_APP_ID_CONTROL,
						pkt->pkt_id,
						link_ready.session_nonce);
			return;
		}
		if (!session->link_ready_pending)
		{
			session_mark_progress(session, now_ms);
		}
		if (!session->peer_capability_received)
		{
			const bool first_lr = !session->link_ready_pending;

			if (first_lr)
			{
				UCP_LOGI("UCP: defer READY until capability exchange");
				ucp_session_trace(session, "H-LR", "defer READY (LINK_READY_RSP, no peer cap)");
			}
			session->link_ready_pending = true;
			session->link_ready_need_rsp = false;
			session->pending_link_ready_version = link_ready.negotiated_ucp_version;
			if (first_lr && !session->cap_exchange_in_flight)
			{
				request_capability_exchange(session, now_ms);
			}
			return;
		}
		enter_ready(session, link_ready.negotiated_ucp_version);
		UCP_LOGD("UCP: rx LINK_READY_RSP");
		return;

	default:
		send_status((uint8_t)UCP_CTRL_PKT_ID_NOT_SUPPORTED,
					UCP_STATUS_INVALID_PACKET,
					(uint8_t)UCP_APP_ID_CONTROL,
					pkt->pkt_id,
					0u);
		return;
	}
}

bool ucp_session_is_ready(const ucp_session_t *session)
{
	return (session != NULL) && (session->state == UCP_LINK_STATE_READY);
}

ucp_link_state_t ucp_session_get_link_state(const ucp_session_t *session)
{
	if (session == NULL)
	{
		return UCP_LINK_STATE_DOWN;
	}
	return session->state;
}

bool ucp_session_has_peer_capability(const ucp_session_t *session)
{
	return (session != NULL) && session->peer_capability_received;
}

ucp_hw_id_t ucp_session_get_peer_hw_id(const ucp_session_t *session)
{
	if (session == NULL)
	{
		return UNKNOWN;
	}
	return session->peer_hw_id;
}

uint16_t ucp_session_get_peer_protocol_version(const ucp_session_t *session)
{
	if (session == NULL)
	{
		return 0u;
	}
	return session->peer_protocol_version;
}

bool ucp_session_peer_supports(const ucp_session_t *session, uint8_t app_id, uint16_t min_version, uint8_t required_roles)
{
	if (session == NULL)
	{
		return false;
	}

	for (uint8_t i = 0u; i < session->negotiated_app_count; i++)
	{
		const ucp_ctrl_app_entry_t *entry = &session->negotiated_apps[i];
		if (entry->app_id != app_id)
		{
			continue;
		}
		if (entry->version < min_version)
		{
			continue;
		}
		if ((entry->role_flags & required_roles) != required_roles)
		{
			continue;
		}
		return true;
	}
	return false;
}

uint8_t ucp_session_get_negotiated_app_count(const ucp_session_t *session)
{
	if (session == NULL)
	{
		return 0u;
	}
	return session->negotiated_app_count;
}

void ucp_session_on_cap_frame_rx_failed(ucp_session_t *session)
{
	if (session == NULL)
	{
		return;
	}

	if (!session->cap_exchange_in_flight || session->peer_capability_received)
	{
		return;
	}

	session->cap_exchange_in_flight = false;
	ucp_session_trace(session, "H-CAP", "cap frame checksum fail, allow CAP retry");
}
