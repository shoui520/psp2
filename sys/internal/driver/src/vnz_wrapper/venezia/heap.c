/* Sony venezia/heap.c: variable-size heap blocks. */
#include <sysmem/debug.h>
#include "../venezia_internal.h"

static inline __attribute__((always_inline)) UnitFixedCtrl *getVeneziaRegionCtrl(VeneziaHeap *heap, UnitFixedCtrl *ctrl)
{
	uintptr_t address = (uintptr_t)ctrl;
	unsigned int index;

#if FW_DEBUG
	ksceKernelAssert(heap != NULL, SCE_DBGINFO_FUNC(74, "_getUnitFixedCtrl"),
		__builtin_return_address(0));
	ksceKernelAssert(address >= (uintptr_t)heap->base &&
		address < (uintptr_t)heap->base + heap->size,
		SCE_DBGINFO_FUNC(50, "_isValidAddr"), __builtin_return_address(0));
#endif
	if (!heap->regionBase || address < heap->regionBase ||
	    address >= heap->regionBase + (heap->regionCount << 18))
		return NULL;
	index = (address - heap->regionBase) >> 18;
#if FW_DEBUG
	ksceKernelAssert(index < heap->regionCount,
		SCE_DBGINFO_FUNC(86, "_getUnitFixedCtrl"), __builtin_return_address(0));
#endif
	return &heap->regions[index];
}

