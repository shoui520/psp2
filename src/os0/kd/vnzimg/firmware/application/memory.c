/*
 * The memory access protocol (Toshiba's, US20120324167A1): a range of
 * memory moves between the states unmanaged, public (shared through the L2),
 * protected (read only, cached) and private (this core's cached copy). Each
 * change goes to the implementation for where the range lies: the kernel's
 * uncached window, its public window (kernel/api.c), or unmanaged memory
 * (drivers/unmanaged.c). The names are the community's (henkakuwiki,
 * VVNZRunner).
 *
 * Every function works on whole lines (0x100 bytes, 0x40 for private and
 * protected) and returns the range's address in the new state, or 0.
 */
#include "application.h"

#define LINE		0x40
#define L2_LINE		0x100

/* the core that runs without a data cache */
#define VNZ_UNCACHED_CORE_ID	55

typedef struct MemoryRange {
	uint32_t base;
	uint32_t offset;	/* of the address in the first line */
	uint32_t size;		/* whole lines */
	uint32_t last;
} MemoryRange;

static void memoryRange(MemoryRange *range, uint32_t address, uint32_t size, uint32_t line)
{
	range->base = address & ~(line - 1);
	range->offset = address - range->base;
	range->size = (size + range->offset + line - 1) & ~(line - 1);
	range->last = range->base + range->size - 1;
}

static uint32_t inUncachedWindow(const MemoryRange *range)
{
	return vnzToUncached(range->base) && vnzToUncached(range->last);
}

static uint32_t inPublicWindow(const MemoryRange *range)
{
	return vnzToPublic(range->base) && vnzToPublic(range->last);
}

static uint32_t rangeAddress(const MemoryRange *range, uint32_t address)
{
	return address ? address + range->offset : address;
}

/* Unmanaged to private: the range in this core's cache, nothing stale in it. */
uint32_t enterMemoryAccessProtocol(uint32_t address, uint32_t size)
{
	MemoryRange range;
	uint32_t physical;

	memoryRange(&range, address, size, L2_LINE);
	if (inUncachedWindow(&range))
		address = vnzMemoryPhysicalToUncached(vnzMemoryEnter(range.base, range.size),
			range.size);
	else if (inPublicWindow(&range))
		address = vnzMemoryPhysicalToPublic(vnzMemoryEnter(range.base, range.size),
			range.size);
	else
		address = vkUnmanagedPhysicalToUncached(vkUnmanagedEnter(range.base, range.size),
			range.size);
	physical = vnzToPhysical(address);
	vnzCachePurge(physical, range.size);
	physical = vnzToPhysical(address);
	vnzL2Invalidate(physical, range.size);
	return rangeAddress(&range, address);
}

uint32_t leaveMemoryAccessProtocol(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, L2_LINE);
	if (inUncachedWindow(&range))
		address = vnzMemoryLeave(vnzMemoryUncachedToPhysical(range.base, range.size),
			range.size);
	else if (inPublicWindow(&range))
		address = vnzMemoryLeave(vnzMemoryPublicToPhysical(range.base, range.size),
			range.size);
	else
		address = vkUnmanagedLeave(vkUnmanagedUncachedToPhysical(range.base, range.size),
			range.size);
	return rangeAddress(&range, address);
}

/* the range's physical address, the caches left alone */
uint32_t openRawMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, L2_LINE);
	if (inUncachedWindow(&range))
		address = vnzUncachedOpenRaw(range.base, range.size);
	else if (inPublicWindow(&range))
		address = vnzPublicOpenRaw(range.base, range.size);
	else
		address = vkUnmanagedPublicOpenRaw(range.base, range.size);
	return rangeAddress(&range, address);
}

uint32_t closeRawMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, L2_LINE);
	if (inUncachedWindow(&range))
		address = vnzUncachedCloseRaw(range.base, range.size);
	else if (inPublicWindow(&range))
		address = vnzPublicCloseRaw(range.base, range.size);
	else
		address = vkUnmanagedPublicCloseRaw(range.base, range.size);
	return rangeAddress(&range, address);
}

uint32_t openPublicMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, L2_LINE);
	address = range.base;
	if (inUncachedWindow(&range))
		address = vnzUncachedOpenPublic(range.base, range.size);
	else if (!inPublicWindow(&range))
		address = vkUnmanagedOpenPublic(range.base, range.size);
	return rangeAddress(&range, address);
}

uint32_t closePublicMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, L2_LINE);
	address = range.base;
	if (inUncachedWindow(&range))
		address = vnzUncachedClosePublic(range.base, range.size);
	else if (inPublicWindow(&range))
		vnzL2Purge(range.base, range.size);
	else
		address = vkUnmanagedClosePublic(range.base, range.size);
	return rangeAddress(&range, address);
}

uint32_t openProtectedMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, LINE);
	if (vnzToPublic(range.base) && vnzToPublic(range.last))
		address = vnzPublicOpenProtected(range.base, range.size);
	else
		address = vkUnmanagedOpenProtected(range.base, range.size);
	if (vnzCurrentCoreId() != VNZ_UNCACHED_CORE_ID)
		vnzCachePurge(vnzToPhysical(address), range.size);
	return rangeAddress(&range, address);
}

