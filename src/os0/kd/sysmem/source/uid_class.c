/*
 * SceSysmem: UID classes. Every class is on one ring, in the order it was
 * registered, and gets a one-byte index there (SceClass.attributes). The
 * root class is SceUIDClass; SceUIDHeapClass is the base of the heaps that
 * kernel objects are allocated from.
 */

#include <psp2kern/kernel/sysclib.h>
#include "sysmem_internal.h"

#define SCE_CLASS_MAGIC			0xabce9da5

/* the address space object's magic, and the fill it is destroyed with */
#define ADDRESS_SPACE_MAGIC		0x4d95aeec
#define ADDRESS_SPACE_DEAD_FILL		0x07fd9f97

extern void *SetMemoryCore(void *destination, int value, SceSize size);

/* SceUIDHeapClass and SceUIDClass */
static UIDHeapClass *s_uidHeapClass;
static SceClass *s_uidClass;

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
	SetMemoryCore(object, ADDRESS_SPACE_DEAD_FILL, sizeof(AddressSpaceObject));
	return 0;
}

/* the destructor of a fixed heap */
int UIDHeapObjectDestroy(void *object)
{
	return 0;
}

SceClass *ksceKernelGetUIDHeapClass(void)
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
	UIDHeapClass *cls = manager->uidHeapClass;

	s_uidHeapClass = cls;
	cls->cls.name = "SceUIDHeapClass";
	cls->cls.create_cb = UIDHeapCreate;
	cls->cls.destroy_cb = UIDHeapDestroy;
	cls->alloc = NULL;
	cls->free = NULL;
	return 0;
}

/* name unknown: SceUIDClass's constructor and destructor */
static int UIDClassCreate(void *object)
{
	((SceObjectBase *)object)->object = NULL;
	return 0;
}

static int UIDClassDestroy(void *object)
{
	return 0;
}

/* a class onto the ring, with the next index */
int RegisterClass(SceClass *cls)
{
	UIDManager *manager = g_uidManager;
	SceClass *head = manager->classes, *c;
	unsigned int index = 0, cindex;
	unsigned int attributes;

	if (head == NULL) {
		cls->next = cls;
		cls->prev = cls;
		cls->attributes = (cls->attributes & ~0xff) | 1;
		manager->classes = cls;
		return 0;
	}
	c = head;
	do {
		const char *name;

		if ((SceUInt8)c->attributes == 0)
			SCE_DBG_PRINTF(0xb, 94, "cinex=0x%08x, %p,0x%08x[%s]\n", index, c, 0, c->name);
		if (c == cls) {
			SCE_DBG_PRINTF(0xb, 105, "Duplicated %p and %p[new]\n", c, c);
			return 0;
		}
		name = cls->name;
		if (strncmp(c->name, name, 0x1f) == 0) {
			SCE_DBG_PRINTF(0xb, 109, "Duplicate name %p[%s]\n", cls, name);
			return 0;
		}
		cindex = (SceUInt8)c->attributes;
		c = c->next;
		if (index <= cindex)
			index = cindex;
	} while (head != c);
	/* before the head: last on the ring */
	cls->next = c;
	cls->prev = c->prev;
	if (c->prev != NULL)
		c->prev->next = cls;
	c->prev = cls;
	if (index != 0xff)
		index = (index + 1) & 0xff;
	attributes = (cls->attributes & ~0xff) | index;
	cls->attributes = attributes;
	SCE_DBG_ASSERT_CHECK((SceUInt8)attributes == index, 125);
	return 0;
}

int ksceKernelFindClassByName(const char *name, SceClass **cls)
{
	SceClass *head, *c;
	int ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (cls == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	head = g_uidManager->classes;
	for (c = head; c != NULL; ) {
		ret = strncmp(c->name, name, 0x1f);
		if (ret == 0) {
			*cls = c;
			return ret;
		}
		c = c->next;
		if (head == c)
			break;
	}
	return SCE_KERNEL_ERROR_UID_CLASS_NOT_FOUND;
}

/* sceKernelFindClassByIndex */
int SceSysmemForKernel_1DE4953F(SceUInt32 index, SceClass **cls)
{
	SceClass *head, *c;

	if (cls == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	head = g_uidManager->classes;
	if (head != NULL) {
		c = head;
		if (index == (SceUInt8)head->attributes)
			goto found;
		for (c = head->next; head != c && c != NULL; c = c->next) {
			if (index == (SceUInt8)c->attributes)
				goto found;
		}
	}
	return SCE_KERNEL_ERROR_UID_CLASS_NOT_FOUND;
found:
	*cls = c;
	return 0;
}

int ksceUIDClassInitClass(SceClass *cls, const char *name, void *uidclass, SceSize itemsize,
			  SceClassCallback create, SceClassCallback destroy)
{
	SceUInt16 aligned;

	if (itemsize <= 4)
		aligned = 4;
	else if (itemsize <= 8)
		aligned = 8;
	else if (itemsize <= 0x10)
		aligned = 0x10;
	else
		aligned = (itemsize + 0x1f) & ~0x1f;
	cls->itemsize = itemsize;
	cls->name = name;
	cls->root = s_uidClass;
	cls->uidclass = uidclass;
	cls->itemsize_aligned = aligned;
	cls->create_cb = create;
	cls->destroy_cb = destroy;
	cls->magic = SCE_CLASS_MAGIC;
	cls->attributes = 0;
	cls->defaultObjectType = 0;
	RegisterClass(cls);
	return 0;
}

SceClass *ksceKernelGetUIDClass(void)
{
	return s_uidClass;
}

/* sceKernelGetClassInfoAll: one info per class, from SceUIDClass on */
int SceSysmemForKernel_100B4029(SceKernelClassInfo *info, SceUInt32 max, SceUInt32 *count)
{
	SceClass *root, *c;
	SceUInt32 n = 0, i = 0;

	if (max != 0) {
		if (info == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (count == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	root = g_uidManager->uidClass;
	c = root;
	do {
		if (i++ < max) {
			const char *name;

			n++;
			info->parent = c->uidclass;
			name = c->name;
			info->size = sizeof(*info);
			info->cls = c;
			info->name = name;
			info++;
		}
		c = c->next;
	} while (root != c);
	if (count != NULL)
		*count = n;
	return i;
}

/* name unknown: SceUIDClass */
int UIDClassInit(UIDManager *manager)
{
	SceClass *cls = manager->uidClass;

	s_uidClass = cls;
	cls->name = "SceUIDClass";
	cls->create_cb = UIDClassCreate;
	cls->destroy_cb = UIDClassDestroy;
	return 0;
}
