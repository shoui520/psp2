/* Sony venezia/heapmgr.c: communication-memory heap management. */
#include <sce/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include "venezia_internal.h"

#if FW_DEBUG
extern void _sceVeneziaHeapInit(void *base, VeneziaHeap *heap, SceSize size, SceSize regionSize);
#else
extern void _sceVeneziaHeapInit(void *base, VeneziaHeap *heap, SceSize size, SceSize regionSize);
#endif

/* name unknown: initialize a heap manager with its first in-buffer heap. */
int initVeneziaHeapManager(VeneziaHeapManager *manager, void *base, SceSize size)
{
	SceSize regionSize = 8 * (size >> 18);
	VeneziaHeapEntry *entry;

	if (!manager)
		return (int)0x80600000;
	if (size < regionSize + 60 || (size & 3))
		return (int)0x80600001;
	if ((uintptr_t)base & 0x17)
		return (int)0x80600002;
	if (base) {
		entry = (VeneziaHeapEntry *)((uintptr_t)base - sizeof(VeneziaHeapEntry) +
			size - regionSize);
		manager->lock = 0;
		manager->base = base;
		manager->magic = 911383599;
		manager->size = size;
		entry->next = entry;
		entry->prev = entry;
		_sceVeneziaHeapInit(base, &entry->heap,
			size - sizeof(VeneziaHeapEntry) - regionSize, regionSize);
		memset(entry->heap.regions, 0, regionSize);
		manager->heaps = entry;
	} else {
		manager->magic = 0;
		manager->lock = 0;
		manager->base = NULL;
		manager->size = 0;
		manager->heaps = NULL;
	}
	return 0;
}

#if FW_DEBUG
extern void *_storeUnitFixedCtrl(VeneziaHeap *heap, SceSize size, SceSize alignment);
#else
extern void *_storeUnitFixedCtrl(VeneziaHeap *heap, SceSize size, SceSize alignment);
#endif

void *_sceVeneziaHeapManagerAllocMemory(VeneziaHeapManager *manager,
				       SceSize size, SceSize alignment)
{
	VeneziaHeapEntry *head, *entry;
	void *memory;
	int intr;

	if (!manager)
		return NULL;
#if FW_DEBUG
	ksceKernelAssert(manager->magic == 911383599,
		SCE_DBGINFO_FUNC(74, "_sceVeneziaHeapManagerAllocMemory"),
		__builtin_return_address(0));
#endif
	if (!size)
		return NULL;
	if (alignment && (alignment > 0x100000 || ((alignment - 1) & alignment))) {
#if FW_DEBUG
		ksceKernelAssert(0,
			SCE_DBGINFO_FUNC(83, "_sceVeneziaHeapManagerAllocMemory"),
			__builtin_return_address(0));
#endif
		return NULL;
	}
	head = manager->heaps;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->lock);
	entry = head->prev;
	for (;;) {
#if FW_DEBUG
		ksceKernelAssert((uintptr_t)manager->base <= (uintptr_t)entry &&
			(uintptr_t)entry < (uintptr_t)manager->base + manager->size,
			SCE_DBGINFO_FUNC(96, "_sceVeneziaHeapManagerAllocMemory"),
			__builtin_return_address(0));
#endif
		memory = _storeUnitFixedCtrl(&entry->heap, size, alignment);
		if (memory)
			break;
		if (entry == head)
			break;
		entry = entry->prev;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->lock, intr);
	return memory;
}

#if FW_DEBUG
extern int freeVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#else
extern int freeVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#endif

int _procMemory(VeneziaHeapManager *manager, void *memory)
{
	VeneziaHeapEntry *head, *entry;
	int intr, ret;

	if (!manager)
		return (int)0x80600000;
#if FW_DEBUG
	ksceKernelAssert(manager->magic == 911383599,
		SCE_DBGINFO_FUNC(150, "_procMemory"), __builtin_return_address(0));
#endif
	head = manager->heaps;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->lock);
	entry = head->prev;
	for (;;) {
#if FW_DEBUG
		ksceKernelAssert((uintptr_t)manager->base <= (uintptr_t)entry &&
			(uintptr_t)entry < (uintptr_t)manager->base + manager->size,
			SCE_DBGINFO_FUNC(160, "_procMemory"), __builtin_return_address(0));
#endif
		ret = freeVeneziaHeapMemory(&entry->heap, memory, 0);
		if (!ret)
			break;
		if (head == entry)
			break;
		entry = entry->prev;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->lock, intr);
	return ret;
}

