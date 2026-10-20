#ifndef SIG_TLV_H_
#define SIG_TLV_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-record quality flags (byte after type). */
#define SIG_TLV_Q_VALID 0x01u
#define SIG_TLV_Q_AGED 0x02u

uint8_t sig_tlv_width(uint8_t type);

/*
 * Append one record: id[2 LE] | type | qflags | raw[W LE].
 * Returns bytes written, or 0 if it does not fit / type unknown.
 */
uint8_t sig_tlv_append(uint8_t *dst, uint8_t cap, uint8_t used,
					   uint16_t id, uint8_t type, uint8_t qflags,
					   uint32_t raw_bits);

typedef struct {
	const uint8_t *p;
	uint8_t left;
} sig_tlv_cursor_t;

void sig_tlv_cursor_init(sig_tlv_cursor_t *c, const uint8_t *payload, uint8_t len);

/*
 * Returns false at end, or if the next header/type would run past `left`,
 * or if type width is unknown (cursor stops).
 * Unknown id with known type: returns true (caller decides).
 * qflags may be NULL if the caller ignores quality.
 */
bool sig_tlv_next(sig_tlv_cursor_t *c, uint16_t *id, uint8_t *type,
				  uint8_t *qflags, uint32_t *raw_bits);

#ifdef __cplusplus
}
#endif

#endif /* SIG_TLV_H_ */
