/*
 * aep_app.c — example orchestration glue (not a portable library).
 *
 * Wires ThreadX threads to: sig + ucp_tlm, vcom + demux + PHYs, comp runtime
 * (PT / PID / cluster / dash), store + ucp_cfg, and aep_ucp_usb.
 * See examples/stm32_h5/GLUE.md and BRINGUP.md.
 */
#include "aep_app.h"

#include "aep_ucp_usb.h"
#include "aep_ucp_port.h"
#include "aep_fdcan_phy.h"
#include "aep_kline_phy.h"
#include "aep_demo_pt.h"
#include "aep_pt_parser.h"
#include "aep_pid_client.h"
#include "aep_cluster_mod.h"
#include "aep_dash_mod.h"
#include "main.h"
#include "fdcan.h"
#include "log.h"
#include "tx_api.h"

#include "comp.h"
#include "sig_catalog.h"
#include "sig_table.h"
#include "store.h"
#include "ucp_cfg.h"
#include "ucp_port.h"
#include "ucp_tlm_sig.h"
#include "vcom_core.h"
#include "vcom_rx_demux.h"

#include "stm32h5xx_hal.h"

#include <string.h>

#define AEP_SUP_STACK 1536u
/* COM: CAN/comp/snap only — USBX UCP poll runs on USBX app thread. */
#define AEP_COM_STACK 4096u
#define AEP_SPEC_CAP 4u
/* Snapshot → ucp_tlm submit cadence (bands reuse snap until next submit). */
#define AEP_TLM_PERIOD_MS 10u

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;
extern TX_THREAD tx_app_thread;

static TX_THREAD s_sup_thread;
static TX_THREAD s_com_thread;
static TX_MUTEX s_vcom_mutex;

static sig_table_t s_sig_table;
static vcom_core_t s_vcom;
static vcom_time_t s_vcom_time;
static vcom_can_phy_t s_phy0;
static vcom_can_phy_t s_phy1;
static vcom_kline_phy_t s_kline_phy;
static vcom_rx_demux_t s_demux;
static IWDG_HandleTypeDef s_iwdg;
static uint8_t s_iwdg_started;

static aep_pt_parser_ctx_t s_parser_ctx;
static aep_pid_client_ctx_t s_pid_ctx;
static aep_cluster_mod_ctx_t s_cluster_ctx;
static aep_dash_mod_ctx_t s_dash_ctx;

static comp_runtime_t s_comp_rt;
static comp_slot_spec_t s_specs[AEP_SPEC_CAP];
static uint8_t s_spec_count;
static void *s_ctx_ptrs[AEP_SPEC_CAP];
static size_t s_ctx_sizes[AEP_SPEC_CAP];
static uint32_t s_next_tlm_ms;
/* ~770 B — keep off the com call stack (nested under USB poll historically). */
static sig_snapshot_t s_tlm_snap;

static void stack_error_notify(TX_THREAD *thread_ptr)
{
	const char *name = (thread_ptr != NULL) ? thread_ptr->tx_thread_name : "?";
	/* TX_ENABLE_STACK_CHECKING: overflow here precedes hardfault / IWDG. */
	loge("STACK OVERFLOW: %s", name);
}

static void log_stack_hwm(TX_THREAD *th)
{
	CHAR *name = TX_NULL;
	UINT state = 0u;
	ULONG run_count = 0u;
	UINT priority = 0u;
	UINT preempt = 0u;
	ULONG time_slice = 0u;
	TX_THREAD *next = TX_NULL;
	TX_THREAD *next_suspended = TX_NULL;
	UCHAR *start;
	UCHAR *end;
	UCHAR *p;
	ULONG stack_size;
	ULONG unused;
	ULONG used;

	if (th == TX_NULL)
	{
		return;
	}
	(void)tx_thread_info_get(th, &name, &state, &run_count, &priority, &preempt, &time_slice,
							 &next, &next_suspended);
	start = (UCHAR *)th->tx_thread_stack_start;
	end = (UCHAR *)th->tx_thread_stack_end;
	if ((start == TX_NULL) || (end == TX_NULL) || (end < start))
	{
		return;
	}
	p = start;
	while ((p < end) && (*p == (UCHAR)0xEFu))
	{
		p++;
	}
	stack_size = (ULONG)(end - start) + 1u;
	unused = (ULONG)(p - start);
	used = (unused < stack_size) ? (stack_size - unused) : stack_size;
	logi("stack %s used=%lu/%lu state=%u", (name != TX_NULL) ? name : "?", (unsigned long)used,
		 (unsigned long)stack_size, (unsigned)state);
}

