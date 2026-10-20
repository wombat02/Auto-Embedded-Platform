/*
 * Example app entry (ThreadX). Orchestration lives in aep_app.c; PHY/port/apps
 * are split across aep_fdcan_phy / aep_kline_phy / aep_ucp_port / aep_ucp_*.
 * See examples/stm32_h5/GLUE.md and BRINGUP.md.
 */
#ifndef AEP_APP_H_
#define AEP_APP_H_

#include "tx_api.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Bring-up gate used only in the Cube paste for App_ThreadX_Init
 * (examples/stm32_h5/BRINGUP.md). 0 = call aep_app_init (default).
 * 1 = LED/CDC only; do not start portable glue.
 */
#ifndef AEP_BRINGUP_MINIMAL
#define AEP_BRINGUP_MINIMAL 0
#endif

UINT aep_app_init(TX_BYTE_POOL *byte_pool);
bool aep_ucp_ready(void);

#endif /* AEP_APP_H_ */
