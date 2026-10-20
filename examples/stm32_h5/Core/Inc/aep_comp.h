/*
 * Compatibility shim — prefer portable comp.h.
 * kind remains metadata; ops include on_can_rx.
 */
#ifndef AEP_COMP_H_
#define AEP_COMP_H_

#include "comp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef comp_kind_t aep_comp_kind_t;
#define AEP_COMP_PARSER COMP_KIND_PARSER
#define AEP_COMP_PID COMP_KIND_PID
#define AEP_COMP_MODULE COMP_KIND_MODULE
#define AEP_COMP_UDS_SRV COMP_KIND_UDS_SRV

typedef comp_ops_t aep_comp_ops_t;
typedef comp_desc_t aep_comp_desc_t;
typedef comp_slot_t aep_comp_slot_t;

#ifdef __cplusplus
}
#endif

#endif /* AEP_COMP_H_ */
