#include "new.h"
#include "stdlib.h"

void *PafNew(size_t size)
{
	return sce_paf_malloc(size);
}

void *PafNewArray(size_t size)
{
	return sce_paf_malloc(size);
}

void *PafAlignedNew(size_t size, size_t align)
{
	return sce_paf_memalign(align, size);
}

void *PafAlignedNewArray(size_t size, size_t align)
{
	return sce_paf_memalign(align, size);
}

void PafDelete(void *ptr)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafDeleteArray(void *ptr)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafAlignedDelete(void *ptr, size_t size, size_t align)
{
	if (ptr)
		sce_paf_free(ptr);
}

void PafAlignedDeleteArray(void *ptr, size_t size, size_t align)
{
	if (ptr)
		sce_paf_free(ptr);
}

void *PafPlacementNew(size_t size, void *where)
{
	return where;
}

void __cxa_pure_virtual(void)
{
}
