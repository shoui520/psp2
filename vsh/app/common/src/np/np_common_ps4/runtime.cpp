/*
 * SceNpCommonPs4: the C++ runtime the library carries: operator new and delete over the runtime's mspace.
 */

#include "np_common_ps4_internal.h"

extern "C" void *g_runtimeMspace;		/* 0x8102b160 */

extern "C" void npCommonPs4Breakpoint(void)
{
	__builtin_trap();
}

void *operator new(SceSize size)
{
	return sceClibMspaceMalloc(g_runtimeMspace, size);
}

extern "C" __attribute__((noinline)) void npCommonPs4RuntimeFree(void *ptr)
{
	sceClibMspaceFree(g_runtimeMspace, ptr);
}

void operator delete(void *ptr)
{
	npCommonPs4RuntimeFree(ptr);
}

extern "C" {

void *SceNpCommonPs4_31451392(void **object);
int SceNpCommonPs4_B467D873(void **object);

const void *g_baseVtable[] = {		/* 0x8102a6b0 */
	(const void *)SceNpCommonPs4_31451392,
	(const void *)SceNpCommonPs4_B467D873,
	NULL,
};

void *SceNpCommonPs4_31451392(void **object)
{
	*object = (void *)g_baseVtable;
	return object;
}

int SceNpCommonPs4_B467D873(void **object)
{
	*object = (void *)g_baseVtable;
	return SceNpCommonPs4_BB5282C8(object);
}

}
