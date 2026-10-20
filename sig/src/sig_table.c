#include "sig_table.h"

#include "sig_catalog.h"

#include <string.h>

void sig_table_init(sig_table_t *t)
{
	if (t == NULL)
	{
		return;
	}
	memset(t, 0, sizeof(*t));
}

void sig_table_rebind_begin(sig_table_t *t)
{
	if (t == NULL)
	{
		return;
	}
	memset(t->slot, 0, sizeof(t->slot));
	t->count = 0u;
	t->generation = (uint8_t)(t->generation + 1u);
	if (t->generation == 0u)
	{
		t->generation = 1u;
	}
}

bool sig_table_bind_pub(sig_table_t *t, uint16_t id, uint8_t type, uint8_t rate,
						uint8_t publish)
{
	const sig_catalog_entry_t *cat;
	uint8_t i;

	if ((t == NULL) || (t->count >= SIG_TABLE_CAPACITY))
	{
		return false;
	}
	for (i = 0u; i < t->count; i++)
	{
		if (t->slot[i].id == id)
		{
			return false;
		}
	}

	cat = sig_catalog_find(id);
	if (cat != NULL)
	{
		if (cat->type != type)
		{
			return false;
		}
	}
	else if (id < 0xF000u)
	{
		/* Unknown non-extension catalog id: reject. */
		return false;
	}

	t->slot[t->count].id = id;
	t->slot[t->count].type = type;
	t->slot[t->count].rate = rate;
	t->slot[t->count].bound = 1u;
	t->slot[t->count].valid = 0u;
	t->slot[t->count].aged = 0u;
	t->slot[t->count].dirty = 0u;
	t->slot[t->count].publish = (publish != 0u) ? 1u : 0u;
	t->slot[t->count].raw_bits = 0u;
	t->slot[t->count].stamp_ms = 0u;
	t->count++;
	if (t->generation == 0u)
	{
		t->generation = 1u;
	}
	return true;
}

bool sig_table_bind(sig_table_t *t, uint16_t id, uint8_t type, uint8_t rate)
{
	return sig_table_bind_pub(t, id, type, rate, 1u);
}

static sig_slot_t *find_slot(sig_table_t *t, uint16_t id)
{
	uint8_t i;

	for (i = 0u; i < t->count; i++)
	{
		if (t->slot[i].id == id)
		{
			return &t->slot[i];
		}
	}
	return NULL;
}

bool sig_table_set_publish(sig_table_t *t, uint16_t id, uint8_t publish)
{
	sig_slot_t *s;

	if (t == NULL)
	{
		return false;
	}
	s = find_slot(t, id);
	if (s == NULL)
	{
		return false;
	}
	s->publish = (publish != 0u) ? 1u : 0u;
	return true;
}

bool sig_table_write(sig_table_t *t, uint16_t id, uint32_t raw_bits,
					 uint8_t valid, uint32_t now_ms)
{
	sig_slot_t *s;

	if (t == NULL)
	{
		return false;
	}
	s = find_slot(t, id);
	if (s == NULL)
	{
		return false;
	}

	if (valid == 0u)
	{
		s->valid = 0u;
		s->stamp_ms = now_ms;
		return true;
	}

	if ((s->rate == SIG_RATE_CHANGE) &&
		((s->valid == 0u) || (s->raw_bits != raw_bits)))
	{
		s->dirty = 1u;
	}

	s->raw_bits = raw_bits;
	s->valid = 1u;
	s->aged = 0u;
	s->stamp_ms = now_ms;
	return true;
}

void sig_table_age(sig_table_t *t, uint32_t now_ms)
{
	uint8_t i;

	if (t == NULL)
	{
		return;
	}

	for (i = 0u; i < t->count; i++)
	{
		sig_slot_t *s = &t->slot[i];
		const sig_catalog_entry_t *cat;
		uint16_t stale_ms;

		if ((s->bound == 0u) || (s->valid == 0u))
		{
			continue;
		}

		cat = sig_catalog_find(s->id);
		if (cat != NULL)
		{
			stale_ms = cat->stale_ms;
		}
		else if (s->id >= 0xF000u)
		{
			/* Tier B / extension: no catalog row — default timeout. */
			stale_ms = 500u;
		}
		else
		{
			continue;
		}
		if (stale_ms == 0u)
		{
			continue;
		}
		if ((uint32_t)(now_ms - s->stamp_ms) < (uint32_t)stale_ms)
		{
			continue;
		}

		s->valid = 0u;
		s->aged = 1u;
		if (s->rate == SIG_RATE_CHANGE)
		{
			s->dirty = 1u;
		}
	}
}

bool sig_table_read(const sig_table_t *t, uint16_t id,
					uint32_t *raw_bits, uint8_t *valid, uint32_t *stamp_ms)
{
	uint8_t i;

	if (t == NULL)
	{
		return false;
	}
	for (i = 0u; i < t->count; i++)
	{
		if (t->slot[i].id == id)
		{
			if (raw_bits != NULL)
			{
				*raw_bits = t->slot[i].raw_bits;
			}
			if (valid != NULL)
			{
				*valid = t->slot[i].valid;
			}
			if (stamp_ms != NULL)
			{
				*stamp_ms = t->slot[i].stamp_ms;
			}
			return true;
		}
	}
	return false;
}

void sig_table_snapshot(const sig_table_t *t, sig_snapshot_t *out)
{
	uint8_t i;

	if ((t == NULL) || (out == NULL))
	{
		return;
	}
	memset(out, 0, sizeof(*out));
	out->generation = t->generation;
	out->count = t->count;
	for (i = 0u; i < t->count; i++)
	{
		out->rec[i].id = t->slot[i].id;
		out->rec[i].type = t->slot[i].type;
		out->rec[i].rate = t->slot[i].rate;
		out->rec[i].valid = t->slot[i].valid;
		out->rec[i].aged = t->slot[i].aged;
		out->rec[i].dirty = t->slot[i].dirty;
		out->rec[i].publish = t->slot[i].publish;
		out->rec[i].raw_bits = t->slot[i].raw_bits;
	}
}

bool sig_table_clear_dirty(sig_table_t *t, uint16_t id)
{
	uint8_t i;

	if (t == NULL)
	{
		return false;
	}
	for (i = 0u; i < t->count; i++)
	{
		if (t->slot[i].id == id)
		{
			t->slot[i].dirty = 0u;
			return true;
		}
	}
	return false;
}
