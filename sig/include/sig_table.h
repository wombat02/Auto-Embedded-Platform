#ifndef SIG_TABLE_H_
#define SIG_TABLE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIG_TABLE_CAPACITY 64u

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t rate;
	uint8_t bound;
	uint8_t valid;
	uint8_t aged; /* 1 = cleared by stale_ms timeout; last raw kept */
	uint8_t dirty;
	uint8_t publish; /* 1 = visible on UCP telemetry */
	uint32_t raw_bits;
	uint32_t stamp_ms;
} sig_slot_t;

typedef struct {
	sig_slot_t slot[SIG_TABLE_CAPACITY];
	uint8_t count;
	uint8_t generation;
} sig_table_t;

typedef struct {
	uint8_t generation;
	uint8_t count;
	struct {
		uint16_t id;
		uint8_t type;
		uint8_t rate;
		uint8_t valid;
		uint8_t aged;
		uint8_t dirty;
		uint8_t publish;
		uint32_t raw_bits;
	} rec[SIG_TABLE_CAPACITY];
} sig_snapshot_t;

void sig_table_init(sig_table_t *t);
bool sig_table_bind(sig_table_t *t, uint16_t id, uint8_t type, uint8_t rate);
bool sig_table_bind_pub(sig_table_t *t, uint16_t id, uint8_t type, uint8_t rate,
						uint8_t publish);
bool sig_table_set_publish(sig_table_t *t, uint16_t id, uint8_t publish);
void sig_table_rebind_begin(sig_table_t *t);
bool sig_table_write(sig_table_t *t, uint16_t id, uint32_t raw_bits,
					 uint8_t valid, uint32_t now_ms);
/* Clear valid (keep raw) when now_ms - stamp_ms >= catalog stale_ms. */
void sig_table_age(sig_table_t *t, uint32_t now_ms);
bool sig_table_read(const sig_table_t *t, uint16_t id,
					uint32_t *raw_bits, uint8_t *valid, uint32_t *stamp_ms);
void sig_table_snapshot(const sig_table_t *t, sig_snapshot_t *out);
bool sig_table_clear_dirty(sig_table_t *t, uint16_t id);

#ifdef __cplusplus
}
#endif

#endif /* SIG_TABLE_H_ */
