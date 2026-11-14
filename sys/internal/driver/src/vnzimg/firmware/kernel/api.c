/*
 * The kernel's interface for the drivers and the application (vnz*): the
 * kernel's own functions, behind checks of their arguments. The names follow
 * the community's (VVNZRunner's vnzVThread*, vnzMemory*).
 */
#include "api.h"
#define VNZ_SMALL_RANGE		0x2000	/* the most a line operation covers */

static int priorityValid(uint32_t priority)
{
	return priority >= 1 && priority <= VNZ_KERNEL_PRIORITIES - 1;
}

/* threads */
int32_t vnzThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize)
{
	if (!priorityValid(priority) || stackSize < 0x100)
		return VNZ_ERROR;
	return vnzKernelThreadCreateHere(entry, coreId, priority, stackSize);
}

int32_t vnzThreadCreateOnHeap(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VnzKernelHeap *heap)
{
	if (!priorityValid(priority) || stackSize < 0x100)
		return VNZ_ERROR;
	return vnzKernelThreadCreate(entry, coreId, priority, stackSize, heap);
}

int32_t vnzThreadStart(VnzKernelThread *thread, uint32_t argument, uint32_t argument2)
{
	return vnzKernelThreadStart(thread, argument, argument2);
}

void vnzThreadExit(void)
{
	vnzKernelThreadExit();
}

void vnzThreadExitWithStatus(uint32_t status, uint32_t status2)
{
	vnzKernelThreadExitWithStatus(status, status2);
}

int32_t vnzThreadDelete(VnzKernelThread *thread)
{
	return vnzKernelThreadDelete(thread);
}

void vnzThreadYield(void)
{
	vnzKernelThreadYield();
}

int32_t vnzThreadSetPriority(VnzKernelThread *thread, uint32_t priority)
{
	if (!priorityValid(priority))
		return VNZ_ERROR;
	vnzKernelThreadSetPriority(thread, priority);
	return 0;
}

VnzKernelThread *vnzThreadCurrent(void)
{
	return vnzKernelThreadCurrent();
}

uint32_t vnzThreadPriority(VnzKernelThread *thread)
{
	return vnzKernelThreadPriority(thread);
}

void vnzThreadInformation(VnzKernelThread *thread, VnzKernelThreadInformation *information)
{
	vnzKernelThreadInformation(thread, information);
}

void vnzDisableDispatch(void)
{
	vnzKernelDisableDispatch();
}

void vnzEnableDispatch(void)
{
	vnzKernelEnableDispatch();
}

uint32_t vnzThreadWaitChildren(void)
{
	return vnzKernelThreadWaitChildren();
}

uint32_t vnzThreadJoining(void)
{
	return vnzKernelThreadJoining();
}

void vnzThreadDetach(VnzKernelThread *thread)
{
	vnzKernelThreadDetach(thread);
}

uint32_t vnzThreadIsAlive(VnzKernelThread *thread)
{
	return vnzKernelThreadIsAlive(thread);
}

uint32_t vnzThreadHasExited(VnzKernelThread *thread)
{
	return vnzKernelThreadHasExited(thread);
}

int32_t vnzThreadNotifyExit(VnzKernelThread *thread, uint32_t flags)
{
	return vnzKernelThreadNotifyExit(thread, flags);
}

int32_t vnzThreadCancelExitNotify(VnzKernelThread *thread)
{
	return vnzKernelThreadCancelExitNotify(thread);
}

int32_t vnzThreadExitStatus(VnzKernelThread *thread, uint32_t *status, uint32_t *status2)
{
	return vnzKernelThreadExitStatus(thread, status, status2);
}

uint32_t vnzExitedThreads(void)
{
	return vnzKernelExitedThreads();
}

int32_t vnzThreadSetFlags(VnzKernelThread *thread, uint32_t flags)
{
	if (flags == 0)
		return VNZ_ERROR;
	vnzKernelThreadSetFlags(thread, flags);
	return 0;
}

uint32_t vnzThreadWaitFlags(uint32_t flags)
{
	if (flags == 0)
		return 0;
	return vnzKernelThreadWaitFlags(flags);
}

uint32_t vnzThreadClearFlags(uint32_t flags)
{
	return vnzKernelThreadClearFlags(flags);
}

uint32_t *vnzThreadLocal0(void)
{
	return vnzKernelThreadLocal0();
}

uint32_t *vnzThreadLocal1(void)
{
	return vnzKernelThreadLocal1();
}

uint32_t *vnzThreadLocal2(void)
{
	return vnzKernelThreadLocal2();
}

