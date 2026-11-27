/*
 * zlib_glue.c: zlib allocation and checksum adapters forwarding to SceLibc and
 * SceDeflt for ScePspemu.
 */
#include "host_common.h"
#include <stdlib.h>

extern uint32_t sceDefltAdler32(uint32_t adler, const uint8_t *buf, uint32_t len);
extern uint32_t sceDefltCrc32(uint32_t crc, const uint8_t *buf, uint32_t len);

void *zcalloc(void *opaque, unsigned int items, unsigned int size)
{
	(void)opaque;
	return malloc((size_t)(items * size));
}

void zcfree(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

unsigned long adler32(unsigned long adler, const unsigned char *buf, unsigned int len)
{
	if (buf != NULL)
		return sceDefltAdler32((uint32_t)adler, buf, len);
	return 1UL;
}

unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned int len)
{
	if (buf != NULL)
		return sceDefltCrc32((uint32_t)crc, buf, len);
	return 0UL;
}
