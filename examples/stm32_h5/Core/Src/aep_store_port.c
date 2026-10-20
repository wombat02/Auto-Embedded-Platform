/*
 * aep_store_port.c — demonstration store_port_* for stm32_h5.
 *
 * RAM-backed NVS only. A product port should implement these hooks on real
 * media (internal flash, external SPI/QSPI, file). See store/docs/PORTING.md
 * and examples/stm32_h5/GLUE.md.
 */
#include "store.h"

#include <string.h>

#ifndef AEP_STORE_RAM_SIZE
#define AEP_STORE_RAM_SIZE 2048u
#endif

static uint8_t s_ram[AEP_STORE_RAM_SIZE];

int store_port_region_size(size_t *out_size)
{
	if (out_size == NULL)
	{
		return -1;
	}
	*out_size = AEP_STORE_RAM_SIZE;
	return 0;
}

int store_port_read(size_t offset, void *buf, size_t len)
{
	if ((buf == NULL) || (offset + len > AEP_STORE_RAM_SIZE))
	{
		return -1;
	}
	memcpy(buf, &s_ram[offset], len);
	return 0;
}

int store_port_write(size_t offset, const void *buf, size_t len)
{
	if ((buf == NULL) || (offset + len > AEP_STORE_RAM_SIZE))
	{
		return -1;
	}
	memcpy(&s_ram[offset], buf, len);
	return 0;
}

int store_port_erase(size_t offset, size_t len)
{
	if (offset + len > AEP_STORE_RAM_SIZE)
	{
		return -1;
	}
	memset(&s_ram[offset], 0xFF, len);
	return 0;
}
