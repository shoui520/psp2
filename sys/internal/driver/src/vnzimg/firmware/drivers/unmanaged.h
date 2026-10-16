/*
 * Unmanaged memory (drivers/unmanaged.c): the memory protocol's operations for
 * memory outside the kernel's windows.
 */
#pragma once

#include "api.h"

uint32_t vkUnmanagedEnter(uint32_t address, uint32_t size);
uint32_t vkUnmanagedLeave(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPhysicalToStack(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPhysicalToPublic(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPhysicalToUncached(uint32_t address, uint32_t size);
uint32_t vkUnmanagedStackToPhysical(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPublicToPhysical(uint32_t address, uint32_t size);
uint32_t vkUnmanagedUncachedToPhysical(uint32_t address, uint32_t size);
uint32_t vkUnmanagedOpenPrivate(uint32_t address, uint32_t size);
uint32_t vkUnmanagedClosePrivate(uint32_t address, uint32_t size);
uint32_t vkUnmanagedOpenProtected(uint32_t address, uint32_t size);
uint32_t vkUnmanagedCloseProtected(uint32_t address, uint32_t size);
uint32_t vkUnmanagedOpenPublic(uint32_t address, uint32_t size);
uint32_t vkUnmanagedClosePublic(uint32_t address, uint32_t size);
uint32_t vkUnmanagedUncachedOpenRaw(uint32_t address, uint32_t size);
uint32_t vkUnmanagedUncachedCloseRaw(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPublicOpenRaw(uint32_t address, uint32_t size);
uint32_t vkUnmanagedPublicCloseRaw(uint32_t address, uint32_t size);
uint32_t vkUnmanagedAllocateStack(VkHeap *heap, uint32_t size);
uint32_t vkUnmanagedAllocatePublic(VkHeap *heap, uint32_t size);
uint32_t vkUnmanagedAllocateUncached(VkHeap *heap, uint32_t size);
void vkUnmanagedFreeStack(VkHeap *heap, uint32_t address, uint32_t size);
void vkUnmanagedFreePublic(VkHeap *heap, uint32_t address, uint32_t size);
void vkUnmanagedFreeUncached(VkHeap *heap, uint32_t address, uint32_t size);
void vkUnmanagedInvalidateInside(uint32_t address, uint32_t size);
void vkUnmanagedPrefetchInside(uint32_t address, uint32_t size);
void vkUnmanagedL2FlushInside(uint32_t address, uint32_t size);
void vkUnmanagedL2SyncInside(uint32_t address, uint32_t size);
