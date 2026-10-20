/*
 * Host smoke: init diag_link against a mock vcom PHY (no ECU required).
 */
#include "diag_link.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static uint32_t g_now;
static uint8_t g_started;
static vcom_can_frame_t g_last_tx;
static uint8_t g_tx_count;

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

static uint32_t now_ms(void)
{
	return g_now;
}

static bool phy_start(void *hw)
{
	(void)hw;
	g_started = 1u;
	return true;
}

static bool phy_send(void *hw, const vcom_can_frame_t *fr)
{
	(void)hw;
	if (fr == NULL)
	{
		return false;
	}
	g_last_tx = *fr;
	g_tx_count++;
	return true;
}

static bool phy_rx_pop(void *hw, vcom_can_frame_t *fr)
{
	(void)hw;
	(void)fr;
	return false;
}

int main(void)
{
	vcom_core_t core;
	vcom_can_phy_t phy;
	diag_link_t *link;
	diag_link_cfg_t cfg;

	g_failures = 0;
	g_now = 1000u;
	memset(&phy, 0, sizeof(phy));
	phy.start = phy_start;
	phy.send = phy_send;
	phy.rx_pop = phy_rx_pop;

	vcom_core_init(&core, NULL);
	vcom_core_set_can_phy(&core, VCOM_BUS_CAN0, &phy);
	expect_true("start bus", vcom_core_start_bus(&core, VCOM_BUS_CAN0, 500000u));

	link = diag_link_instance();
	memset(&cfg, 0, sizeof(cfg));
	cfg.core = &core;
	cfg.bus = VCOM_BUS_CAN0;
	cfg.phys_req_id = 0x7E0u;
	cfg.phys_resp_id = 0x7E8u;
	cfg.func_req_id = 0x7DFu;
	cfg.now_ms = now_ms;

	expect_true("diag init", diag_link_init(link, &cfg));
	expect_true("subscribe", diag_link_subscribe_rx(link));
	expect_true("client ptr", diag_link_client(link) != NULL);

	(void)diag_link_poll(link);
	g_now += 1u;
	(void)diag_link_poll(link);

	diag_link_deinit(link);
	expect_true("done", 1);

	if (g_failures != 0)
	{
		printf("%d failure(s)\n", g_failures);
		return 1;
	}
	printf("all vcom_diag tests passed\n");
	return 0;
}
