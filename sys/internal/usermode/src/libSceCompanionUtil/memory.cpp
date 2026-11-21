/*
 * The library's heap: an mspace in the work memory sceCompanionUtilInit is given.
 */
#include <libkernel/libkernel.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

static void *s_heap;

int CreateHeap(void *base, SceSize size)
{
	s_heap = sceClibMspaceCreate(base, size);
	return 0;
}

int DestroyHeap(void)
{
	sceClibMspaceDestroy(s_heap);
	return 0;
}

void *Allocate(SceSize size)
{
	return sceClibMspaceMalloc(s_heap, size);
}

void Free(void *pointer)
{
	sceClibMspaceFree(s_heap, pointer);
}

} /* namespace SceCompanionUtil */
