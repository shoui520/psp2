#include "new.h"
#include "stdlib.h"

/* Only the import declaration is needed; the broad SDK header uses C11. */
extern int sceClibPrintf(const char *fmt, ...);

/* Sony exports this as __cxa_deleted_virtual (NID 0x25A6D9BB). */
int ScePafStdc_25A6D9BB(void)
{
	int result = sceClibPrintf(
		"[PAF C++ Runtime] __cxa_deleted_virtual() is called\n");
	__breakpoint(0);
	return result;
}

int ScePafStdc_D608246A(void)
{
	ScePafStdc_D41FD265();
	__breakpoint(0);
	return -1;
}

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
