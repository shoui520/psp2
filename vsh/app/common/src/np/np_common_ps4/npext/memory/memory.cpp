/*
 * SceNpCommonPs4: allocation of objects with a header that names their allocator, and objects that count their
 * references.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_refCountedVtable[];	/* 0x8102a8d0 */

void *SceNpCommonPs4_69DBBE0D(SceSize size, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4Allocator *allocator;
	struct NpCommonPs4Allocator **memory;

	allocator = SceNpCommonPs4_31DB2D97(pool);
	memory = (struct NpCommonPs4Allocator **)allocator->allocate(size + 16, allocator->context);
	if (memory != NULL) {
		*memory = allocator;
		return memory + 4;
	}
	return NULL;
}

void SceNpCommonPs4_9D76B15D(void *pointer)
{
	struct NpCommonPs4Allocator **memory;

	if (pointer != NULL) {
		memory = (struct NpCommonPs4Allocator **)pointer - 4;
		(*memory)->deallocate(memory, (*memory)->context);
	}
}

void SceNpCommonPs4_CAB3F18A(void *pointer)
{
	struct NpCommonPs4Allocator **memory;

	if (pointer != NULL) {
		memory = (struct NpCommonPs4Allocator **)pointer - 4;
		(*memory)->deallocate(memory, (*memory)->context);
	}
}

void SceNpCommonPs4_F791AD2F(void *pointer)
{
	struct NpCommonPs4Allocator **memory;

	if (pointer != NULL) {
		memory = (struct NpCommonPs4Allocator **)pointer - 4;
		(*memory)->deallocate(memory, (*memory)->context);
	}
}

struct NpCommonPs4RefCounted *SceNpCommonPs4_C4A4D856(struct NpCommonPs4RefCounted *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_refCountedVtable);
	object->refCount = 0;
	return object;
}

struct NpCommonPs4RefCounted *SceNpCommonPs4_108738F2(struct NpCommonPs4RefCounted *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_refCountedVtable);
	object->refCount = 0;
	return object;
}

struct NpCommonPs4RefCounted *SceNpCommonPs4_039CE16B(struct NpCommonPs4RefCounted *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

int SceNpCommonPs4_F2F4266F(struct NpCommonPs4RefCounted *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(object);
}

struct NpCommonPs4RefCounted *SceNpCommonPs4_BA23AD76(struct NpCommonPs4RefCounted *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

int SceNpCommonPs4_64BAC888(struct NpCommonPs4RefCounted *object)
{
	return SceNpCommonPs4_741F88FE(&object->refCount) + 1;
}

int SceNpCommonPs4_93C97239(struct NpCommonPs4RefCounted *object)
{
	SceUInt32 count;
	int result;

	count = SceNpCommonPs4_7B512A98(&object->refCount);
	result = count - 1;
	if (count == 1 && object != NULL)
		((void (*)(void *))object->vtable[1])(object);
	return result;
}

const void *g_refCountedVtable[] = {		/* 0x8102a8d0 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_039CE16B,
	(const void *)SceNpCommonPs4_F2F4266F,
};

}
