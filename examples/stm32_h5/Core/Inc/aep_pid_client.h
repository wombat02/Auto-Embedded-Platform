#ifndef AEP_PID_CLIENT_H_
#define AEP_PID_CLIENT_H_

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
	uint32_t (*now_ms)(void);
} aep_pid_client_cfg_t;

typedef struct {
	aep_pid_client_cfg_t cfg;
	uint8_t started;
	uint8_t phase; /* 0=need session, 1=running */
	uint8_t did_idx;
	uint8_t last_op; /* 0=none, 1=session, 2=tp, 3=rdbi */
	uint32_t next_req_ms;
	uint32_t next_tp_ms;
	uint8_t pending;
} aep_pid_client_ctx_t;

const aep_comp_desc_t *aep_pid_client_desc(void);

#ifdef __cplusplus
}
#endif

#endif /* AEP_PID_CLIENT_H_ */
