/*
 * ScebXCe's allocator: what the library's memory comes from. The application can give a
 * MemAllocator (ScebXCe_EB4E6C8D); without one the library makes its own, an mspace in a
 * "SceBxceMemBlock" memory block. The library's reference count makes the heap on the first
 * interface object and deletes it, and the allocator it made, with the last.
 */
#include "bxce_internal.h"

namespace bXCe {

MemAllocator *g_bxceDefaultAllocator;
MemAllocator *g_bxceAllocator;

} /* namespace bXCe */

using bXCe::MemAllocator;
using bXCe::DefaultMemAllocator;
using bXCe::g_bxceDefaultAllocator;
using bXCe::g_bxceAllocator;

void *g_bxceAllocatorUserData;
SceUID g_bxceMemBlock;
void *g_bxceMspace;
SceUInt8 g_bxceAllocatorByApplication;
int g_bxceInitializeCount;
SceUInt8 g_bxceHeapByApplication;
int g_bxceInitializeResult;

/* a memory block of the size rounded up to the next page (a whole page more when it is one) */
int bxceMemBlockCreate(SceSize size, SceUID *memblock, void **mspace)
{
	SceSize blockSize = ((size >> 12) + 1) << 12;
	int ret = SCE_BXCE_ERROR_NO_MEMORY;
	void *base;

	*memblock = sceKernelAllocMemBlock("SceBxceMemBlock", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					   blockSize, NULL);
	if (*memblock >= 0) {
		*mspace = NULL;
		base = NULL;
		if (sceKernelGetMemBlockBase(*memblock, &base) >= 0) {
			*mspace = sceClibMspaceCreate(base, blockSize);
			if (*mspace != NULL) {
				ret = 0;
				goto out;
			}
		}
	}

	if (*mspace != NULL) {
		sceClibMspaceDestroy(*mspace);
		*mspace = NULL;
	}
	if (*memblock != 0) {
		sceKernelFreeMemBlock(*memblock);
		*memblock = 0;
	}
out:
	return ret;
}

void *bxceMspaceMalloc(SceSize size)
{
	return sceClibMspaceMalloc(g_bxceMspace, size);
}

/* not while the application's allocator is set */
void bxceMspaceDestroy(void)
{
	if (g_bxceAllocatorByApplication)
		return;

	if (g_bxceMspace != NULL) {
		sceClibMspaceDestroy(g_bxceMspace);
		g_bxceMspace = NULL;
	}
	if (g_bxceMemBlock != 0) {
		sceKernelFreeMemBlock(g_bxceMemBlock);
		g_bxceMemBlock = 0;
	}
}

/* the library's own allocator, made the first time */
static inline MemAllocator *bxceGetDefaultAllocator(void)
{
	if (g_bxceDefaultAllocator == NULL) {
		DefaultMemAllocator *allocator = new DefaultMemAllocator();

		g_bxceDefaultAllocator = allocator;
		if (allocator != NULL)
			bxceMemBlockCreate(BXCE_MEMBLOCK_DEFAULT_SIZE, &allocator->memblock,
					   &allocator->mspace);
	}
	return g_bxceDefaultAllocator;
}

/* sets the allocator, or without one the library's own */
__attribute__((noinline)) int ScebXCe_EB4E6C8D(MemAllocator *allocator, void *userData)
{
	MemAllocator *current = g_bxceDefaultAllocator;

	if (allocator == NULL) {
		current = bxceGetDefaultAllocator();
		userData = NULL;
	} else {
		if (allocator != current)
			g_bxceAllocatorByApplication = 1;
		current = allocator;
	}

	g_bxceAllocator = current;
	g_bxceAllocatorUserData = userData;
	if (allocator == NULL)
		g_bxceAllocatorByApplication = 0;
	return 0;
}

void *bxceDefaultAllocate(SceSize size)
{
	if (g_bxceAllocator == NULL && ScebXCe_EB4E6C8D(bxceGetDefaultAllocator(), NULL) != 0)
		return NULL;
	return g_bxceAllocator->allocate(size, g_bxceAllocatorUserData);
}

void bxceDefaultDeallocate(void *ptr)
{
	if (g_bxceAllocator == NULL && ScebXCe_EB4E6C8D(bxceGetDefaultAllocator(), NULL) != 0)
		return;
	g_bxceAllocator->deallocate(ptr, g_bxceAllocatorUserData);
}

__attribute__((noinline)) void bxceDeleteDefaultAllocator(void)
{
	MemAllocator *allocator = g_bxceDefaultAllocator;

	if (g_bxceAllocator == allocator)
		g_bxceAllocator = NULL;
	if (allocator != NULL) {
		delete allocator;
		g_bxceDefaultAllocator = NULL;
	}
}

/* the first reference makes the heap; the result is kept */
int bxceLibraryAddRef(void)
{
	int ret = 0;

	if (g_bxceInitializeCount == 0)
		ret = bxceHeapCreate(BXCE_HEAP_DEFAULT_SIZE, NULL);
	g_bxceInitializeCount++;
	g_bxceInitializeResult = bxceConvertResult(ret);
	return g_bxceInitializeResult;
}

/* the last reference deletes the heap and the library's allocator */
void bxceLibraryRelease(void)
{
	int count = g_bxceInitializeCount;

	if (count > 0)
		g_bxceInitializeCount = --count;
	if (count == 0) {
		bxceHeapDelete();
		bxceDeleteDefaultAllocator();
	}
}
