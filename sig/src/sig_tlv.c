#include "sig_tlv.h"

#include <string.h>

uint8_t sig_tlv_width(uint8_t type)
{
	switch (type)
	{
	case 0x01: /* BOOL */
	case 0x02: /* U8 */
	case 0x03: /* I8 */
	case 0x08: /* CHAR */
		return 1u;
	case 0x04: /* U16 */
	case 0x05: /* I16 */
		return 2u;
	case 0x06: /* U32 */
	case 0x07: /* I32 */
	case 0x10: /* FLAG32 */
		return 4u;
	default:
		return 0u;
	}
}

uint8_t sig_tlv_append(uint8_t *dst, uint8_t cap, uint8_t used,
					   uint16_t id, uint8_t type, uint8_t qflags,
					   uint32_t raw_bits)
{
	const uint8_t width = sig_tlv_width(type);
	const uint8_t need = (uint8_t)(4u + width);
	uint8_t *p;

	if ((dst == NULL) || (width == 0u) || ((uint16_t)used + need > cap))
	{
		return 0u;
	}

	p = &dst[used];
	p[0] = (uint8_t)(id & 0xFFu);
	p[1] = (uint8_t)((id >> 8) & 0xFFu);
	p[2] = type;
	p[3] = qflags;
	if (width == 1u)
	{
		p[4] = (uint8_t)(raw_bits & 0xFFu);
	}
	else if (width == 2u)
	{
		p[4] = (uint8_t)(raw_bits & 0xFFu);
		p[5] = (uint8_t)((raw_bits >> 8) & 0xFFu);
	}
	else
	{
		p[4] = (uint8_t)(raw_bits & 0xFFu);
		p[5] = (uint8_t)((raw_bits >> 8) & 0xFFu);
		p[6] = (uint8_t)((raw_bits >> 16) & 0xFFu);
		p[7] = (uint8_t)((raw_bits >> 24) & 0xFFu);
	}
	return need;
}

void sig_tlv_cursor_init(sig_tlv_cursor_t *c, const uint8_t *payload, uint8_t len)
{
	if (c == NULL)
	{
		return;
	}
	c->p = payload;
	c->left = (payload != NULL) ? len : 0u;
}

bool sig_tlv_next(sig_tlv_cursor_t *c, uint16_t *id, uint8_t *type,
				  uint8_t *qflags, uint32_t *raw_bits)
{
	uint8_t width;
	uint32_t raw = 0u;

	if ((c == NULL) || (c->p == NULL) || (id == NULL) || (type == NULL) || (raw_bits == NULL))
	{
		return false;
	}
	if (c->left < 4u)
	{
		return false;
	}

	*id = (uint16_t)c->p[0] | ((uint16_t)c->p[1] << 8);
	*type = c->p[2];
	if (qflags != NULL)
	{
		*qflags = c->p[3];
	}
	width = sig_tlv_width(*type);
	if (width == 0u)
	{
		/* Unknown type: stop cursor (width unknowable). */
		c->left = 0u;
		return false;
	}
	if (c->left < (uint8_t)(4u + width))
	{
		c->left = 0u;
		return false;
	}

	if (width == 1u)
	{
		raw = c->p[4];
	}
	else if (width == 2u)
	{
		raw = (uint32_t)c->p[4] | ((uint32_t)c->p[5] << 8);
	}
	else
	{
		raw = (uint32_t)c->p[4] | ((uint32_t)c->p[5] << 8) |
			  ((uint32_t)c->p[6] << 16) | ((uint32_t)c->p[7] << 24);
	}

	*raw_bits = raw;
	c->p += (4u + width);
	c->left = (uint8_t)(c->left - (4u + width));
	return true;
}
