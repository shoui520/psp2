/*
 * Unmanaged memory, the driver at +0xc000 of the image: the memory
 * protocol's operations (application/memory.c) for memory outside the
 * kernel's windows, where an address is the same in every state and only
 * this core's cache needs keeping. The same operations for the windows are
 * the kernel's (kernel/api.c).
 *
 * The driver is not in the configuration's module table; the application
 * calls it directly.
 */
#include "unmanaged.h"

#define LINE		64
#define L2_LINE		0x100

static uint32_t roundLine(uint32_t size)
{
	return (size + LINE - 1) & ~(LINE - 1);
}

static uint32_t roundL2Line(uint32_t size)
{
	return (size + L2_LINE - 1) & ~(L2_LINE - 1);
}

static void moduleResume(uint32_t primary, uint32_t resumed)
{
}

static void moduleSuspend(void)
{
}

const VkModule g_vkUnmanagedModule VK_SECTION(".unmanaged.module") = {
	.magic = VK_MODULE_DRIVER,
	.self = VNZ_PHYSICAL_OF(0x80c000),
	.size = 0x41c,
	.attributes = VK_MODULE_ATTRIBUTES,
	.version = VK_MODULE_VERSION,
	.resume = moduleResume,
	.suspend = moduleSuspend,
};

/* the base operations: no address changes */
uint32_t vkUnmanagedEnter(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedLeave(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedPhysicalToStack(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedPhysicalToPublic(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedPhysicalToUncached(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedStackToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedPublicToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedUncachedToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

/* the state changes */
uint32_t vkUnmanagedOpenPrivate(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedClosePrivate(uint32_t address, uint32_t size)
{
	vnzCacheWriteback(address, roundLine(size));
	return address;
}

uint32_t vkUnmanagedOpenProtected(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedCloseProtected(uint32_t address, uint32_t size)
{
	vnzCachePurge(address, roundLine(size));
	return address;
}

uint32_t vkUnmanagedOpenPublic(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedClosePublic(uint32_t address, uint32_t size)
{
	size = roundL2Line(size);
	mep_syncm();
	vnzL2Purge(address, size);
	return address;
}

uint32_t vkUnmanagedUncachedOpenRaw(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedUncachedCloseRaw(uint32_t address, uint32_t size)
{
	vnzCachePurgeAllCores(address, roundLine(size));
	return address;
}

uint32_t vkUnmanagedPublicOpenRaw(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vkUnmanagedPublicCloseRaw(uint32_t address, uint32_t size)
{
	size = roundLine(size);
	vnzCachePurgeAllCores(address, size);
	vnzL2Invalidate(address, roundL2Line(size));
	return address;
}

/* allocation from a heap, in a state */
uint32_t vkUnmanagedAllocateStack(VkHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, LINE);

	if (address == 0)
		return 0;
	size = roundLine(size);
	return vkUnmanagedPhysicalToStack(vkUnmanagedEnter(address, size), size);
}

uint32_t vkUnmanagedAllocatePublic(VkHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, LINE);

	if (address == 0)
		return 0;
	size = roundLine(size);
	return vkUnmanagedPhysicalToPublic(vkUnmanagedEnter(address, size), size);
}

uint32_t vkUnmanagedAllocateUncached(VkHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, L2_LINE);

	if (address == 0)
		return 0;
	size = roundL2Line(size);
	vnzL2Invalidate(address, size);
	return vkUnmanagedPhysicalToUncached(vkUnmanagedEnter(address, size), size);
}

void vkUnmanagedFreeStack(VkHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundLine(size);

	address = vkUnmanagedStackToPhysical(address, rounded);
	vnzCachePurge(address, rounded);
	address = vkUnmanagedLeave(address, rounded);
	vnzHeapFree(heap, (void *)address, size, LINE);
}

void vkUnmanagedFreePublic(VkHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundLine(size);

	address = vkUnmanagedLeave(vkUnmanagedPublicToPhysical(address, rounded), rounded);
	vnzHeapFree(heap, (void *)address, size, LINE);
}

void vkUnmanagedFreeUncached(VkHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundL2Line(size);

	address = vkUnmanagedLeave(vkUnmanagedUncachedToPhysical(address, rounded), rounded);
	vnzHeapFree(heap, (void *)address, size, L2_LINE);
}

/* the cache lines wholly inside a range of at most 0x2000 bytes */
void vkUnmanagedInvalidateInside(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCacheInvalidate(start, end - start);
}

void vkUnmanagedPrefetchInside(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCachePrefetch(start, end - start);
}

/* unmanaged memory is not in the L2's windows */
void vkUnmanagedL2FlushInside(uint32_t address, uint32_t size)
{
}

void vkUnmanagedL2SyncInside(uint32_t address, uint32_t size)
{
}
