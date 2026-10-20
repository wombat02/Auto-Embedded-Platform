/*
 * aep_ucp_port.c — example overrides for portable ucp_port_* hooks.
 *
 * Time: ThreadX ticks. Send: aep_ucp_usb.
 * Apps: ucp_xfer (route only), ucp_tlm (0x20), ucp_cfg (0x71).
 * Log: strong aep_port_log → USB ring (logi/logw/loge).
 * Firmware leaves ops NULL and links these strong symbols instead of
 * ucp_port_set_ops() used by host tests.
 *
 * See examples/stm32_h5/GLUE.md and ucp/docs/PORTING.md.
 */
#include "aep_ucp_port.h"

#include "aep_log.h"
#include "aep_ucp_usb.h"
#include "log.h"
#include "ucp_cfg.h"
#include "ucp_port.h"
#include "ucp_tlm_sig.h"
#include "ucp_xfer.h"
#include "tx_api.h"

#include <string.h>

/* Index 0 = xfer (route/tick only; omitted from CAP descriptors). */
static ucp_app_vtable_t s_app_registry[3];
static const uint8_t s_app_count = 3u;
static uint8_t s_registry_ready;

uint32_t aep_now_ms(void)
{
	return (uint32_t)tx_time_get();
}

uint32_t ucp_port_get_time_ms(void)
{
	return aep_now_ms();
}

bool ucp_port_send_frame(const uint8_t *frame, uint16_t len)
{
	return aep_ucp_usb_send_frame(frame, len);
}

ucp_hw_id_t ucp_port_get_local_hw_id(void)
{
	return UCP_HW_ID_DEVBOARD;
}

static void ensure_registry(void)
{
	if (s_registry_ready != 0u)
	{
		return;
	}
	s_app_registry[0] = g_ucp_xfer_app_vtable;
	s_app_registry[1] = g_ucp_tlm_sig_app_vtable;
	s_app_registry[2] = g_ucp_cfg_app_vtable;
	s_registry_ready = 1u;
}

uint8_t ucp_port_get_app_descriptors(ucp_app_version_entry_t *out_entries, uint8_t max_entries)
{
	uint8_t n = 0u;
	uint8_t i;

	if ((out_entries == NULL) || (max_entries == 0u))
	{
		return 0u;
	}
	ensure_registry();
	/* Skip transport-owned xfer (APP 0x01) — not CAP-negotiated. */
	for (i = 0u; (i < s_app_count) && (n < max_entries); i++)
	{
		if (s_app_registry[i].app_id == (uint8_t)UCP_APP_ID_XFER)
		{
			continue;
		}
		out_entries[n].app_id = s_app_registry[i].app_id;
		out_entries[n].version = s_app_registry[i].version;
		out_entries[n].role_flags = s_app_registry[i].role_flags;
		n++;
	}
	return n;
}

uint8_t ucp_port_get_app_registry(const ucp_app_vtable_t **out_registry, uint8_t max_entries)
{
	if ((out_registry == NULL) || (max_entries == 0u))
	{
		return 0u;
	}
	ensure_registry();
	*out_registry = s_app_registry;
	return (s_app_count <= max_entries) ? s_app_count : max_entries;
}

void ucp_port_apps_init(void)
{
	uint8_t i;
	ensure_registry();
	for (i = 0u; i < s_app_count; i++)
	{
		logi("UCP app 0x%02X ver=0x%04X", (unsigned)s_app_registry[i].app_id,
			 (unsigned)s_app_registry[i].version);
		if (s_app_registry[i].init != NULL)
		{
			s_app_registry[i].init();
		}
	}
}

void ucp_port_apps_tick(uint32_t now_ms, bool link_ready, uint32_t scheduler_flags)
{
	uint8_t i;
	(void)scheduler_flags;
	ensure_registry();
	for (i = 0u; i < s_app_count; i++)
	{
		if (s_app_registry[i].on_tick != NULL)
		{
			s_app_registry[i].on_tick(now_ms, link_ready);
		}
	}
}

void ucp_port_route_application_packet(const ucp_pkt_t *pkt)
{
	uint8_t i;
	if (pkt == NULL)
	{
		return;
	}
	ensure_registry();
	for (i = 0u; i < s_app_count; i++)
	{
		if ((s_app_registry[i].app_id == pkt->app_id) && (s_app_registry[i].on_rx != NULL))
		{
			s_app_registry[i].on_rx(pkt);
			return;
		}
	}
}

void ucp_port_on_link_event(ucp_app_link_event_t event)
{
	uint8_t i;
	ensure_registry();
	for (i = 0u; i < s_app_count; i++)
	{
		if (s_app_registry[i].on_link_event != NULL)
		{
			s_app_registry[i].on_link_event(event);
		}
	}
}

void aep_port_log(aep_log_level_t level, const char *msg, size_t len)
{
	char buf[LOG_MSG_SIZE];
	size_t n;

	if ((msg == NULL) || (len == 0u))
	{
		return;
	}

	n = len;
	if (n >= sizeof(buf))
	{
		n = sizeof(buf) - 1u;
	}
	memcpy(buf, msg, n);
	buf[n] = '\0';

	switch (level)
	{
	case AEP_LOG_ERROR:
		loge("%s", buf);
		break;
	case AEP_LOG_WARN:
		logw("%s", buf);
		break;
	case AEP_LOG_DEBUG:
	case AEP_LOG_INFO:
	default:
		logi("%s", buf);
		break;
	}
}
