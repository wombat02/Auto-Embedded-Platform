#ifndef AEP_DASH_MOD_H_
#define AEP_DASH_MOD_H_

#include "aep_comp.h"
#include "aep_demo_pt.h"
#include "sig_table.h"
#include "vcom_core.h"
#include "vcom_rx_demux.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	vcom_core_t *core;
	vcom_rx_demux_t *demux;
	sig_table_t *table;
	uint8_t bus;
} aep_dash_mod_cfg_t;

typedef struct {
	aep_dash_mod_cfg_t cfg;
	uint8_t started;
} aep_dash_mod_ctx_t;

const aep_comp_desc_t *aep_dash_mod_desc(void);

#ifdef __cplusplus
}
#endif

#endif /* AEP_DASH_MOD_H_ */
