/* Native memblock ownership, shadow mappings and backing-heap lists. */
#include "gpu-blockheap.h"
#include "sce/sysmem.h"
#include "sce/debug.h"
#include <psp2kern/kernel/sysmem/uid_puid.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/blockmgr.c"

extern void *AllocProcessGpuMemory(unsigned size);
extern int FreeProcessGpuMemory(void *address);
extern void *memset(void *destination, int value, unsigned size);
extern unsigned ksceKernelMemBlockTypeGetPrivileges(unsigned type);
extern unsigned SceSysmemForDriver_CB0F3A33(unsigned type);
extern int ksceKernelIncRefCountMemBlock(int uid);
extern int ksceKernelDecRefCountMemBlock(int uid);
extern int ksceSblACMgrIsGameProgram(int pid);
extern GpuMemBlockHeap *kernelMemBlockHeap;

unsigned nextMemBlockGroupId = 0x12340000;

GpuMemBlockHeap *GetKernelMemBlockHeap(void)
{
	return kernelMemBlockHeap;
}

GpuMemBlock *sceGpuMemBlockNewBlock(int uid, unsigned *groupId,
				 unsigned restoreMapping)
{
	volatile MemBlockPhyInfo info;
	volatile unsigned *group = groupId;
	volatile GpuMemBlock *block;
	GpuMemBlockRange *range;
	SceKernelAllocMemBlockKernelOpt options;
	unsigned kernel, shadowType, type, base, alternate, size;
	unsigned attributes, value, next;
	void *mappedBase = 0;
	int referencedUid, mappingUid, ret;

	kernel = *group == 1;
	referencedUid = kernel ? uid : kscePUIDtoGUID(0, uid);
	if (referencedUid <= 0) {
		SCE_DBG_PRINTF(0, 75, "%s: Failed to get GUID for MemBlock %08x\n",
			       __func__, uid);
		return 0;
	}
	memset((void *)&info, 0, sizeof(info));
	info.ex.size = sizeof(info);
	ret = ksceKernelMemBlockGetInfoEx(referencedUid, (MemBlockPhyInfo *)&info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 84, "Failed to get memblock info for %08x: %08x\n",
			       referencedUid, ret);
		return 0;
	}
	if (ksceKernelMemBlockTypeGetPrivileges(info.ex.details.type) & 0x44)
		return 0;
	mappingUid = uid;
	if (!kernel) {
		type = info.ex.details.type;
		if (type == 0x09408060) {
			shadowType = 0x40408006;
		} else if (type == 0x0d808060 || type == 0x0e208050 ||
			   type == 0x0e208060 || type == 0x0c208060) {
			shadowType = ksceSblACMgrIsGameProgram(0) ?
				     0x11208006 : 0x10208006;
		} else {
			SCE_DBG_PRINTF(0, 113,
				       "%s failed with invalid memblock type %08x.\n",
				       __func__, type);
			return 0;
		}
		if (restoreMapping) {
			ret = ksceKernelUnmapMemBlock(referencedUid);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 120,
					"Failed to unmap memblock %08x:%08x from usermode with error %08x.\n",
					uid, referencedUid, ret);
				return 0;
			}
		}
		memset(&options, 0, sizeof(options));
		options.size = sizeof(options);
		options.attr = 0x01000040;
		options.mirror_blockid = referencedUid;
		mappingUid = ksceKernelAllocMemBlock("SceGpuShadowMemBlock",
						    shadowType, 0, &options);
		if (mappingUid < 0) {
			SCE_DBG_PRINTF(0, 127,
				       "%s failed to kernel map memblock %08x with error %08x.\n",
				       __func__, uid, mappingUid);
			return 0;
		}
	}
	block = AllocProcessGpuMemory(sizeof(*block));
	if (!block)
		goto releaseMapping;
	memset((void *)block, 0, sizeof(*block));
	ksceKernelGetMemBlockBase(mappingUid, &mappedBase);
	base = info.ex.details.vbase;
	alternate = (unsigned)mappedBase;
	size = info.ex.details.size;
	range = AllocProcessGpuMemory(sizeof(*range));
	if (!range) {
		FreeProcessGpuMemory((void *)block);
		goto releaseMapping;
	}
	memset(range, 0, sizeof(*range));
	range->primaryAddress = base;
	range->alternateAddress = alternate;
	range->size = size;
	if (!kernel) {
		ksceKernelIncRefCountMemBlock(referencedUid);
		ksceKernelIncRefCountMemBlock(mappingUid);
	}
	block->inputUid = uid;
	block->referencedUid = kernel ? -1 : referencedUid;
	block->mappingUid = kernel ? -1 : mappingUid;
	block->restoreMapping = restoreMapping;
	block->ranges = range;
	attributes = SceSysmemForDriver_CB0F3A33(info.ex.details.type);
	attributes |= ksceKernelMemBlockType2Memtype(info.ex.details.type);
	block->next = 0;
	block->allocatedRangeCount = 0;
	block->attributes = attributes;
	block->primaryBase = info.ex.details.vbase;
	block->size = info.ex.details.size;
	value = *group;
	if (value) {
		block->groupId = value;
	} else {
		value = nextMemBlockGroupId;
		next = value + 1;
		block->groupId = value;
		nextMemBlockGroupId = next;
		*group = value;
		if ((int)next <= 0)
			nextMemBlockGroupId = 2;
	}
	return (GpuMemBlock *)block;

