/*
 * Portable keyed blob store. Media layout via store_port_* hooks (glue).
 */
#ifndef STORE_H_
#define STORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Well-known keys for AEP config. */
#define STORE_KEY_SLOTS 0x0001u
#define STORE_KEY_PACK_ADS 0x0002u

int store_init(void);
int store_get(uint16_t key, void *buf, size_t buf_len, size_t *out_len);
int store_set(uint16_t key, const void *buf, size_t len);
int store_delete(uint16_t key);
int store_commit(void);

/* Port (implemented by glue; weak stubs return errors). */
int store_port_region_size(size_t *out_size);
int store_port_read(size_t offset, void *buf, size_t len);
int store_port_write(size_t offset, const void *buf, size_t len);
int store_port_erase(size_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STORE_H_ */
