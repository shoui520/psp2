/*
 * SceNpCommonPs4: Handle (npext/handle/handle.cpp), a reference counted object that is a Cancelable.
 * The Cancelable is its second base, at offset 4.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_handleVtable[];		/* 0x8102a6d0 */

struct NpCommonPs4Handle *SceNpCommonPs4_2E2B2655(struct NpCommonPs4Handle *handle)
{
	handle->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	SceNpCommonPs4_EB1FBB74(&handle->cancelable);
	handle->vtable = NP_COMMON_PS4_VTABLE(g_handleVtable);
	handle->cancelable.vtable = (&g_handleVtable[7]);
	handle->refCount = 0;
	return handle;
}

struct NpCommonPs4Handle *SceNpCommonPs4_2BC6CE6A(struct NpCommonPs4Handle *handle)
{
	handle->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	SceNpCommonPs4_EB1FBB74(&handle->cancelable);
	handle->vtable = NP_COMMON_PS4_VTABLE(g_handleVtable);
	handle->cancelable.vtable = (&g_handleVtable[7]);
	handle->refCount = 0;
	return handle;
}

__attribute__((noinline)) struct NpCommonPs4Handle *SceNpCommonPs4_62214062(struct NpCommonPs4Handle *handle)
{
	int refCount;

	refCount = handle->refCount;
	handle->vtable = NP_COMMON_PS4_VTABLE(g_handleVtable);
	handle->cancelable.vtable = (&g_handleVtable[7]);
	if (refCount != 0)
		NP_COMMON_PS4_ASSERT(0, "~Handle", "npext\\handle\\handle.cpp", 28, "m_refCount == 0");
	SceNpCommonPs4_C6E3FE21(&handle->cancelable);
	handle->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return handle;
}

__attribute__((noinline)) int SceNpCommonPs4_D4330FB2(struct NpCommonPs4Handle *handle)
{
	int refCount;

	refCount = handle->refCount;
	handle->vtable = NP_COMMON_PS4_VTABLE(g_handleVtable);
	handle->cancelable.vtable = (&g_handleVtable[7]);
	if (refCount != 0)
		NP_COMMON_PS4_ASSERT(0, "~Handle", "npext\\handle\\handle.cpp", 28, "m_refCount == 0");
	SceNpCommonPs4_C6E3FE21(&handle->cancelable);
	handle->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(handle);
}

struct NpCommonPs4Handle *SceNpCommonPs4_58041BD5(struct NpCommonPs4Handle *handle)
{
	int refCount;

	refCount = handle->refCount;
	handle->vtable = NP_COMMON_PS4_VTABLE(g_handleVtable);
	handle->cancelable.vtable = (&g_handleVtable[7]);
	if (refCount != 0)
		NP_COMMON_PS4_ASSERT(0, "~Handle", "npext\\handle\\handle.cpp", 28, "m_refCount == 0");
	SceNpCommonPs4_C6E3FE21(&handle->cancelable);
	handle->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return handle;
}

int SceNpCommonPs4_D86527FB(struct NpCommonPs4Handle *handle)
{
	int result;

	if (handle->refCount != 0)
		NP_COMMON_PS4_ASSERT(0, "Init", "npext\\handle\\handle.cpp", 36, "m_refCount == 0");
	result = SceNpCommonPs4_2CA5AE6A(&handle->cancelable);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_6B8D9291(struct NpCommonPs4Handle *handle)
{
	if (handle->refCount != 0)
		NP_COMMON_PS4_ASSERT(0, "Destroy", "npext\\handle\\handle.cpp", 55, "m_refCount == 0");
	return SceNpCommonPs4_9BCAE348(&handle->cancelable);
}

int SceNpCommonPs4_FC2480E5(struct NpCommonPs4Handle *handle)
{
	return SceNpCommonPs4_3AAA7DF7(&handle->cancelable);
}

__attribute__((noinline)) void SceNpCommonPs4_26AA30C8(void)
{
}

int SceNpCommonPs4_F6977C9F(struct NpCommonPs4Cancelable *cancelable)
{
	return SceNpCommonPs4_D4330FB2((struct NpCommonPs4Handle *)((char *)cancelable - 4));
}

struct NpCommonPs4Handle *SceNpCommonPs4_86C253E9(struct NpCommonPs4Cancelable *cancelable)
{
	return SceNpCommonPs4_62214062((struct NpCommonPs4Handle *)((char *)cancelable - 4));
}

void SceNpCommonPs4_A486D855(void)
{
	SceNpCommonPs4_26AA30C8();
}

const void *g_handleVtable[] = {		/* 0x8102a6d0: the vtables of Handle and of its Cancelable base */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_62214062,
	(const void *)SceNpCommonPs4_D4330FB2,
	(const void *)SceNpCommonPs4_26AA30C8,
	(const void *)-4,
	NULL,
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)SceNpCommonPs4_86C253E9,
	(const void *)SceNpCommonPs4_F6977C9F,
	(const void *)SceNpCommonPs4_A486D855,
};

}
