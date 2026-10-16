/* SceSysmem: the MMU interface (mmu.c) shared with the other files */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>
#include "sce/mmu.h"

/* a memblock type code: access in bits 7:0, memory type in 15:8 (henkaku wiki "memtype bit value") */
#define MEMBLOCK_CODE_ACCESS(type)		((type) & 0xff)
#define MEMBLOCK_CODE_MEMORY_TYPE_MASK		0xff00
#define MEMBLOCK_CODE_MEMORY_TYPE(type)		((type) & MEMBLOCK_CODE_MEMORY_TYPE_MASK)
#define MEMBLOCK_CODE_GLOBAL			0x10000		/* no nG bit */

/* access, Sony's SDK memblock.h; user mode's are the same shifted by 4 */
#define SCE_KERNEL_MEMORY_ACCESS_R		0x04
#define SCE_KERNEL_MEMORY_ACCESS_W		0x02
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