int32_t vnzThreadMove(VnzKernelThread *thread, uint32_t coreId)
{
	return vnzKernelThreadMigrate(thread, coreId, NULL, 0);
}

int32_t vnzThreadMoveNotify(VnzKernelThread *thread, uint32_t coreId, VnzKernelThread *notify,
	uint32_t flags)
{
	return vnzKernelThreadMigrate(thread, coreId, notify, flags);
}

uint32_t vnzThreadAttributes(VnzKernelThread *thread)
{
	return vnzKernelThreadAttributes(thread);
}

/* memory */
uint32_t vnzMemoryRegion(uint32_t id, uint32_t *size)
{
	return vnzKernelMemoryRegion(id, size);
}

VnzKernelHeap *vnzHeapCreatePool(uint32_t base, uint32_t size)
{
	return vnzKernelHeapCreatePool(base, size);
}

void vnzHeapDeletePool(VnzKernelHeap *heap)
{
	vnzKernelHeapDeletePool(heap);
}

VnzKernelHeap *vnzHeapFind(uint32_t address, uint32_t size)
{
	return vnzKernelHeapFind(address, size);
}

VnzKernelHeap *vnzHeapCreate(uint32_t base, uint32_t size, VnzKernelHeapCreate create,
	VnzKernelHeapDelete delete,
	VnzKernelHeapAllocate allocate, VnzKernelHeapFree free)
{
	return vnzKernelHeapCreate(base, size, create, delete, allocate, free);
}

void vnzHeapDelete(VnzKernelHeap *heap)
{
	vnzKernelHeapDelete(heap);
}

void *vnzHeapContext(VnzKernelHeap *heap)
{
	return vnzKernelHeapContext(heap);
}

void *vnzHeapAllocateStack(VnzKernelHeap *heap, uint32_t size)
{
	return vnzKernelHeapAllocateStack(heap, size);
}

void *vnzHeapAllocatePublic(VnzKernelHeap *heap, uint32_t size)
{
	return vnzKernelHeapAllocatePublic(heap, size);
}

void *vnzHeapAllocateUncached(VnzKernelHeap *heap, uint32_t size)
{
	return vnzKernelHeapAllocateUncached(heap, size);
}

void *vnzHeapAllocate(VnzKernelHeap *heap, uint32_t size, uint32_t alignment)
{
	return vnzKernelHeapAllocate(heap, size, alignment);
}

void vnzHeapFreeStack(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	vnzKernelHeapFreeStack(heap, memory, size);
}

void vnzHeapFreePublic(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	vnzKernelHeapFreePublic(heap, memory, size);
}

void vnzHeapFreeUncached(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	vnzKernelHeapFreeUncached(heap, memory, size);
}

void vnzHeapFree(VnzKernelHeap *heap, void *memory, uint32_t size, uint32_t alignment)
{
	vnzKernelHeapFree(heap, memory, size, alignment);
}

VnzKernelHeap *vnzSystemHeap(void)
{
	return vnzKernelSystemHeap();
}

VnzKernelHeap *vnzCurrentHeap(void)
{
	return vnzKernelCurrentHeap();
}

/* the functions of the kernel's kind of heap, for heaps of a caller's own */
int32_t vnzPoolHeapCreate(void *context, uint32_t base, uint32_t size)
{
	return vnzKernelPoolHeapCreate(context, base, size);
}

void vnzPoolHeapDelete(void *context, uint32_t base, uint32_t size)
{
	vnzKernelPoolHeapDelete(context, base, size);
}

void *vnzPoolHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window)
{
	return vnzKernelPoolHeapAllocate(context, size, alignment, window);
}

void vnzPoolHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment, int32_t window)
{
	vnzKernelPoolHeapFree(context, memory, size, alignment, window);
}

/* the data cache */
void vnzCachePurge(uint32_t address, uint32_t size)
{
	vnzKernelCachePurge(address, size);
}

void vnzCachePurgeAllCores(uint32_t address, uint32_t size)
{
	vnzKernelCachePurgeAllCores(address, size);
}

void vnzCacheFlush(uint32_t address, uint32_t size)
{
	vnzKernelCacheFlush(address, size);
}

void vnzCacheWriteback(uint32_t address, uint32_t size)
{
	vnzKernelCacheWriteback(address, size);
}

void vnzCachePrefetch(uint32_t address, uint32_t size)
{
	vnzKernelCachePrefetch(address, size);
}

void vnzCacheInvalidate(uint32_t address, uint32_t size)
{
	vnzKernelCacheInvalidate(address, size);
}

void vnzL2PurgeAllCores(uint32_t address, uint32_t size)
{
	vnzKernelL2PurgeAllCores(address, size);
}

