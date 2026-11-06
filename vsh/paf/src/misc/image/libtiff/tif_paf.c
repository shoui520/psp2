/*
 * libpaf's libtiff platform functions (upstream's tif_unix.c and its kin): the memory functions
 * over paf's. There is no _TIFFmalloc, _TIFFrealloc or _TIFFfree: every allocation goes through
 * the allocator TIFFClientOpen was given (tiffiop.h), and no file is opened by name. File name
 * ours.
 */
#include "tiffiop.h"

void _TIFFmemset(tdata_t p, int v, tsize_t c)
{
	sce_paf_memset(p, v, (size_t)c);
}

void _TIFFmemcpy(tdata_t d, const tdata_t s, tsize_t c)
{
	sce_paf_memcpy(d, s, (size_t)c);
}

int _TIFFmemcmp(const tdata_t p1, const tdata_t p2, tsize_t c)
{
	return sce_paf_memcmp(p1, p2, (size_t)c);
}
