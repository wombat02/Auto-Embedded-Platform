#include "ucp_tlm_sig.h"

#include "sig_catalog.h"
#include "sig_tlv.h"
#include "ucp_protocol.h"
#include "ucp_xfer.h"

#include <string.h>

#define TLM_PART_HDR 3u /* gen | part_index | part_count */

static sig_snapshot_t s_snap;
static uint8_t s_have_snap;
static sig_table_t *s_live_table;

static uint32_t s_last_live_ms;
static uint32_t s_last_hf_ms;
static uint32_t s_last_mf_ms;
static uint32_t s_last_lf_ms;
static uint8_t s_frames_budget;

void ucp_tlm_sig_submit(const sig_snapshot_t *snap)
{
	if (snap == NULL)
	{
		return;
	}
	s_snap = *snap;
	s_have_snap = 1u;
}

void ucp_tlm_sig_attach_table(sig_table_t *table)
{
	s_live_table = table;
}

static uint8_t rec_include(const sig_snapshot_t *snap, uint8_t i, uint8_t rate)
{
	if (snap->rec[i].rate != rate)
	{
		return 0u;
	}
	if (snap->rec[i].publish == 0u)
	{
		return 0u;
	}
	if ((snap->rec[i].valid == 0u) && (snap->rec[i].aged == 0u))
	{
		return 0u;
	}
	if ((rate == SIG_RATE_CHANGE) && (snap->rec[i].dirty == 0u))
	{
		return 0u;
	}
	return 1u;
}

static uint8_t tlv_qflags(const sig_snapshot_t *snap, uint8_t i)
{
	return (snap->rec[i].valid != 0u) ? SIG_TLV_Q_VALID : SIG_TLV_Q_AGED;
}

uint8_t ucp_tlm_sig_encode_rate(const sig_snapshot_t *snap, uint8_t rate,
								uint8_t *dst, uint8_t cap)
{
	uint8_t slot = 0u;
	return ucp_tlm_sig_encode_rate_part(snap, rate, 0u, 1u, &slot, dst, cap);
}

uint8_t ucp_tlm_sig_encode_rate_part(const sig_snapshot_t *snap, uint8_t rate,
									 uint8_t part_index, uint8_t part_count,
									 uint8_t *io_slot, uint8_t *dst, uint8_t cap)
{
	uint8_t used;
	uint8_t i;
	uint8_t wrote_any = 0u;

	if ((snap == NULL) || (dst == NULL) || (io_slot == NULL) || (cap < TLM_PART_HDR))
	{
		return 0u;
	}

	dst[0] = snap->generation;
	dst[1] = part_index;
	dst[2] = part_count;
	used = TLM_PART_HDR;

	for (i = *io_slot; i < snap->count; i++)
	{
		uint8_t wrote;

		if (rec_include(snap, i, rate) == 0u)
		{
			continue;
		}
		wrote = sig_tlv_append(dst, cap, used,
							   snap->rec[i].id,
							   snap->rec[i].type,
							   tlv_qflags(snap, i),
							   snap->rec[i].raw_bits);
		if (wrote == 0u)
		{
			break;
		}
		used = (uint8_t)(used + wrote);
		wrote_any = 1u;
	}
	*io_slot = i;
	if (wrote_any == 0u)
	{
		return 0u;
	}
	return used;
}

uint8_t ucp_tlm_sig_count_parts(const sig_snapshot_t *snap, uint8_t rate,
								uint8_t max_parts)
{
	uint8_t slot = 0u;
	uint8_t parts = 0u;
	uint8_t frame[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];

	if ((snap == NULL) || (max_parts == 0u))
	{
		return 0u;
	}
	while (parts < max_parts)
	{
		uint8_t before = slot;
		uint8_t len = ucp_tlm_sig_encode_rate_part(snap, rate, parts, max_parts, &slot, frame,
													sizeof(frame));
		if (len == 0u)
		{
			break;
		}
		parts++;
		if (slot == before)
		{
			break;
		}
		if (slot >= snap->count)
		{
			break;
		}
	}
	return parts;
}

