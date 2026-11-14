/*
 * Unmanaged memory (drivers/unmanaged.c): the memory protocol's operations for
 * memory outside the kernel's windows.
 */
#pragma once

#include "api.h"

uint32_t vnzKernelUnmanagedEnter(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedLeave(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPhysicalToStack(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPhysicalToPublic(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPhysicalToUncached(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedStackToPhysical(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPublicToPhysical(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedUncachedToPhysical(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedOpenPrivate(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedClosePrivate(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedOpenProtected(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedCloseProtected(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedOpenPublic(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedClosePublic(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedUncachedOpenRaw(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedUncachedCloseRaw(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPublicOpenRaw(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedPublicCloseRaw(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedAllocateStack(VnzKernelHeap *heap, uint32_t size);
uint32_t vnzKernelUnmanagedAllocatePublic(VnzKernelHeap *heap, uint32_t size);
uint32_t vnzKernelUnmanagedAllocateUncached(VnzKernelHeap *heap, uint32_t size);
void vnzKernelUnmanagedFreeStack(VnzKernelHeap *heap, uint32_t address, uint32_t size);
void vnzKernelUnmanagedFreePublic(VnzKernelHeap *heap, uint32_t address, uint32_t size);
void vnzKernelUnmanagedFreeUncached(VnzKernelHeap *heap, uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedSameAddress1(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedSameAddress2(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedSameAddress3(uint32_t address, uint32_t size);
uint32_t vnzKernelUnmanagedSameAddress4(uint32_t address, uint32_t size);
void vnzKernelUnmanagedInvalidateInside(uint32_t address, uint32_t size);
void vnzKernelUnmanagedPrefetchInside(uint32_t address, uint32_t size);
void vnzKernelUnmanagedL2FlushInside(uint32_t address, uint32_t size);
void vnzKernelUnmanagedL2SyncInside(uint32_t address, uint32_t size);
void vnzKernelUnmanagedPrefetchInsideCopy1(uint32_t address, uint32_t size);
void vnzKernelUnmanagedPrefetchInsideCopy2(uint32_t address, uint32_t size);
void vnzKernelUnmanagedPrefetchInsideCopy3(uint32_t address, uint32_t size);
void vnzKernelUnmanagedNothing1(void);
void vnzKernelUnmanagedNothing2(void);