/* name unknown: switch an allocated block to the registered state. */
#if FW_DEBUG
__attribute__((noinline)) int registerVeneziaUnitFixedCtrl(
#else
static inline __attribute__((always_inline)) int registerVeneziaUnitFixedCtrl(
#endif
VeneziaHeap *heap, UnitFixedCtrl *previous,
			       UnitFixedCtrl *ctrl, SceSize size)
{
	UnitFixedCtrl *region = getVeneziaRegionCtrl(heap, ctrl);
	SceSize capacity;
	uintptr_t tag;

	if (region && region->units) {
		ctrl = region;
		capacity = region->units * sizeof(UnitFixedCtrl);
	} else {
		capacity = (ctrl->units - 1) * sizeof(UnitFixedCtrl);
	}
	tag = (uintptr_t)ctrl->next;
	if (tag != (uintptr_t)heap + 1) {
		if (tag == (uintptr_t)heap + 3)
			return (int)0x8060000b;
		return (int)0x8060000a;
	}
	if (capacity < size)
		return (int)0x80600001;
	ctrl->next = (UnitFixedCtrl *)((uintptr_t)heap + 3);
	return 0;
}

/* name unknown: restore a registered block to allocated state. */
#if FW_DEBUG
__attribute__((noinline)) int unregisterVeneziaUnitFixedCtrl(
#else
static inline __attribute__((always_inline)) int unregisterVeneziaUnitFixedCtrl(
#endif
VeneziaHeap *heap, UnitFixedCtrl *previous,
			       UnitFixedCtrl *ctrl, SceSize size)
{
	UnitFixedCtrl *region = getVeneziaRegionCtrl(heap, ctrl);
	SceSize capacity;
	uintptr_t tag;

	if (region && region->units) {
		ctrl = region;
		capacity = region->units * sizeof(UnitFixedCtrl);
	} else {
		capacity = (ctrl->units - 1) * sizeof(UnitFixedCtrl);
	}
	tag = (uintptr_t)ctrl->next;
	if (tag != (uintptr_t)heap + 3) {
		if (tag == (uintptr_t)heap + 1)
			return (int)0x8060000c;
		return (int)0x8060000a;
	}
	if (capacity < size)
		return (int)0x80600001;
	ctrl->next = (UnitFixedCtrl *)((uintptr_t)heap + 1);
	return 0;
}

int _loadUnitFixedCtrl(VeneziaHeap *heap, UnitFixedCtrl *previous, UnitFixedCtrl *ctrl)
{
	UnitFixedCtrl *region = getVeneziaRegionCtrl(heap, ctrl);
	UnitFixedCtrl *next;
	uintptr_t tag;
	unsigned int units, previousUnits;

	if (region && region->units) {
		tag = (uintptr_t)region->next;
		if (tag != (uintptr_t)heap + 1)
			goto invalidTag;
		region = getVeneziaRegionCtrl(heap, ctrl);
#if FW_DEBUG
		ksceKernelAssert(region != NULL, SCE_DBGINFO_FUNC(114, "_loadUnitFixedCtrl"),
			__builtin_return_address(0));
		ksceKernelAssert(region->next != NULL && region->units != 0,
			SCE_DBGINFO_FUNC(115, "_loadUnitFixedCtrl"), __builtin_return_address(0));
#endif
		ctrl->next = region->next;
		ctrl->units = region->units;
		region->next = NULL;
		region->units = 0;
	} else {
		tag = (uintptr_t)ctrl->next;
		if (tag != (uintptr_t)heap + 1)
			goto invalidTag;
	}
	heap->allocatedUnits -= ctrl->units;
#if FW_DEBUG
	ksceKernelAssert((uintptr_t)previous->next >= (uintptr_t)heap->base &&
		(uintptr_t)previous->next < (uintptr_t)heap->base + heap->size,
		SCE_DBGINFO_FUNC(50, "_isValidAddr"), __builtin_return_address(0));
#endif
	units = ctrl->units;
	next = previous->next;
	if (ctrl - units == next) {
		if (next->units) {
			units += next->units;
			next = next->next;
			ctrl->units = units;
		}
	}
	previousUnits = previous->units;
	ctrl->next = next;
	previous->next = ctrl;
	if (previous - previousUnits == ctrl) {
		previous->next = ctrl->next;
		previous->units = units + previousUnits;
	}
	heap->lastFreeBlock = previous;
	return 0;

invalidTag:
	if (tag == (uintptr_t)heap + 3)
		return (int)0x8060000b;
	return (int)0x8060000a;
}

void _sceVeneziaHeapInit(void *base, VeneziaHeap *heap, SceSize size, SceSize regionSize)
{
	unsigned int regionCount = regionSize >> 3;
	unsigned int lastIndex, freeIndex;
	UnitFixedCtrl *last, *freeBlock;
	uintptr_t regionBase = 0;

#if FW_DEBUG
	ksceKernelAssert(base != NULL, SCE_DBGINFO_FUNC(131, "_sceVeneziaHeapInit"),
		__builtin_return_address(0));
	ksceKernelAssert(heap != NULL, SCE_DBGINFO_FUNC(132, "_sceVeneziaHeapInit"),
		__builtin_return_address(0));
	ksceKernelAssert(size > 23, SCE_DBGINFO_FUNC(133, "_sceVeneziaHeapInit"),
		__builtin_return_address(0));
	ksceKernelAssert((uintptr_t)heap == (uintptr_t)base + size + 8,
		SCE_DBGINFO_FUNC(134, "_sceVeneziaHeapInit"), __builtin_return_address(0));
#endif
	heap->size = size;
	lastIndex = (size >> 3) - 1;
	freeIndex = (size >> 3) - 2;
	last = (UnitFixedCtrl *)((uintptr_t)base + lastIndex * sizeof(UnitFixedCtrl));
	freeBlock = (UnitFixedCtrl *)((uintptr_t)base + freeIndex * sizeof(UnitFixedCtrl));
	heap->marker = (uintptr_t)heap - 1;
	heap->allocatedUnits = 0;
	heap->lastFreeBlock = last;
	if (size >= 0x40000)
		regionBase = ((uintptr_t)base + 0x3ffff) >> 18;
	heap->base = base;
	if (size >= 0x40000)
		regionBase <<= 18;
	heap->regionBase = regionBase;
	heap->regionCount = regionCount;
	last->next = freeBlock;
	last->units = 1;
	freeBlock->next = last;
	last->next->units = lastIndex;
}

static inline __attribute__((always_inline)) void assertVeneziaHeapAddress(VeneziaHeap *heap, const void *address)
{
#if FW_DEBUG
	ksceKernelAssert((uintptr_t)address >= (uintptr_t)heap->base &&
		(uintptr_t)address < (uintptr_t)heap->base + heap->size,
		SCE_DBGINFO_FUNC(50, "_isValidAddr"), __builtin_return_address(0));
#endif
}

void *_storeUnitFixedCtrl(VeneziaHeap *heap, SceSize size, SceSize alignment)
{
	UnitFixedCtrl *head, *previous, *current, *remainder, *allocated, *region;
	unsigned int units, paddingUnits, requiredUnits, availableUnits;
	SceSize roundedSize;
	int indirect;
	void *memory;

	if (!heap || heap->marker != (uintptr_t)heap - 1)
		return NULL;
	head = heap->lastFreeBlock;
	if (size <= 7) {
		units = 1;
		roundedSize = 8;
	} else {
		units = (size + 7) >> 3;
		roundedSize = units * sizeof(UnitFixedCtrl);
	}
	if (alignment < 0x40000 || (roundedSize & 0x3ffff)) {
		units++;
		indirect = 0;
	} else {
		indirect = 1;
	}
	assertVeneziaHeapAddress(heap, head);
	previous = heap->lastFreeBlock;
	current = previous->next;
	if (alignment) {
		for (;;) {
			assertVeneziaHeapAddress(heap, current);
			paddingUnits = ((uintptr_t)current + 8 - units * 8) % alignment >> 3;
			availableUnits = current->units;
			requiredUnits = paddingUnits + units;
			if (requiredUnits == availableUnits)
				break;
			if (requiredUnits < availableUnits) {
				remainder = current - requiredUnits;
				remainder->next = current->next;
				remainder->units = availableUnits - requiredUnits;
				current->next = remainder;
				break;
			}
			previous = current;
			if (current == head)
				return NULL;
			current = current->next;
		}
	} else {
		for (;;) {
			assertVeneziaHeapAddress(heap, current);
			paddingUnits = 0;
			availableUnits = current->units;
			requiredUnits = paddingUnits + units;
			if (requiredUnits == availableUnits)
				break;
			if (requiredUnits < availableUnits) {
				remainder = current - requiredUnits;
				remainder->next = current->next;
				remainder->units = availableUnits - requiredUnits;
				current->next = remainder;
				break;
			}
			previous = current;
			if (current == head)
				return NULL;
			current = current->next;
		}
	}
	if (paddingUnits) {
		current->units = paddingUnits;
		allocated = current - paddingUnits;
		previous = current;
	} else {
		allocated = current;
		previous->next = current->next;
	}
	allocated->next = (UnitFixedCtrl *)((uintptr_t)heap + 1);
	allocated->units = units;
	memory = (void *)((uintptr_t)allocated + 8 - units * 8);
	heap->lastFreeBlock = previous;
	heap->allocatedUnits += units;
	if (indirect) {
		region = getVeneziaRegionCtrl(heap, allocated);
#if FW_DEBUG
		ksceKernelAssert(region != NULL, SCE_DBGINFO_FUNC(100, "_storeUnitFixedCtrl"),
			__builtin_return_address(0));
		ksceKernelAssert(region->next == NULL && region->units == 0,
			SCE_DBGINFO_FUNC(101, "_storeUnitFixedCtrl"), __builtin_return_address(0));
#endif
		region->next = allocated->next;
		region->units = allocated->units;
	}
	return memory;
}

/* Sony _procMemory inline traversal, instantiated for each block operation. */
static inline __attribute__((always_inline)) int processVeneziaHeapMemory(
	VeneziaHeap *heap, void *memory, SceSize size, int operation)
{
	uintptr_t address = (uintptr_t)memory;
	UnitFixedCtrl *previous, *next, *ctrl, *region;
	unsigned int units;
	int ret;

	if (!heap || heap->marker != (uintptr_t)heap - 1)
		return (int)0x80600005;
	if (!address || address < (uintptr_t)heap->base ||
	    address >= (uintptr_t)heap->base + heap->size)
		return (int)0x80600000;
	previous = heap->lastFreeBlock;
	for (;;) {
		assertVeneziaHeapAddress(heap, previous);
		next = previous->next;
		if (((uintptr_t)previous <= (uintptr_t)next || address > (uintptr_t)next) &&
		    address < (uintptr_t)previous)
			break;
		previous = next;
		if (next == heap->lastFreeBlock) {
#if FW_DEBUG
			ksceKernelAssert(0, SCE_DBGINFO_FUNC(297, "_procMemory"),
				__builtin_return_address(0));
#endif
			return (int)0x80600006;
		}
	}
	ctrl = previous - previous->units;
	if ((uintptr_t)ctrl < (uintptr_t)heap->base ||
	    (uintptr_t)ctrl >= (uintptr_t)heap->base + heap->size ||
	    address >= (uintptr_t)ctrl)
		return (int)0x80600000;
	do {
		region = getVeneziaRegionCtrl(heap, ctrl);
		if (region && region->units)
			units = region->units;
		else {
			units = ctrl->units;
			if (!units)
				break;
		}
		if (address == (uintptr_t)ctrl + 8 - units * 8) {
			if (address >= (uintptr_t)ctrl)
				return (int)0x80600000;
			if ((uintptr_t)previous->next > (uintptr_t)(ctrl - units) &&
			    (uintptr_t)previous->next < (uintptr_t)ctrl) {
#if FW_DEBUG
				ksceKernelAssert(0, SCE_DBGINFO_FUNC(335, "_procMemory"),
					__builtin_return_address(0));
#endif
				return (int)0x80600006;
			}
			if ((uintptr_t)ctrl > (uintptr_t)(previous - previous->units) &&
			    (uintptr_t)previous > (uintptr_t)ctrl) {
#if FW_DEBUG
				ksceKernelAssert(0, SCE_DBGINFO_FUNC(340, "_procMemory"),
					__builtin_return_address(0));
#endif
				return (int)0x80600006;
			}
			if (operation == 0)
				ret = _loadUnitFixedCtrl(heap, previous, ctrl);
			else if (operation == 1)
				ret = registerVeneziaUnitFixedCtrl(heap, previous, ctrl, size);
			else
				ret = unregisterVeneziaUnitFixedCtrl(heap, previous, ctrl, size);
			return ret & (ret >> 31);
		}
		ctrl -= units;
	} while (address < (uintptr_t)ctrl);
	return (int)0x80600000;
}

/* name unknown: Sony _procMemory instantiated for freeing an allocation. */
int freeVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize unused)
{
	return processVeneziaHeapMemory(heap, memory, 0, 0);
}

/* name unknown: Sony _procMemory instantiated for registering an allocation. */
int registerVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size)
{
	return processVeneziaHeapMemory(heap, memory, size, 1);
}

