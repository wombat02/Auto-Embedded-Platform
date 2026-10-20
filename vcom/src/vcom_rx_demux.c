#include "vcom_rx_demux.h"

#include <string.h>

static void demux_on_frame(const vcom_can_frame_t *fr, void *user)
{
	vcom_rx_demux_t *d = (vcom_rx_demux_t *)user;
	uint8_t i;
	uint8_t bus;

	if ((d == NULL) || (fr == NULL) || (d->core == NULL))
	{
		return;
	}

	/* Bus is not in the frame; match against all attached buses' handlers.
	 * Caller registers per-bus; we store bus on each entry. For multi-bus
	 * wide subscribe we need bus context — attach uses distinct user wrappers.
	 * Here user is demux*; bus recovered by trying entries (handlers check bus).
	 * Prefer: store pending bus in demux from attach-specific callback. */
	(void)bus;
	for (i = 0u; i < VCOM_RX_DEMUX_HANDLERS; i++)
	{
		const vcom_rx_demux_entry_t *e = &d->entry[i];

		if ((e->used == 0u) || (e->fn == NULL))
		{
			continue;
		}
		if (((fr->id ^ e->id) & e->id_mask) == 0u)
		{
			e->fn(e->bus, fr, e->user);
		}
	}
}

typedef struct {
	vcom_rx_demux_t *d;
	uint8_t bus;
} demux_bus_hook_t;

/* One static hook per bus (portable single demux instance typical). */
static demux_bus_hook_t s_hooks[VCOM_BUS_COUNT];

static void demux_on_frame_bus(const vcom_can_frame_t *fr, void *user)
{
	demux_bus_hook_t *h = (demux_bus_hook_t *)user;
	vcom_rx_demux_t *d;
	uint8_t i;

	if ((h == NULL) || (fr == NULL) || (h->d == NULL))
	{
		return;
	}
	d = h->d;
	for (i = 0u; i < VCOM_RX_DEMUX_HANDLERS; i++)
	{
		const vcom_rx_demux_entry_t *e = &d->entry[i];

		if ((e->used == 0u) || (e->fn == NULL) || (e->bus != h->bus))
		{
			continue;
		}
		if (((fr->id ^ e->id) & e->id_mask) == 0u)
		{
			e->fn(e->bus, fr, e->user);
		}
	}
}

void vcom_rx_demux_init(vcom_rx_demux_t *d, vcom_core_t *core)
{
	if (d == NULL)
	{
		return;
	}
	memset(d, 0, sizeof(*d));
	d->core = core;
}

bool vcom_rx_demux_attach_bus(vcom_rx_demux_t *d, uint8_t bus)
{
	if ((d == NULL) || (d->core == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return false;
	}
	if (d->bus_attached[bus] != 0u)
	{
		return true;
	}
	s_hooks[bus].d = d;
	s_hooks[bus].bus = bus;
	/* id=0, mask=0 → match all frames on this subscribe slot */
	if (!vcom_rx_subscribe(d->core, bus, 0u, 0u, demux_on_frame_bus, &s_hooks[bus]))
	{
		return false;
	}
	d->bus_attached[bus] = 1u;
	(void)demux_on_frame; /* silence if unused */
	return true;
}

int vcom_rx_demux_register(vcom_rx_demux_t *d, uint8_t bus, uint32_t id, uint32_t id_mask,
						   vcom_rx_demux_fn fn, void *user)
{
	uint8_t i;

	if ((d == NULL) || (fn == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return -1;
	}
	if (!vcom_rx_demux_attach_bus(d, bus))
	{
		return -1;
	}
	for (i = 0u; i < VCOM_RX_DEMUX_HANDLERS; i++)
	{
		if (d->entry[i].used == 0u)
		{
			d->entry[i].used = 1u;
			d->entry[i].bus = bus;
			d->entry[i].id = id;
			d->entry[i].id_mask = id_mask;
			d->entry[i].fn = fn;
			d->entry[i].user = user;
			return (int)i;
		}
	}
	return -1;
}

void vcom_rx_demux_unregister_user(vcom_rx_demux_t *d, void *user)
{
	uint8_t i;

	if (d == NULL)
	{
		return;
	}
	for (i = 0u; i < VCOM_RX_DEMUX_HANDLERS; i++)
	{
		if ((d->entry[i].used != 0u) && (d->entry[i].user == user))
		{
			memset(&d->entry[i], 0, sizeof(d->entry[i]));
		}
	}
}
