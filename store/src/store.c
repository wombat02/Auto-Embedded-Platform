/*
 * Dual-slot CRC'd blob store. RAM holds a small key→value map; commit writes
 * the whole map as a TLV stream into alternating flash halves.
 */
#include "store.h"

#include "aep_log.h"

#include <string.h>

#define STORE_MAGIC 0xAE50u
#define STORE_MAX_BLOB 512u
#define STORE_HDR_SIZE 16u /* quad-word aligned for STM32H5 flash program */
#define STORE_MAX_KEYS 8u

typedef struct {
	uint16_t magic;
	uint16_t reserved;
	uint16_t len;
	uint16_t seq;
	uint32_t crc;
	uint32_t pad;
} store_hdr_t;

typedef struct {
	uint16_t key;
	uint16_t len;
	uint8_t data[64];
} store_kv_t;

static store_kv_t s_kv[STORE_MAX_KEYS];
static uint8_t s_kv_count;
static uint16_t s_seq;
static size_t s_region;

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
	size_t i;
	int k;

	crc = ~crc;
	for (i = 0u; i < n; i++)
	{
		crc ^= p[i];
		for (k = 0; k < 8; k++)
		{
			crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
		}
	}
	return ~crc;
}

#if defined(__GNUC__) || defined(__clang__)
#define STORE_WEAK __attribute__((weak))
#else
#define STORE_WEAK
#endif

STORE_WEAK int store_port_region_size(size_t *out_size)
{
	if (out_size != NULL)
	{
		*out_size = 0u;
	}
	return -1;
}

STORE_WEAK int store_port_read(size_t offset, void *buf, size_t len)
{
	(void)offset;
	(void)buf;
	(void)len;
	return -1;
}

STORE_WEAK int store_port_write(size_t offset, const void *buf, size_t len)
{
	(void)offset;
	(void)buf;
	(void)len;
	return -1;
}

STORE_WEAK int store_port_erase(size_t offset, size_t len)
{
	(void)offset;
	(void)len;
	return -1;
}

static store_kv_t *find_kv(uint16_t key)
{
	uint8_t i;

	for (i = 0u; i < s_kv_count; i++)
	{
		if (s_kv[i].key == key)
		{
			return &s_kv[i];
		}
	}
	return NULL;
}

static size_t pack_blob(uint8_t *out, size_t cap)
{
	size_t used = 0u;
	uint8_t i;

	for (i = 0u; i < s_kv_count; i++)
	{
		if (used + 4u + s_kv[i].len > cap)
		{
			break;
		}
		out[used++] = (uint8_t)(s_kv[i].key & 0xFFu);
		out[used++] = (uint8_t)((s_kv[i].key >> 8) & 0xFFu);
		out[used++] = (uint8_t)(s_kv[i].len & 0xFFu);
		out[used++] = (uint8_t)((s_kv[i].len >> 8) & 0xFFu);
		memcpy(&out[used], s_kv[i].data, s_kv[i].len);
		used += s_kv[i].len;
	}
	return used;
}

static int unpack_blob(const uint8_t *in, size_t len)
{
	size_t off = 0u;

	s_kv_count = 0u;
	while (off + 4u <= len)
	{
		uint16_t key = (uint16_t)in[off] | ((uint16_t)in[off + 1u] << 8);
		uint16_t klen = (uint16_t)in[off + 2u] | ((uint16_t)in[off + 3u] << 8);

		off += 4u;
		if ((klen > 64u) || (off + klen > len) || (s_kv_count >= STORE_MAX_KEYS))
		{
			return -1;
		}
		s_kv[s_kv_count].key = key;
		s_kv[s_kv_count].len = klen;
		memcpy(s_kv[s_kv_count].data, &in[off], klen);
		s_kv_count++;
		off += klen;
	}
	return 0;
}

static int slot_ok(const store_hdr_t *h, const uint8_t *blob)
{
	uint32_t c;

	if ((h->magic != STORE_MAGIC) || (h->len > STORE_MAX_BLOB))
	{
		return 0;
	}
	c = crc32_update(0u, (const uint8_t *)&h->magic, 8u);
	c = crc32_update(c, blob, h->len);
	return (c == h->crc) ? 1 : 0;
}

