/*
 * The kernel's interface for the drivers and the application (kernel/api.c).
 */
#pragma once

#include "kernel.h"

#define VNZ_ERROR		(-1)

int32_t vnzThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize);
int32_t vnzThreadCreateOnHeap(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VkHeap *heap);
int32_t vnzThreadStart(VkThread *thread, uint32_t argument, uint32_t argument2);
void vnzThreadExit(void);
void vnzThreadExitWithStatus(uint32_t status, uint32_t status2);
int32_t vnzThreadDelete(VkThread *thread);
void vnzThreadYield(void);
int32_t vnzThreadSetPriority(VkThread *thread, uint32_t priority);
VkThread *vnzThreadCurrent(void);
uint32_t vnzThreadPriority(VkThread *thread);
void vnzDisableDispatch(void);
void vnzEnableDispatch(void);
uint32_t vnzThreadWaitChildren(void);
uint32_t vnzThreadIsAlive(VkThread *thread);
uint32_t vnzThreadHasExited(VkThread *thread);
int32_t vnzThreadNotifyExit(VkThread *thread, uint32_t flags);
int32_t vnzThreadCancelExitNotify(VkThread *thread);
int32_t vnzThreadExitStatus(VkThread *thread, uint32_t *status, uint32_t *status2);
uint32_t vnzExitedThreads(void);
int32_t vnzThreadSetFlags(VkThread *thread, uint32_t flags);
uint32_t vnzThreadWaitFlags(uint32_t flags);
uint32_t vnzThreadClearFlags(uint32_t flags);
uint32_t *vnzThreadLocal0(void);
uint32_t *vnzThreadLocal1(void);
uint32_t *vnzThreadLocal2(void);
int32_t vnzThreadMove(VkThread *thread, uint32_t coreId);
int32_t vnzThreadMoveNotify(VkThread *thread, uint32_t coreId, VkThread *notify, uint32_t flags);
uint32_t vnzThreadAttributes(VkThread *thread);
uint32_t vnzMemoryRegion(uint32_t id, uint32_t *size);
VkHeap *vnzHeapCreatePool(uint32_t base, uint32_t size);
void vnzHeapDelete(VkHeap *heap);
VkHeap *vnzHeapFind(uint32_t address, uint32_t size);
VkHeap *vnzHeapCreate(uint32_t base, uint32_t size, VkHeapCreate create, VkHeapDelete delete,
	VkHeapAllocate allocate, VkHeapFree free);
void *vnzHeapAllocateStack(VkHeap *heap, uint32_t size);
void *vnzHeapAllocatePublic(VkHeap *heap, uint32_t size);
void *vnzHeapAllocateUncached(VkHeap *heap, uint32_t size);
void *vnzHeapAllocate(VkHeap *heap, uint32_t size, uint32_t alignment);
void vnzHeapFreeStack(VkHeap *heap, void *memory, uint32_t size);
void vnzHeapFreePublic(VkHeap *heap, void *memory, uint32_t size);
void vnzHeapFreeUncached(VkHeap *heap, void *memory, uint32_t size);
void vnzHeapFree(VkHeap *heap, void *memory, uint32_t size, uint32_t alignment);
VkHeap *vnzSystemHeap(void);
VkHeap *vnzCurrentHeap(void);
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
void vnzMemorySync(void);
void *vnzAllocateStack(uint32_t size);
void vnzFreeStack(void *memory, uint32_t size);
void *vkAllocatePublic(uint32_t size);
void vkFreePublic(void *memory, uint32_t size);
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
uint32_t vnzUncachedToPhysical(uint32_t address);
uint32_t vnzPhysicalToUncached(uint32_t address);
uint32_t vnzDramAddress(uint32_t address);
uint32_t vnzToPhysical(uint32_t address);
uint32_t vnzToPublic(uint32_t address);
uint32_t vnzToUncached(uint32_t address);
uint32_t vnzCurrentCoreId(void);
volatile uint8_t *vnzLockAllocate(void);
void vnzLockFree(volatile uint8_t *lock);
VkInterruptHandler *vnzInterruptRegister(uint32_t interrupt, int32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t, uint32_t), uint32_t argument, uint32_t argument2);
void vnzInterruptRelease(VkInterruptHandler *handler);
int32_t vnzInterruptEnable(VkInterruptHandler *handler);
int32_t vnzInterruptDisable(VkInterruptHandler *handler);
int32_t vnzCrashSetHook(VkCrashHook hook, uint32_t argument, uint32_t argument2);
int32_t vnzCrashClearHook(void);
int32_t vnzCrashCopyRegister(uint32_t id, const void *source, uint32_t size);
int32_t vnzTraceSetup(uint32_t id, uint32_t mode);
int32_t vnzTraceSetShare(uint32_t coreId, uint32_t size);
int32_t vnzTraceStart(uint32_t mask);
int32_t vnzTraceStop(void);
void vnzTraceRelease(void);
uint32_t vnzPowerSetCores(uint32_t expected, uint32_t cores);
VkPowerHook *vnzPowerNotifyCreate(VkThread *thread, uint32_t flags);
void vnzPowerNotifyDelete(VkPowerHook *hook);
void vnzPowerNotifyRegister(VkPowerHook *hook);
void vnzPowerNotifyUnregister(VkPowerHook *hook);
uint32_t vnzPowerActiveCores(void);
VkPowerHook *vnzPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument);
void vnzPowerCallbackDelete(VkPowerHook *hook);
void vnzPowerCallbackRegister(VkPowerHook *hook);
void vnzPowerCallbackUnregister(VkPowerHook *hook);
