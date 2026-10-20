#include "comp.h"

#include "aep_log.h"

#include <stdio.h>
#include <string.h>

void comp_runtime_init(comp_runtime_t *rt)
{
	if (rt == NULL)
	{
		return;
	}
	memset(rt, 0, sizeof(*rt));
}

int comp_runtime_register(comp_runtime_t *rt, const comp_desc_t *desc)
{
	uint8_t i;

	if ((rt == NULL) || (desc == NULL) || (desc->ops == NULL) || (rt->desc_count >= COMP_MAX_DESCS))
	{
		return -1;
	}
	for (i = 0u; i < rt->desc_count; i++)
	{
		if ((rt->descs[i] != NULL) && (rt->descs[i]->id == desc->id))
		{
			return -1;
		}
	}
	rt->descs[rt->desc_count++] = desc;
	return 0;
}

const comp_desc_t *comp_runtime_find_desc(const comp_runtime_t *rt, uint16_t id)
{
	uint8_t i;

	if (rt == NULL)
	{
		return NULL;
	}
	for (i = 0u; i < rt->desc_count; i++)
	{
		if ((rt->descs[i] != NULL) && (rt->descs[i]->id == id))
		{
			return rt->descs[i];
		}
	}
	return NULL;
}

void comp_runtime_set_materialize(comp_runtime_t *rt,
								  int (*fn)(uint16_t, const uint8_t *, uint8_t, void *, size_t,
											size_t *, void *),
								  void *user)
{
	if (rt == NULL)
	{
		return;
	}
	rt->materialize_cfg = fn;
	rt->materialize_user = user;
}

void comp_runtime_stop_all(comp_runtime_t *rt)
{
	uint8_t i;

	if (rt == NULL)
	{
		return;
	}
	for (i = 0u; i < rt->slot_count; i++)
	{
		comp_slot_t *s = &rt->slots[i];

		if ((s->active != 0u) && (s->desc != NULL) && (s->desc->ops != NULL) &&
			(s->desc->ops->stop != NULL) && (s->ctx != NULL))
		{
			s->desc->ops->stop(s->ctx);
		}
		s->active = 0u;
		s->enabled = 0u;
		s->desc = NULL;
		s->ctx = NULL;
		s->cfg_len = 0u;
	}
	rt->slot_count = 0u;
}

