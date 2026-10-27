/*
 * SceSysmem: object heaps. An object heap hands out kernel objects of up to
 * 0x200 bytes from a fixed heap per size class, and counts the objects of
 * each type against the owner's resource limits.
 */

#include "sysmem_internal.h"
#include <sysmem/uid_guid.h>

/* the object heap of the kernel, set from the boot parameters */
static ObjectHeap *s_kernelObjectHeap;
/* the object heap shared by everything without one of its own */
static ObjectHeap *s_sharedObjectHeap;
/* the object size of each type, for the preallocation of its reserve */
static SceSize s_objectTypeSize[OBJECT_HEAP_TYPES];
/* SceKernelHeapForShared */
static void *s_sharedHeap;

/*
 * The walks below stop at the first size over 0x200, as Sony's do: past the last
 * class that is the first word of the object after the table.
 */
static const ObjectHeapSizeClassDef s_sizeClassDefs[] = {
	{ 0x10, 0 }, { 0x20, 1 }, { 0x40, 1 }, { 0x80, 0 }, { 0x100, 0 }, { 0x200, 0 },
};

#define OBJECT_HEAP_SIZE_CLASSES	6

/* name unknown */
static __attribute__((noinline, noclone)) ObjectHeap *ObjectHeapInit(ObjectHeap *heap, void *partition, SceUInt32 count,
			   ObjectHeapSizeClass *sizeClasses, SceKernelHeapHook *hook)
{
	unsigned int i;
	int type;
	ObjectHeapSizeClass *entry = sizeClasses;

	heap->partition = partition;
	heap->sizeClassCount = count;
	heap->sizeClasses = sizeClasses;
	heap->lock = 0;
	i = 0;
	while (i != count) {
		i++;
		entry->size = 0;
		entry->heap = NULL;
		entry->uid = -1;
		entry++;
	}
	heap->hook = hook;
	heap->heap = NULL;
	for (type = 0; type < OBJECT_HEAP_TYPES; type++) {
		heap->limits[type].max = 0xffffffff;
		heap->limits[type].reserve = 0;
		heap->limits[type].count = 0;
		heap->limits[type].peak = 0;
		heap->limits[type].freeCount = 0;
		heap->limits[type].freeHead = NULL;
		heap->limits[type].freeTail = NULL;
	}
	return heap;
}

/* name unknown: fill the first free (or same-size) size class */
static int ObjectHeapSetSizeClass(ObjectHeap *heap, SceSize size, void *fixedHeap, SceUID uid)
{
	ObjectHeapSizeClass *entry = heap->sizeClasses;
	unsigned int i;

	for (i = 0; i < heap->sizeClassCount; i++, entry++) {
		if ((entry->size == 0 || entry->size == size) && entry->heap == NULL) {
			entry->size = size;
			entry->heap = fixedHeap;
			entry->uid = uid;
			if (fixedHeap != NULL)
				((FixedHeap *)fixedHeap)->owner = heap;
			return 0;
		}
	}
	return SCE_KERNEL_ERROR_ERROR;
}

#if !SYSMEM_NSKBL
/* name unknown: a size class with a fixed heap from the kernel heap */
static int ObjectHeapAddKernelSizeClass(ObjectHeap *heap, SceSize size)
{
	FixedHeap *fixedHeap;
	SceUID uid;
	int ret;

	fixedHeap = SceSysmemForKernel_85571907(sizeof(*fixedHeap));
	if (fixedHeap == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	fixedHeap->hook = NULL;
	fixedHeap->heap = s_sharedHeap;
	ret = FixedHeapInitForProcess(fixedHeap, SCE_KERNEL_PROCESS_ID, 0, 0, size,
				      SceSysmemForKernel_0164D817(), 0x1000, 0x10);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(fixedHeap);
		return ret;
	}
	uid = SceSysmemForKernel_6DB44A42(&FixedHeapGetClass()->heapClass.cls, fixedHeap->name, fixedHeap);
	ObjectHeapSetSizeClass(heap, size, fixedHeap, uid);
	return 0;
}
#endif

void *ObjectHeapGetHeap(ObjectHeap *heap, SceSize size)
{
	ObjectHeapSizeClass *entry;
	unsigned int i;

	if (heap == NULL)
		return NULL;
	entry = heap->sizeClasses;
	for (i = 0; i < heap->sizeClassCount; i++, entry++) {
		if (size <= entry->size && entry->heap != NULL)
			return entry->heap;
	}
	SCE_DBG_PRINTF(0xb, 172, "Cannot found for 0x%08x\n", size);
	return NULL;
}

