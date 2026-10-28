#include "../compiler.h"
#include "string.h"
#include <libkernel/libkernel.h>

/* Keep the checked-copy caller boundary separate from its string users. */
void *sce_paf_memcpy(void *dst, const void *src, size_t count)
{
	if (dst != src)
		sceClibMemcpy_safe(dst, src, count);
	return dst;
}

void *sce_paf_memmove(void *dst, const void *src, size_t count)
{
	if (dst != src)
		sceClibMemmove(dst, src, count);
	return dst;
}

void *sce_paf_memset(void *dst, int ch, size_t count)
{
	sceClibMemset(dst, ch, count);
	return dst;
}

void *sce_paf_bcopy(const void *src, void *dst, size_t count)
{
	if (dst != src)
		return sceClibMemmove(dst, src, count);
	return dst;
}

void *sce_paf_bzero(void *dst, size_t count)
{
	return sceClibMemset(dst, 0, count);
}