#if FW_DEBUG
extern int registerVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#else
extern int registerVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#endif

/* name unknown: instantiate Sony _procMemory for memory registration. */
int registerVeneziaHeapManagerMemory(VeneziaHeapManager *manager, void *memory, SceSize size)
{
	VeneziaHeapEntry *head, *entry;
	int intr, ret;

	if (!manager)
		return (int)0x80600000;
#if FW_DEBUG
	ksceKernelAssert(manager->magic == 911383599,
		SCE_DBGINFO_FUNC(150, "_procMemory"), __builtin_return_address(0));
#endif
	head = manager->heaps;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->lock);
	entry = head->prev;
	for (;;) {
#if FW_DEBUG
		ksceKernelAssert((uintptr_t)manager->base <= (uintptr_t)entry &&
			(uintptr_t)entry < (uintptr_t)manager->base + manager->size,
			SCE_DBGINFO_FUNC(160, "_procMemory"), __builtin_return_address(0));
#endif
		ret = registerVeneziaHeapMemory(&entry->heap, memory, size);
		if (!ret)
			break;
		if (head == entry)
			break;
		entry = entry->prev;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->lock, intr);
	return ret;
}

#if FW_DEBUG
extern int unregisterVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#else
extern int unregisterVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
#endif

/* name unknown: instantiate Sony _procMemory for memory unregistration. */
int unregisterVeneziaHeapManagerMemory(VeneziaHeapManager *manager, void *memory, SceSize size)
{
	VeneziaHeapEntry *head, *entry;
	int intr, ret;

	if (!manager)
		return (int)0x80600000;
#if FW_DEBUG
	ksceKernelAssert(manager->magic == 911383599,
		SCE_DBGINFO_FUNC(150, "_procMemory"), __builtin_return_address(0));
#endif
	head = manager->heaps;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->lock);
	entry = head->prev;
	for (;;) {
#if FW_DEBUG
		ksceKernelAssert((uintptr_t)manager->base <= (uintptr_t)entry &&
			(uintptr_t)entry < (uintptr_t)manager->base + manager->size,
			SCE_DBGINFO_FUNC(160, "_procMemory"), __builtin_return_address(0));
#endif
		ret = unregisterVeneziaHeapMemory(&entry->heap, memory, size);
		if (!ret)
			break;
		if (head == entry)
			break;
		entry = entry->prev;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->lock, intr);
	return ret;
}

#if FW_DEBUG
extern int _sceVeneziaHeapIsRegistered(VeneziaHeap *heap, unsigned int unused0, unsigned int unused1);
#else
extern int _sceVeneziaHeapIsRegistered(VeneziaHeap *heap, unsigned int unused0, unsigned int unused1);
#endif

/* name unknown: instantiate Sony _procMemory for registration-state checks. */
int isVeneziaHeapManagerRegistered(VeneziaHeapManager *manager)
{
	VeneziaHeapEntry *head, *entry;
	int intr, ret;

	if (!manager)
		return (int)0x80600000;
#if FW_DEBUG
	ksceKernelAssert(manager->magic == 911383599,
		SCE_DBGINFO_FUNC(150, "_procMemory"), __builtin_return_address(0));
#endif
	head = manager->heaps;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->lock);
	entry = head->prev;
	for (;;) {
#if FW_DEBUG
		ksceKernelAssert((uintptr_t)manager->base <= (uintptr_t)entry &&
			(uintptr_t)entry < (uintptr_t)manager->base + manager->size,
			SCE_DBGINFO_FUNC(160, "_procMemory"), __builtin_return_address(0));
#endif
		ret = _sceVeneziaHeapIsRegistered(&entry->heap, 0, 0);
		if (!ret)
			break;
		if (head == entry)
			break;
		entry = entry->prev;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->lock, intr);
	return ret;
}