#if !SYSMEM_NSKBL
int ObjectHeapSetResourceLimit(ObjectHeap *heap, unsigned int type, SceUInt32 max,
			       SceUInt32 reserve)
{
	ObjectHeapLimit *limit = &heap->limits[type];
	SceSize size = s_objectTypeSize[type];
	SceUInt32 count;
	void *fixedHeap, *object;
	int ret;

	limit->max = max;
	limit->reserve = reserve;
	if (size == 0)
		return 0;
	count = limit->freeCount;
	fixedHeap = ObjectHeapGetHeap(heap, size);
	for (; count < limit->reserve; count++) {
		ret = UIDHeapCallAlloc(fixedHeap, size, &object);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 629, "UIDHeapCallAlloc failed 0x%08x\n", ret);
			return SCE_KERNEL_ERROR_ERROR;
		}
		void *reserved = object;
		void *tail = limit->freeTail;

		*(void **)reserved = NULL;
		if (tail == NULL) {
			limit->freeHead = reserved;
			limit->freeTail = reserved;
		} else {
			*(void **)tail = reserved;
			limit->freeTail = reserved;
		}
		limit->freeCount++;
	}
	return 0;
}

/* name unknown */
void *ObjectHeapGetDefaultHeap(ObjectHeap *heap)
{
	if (heap == NULL)
		return NULL;
	return heap->defaultHeap;
}
#endif

/*
 * Count one more object of the type against its limit; hand out a
 * preallocated one if there is one (else *object is NULL). 0: over the limit.
 */
int ObjectHeapAllocLimit(ObjectHeap *heap, unsigned int type, void **object)
{
	ObjectHeapLimit *limit;
	SceKernelIntrStatus status;
	void *reserved;

	if (type >= OBJECT_HEAP_TYPES)
		return 0;
	limit = &heap->limits[type];
	status = sceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	if (limit->count >= limit->max) {
		SCE_PRINTF_WITH_INFO(0xb, 259, "Object Limit Overflow:type=%d, %d >= %d nMaxLimit\n",
				     type, limit->count, limit->max);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, status);
		return 0;
	}
	limit->count++;
	if (limit->count > limit->peak)
		limit->peak = limit->count;
	reserved = NULL;
	if (limit->freeCount != 0) {
		reserved = limit->freeHead;
		limit->freeCount--;
		if (reserved != NULL) {
			if (reserved == limit->freeTail) {
				limit->freeHead = NULL;
				limit->freeTail = NULL;
			} else {
				limit->freeHead = *(void **)reserved;
			}
			*(void **)reserved = NULL;
		}
	}
	*object = reserved;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, status);
	return 1;
}

/*
 * name unknown: one object of the type less; keep it as a reserve object if
 * the reserve is short (1), else the caller frees it (0)
 */
int ObjectHeapFreeLimit(ObjectHeap *heap, unsigned int type, void *object)
{
	ObjectHeapLimit *limit;
	SceKernelIntrStatus status;
	int kept;

	if (type >= OBJECT_HEAP_TYPES)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	limit = &heap->limits[type];
	status = sceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	if (limit->count == 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, status);
		return 0;
	}
	if (limit->freeCount < limit->reserve) {
		void *tail = limit->freeTail;

		*(void **)object = NULL;
		if (tail == NULL) {
			limit->freeHead = object;
			limit->freeTail = object;
		} else {
			*(void **)tail = object;
			limit->freeTail = object;
		}
		limit->freeCount++;
		kept = 1;
	} else {
		kept = 0;
	}
	limit->count--;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, status);
	return kept;
}

#if !SYSMEM_NSKBL
/* sceKernelGetObjectHeap (wiki name) */
ObjectHeap *SceSysmemForKernel_857408DA(void)
{
	return s_kernelObjectHeap;
}