void vcom_port_lock(void)
{
	(void)tx_mutex_get(&s_vcom_mutex, TX_WAIT_FOREVER);
}

void vcom_port_unlock(void)
{
	(void)tx_mutex_put(&s_vcom_mutex);
}

static void bind_signal_set(void)
{
	uint8_t i;
	uint8_t dash_on = 0u;

	for (i = 0u; i < s_spec_count; i++)
	{
		if ((s_specs[i].comp_id == AEP_COMP_ID_DASH) && (s_specs[i].enabled != 0u))
		{
			dash_on = 1u;
		}
	}

	sig_table_rebind_begin(&s_sig_table);
	/* LIVE — latency-sensitive */
	(void)sig_table_bind(&s_sig_table, SIG_ENGINE_RPM, SIG_T_U16, SIG_RATE_LIVE);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_DASH_STEER_DEG, SIG_T_I16, SIG_RATE_LIVE, dash_on);
	/* HF */
	(void)sig_table_bind(&s_sig_table, SIG_COOLANT_TEMP, SIG_T_I16, SIG_RATE_HF);
	(void)sig_table_bind(&s_sig_table, SIG_ENGINE_TORQUE, SIG_T_I16, SIG_RATE_HF);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_DASH_TURN_L, SIG_T_U8, SIG_RATE_HF, dash_on);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_DASH_TURN_R, SIG_T_U8, SIG_RATE_HF, dash_on);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_DASH_WIPER, SIG_T_U8, SIG_RATE_HF, dash_on);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_DASH_HIGH_BEAM, SIG_T_U8, SIG_RATE_HF,
							 dash_on);
	/* MF */
	(void)sig_table_bind(&s_sig_table, SIG_ENGINE_LOAD, SIG_T_U16, SIG_RATE_MF);
	(void)sig_table_bind(&s_sig_table, SIG_THROTTLE_POS, SIG_T_U16, SIG_RATE_MF);
	(void)sig_table_bind(&s_sig_table, SIG_VEHICLE_SPEED, SIG_T_U16, SIG_RATE_MF);
	/* LF — UDS PID extension signals */
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_EXT_BOOST, SIG_T_U16, SIG_RATE_LF, 1u);
	(void)sig_table_bind_pub(&s_sig_table, AEP_SIG_EXT_FUEL_PRESS, SIG_T_U16, SIG_RATE_LF, 1u);
	/* CHANGE */
	(void)sig_table_bind(&s_sig_table, SIG_GEAR, SIG_T_I8, SIG_RATE_CHANGE);

	if (dash_on != 0u)
	{
		ucp_cfg_pack_ad_t ad;

		ad.pack_id = AEP_PACK_DEMO_DASH;
		ad.revision = AEP_PACK_DEMO_DASH_REV;
		ucp_cfg_set_ads(&ad, 1u);
	}
	else
	{
		ucp_cfg_set_ads(NULL, 0u);
	}
}

static void sync_ctx_arrays(void *user)
{
	uint8_t i;

	(void)user;
	for (i = 0u; i < s_spec_count; i++)
	{
		switch (s_specs[i].comp_id)
		{
		case AEP_COMP_ID_PT_PARSER:
			s_ctx_ptrs[i] = &s_parser_ctx;
			s_ctx_sizes[i] = sizeof(s_parser_ctx);
			break;
		case AEP_COMP_ID_PID_CLIENT:
			s_ctx_ptrs[i] = &s_pid_ctx;
			s_ctx_sizes[i] = sizeof(s_pid_ctx);
			break;
		case AEP_COMP_ID_CLUSTER:
			s_ctx_ptrs[i] = &s_cluster_ctx;
			s_ctx_sizes[i] = sizeof(s_cluster_ctx);
			break;
		case AEP_COMP_ID_DASH:
			s_ctx_ptrs[i] = &s_dash_ctx;
			s_ctx_sizes[i] = sizeof(s_dash_ctx);
			break;
		default:
			s_ctx_ptrs[i] = NULL;
			s_ctx_sizes[i] = 0u;
			break;
		}
	}
}

static void on_cfg_applied(void *user)
{
	(void)user;
	bind_signal_set();
}

