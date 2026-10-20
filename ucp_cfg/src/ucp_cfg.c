#include "ucp_cfg.h"

#include "aep_log.h"
#include "store.h"
#include "ucp_protocol.h"

#include <string.h>

static ucp_cfg_bind_t s_bind;
static uint8_t s_bound;

void ucp_cfg_bind(const ucp_cfg_bind_t *bind)
{
	if (bind == NULL)
	{
		s_bound = 0u;
		return;
	}
	s_bind = *bind;
	s_bound = 1u;
}

void ucp_cfg_set_ads(const ucp_cfg_pack_ad_t *ads, uint8_t count)
{
	uint8_t n;

	if (!s_bound)
	{
		return;
	}
	n = (count <= UCP_CFG_MAX_ADS) ? count : UCP_CFG_MAX_ADS;
	if ((ads != NULL) && (n > 0u))
	{
		memcpy(s_bind.ads, ads, (size_t)n * sizeof(ucp_cfg_pack_ad_t));
	}
	s_bind.ad_count = n;
}

static void send_status(uint16_t corr, uint8_t opcode, uint8_t status)
{
	uint8_t rsp[4];

	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = opcode;
	rsp[3] = status;
	(void)ucp_send_pkt(UCP_CFG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, 4u);
}

static void op_list_descs(uint16_t corr)
{
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t used = 4u;
	uint8_t count_at;
	uint8_t packed = 0u;
	uint8_t i;
	uint8_t n;

	if (!s_bound || (s_bind.rt == NULL))
	{
		send_status(corr, UCP_CFG_OP_LIST_DESCS, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = UCP_CFG_OP_LIST_DESCS;
	rsp[3] = (uint8_t)UCP_SERVICE_STATUS_OK;
	n = comp_runtime_desc_count(s_bind.rt);
	if (used >= sizeof(rsp))
	{
		send_status(corr, UCP_CFG_OP_LIST_DESCS, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	/* Placeholder — overwritten with how many entries actually fit in 64 B. */
	count_at = used;
	rsp[used++] = 0u;
	for (i = 0u; i < n; i++)
	{
		const comp_desc_t *d = comp_runtime_desc_at(s_bind.rt, i);
		uint8_t name_len = 0u;

		if (d == NULL)
		{
			continue;
		}
		if (d->name != NULL)
		{
			/* Cap so typical 4 demo descs fit in one UCP payload. */
			while ((d->name[name_len] != '\0') && (name_len < 8u))
			{
				name_len++;
			}
		}
		if ((uint16_t)used + 6u + name_len > sizeof(rsp))
		{
			break;
		}
		rsp[used++] = (uint8_t)(d->id & 0xFFu);
		rsp[used++] = (uint8_t)((d->id >> 8) & 0xFFu);
		rsp[used++] = (uint8_t)d->kind;
		rsp[used++] = (uint8_t)(d->max_cfg_len & 0xFFu);
		rsp[used++] = (uint8_t)((d->max_cfg_len >> 8) & 0xFFu);
		rsp[used++] = name_len;
		if (name_len > 0u)
		{
			memcpy(&rsp[used], d->name, name_len);
			used = (uint8_t)(used + name_len);
		}
		packed++;
	}
	rsp[count_at] = packed;
	(void)ucp_send_pkt(UCP_CFG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, used);
}

static void op_get_slots(uint16_t corr)
{
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t used = 4u;
	uint8_t i;

	if (!s_bound || (s_bind.specs == NULL) || (s_bind.spec_count == NULL))
	{
		send_status(corr, UCP_CFG_OP_GET_SLOTS, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = UCP_CFG_OP_GET_SLOTS;
	rsp[3] = (uint8_t)UCP_SERVICE_STATUS_OK;
	rsp[used++] = *s_bind.spec_count;
	for (i = 0u; i < *s_bind.spec_count; i++)
	{
		const comp_slot_spec_t *s = &s_bind.specs[i];

		if ((uint16_t)used + 4u + s->cfg_len > sizeof(rsp))
		{
			break;
		}
		rsp[used++] = (uint8_t)(s->comp_id & 0xFFu);
		rsp[used++] = (uint8_t)((s->comp_id >> 8) & 0xFFu);
		rsp[used++] = s->enabled;
		rsp[used++] = s->cfg_len;
		if (s->cfg_len > 0u)
		{
			memcpy(&rsp[used], s->cfg, s->cfg_len);
			used = (uint8_t)(used + s->cfg_len);
		}
	}
	(void)ucp_send_pkt(UCP_CFG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, used);
}

static void op_set_slot(uint16_t corr, const uint8_t *body, uint8_t body_len)
{
	uint16_t id;
	uint8_t enabled;
	uint8_t cfg_len;
	uint8_t i;
	comp_slot_spec_t *slot;

	if (!s_bound || (s_bind.specs == NULL) || (s_bind.spec_count == NULL) || (body_len < 4u))
	{
		send_status(corr, UCP_CFG_OP_SET_SLOT, (uint8_t)UCP_SERVICE_STATUS_INVALID_ARG);
		return;
	}
	id = (uint16_t)body[0] | ((uint16_t)body[1] << 8);
	enabled = body[2];
	cfg_len = body[3];
	if ((uint16_t)4u + cfg_len > body_len || cfg_len > COMP_CFG_MAX)
	{
		send_status(corr, UCP_CFG_OP_SET_SLOT, (uint8_t)UCP_SERVICE_STATUS_INVALID_ARG);
		return;
	}

	slot = NULL;
	for (i = 0u; i < *s_bind.spec_count; i++)
	{
		if (s_bind.specs[i].comp_id == id)
		{
			slot = &s_bind.specs[i];
			break;
		}
	}
	if (slot == NULL)
	{
		if (*s_bind.spec_count >= s_bind.spec_cap)
		{
			send_status(corr, UCP_CFG_OP_SET_SLOT, (uint8_t)UCP_SERVICE_STATUS_BUSY);
			return;
		}
		slot = &s_bind.specs[*s_bind.spec_count];
		(*s_bind.spec_count)++;
		memset(slot, 0, sizeof(*slot));
		slot->comp_id = id;
	}
	slot->enabled = enabled;
	slot->cfg_len = cfg_len;
	if (cfg_len > 0u)
	{
		memcpy(slot->cfg, &body[4], cfg_len);
	}
	send_status(corr, UCP_CFG_OP_SET_SLOT, (uint8_t)UCP_SERVICE_STATUS_OK);
}

static int encode_slots_blob(uint8_t *out, size_t cap, size_t *out_len)
{
	size_t used = 0u;
	uint8_t i;

	if ((out == NULL) || (out_len == NULL) || !s_bound)
	{
		return -1;
	}
	if (used + 1u > cap)
	{
		return -1;
	}
	out[used++] = *s_bind.spec_count;
	for (i = 0u; i < *s_bind.spec_count; i++)
	{
		const comp_slot_spec_t *s = &s_bind.specs[i];

		if (used + 4u + s->cfg_len > cap)
		{
			return -1;
		}
		out[used++] = (uint8_t)(s->comp_id & 0xFFu);
		out[used++] = (uint8_t)((s->comp_id >> 8) & 0xFFu);
		out[used++] = s->enabled;
		out[used++] = s->cfg_len;
		memcpy(&out[used], s->cfg, s->cfg_len);
		used += s->cfg_len;
	}
	*out_len = used;
	return 0;
}

static int decode_slots_blob(const uint8_t *in, size_t len)
{
	size_t off = 0u;
	uint8_t n;
	uint8_t i;

	if ((in == NULL) || (len < 1u) || !s_bound)
	{
		return -1;
	}
	n = in[off++];
	if (n > s_bind.spec_cap)
	{
		return -1;
	}
	*s_bind.spec_count = 0u;
	for (i = 0u; i < n; i++)
	{
		comp_slot_spec_t *s;

		if (off + 4u > len)
		{
			return -1;
		}
		s = &s_bind.specs[i];
		memset(s, 0, sizeof(*s));
		s->comp_id = (uint16_t)in[off] | ((uint16_t)in[off + 1u] << 8);
		s->enabled = in[off + 2u];
		s->cfg_len = in[off + 3u];
		off += 4u;
		if ((s->cfg_len > COMP_CFG_MAX) || (off + s->cfg_len > len))
		{
			return -1;
		}
		memcpy(s->cfg, &in[off], s->cfg_len);
		off += s->cfg_len;
		(*s_bind.spec_count)++;
	}
	return 0;
}

static void op_apply(uint16_t corr)
{
	if (!s_bound || (s_bind.rt == NULL))
	{
		AEP_LOGW("ucp_cfg: apply not bound");
		send_status(corr, UCP_CFG_OP_APPLY, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	if (s_bind.before_apply != NULL)
	{
		s_bind.before_apply(s_bind.user);
	}
	if (comp_runtime_apply(s_bind.rt, s_bind.specs, *s_bind.spec_count, s_bind.ctx_ptrs,
						   s_bind.ctx_sizes, s_bind.ctx_count) != 0)
	{
		AEP_LOGE("ucp_cfg: apply failed");
		send_status(corr, UCP_CFG_OP_APPLY, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	if (s_bind.on_applied != NULL)
	{
		s_bind.on_applied(s_bind.user);
	}
	send_status(corr, UCP_CFG_OP_APPLY, (uint8_t)UCP_SERVICE_STATUS_OK);
}

static void op_save(uint16_t corr)
{
	uint8_t blob[64];
	size_t blen = 0u;

	if (encode_slots_blob(blob, sizeof(blob), &blen) != 0)
	{
		AEP_LOGW("ucp_cfg: save encode fail");
		send_status(corr, UCP_CFG_OP_SAVE, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	if (store_set(STORE_KEY_SLOTS, blob, blen) != 0)
	{
		AEP_LOGE("ucp_cfg: save store_set fail");
		send_status(corr, UCP_CFG_OP_SAVE, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	if (s_bind.ad_count > 0u)
	{
		(void)store_set(STORE_KEY_PACK_ADS, s_bind.ads,
						(size_t)s_bind.ad_count * sizeof(ucp_cfg_pack_ad_t));
	}
	if (store_commit() != 0)
	{
		AEP_LOGE("ucp_cfg: save commit fail");
		send_status(corr, UCP_CFG_OP_SAVE, (uint8_t)UCP_SERVICE_STATUS_BUSY);
		return;
	}
	send_status(corr, UCP_CFG_OP_SAVE, (uint8_t)UCP_SERVICE_STATUS_OK);
}

static void op_load(uint16_t corr)
{
	uint8_t blob[64];
	size_t blen = 0u;
	ucp_cfg_pack_ad_t ads[UCP_CFG_MAX_ADS];
	size_t alen = 0u;

	if (store_get(STORE_KEY_SLOTS, blob, sizeof(blob), &blen) != 0)
	{
		AEP_LOGW("ucp_cfg: load missing slots");
		send_status(corr, UCP_CFG_OP_LOAD, (uint8_t)UCP_SERVICE_STATUS_INVALID_ARG);
		return;
	}
	if (decode_slots_blob(blob, blen) != 0)
	{
		AEP_LOGE("ucp_cfg: load decode fail");
		send_status(corr, UCP_CFG_OP_LOAD, (uint8_t)UCP_SERVICE_STATUS_INVALID_ARG);
		return;
	}
	if (store_get(STORE_KEY_PACK_ADS, ads, sizeof(ads), &alen) == 0)
	{
		ucp_cfg_set_ads(ads, (uint8_t)(alen / sizeof(ucp_cfg_pack_ad_t)));
	}
	send_status(corr, UCP_CFG_OP_LOAD, (uint8_t)UCP_SERVICE_STATUS_OK);
}

static void op_get_ads(uint16_t corr)
{
	uint8_t rsp[UCP_PKT_MAX_PAYLOAD_LEN_BYTES];
	uint8_t used = 4u;
	uint8_t i;

	rsp[0] = (uint8_t)(corr & 0xFFu);
	rsp[1] = (uint8_t)((corr >> 8) & 0xFFu);
	rsp[2] = UCP_CFG_OP_GET_ADS;
	rsp[3] = (uint8_t)UCP_SERVICE_STATUS_OK;
	rsp[used++] = s_bound ? s_bind.ad_count : 0u;
	if (s_bound)
	{
		for (i = 0u; i < s_bind.ad_count; i++)
		{
			if ((uint16_t)used + 4u > sizeof(rsp))
			{
				break;
			}
			rsp[used++] = (uint8_t)(s_bind.ads[i].pack_id & 0xFFu);
			rsp[used++] = (uint8_t)((s_bind.ads[i].pack_id >> 8) & 0xFFu);
			rsp[used++] = (uint8_t)(s_bind.ads[i].revision & 0xFFu);
			rsp[used++] = (uint8_t)((s_bind.ads[i].revision >> 8) & 0xFFu);
		}
	}
	(void)ucp_send_pkt(UCP_CFG_APP_ID, (uint8_t)UCP_SERVICE_PKT_ID_RSP, rsp, used);
}

static void cfg_init(void)
{
}

static void cfg_on_rx(const ucp_pkt_t *pkt)
{
	uint16_t corr;
	uint8_t opcode;
	uint8_t body_len;
	const uint8_t *body;

	if ((pkt == NULL) || (pkt->pkt_id != (uint8_t)UCP_SERVICE_PKT_ID_REQ))
	{
		return;
	}
	if (pkt->payload_len < 4u)
	{
		return;
	}
	corr = (uint16_t)pkt->payload[0] | ((uint16_t)pkt->payload[1] << 8);
	opcode = pkt->payload[2];
	body_len = (uint8_t)(pkt->payload_len - 4u);
	body = &pkt->payload[4];

	switch (opcode)
	{
	case UCP_CFG_OP_LIST_DESCS:
		op_list_descs(corr);
		break;
	case UCP_CFG_OP_GET_SLOTS:
		op_get_slots(corr);
		break;
	case UCP_CFG_OP_SET_SLOT:
		op_set_slot(corr, body, body_len);
		break;
	case UCP_CFG_OP_APPLY:
		op_apply(corr);
		break;
	case UCP_CFG_OP_SAVE:
		op_save(corr);
		break;
	case UCP_CFG_OP_LOAD:
		op_load(corr);
		break;
	case UCP_CFG_OP_GET_ADS:
		op_get_ads(corr);
		break;
	default:
		send_status(corr, opcode, (uint8_t)UCP_SERVICE_STATUS_UNSUPPORTED_OPCODE);
		break;
	}
}

static void cfg_on_tick(uint32_t now_ms, bool link_ready)
{
	(void)now_ms;
	(void)link_ready;
}

static void cfg_on_link_event(ucp_app_link_event_t event)
{
	(void)event;
}

const ucp_app_vtable_t g_ucp_cfg_app_vtable = {
	.app_id = UCP_CFG_APP_ID,
	.version = UCP_CFG_VERSION,
	.role_flags = (uint8_t)UCP_APP_ROLE_REQ_RSP,
	.init = cfg_init,
	.on_rx = cfg_on_rx,
	.on_tick = cfg_on_tick,
	.on_link_event = cfg_on_link_event,
};
