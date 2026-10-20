/*
 * Platform hook contract for integrating portable UCP core.
 *
 * Host tests may call ucp_port_set_ops() to install per-peer send/time/descriptor
 * hooks. Firmware may leave ops NULL and override the weak defaults instead.
 */
#ifndef UCP_PORT_H_
#define UCP_PORT_H_

#include <stdbool.h>
#include <stdint.h>

#include "ucp_protocol.h"
#include "ucp_log.h"

#define UCP_PORT_MAX_APP_DESCRIPTORS UCP_CTRL_CAP_MAX_ENTRIES

typedef struct
{
	uint8_t app_id;
	uint16_t version;
	uint8_t role_flags;
} ucp_app_version_entry_t;

typedef enum
{
	UCP_APP_ROLE_TX = (1u << 0),
	UCP_APP_ROLE_RX = (1u << 1),
	UCP_APP_ROLE_REQ_RSP = (1u << 2)
} ucp_app_role_flags_t;

/**
 * Compute negotiated role flags for a matching app_id/version pair (local perspective).
 *
 * If both peers advertise REQ_RSP:
 *   - set REQ_RSP
 *   - also apply complementary TX/RX (local TX+peer RX => TX; local RX+peer TX => RX)
 *
 * Without REQ_RSP on either side: complementary TX/RX only.
 *
 * Same-direction overlap (both TX or both RX) warns only when that is the only
 * usable overlap (no complementary direction and no REQ_RSP negotiated).
 *
 * Returns false when no usable role combination exists.
 */
bool ucp_app_roles_negotiate(uint8_t local_roles,
							 uint8_t peer_roles,
							 uint8_t *out_negotiated_roles,
							 bool *out_warn_duplicate_roles);

typedef enum
{
	UCP_APP_LINK_EVENT_LINK_READY = 0,
	UCP_APP_LINK_EVENT_TIMEOUT,
	UCP_APP_LINK_EVENT_DISCOVERING
} ucp_app_link_event_t;

typedef struct
{
	uint8_t app_id;
	uint16_t version;
	uint8_t role_flags;
	void (*init)(void);
	void (*on_rx)(const ucp_pkt_t *pkt);
	void (*on_tick)(uint32_t now_ms, bool link_ready);
	void (*on_link_event)(ucp_app_link_event_t event);
} ucp_app_vtable_t;

/* Optional host/test binding. When set, overrides weak defaults. */
typedef struct
{
	uint32_t (*get_time_ms)(void *user);
	bool (*send_frame)(const uint8_t *frame, uint16_t len, void *user);
	ucp_hw_id_t (*get_hw_id)(void *user);
	uint8_t (*get_app_descriptors)(ucp_app_version_entry_t *out_entries,
								   uint8_t max_entries,
								   void *user);
	void *user;
} ucp_port_ops_t;

void ucp_port_set_ops(const ucp_port_ops_t *ops);
const ucp_port_ops_t *ucp_port_get_ops(void);

uint32_t ucp_port_get_time_ms(void);

static inline uint32_t ucp_elapsed_ms(uint32_t now_ms, uint32_t then_ms)
{
	return (uint32_t)(now_ms - then_ms);
}

static inline bool ucp_elapsed_ge_ms(uint32_t now_ms, uint32_t then_ms, uint32_t limit_ms)
{
	return ucp_elapsed_ms(now_ms, then_ms) >= limit_ms;
}

static inline bool ucp_elapsed_gt_ms(uint32_t now_ms, uint32_t then_ms, uint32_t limit_ms)
{
	return ucp_elapsed_ms(now_ms, then_ms) > limit_ms;
}

static inline bool ucp_time_before_deadline_ms(uint32_t now_ms, uint32_t deadline_ms)
{
	return (int32_t)(now_ms - deadline_ms) < 0;
}

/** Primary send hook: framed bytes. Weak default returns false. */
bool ucp_port_send_frame(const uint8_t *frame, uint16_t len);

/** Build a V1 frame and call ucp_port_send_frame (or ops->send_frame). */
bool ucp_send_pkt(uint8_t app_id,
				  uint8_t pkt_id,
				  const void *payload,
				  uint8_t payload_len);

/** Packet-level send used by the session. Default frames via ucp_send_pkt path. */
bool ucp_port_send(const ucp_pkt_t *pkt);

void ucp_port_tx_flush(void);
void ucp_port_cap_tx_guard(void);
bool ucp_port_handshake_tx_cap_allowed(void);
void ucp_port_peer_turnaround_delay(void);

ucp_hw_id_t ucp_port_get_local_hw_id(void);
uint16_t ucp_port_get_protocol_version(void);
uint8_t ucp_port_get_app_descriptors(ucp_app_version_entry_t *out_entries, uint8_t max_entries);
uint8_t ucp_port_get_app_registry(const ucp_app_vtable_t **out_registry, uint8_t max_entries);
bool ucp_port_validate_peer_apps(const ucp_ctrl_app_entry_t *peer_entries, uint8_t entry_count);
void ucp_port_apps_init(void);
void ucp_port_apps_tick(uint32_t now_ms, bool link_ready, uint32_t scheduler_flags);
void ucp_port_route_application_packet(const ucp_pkt_t *pkt);
void ucp_port_on_link_event(ucp_app_link_event_t event);

#endif /* UCP_PORT_H_ */
