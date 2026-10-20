/*
 * UCP transport transfer channel (APP 0x01): pull offers + push SDUs.
 * Available whenever the link is READY; not advertised in CAP.
 */
#ifndef UCP_XFER_H_
#define UCP_XFER_H_

#include <stdbool.h>
#include <stdint.h>

#include "ucp_port.h"
#include "ucp_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCP_XFER_VERSION 0x0100u /* V1.0 — high byte major, low byte minor */

#define UCP_XFER_MAX_SDU 1024u
#define UCP_XFER_PUSH_WINDOW 4u
#define UCP_XFER_PUSH_ACK_TIMEOUT_MS 500u

#define UCP_XFER_PKT_INFO_REQ 0x01u
#define UCP_XFER_PKT_INFO_RSP 0x02u
#define UCP_XFER_PKT_CHUNK_REQ 0x03u
#define UCP_XFER_PKT_CHUNK_RSP 0x04u
#define UCP_XFER_PKT_ABORT 0x05u
#define UCP_XFER_PKT_PUSH_DATA 0x10u
#define UCP_XFER_PKT_PUSH_ACK 0x11u

#define UCP_XFER_FLAG_FIRST 0x01u
#define UCP_XFER_FLAG_CONT 0x02u
#define UCP_XFER_FLAG_LAST 0x04u

#define UCP_XFER_ACK_OK 0u
#define UCP_XFER_ACK_BUSY 1u
#define UCP_XFER_ACK_OVERFLOW 2u
#define UCP_XFER_ACK_BAD_SEQ 3u
#define UCP_XFER_ACK_ABORTED 4u

/* Short service RSP flag: body continues via pull offer. */
#define UCP_XFER_SVC_FLAG_OFFERED 0x01u

typedef void (*ucp_xfer_push_rx_cb_t)(uint16_t token, const uint8_t *data, uint16_t len,
									  void *user);

extern const ucp_app_vtable_t g_ucp_xfer_app_vtable;

void ucp_xfer_init(void);
void ucp_xfer_on_tick(uint32_t now_ms, bool link_ready);
void ucp_xfer_on_link_event(ucp_app_link_event_t event);

/* Pull producer: copy SDU into the single offer slot. */
bool ucp_xfer_offer(uint16_t *token, const uint8_t *data, uint16_t len);
bool ucp_xfer_offer_active(void);

/* Push TX: copy SDU and segment with windowed ACK. */
bool ucp_xfer_push_begin(uint16_t *token, const uint8_t *data, uint16_t len);
bool ucp_xfer_push_done(uint16_t token);
bool ucp_xfer_push_busy(void);

/* Push RX: one complete SDU callback. */
void ucp_xfer_set_push_rx_cb(ucp_xfer_push_rx_cb_t cb, void *user);

/* Max data bytes in one CHUNK_RSP / PUSH_DATA payload. */
uint8_t ucp_xfer_chunk_data_cap(void);
uint8_t ucp_xfer_push_data_cap(void);

#ifdef __cplusplus
}
#endif

#endif /* UCP_XFER_H_ */
