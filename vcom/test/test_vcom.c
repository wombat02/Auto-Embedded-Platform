#include "vcom_core.h"
#include "vcom_rx_demux.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static vcom_can_frame_t g_tx[8];
static uint8_t g_tx_count;
static vcom_can_frame_t g_rx[8];
uint8_t g_rx_count;
static uint8_t g_rx_pop_idx;
static uint8_t g_started;
static uint32_t g_rx_seen_id;
static uint8_t g_rx_cb_count;

static void expect_true(const char *name, int cond)
{
	if (!cond)
	{
		printf("FAIL: %s\n", name);
		g_failures++;
	}
	else
	{
		printf("ok: %s\n", name);
	}
}

static bool phy_start(void *hw)
{
	(void)hw;
	g_started = 1u;
	return true;
}

static void phy_stop(void *hw)
{
	(void)hw;
	g_started = 0u;
}

static bool phy_send(void *hw, const vcom_can_frame_t *fr)
{
	(void)hw;
	if ((fr == NULL) || (g_tx_count >= 8u))
	{
		return false;
	}
	g_tx[g_tx_count++] = *fr;
	return true;
}

static bool phy_rx_pop(void *hw, vcom_can_frame_t *fr)
{
	(void)hw;
	if (g_rx_pop_idx >= g_rx_count)
	{
		return false;
	}
	*fr = g_rx[g_rx_pop_idx++];
	return true;
}

static void on_rx(const vcom_can_frame_t *fr, void *user)
{
	(void)user;
	g_rx_seen_id = fr->id;
	g_rx_cb_count++;
}

static uint8_t g_demux_hits;

static void on_demux(uint8_t bus, const vcom_can_frame_t *fr, void *user)
{
	(void)bus;
	(void)user;
	if ((fr != NULL) && (fr->id == 0x201u))
	{
		g_demux_hits++;
	}
}

int main(void)
{
	vcom_core_t core;
	vcom_can_phy_t phy;
	vcom_can_frame_t fr;
	vcom_rx_demux_t demux;
	int slot;

	g_failures = 0;
	memset(&phy, 0, sizeof(phy));
	phy.start = phy_start;
	phy.stop = phy_stop;
	phy.send = phy_send;
	phy.rx_pop = phy_rx_pop;

	vcom_core_init(&core, NULL);
	vcom_core_set_can_phy(&core, VCOM_BUS_CAN0, &phy);
	expect_true("start bus", vcom_core_start_bus(&core, VCOM_BUS_CAN0, 500000u));

	memset(&fr, 0, sizeof(fr));
	fr.id = 0x123u;
	fr.dlc = 2u;
	fr.data[0] = 0xAAu;
	fr.data[1] = 0xBBu;
	expect_true("post", vcom_tx_post_local(&core, VCOM_BUS_CAN0, &fr));
	g_tx_count = 0u;
	vcom_core_tick(&core, 100u);
	expect_true("once tx", g_tx_count == 1u && g_tx[0].id == 0x123u);

	slot = vcom_tx_periodic(&core, VCOM_BUS_CAN0, 50u, &fr, NULL, NULL);
	expect_true("periodic slot", slot >= 0);
	g_tx_count = 0u;
	vcom_core_tick(&core, 100u);
	expect_true("periodic first", g_tx_count == 1u);
	g_tx_count = 0u;
	vcom_core_tick(&core, 120u);
	expect_true("periodic not due", g_tx_count == 0u);
	vcom_core_tick(&core, 150u);
	expect_true("periodic due", g_tx_count == 1u);

	g_rx[0] = fr;
	g_rx[0].id = 0x456u;
	g_rx_count = 1u;
	g_rx_pop_idx = 0u;
	g_rx_cb_count = 0u;
	expect_true("subscribe",
				vcom_rx_subscribe(&core, VCOM_BUS_CAN0, 0x456u, 0x7FFu, on_rx, NULL));
	vcom_core_drain_rx(&core, VCOM_BUS_CAN0);
	expect_true("rx dispatch", g_rx_cb_count == 1u && g_rx_seen_id == 0x456u);

	vcom_core_init(&core, NULL);
	vcom_core_set_can_phy(&core, VCOM_BUS_CAN0, &phy);
	(void)vcom_core_start_bus(&core, VCOM_BUS_CAN0, 500000u);
	vcom_rx_demux_init(&demux, &core);
	expect_true("demux attach", vcom_rx_demux_attach_bus(&demux, VCOM_BUS_CAN0));
	g_demux_hits = 0u;
	expect_true("demux register",
				vcom_rx_demux_register(&demux, VCOM_BUS_CAN0, 0x201u, 0x7FFu, on_demux, NULL) >= 0);
	g_rx[0] = fr;
	g_rx[0].id = 0x201u;
	g_rx_count = 1u;
	g_rx_pop_idx = 0u;
	vcom_core_drain_rx(&core, VCOM_BUS_CAN0);
	expect_true("demux hit", g_demux_hits == 1u);

	if (g_failures != 0)
	{
		printf("%d failure(s)\n", g_failures);
		return 1;
	}
	printf("all vcom tests passed\n");
	return 0;
}
