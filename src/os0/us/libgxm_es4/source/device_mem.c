/*
 * SceGxm: device memory blocks for the test interface.
 *
 * A block is memory allocated from the kernel and mapped for the GPU as
 * plain memory or as vertex or fragment USSE code.
 */

#include "gxm_internal.h"

#define GXM_DEVICE_MEM_LPDDR		0
#define GXM_DEVICE_MEM_CDRAM		1
#define GXM_DEVICE_MEM_VERTEX_USSE	2
#define GXM_DEVICE_MEM_FRAGMENT_USSE	3

typedef struct GxmDeviceMem {
	SceUID uid;
	void *base;
	SceUInt32 usseOffset;
	SceSize size;
	SceUInt32 type;
} GxmDeviceMem;

int SceGxmInternalForTest_D412F27B(SceUInt32 type, SceUInt32 attr, SceSize size,
				   SceSize alignment, GxmDeviceMem **memory)
{
	GxmDeviceMem *block;
	SceSize allocSize;
	int err;

	if (type > GXM_DEVICE_MEM_FRAGMENT_USSE || (attr & ~3) != 0 || attr == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((alignment & (alignment - 1)) != 0 || alignment == 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (memory == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	/* CDRAM comes in 256 KiB units, the rest in pages */
	if (type == GXM_DEVICE_MEM_CDRAM)
		allocSize = (size + 0x3ffff) & ~0x3ffff;
	else
		allocSize = (size + 0xfff) & ~0xfff;
	block = SceGpuEs4User_7AF40138(sizeof(*block));
	if (block == NULL)
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	block->size = size;
	block->uid = sceKernelAllocMemBlock("devicemem",
					    type == GXM_DEVICE_MEM_CDRAM ?
					    SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW :
					    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
					    allocSize, NULL);
	if (block->uid < 0)
		return block->uid;
	err = sceKernelGetMemBlockBase(block->uid, &block->base);
	if (err != 0)
		return err;
	block->type = type;
	if (type == GXM_DEVICE_MEM_VERTEX_USSE)
		err = sceGxmMapVertexUsseMemory(block->base, allocSize, &block->usseOffset);
	else if (type == GXM_DEVICE_MEM_FRAGMENT_USSE)
		err = sceGxmMapFragmentUsseMemory(block->base, allocSize, &block->usseOffset);
	else
		err = sceGxmMapMemory(block->base, allocSize, attr);
	if (err != 0)
		return err;
	*memory = block;
	return 0;
}

int SceGxmInternalForTest_2B0A5A39(GxmDeviceMem *memory)
{
	if (memory != NULL) {
		if (memory->type == GXM_DEVICE_MEM_VERTEX_USSE)
			sceGxmUnmapVertexUsseMemory(memory->base);
		else if (memory->type == GXM_DEVICE_MEM_FRAGMENT_USSE)
			sceGxmUnmapFragmentUsseMemory(memory->base);
		else
			sceGxmUnmapMemory(memory->base);
		sceKernelFreeMemBlock(memory->uid);
		SceGpuEs4User_1BE5985A(memory);
	}
	return 0;
}

void *SceGxmInternalForTest_7C427319(const GxmDeviceMem *memory)
{
	if (memory == NULL)
		return NULL;
	return memory->base;
}

SceUInt32 SceGxmInternalForTest_588FD7FD(const GxmDeviceMem *memory)
{
	if (memory == NULL)
		return 0;
	return memory->usseOffset;
}

SceUInt32 SceGxmInternalForTest_3D8863E2(const GxmDeviceMem *memory)
{
	if (memory == NULL)
		return 0;
	return memory->usseOffset;
}

void SceGxmInternalForTest_9C7DC365(void)
{
}

void SceGxmInternalForTest_9CAFAE4B(void)
{
}

/* the release library does not name memory blocks */
void SceGxmInternalForTest_631E95AA(const void *mem, SceSize size, SceUInt32 a, SceUInt32 b,
				    const char *name)
{
}