int sceKernelCreateObjectHeap(SceUID pid, int kernel, SceKernelHeapHook *hook, ObjectHeap **out)
{
	const ObjectHeapSizeClassDef *def;
	ObjectHeapSizeClass *sizeClasses;
	ObjectHeap *heap;
	FixedHeap *fixedHeap;
	void *addressSpaceCB, *partition;
	SceUID uid;
	int ret;

	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SysrootPIDtoAddressSpaceCB(pid, &addressSpaceCB);
	if (ret < 0)
		return ret;
	ret = AddressSpaceGetDefaultPhyMemPart(addressSpaceCB, kernel, &partition);
	if (ret < 0)
		return ret;
	heap = hook->funcAlloc(hook->arg, sizeof(*heap));
	if (heap == NULL) {
		SCE_DBG_PRINTF(0xb, 392, "pHeapHook->funcAlloc(0x%08x) failed\n", sizeof(*heap));
		return SCE_KERNEL_ERROR_ERROR;
	}
	sizeClasses = hook->funcAlloc(hook->arg, OBJECT_HEAP_SIZE_CLASSES * sizeof(*sizeClasses));
	if (sizeClasses == NULL) {
		SCE_DBG_PRINTF(0xb, 400, "sceKernelHeapHookAlloc(0x%08x) failed\n",
			       OBJECT_HEAP_SIZE_CLASSES * sizeof(*sizeClasses));
		return SCE_KERNEL_ERROR_ERROR;
	}
	ObjectHeapInit(heap, partition, OBJECT_HEAP_SIZE_CLASSES, sizeClasses, hook);
	for (def = s_sizeClassDefs; def->size <= 0x200; def++) {
		if (!def->ownHeap) {
			fixedHeap = NULL;
			uid = -1;
		} else {
			fixedHeap = hook->funcAlloc(hook->arg, sizeof(*fixedHeap));
			if (fixedHeap == NULL)
				return SCE_KERNEL_ERROR_ERROR;
			fixedHeap->hook = hook;
			ret = FixedHeapInitForProcess(fixedHeap, pid, 0, 0, def->size,
						      partition, 0x1000, 0x10);
			if (ret < 0)
				SCE_DBG_PRINTF(0xb, 432, "FixedHeapCreateForProcess failed 0x%08x\n",
					       ret);
			uid = SceSysmemForKernel_6DB44A42(&FixedHeapGetClass()->heapClass.cls, fixedHeap->name,
							  fixedHeap);
		}
		ObjectHeapSetSizeClass(heap, def->size, fixedHeap, uid);
	}
	heap->defaultHeap = ObjectHeapGetHeap(heap, 0x40);
	*out = heap;
	return 0;
}

/* sceKernelCreateObjectHeapWithHeap (wiki name): the shared size classes from a process heap */
int sceKernelObjectHeapStart(ObjectHeap *heap, SceUID pid, SceUID heapUid)
{
	const ObjectHeapSizeClassDef *def;
	FixedHeap *fixedHeap;
	void *processHeap;
	SceUID uid;
	int ret;

	ret = sceGUIDGetObject(heapUid, &processHeap);
	if (ret < 0)
		return ret;
	if (processHeap == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap->heap = processHeap;
	for (def = s_sizeClassDefs; def->size <= 0x200; def++) {
		if (!def->ownHeap) {
			fixedHeap = HeapAllocLocked(processHeap, sizeof(*fixedHeap));
			if (fixedHeap == NULL)
				return SCE_KERNEL_ERROR_ERROR;
			fixedHeap->hook = NULL;
			fixedHeap->heap = processHeap;
			FixedHeapInitForProcess(fixedHeap, pid, 0, 0, def->size,
						heap->partition, 0x1000, 0x10);
			uid = SceSysmemForKernel_6DB44A42(&FixedHeapGetClass()->heapClass.cls, fixedHeap->name,
							  fixedHeap);
			ObjectHeapSetSizeClass(heap, def->size, fixedHeap, uid);
		} else {
			fixedHeap = ObjectHeapGetHeap(heap, def->size);
			fixedHeap->heap = processHeap;
		}
	}
	ObjectHeapSetSizeClass(heap, (SceSize)-1, processHeap, -1);
	return 0;
}

/* sceKernelDeleteObjectHeap (wiki name) */
int sceKernelDeleteObjectHeap(ObjectHeap *heap)
{
	const ObjectHeapSizeClassDef *def;
	ObjectHeapSizeClass *entry = heap->sizeClasses;
	unsigned int i;

	for (i = 0; i < heap->sizeClassCount; i++, entry++) {
		if (entry->heap == NULL || entry->size == (SceSize)-1)
			continue;
		FixedHeapRelease(entry->heap);
		for (def = s_sizeClassDefs; def->size <= 0x200; def++)
			if (def->size == entry->size)
				break;
		if (def->size <= 0x200 && def->ownHeap)
			heap->hook->funcFree(heap->hook->arg, entry->heap);
		else
			HeapFreeLocked(heap->heap, entry->heap);
		entry->heap = NULL;
		if (entry->uid != -1) {
			GUIDUnregisterChecked(entry->uid);
			entry->uid = -1;
		}
	}
	ObjectHeapSetSizeClass(heap, (SceSize)-1, NULL, -1);
	heap->hook->funcFree(heap->hook->arg, heap->sizeClasses);
	heap->hook->funcFree(heap->hook->arg, heap);
	return 0;
}

/* sceKernelObjectHeapSetSize (wiki name): the object size of a type */
int SceSysmemForKernel_B591E49F(unsigned int type, SceSize size)
{
	if (type >= OBJECT_HEAP_TYPES)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_objectTypeSize[type] = size;
	return 0;
}

int sceKernelObjectHeapSetResourceLimit(SceUID pid, int kind, unsigned int type,
					SceUInt32 max, SceUInt32 reserve)
{
	ObjectHeap *heap;
	int ret;

	heap = SysrootGetProcessObjectHeap(pid, kind);
	if (heap == NULL) {
		/* Sony asks again (kind 0) and then takes the kernel's */
		SysrootGetProcessObjectHeap(pid, 0);
		heap = s_kernelObjectHeap;
	}
	ret = ObjectHeapSetResourceLimit(heap, type, max, reserve);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 663, "ObjectHeapSetResourceLimit failed 0x%08x\n", ret);
	return ret;
}

