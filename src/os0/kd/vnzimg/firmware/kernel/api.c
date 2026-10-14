/*
 * The kernel's interface for the drivers and the application (vnz*): the
 * kernel's own functions, behind checks of their arguments. The names follow
 * the community's (VVNZRunner's vnzVThread*, vnzMemory*).
 */
#include "api.h"
#define VNZ_SMALL_RANGE		0x2000	/* the most a line operation covers */

static int priorityValid(uint32_t priority)
{
	return priority >= 1 && priority <= VK_PRIORITIES - 1;
}

/* threads */
int32_t vnzThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize)
{
	if (!priorityValid(priority) || stackSize < 0x100)
		return VNZ_ERROR;
	return vkThreadCreateHere(entry, coreId, priority, stackSize);
}

int32_t vnzThreadCreateOnHeap(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VkHeap *heap)
{
	if (!priorityValid(priority) || stackSize < 0x100)
		return VNZ_ERROR;
	return vkThreadCreate(entry, coreId, priority, stackSize, heap);
}

int32_t vnzThreadStart(VkThread *thread, uint32_t argument, uint32_t argument2)
{
	return vkThreadStart(thread, argument, argument2);
}

void vnzThreadExit(void)
{
	vkThreadExit();
}

void vnzThreadExitWithStatus(uint32_t status, uint32_t status2)
{
	vkThreadExitWithStatus(status, status2);
}

int32_t vnzThreadDelete(VkThread *thread)
{
	return vkThreadDelete(thread);
}

void vnzThreadYield(void)
{
	vkThreadYield();
}

int32_t vnzThreadSetPriority(VkThread *thread, uint32_t priority)
{
	if (!priorityValid(priority))
		return VNZ_ERROR;
	vkThreadSetPriority(thread, priority);
	return 0;
}

VkThread *vnzThreadCurrent(void)
{
	return vkThreadCurrent();
}

uint32_t vnzThreadPriority(VkThread *thread)
{
	return vkThreadPriority(thread);
}

void vnzDisableDispatch(void)
{
	vkDisableDispatch();
}

void vnzEnableDispatch(void)
{
	vkEnableDispatch();
}

uint32_t vnzThreadWaitChildren(void)
{
	return vkThreadWaitChildren();
}

uint32_t vnzThreadIsAlive(VkThread *thread)
{
	return vkThreadIsAlive(thread);
}

uint32_t vnzThreadHasExited(VkThread *thread)
{
	return vkThreadHasExited(thread);
}

int32_t vnzThreadNotifyExit(VkThread *thread, uint32_t flags)
{
	return vkThreadNotifyExit(thread, flags);
}

int32_t vnzThreadCancelExitNotify(VkThread *thread)
{
	return vkThreadCancelExitNotify(thread);
}

int32_t vnzThreadExitStatus(VkThread *thread, uint32_t *status, uint32_t *status2)
{
	return vkThreadExitStatus(thread, status, status2);
}

uint32_t vnzExitedThreads(void)
{
	return vkExitedThreads();
}

int32_t vnzThreadSetFlags(VkThread *thread, uint32_t flags)
{
	if (flags == 0)
		return VNZ_ERROR;
	vkThreadSetFlags(thread, flags);
	return 0;
}

uint32_t vnzThreadWaitFlags(uint32_t flags)
{
	if (flags == 0)
		return 0;
	return vkThreadWaitFlags(flags);
}

uint32_t vnzThreadClearFlags(uint32_t flags)
{
	return vkThreadClearFlags(flags);
}

uint32_t *vnzThreadLocal0(void)
{
	return vkThreadLocal(0);
}

uint32_t *vnzThreadLocal1(void)
{
	return vkThreadLocal(1);
}

uint32_t *vnzThreadLocal2(void)
{
	return vkThreadLocal(2);
}

int32_t vnzThreadMove(VkThread *thread, uint32_t coreId)
{
	return vkThreadMigrate(thread, coreId, NULL, 0);
}

int32_t vnzThreadMoveNotify(VkThread *thread, uint32_t coreId, VkThread *notify, uint32_t flags)
{
	return vkThreadMigrate(thread, coreId, notify, flags);
}

uint32_t vnzThreadAttributes(VkThread *thread)
{
	return vkThreadAttributes(thread);
}

/* memory */
uint32_t vnzMemoryRegion(uint32_t id, uint32_t *size)
{
	return vkMemoryRegion(id, size);
}