void vnzL2InvalidateTags(uint32_t address, uint32_t size)
{
	vnzKernelL2InvalidateTags(address, size);
}

/* the L2: line by line up to VNZ_KERNEL_L2_LINE_LIMIT, by the controller past it */
static void l2Range(uint32_t address, uint32_t size, uint32_t operation)
{
	if (size <= VNZ_KERNEL_L2_LINE_LIMIT)
		vnzKernelL2LineOperation(address, size, operation);
	else
		vnzKernelL2Operation(address, size, operation);
}

void vnzL2Invalidate(uint32_t address, uint32_t size)
{
	l2Range(address, size, VNZ_KERNEL_L2_INVALIDATE);
}

void vnzL2Writeback(uint32_t address, uint32_t size)
{
	l2Range(address, size, VNZ_KERNEL_L2_WRITEBACK);
}

void vnzL2Purge(uint32_t address, uint32_t size)
{
	l2Range(address, size, VNZ_KERNEL_L2_PURGE);
}

void vnzL2Sync(uint32_t address, uint32_t size)
{
	vnzKernelL2LineOperation(address, size, VNZ_KERNEL_L2_SYNC);
}

void vnzL2Flush(uint32_t address, uint32_t size)
{
	vnzKernelL2LineOperation(address, size, VNZ_KERNEL_L2_FLUSH);
}

void vnzL2SetMode(uint32_t mode)
{
	vnzKernelL2SetMode(mode);
}

void vnzL2Disable(void)
{
	vnzKernelL2Disable();
}

void vnzL2Enable(void)
{
	vnzKernelL2Enable();
}

/* spinlocks */
uint32_t vnzSpinTry(volatile uint8_t *lock)
{
	return vnzKernelSpinTry(lock);
}

void vnzSpinUnlock(volatile uint8_t *lock)
{
	vnzKernelSpinUnlock(lock);
}

void vnzSpinLock(volatile uint8_t *lock)
{
	vnzKernelSpinLock(lock);
}

