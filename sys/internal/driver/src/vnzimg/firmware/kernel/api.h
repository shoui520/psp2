/*
 * The kernel's interface for the drivers and the application (kernel/api.c).
 */
#pragma once

#include "kernel.h"

#define VNZ_ERROR		(-1)

int32_t vnzThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize);
int32_t vnzThreadCreateOnHeap(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VnzKernelHeap *heap);
int32_t vnzThreadStart(VnzKernelThread *thread, uint32_t argument, uint32_t argument2);
void vnzThreadExit(void);
void vnzThreadExitWithStatus(uint32_t status, uint32_t status2);
int32_t vnzThreadDelete(VnzKernelThread *thread);
void vnzThreadYield(void);
int32_t vnzThreadSetPriority(VnzKernelThread *thread, uint32_t priority);
VnzKernelThread *vnzThreadCurrent(void);
uint32_t vnzThreadPriority(VnzKernelThread *thread);
void vnzThreadInformation(VnzKernelThread *thread, VnzKernelThreadInformation *information);
void vnzDisableDispatch(void);
void vnzEnableDispatch(void);
uint32_t vnzThreadWaitChildren(void);
uint32_t vnzThreadJoining(void);
void vnzThreadDetach(VnzKernelThread *thread);
uint32_t vnzThreadIsAlive(VnzKernelThread *thread);
uint32_t vnzThreadHasExited(VnzKernelThread *thread);
int32_t vnzThreadNotifyExit(VnzKernelThread *thread, uint32_t flags);
int32_t vnzThreadCancelExitNotify(VnzKernelThread *thread);
int32_t vnzThreadExitStatus(VnzKernelThread *thread, uint32_t *status, uint32_t *status2);
uint32_t vnzExitedThreads(void);
int32_t vnzThreadSetFlags(VnzKernelThread *thread, uint32_t flags);
uint32_t vnzThreadWaitFlags(uint32_t flags);
uint32_t vnzThreadClearFlags(uint32_t flags);
uint32_t *vnzThreadLocal0(void);
uint32_t *vnzThreadLocal1(void);
uint32_t *vnzThreadLocal2(void);
int32_t vnzThreadMove(VnzKernelThread *thread, uint32_t coreId);
int32_t vnzThreadMoveNotify(VnzKernelThread *thread, uint32_t coreId, VnzKernelThread *notify,
	uint32_t flags);
uint32_t vnzThreadAttributes(VnzKernelThread *thread);
uint32_t vnzMemoryRegion(uint32_t id, uint32_t *size);
VnzKernelHeap *vnzHeapCreatePool(uint32_t base, uint32_t size);
void vnzHeapDeletePool(VnzKernelHeap *heap);
VnzKernelHeap *vnzHeapFind(uint32_t address, uint32_t size);
VnzKernelHeap *vnzHeapCreate(uint32_t base, uint32_t size, VnzKernelHeapCreate create,
	VnzKernelHeapDelete delete,
	VnzKernelHeapAllocate allocate, VnzKernelHeapFree free);
