/*
 * UCP telemetry application: TLV publisher + GET_DIRECTORY for demo signals.
 * APP_ID 0x20, version 0x0100 (V1.0).
 */
#ifndef UCP_TLM_SIG_H_
#define UCP_TLM_SIG_H_

#include "sig_table.h"
#include "ucp_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCP_TLM_SIG_APP_ID 0x20u
#define UCP_TLM_SIG_VERSION 0x0100u

#define UCP_TLM_PKT_LIVE 0x10u
#define UCP_TLM_PKT_HF 0x11u
#define UCP_TLM_PKT_MF 0x12u
#define UCP_TLM_PKT_LF 0x13u
#define UCP_TLM_PKT_CHANGE 0x14u

#define UCP_TLM_OP_GET_DIRECTORY 0x01u

/* Band periods (ms). FS CDC + forever-armed OUT should clear these easily. */
#define UCP_TLM_PERIOD_LIVE_MS 20u
#define UCP_TLM_PERIOD_HF_MS 40u
#define UCP_TLM_PERIOD_MF_MS 100u
#define UCP_TLM_PERIOD_LF_MS 500u
/* Max UCP frames enqueued from tlm_on_tick per call (multipart headroom). */
#define UCP_TLM_MAX_FRAMES_PER_TICK 16u

extern const ucp_app_vtable_t g_ucp_tlm_sig_app_vtable;

void ucp_tlm_sig_submit(const sig_snapshot_t *snap);

/* Optional live table for CHANGE dirty clear after send. */
void ucp_tlm_sig_attach_table(sig_table_t *table);

/* Encode one frame for a rate (legacy single-frame helper). */
uint8_t ucp_tlm_sig_encode_rate(const sig_snapshot_t *snap, uint8_t rate,
								uint8_t *dst, uint8_t cap);

/*
 * Multipart encode: write part_index into dst (gen|part|part_count|TLVs).
 * *io_slot is the next snapshot record index to consider; advanced on return.
 * Returns bytes used, or 0 if no more TLVs for this rate.
 */
uint8_t ucp_tlm_sig_encode_rate_part(const sig_snapshot_t *snap, uint8_t rate,
									 uint8_t part_index, uint8_t part_count,
									 uint8_t *io_slot, uint8_t *dst, uint8_t cap);

/* Count how many multipart frames are needed for a rate (cap parts at max_parts). */
uint8_t ucp_tlm_sig_count_parts(const sig_snapshot_t *snap, uint8_t rate,
								uint8_t max_parts);

/* Build full directory blob for xfer offer. Returns length, or 0 on error. */
uint16_t ucp_tlm_sig_directory_build(const sig_snapshot_t *snap, uint8_t *dst,
									 uint16_t dst_cap);

#ifdef __cplusplus
}
#endif

#endif /* UCP_TLM_SIG_H_ */
