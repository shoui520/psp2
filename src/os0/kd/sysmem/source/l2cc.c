/*
 * SceSysmem: the PL310 L2 cache controller. Its registers are used at their
 * physical address until sceKernelL2CacheRemap maps them. The secure
 * kernel's SceSysmem (SYSMEM_TZS) keeps only the remap here; its maintenance
 * functions are assembly. SKBL (SYSMEM_SKBL) sets the controller up, keeps
 * the registers physical and adds whole-cache maintenance.
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

#if defined(SYSMEM_SKBL)
/* name unknown: prefetching (r3p0 and later), the auxiliary control, clock
 * gating and the RAM latencies */
__attribute__((noinline))
void CpuL2CacheSetup(void)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;
	int release = registers->cacheId & 0x3f;

	if (release >= 6) {
		if (release >= 7)
			registers->prefetchControl = 0x21000000;
		else
			registers->prefetchControl = 0x20000000;
	}
	registers->auxControl = 0x60090001;
	registers->powerControl = 2;
	registers->tagRamControl = 0;
	registers->dataRamControl = 0x131;
	dsb();
}

/* name unknown: the event monitor bus, for the performance monitors */
void CpuL2CacheSetEventMonitorBus(int enable)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;

	if (enable)
		registers->auxControl |= PL310_AUX_EVENT_MONITOR_BUS;
	else
		registers->auxControl &= ~PL310_AUX_EVENT_MONITOR_BUS;
	dsb();
}

/* name unknown */
void CpuL2CacheEnable(void)
{
	s_cpuL2RegisterBase->control = 1;
	dsb();
}

/* name unknown: drains the controller before turning it off */
__attribute__((noinline))
void CpuL2CacheDisable(void)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;

	registers->cacheSync = 0;
	while (registers->cacheSync & 1)
		;
	registers->control = 0;
	dsb();
}
#endif

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

#if defined(SYSMEM_SKBL)
/* name unknown: every way */
void CpuL2InvalidateAll(void)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;

	registers->invalidateWay = 0xffff;
	dmb();
	while (registers->invalidateWay & 1)
		;
}
#endif

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

#if defined(SYSMEM_SKBL)
/* name unknown: every way */
void CpuL2CleanInvalidateAll(void)
{
	ScePl310Registers *registers = s_cpuL2RegisterBase;

	registers->cleanInvalidateWay = 0xffff;
	dmb();
	while (registers->cleanInvalidateWay & 1)
		;
}

/* name unknown (cache.c by its address): the controller at registerBase, off
 * and set up again */
void CpuL2CacheInit(ScePl310Registers *registerBase)
{
	s_cpuL2RegisterBase = registerBase;
	CpuL2CacheDisable();
	CpuL2CacheSetup();
}
#else
void SceCpuForDriver_E813EBB2(void)
{
	dsb();
	CpuL2CacheSyncCore();
}
#endif

#endif

#if !defined(SYSMEM_SKBL)

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
#endif