/* name unknown: Sony _procMemory instantiated for unregistering an allocation. */
int unregisterVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size)
{
	return processVeneziaHeapMemory(heap, memory, size, 2);
}

int _sceVeneziaHeapIsRegistered(VeneziaHeap *heap, unsigned int unused0, unsigned int unused1)
{
	UnitFixedCtrl *current, *next, *region, *ctrl;
	uintptr_t registeredTag;

	if (!heap || heap->marker != (uintptr_t)heap - 1)
		return (int)0x80600005;
	current = heap->lastFreeBlock;
	registeredTag = (uintptr_t)heap + 3;
	next = current->next;
	do {
		assertVeneziaHeapAddress(heap, current);
		assertVeneziaHeapAddress(heap, next);
		if ((uintptr_t)current > (uintptr_t)next) {
			do {
				region = getVeneziaRegionCtrl(heap, current);
				ctrl = region && region->units ? region : current;
				if ((uintptr_t)ctrl->next == registeredTag)
					return (int)0x8060000b;
				current -= ctrl->units;
			} while ((uintptr_t)next < (uintptr_t)current);
#if FW_DEBUG
			ksceKernelAssert(next == current,
				SCE_DBGINFO_FUNC(514, "_sceVeneziaHeapIsRegistered"),
				__builtin_return_address(0));
#endif
			next = next->next;
		} else {
			while ((uintptr_t)heap->base <= (uintptr_t)current) {
				region = getVeneziaRegionCtrl(heap, current);
				ctrl = region && region->units ? region : current;
				if ((uintptr_t)ctrl->next == registeredTag)
					return (int)0x8060000b;
				current -= ctrl->units;
			}
			current = next;
			next = next->next;
		}
	} while (current != heap->lastFreeBlock);
	return 0;
}
