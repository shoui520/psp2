/* SceSysmem: the MMU interface (mmu.c) shared with the other files */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/mmu.h>
#include <kernel/sysmem/memblock.h>

/* a memblock type code: access in bits 7:0, memory type in 15:8 (henkaku wiki "memtype bit value") */
#define MEMBLOCK_CODE_ACCESS(type)		((type) & 0xff)
#define MEMBLOCK_CODE_MEMORY_TYPE_MASK		0xff00
#define MEMBLOCK_CODE_MEMORY_TYPE(type)		((type) & MEMBLOCK_CODE_MEMORY_TYPE_MASK)
#define MEMBLOCK_CODE_GLOBAL			0x10000		/* no nG bit */

/* access: R and W are the SDK's (kernel/sysmem/memblock.h), X ours; user mode's are the same shifted by 4 */
#define SCE_KERNEL_MEMORY_ACCESS_X		0x01
#define MEMBLOCK_ACCESS_KERNEL_R		SCE_KERNEL_MEMORY_ACCESS_R
#define MEMBLOCK_ACCESS_KERNEL_RX		(SCE_KERNEL_MEMORY_ACCESS_R | SCE_KERNEL_MEMORY_ACCESS_X)
#define MEMBLOCK_ACCESS_KERNEL_RW		(SCE_KERNEL_MEMORY_ACCESS_R | SCE_KERNEL_MEMORY_ACCESS_W)
#define MEMBLOCK_ACCESS_KERNEL_RWX		(MEMBLOCK_ACCESS_KERNEL_RW | SCE_KERNEL_MEMORY_ACCESS_X)
#define MEMBLOCK_ACCESS_USER_R			(MEMBLOCK_ACCESS_KERNEL_R << 4)
#define MEMBLOCK_ACCESS_USER_RX			(MEMBLOCK_ACCESS_KERNEL_RX << 4)
#define MEMBLOCK_ACCESS_USER_RW			(MEMBLOCK_ACCESS_KERNEL_RW << 4)

/* memory types (SCE_KERNEL_MEMORY_TYPE_* and the wiki's code memory types, << 8) */
#define MEMBLOCK_CODE_SO			0x0200
#define MEMBLOCK_CODE_DEVICE			0x0800
#define MEMBLOCK_CODE_L1WBWA			0x4000
#define MEMBLOCK_CODE_NORMAL_NC			0x8000
#define MEMBLOCK_CODE_NORMAL			0xd000

int MMUContextSave(MMUContext *context);
int MMUContextRestore(const MMUContext *context);
int MMUContextSwitch(const MMUContext *context, MMUContext *saved, uint32_t *contextId);
int MMUContextSwitchSync(const MMUContext *context, MMUContext *saved);
int MMUVAtoPACore(uint32_t mode, uint32_t flags, uintptr_t va, uintptr_t *pa, int *memoryType);
int MMUCheckAccessibleRange(uint32_t mode, uintptr_t va, SceSize size);

/* mmu.c */
int MMUSetReady(void);
int MMUInitPost(void);
/* the boot loaders: KblMMUInit (cache.c) starts the performance monitor (KblPmuInit, dipsw.c) */
struct SceKernelBootParam;
int KblMMUInit(struct SceKernelBootParam *param);
int KblPmuInit(void *sysroot);
int sceKernelMMUL1Init(uint32_t *l1);
int sceKernelMMUSetL2PageTable(uint32_t *l1, uint32_t domain, uintptr_t va, uintptr_t l2pa);
int sceKernelMMUUnsetL2PageTable(uint32_t *l1, uintptr_t va);
int sceKernelMMUMapSupersections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
				 SceSize size, uintptr_t pa);
int sceKernelMMUMapSections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
			    SceSize size, uintptr_t pa);
int sceKernelMMUUnmapSectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				       uint32_t flags);
int MMUUnmapSupersectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				   uint32_t flags);
int sceKernelMMUL2Init(uint32_t *l2);
/* the boot loaders: one small page */
int sceKernelMMUMapSmallPage(uint32_t *l2, uint32_t type, uintptr_t va, uintptr_t offset,
			     uintptr_t pa);
int sceKernelMMUMapLargePages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa);
int sceKernelMMUUnmapLargePagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags);
int sceKernelMMUMapSmallPages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa);
int sceKernelMMUUnmapSmallPagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags);
int MMUVAtoPAWithFlags(uint32_t mode, uint32_t flags, uintptr_t va, uintptr_t *pa);
int MMUVAtoPAWithMemoryType(uint32_t mode, uintptr_t va, uintptr_t *pa, int *memoryType);
/*
 * Walking the tables. These return 3 for a section, 4 for a supersection,
 * 5 for an L1 entry pointing to an L2 table and 6 for a fault entry; the L2
 * walkers 1 for a small page, 2 for a large page and 6 for a fault entry.
 */
#define MMU_WALK_SMALL_PAGE		1
#define MMU_WALK_LARGE_PAGE		2
#define MMU_WALK_SECTION		3
#define MMU_WALK_SUPERSECTION		4
#define MMU_WALK_PAGE_TABLE		5
#define MMU_WALK_FAULT			6
int sceKernelMMUL1VAtoPABySW(const uint32_t *l1, uintptr_t va, uintptr_t *pa);
int sceKernelMMUL1IsEqualAccessibleBySW(const uint32_t *l1, uint32_t perm, uintptr_t va,
					uintptr_t *pa);
int sceKernelMMUL1GetMemoryType(const uint32_t *l1, uintptr_t va);
int MMUL2VAtoPABySW(const uint32_t *l2, uintptr_t va, uintptr_t *pa);
int MMUL2IsEqualAccessibleBySW(const uint32_t *l2, uint32_t perm, uintptr_t va, uintptr_t *pa);
int sceKernelMMUL2GetMemoryType(const uint32_t *l2, uintptr_t va);
int MMUIsValidMap(const MMUContext *context, uintptr_t va, SceSize size, uintptr_t pa);