static int materialize_cfg(uint16_t comp_id, const uint8_t *blob, uint8_t blob_len, void *out_cfg,
						   size_t out_cap, size_t *out_len, void *user)
{
	(void)user;
	if ((out_cfg == NULL) || (out_len == NULL))
	{
		return -1;
	}

	switch (comp_id)
	{
	case AEP_COMP_ID_PT_PARSER:
	{
		aep_pt_parser_cfg_t *c = (aep_pt_parser_cfg_t *)out_cfg;
		aep_pt_nvm_cfg_t nvm;

		if (out_cap < sizeof(*c))
		{
			return -1;
		}
		memset(&nvm, 0, sizeof(nvm));
		nvm.bus = VCOM_BUS_CAN0;
		nvm.profile = AEP_PT_PROFILE_A;
		if ((blob != NULL) && (blob_len >= sizeof(nvm)))
		{
			memcpy(&nvm, blob, sizeof(nvm));
		}
		memset(c, 0, sizeof(*c));
		c->core = &s_vcom;
		c->demux = &s_demux;
		c->table = &s_sig_table;
		c->bus = nvm.bus;
		c->profile = nvm.profile;
		*out_len = sizeof(*c);
		return 0;
	}
	case AEP_COMP_ID_PID_CLIENT:
	{
		aep_pid_client_cfg_t *c = (aep_pid_client_cfg_t *)out_cfg;
		aep_bus_nvm_cfg_t nvm;

		if (out_cap < sizeof(*c))
		{
			return -1;
		}
		memset(&nvm, 0, sizeof(nvm));
		nvm.bus = VCOM_BUS_CAN0;
		if ((blob != NULL) && (blob_len >= sizeof(nvm)))
		{
			memcpy(&nvm, blob, sizeof(nvm));
		}
		memset(c, 0, sizeof(*c));
		c->core = &s_vcom;
		c->table = &s_sig_table;
		c->bus = nvm.bus;
		c->now_ms = aep_now_ms;
		*out_len = sizeof(*c);
		return 0;
	}
	case AEP_COMP_ID_CLUSTER:
	{
		aep_cluster_mod_cfg_t *c = (aep_cluster_mod_cfg_t *)out_cfg;
		aep_bus_nvm_cfg_t nvm;

		if (out_cap < sizeof(*c))
		{
			return -1;
		}
		memset(&nvm, 0, sizeof(nvm));
		nvm.bus = VCOM_BUS_CAN1;
		if ((blob != NULL) && (blob_len >= sizeof(nvm)))
		{
			memcpy(&nvm, blob, sizeof(nvm));
		}
		memset(c, 0, sizeof(*c));
		c->core = &s_vcom;
		c->table = &s_sig_table;
		c->bus = nvm.bus;
		*out_len = sizeof(*c);
		return 0;
	}
	case AEP_COMP_ID_DASH:
	{
		aep_dash_mod_cfg_t *c = (aep_dash_mod_cfg_t *)out_cfg;
		aep_bus_nvm_cfg_t nvm;

		if (out_cap < sizeof(*c))
		{
			return -1;
		}
		memset(&nvm, 0, sizeof(nvm));
		nvm.bus = VCOM_BUS_CAN1;
		if ((blob != NULL) && (blob_len >= sizeof(nvm)))
		{
			memcpy(&nvm, blob, sizeof(nvm));
		}
		memset(c, 0, sizeof(*c));
		c->core = &s_vcom;
		c->demux = &s_demux;
		c->table = &s_sig_table;
		c->bus = nvm.bus;
		*out_len = sizeof(*c);
		return 0;
	}
	default:
		return -1;
	}
}

static void factory_specs(void)
{
	aep_pt_nvm_cfg_t pt;
	aep_bus_nvm_cfg_t bus0;
	aep_bus_nvm_cfg_t bus1;

	memset(s_specs, 0, sizeof(s_specs));
	s_spec_count = 0u;

	memset(&pt, 0, sizeof(pt));
	pt.bus = VCOM_BUS_CAN0;
	pt.profile = AEP_PT_PROFILE_A;
	s_specs[s_spec_count].comp_id = AEP_COMP_ID_PT_PARSER;
	s_specs[s_spec_count].enabled = 1u;
	s_specs[s_spec_count].cfg_len = (uint8_t)sizeof(pt);
	memcpy(s_specs[s_spec_count].cfg, &pt, sizeof(pt));
	s_spec_count++;

	memset(&bus0, 0, sizeof(bus0));
	bus0.bus = VCOM_BUS_CAN0;
	s_specs[s_spec_count].comp_id = AEP_COMP_ID_PID_CLIENT;
	s_specs[s_spec_count].enabled = 1u;
	s_specs[s_spec_count].cfg_len = (uint8_t)sizeof(bus0);
	memcpy(s_specs[s_spec_count].cfg, &bus0, sizeof(bus0));
	s_spec_count++;

	memset(&bus1, 0, sizeof(bus1));
	bus1.bus = VCOM_BUS_CAN1;
	s_specs[s_spec_count].comp_id = AEP_COMP_ID_CLUSTER;
	s_specs[s_spec_count].enabled = 1u;
	s_specs[s_spec_count].cfg_len = (uint8_t)sizeof(bus1);
	memcpy(s_specs[s_spec_count].cfg, &bus1, sizeof(bus1));
	s_spec_count++;

	s_specs[s_spec_count].comp_id = AEP_COMP_ID_DASH;
	s_specs[s_spec_count].enabled = 0u;
	s_specs[s_spec_count].cfg_len = (uint8_t)sizeof(bus1);
	memcpy(s_specs[s_spec_count].cfg, &bus1, sizeof(bus1));
	s_spec_count++;
}