uint16_t ucp_tlm_sig_directory_build(const sig_snapshot_t *snap, uint8_t *dst,
									 uint16_t dst_cap)
{
	uint16_t used;
	uint8_t i;
	uint8_t rec_count = 0u;

	if ((snap == NULL) || (dst == NULL) || (dst_cap < 6u))
	{
		return 0u;
	}

	dst[0] = snap->generation;
	dst[1] = 0u; /* reserved / single blob */
	dst[2] = 1u; /* chunk_count = 1 (compat layout) */
	dst[3] = 0u; /* rec_count filled below */
	dst[4] = (uint8_t)(SIG_CATALOG_REVISION & 0xFFu);
	dst[5] = (uint8_t)((SIG_CATALOG_REVISION >> 8) & 0xFFu);
	used = 6u;

	for (i = 0u; i < snap->count; i++)
	{
		uint8_t flags = 0u;

		if (snap->rec[i].publish == 0u)
		{
			continue;
		}
		if ((uint16_t)(used + 6u) > dst_cap)
		{
			return 0u;
		}
		flags |= 0x01u;
		dst[used + 0u] = 6u;
		dst[used + 1u] = (uint8_t)(snap->rec[i].id & 0xFFu);
		dst[used + 2u] = (uint8_t)((snap->rec[i].id >> 8) & 0xFFu);
		dst[used + 3u] = snap->rec[i].type;
		dst[used + 4u] = snap->rec[i].rate;
		dst[used + 5u] = flags;
		used = (uint16_t)(used + 6u);
		rec_count++;
	}
	dst[3] = rec_count;
	return used;
}

static void clear_change_dirty(void)
{
	uint8_t i;

	if (s_live_table == NULL)
	{
		return;
	}
	for (i = 0u; i < s_snap.count; i++)
	{
		if ((s_snap.rec[i].rate == SIG_RATE_CHANGE) && (s_snap.rec[i].dirty != 0u) &&
			(s_snap.rec[i].publish != 0u))
		{
			sig_table_clear_dirty(s_live_table, s_snap.rec[i].id);
		}
	}
}

