#ifndef AEP_CLUSTER_MOD_H_
#define AEP_CLUSTER_MOD_H_

#include "aep_comp.h"
#include "aep_demo_pt.h"
#include "sig_table.h"
#include "vcom_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	vcom_core_t *core;
	sig_table_t *table;
	uint8_t bus;
} aep_cluster_mod_cfg_t;

typedef struct {
	aep_cluster_mod_cfg_t cfg;
	uint8_t started;
	int tx_slot;
} aep_cluster_mod_ctx_t;

const aep_comp_desc_t *aep_cluster_mod_desc(void);

#ifdef __cplusplus
}
#endif

#endif /* AEP_CLUSTER_MOD_H_ */