static void load_specs_from_store(void)
{
	uint8_t blob[64];
	size_t blen = 0u;
	size_t off;
	uint8_t n;
	uint8_t i;

	if (store_get(STORE_KEY_SLOTS, blob, sizeof(blob), &blen) != 0 || blen < 1u)
	{
		factory_specs();
		return;
	}
	off = 0u;
	n = blob[off++];
	if (n > AEP_SPEC_CAP)
	{
		factory_specs();
		return;
	}
	s_spec_count = 0u;
	for (i = 0u; i < n; i++)
	{
		comp_slot_spec_t *s;

		if (off + 4u > blen)
		{
			factory_specs();
			return;
		}
		s = &s_specs[i];
		memset(s, 0, sizeof(*s));
		s->comp_id = (uint16_t)blob[off] | ((uint16_t)blob[off + 1u] << 8);
		s->enabled = blob[off + 2u];
		s->cfg_len = blob[off + 3u];
		off += 4u;
		if ((s->cfg_len > COMP_CFG_MAX) || (off + s->cfg_len > blen))
		{
			factory_specs();
			return;
		}
		memcpy(s->cfg, &blob[off], s->cfg_len);
		off += s->cfg_len;
		s_spec_count++;
	}
}

static void supervisor_entry(ULONG arg)
{
	TX_PARAMETER_NOT_USED(arg);

	tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND * 3u);

	s_iwdg.Instance = IWDG;
	s_iwdg.Init.Prescaler = IWDG_PRESCALER_64;
	s_iwdg.Init.Window = IWDG_WINDOW_DISABLE;
	s_iwdg.Init.Reload = 2000;
	s_iwdg.Init.EWI = 0;
	if (HAL_IWDG_Init(&s_iwdg) == HAL_OK)
	{
		s_iwdg_started = 1u;
		logi("IWDG started");
	}

	while (1)
	{
		if (s_iwdg_started != 0u)
		{
			(void)HAL_IWDG_Refresh(&s_iwdg);
		}
		log_stack_hwm(&tx_app_thread);
		log_stack_hwm(&s_sup_thread);
		log_stack_hwm(&s_com_thread);
		aep_ucp_usb_log_stacks();
		tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
	}
}

static void demo_runtime_start(void)
{
	ucp_cfg_bind_t cbind;

	comp_runtime_init(&s_comp_rt);
	(void)comp_runtime_register(&s_comp_rt, aep_pt_parser_desc());
	(void)comp_runtime_register(&s_comp_rt, aep_pid_client_desc());
	(void)comp_runtime_register(&s_comp_rt, aep_cluster_mod_desc());
	(void)comp_runtime_register(&s_comp_rt, aep_dash_mod_desc());
	comp_runtime_set_materialize(&s_comp_rt, materialize_cfg, NULL);

	(void)store_init();
	load_specs_from_store();

	memset(&cbind, 0, sizeof(cbind));
	cbind.rt = &s_comp_rt;
	cbind.specs = s_specs;
	cbind.spec_count = &s_spec_count;
	cbind.spec_cap = AEP_SPEC_CAP;
	cbind.ctx_ptrs = s_ctx_ptrs;
	cbind.ctx_sizes = s_ctx_sizes;
	cbind.ctx_count = AEP_SPEC_CAP;
	cbind.before_apply = sync_ctx_arrays;
	cbind.on_applied = on_cfg_applied;
	cbind.user = NULL;
	ucp_cfg_bind(&cbind);

	sync_ctx_arrays(NULL);
	bind_signal_set();
	(void)comp_runtime_apply(&s_comp_rt, s_specs, s_spec_count, s_ctx_ptrs, s_ctx_sizes,
							 AEP_SPEC_CAP);
	logi("comp slots=%u active via runtime", (unsigned)comp_runtime_slot_count(&s_comp_rt));
}