int store_init(void)
{
	/* Static: avoid ~1 KiB stack frames on the com thread. */
	static store_hdr_t h0;
	static store_hdr_t h1;
	static uint8_t blob[STORE_MAX_BLOB];
	int ok0 = 0;
	int ok1 = 0;

	s_kv_count = 0u;
	s_seq = 0u;
	if ((store_port_region_size(&s_region) != 0) ||
		(s_region < (2u * (STORE_HDR_SIZE + 64u))))
	{
		AEP_LOGE("store: init region fail");
		return -1;
	}

	memset(&h0, 0, sizeof(h0));
	memset(&h1, 0, sizeof(h1));
	if (store_port_read(0u, &h0, sizeof(h0)) == 0)
	{
		if ((h0.magic == STORE_MAGIC) && (h0.len <= STORE_MAX_BLOB) &&
			(store_port_read(STORE_HDR_SIZE, blob, h0.len) == 0))
		{
			ok0 = slot_ok(&h0, blob);
		}
	}
	if (store_port_read(s_region / 2u, &h1, sizeof(h1)) == 0)
	{
		if ((h1.magic == STORE_MAGIC) && (h1.len <= STORE_MAX_BLOB) &&
			(store_port_read(s_region / 2u + STORE_HDR_SIZE, blob, h1.len) == 0))
		{
			ok1 = slot_ok(&h1, blob);
		}
	}

	if (ok0 && (!ok1 || ((int16_t)(h0.seq - h1.seq) >= 0)))
	{
		if (store_port_read(STORE_HDR_SIZE, blob, h0.len) == 0)
		{
			(void)unpack_blob(blob, h0.len);
			s_seq = h0.seq;
		}
	}
	else if (ok1)
	{
		if (store_port_read(s_region / 2u + STORE_HDR_SIZE, blob, h1.len) == 0)
		{
			(void)unpack_blob(blob, h1.len);
			s_seq = h1.seq;
		}
	}
	return 0;
}

int store_get(uint16_t key, void *buf, size_t buf_len, size_t *out_len)
{
	store_kv_t *kv = find_kv(key);

	if ((buf == NULL) || (out_len == NULL) || (kv == NULL))
	{
		return -1;
	}
	if (buf_len < kv->len)
	{
		return -1;
	}
	memcpy(buf, kv->data, kv->len);
	*out_len = kv->len;
	return 0;
}

int store_set(uint16_t key, const void *buf, size_t len)
{
	store_kv_t *kv;

	if ((buf == NULL) || (len > 64u))
	{
		return -1;
	}
	kv = find_kv(key);
	if (kv == NULL)
	{
		if (s_kv_count >= STORE_MAX_KEYS)
		{
			return -1;
		}
		kv = &s_kv[s_kv_count++];
		kv->key = key;
	}
	kv->len = (uint16_t)len;
	memcpy(kv->data, buf, len);
	return 0;
}

int store_delete(uint16_t key)
{
	uint8_t i;

	for (i = 0u; i < s_kv_count; i++)
	{
		if (s_kv[i].key == key)
		{
			s_kv[i] = s_kv[s_kv_count - 1u];
			s_kv_count--;
			return 0;
		}
	}
	return 0;
}

int store_commit(void)
{
	store_hdr_t h;
	uint8_t blob[STORE_MAX_BLOB];
	size_t blen;
	size_t off;
	uint16_t seq;

	if (s_region == 0u)
	{
		return -1;
	}
	blen = pack_blob(blob, sizeof(blob));
	seq = (uint16_t)(s_seq + 1u);
	off = ((seq & 1u) != 0u) ? 0u : (s_region / 2u);

	memset(&h, 0xFF, sizeof(h));
	h.magic = STORE_MAGIC;
	h.reserved = 0u;
	h.len = (uint16_t)blen;
	h.seq = seq;
	h.crc = crc32_update(0u, (const uint8_t *)&h.magic, 8u);
	h.crc = crc32_update(h.crc, blob, blen);
	h.pad = 0u;

	/* Pad program image to 16-byte multiple (CRC uses logical len only). */
	{
		size_t prog = blen;

		while ((prog & 0xFu) != 0u)
		{
			if (prog >= sizeof(blob))
			{
				return -1;
			}
			blob[prog++] = 0xFFu;
		}
		if (store_port_erase(off, s_region / 2u) != 0)
		{
			AEP_LOGE("store: commit erase fail");
			return -1;
		}
		if (store_port_write(off, &h, sizeof(h)) != 0)
		{
			AEP_LOGE("store: commit hdr write fail");
			return -1;
		}
		if ((prog > 0u) && (store_port_write(off + STORE_HDR_SIZE, blob, prog) != 0))
		{
			AEP_LOGE("store: commit blob write fail");
			return -1;
		}
	}
	s_seq = seq;
	return 0;
}