int comp_runtime_apply(comp_runtime_t *rt, const comp_slot_spec_t *specs, uint8_t spec_count,
					   void *const *ctx_ptrs, const size_t *ctx_sizes, uint8_t ctx_count)
{
	uint8_t i;
	uint8_t n;

	if ((rt == NULL) || ((spec_count > 0u) && (specs == NULL)))
	{
		AEP_LOGW("comp: apply bad args");
		return -1;
	}
	if ((ctx_ptrs == NULL) || (ctx_sizes == NULL) || (ctx_count < spec_count))
	{
		AEP_LOGW("comp: apply ctx mismatch");
		return -1;
	}

	comp_runtime_stop_all(rt);

	n = (spec_count <= COMP_MAX_SLOTS) ? spec_count : COMP_MAX_SLOTS;
	for (i = 0u; i < n; i++)
	{
		const comp_desc_t *desc;
		comp_slot_t *slot;
		uint8_t rt_cfg[128];
		size_t rt_cfg_len = 0u;
		const void *cfg_ptr;
		size_t cfg_len;
		char msg[56];

		desc = comp_runtime_find_desc(rt, specs[i].comp_id);
		if (desc == NULL)
		{
			(void)snprintf(msg, sizeof(msg), "comp: unknown id=0x%02X skipped",
						   (unsigned)specs[i].comp_id);
			AEP_LOGW(msg);
			continue;
		}
		if (ctx_ptrs[i] == NULL)
		{
			(void)snprintf(msg, sizeof(msg), "comp: null ctx id=0x%02X skipped",
						   (unsigned)specs[i].comp_id);
			AEP_LOGW(msg);
			continue;
		}
		if (ctx_sizes[i] < desc->ctx_size)
		{
			(void)snprintf(msg, sizeof(msg), "comp: ctx small id=0x%02X skipped",
						   (unsigned)specs[i].comp_id);
			AEP_LOGW(msg);
			continue;
		}

		slot = &rt->slots[rt->slot_count];
		memset(slot, 0, sizeof(*slot));
		slot->desc = desc;
		slot->ctx = ctx_ptrs[i];
		slot->enabled = specs[i].enabled;
		slot->cfg_len = (specs[i].cfg_len <= COMP_CFG_MAX) ? specs[i].cfg_len : COMP_CFG_MAX;
		if (slot->cfg_len > 0u)
		{
			memcpy(slot->cfg, specs[i].cfg, slot->cfg_len);
		}

		if (slot->enabled == 0u)
		{
			rt->slot_count++;
			continue;
		}

		cfg_ptr = slot->cfg;
		cfg_len = slot->cfg_len;
		if (rt->materialize_cfg != NULL)
		{
			if (rt->materialize_cfg(specs[i].comp_id, slot->cfg, slot->cfg_len, rt_cfg,
									sizeof(rt_cfg), &rt_cfg_len, rt->materialize_user) != 0)
			{
				(void)snprintf(msg, sizeof(msg), "comp: materialize fail id=0x%02X",
							   (unsigned)specs[i].comp_id);
				AEP_LOGW(msg);
				continue;
			}
			cfg_ptr = rt_cfg;
			cfg_len = rt_cfg_len;
		}

		if ((desc->ops->start == NULL) || (desc->ops->start(slot->ctx, cfg_ptr, cfg_len) != 0))
		{
			(void)snprintf(msg, sizeof(msg), "comp: start fail id=0x%02X",
						   (unsigned)specs[i].comp_id);
			AEP_LOGW(msg);
			slot->active = 0u;
			rt->slot_count++;
			continue;
		}
		slot->active = 1u;
		rt->slot_count++;
	}
	return 0;
}

void comp_runtime_poll(comp_runtime_t *rt, uint32_t now_ms)
{
	uint8_t i;

	if (rt == NULL)
	{
		return;
	}
	for (i = 0u; i < rt->slot_count; i++)
	{
		const comp_slot_t *s = &rt->slots[i];

		if ((s->active != 0u) && (s->desc != NULL) && (s->desc->ops != NULL) &&
			(s->desc->ops->poll != NULL) && (s->ctx != NULL))
		{
			s->desc->ops->poll(s->ctx, now_ms);
		}
	}
}

void comp_runtime_on_can_rx(comp_runtime_t *rt, uint8_t bus, const vcom_can_frame_t *fr)
{
	uint8_t i;

	if ((rt == NULL) || (fr == NULL))
	{
		return;
	}
	for (i = 0u; i < rt->slot_count; i++)
	{
		const comp_slot_t *s = &rt->slots[i];

		if ((s->active != 0u) && (s->desc != NULL) && (s->desc->ops != NULL) &&
			(s->desc->ops->on_can_rx != NULL) && (s->ctx != NULL))
		{
			s->desc->ops->on_can_rx(s->ctx, bus, fr);
		}
	}
}

uint8_t comp_runtime_slot_count(const comp_runtime_t *rt)
{
	return (rt != NULL) ? rt->slot_count : 0u;
}

const comp_slot_t *comp_runtime_slot(const comp_runtime_t *rt, uint8_t index)
{
	if ((rt == NULL) || (index >= rt->slot_count))
	{
		return NULL;
	}
	return &rt->slots[index];
}

uint8_t comp_runtime_desc_count(const comp_runtime_t *rt)
{
	return (rt != NULL) ? rt->desc_count : 0u;
}

const comp_desc_t *comp_runtime_desc_at(const comp_runtime_t *rt, uint8_t index)
{
	if ((rt == NULL) || (index >= rt->desc_count))
	{
		return NULL;
	}
	return rt->descs[index];
}
