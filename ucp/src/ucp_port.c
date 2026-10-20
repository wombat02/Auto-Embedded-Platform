/*
 * Weak defaults for UCP platform hooks, frame helper, and role negotiation.
 */
#include "ucp_port.h"

#include <string.h>

static const ucp_port_ops_t *s_ops;

void ucp_port_set_ops(const ucp_port_ops_t *ops)
{
	s_ops = ops;
}

const ucp_port_ops_t *ucp_port_get_ops(void)
{
	return s_ops;
}

#if defined(__GNUC__) || defined(__clang__)
#define UCP_WEAK __attribute__((weak))
#else
#define UCP_WEAK
#endif

UCP_WEAK uint32_t ucp_port_get_time_ms(void)
{
	if ((s_ops != NULL) && (s_ops->get_time_ms != NULL))
	{
		return s_ops->get_time_ms(s_ops->user);
	}
	return 0u;
}

UCP_WEAK bool ucp_port_send_frame(const uint8_t *frame, uint16_t len)
{
	if ((s_ops != NULL) && (s_ops->send_frame != NULL))
	{
		return s_ops->send_frame(frame, len, s_ops->user);
	}
	(void)frame;
	(void)len;
	return false;
}

bool ucp_send_pkt(uint8_t app_id,
				  uint8_t pkt_id,
				  const void *payload,
				  uint8_t payload_len)
{
	uint8_t frame[UCP_V1_FRAME_OVERHEAD_LEN_BYTES + UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t idx = 0u;

	if (payload_len > UCP_PKT_MAX_PAYLOAD_LEN_BYTES)
	{
		return false;
	}

	frame[idx++] = UCP_PKT_SOF;
	frame[idx++] = app_id;
	frame[idx++] = pkt_id;
	frame[idx++] = payload_len;

	if ((payload_len > 0u) && (payload != NULL))
	{
		memcpy(&frame[idx], payload, payload_len);
	}
	idx = (uint8_t)(idx + payload_len);

	frame[idx++] = ucp_calc_checksum(&frame[1], (uint8_t)(3u + payload_len));
	return ucp_port_send_frame(frame, idx);
}

UCP_WEAK bool ucp_port_send(const ucp_pkt_t *pkt)
{
	if (pkt == NULL)
	{
		return false;
	}
	return ucp_send_pkt(pkt->app_id, pkt->pkt_id, pkt->payload, pkt->payload_len);
}

UCP_WEAK void ucp_port_tx_flush(void)
{
}

UCP_WEAK void ucp_port_cap_tx_guard(void)
{
}

UCP_WEAK bool ucp_port_handshake_tx_cap_allowed(void)
{
	return true;
}

UCP_WEAK void ucp_port_peer_turnaround_delay(void)
{
}

UCP_WEAK void ucp_port_log(ucp_log_level_t level, const char *msg)
{
	(void)level;
	(void)msg;
}

UCP_WEAK ucp_hw_id_t ucp_port_get_local_hw_id(void)
{
	if ((s_ops != NULL) && (s_ops->get_hw_id != NULL))
	{
		return s_ops->get_hw_id(s_ops->user);
	}
	return UCP_HW_ID_UNKNOWN;
}

UCP_WEAK uint16_t ucp_port_get_protocol_version(void)
{
	return UCP_PROTOCOL_VERSION;
}

UCP_WEAK uint8_t ucp_port_get_app_descriptors(ucp_app_version_entry_t *out_entries,
											  uint8_t max_entries)
{
	if ((s_ops != NULL) && (s_ops->get_app_descriptors != NULL))
	{
		return s_ops->get_app_descriptors(out_entries, max_entries, s_ops->user);
	}
	(void)out_entries;
	(void)max_entries;
	return 0u;
}

UCP_WEAK uint8_t ucp_port_get_app_registry(const ucp_app_vtable_t **out_registry,
										   uint8_t max_entries)
{
	(void)out_registry;
	(void)max_entries;
	return 0u;
}

bool ucp_app_roles_negotiate(uint8_t local_roles,
							 uint8_t peer_roles,
							 uint8_t *out_negotiated_roles,
							 bool *out_warn_duplicate_roles)
{
	const bool local_req = ((local_roles & (uint8_t)UCP_APP_ROLE_REQ_RSP) != 0u);
	const bool peer_req = ((peer_roles & (uint8_t)UCP_APP_ROLE_REQ_RSP) != 0u);
	const bool local_tx = ((local_roles & (uint8_t)UCP_APP_ROLE_TX) != 0u);
	const bool local_rx = ((local_roles & (uint8_t)UCP_APP_ROLE_RX) != 0u);
	const bool peer_tx = ((peer_roles & (uint8_t)UCP_APP_ROLE_TX) != 0u);
	const bool peer_rx = ((peer_roles & (uint8_t)UCP_APP_ROLE_RX) != 0u);
	const bool dup_tx = (local_tx && peer_tx);
	const bool dup_rx = (local_rx && peer_rx);
	bool complementary = false;

	if ((out_negotiated_roles == NULL) || (out_warn_duplicate_roles == NULL))
	{
		return false;
	}

	*out_negotiated_roles = 0u;
	*out_warn_duplicate_roles = false;

	/* One side REQ_RSP without the other: not a usable service match. */
	if (local_req != peer_req)
	{
		return false;
	}

	if (local_req && peer_req)
	{
		*out_negotiated_roles |= (uint8_t)UCP_APP_ROLE_REQ_RSP;
	}

	if (local_tx && peer_rx)
	{
		*out_negotiated_roles |= (uint8_t)UCP_APP_ROLE_TX;
		complementary = true;
	}
	if (local_rx && peer_tx)
	{
		*out_negotiated_roles |= (uint8_t)UCP_APP_ROLE_RX;
		complementary = true;
	}

	if (*out_negotiated_roles != 0u)
	{
		/*
		 * Warn only when same-direction overlap is the only overlap.
		 * REQ_RSP success or complementary TX/RX means do not warn.
		 */
		return true;
	}

	if (dup_tx || dup_rx)
	{
		*out_warn_duplicate_roles = true;
		if (dup_tx)
		{
			*out_negotiated_roles |= (uint8_t)UCP_APP_ROLE_TX;
		}
		if (dup_rx)
		{
			*out_negotiated_roles |= (uint8_t)UCP_APP_ROLE_RX;
		}
	}

	(void)complementary;
	return (*out_negotiated_roles != 0u);
}

UCP_WEAK bool ucp_port_validate_peer_apps(const ucp_ctrl_app_entry_t *peer_entries,
										  uint8_t entry_count)
{
	(void)peer_entries;
	(void)entry_count;
	return true;
}

UCP_WEAK void ucp_port_apps_init(void)
{
}

UCP_WEAK void ucp_port_apps_tick(uint32_t now_ms, bool link_ready, uint32_t scheduler_flags)
{
	(void)now_ms;
	(void)link_ready;
	(void)scheduler_flags;
}

UCP_WEAK void ucp_port_route_application_packet(const ucp_pkt_t *pkt)
{
	(void)pkt;
}

UCP_WEAK void ucp_port_on_link_event(ucp_app_link_event_t event)
{
	(void)event;
}
