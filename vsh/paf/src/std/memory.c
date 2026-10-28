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
