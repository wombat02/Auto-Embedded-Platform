#include "vcom_core.h"

#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define VCOM_WEAK __attribute__((weak))
#else
#define VCOM_WEAK
#endif

VCOM_WEAK void vcom_port_lock(void)
{
}

VCOM_WEAK void vcom_port_unlock(void)
{
}

void vcom_core_init(vcom_core_t *c, const vcom_time_t *time)
{
	if (c == NULL)
	{
		return;
	}
	memset(c, 0, sizeof(*c));
	c->time = time;
}

void vcom_core_set_can_phy(vcom_core_t *c, uint8_t bus, const vcom_can_phy_t *phy)
{
	if ((c == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return;
	}
	c->can_phy[bus] = phy;
}

void vcom_core_set_kline_phy(vcom_core_t *c, const vcom_kline_phy_t *phy)
{
	if (c == NULL)
	{
		return;
	}
	c->kline_phy = phy;
}

bool vcom_core_start_bus(vcom_core_t *c, uint8_t bus, uint32_t bitrate)
{
	const vcom_can_phy_t *phy;

	if ((c == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return false;
	}
	phy = c->can_phy[bus];
	if ((phy == NULL) || (phy->start == NULL))
	{
		return false;
	}
	if (phy->set_standby != NULL)
	{
		(void)phy->set_standby(phy->hw, false);
	}
	if ((bitrate != 0u) && (phy->set_bitrate != NULL))
	{
		if (!phy->set_bitrate(phy->hw, bitrate))
		{
			return false;
		}
	}
	if (!phy->start(phy->hw))
	{
		return false;
	}
	c->can_started[bus] = 1u;
	return true;
}

void vcom_core_stop_bus(vcom_core_t *c, uint8_t bus)
{
	const vcom_can_phy_t *phy;

	if ((c == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return;
	}
	phy = c->can_phy[bus];
	if ((phy != NULL) && (phy->stop != NULL))
	{
		phy->stop(phy->hw);
	}
	c->can_started[bus] = 0u;
	vcom_tx_cancel_bus(c, bus);
}

void vcom_core_start_kline(vcom_core_t *c)
{
	if ((c == NULL) || (c->kline_phy == NULL) || (c->kline_phy->start == NULL))
	{
		return;
	}
	if (c->kline_phy->set_awake != NULL)
	{
		c->kline_phy->set_awake(c->kline_phy->hw, true);
	}
	if (c->kline_phy->start(c->kline_phy->hw))
	{
		c->kline_started = 1u;
	}
}

void vcom_core_stop_kline(vcom_core_t *c)
{
	if ((c == NULL) || (c->kline_phy == NULL))
	{
		return;
	}
	if (c->kline_phy->stop != NULL)
	{
		c->kline_phy->stop(c->kline_phy->hw);
	}
	c->kline_started = 0u;
}

static bool once_push(vcom_tx_once_q_t *q, const vcom_can_frame_t *fr)
{
	if ((q == NULL) || (fr == NULL) || (q->count >= VCOM_TX_ONCE_DEPTH))
	{
		return false;
	}
	q->q[q->tail].used = 1u;
	q->q[q->tail].fr = *fr;
	q->tail = (uint8_t)((q->tail + 1u) % VCOM_TX_ONCE_DEPTH);
	q->count++;
	return true;
}

bool vcom_tx_post_local(vcom_core_t *c, uint8_t bus, const vcom_can_frame_t *fr)
{
	if ((c == NULL) || (bus >= VCOM_BUS_COUNT))
	{
		return false;
	}
	return once_push(&c->once[bus], fr);
}

bool vcom_tx_post(vcom_core_t *c, uint8_t bus, const vcom_can_frame_t *fr)
{
	bool ok;

	vcom_port_lock();
	ok = vcom_tx_post_local(c, bus, fr);
	vcom_port_unlock();
	return ok;
}

int vcom_tx_periodic(vcom_core_t *c, uint8_t bus, uint32_t period_ms,
					 const vcom_can_frame_t *tmpl,
					 vcom_tx_fill_fn fill, void *user)
{
	uint8_t i;

	if ((c == NULL) || (bus >= VCOM_BUS_COUNT) || (tmpl == NULL) || (period_ms == 0u))
	{
		return -1;
	}
	for (i = 0u; i < VCOM_TX_PERIODIC_SLOTS; i++)
	{
		if (c->periodic[i].used == 0u)
		{
			c->periodic[i].used = 1u;
			c->periodic[i].bus = bus;
			c->periodic[i].period_ms = period_ms;
			c->periodic[i].next_ms = 0u;
			c->periodic[i].tmpl = *tmpl;
			c->periodic[i].fill = fill;
			c->periodic[i].user = user;
			return (int)i;
		}
	}
	return -1;
}

void vcom_tx_cancel(vcom_core_t *c, int slot)
{
	if ((c == NULL) || (slot < 0) || (slot >= (int)VCOM_TX_PERIODIC_SLOTS))
	{
		return;
	}
	memset(&c->periodic[slot], 0, sizeof(c->periodic[slot]));
}

void vcom_tx_cancel_bus(vcom_core_t *c, uint8_t bus)
{
	uint8_t i;

	if (c == NULL)
	{
		return;
	}
	for (i = 0u; i < VCOM_TX_PERIODIC_SLOTS; i++)
	{
		if ((c->periodic[i].used != 0u) && (c->periodic[i].bus == bus))
		{
			memset(&c->periodic[i], 0, sizeof(c->periodic[i]));
		}
	}
	if (bus < VCOM_BUS_COUNT)
	{
		memset(&c->once[bus], 0, sizeof(c->once[bus]));
	}
}

bool vcom_rx_subscribe(vcom_core_t *c, uint8_t bus,
					   uint32_t id, uint32_t id_mask,
					   vcom_rx_fn fn, void *user)
{
	uint8_t i;

	if ((c == NULL) || (bus >= VCOM_BUS_COUNT) || (fn == NULL))
	{
		return false;
	}
	for (i = 0u; i < VCOM_RX_SUBSCRIBERS; i++)
	{
		if (c->rx_sub[i].used == 0u)
		{
			c->rx_sub[i].used = 1u;
			c->rx_sub[i].bus = bus;
			c->rx_sub[i].id = id;
			c->rx_sub[i].id_mask = id_mask;
			c->rx_sub[i].fn = fn;
			c->rx_sub[i].user = user;
			return true;
		}
	}
	return false;
}

void vcom_rx_unsubscribe_user(vcom_core_t *c, void *user)
{
	uint8_t i;

	if (c == NULL)
	{
		return;
	}
	for (i = 0u; i < VCOM_RX_SUBSCRIBERS; i++)
	{
		if ((c->rx_sub[i].used != 0u) && (c->rx_sub[i].user == user))
		{
			memset(&c->rx_sub[i], 0, sizeof(c->rx_sub[i]));
		}
	}
}

void vcom_core_drain_rx(vcom_core_t *c, uint8_t bus)
{
	const vcom_can_phy_t *phy;
	vcom_can_frame_t fr;
	uint8_t i;

	if ((c == NULL) || (bus >= VCOM_BUS_COUNT) || (c->can_started[bus] == 0u))
	{
		return;
	}
	phy = c->can_phy[bus];
	if ((phy == NULL) || (phy->rx_pop == NULL))
	{
		return;
	}

	while (phy->rx_pop(phy->hw, &fr))
	{
		for (i = 0u; i < VCOM_RX_SUBSCRIBERS; i++)
		{
			const vcom_rx_sub_t *s = &c->rx_sub[i];

			if ((s->used == 0u) || (s->bus != bus))
			{
				continue;
			}
			if (((fr.id ^ s->id) & s->id_mask) == 0u)
			{
				s->fn(&fr, s->user);
			}
		}
	}
}

static bool elapsed_ge(uint32_t now, uint32_t then, uint32_t limit)
{
	return (uint32_t)(now - then) >= limit;
}

void vcom_core_tick(vcom_core_t *c, uint32_t now_ms)
{
	uint8_t bus;
	uint8_t i;

	if (c == NULL)
	{
		return;
	}

	for (bus = 0u; bus < VCOM_BUS_COUNT; bus++)
	{
		const vcom_can_phy_t *phy = c->can_phy[bus];
		vcom_tx_once_q_t *oq;
		bool stalled = false;

		if ((c->can_started[bus] == 0u) || (phy == NULL) || (phy->send == NULL))
		{
			continue;
		}

		oq = &c->once[bus];
		while ((oq->count > 0u) && !stalled)
		{
			vcom_tx_once_t *slot = &oq->q[oq->head];

			if (!phy->send(phy->hw, &slot->fr))
			{
				stalled = true;
				break;
			}
			slot->used = 0u;
			oq->head = (uint8_t)((oq->head + 1u) % VCOM_TX_ONCE_DEPTH);
			oq->count--;
		}

		if (stalled)
		{
			continue;
		}

		for (i = 0u; i < VCOM_TX_PERIODIC_SLOTS; i++)
		{
			vcom_tx_periodic_t *p = &c->periodic[i];
			vcom_can_frame_t fr;

			if ((p->used == 0u) || (p->bus != bus))
			{
				continue;
			}
			if ((p->next_ms != 0u) && !elapsed_ge(now_ms, p->next_ms, p->period_ms))
			{
				continue;
			}

			fr = p->tmpl;
			if (p->fill != NULL)
			{
				p->fill(&fr, p->user);
			}
			if (!phy->send(phy->hw, &fr))
			{
				break;
			}
			p->next_ms = now_ms;
		}
	}
}
