/*
 * SceSysmem: SceUIDHeapClass, the base of the heaps kernel objects are
 * allocated from, and the constructors of the objects made from them. Sony's
 * uid_heap.c (NSKBL's records name it); in os0 its functions are the range
 * before uid_class.c's.
 */

#include <sysmem/sysclib.h>
#include "sysmem_internal.h"

/* SceUIDHeapClass */
static UIDHeapClass *s_uidHeapClass;

#if !SYSMEM_NSKBL
/* name unknown: SceUIDHeapClass's constructor and destructor */
static int UIDHeapCreate(void *object)
{
	((FixedHeap *)object)->owner = NULL;
	return 0;
}
#endif

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

#if !SYSMEM_NSKBL
static int UIDHeapDestroy(void *object)
{
	((FixedHeap *)object)->owner = NULL;
	return 0;
}
#endif

/* name unknown: the destructor of an address space object */
int AddressSpaceObjectDestroy(void *object)
{
	SetMemory32Core(object, ADDRESS_SPACE_DEAD_FILL, sizeof(AddressSpaceObject));
	return 0;
}

#if !SYSMEM_NSKBL
/* the destructor of a fixed heap */
int UIDHeapObjectDestroy(void *object)
{
	return 0;
}

SceClass *SceSysmemForKernel_7C878E94(void)
{
	return &s_uidHeapClass->cls;
}

int UIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClassCallback create,
		     SceClassCallback destroy, int (*alloc)(void *heap, SceSize size, void **object),
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

#if !SYSMEM_NSKBL
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
	UIDHeapClass *cls = manager->uidHeapClass;

	s_uidHeapClass = cls;
	cls->cls.name = "SceUIDHeapClass";
	cls->cls.create_cb = UIDHeapCreate;
	cls->cls.destroy_cb = UIDHeapDestroy;
	cls->alloc = NULL;
	cls->free = NULL;
	return 0;
}
#endif
