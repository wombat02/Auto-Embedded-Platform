#ifndef UCP_SESSION_DEBUG_H_
#define UCP_SESSION_DEBUG_H_

#include "ucp_session.h"

#ifndef UCP_DEBUG_VERBOSE
#define UCP_DEBUG_VERBOSE 0
#endif

#if UCP_DEBUG_VERBOSE

void ucp_session_trace(const ucp_session_t *session, const char *hypothesis_id, const char *event);

void ucp_session_trace_timeout_check(const ucp_session_t *session,
									 uint32_t tick_now_ms,
									 uint32_t last_progress_ms,
									 uint32_t limit_ms,
									 bool will_timeout);

void ucp_session_trace_tx_ctrl(uint8_t pkt_id, uint8_t payload_len);

#else

#define ucp_session_trace(session, hypothesis_id, event) ((void)0)
#define ucp_session_trace_timeout_check(session, tick_now, last_prog, lim, will) ((void)0)
#define ucp_session_trace_tx_ctrl(pkt_id, payload_len) ((void)0)

#endif

#endif /* UCP_SESSION_DEBUG_H_ */