VkHeap *vnzHeapCreatePool(uint32_t base, uint32_t size)
{
	return vkHeapCreatePool(base, size);
}

void vnzHeapDelete(VkHeap *heap)
{
	vkHeapDelete(heap);
}

VkHeap *vnzHeapFind(uint32_t address, uint32_t size)
{
	return vkHeapFind(address, size);
}

VkHeap *vnzHeapCreate(uint32_t base, uint32_t size, VkHeapCreate create, VkHeapDelete delete,
	VkHeapAllocate allocate, VkHeapFree free)
{
	return vkHeapCreate(base, size, create, delete, allocate, free);
}

void *vnzHeapAllocateStack(VkHeap *heap, uint32_t size)
{
	return vkHeapAllocateStack(heap, size);
}

void *vnzHeapAllocatePublic(VkHeap *heap, uint32_t size)
{
	return vkHeapAllocatePublic(heap, size);
}

void *vnzHeapAllocateUncached(VkHeap *heap, uint32_t size)
{
	return vkHeapAllocateUncached(heap, size);
}

void *vnzHeapAllocate(VkHeap *heap, uint32_t size, uint32_t alignment)
{
	return vkHeapAllocate(heap, size, alignment);
}

void vnzHeapFreeStack(VkHeap *heap, void *memory, uint32_t size)
{
	vkHeapFreeStack(heap, memory, size);
}

void vnzHeapFreePublic(VkHeap *heap, void *memory, uint32_t size)
{
	vkHeapFreePublic(heap, memory, size);
}

void vnzHeapFreeUncached(VkHeap *heap, void *memory, uint32_t size)
{
	vkHeapFreeUncached(heap, memory, size);
}

void vnzHeapFree(VkHeap *heap, void *memory, uint32_t size, uint32_t alignment)
{
	vkHeapFree(heap, memory, size, alignment);
}

VkHeap *vnzSystemHeap(void)
{
	return vkSystemHeap();
}

VkHeap *vnzCurrentHeap(void)
{
	return vkCurrentHeap();
}

/* the data cache */
void vnzCachePurge(uint32_t address, uint32_t size)
{
	vkCachePurge(address, size);
}

void vnzCachePurgeAllCores(uint32_t address, uint32_t size)
{
	vkCachePurgeAllCores(address, size);
}

void vnzCacheFlush(uint32_t address, uint32_t size)
{
	vkCacheFlush(address, size);
}

void vnzCacheWriteback(uint32_t address, uint32_t size)
{
	vkCacheWriteback(address, size);
}

void vnzCachePrefetch(uint32_t address, uint32_t size)
{
	vkCachePrefetch(address, size);
}

void vnzCacheInvalidate(uint32_t address, uint32_t size)
{
	vkCacheInvalidate(address, size);
}

void vnzL2PurgeAllCores(uint32_t address, uint32_t size)
{
	vkL2PurgeAllCores(address, size);
}

void vnzL2InvalidateTags(uint32_t address, uint32_t size)
{
	vkL2InvalidateTags(address, size);
}

/* the L2: line by line up to VK_L2_LINE_LIMIT, by the controller past it */
static void l2Range(uint32_t address, uint32_t size, uint32_t operation)
{
	if (size <= VK_L2_LINE_LIMIT)
		vkL2LineOperation(address, size, operation);
	else
		vkL2Operation(address, size, operation);
}

void vnzL2Invalidate(uint32_t address, uint32_t size)
{
	l2Range(address, size, VK_L2_INVALIDATE);
}

void vnzL2Writeback(uint32_t address, uint32_t size)
{
	l2Range(address, size, VK_L2_WRITEBACK);
}

void vnzL2Purge(uint32_t address, uint32_t size)
{
	l2Range(address, size, VK_L2_PURGE);
}

void vnzL2Sync(uint32_t address, uint32_t size)
{
	vkL2LineOperation(address, size, VK_L2_SYNC);
}

void vnzL2Flush(uint32_t address, uint32_t size)
{
	vkL2LineOperation(address, size, VK_L2_FLUSH);
}

/* spinlocks */
uint32_t vnzSpinTry(volatile uint8_t *lock)
{
	return vkSpinTry(lock);
}

void vnzSpinUnlock(volatile uint8_t *lock)
{
	vkSpinUnlock(lock);
}

void vnzSpinLock(volatile uint8_t *lock)
{
	vkSpinLock(lock);
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
	return vkPhysicalToPublic(address);
}