static bool send_band_parts(uint8_t rate, uint8_t pkt_id)
{
	uint8_t frame[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t parts;
	uint8_t p;
	uint8_t slot = 0u;

	if (s_frames_budget == 0u)
	{
		return false;
	}

	parts = ucp_tlm_sig_count_parts(&s_snap, rate, 8u);
	if (parts == 0u)
	{
		return true; /* nothing to send — treat as success (not a TX failure) */
	}
	for (p = 0u; p < parts; p++)
	{
		uint8_t len;

		if (s_frames_budget == 0u)
		{
			return false;
		}
		len = ucp_tlm_sig_encode_rate_part(&s_snap, rate, p, parts, &slot, frame,
										   sizeof(frame));
		if (len <= TLM_PART_HDR)
		{
			return false;
		}
		if (!ucp_send_pkt(UCP_TLM_SIG_APP_ID, pkt_id, frame, len))
		{
			/* TX full: retry band next due (last_ms not advanced by caller). */
			return false;
		}
		s_frames_budget--;
	}
	return true;
}

static void tlm_init(void)
{
	memset(&s_snap, 0, sizeof(s_snap));
	s_have_snap = 0u;
	s_live_table = NULL;
	s_last_live_ms = 0u;
	s_last_hf_ms = 0u;
	s_last_mf_ms = 0u;
	s_last_lf_ms = 0u;
	s_frames_budget = 0u;
}

/* BSS — never put UCP_XFER_MAX_SDU (1 KiB) on the com/USB call stack. */
static uint8_t s_dir_buf[UCP_XFER_MAX_SDU];

static void tlm_on_rx(const ucp_pkt_t *pkt)
{
	uint16_t corr;
	uint8_t opcode;
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint16_t dir_len;
	uint16_t token;
	uint8_t rsp_len;

	if ((pkt == NULL) || (pkt->pkt_id != (uint8_t)UCP_SERVICE_PKT_ID_REQ))
	{
		return;
	}
	if (pkt->payload_len < 4u)
	{
		return;
	}
	if (!s_have_snap)
	{
		return;
	}

	corr = (uint16_t)pkt->payload[0] | ((uint16_t)pkt->payload[1] << 8);
	opcode = pkt->payload[2];

	if (opcode != UCP_TLM_OP_GET_DIRECTORY)
	{
		rsp[0] = (uint8_t)(corr & 0xFFu);
		rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
		rsp[2] = opcode;
		rsp[3] = (uint8_t)UCP_SERVICE_STATUS_UNSUPPORTED_OPCODE;
		(void)ucp_send_pkt(UCP_TLM_SIG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, 4u);
		return;
	}

	dir_len = ucp_tlm_sig_directory_build(&s_snap, s_dir_buf, (uint16_t)sizeof(s_dir_buf));
	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = opcode;
	if (dir_len == 0u)
	{
		rsp[3] = (uint8_t)UCP_SERVICE_STATUS_BACKEND_ERROR;
		rsp_len = 4u;
		(void)ucp_send_pkt(UCP_TLM_SIG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, rsp_len);
		return;
	}
	if (!ucp_xfer_offer(&token, s_dir_buf, dir_len))
	{
		rsp[3] = (uint8_t)UCP_SERVICE_STATUS_BUSY;
		rsp_len = 4u;
		(void)ucp_send_pkt(UCP_TLM_SIG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, rsp_len);
		return;
	}
	rsp[3] = (uint8_t)UCP_SERVICE_STATUS_OK;
	rsp[4] = UCP_XFER_SVC_FLAG_OFFERED;
	rsp[5] = (uint8_t)(token & 0xFFu);
	rsp[6] = (uint8_t)((token >> 8) & 0xFFu);
	rsp[7] = (uint8_t)(dir_len & 0xFFu);
	rsp[8] = (uint8_t)((dir_len >> 8) & 0xFFu);
	rsp_len = 9u;
	(void)ucp_send_pkt(UCP_TLM_SIG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, rsp_len);
}

static bool band_due(uint32_t now_ms, uint32_t last_ms, uint32_t period_ms)
{
	if (last_ms == 0u)
	{
		return true;
	}
	return (uint32_t)(now_ms - last_ms) >= period_ms;
}

static void tlm_on_tick(uint32_t now_ms, bool link_ready)
{
	if (!link_ready || !s_have_snap)
	{
		return;
	}

	/*
	 * Phase-stagger first due times so LIVE/HF/MF/LF do not all arm together
	 * (LCM 10/20/100/500 = 500 ms). last = now - (period - phase).
	 */
	if (s_last_live_ms == 0u)
	{
		s_last_live_ms = now_ms;
		s_last_hf_ms = now_ms - (UCP_TLM_PERIOD_HF_MS - 10u);
		s_last_mf_ms = now_ms - (UCP_TLM_PERIOD_MF_MS - 30u);
		s_last_lf_ms = now_ms - (UCP_TLM_PERIOD_LF_MS - 50u);
	}

	s_frames_budget = UCP_TLM_MAX_FRAMES_PER_TICK;

	/*
	 * Send every due band within the frame budget. Do not return after LIVE:
	 * if LIVE enqueue fails, last_live stays stale and LIVE stays "due" — an
	 * early return would starve HF/MF/LF forever (only RPM / LIVE survives).
	 */
	if (band_due(now_ms, s_last_live_ms, UCP_TLM_PERIOD_LIVE_MS))
	{
		if (send_band_parts(SIG_RATE_LIVE, UCP_TLM_PKT_LIVE))
		{
			s_last_live_ms = now_ms;
		}
	}
	if ((s_frames_budget > 0u) && band_due(now_ms, s_last_hf_ms, UCP_TLM_PERIOD_HF_MS))
	{
		if (send_band_parts(SIG_RATE_HF, UCP_TLM_PKT_HF))
		{
			s_last_hf_ms = now_ms;
		}
	}
	if ((s_frames_budget > 0u) && band_due(now_ms, s_last_mf_ms, UCP_TLM_PERIOD_MF_MS))
	{
		if (send_band_parts(SIG_RATE_MF, UCP_TLM_PKT_MF))
		{
			s_last_mf_ms = now_ms;
		}
	}
	if ((s_frames_budget > 0u) && band_due(now_ms, s_last_lf_ms, UCP_TLM_PERIOD_LF_MS))
	{
		if (send_band_parts(SIG_RATE_LF, UCP_TLM_PKT_LF))
		{
			s_last_lf_ms = now_ms;
		}
	}
	if (s_frames_budget > 0u)
	{
		if (send_band_parts(SIG_RATE_CHANGE, UCP_TLM_PKT_CHANGE))
		{
			clear_change_dirty();
		}
	}
}

static void tlm_on_link_event(ucp_app_link_event_t event)
{
	(void)event;
	s_last_live_ms = 0u;
	s_last_hf_ms = 0u;
	s_last_mf_ms = 0u;
	s_last_lf_ms = 0u;
}

const ucp_app_vtable_t g_ucp_tlm_sig_app_vtable = {
	.app_id = UCP_TLM_SIG_APP_ID,
	.version = UCP_TLM_SIG_VERSION,
	.role_flags = (uint8_t)(UCP_APP_ROLE_TX | UCP_APP_ROLE_REQ_RSP),
	.init = tlm_init,
	.on_rx = tlm_on_rx,
	.on_tick = tlm_on_tick,
	.on_link_event = tlm_on_link_event,
};
