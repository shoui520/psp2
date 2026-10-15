/*
 * SceSysmem: the PL310 L2 cache controller. Its registers are used at their
 * physical address until sceKernelL2CacheRemap maps them. The secure
 * kernel's SceSysmem (SYSMEM_TZS) keeps only the remap here; its maintenance
 * functions are assembly.
 */

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include "fw.h"
#include <sce/debug.h>
#include <sce/pl310_regs.h>
#include <sce/barrier.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "l2cc.c"

/* physical until sceKernelL2CacheRemap maps the registers */
static ScePl310Registers *volatile s_cpuL2RegisterBase =
	(ScePl310Registers *)PL310_REG_BASE;

#ifndef SYSMEM_TZS

__attribute__((noinline))
void CpuL2CacheSyncCore(void)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;

	registers->cacheSync = 0;
	while (registers->cacheSync & 1)
		;
	dmb();
}

/*
 * name unknown: L2 maintenance by physical address, line by line. The
 * invalidate does not widen the size by the start's offset in its line.
 */
__attribute__((noinline))
void CpuL2InvalidateRangePA(uintptr_t pa, SceSize size)
{
	ScePl310Registers *registers;
	int remaining = size;

	if (size == 0)
		return;
	registers = s_cpuL2RegisterBase;
	pa &= ~0x1f;
	do {
		registers->invalidatePA = pa;
		pa += 32;
		remaining -= 32;
	} while (remaining > 0);
	dmb();
}

__attribute__((noinline))
void CpuL2CleanRangePA(uintptr_t pa, SceSize size)
{
	ScePl310Registers *registers;
	int remaining = size;

	if (size == 0)
		return;
	registers = s_cpuL2RegisterBase;
	remaining += pa & 0x1f;
	pa &= ~0x1f;
	do {
		registers->cleanPA = pa;
		pa += 32;
		remaining -= 32;
	} while (remaining > 0);
	dmb();
}

__attribute__((noinline))
void CpuL2CleanInvalidateRangePA(uintptr_t pa, SceSize size)
{
	ScePl310Registers *registers;
	int remaining = size;

	if (size == 0)
		return;
	registers = s_cpuL2RegisterBase;
	remaining += pa & 0x1f;
	pa &= ~0x1f;
	do {
		registers->cleanInvalidatePA = pa;
		pa += 32;
		remaining -= 32;
	} while (remaining > 0);
	dmb();
}

void SceCpuForDriver_E813EBB2(void)
{
	dsb();
	CpuL2CacheSyncCore();
}

#endif

/* the controller's registers into kernel virtual memory */
void sceKernelL2CacheRemap(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID block;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PL310_REG_BASE;
	block = ksceKernelAllocMemBlock("SceL2CacheReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
					0x1000, &opt);
	if (block < 0)
		SCE_PANIC(350);
	ksceKernelGetMemBlockBase(block, &base);
	s_cpuL2RegisterBase = base;
}