uint32_t vnzMemoryPhysicalToUncached(uint32_t address, uint32_t size)
{
	return vkPhysicalToUncached(address);
}

uint32_t vnzMemoryStackToPhysical(uint32_t address, uint32_t size)
{
	return address;
}

uint32_t vnzMemoryPublicToPhysical(uint32_t address, uint32_t size)
{
	return vkPublicToPhysical(address);
}

uint32_t vnzMemoryUncachedToPhysical(uint32_t address, uint32_t size)
{
	return vkUncachedToPhysical(address);
}

void vnzMemorySync(void)
{
}

void *vnzAllocateStack(uint32_t size)
{
	return vkHeapAllocateStack(vkCurrentHeap(), size);
}

void vnzFreeStack(void *memory, uint32_t size)
{
	vkHeapFreeStack(vkCurrentHeap(), memory, size);
}

void *vkAllocatePublic(uint32_t size)
{
	return vkHeapAllocatePublic(vkCurrentHeap(), size);
}

void vkFreePublic(void *memory, uint32_t size)
{
	vkHeapFreePublic(vkCurrentHeap(), memory, size);
}

void *vnzAllocateUncached(uint32_t size)
{
	return vkHeapAllocateUncached(vkCurrentHeap(), size);
}

void vnzFreeUncached(void *memory, uint32_t size)
{
	vkHeapFreeUncached(vkCurrentHeap(), memory, size);
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
	return vkPublicToPhysical(address);
}

uint32_t vnzPublicClosePrivate(uint32_t address, uint32_t size)
{
	vkCacheWriteback(address, (size + 63) & ~63);
	return vkPhysicalToPublic(address);
}

uint32_t vnzPublicOpenProtected(uint32_t address, uint32_t size)
{
	if (address >= 0xc0000000 && address < 0xd8000000)
		return vkPublicToPhysical(address);
	return address;
}

uint32_t vnzPublicCloseProtected(uint32_t address, uint32_t size)
{
	if (address < VNZ_DRAM || address >= VNZ_DRAM + 0xa8000000)
		return address;
	vkCachePurge(address, (size + 63) & ~63);
	return vkPhysicalToPublic(address);
}

uint32_t vnzUncachedOpenPublic(uint32_t address, uint32_t size)
{
	return vkUncachedToPublic(address);
}

/* to uncached: purged from the caches first */
uint32_t vnzUncachedClosePublic(uint32_t address, uint32_t size)
{
	uint32_t physical;

	size = (size + 0xff) & ~0xff;
	mep_syncm();
	physical = vkPublicToPhysical(address);
	l2Range(physical, size, VK_L2_PURGE);
	return vkPhysicalToUncached(physical);
}

uint32_t vnzUncachedOpenRaw(uint32_t address, uint32_t size)
{
	return vkUncachedToPhysical(address);
}

uint32_t vnzUncachedCloseRaw(uint32_t address, uint32_t size)
{
	size = (size + 63) & ~63;
	vkCachePurgeAllCores(address, size);
	l2Range(address, (size + 0xff) & ~0xff, VK_L2_INVALIDATE);
	return vkPhysicalToUncached(address);
}

uint32_t vnzPublicOpenRaw(uint32_t address, uint32_t size)
{
	return vkPublicToPhysical(address);
}

uint32_t vnzPublicCloseRaw(uint32_t address, uint32_t size)
{
	vkCachePurgeAllCores(address, (size + 63) & ~63);
	return vkPhysicalToPublic(address);
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
		vkCacheInvalidate(start, end - start);
}

void vnzCachePrefetchInside(uint32_t address, uint32_t size)
{
	uint32_t start, end;

	if (WHOLE_LINES(address, size, 0x40, start, end))
		vkCachePrefetch(start, end - start);
}

void vnzL2FlushInside(uint32_t address, uint32_t size)
{
	uint32_t start, end, physical;

	if (size > VNZ_SMALL_RANGE)
		return;
	physical = vkPublicToPhysical(address);
	if (WHOLE_LINES(physical, size, 0x100, start, end))
		vkL2LineOperation(start, end - start, VK_L2_FLUSH);
}

void vnzL2SyncInside(uint32_t address, uint32_t size)
{
	uint32_t start, end, physical;

	if (size > VNZ_SMALL_RANGE)
		return;
	physical = vkPublicToPhysical(address);
	if (WHOLE_LINES(physical, size, 0x100, start, end))
		vkL2LineOperation(start, end - start, VK_L2_SYNC);
}