/* memory for the current thread */
/* the memory protocol's base operations (most change nothing here) */
uint32_t vnzMemoryEnter(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzMemoryLeave(uint32_t address, uint32_t size)
{
	return address;
}

void vnzMemoryResume(void)
{
}

void vnzMemorySuspend(void)
{
}

uint32_t vnzMemoryPhysicalToStack(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzMemoryPhysicalToPublic(uint32_t address, uint32_t size)
{
	return vnzKernelPhysicalToPublic(address);
}

uint32_t vnzMemoryPhysicalToUncached(uint32_t address, uint32_t size)
{
	return vnzKernelPhysicalToUncached(address);
}

uint32_t vnzMemoryStackToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzMemoryPublicToPhysical(uint32_t address, uint32_t size)
{
	return vnzKernelPublicToPhysical(address);
}

uint32_t vnzMemoryUncachedToPhysical(uint32_t address, uint32_t size)
{
	return vnzKernelUncachedToPhysical(address);
}

/* a thread's stack changes hands (vnzKernelThreadStart, and before it is freed) */
void vnzMemorySync(uint32_t address, uint32_t size, VnzKernelThread *owner)
{
}

void *vnzAllocateStack(uint32_t size)
{
	return vnzKernelHeapAllocateStack(vnzKernelCurrentHeap(), size);
}

void vnzFreeStack(void *memory, uint32_t size)
{
	vnzKernelHeapFreeStack(vnzKernelCurrentHeap(), memory, size);
}

void *vnzKernelAllocatePublic(uint32_t size)
{
	return vnzKernelHeapAllocatePublic(vnzKernelCurrentHeap(), size);
}

void vnzKernelFreePublic(void *memory, uint32_t size)
{
	vnzKernelHeapFreePublic(vnzKernelCurrentHeap(), memory, size);
}

void *vnzAllocateUncached(uint32_t size)
{
	return vnzKernelHeapAllocateUncached(vnzKernelCurrentHeap(), size);
}

void vnzFreeUncached(void *memory, uint32_t size)
{
	vnzKernelHeapFreeUncached(vnzKernelCurrentHeap(), memory, size);
}

/*
 * The memory protocol's state changes (application/memory.c), for memory the
 * kernel manages: the public window (DRAM through the L2) and the uncached
 * window. drivers/unmanaged.c has the same operations for the rest.
 * Private: this core's cached copy; protected: read only; public: shared
 * through the L2; raw: the physical address, the caches left alone.
 */
uint32_t vnzPublicOpenPrivate(uint32_t address, uint32_t size)
{
	return vnzKernelPublicToPhysical(address);
}

uint32_t vnzPublicClosePrivate(uint32_t address, uint32_t size)
{
	vnzKernelCacheWriteback(address, (size + 63) & ~63);
	return vnzKernelPhysicalToPublic(address);
}

uint32_t vnzPublicOpenProtected(uint32_t address, uint32_t size)
{
	if (address >= 0xc0000000 && address < 0xe0000000)
		return vnzKernelPublicToPhysical(address);
	return address;
}

uint32_t vnzPublicCloseProtected(uint32_t address, uint32_t size)
{
	if (address < VNZ_DRAM || address >= VNZ_DRAM + 0xa8000000)
		return address;
	vnzKernelCachePurge(address, (size + 63) & ~63);
	return vnzKernelPhysicalToPublic(address);
}

uint32_t vnzUncachedOpenPublic(uint32_t address, uint32_t size)
{
	return vnzKernelUncachedToPublic(address);
}

/* to uncached: purged from the caches first */
uint32_t vnzUncachedClosePublic(uint32_t address, uint32_t size)
{
	uint32_t physical;

	size = (size + 0xff) & ~0xff;
	mep_syncm();
	physical = vnzKernelPublicToPhysical(address);
	l2Range(physical, size, VNZ_KERNEL_L2_PURGE);
	return vnzKernelPhysicalToUncached(physical);
}

uint32_t vnzUncachedOpenRaw(uint32_t address, uint32_t size)
{
	return vnzKernelUncachedToPhysical(address);
}

uint32_t vnzUncachedCloseRaw(uint32_t address, uint32_t size)
{
	size = (size + 63) & ~63;
	vnzKernelCachePurgeAllCores(address, size);
	l2Range(address, (size + 0xff) & ~0xff, VNZ_KERNEL_L2_INVALIDATE);
	return vnzKernelPhysicalToUncached(address);
}

uint32_t vnzPublicOpenRaw(uint32_t address, uint32_t size)
{
	return vnzKernelPublicToPhysical(address);
}

uint32_t vnzPublicCloseRaw(uint32_t address, uint32_t size)
{
	vnzKernelCachePurgeAllCores(address, (size + 63) & ~63);
	return vnzKernelPhysicalToPublic(address);
}

/* the lines wholly inside a range, for ranges up to VNZ_SMALL_RANGE */
#define WHOLE_LINES(address, size, line, start, end) ({			\
	start = ((address) + (line) - 1) & ~((line) - 1);		\
	end = ((address) + (size)) & ~((line) - 1);			\
	(size) <= VNZ_SMALL_RANGE && start < end; })

void vnzCacheInvalidateInside(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vnzKernelCacheInvalidate(start, end - start);
}

void vnzCachePrefetchInside(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vnzKernelCachePrefetch(start, end - start);
}

void vnzL2FlushInside(uint32_t address, uint32_t size)
{
	uint32_t start, end, physical;

	if (size > VNZ_SMALL_RANGE)
		return;
	physical = vnzKernelPublicToPhysical(address);
	if (WHOLE_LINES(physical, size, 0x100, start, end))
		vnzKernelL2LineOperation(start, end - start, VNZ_KERNEL_L2_FLUSH);
}

void vnzL2SyncInside(uint32_t address, uint32_t size)
{
	uint32_t start, end, physical;

	if (size > VNZ_SMALL_RANGE)
		return;
	physical = vnzKernelPublicToPhysical(address);
	if (WHOLE_LINES(physical, size, 0x100, start, end))
		vnzKernelL2LineOperation(start, end - start, VNZ_KERNEL_L2_SYNC);
}

/* three more copies of vnzCachePrefetchInside in Sony's kernel; nothing calls them */
void vnzCachePrefetchInsideCopy1(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vnzKernelCachePrefetch(start, end - start);
}

void vnzCachePrefetchInsideCopy2(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vnzKernelCachePrefetch(start, end - start);
}

void vnzCachePrefetchInsideCopy3(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vnzKernelCachePrefetch(start, end - start);
}

/* address windows */
uint32_t vnzUncachedToPhysical(uint32_t address)
{
	return vnzKernelUncachedToPhysical(address);
}

uint32_t vnzPhysicalToUncached(uint32_t address)
{
	return vnzKernelPhysicalToUncached(address);
}

uint32_t vnzDramAddress(uint32_t address)
{
	return vnzKernelDramAddress(address);
}

/* a second copy in Sony's kernel; nothing calls it */
uint32_t vnzDramAddressCopy(uint32_t address)
{
	return vnzKernelDramAddress(address);
}

