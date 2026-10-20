/*
 * Portable component runtime — descriptor registry, slots, apply/poll.
 * kind is metadata only; behavior is ops + opaque cfg (no switch(kind)).
 */
#ifndef COMP_H_
#define COMP_H_

#include <stddef.h>
#include <stdint.h>

#include "vcom_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMP_MAX_DESCS
#define COMP_MAX_DESCS 16u
#endif
#ifndef COMP_MAX_SLOTS
#define COMP_MAX_SLOTS 8u
#endif
#ifndef COMP_CFG_MAX
#define COMP_CFG_MAX 32u
#endif

typedef enum {
	COMP_KIND_PARSER = 1,
	COMP_KIND_PID = 2,
	COMP_KIND_MODULE = 3,
	COMP_KIND_UDS_SRV = 4
} comp_kind_t;

typedef struct comp_ops {
	int (*start)(void *ctx, const void *cfg, size_t cfg_len);
	void (*stop)(void *ctx);
	void (*poll)(void *ctx, uint32_t now_ms);
	/* Hot path from RX demux (may be NULL). */
	void (*on_can_rx)(void *ctx, uint8_t bus, const vcom_can_frame_t *fr);
} comp_ops_t;

typedef struct comp_desc {
	uint16_t id;
	comp_kind_t kind;
	const comp_ops_t *ops;
	size_t ctx_size;
	uint16_t max_cfg_len;
	const char *name;
} comp_desc_t;

typedef struct comp_slot {
	const comp_desc_t *desc;
	void *ctx;
	uint8_t active;
	uint8_t enabled;
	uint8_t cfg_len;
	uint8_t cfg[COMP_CFG_MAX];
} comp_slot_t;

/* Desired slot configuration (host / NVM facing). */
typedef struct comp_slot_spec {
	uint16_t comp_id;
	uint8_t enabled;
	uint8_t cfg_len;
	uint8_t cfg[COMP_CFG_MAX];
} comp_slot_spec_t;

typedef struct comp_runtime {
	const comp_desc_t *descs[COMP_MAX_DESCS];
	uint8_t desc_count;
	comp_slot_t slots[COMP_MAX_SLOTS];
	uint8_t slot_count;
	/* Optional: glue builds runtime cfg from NVM blob before start. */
	int (*materialize_cfg)(uint16_t comp_id, const uint8_t *blob, uint8_t blob_len,
						   void *out_cfg, size_t out_cap, size_t *out_len, void *user);
	void *materialize_user;
} comp_runtime_t;

void comp_runtime_init(comp_runtime_t *rt);
int comp_runtime_register(comp_runtime_t *rt, const comp_desc_t *desc);
const comp_desc_t *comp_runtime_find_desc(const comp_runtime_t *rt, uint16_t id);

void comp_runtime_set_materialize(comp_runtime_t *rt,
								  int (*fn)(uint16_t, const uint8_t *, uint8_t, void *, size_t,
											size_t *, void *),
								  void *user);

/* Stop all, then start enabled specs (ctx buffers provided by glue). */
int comp_runtime_apply(comp_runtime_t *rt, const comp_slot_spec_t *specs, uint8_t spec_count,
					   void *const *ctx_ptrs, const size_t *ctx_sizes, uint8_t ctx_count);

void comp_runtime_stop_all(comp_runtime_t *rt);
void comp_runtime_poll(comp_runtime_t *rt, uint32_t now_ms);

/* Fan-in for components that expose on_can_rx (optional path). */
void comp_runtime_on_can_rx(comp_runtime_t *rt, uint8_t bus, const vcom_can_frame_t *fr);

uint8_t comp_runtime_slot_count(const comp_runtime_t *rt);
const comp_slot_t *comp_runtime_slot(const comp_runtime_t *rt, uint8_t index);
uint8_t comp_runtime_desc_count(const comp_runtime_t *rt);
const comp_desc_t *comp_runtime_desc_at(const comp_runtime_t *rt, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* COMP_H_ */
