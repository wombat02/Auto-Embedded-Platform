/*
 * diag_link — wraps iso14229 client + isotp-c over vcom TX/RX hooks.
 */
#include "diag_link.h"

#ifndef UDS_SYS
#define UDS_SYS 0 /* UDS_SYS_CUSTOM */
#endif
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#include "iso14229.h"

#include <stdarg.h>
#include <string.h>

struct diag_link {
	diag_link_cfg_t cfg;
	UDSClient_t client;
	UDSTpISOTpC_t tp;
	uint8_t inited;
	uint8_t subscribed;
};

static diag_link_t s_link;
static diag_link_t *s_active;
static uint32_t (*s_clock_ms)(void);

diag_link_t *diag_link_instance(void)
{
	return &s_link;
}

void diag_uds_set_clock(uint32_t (*now_ms)(void))
{
	s_clock_ms = now_ms;
}

uint32_t UDSMillis(void)
{
	if ((s_active != NULL) && (s_active->cfg.now_ms != NULL))
	{
		return s_active->cfg.now_ms();
	}
	if (s_clock_ms != NULL)
	{
		return s_clock_ms();
	}
	return 0u;
}

uint32_t isotp_user_get_us(void)
{
	return UDSMillis() * 1000u;
}

void isotp_user_debug(const char *message, ...)
{
	(void)message;
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size
#ifdef ISO_TP_USER_SEND_CAN_FLAGS
						,
						const uint8_t flags
#endif
#ifdef ISO_TP_USER_SEND_CAN_ARG
						,
						void *arg
#endif
)
{
	vcom_can_frame_t fr;
	/* diag_link_t and diag_server_t both start with a cfg whose first fields are core, bus. */
	typedef struct {
		vcom_core_t *core;
		uint8_t bus;
	} vcom_tx_prefix_t;
	typedef struct {
		vcom_tx_prefix_t cfg;
	} vcom_tx_owner_t;
	vcom_tx_owner_t *owner = (vcom_tx_owner_t *)arg;

#ifdef ISO_TP_USER_SEND_CAN_FLAGS
	(void)flags;
#endif

	if ((owner == NULL) || (owner->cfg.core == NULL) || (data == NULL) || (size > 8u))
	{
		return ISOTP_RET_ERROR;
	}

	memset(&fr, 0, sizeof(fr));
	fr.id = arbitration_id;
	fr.ide = 0u;
	fr.dlc = size;
	memcpy(fr.data, data, size);

	/* Locked post — ISO-TP may run from a timer while the com thread ticks TX. */
	if (!vcom_tx_post(owner->cfg.core, owner->cfg.bus, &fr))
	{
		return ISOTP_RET_NOSPACE;
	}
	return ISOTP_RET_OK;
}

static void diag_rx_cb(const vcom_can_frame_t *fr, void *user)
{
	diag_link_on_can_rx((diag_link_t *)user, fr);
}

bool diag_link_init(diag_link_t *link, const diag_link_cfg_t *cfg)
{
	UDSErr_t err;

	if ((link == NULL) || (cfg == NULL) || (cfg->core == NULL) || (cfg->now_ms == NULL))
	{
		return false;
	}

	memset(link, 0, sizeof(*link));
	link->cfg = *cfg;
	s_active = link;

	err = UDSClientTpISOTpCInit(&link->tp, cfg->phys_resp_id, cfg->phys_req_id, cfg->func_req_id);
	if (err != UDS_OK)
	{
		s_active = NULL;
		return false;
	}

	/* Pass link as send arg for both phys and func links. */
	link->tp.phys_link.user_send_can_arg = link;
	link->tp.func_link.user_send_can_arg = link;

	if (UDSClientInit(&link->client) != UDS_OK)
	{
		s_active = NULL;
		return false;
	}
	link->client.tp = (UDSTp_t *)&link->tp;
	link->inited = 1u;
	return true;
}

void diag_link_deinit(diag_link_t *link)
{
	if (link == NULL)
	{
		return;
	}
	if (link->subscribed != 0u)
	{
		vcom_rx_unsubscribe_user(link->cfg.core, link);
		link->subscribed = 0u;
	}
	if (s_active == link)
	{
		s_active = NULL;
	}
	memset(link, 0, sizeof(*link));
}

bool diag_link_subscribe_rx(diag_link_t *link)
{
	if ((link == NULL) || (link->inited == 0u))
	{
		return false;
	}
	if (link->subscribed != 0u)
	{
		return true;
	}
	if (!vcom_rx_subscribe(link->cfg.core, link->cfg.bus, link->cfg.phys_resp_id, 0x7FFu,
						   diag_rx_cb, link))
	{
		return false;
	}
	link->subscribed = 1u;
	return true;
}

void diag_link_on_can_rx(diag_link_t *link, const vcom_can_frame_t *fr)
{
	if ((link == NULL) || (fr == NULL) || (link->inited == 0u))
	{
		return;
	}
	if (fr->id != link->cfg.phys_resp_id)
	{
		return;
	}
	isotp_on_can_message(&link->tp.phys_link, fr->data, fr->dlc);
}

bool diag_link_poll(diag_link_t *link)
{
	if ((link == NULL) || (link->inited == 0u))
	{
		return false;
	}
	s_active = link;
	(void)UDSClientPoll(&link->client);
	return (link->client.state != 0); /* non-zero = busy-ish; exact enum varies */
}

bool diag_link_session_default(diag_link_t *link)
{
	if ((link == NULL) || (link->inited == 0u))
	{
		return false;
	}
	s_active = link;
	return UDSSendDiagSessCtrl(&link->client, 0x01u) == UDS_OK;
}

bool diag_link_tester_present(diag_link_t *link)
{
	if ((link == NULL) || (link->inited == 0u))
	{
		return false;
	}
	s_active = link;
	return UDSSendTesterPresent(&link->client) == UDS_OK;
}

bool diag_link_read_did(diag_link_t *link, const uint16_t *dids, uint16_t count)
{
	if ((link == NULL) || (link->inited == 0u) || (dids == NULL) || (count == 0u))
	{
		return false;
	}
	s_active = link;
	return UDSSendRDBI(&link->client, dids, count) == UDS_OK;
}

void *diag_link_client(diag_link_t *link)
{
	if ((link == NULL) || (link->inited == 0u))
	{
		return NULL;
	}
	return &link->client;
}
