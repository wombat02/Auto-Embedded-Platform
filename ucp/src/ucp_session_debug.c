/*
 * File: ucp_session_debug.c
 * Brief: Compile-time-gated session trace helpers for handshake diagnostics.
 *
 * Author:
 * GitHub:
 * Email:
 * License:
 */

#include "ucp_session_debug.h"

#include "ucp_log.h"
#include "ucp_port.h"

#include <stdio.h>

#if UCP_DEBUG_VERBOSE

void ucp_session_trace(const ucp_session_t *session, const char *hypothesis_id, const char *event)
{
	char msg[160];

	if ((session == NULL) || (event == NULL))
	{
		return;
	}

	(void)snprintf(msg,
				   sizeof(msg),
				   "[UCP-TR][%s] %s st=%u hs=%u cap=%u peer_cap=%u lr_pend=%u "
				   "lr_rsp=%u ln=%u pn=%u",
				   (hypothesis_id != NULL) ? hypothesis_id : "-",
				   event,
				   (unsigned)session->state,
				   (unsigned)session->hs_step,
				   (unsigned)session->cap_exchange_in_flight,
				   (unsigned)session->peer_capability_received,
				   (unsigned)session->link_ready_pending,
				   (unsigned)session->link_ready_need_rsp,
				   (unsigned)session->local_session_nonce,
				   (unsigned)session->peer_session_nonce);
	UCP_LOGD(msg);
}

void ucp_session_trace_timeout_check(const ucp_session_t *session,
									 uint32_t tick_now_ms,
									 uint32_t last_progress_ms,
									 uint32_t limit_ms,
									 bool will_timeout)
{
	char msg[128];
	uint32_t delta;

	(void)session;

	delta = ucp_elapsed_ms(tick_now_ms, last_progress_ms);

	(void)snprintf(msg,
				   sizeof(msg),
				   "[UCP-TR][H-TMO] tick_chk now=%lu prog=%lu d=%lu lim=%lu stale=%u ->%u",
				   (unsigned long)tick_now_ms,
				   (unsigned long)last_progress_ms,
				   (unsigned long)delta,
				   (unsigned long)limit_ms,
				   (unsigned)(tick_now_ms < last_progress_ms),
				   (unsigned)will_timeout);
	UCP_LOGD(msg);
}

void ucp_session_trace_tx_ctrl(uint8_t pkt_id, uint8_t payload_len)
{
	char msg[48];

	(void)snprintf(msg,
				   sizeof(msg),
				   "[UCP-TR][H-TX] enqueue ctrl pkt=0x%02X len=%u",
				   (unsigned)pkt_id,
				   (unsigned)payload_len);
	UCP_LOGD(msg);
}

#endif /* UCP_DEBUG_VERBOSE */