releaseMapping:
	if (restoreMapping)
		ksceKernelMapMemBlock(referencedUid);
	if (!kernel)
		ksceKernelFreeMemBlock(mappingUid);
	return 0;
}

int sceGpuMemBlockAddBlock(GpuMemBlockHeap *heap, int uid,
			  unsigned *groupId, unsigned restoreMapping)
{
	volatile GpuMemBlockHeap *blockHeap = heap;
	volatile GpuMemBlock *tail;
	GpuMemBlock *block, *next;

	if (!heap || !groupId)
		return (int)0x804c0000;
	tail = blockHeap->blocks;
	if (!tail) {
		block = sceGpuMemBlockNewBlock(uid, groupId, restoreMapping);
		if (!block)
			return (int)0x804c0001;
		block->next = 0;
		blockHeap->blocks = block;
		return 0;
	}
	for (;;) {
		if (tail->inputUid == (unsigned)uid)
			return (int)0x804c0004;
		next = tail->next;
		if (!next)
			break;
		tail = next;
	}
	SCE_DBG_ASSERT(1, 224);
	block = sceGpuMemBlockNewBlock(uid, groupId, restoreMapping);
	if (!block)
		return (int)0x804c0001;
	tail->next = block;
	return 0;
}

int sceGpuMemBlockDelBlock(GpuMemBlockHeap *heap, int uid, int *referencedOut)
{
	volatile GpuMemBlockHeap *blockHeap = heap;
	volatile GpuMemBlock *block, *previous = 0;
	volatile GpuMemBlockRange *range;
	GpuMemBlockRange *next;
	unsigned allocated;
	int referenced;

	if (!heap)
		return (int)0x804c0000;
	for (block = blockHeap->blocks; block; block = block->next) {
		if (block->inputUid != (unsigned)uid) {
			previous = block;
			continue;
		}
		range = block->ranges;
		if (!range)
			return (int)0x804c0005;
		allocated = range->allocated;
		if (allocated || range->next) {
			SCE_DBG_PRINTF(0, 259,
				"%s: Trying to delete a block that has a used heap (Used=%d Next=%p)\n",
				__func__, allocated, range->next);
			return (int)0x804c0006;
		}
		allocated = block->allocatedRangeCount;
		if ((int)allocated > 0) {
			SCE_DBG_PRINTF(0, 264,
				       "%s: Trying to delete a block that has sub handles (Count=%d)\n",
				       __func__, allocated);
			return (int)0x804c0006;
		}
		if (previous)
			previous->next = block->next;
		else
			blockHeap->blocks = block->next;
		for (range = block->ranges; range; range = next) {
			next = range->next;
			FreeProcessGpuMemory((void *)range);
		}
		block->ranges = 0;
		if (block->mappingUid != -1) {
			ksceKernelDecRefCountMemBlock(block->mappingUid);
			ksceKernelFreeMemBlock(block->mappingUid);
		}
		if (block->referencedUid != -1)
			ksceKernelDecRefCountMemBlock(block->referencedUid);
		if (referencedOut)
			*referencedOut = block->referencedUid;
		if (block->restoreMapping) {
			referenced = kscePUIDtoGUID(0, block->inputUid);
			if (referenced == block->referencedUid)
				ksceKernelMapMemBlock(referenced);
		}
		FreeProcessGpuMemory((void *)block);
		return 0;
	}
	return (int)0x804c0005;
}

/* Descriptive private name; unlike DelBlock, this does not restore mappings. */
int DestroyMemBlockHeap(GpuMemBlockHeap *heap)
{
	volatile GpuMemBlock *block;
	GpuMemBlock *nextBlock;
	GpuMemBlockRange *range, *nextRange;
	int uid;

	if (!heap)
		return (int)0x804c0000;
	for (block = heap->blocks; block; block = nextBlock) {
		range = block->ranges;
		nextBlock = block->next;
		for (; range; range = nextRange) {
			nextRange = range->next;
			FreeProcessGpuMemory(range);
		}
		block->ranges = 0;
		uid = block->mappingUid;
		if (uid != -1) {
			ksceKernelDecRefCountMemBlock(uid);
			ksceKernelFreeMemBlock(block->mappingUid);
		}
		uid = block->referencedUid;
		if (uid != -1)
			ksceKernelDecRefCountMemBlock(uid);
		FreeProcessGpuMemory((void *)block);
	}
	FreeProcessGpuMemory(heap);
	return 0;
}
