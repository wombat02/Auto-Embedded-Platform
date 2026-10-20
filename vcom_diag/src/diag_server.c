/*
 * UDS server wrapper around iso14229 for the example board.
 */
#include "diag_server.h"
#include "diag_link.h"

#ifndef UDS_SYS
#define UDS_SYS 0
#endif
#ifndef UDS_TP_ISOTP_C
#define UDS_TP_ISOTP_C
#endif
#include "iso14229.h"

#include <string.h>

struct diag_server {
	diag_server_cfg_t cfg;
	UDSServer_t server;
	UDSTpISOTpC_t tp;
	uint8_t inited;
	uint8_t subscribed;
};

static diag_server_t s_srv;
static diag_server_t s_srv1;
static diag_server_t *s_active_srv;

diag_server_t *diag_server_instance(void)
{
	return &s_srv;
}

diag_server_t *diag_server_instance_1(void)
{
	return &s_srv1;
}

static UDSErr_t server_fn(UDSServer_t *srv, UDSEvent_t event, void *arg)
{
	diag_server_t *self = (diag_server_t *)srv->fn_data;

	switch (event)
	{
	case UDS_EVT_DiagSessCtrl:
		return UDS_PositiveResponse;
	case UDS_EVT_ReadDataByIdent:
	{
		UDSRDBIArgs_t *rdbi = (UDSRDBIArgs_t *)arg;
		const uint8_t *data = NULL;
		uint16_t len = 0u;

		if ((self == NULL) || (self->cfg.rdbi == NULL) || (rdbi == NULL))
		{
			return UDS_NRC_RequestOutOfRange;
		}
		if (!self->cfg.rdbi(rdbi->dataId, &data, &len, self->cfg.rdbi_user) ||
			(data == NULL) || (len == 0u))
		{
			return UDS_NRC_RequestOutOfRange;
		}
		/* iso14229 safe_copy returns UDS_PositiveResponse (0) or an NRC — not byte count. */
		{
			uint8_t cret = rdbi->copy(srv, data, len);

			if (cret != (uint8_t)UDS_PositiveResponse)
			{
				return (UDSErr_t)cret;
			}
		}
		return UDS_PositiveResponse;
	}
	default:
		return UDS_PositiveResponse;
	}
}

static void server_rx_cb(const vcom_can_frame_t *fr, void *user)
{
	diag_server_on_can_rx((diag_server_t *)user, fr);
}

bool diag_server_init(diag_server_t *srv, const diag_server_cfg_t *cfg)
{
	UDSErr_t err;

	if ((srv == NULL) || (cfg == NULL) || (cfg->core == NULL) || (cfg->now_ms == NULL))
	{
		return false;
	}

	memset(srv, 0, sizeof(*srv));
	srv->cfg = *cfg;
	s_active_srv = srv;
	diag_uds_set_clock(cfg->now_ms);

	/* Server listens on phys_req / func_req, answers on phys_resp. */
	err = UDSServerTpISOTpCInit(&srv->tp, cfg->phys_req_id, cfg->phys_resp_id, cfg->func_req_id);
	if (err != UDS_OK)
	{
		s_active_srv = NULL;
		return false;
	}
	srv->tp.phys_link.user_send_can_arg = srv;
	srv->tp.func_link.user_send_can_arg = srv;

	if (UDSServerInit(&srv->server) != UDS_OK)
	{
		s_active_srv = NULL;
		return false;
	}
	srv->server.tp = (UDSTp_t *)&srv->tp;
	srv->server.fn = server_fn;
	srv->server.fn_data = srv;
	srv->inited = 1u;
	return true;
}

void diag_server_deinit(diag_server_t *srv)
{
	if (srv == NULL)
	{
		return;
	}
	if (srv->subscribed != 0u)
	{
		vcom_rx_unsubscribe_user(srv->cfg.core, srv);
		srv->subscribed = 0u;
	}
	if (s_active_srv == srv)
	{
		s_active_srv = NULL;
	}
	memset(srv, 0, sizeof(*srv));
}

bool diag_server_subscribe_rx(diag_server_t *srv)
{
	if ((srv == NULL) || (srv->inited == 0u))
	{
		return false;
	}
	if (srv->subscribed != 0u)
	{
		return true;
	}
	if (!vcom_rx_subscribe(srv->cfg.core, srv->cfg.bus, srv->cfg.phys_req_id, 0x7FFu,
						   server_rx_cb, srv))
	{
		return false;
	}
	if (srv->cfg.func_req_id != 0u)
	{
		(void)vcom_rx_subscribe(srv->cfg.core, srv->cfg.bus, srv->cfg.func_req_id, 0x7FFu,
								server_rx_cb, srv);
	}
	srv->subscribed = 1u;
	return true;
}

void diag_server_on_can_rx(diag_server_t *srv, const vcom_can_frame_t *fr)
{
	if ((srv == NULL) || (fr == NULL) || (srv->inited == 0u))
	{
		return;
	}
	if (fr->id == srv->cfg.phys_req_id)
	{
		isotp_on_can_message(&srv->tp.phys_link, fr->data, fr->dlc);
	}
	else if ((srv->cfg.func_req_id != 0u) && (fr->id == srv->cfg.func_req_id))
	{
		isotp_on_can_message(&srv->tp.func_link, fr->data, fr->dlc);
	}
}

void diag_server_poll(diag_server_t *srv)
{
	if ((srv == NULL) || (srv->inited == 0u))
	{
		return;
	}
	s_active_srv = srv;
	UDSServerPoll(&srv->server);
}