/* address windows */
uint32_t vnzUncachedToPhysical(uint32_t address)
{
	return vkUncachedToPhysical(address);
}

uint32_t vnzPhysicalToUncached(uint32_t address)
{
	return vkPhysicalToUncached(address);
}

uint32_t vnzDramAddress(uint32_t address)
{
	return vkDramAddress(address);
}

uint32_t vnzToPhysical(uint32_t address)
{
	return vkToPhysical(address);
}

uint32_t vnzToPublic(uint32_t address)
{
	return vkToPublic(address);
}

uint32_t vnzToUncached(uint32_t address)
{
	return vkToUncached(address);
}

uint32_t vnzCurrentCoreId(void)
{
	return vkCurrentCoreId();
}

/* locks */
volatile uint8_t *vnzLockAllocate(void)
{
	return vkLockPageAllocate();
}

void vnzLockFree(volatile uint8_t *lock)
{
	vkLockPageFree(lock);
}

/* interrupts: levels below 16, edge or level triggered */
VkInterruptHandler *vnzInterruptRegister(uint32_t interrupt, int32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t, uint32_t), uint32_t argument, uint32_t argument2)
{
	if (level < 0 || level >= 0x10 || (edge != 0 && edge != 1))
		return NULL;
	return vkInterruptRegister(interrupt, level, edge, handler, argument, argument2);
}

void vnzInterruptRelease(VkInterruptHandler *handler)
{
	vkInterruptRelease(handler);
}

int32_t vnzInterruptEnable(VkInterruptHandler *handler)
{
	return vkInterruptEnable(handler);
}

int32_t vnzInterruptDisable(VkInterruptHandler *handler)
{
	return vkInterruptDisable(handler);
}

/* crashes */
int32_t vnzCrashSetHook(VkCrashHook hook, uint32_t argument, uint32_t argument2)
{
	return vkCrashSetHook(hook, argument, argument2);
}

int32_t vnzCrashClearHook(void)
{
	return vkCrashClearHook();
}

int32_t vnzCrashCopyRegister(uint32_t id, const void *source, uint32_t size)
{
	return vkCrashCopyRegister(id, source, size);
}

/* tracing, into a memory region (id) */
int32_t vnzTraceSetup(uint32_t id, uint32_t mode)
{
	uint32_t base, size;

	if (mode >= 2)
		return VNZ_ERROR;
	base = vkMemoryRegion(id, &size);
	if (base == 0)
		return VNZ_ERROR;
	return vkTraceSetup((void *)vkPhysicalToPublic(base), size, mode);
}

int32_t vnzTraceSetShare(uint32_t coreId, uint32_t size)
{
	return vkTraceSetShare(coreId, size);
}

int32_t vnzTraceStart(uint32_t mask)
{
	return vkTraceStart(mask);
}

int32_t vnzTraceStop(void)
{
	return vkTraceStop();
}

/* Stop using the buffer, written back for the reader. */
void vnzTraceRelease(void)
{
	uint32_t size;
	void *buffer = vkTraceBuffer(&size);

	vkTraceRelease();
	l2Range(vkPublicToPhysical((uint32_t)buffer), size, VK_L2_WRITEBACK);
}

/* power */
uint32_t vnzPowerSetCores(uint32_t expected, uint32_t cores)
{
	return vkPowerSetCores(expected, cores);
}

VkPowerHook *vnzPowerNotifyCreate(VkThread *thread, uint32_t flags)
{
	return vkPowerNotifyCreate(thread, flags);
}

void vnzPowerNotifyDelete(VkPowerHook *hook)
{
	vkPowerNotifyDelete(hook);
}

void vnzPowerNotifyRegister(VkPowerHook *hook)
{
	vkPowerNotifyRegister(hook);
}

void vnzPowerNotifyUnregister(VkPowerHook *hook)
{
	vkPowerNotifyUnregister(hook);
}

uint32_t vnzPowerActiveCores(void)
{
	return vkPowerActiveCores();
}

VkPowerHook *vnzPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument)
{
	return vkPowerCallbackCreate(function, argument);
}

void vnzPowerCallbackDelete(VkPowerHook *hook)
{
	vkPowerCallbackDelete(hook);
}

void vnzPowerCallbackRegister(VkPowerHook *hook)
{
	vkPowerCallbackRegister(hook);
}

void vnzPowerCallbackUnregister(VkPowerHook *hook)
{
	vkPowerCallbackUnregister(hook);
}