static void com_entry(ULONG arg)
{
	vcom_can_frame_t hb;
	TX_PARAMETER_NOT_USED(arg);

	(void)tx_mutex_create(&s_vcom_mutex, "vcom", TX_INHERIT);

	s_vcom_time.now_ms = aep_now_ms;
	vcom_core_init(&s_vcom, &s_vcom_time);
	vcom_rx_demux_init(&s_demux, &s_vcom);

	aep_fdcan_phy_bind(&s_phy0, &hfdcan1);
	aep_fdcan_phy_bind(&s_phy1, &hfdcan2);
	vcom_core_set_can_phy(&s_vcom, VCOM_BUS_CAN0, &s_phy0);
	vcom_core_set_can_phy(&s_vcom, VCOM_BUS_CAN1, &s_phy1);

	aep_kline_phy_init();
	aep_kline_phy_bind(&s_kline_phy);
	vcom_core_set_kline_phy(&s_vcom, &s_kline_phy);

	if (!vcom_core_start_bus(&s_vcom, VCOM_BUS_CAN0, 500000u))
	{
		loge("CAN0 start failed");
	}
	else
	{
		logi("CAN0 up @ 500 kbit/s");
	}
	if (!vcom_core_start_bus(&s_vcom, VCOM_BUS_CAN1, 500000u))
	{
		loge("CAN1 start failed");
	}
	else
	{
		logi("CAN1 up @ 500 kbit/s");
	}
	vcom_core_start_kline(&s_vcom);

	(void)vcom_rx_demux_attach_bus(&s_demux, VCOM_BUS_CAN0);
	(void)vcom_rx_demux_attach_bus(&s_demux, VCOM_BUS_CAN1);

	memset(&hb, 0, sizeof(hb));
	hb.id = 0x100u;
	hb.dlc = 8u;
	hb.data[0] = 0xAEu;
	hb.data[1] = 0x50u;
	(void)vcom_tx_periodic(&s_vcom, VCOM_BUS_CAN0, 500u, &hb, NULL, NULL);
	hb.data[1] = 0x51u;
	(void)vcom_tx_periodic(&s_vcom, VCOM_BUS_CAN1, 500u, &hb, NULL, NULL);

	/* USB before store/comp so a later fault still leaves a CDC port for logs. */
	aep_ucp_usb_init();
	ucp_port_apps_init();
	ucp_tlm_sig_attach_table(&s_sig_table);
	logi("UCP USB up; starting store/comp (NVS=RAM demo)");

	demo_runtime_start();
	logi("store/comp started");
	s_next_tlm_ms = aep_now_ms() + AEP_TLM_PERIOD_MS;

	while (1)
	{
		uint32_t now = aep_now_ms();

		/* UCP CDC poll is on USBX app thread (aep_ucp_usb_owns_cdc path). */
		vcom_core_drain_rx(&s_vcom, VCOM_BUS_CAN0);
		vcom_core_drain_rx(&s_vcom, VCOM_BUS_CAN1);
		comp_runtime_poll(&s_comp_rt, now);
		vcom_core_tick(&s_vcom, now);

		if ((int32_t)(now - s_next_tlm_ms) >= 0)
		{
			/*
			 * Re-sample time before age/snapshot. CAN writes during drain_rx use
			 * aep_now_ms() at RX time; aging with the loop-start `now` can be
			 * slightly behind those stamps and unsigned (now - stamp) underflows
			 * → every slot looks stale while raw values still update.
			 */
			now = aep_now_ms();
			sig_table_age(&s_sig_table, now);
			sig_table_snapshot(&s_sig_table, &s_tlm_snap);
			ucp_tlm_sig_submit(&s_tlm_snap);
			s_next_tlm_ms = now + AEP_TLM_PERIOD_MS;
		}
		tx_thread_sleep(1);
	}
}

bool aep_ucp_ready(void)
{
	return aep_ucp_usb_ready();
}

UINT aep_app_init(TX_BYTE_POOL *byte_pool)
{
	CHAR *pointer;

	sig_table_init(&s_sig_table);

	(void)tx_thread_stack_error_notify(stack_error_notify);

	if (tx_byte_allocate(byte_pool, (VOID **)&pointer, AEP_SUP_STACK, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}
	if (tx_thread_create(&s_sup_thread, "aep supervisor", supervisor_entry, 0, pointer,
						 AEP_SUP_STACK, 3, 3, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
	{
		return TX_THREAD_ERROR;
	}

	if (tx_byte_allocate(byte_pool, (VOID **)&pointer, AEP_COM_STACK, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}
	if (tx_thread_create(&s_com_thread, "aep com", com_entry, 0, pointer, AEP_COM_STACK, 8, 8,
						 TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
	{
		return TX_THREAD_ERROR;
	}

	return TX_SUCCESS;
}
