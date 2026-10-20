/*
 * Diagnostic link: ISO-TP + UDS via unmodified iso14229 (isotp-c embedded).
 *
 * Poll cadence: call diag_link_poll() every 1 ms from example glue so STmin
 * of 0–1 ms can progress. A 10 ms vehicle-comms tick will miss short STmin.
 *
 * Only this module includes iso14229.h. Vehicle-specific K-Line framing stays out.
 *
 * Compile with: -DUDS_SYS=0  (UDS_SYS_CUSTOM) when building iso14229.c
 */
#ifndef DIAG_LINK_H_
#define DIAG_LINK_H_

#include <stdbool.h>
#include <stdint.h>

#include "vcom_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct diag_link diag_link_t;

typedef struct {
	vcom_core_t *core;
	uint8_t bus;
	uint32_t phys_req_id;  /* client TX / server RX, e.g. 0x7E0 */
	uint32_t phys_resp_id; /* client RX / server TX, e.g. 0x7E8 */
	uint32_t func_req_id;  /* functional request id, e.g. 0x7DF; 0 = unused */
	uint32_t (*now_ms)(void);
} diag_link_cfg_t;

/* Single-instance link (isotp user shims are process-global). */
diag_link_t *diag_link_instance(void);

/* Shared UDSMillis clock when no client link is active (e.g. UDS server only). */
void diag_uds_set_clock(uint32_t (*now_ms)(void));

bool diag_link_init(diag_link_t *link, const diag_link_cfg_t *cfg);
void diag_link_deinit(diag_link_t *link);

/* RX subscription helper: register for phys_resp_id on cfg.bus. */
bool diag_link_subscribe_rx(diag_link_t *link);

void diag_link_on_can_rx(diag_link_t *link, const vcom_can_frame_t *fr);

/* Advance ISO-TP + UDS. Prefer 1 ms. Returns true if a request is in flight. */
bool diag_link_poll(diag_link_t *link);

bool diag_link_session_default(diag_link_t *link);
bool diag_link_tester_present(diag_link_t *link);
bool diag_link_read_did(diag_link_t *link, const uint16_t *dids, uint16_t count);

/* Opaque UDSClient_t* for advanced callers. */
void *diag_link_client(diag_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_LINK_H_ */
