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

extern const uint8_t vnzKernelUnmanagedSize[];	/* the link's (image.ld) */

const VnzKernelModule g_vnzKernelUnmanagedModule VNZ_KERNEL_SECTION(".unmanaged.module") = {
	.magic = VNZ_KERNEL_MODULE_DRIVER,
	.self = VNZ_PHYSICAL_OF(0x80c000),
	.size = (uint32_t)vnzKernelUnmanagedSize,
	.attributes = VNZ_KERNEL_MODULE_ATTRIBUTES,
	.version = VNZ_KERNEL_MODULE_VERSION,
	.resume = moduleResume,
	.suspend = moduleSuspend,
};

/* the base operations: no address changes */
uint32_t vnzKernelUnmanagedEnter(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedLeave(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedPhysicalToStack(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedPhysicalToPublic(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedPhysicalToUncached(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedStackToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedPublicToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedUncachedToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

/* the state changes */
uint32_t vnzKernelUnmanagedOpenPrivate(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedClosePrivate(uint32_t address, uint32_t size)
{
	vnzCacheWriteback(address, roundLine(size));
	return address;
}

uint32_t vnzKernelUnmanagedOpenProtected(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedCloseProtected(uint32_t address, uint32_t size)
{
	vnzCachePurge(address, roundLine(size));
	return address;
}

uint32_t vnzKernelUnmanagedOpenPublic(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedClosePublic(uint32_t address, uint32_t size)
{
	size = roundL2Line(size);
	mep_syncm();
	vnzL2Purge(address, size);
	return address;
}

uint32_t vnzKernelUnmanagedUncachedOpenRaw(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedUncachedCloseRaw(uint32_t address, uint32_t size)
{
	vnzCachePurgeAllCores(address, roundLine(size));
	return address;
}

uint32_t vnzKernelUnmanagedPublicOpenRaw(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedPublicCloseRaw(uint32_t address, uint32_t size)
{
	size = roundLine(size);
	vnzCachePurgeAllCores(address, size);
	vnzL2Invalidate(address, roundL2Line(size));
	return address;
}

/* allocation from a heap, in a state */
uint32_t vnzKernelUnmanagedAllocateStack(VnzKernelHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, LINE);

	if (address == 0)
		return 0;
	size = roundLine(size);
	return vnzKernelUnmanagedPhysicalToStack(vnzKernelUnmanagedEnter(address, size), size);
}

uint32_t vnzKernelUnmanagedAllocatePublic(VnzKernelHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, LINE);

	if (address == 0)
		return 0;
	size = roundLine(size);
	return vnzKernelUnmanagedPhysicalToPublic(vnzKernelUnmanagedEnter(address, size), size);
}

uint32_t vnzKernelUnmanagedAllocateUncached(VnzKernelHeap *heap, uint32_t size)
{
	uint32_t address = (uint32_t)vnzHeapAllocate(heap, size, L2_LINE);

	if (address == 0)
		return 0;
	size = roundL2Line(size);
	vnzL2Invalidate(address, size);
	return vnzKernelUnmanagedPhysicalToUncached(vnzKernelUnmanagedEnter(address, size), size);
}

void vnzKernelUnmanagedFreeStack(VnzKernelHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundLine(size);

	address = vnzKernelUnmanagedStackToPhysical(address, rounded);
	vnzCachePurge(address, rounded);
	address = vnzKernelUnmanagedLeave(address, rounded);
	vnzHeapFree(heap, (void *)address, size, LINE);
}

void vnzKernelUnmanagedFreePublic(VnzKernelHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundLine(size);

	address = vnzKernelUnmanagedLeave(vnzKernelUnmanagedPublicToPhysical(address, rounded),
		rounded);
	vnzHeapFree(heap, (void *)address, size, LINE);
}

void vnzKernelUnmanagedFreeUncached(VnzKernelHeap *heap, uint32_t address, uint32_t size)
{
	uint32_t rounded = roundL2Line(size);

	address = vnzKernelUnmanagedLeave(vnzKernelUnmanagedUncachedToPhysical(address, rounded),
		rounded);
	vnzHeapFree(heap, (void *)address, size, L2_LINE);
}

/* four more functions in Sony's driver that keep the address; nothing calls them */
uint32_t vnzKernelUnmanagedSameAddress1(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedSameAddress2(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedSameAddress3(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzKernelUnmanagedSameAddress4(uint32_t address, uint32_t size)
{
	return address;
}

/* the cache lines wholly inside a range of at most 0x2000 bytes */
void vnzKernelUnmanagedInvalidateInside(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCacheInvalidate(start, end - start);
}

void vnzKernelUnmanagedPrefetchInside(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCachePrefetch(start, end - start);
}

/* unmanaged memory is not in the L2's windows */
void vnzKernelUnmanagedL2FlushInside(uint32_t address, uint32_t size)
{
}

void vnzKernelUnmanagedL2SyncInside(uint32_t address, uint32_t size)
{
}

/* three more copies of vnzKernelUnmanagedPrefetchInside and two empty functions in Sony's driver; nothing calls them */
void vnzKernelUnmanagedPrefetchInsideCopy1(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCachePrefetch(start, end - start);
}

void vnzKernelUnmanagedPrefetchInsideCopy2(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCachePrefetch(start, end - start);
}

void vnzKernelUnmanagedPrefetchInsideCopy3(uint32_t address, uint32_t size)
{
	uint32_t start = (address + LINE - 1) & ~(LINE - 1);
	uint32_t end = (address + size) & ~(LINE - 1);

	if (size <= 0x2000 && start < end)
		vnzCachePrefetch(start, end - start);
}

void vnzKernelUnmanagedNothing1(void)
{
}

void vnzKernelUnmanagedNothing2(void)
{
}
