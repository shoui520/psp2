/*
 * Sony's ftstdlib.h: FreeType's own, with ft_qsort taking the memory object its allocator
 * buffers come from (the callers' FT_Memory in scope).
 */
#ifndef SCE_FTSTDLIB_H
#define SCE_FTSTDLIB_H

#include_next <freetype/config/ftstdlib.h>

#undef ft_qsort
extern void ftQsort(void *pBase, int count, unsigned int size,
		int (*compare)(const void *, const void *), void *pMemory);
/* ft_qsort( base, count, size, compare ) takes the memory object in scope; a caller without one passes its own */
#define FT_QSORT_(base, count, size, compare, pMemory, ...)	ftQsort(base, count, size, compare, pMemory)
#define ft_qsort(base, count, size, ...)	FT_QSORT_(base, count, size, __VA_ARGS__, memory)

#ifdef PAF_STATIC_BUILD
#undef ft_memcpy
extern void *ft2_memcpy(void *dst, const void *src, unsigned int n);
#define ft_memcpy	ft2_memcpy

#undef ft_memset
extern void *ft2_memset(void *dst, int ch, unsigned int len);
#define ft_memset	ft2_memset
#endif

#endif
