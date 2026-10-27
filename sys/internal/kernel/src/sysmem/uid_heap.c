/*
 * SceSysmem: SceUIDHeapClass, the base of the heaps kernel objects are
 * allocated from, and the constructors of the objects made from them. Sony's
 * uid_heap.c (NSKBL's records name it); in os0 its functions are the range
 * before uid_class.c's.
 */

#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include "sysmem_internal.h"

/* SceUIDHeapClass */
static UIDHeapClass *s_uidHeapClass;

/* name unknown: SceUIDHeapClass's constructor and destructor */
static int UIDHeapCreate(void *object)
{
	((FixedHeap *)object)->owner = NULL;
	return 0;
}

/* name unknown: the constructor of an address space object (address_space.c's class) */
int AddressSpaceObjectCreate(void *object)
{
	AddressSpaceObject *as = object;
	unsigned int i;

	as->refCount = 0;
	as->lock = 0;
	as->type = 0;
	for (i = 0; i < 32; i++) {
		as->partition[i] = NULL;
		as->partitionUid[i] = -1;
	}
	for (i = 0; i < 16; i++)
		as->phyMemPart[i] = NULL;
	as->userIOTimerBase = 0;
	as->magic = ADDRESS_SPACE_MAGIC;
	as->userIOPA = -1;
	as->userIOTimer = -1;
	return 0;
}

/* the constructor of a fixed heap (FixedHeapInitClass) */
int UIDHeapObjectCreate(void *object)
{
	((FixedHeap *)object)->magic = FIXED_HEAP_MAGIC;
	return 0;
}

static int UIDHeapDestroy(void *object)
{
	((FixedHeap *)object)->owner = NULL;
	return 0;
}

/* name unknown: the destructor of an address space object */
int AddressSpaceObjectDestroy(void *object)
{
	SetMemory32Core(object, ADDRESS_SPACE_DEAD_FILL, sizeof(AddressSpaceObject));
	return 0;
}

/* the destructor of a fixed heap */
int UIDHeapObjectDestroy(void *object)
{
	return 0;
}

SceClass *SceSysmemForKernel_7C878E94(void)
{
	return &s_uidHeapClass->cls;
}

#if SYSMEM_NSKBL
/* the boot loader makes each heap class here, as a class of its parent */
int sceUIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClass *parent,
			SceSize itemsize, SceClassCallback create, SceClassCallback destroy,
			int (*alloc)(void *heap, SceSize size, void **object),
			int (*free)(void *heap, void *object))
{
	int ret;

	ret = sceUIDClassInitClass(&cls->cls, name, parent, itemsize, create, destroy);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(70);
		return ret;
	}
	cls->alloc = alloc;
	cls->free = free;
	return 0;
}
#else
int sceUIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClassCallback create,
			SceClassCallback destroy,
			int (*alloc)(void *heap, SceSize size, void **object),
			int (*free)(void *heap, void *object))
{
	cls->cls.name = name;
	cls->cls.create_cb = create;
	cls->cls.destroy_cb = destroy;
	cls->alloc = alloc;
	cls->free = free;
	return 0;
}
#endif

/* an object from a heap, by the heap's class */
int UIDHeapCallAlloc(void *heap, SceSize size, void **object)
{
	return ((UIDHeap *)heap)->cls->heapClass.alloc(heap, size, object);
}

int UIDHeapCallFree(void *heap, void *object)
{
	return ((UIDHeap *)heap)->cls->heapClass.free(heap, object);
}

int UIDHeapInit(UIDHeap *heap)
{
	heap->object = NULL;
	return 0;
}

/* name unknown: SceUIDHeapClass */
int UIDHeapClassInitManager(UIDManager *manager)
{
#if SYSMEM_NSKBL
	/* core 0 makes the class; its objects are a UIDHeap and its owner */
	UIDHeapClass *cls;
	int ret = 0;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		cls = sceKernelSysrootAlloc(sizeof(UIDHeapClass));
		manager->uidHeapClass = cls;
		s_uidHeapClass = cls;
		ret = sceUIDHeapClassInit(cls, "SceUIDHeapClass", SceSysmemForDriver_85336A1C(), 12,
					  UIDHeapCreate, UIDHeapDestroy, NULL, NULL);
	}
	sceKernelSysrootCorelockUnlock();
	return ret;
#else
	UIDHeapClass *cls = manager->uidHeapClass;

	s_uidHeapClass = cls;
	cls->cls.name = "SceUIDHeapClass";
	cls->cls.create_cb = UIDHeapCreate;
	cls->cls.destroy_cb = UIDHeapDestroy;
	cls->alloc = NULL;
	cls->free = NULL;
	return 0;
#endif
}