uint32_t vnzToPhysical(uint32_t address)
{
	return vnzKernelToPhysical(address);
}

uint32_t vnzToPublic(uint32_t address)
{
	return vnzKernelToPublic(address);
}

uint32_t vnzToUncached(uint32_t address)
{
	return vnzKernelToUncached(address);
}

uint32_t vnzCurrentCoreId(void)
{
	return vnzKernelCurrentCoreId();
}

/* locks */
volatile uint8_t *vnzLockAllocate(void)
{
	return vnzKernelLockPageAllocate();
}

void vnzLockFree(volatile uint8_t *lock)
{
	vnzKernelLockPageFree(lock);
}

/* interrupts: levels below 16, edge or level triggered */
VnzKernelInterruptHandler *vnzInterruptRegister(uint32_t interrupt, int32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t, uint32_t), uint32_t argument, uint32_t argument2)
{
	if (level < 0 || level >= 0x10 || (edge != 0 && edge != 1))
		return NULL;
	return vnzKernelInterruptRegister(interrupt, level, edge, handler, argument, argument2);
}

void vnzInterruptRelease(VnzKernelInterruptHandler *handler)
{
	vnzKernelInterruptRelease(handler);
}

int32_t vnzInterruptEnable(VnzKernelInterruptHandler *handler)
{
	return vnzKernelInterruptEnable(handler);
}

int32_t vnzInterruptDisable(VnzKernelInterruptHandler *handler)
{
	return vnzKernelInterruptDisable(handler);
}

/* crashes */
int32_t vnzCrashSetHook(VnzKernelCrashHook hook, uint32_t argument, uint32_t argument2)
{
	return vnzKernelCrashSetHook(hook, argument, argument2);
}

int32_t vnzCrashClearHook(void)
{
	return vnzKernelCrashClearHook();
}

int32_t vnzCrashCopyRegister(uint32_t id, const void *source, uint32_t size)
{
	return vnzKernelCrashCopyRegister(id, source, size);
}

/* tracing, into a memory region (id) */
int32_t vnzTraceSetup(uint32_t id, uint32_t mode)
{
	uint32_t base, size;

	if (mode >= 2)
		return VNZ_ERROR;
	base = vnzKernelMemoryRegion(id, &size);
	if (base == 0)
		return VNZ_ERROR;
	return vnzKernelTraceSetup((void *)vnzKernelPhysicalToPublic(base), size, mode);
}

int32_t vnzTraceSetShare(uint32_t coreId, uint32_t size)
{
	return vnzKernelTraceSetShare(coreId, size);
}

int32_t vnzTraceStart(uint32_t mask)
{
	return vnzKernelTraceStart(mask);
}

int32_t vnzTraceStop(void)
{
	return vnzKernelTraceStop();
}

/* Stop using the buffer, written back for the reader. */
void vnzTraceRelease(void)
{
	uint32_t size;
	void *buffer = vnzKernelTraceBuffer(&size);

	vnzKernelTraceRelease();
	l2Range(vnzKernelPublicToPhysical((uint32_t)buffer), size, VNZ_KERNEL_L2_WRITEBACK);
}

/* power */
uint32_t vnzPowerSetCores(uint32_t expected, uint32_t cores)
{
	return vnzKernelPowerSetCores(expected, cores);
}

VnzKernelPowerHook *vnzPowerNotifyCreate(VnzKernelThread *thread, uint32_t flags)
{
	return vnzKernelPowerNotifyCreate(thread, flags);
}

void vnzPowerNotifyDelete(VnzKernelPowerHook *hook)
{
	vnzKernelPowerNotifyDelete(hook);
}

void vnzPowerNotifyRegister(VnzKernelPowerHook *hook)
{
	vnzKernelPowerNotifyRegister(hook);
}

void vnzPowerNotifyUnregister(VnzKernelPowerHook *hook)
{
	vnzKernelPowerNotifyUnregister(hook);
}

uint32_t vnzPowerActiveCores(void)
{
	return vnzKernelPowerActiveCores();
}

VnzKernelPowerHook *vnzPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument)
{
	return vnzKernelPowerCallbackCreate(function, argument);
}

void vnzPowerCallbackDelete(VnzKernelPowerHook *hook)
{
	vnzKernelPowerCallbackDelete(hook);
}

void vnzPowerCallbackRegister(VnzKernelPowerHook *hook)
{
	vnzKernelPowerCallbackRegister(hook);
}

void vnzPowerCallbackUnregister(VnzKernelPowerHook *hook)
{
	vnzKernelPowerCallbackUnregister(hook);
}