void vnzHeapDelete(VnzKernelHeap *heap);
void *vnzHeapContext(VnzKernelHeap *heap);
void *vnzHeapAllocateStack(VnzKernelHeap *heap, uint32_t size);
void *vnzHeapAllocatePublic(VnzKernelHeap *heap, uint32_t size);
void *vnzHeapAllocateUncached(VnzKernelHeap *heap, uint32_t size);
void *vnzHeapAllocate(VnzKernelHeap *heap, uint32_t size, uint32_t alignment);
void vnzHeapFreeStack(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzHeapFreePublic(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzHeapFreeUncached(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzHeapFree(VnzKernelHeap *heap, void *memory, uint32_t size, uint32_t alignment);
VnzKernelHeap *vnzSystemHeap(void);
VnzKernelHeap *vnzCurrentHeap(void);
int32_t vnzPoolHeapCreate(void *context, uint32_t base, uint32_t size);
void vnzPoolHeapDelete(void *context, uint32_t base, uint32_t size);
void *vnzPoolHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window);
void vnzPoolHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment, int32_t window);
void vnzCachePurge(uint32_t address, uint32_t size);
void vnzCachePurgeAllCores(uint32_t address, uint32_t size);
void vnzCacheFlush(uint32_t address, uint32_t size);
void vnzCacheWriteback(uint32_t address, uint32_t size);
void vnzCachePrefetch(uint32_t address, uint32_t size);
void vnzCacheInvalidate(uint32_t address, uint32_t size);
void vnzL2PurgeAllCores(uint32_t address, uint32_t size);
void vnzL2InvalidateTags(uint32_t address, uint32_t size);
void vnzL2Invalidate(uint32_t address, uint32_t size);
void vnzL2Writeback(uint32_t address, uint32_t size);
void vnzL2Purge(uint32_t address, uint32_t size);
void vnzL2Sync(uint32_t address, uint32_t size);
void vnzL2Flush(uint32_t address, uint32_t size);
void vnzL2SetMode(uint32_t mode);
void vnzL2Disable(void);
void vnzL2Enable(void);
uint32_t vnzSpinTry(volatile uint8_t *lock);
void vnzSpinUnlock(volatile uint8_t *lock);
void vnzSpinLock(volatile uint8_t *lock);
uint32_t vnzMemoryEnter(uint32_t address, uint32_t size);
uint32_t vnzMemoryLeave(uint32_t address, uint32_t size);
void vnzMemoryResume(void);
void vnzMemorySuspend(void);
uint32_t vnzMemoryPhysicalToStack(uint32_t address, uint32_t size);
uint32_t vnzMemoryPhysicalToPublic(uint32_t address, uint32_t size);
uint32_t vnzMemoryPhysicalToUncached(uint32_t address, uint32_t size);
uint32_t vnzMemoryStackToPhysical(uint32_t address, uint32_t size);
uint32_t vnzMemoryPublicToPhysical(uint32_t address, uint32_t size);
uint32_t vnzMemoryUncachedToPhysical(uint32_t address, uint32_t size);
void vnzMemorySync(uint32_t address, uint32_t size, VnzKernelThread *owner);
void *vnzAllocateStack(uint32_t size);
void vnzFreeStack(void *memory, uint32_t size);
void *vnzKernelAllocatePublic(uint32_t size);
void vnzKernelFreePublic(void *memory, uint32_t size);
void *vnzAllocateUncached(uint32_t size);
void vnzFreeUncached(void *memory, uint32_t size);
uint32_t vnzPublicOpenPrivate(uint32_t address, uint32_t size);
uint32_t vnzPublicClosePrivate(uint32_t address, uint32_t size);
uint32_t vnzPublicOpenProtected(uint32_t address, uint32_t size);
uint32_t vnzPublicCloseProtected(uint32_t address, uint32_t size);
uint32_t vnzUncachedOpenPublic(uint32_t address, uint32_t size);
uint32_t vnzUncachedClosePublic(uint32_t address, uint32_t size);
uint32_t vnzUncachedOpenRaw(uint32_t address, uint32_t size);
uint32_t vnzUncachedCloseRaw(uint32_t address, uint32_t size);
uint32_t vnzPublicOpenRaw(uint32_t address, uint32_t size);
uint32_t vnzPublicCloseRaw(uint32_t address, uint32_t size);
void vnzCacheInvalidateInside(uint32_t address, uint32_t size);
void vnzCachePrefetchInside(uint32_t address, uint32_t size);
void vnzL2FlushInside(uint32_t address, uint32_t size);
void vnzL2SyncInside(uint32_t address, uint32_t size);
void vnzCachePrefetchInsideCopy1(uint32_t address, uint32_t size);
void vnzCachePrefetchInsideCopy2(uint32_t address, uint32_t size);
void vnzCachePrefetchInsideCopy3(uint32_t address, uint32_t size);
uint32_t vnzUncachedToPhysical(uint32_t address);
uint32_t vnzPhysicalToUncached(uint32_t address);
uint32_t vnzDramAddress(uint32_t address);
uint32_t vnzDramAddressCopy(uint32_t address);
uint32_t vnzToPhysical(uint32_t address);
uint32_t vnzToPublic(uint32_t address);
uint32_t vnzToUncached(uint32_t address);
uint32_t vnzCurrentCoreId(void);
volatile uint8_t *vnzLockAllocate(void);
void vnzLockFree(volatile uint8_t *lock);
VnzKernelInterruptHandler *vnzInterruptRegister(uint32_t interrupt, int32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t, uint32_t), uint32_t argument, uint32_t argument2);
void vnzInterruptRelease(VnzKernelInterruptHandler *handler);
int32_t vnzInterruptEnable(VnzKernelInterruptHandler *handler);
int32_t vnzInterruptDisable(VnzKernelInterruptHandler *handler);
int32_t vnzCrashSetHook(VnzKernelCrashHook hook, uint32_t argument, uint32_t argument2);
int32_t vnzCrashClearHook(void);
int32_t vnzCrashCopyRegister(uint32_t id, const void *source, uint32_t size);
int32_t vnzTraceSetup(uint32_t id, uint32_t mode);
int32_t vnzTraceSetShare(uint32_t coreId, uint32_t size);
int32_t vnzTraceStart(uint32_t mask);
int32_t vnzTraceStop(void);
void vnzTraceRelease(void);
uint32_t vnzPowerSetCores(uint32_t expected, uint32_t cores);
VnzKernelPowerHook *vnzPowerNotifyCreate(VnzKernelThread *thread, uint32_t flags);
void vnzPowerNotifyDelete(VnzKernelPowerHook *hook);
void vnzPowerNotifyRegister(VnzKernelPowerHook *hook);
void vnzPowerNotifyUnregister(VnzKernelPowerHook *hook);
uint32_t vnzPowerActiveCores(void);
VnzKernelPowerHook *vnzPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument);
void vnzPowerCallbackDelete(VnzKernelPowerHook *hook);
void vnzPowerCallbackRegister(VnzKernelPowerHook *hook);
void vnzPowerCallbackUnregister(VnzKernelPowerHook *hook);
