/*
 * UDS server over vcom + iso14229 (example / product glue companion to diag_link client).
 * Poll every 1 ms via diag_server_poll().
 */
#ifndef DIAG_SERVER_H_
#define DIAG_SERVER_H_

#include <stdbool.h>
#include <stdint.h>

#include "vcom_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct diag_server diag_server_t;

typedef struct {
	vcom_core_t *core;
	uint8_t bus;
	uint32_t phys_req_id;  /* server RX, e.g. 0x7E0 */
	uint32_t phys_resp_id; /* server TX, e.g. 0x7E8 */
	uint32_t func_req_id;  /* 0x7DF or 0 */
	uint32_t (*now_ms)(void);
	/* Optional RDBI provider: return true and fill *out_* for known DID. */
	bool (*rdbi)(uint16_t did, const uint8_t **data, uint16_t *len, void *user);
	void *rdbi_user;
} diag_server_cfg_t;

diag_server_t *diag_server_instance(void);
/* Second static instance for dual-bus example boards (optional). */
diag_server_t *diag_server_instance_1(void);
bool diag_server_init(diag_server_t *srv, const diag_server_cfg_t *cfg);
void diag_server_deinit(diag_server_t *srv);
bool diag_server_subscribe_rx(diag_server_t *srv);
void diag_server_on_can_rx(diag_server_t *srv, const vcom_can_frame_t *fr);
void diag_server_poll(diag_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_SERVER_H_ */
