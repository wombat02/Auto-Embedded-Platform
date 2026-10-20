/*
 * UCP config application — APP 0x71 / version 0x0100 (V1.0), REQ_RSP.
 * List descriptors, get/set slots, apply, save/load, pack ads.
 */
#ifndef UCP_CFG_H_
#define UCP_CFG_H_

#include "comp.h"
#include "ucp_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCP_CFG_APP_ID 0x71u
#define UCP_CFG_VERSION 0x0100u

#define UCP_CFG_OP_LIST_DESCS 0x01u
#define UCP_CFG_OP_GET_SLOTS 0x02u
#define UCP_CFG_OP_SET_SLOT 0x03u
#define UCP_CFG_OP_APPLY 0x04u
#define UCP_CFG_OP_SAVE 0x05u
#define UCP_CFG_OP_LOAD 0x06u
#define UCP_CFG_OP_GET_ADS 0x07u

typedef struct {
	uint16_t pack_id;
	uint16_t revision;
} ucp_cfg_pack_ad_t;

#ifndef UCP_CFG_MAX_ADS
#define UCP_CFG_MAX_ADS 4u
#endif

typedef struct {
	comp_runtime_t *rt;
	/* Working slot specs (edited by SET_SLOT; APPLY/SAVE use these). */
	comp_slot_spec_t *specs;
	uint8_t *spec_count;
	uint8_t spec_cap;
	void *const *ctx_ptrs;
	const size_t *ctx_sizes;
	uint8_t ctx_count;
	/* Optional pack advertisements (Tier B). */
	ucp_cfg_pack_ad_t ads[UCP_CFG_MAX_ADS];
	uint8_t ad_count;
	/* Optional: sync ctx pointers to specs before apply. */
	void (*before_apply)(void *user);
	/* Called after successful apply (e.g. rebind signals). */
	void (*on_applied)(void *user);
	void *user;
} ucp_cfg_bind_t;

void ucp_cfg_bind(const ucp_cfg_bind_t *bind);
void ucp_cfg_set_ads(const ucp_cfg_pack_ad_t *ads, uint8_t count);

extern const ucp_app_vtable_t g_ucp_cfg_app_vtable;

#ifdef __cplusplus
}
#endif

#endif /* UCP_CFG_H_ */
