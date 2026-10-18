/*
 * kernel/sysmem/memblock.h: user memory blocks, under Sony's names and layouts (SDK Release
 * 03.570.011, target/include/kernel/sysmem/memblock.h). SceSysmem's syscalls implement the
 * functions; the kernel's own memory block types are sysmem/memblock.h's.
 */

#pragma once

#include <scetypes.h>
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the memory block types user code allocates */
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW			0x0c20d060U
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA			SCE_KERNEL_MEMBLOCK_TYPE_USER_RW
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW			0x0c208060U
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE		SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW		0x0c80d060U
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW	0x0d808060U
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW			0x09408060U
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW

/* SceKernelAllocMemBlockOpt.attr */
#define SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT		0x00000004U
#define SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT			0x00200000U

/* SceKernelMemBlockInfo.access */
#define SCE_KERNEL_MEMORY_ACCESS_R				0x04U
#define SCE_KERNEL_MEMORY_ACCESS_W				0x02U

/* SceKernelMemBlockInfo.memoryType */
#define SCE_KERNEL_MEMORY_TYPE_NORMAL_NC			0x80
#define SCE_KERNEL_MEMORY_TYPE_NORMAL				0xD0

/* sceKernelAllocMemBlock's option before SDK 0.940 */
typedef struct SceKernelAllocMemBlockOpt0940 {
	SceSize size;
	SceUInt32 attr;
	SceSize alignment;
	SceUID uidBaseBlock;
	const char *strBaseBlockName;
} SceKernelAllocMemBlockOpt0940;

typedef struct SceKernelAllocMemBlockOpt {
	SceSize size;				/* sizeof(SceKernelAllocMemBlockOpt) */
	SceUInt32 attr;
	SceSize alignment;			/* with SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT */
	SceUInt32 reserved;
	const char *strBaseBlockName;
} SceKernelAllocMemBlockOpt;

typedef int SceKernelMemoryType;

/* sceKernelGetMemBlockInfoByAddr and ...ByRange */
typedef struct SceKernelMemBlockInfo {
	SceSize size;				/* sizeof(SceKernelMemBlockInfo) */
	void *mappedBase;
	SceSize mappedSize;
	SceKernelMemoryType memoryType;
	SceUInt32 access;
	SceKernelMemBlockType type;
} SceKernelMemBlockInfo;

SceUID sceKernelAllocMemBlock(const char *name, SceKernelMemBlockType type, SceSize vsize,
			      const struct SceKernelAllocMemBlockOpt *pOpt);
int sceKernelFreeMemBlock(SceUID uid);
int sceKernelGetMemBlockBase(SceUID uid, void **ppBase);
SceUID sceKernelFindMemBlockByAddr(void *uaddr, SceSize size);
int sceKernelGetMemBlockInfoByAddr(void *vbase, SceKernelMemBlockInfo *pInfo);
int sceKernelGetMemBlockInfoByRange(void *vbase, SceSize vsize, SceKernelMemBlockInfo *pInfo);

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(SceKernelAllocMemBlockOpt) == 0x14, "SceKernelAllocMemBlockOpt size");
_Static_assert(sizeof(SceKernelMemBlockInfo) == 0x18, "SceKernelMemBlockInfo size");
#endif

#ifdef __cplusplus
}
#endif