uint32_t closeProtectedMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, LINE);
	if (vnzToPublic(range.base) && vnzToPublic(range.last))
		address = vnzPublicCloseProtected(range.base, range.size);
	else
		address = vkUnmanagedCloseProtected(range.base, range.size);
	return rangeAddress(&range, address);
}

uint32_t openPrivateMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, LINE);
	if (vnzToPublic(range.base) && vnzToPublic(range.last))
		address = vnzPublicOpenPrivate(range.base, range.size);
	else
		address = vkUnmanagedOpenPrivate(range.base, range.size);
	if (vnzCurrentCoreId() != VNZ_UNCACHED_CORE_ID)
		vnzCachePurge(vnzToPhysical(address), range.size);
	return rangeAddress(&range, address);
}

uint32_t closePrivateMemory(uint32_t address, uint32_t size)
{
	MemoryRange range;

	memoryRange(&range, address, size, LINE);
	if (vnzToPublic(range.base) && vnzToPublic(range.last))
		address = vnzPublicClosePrivate(range.base, range.size);
	else
		address = vkUnmanagedClosePrivate(range.base, range.size);
	return rangeAddress(&range, address);
}

/*
 * Memory opened in one call (VVNZRunner's VnzExtMemory): entered from
 * unmanaged (or given public), then made public, private or protected.
 */
void vnzExtMemoryCreate(VnzExtMemory *memory)
{
	memory->paddr = 0;
	memory->vaddrManagedHeader = 0;
	memory->vaddrPublicVnz = 0;
	memory->vaddrPrivateVnz = 0;
	memory->vaddrProtectedVnz = 0;
	memory->size = 0;
}

int32_t vnzExtMemoryOpen(VnzExtMemory *memory, uint32_t isManagedMemory, uint32_t mode,
	uint32_t *address, uint32_t size)
{
	uint32_t opened, *target;
	int32_t error;

	if (memory == NULL || address == NULL || *address == 0)
		return SCE_CODECENGINE_ERROR_INVALID_POINTER;
	if (size == 0)
		return SCE_CODECENGINE_ERROR_INVALID_SIZE;
	memory->size = size;
	if (isManagedMemory == 0)
		target = &memory->paddr;
	else if (isManagedMemory == 1)
		target = &memory->vaddrPublicVnz;
	else {
		error = SCE_CODECENGINE_ERROR_INVALID_VALUE;
		goto fail;
	}
	*target = *address;

	opened = 0;
	if (memory->paddr) {
		opened = enterMemoryAccessProtocol(memory->paddr, size);
		if (opened == 0)
			goto failOpen;
		memory->vaddrManagedHeader = opened;
		opened = openPublicMemory(opened, size);
		if (opened == 0)
			goto failOpen;
		memory->vaddrPublicVnz = opened;
	}

	if (mode == VNZ_MEMORY_MODE_PROTECTED) {
		opened = openProtectedMemory(memory->vaddrPublicVnz, size);
		if (opened == 0)
			goto failOpen;
		memory->vaddrProtectedVnz = opened;
	} else if (mode == VNZ_MEMORY_MODE_PRIVATE) {
		opened = openPrivateMemory(memory->vaddrPublicVnz, size);
		if (opened == 0)
			goto failOpen;
		memory->vaddrPrivateVnz = opened;
	} else if (mode != VNZ_MEMORY_MODE_PUBLIC) {
		error = SCE_CODECENGINE_ERROR_INVALID_VALUE;
		goto fail;
	}
	*address = opened;
	return 0;

failOpen:
	error = VNZ_EXT_MEMORY_ERROR_OPEN;
fail:
	vnzExtMemoryClose(memory, address);
	return error;
}

int32_t vnzExtMemoryClose(VnzExtMemory *memory, uint32_t *address)
{
	int32_t result = 0;

	if (memory == NULL || address == NULL)
		return SCE_CODECENGINE_ERROR_INVALID_POINTER;
	if (memory->size == 0)
		return 0;
	if (memory->vaddrPrivateVnz) {
		if (!closePrivateMemory(memory->vaddrPrivateVnz, memory->size))
			result = VNZ_EXT_MEMORY_ERROR_CLOSE;
		memory->vaddrPrivateVnz = 0;
	}
	if (memory->vaddrProtectedVnz) {
		if (!closeProtectedMemory(memory->vaddrProtectedVnz, memory->size))
			result = VNZ_EXT_MEMORY_ERROR_CLOSE;
		memory->vaddrProtectedVnz = 0;
	}
	if (memory->vaddrPublicVnz) {
		if (memory->paddr == 0)
			*address = memory->vaddrPublicVnz;
		else if (!closePublicMemory(memory->vaddrPublicVnz, memory->size))
			result = VNZ_EXT_MEMORY_ERROR_CLOSE;
		memory->vaddrPublicVnz = 0;
	}
	if (memory->vaddrManagedHeader) {
		if (!leaveMemoryAccessProtocol(memory->vaddrManagedHeader, memory->size))
			result = VNZ_EXT_MEMORY_ERROR_CLOSE;
		memory->vaddrManagedHeader = 0;
	}
	if (memory->paddr) {
		*address = memory->paddr;
		memory->paddr = 0;
	}
	memory->size = 0;
	return result;
}
