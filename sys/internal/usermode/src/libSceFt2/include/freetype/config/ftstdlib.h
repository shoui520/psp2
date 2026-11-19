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
#define ft_qsort(base, count, size, compare)	ftQsort(base, count, size, compare, memory)

#endif
