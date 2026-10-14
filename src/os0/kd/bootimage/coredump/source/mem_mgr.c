/* Coredump persistent backing blocks and per-dump heaps. */

#include <psp2kern/types.h>
#include <stddef.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/debug.h>

typedef struct CoredumpLargeAllocation {
	SceUID blockId;
	void *base;
} CoredumpLargeAllocation;

_Static_assert(sizeof(CoredumpLargeAllocation) == 8,
	"large allocation record size");

SceUID localHeapId;
SceUID dumpHeapId;
CoredumpLargeAllocation largeAllocations[2];
SceSize localHeapInitialFreeSize;
SceUID localHeapBlockId;
SceSize dumpHeapInitialFreeSize;
SceUID dumpHeapBlockId;

extern int oalMemBlockInit(SceUID *blockId, SceSize size);
extern int oalMemBlockFinal(SceUID blockId);
extern int oalHeapInit(SceUID blockId, SceUID *heapId, SceSize maxSize);
extern int oalHeapFinal(SceUID heapId);
extern SceSize oalHeapGetFreeSize(SceUID heapId);
extern void *oalHeapAlloc(SceUID heapId, SceSize size);
extern int oalIsToolDipswEnabled(void);
extern int oalMemBlockInitForDevTool(SceUID *blockId, SceSize size);
extern int oalHeapFree(SceUID heapId, void *allocation);

int memMgrBootstrap(void)
{
	int result;

	result = oalMemBlockInit(&dumpHeapBlockId, 0x8000);
	if (result == 0) {
		largeAllocations[0].blockId = -1;
		largeAllocations[1].blockId = -1;
		largeAllocations[0].base = NULL;
		largeAllocations[1].base = NULL;
		result = oalMemBlockInit(&localHeapBlockId, 0x8000);
		if (result != 0) {
			oalMemBlockFinal(dumpHeapBlockId);
			return result;
		}
	}
	return result;
}

int memMgrTerminate(void)
{
	int result, finalResult;

	result = oalMemBlockFinal(localHeapBlockId);
	finalResult = oalMemBlockFinal(dumpHeapBlockId);
	if (finalResult != 0)
		result = finalResult;
	if (largeAllocations[0].blockId != -1) {
		finalResult = oalMemBlockFinal(largeAllocations[0].blockId);
		if (finalResult != 0)
			result = finalResult;
	}
	if (largeAllocations[1].blockId != -1) {
		finalResult = oalMemBlockFinal(largeAllocations[1].blockId);
		if (finalResult != 0)
			result = finalResult;
	}
	return result;
}

int memMgrInit(void)
{
	int result;

	result = oalHeapInit(dumpHeapBlockId, &dumpHeapId, 0x8000);
	if (result == 0) {
		result = oalHeapInit(localHeapBlockId, &localHeapId, 0x8000);
		if (result != 0) {
			oalHeapFinal(dumpHeapId);
			return result;
		}
		dumpHeapInitialFreeSize = oalHeapGetFreeSize(dumpHeapId);
		localHeapInitialFreeSize = oalHeapGetFreeSize(localHeapId);
	}
	return result;
}

int memMgrFinal(void)
{
	int result, dumpResult;

	oalHeapGetFreeSize(dumpHeapId);
	oalHeapGetFreeSize(localHeapId);
	result = oalHeapFinal(localHeapId);
	dumpResult = oalHeapFinal(dumpHeapId);
	return dumpResult == 0 ? result : dumpResult;
}

void *memMgrAlloc(SceSize size)
{
	CoredumpLargeAllocation *allocation;
	unsigned int index;
	int result;

	if ((size & 0xfff) != 0)
		return oalHeapAlloc(dumpHeapId, size);
	if (largeAllocations[0].blockId == -1) {
		index = 0;
	} else {
		if (largeAllocations[1].blockId != -1)
			return NULL;
		index = 1;
	}
	allocation = &largeAllocations[index];
	if (oalIsToolDipswEnabled() != 0)
		result = oalMemBlockInitForDevTool(&allocation->blockId, size);
	else
		result = oalMemBlockInit(&allocation->blockId, size);
	if (result != 0)
		return NULL;
	if (ksceKernelGetMemBlockBase(allocation->blockId, &allocation->base) == 0)
		return allocation->base;
	oalMemBlockFinal(largeAllocations[index].blockId);
	largeAllocations[index].blockId = -1;
	allocation->base = NULL;
	return NULL;
}

int memMgrFreeForWriter(void *allocation)
{
	unsigned int index;
	int result;

	SCE_DBG_ASSERT(allocation != NULL, 234);
	if (largeAllocations[0].base == allocation) {
		index = 0;
	} else {
		if (largeAllocations[1].base != allocation)
			return oalHeapFree(dumpHeapId, allocation);
		index = 1;
	}
	result = oalMemBlockFinal(largeAllocations[index].blockId);
	largeAllocations[index].blockId = -1;
	largeAllocations[index].base = NULL;
	return result;
}

void *memMgrLocalAlloc(SceSize size)
{
	return oalHeapAlloc(localHeapId, size);
}

void *memMgrLocalAllAlloc(SceSize *size)
{
	SceSize freeSize;
	void *allocation;

	SCE_DBG_ASSERT(size != NULL, 269);
	freeSize = oalHeapGetFreeSize(localHeapId);
	allocation = oalHeapAlloc(localHeapId, freeSize);
	if (allocation != NULL)
		*size = freeSize;
	return allocation;
}

int memMgrLocalFree(void *allocation)
{
	SCE_DBG_ASSERT(allocation != NULL, 287);
	return oalHeapFree(localHeapId, allocation);
}

SceSize memMgrLocalGetFreeSize(void)
{
	return oalHeapGetFreeSize(localHeapId);
}
