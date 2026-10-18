/*
 * SceSysmem: the PL310 L2 cache controller. Its registers are used at their
 * physical address until sceKernelL2CacheRemap maps them. The secure
 * kernel's SceSysmem (SYSMEM_TZS) keeps only the remap here. The sync and
 * the by-address maintenance are Sony's assembly (l2cc_maintenance.S), as
 * are SKBL's (SYSMEM_SKBL) setup, enable, disable and whole-cache
 * maintenance; SKBL keeps the registers physical.
 */

#include <stddef.h>
#include <stdint.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/pl310_regs.h>
#include <sysmem/barrier.h>

/* physical until sceKernelL2CacheRemap maps the registers; l2cc_maintenance.S reads it */
ScePl310Registers *volatile s_cpuL2RegisterBase =
	(ScePl310Registers *)PL310_REG_BASE;

#ifndef SYSMEM_TZS

void CpuL2CacheSyncCore(void);

#if defined(SYSMEM_SKBL)
void CpuL2CacheSetup(void);
void CpuL2CacheDisable(void);

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
	block = sceKernelAllocMemBlockForKernel("SceL2CacheReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
					0x1000, &opt);
	if (block < 0)
		SCE_PANIC(350);
	sceKernelGetMemBlockBase(block, &base);
	s_cpuL2RegisterBase = base;
}
#endif