int sceKernelObjectHeapSetResourceLimitAll(ObjectHeap *heap, unsigned int count,
					   const ObjectHeapResourceLimit *limits)
{
	unsigned int type;
	int ret;

	type = 0;
	while (type != count) {
		ret = ObjectHeapSetResourceLimit(heap, type, limits->max, limits->reserve);
		type++;
		limits++;
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 682, "ObjectHeapSetResourceLimit failed 0x%08x\n", ret);
			return ret;
		}
	}
	return 0;
}

void *sceKernelObjectHeapAlloc(ObjectHeap *heap, SceSize size)
{
	void *fixedHeap, *object;
	int ret;

	fixedHeap = ObjectHeapGetHeap(heap, size);
	if (fixedHeap == NULL)
		SCE_DBG_PRINTF(0xb, 699, "ObjectHeapGetHeap returns NULL\n");
	ret = UIDHeapCallAlloc(fixedHeap, size, &object);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 702, "UIDHeapCallAlloc failed 0x%08x\n", ret);
		return NULL;
	}
	return object;
}

/* sceKernelObjectHeapFree (wiki name) */
int SceSysmemForKernel_17D3D4BD(ObjectHeap *heap, SceSize size, void *object)
{
	return UIDHeapCallFree(ObjectHeapGetHeap(heap, size), object);
}

/* name unknown: sceKernelGetObjectHeapInfo */
int SceSysmemForKernel_DBEACE2C(ObjectHeap *heap, ObjectHeapInfo *info)
{
	unsigned int i;
	SceUInt32 count;
	ObjectHeapSizeClass *sizeClasses;

	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	count = heap->sizeClassCount;
	sizeClasses = heap->sizeClasses;
	info->count = count;
	for (i = 0; i != count; i++) {
		info->sizeClasses[i].size = sizeClasses[i].size;
		info->sizeClasses[i].heap = sizeClasses[i].heap;
	}
	return 0;
}

/* sceKernelGetSharedObjectHeap (wiki name) */
ObjectHeap *SceSysmemForKernel_791D6205(void)
{
	return s_sharedObjectHeap;
}

void ObjectHeapSetKernelHeap(const struct UIDManager *manager)
{
	s_kernelObjectHeap = manager->kernelObjectHeap;
}

/* name unknown: create SceKernelHeapForShared and the shared object heap */
int ObjectHeapInitShared(void)
{
	ObjectHeap *heap;
	ObjectHeapSizeClass *sizeClasses;
	SceUID heapUid;
	int ret;

	heapUid = sceKernelCreateHeap("SceKernelHeapForShared", 0x10000, NULL);
	ret = sceGUIDGetObject(heapUid, &s_sharedHeap);
	if (ret < 0)
		return ret;
	s_sharedObjectHeap = HeapAllocLocked(s_sharedHeap, sizeof(ObjectHeap));
	sizeClasses = HeapAllocLocked(s_sharedHeap, OBJECT_HEAP_SIZE_CLASSES * sizeof(*sizeClasses));
	heap = s_sharedObjectHeap;
	ObjectHeapInit(heap, SceSysmemForKernel_0164D817(), OBJECT_HEAP_SIZE_CLASSES,
		       sizeClasses, NULL);
	if ((ret = ObjectHeapAddKernelSizeClass(heap, 0x10)) < 0 ||
	    (ret = ObjectHeapAddKernelSizeClass(s_sharedObjectHeap, 0x20)) < 0 ||
	    (ret = ObjectHeapAddKernelSizeClass(s_sharedObjectHeap, 0x40)) < 0 ||
	    (ret = ObjectHeapAddKernelSizeClass(s_sharedObjectHeap, 0x80)) < 0 ||
	    (ret = ObjectHeapAddKernelSizeClass(s_sharedObjectHeap, 0x100)) < 0 ||
	    (ret = ObjectHeapAddKernelSizeClass(s_sharedObjectHeap, 0x200)) < 0)
		return ret;
	ObjectHeapSetSizeClass(s_sharedObjectHeap, (SceSize)-1, s_sharedHeap, -1);
	return 0;
}
#endif
