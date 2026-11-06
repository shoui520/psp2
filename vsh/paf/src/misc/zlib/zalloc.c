/*
 * The default allocator of libpaf's zlib (Sony's change to zutil.c, built here with zlib's own
 * MY_ZCALLOC): zcalloc and zcfree go through two hooks that nothing in libpaf sets, so without
 * them zcalloc fails and zcfree does nothing. Every user passes its own zalloc and zfree
 * (libpng's png_zalloc, paf::compress's Alloc and Free). Names ours.
 */
#include <stddef.h>
#include "zutil.h"

void *(*zlibAllocHook)(size_t size) = NULL;
void (*zlibFreeHook)(void *ptr) = NULL;

voidpf ZLIB_INTERNAL zcalloc(voidpf opaque, unsigned items, unsigned size)
{
	if (zlibAllocHook != NULL)
		return zlibAllocHook(items * size);

	return Z_NULL;
}

void ZLIB_INTERNAL zcfree(voidpf opaque, voidpf ptr)
{
	if (zlibFreeHook != NULL)
		zlibFreeHook(ptr);
}
