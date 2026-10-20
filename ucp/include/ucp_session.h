#ifndef UCP_SESSION_H_
#define UCP_SESSION_H_

#include <stdbool.h>
#include <stdint.h>

#include "ucp_protocol.h"

#define UCP_LINK_TIMEOUT_MS 3000u
#define UCP_KEEPALIVE_PERIOD_MS ((UCP_LINK_TIMEOUT_MS * 2u) / 3u)
#define UCP_HELLO_RETRY_PERIOD_MS 500u
#define UCP_HANDSHAKE_RETRY_PERIOD_MS 500u
#define UCP_CAP_REQ_RETRY_MS 600u
#define UCP_CAP_RSP_WINDOW_MS 800u
#define UCP_CAP_PEER_LISTEN_MS 300u
#define UCP_HANDSHAKE_PROGRESS_TIMEOUT_MS UCP_LINK_TIMEOUT_MS
#define UCP_RETRY_JITTER_MAX_MS 120u

typedef enum
{
	UCP_LINK_STATE_DOWN = 0,
	UCP_LINK_STATE_DISCOVERING,
	UCP_LINK_STATE_HANDSHAKING,
	UCP_LINK_STATE_READY,
	UCP_LINK_STATE_TIMEOUT
} ucp_link_state_t;

typedef enum
{
	UCP_HS_STEP_CAP_REQ = 0,
	UCP_HS_STEP_CAP_RSP,
	UCP_HS_STEP_LINK_READY_REQ,
	UCP_HS_STEP_LINK_READY_RSP
} ucp_handshake_step_t;

typedef struct
{
	ucp_link_state_t state;
	ucp_handshake_step_t hs_step;

	ucp_hw_id_t local_hw_id;
	ucp_hw_id_t peer_hw_id;
	uint16_t peer_protocol_version;
	uint16_t negotiated_ucp_version;
	uint16_t local_session_nonce;
	uint16_t peer_session_nonce;

	uint8_t peer_app_count;
	ucp_ctrl_app_entry_t peer_apps[UCP_CTRL_CAP_MAX_ENTRIES];
	uint8_t negotiated_app_count;
	ucp_ctrl_app_entry_t negotiated_apps[UCP_CTRL_CAP_MAX_ENTRIES];

	bool peer_capability_received;
	bool cap_exchange_in_flight;
	bool link_ready_pending;
	bool link_ready_need_rsp;
	uint16_t pending_link_ready_version;

	uint32_t last_rx_ms;
	uint32_t last_progress_ms;
	uint32_t last_hello_tx_ms;
	uint32_t last_hs_tx_ms;
	uint32_t last_cap_req_tx_ms;
	uint32_t handshake_enter_ms;
	uint32_t last_keepalive_tx_ms;
	uint32_t discover_quiet_until_ms;
} ucp_session_t;

void ucp_session_init(ucp_session_t *session);
void ucp_session_tick(ucp_session_t *session, uint32_t now_ms);
void ucp_session_on_packet(ucp_session_t *session, const ucp_pkt_t *pkt);
bool ucp_session_is_ready(const ucp_session_t *session);
ucp_link_state_t ucp_session_get_link_state(const ucp_session_t *session);
bool ucp_session_has_peer_capability(const ucp_session_t *session);
ucp_hw_id_t ucp_session_get_peer_hw_id(const ucp_session_t *session);
uint16_t ucp_session_get_peer_protocol_version(const ucp_session_t *session);
bool ucp_session_peer_supports(const ucp_session_t *session, uint8_t app_id, uint16_t min_version, uint8_t required_roles);
uint8_t ucp_session_get_negotiated_app_count(const ucp_session_t *session);
void ucp_session_on_cap_frame_rx_failed(ucp_session_t *session);

#endif /* UCP_SESSION_H_ */
