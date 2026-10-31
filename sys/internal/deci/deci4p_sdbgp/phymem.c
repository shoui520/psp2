/*
 * Physical memory read and write for the system debugger (Sony: phymem.c):
 * maps one 4 KiB page at a time into the per-core "SceKernelPhyAccessWindow"
 * L2 page-table slot and accesses it through the fault-tolerant VMRead/VMWrite
 * probes.
 */

#include <sysmem/address_space.h>
#include <sysmem/barrier.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include "sdbgp_internal.h"

typedef struct SdbgpPhyMemRange {
	uintptr_t base;
	SceSize size;
} SdbgpPhyMemRange;

static uint32_t *s_phyAccessPte[4];
static uintptr_t s_phyAccessWindow[4];

static const SdbgpPhyMemRange s_phyMemRanges[138] = {
	{ 0x00000000, 0x00040000 },
	{ 0x1a000000, 0x00002000 },
	{ 0x1a002000, 0x00001000 },
	{ 0x1d000000, 0x00001000 },
	{ 0x1d001000, 0x00001000 },
	{ 0x1f000000, 0x00008000 },
	{ 0x1f840000, 0x00020000 },
	{ 0x20000000, 0x10000000 },
	{ 0x40200000, 0x3fe00000 },
	{ 0xe0100000, 0x00010000 },
	{ 0xe0400000, 0x00010000 },
	{ 0xe0420000, 0x00010000 },
	{ 0xe0430000, 0x00010000 },
	{ 0xe0440000, 0x00010000 },
	{ 0xe0450000, 0x00010000 },
	{ 0xe0470000, 0x00010000 },
	{ 0xe0460000, 0x00010000 },
	{ 0xe0490000, 0x00010000 },
	{ 0xe04a0000, 0x00010000 },
	{ 0xe04b0000, 0x00010000 },
	{ 0xe04c0000, 0x00010000 },
	{ 0xe04d0000, 0x0000c000 },
	{ 0xe04dc000, 0x00004000 },
	{ 0xe0500000, 0x00010000 },
	{ 0xe0510000, 0x00010000 },
	{ 0xe0800000, 0x00010000 },
	{ 0xe0900000, 0x00010000 },
	{ 0xe0a00000, 0x00010000 },
	{ 0xe0a10000, 0x00010000 },
	{ 0xe0a20000, 0x00010000 },
	{ 0xe0b00000, 0x00010000 },
	{ 0xe0c00000, 0x00010000 },
	{ 0xe0c10000, 0x00010000 },
	{ 0xe0c20000, 0x00010000 },
	{ 0xe2030000, 0x00010000 },
	{ 0xe2040000, 0x00010000 },
	{ 0xe2050000, 0x00010000 },
	{ 0xe2060000, 0x00010000 },
	{ 0xe2070000, 0x00010000 },
	{ 0xe2080000, 0x00010000 },
	{ 0xe2090000, 0x00010000 },
	{ 0xe20a0000, 0x00010000 },
	{ 0xe20b0000, 0x00001000 },
	{ 0xe20b1000, 0x00001000 },
	{ 0xe20b2000, 0x00001000 },
	{ 0xe20b3000, 0x00001000 },
	{ 0xe20b4000, 0x00001000 },
	{ 0xe20b5000, 0x00001000 },
	{ 0xe20b6000, 0x00001000 },
	{ 0xe20b7000, 0x00001000 },
	{ 0xe20b8000, 0x00001000 },
	{ 0xe20b9000, 0x00001000 },
	{ 0xe20ba000, 0x00001000 },
	{ 0xe20bb000, 0x00001000 },
	{ 0xe20bc000, 0x00001000 },
	{ 0xe20bd000, 0x00001000 },
	{ 0xe20be000, 0x00001000 },
	{ 0xe20bf000, 0x00001000 },
	{ 0xe20c0000, 0x00010000 },
	{ 0xe3000000, 0x00010000 },
	{ 0xe3010000, 0x00010000 },
	{ 0xe3020000, 0x00010000 },
	{ 0xe3030000, 0x00010000 },
	{ 0xe3050000, 0x00010000 },
	{ 0xe3060000, 0x00010000 },
	{ 0xe3100000, 0x00001000 },
	{ 0xe3101000, 0x00001000 },
	{ 0xe3102000, 0x00001000 },
	{ 0xe3103000, 0x00001000 },
	{ 0xe3104000, 0x00001000 },
	{ 0xe3105000, 0x00001000 },
	{ 0xe3106000, 0x00001000 },
	{ 0xe3108000, 0x00001000 },
	{ 0xe3109000, 0x00001000 },
	{ 0xe310a000, 0x00001000 },
	{ 0xe310b000, 0x00001000 },
	{ 0xe310c000, 0x00001000 },
	{ 0xe310d000, 0x00001000 },
	{ 0xe310e000, 0x00001000 },
	{ 0xe310f000, 0x00001000 },
	{ 0xe3110000, 0x00010000 },
	{ 0xe3200000, 0x00006000 },
	{ 0xe3300000, 0x00001000 },
	{ 0xe3310000, 0x00010000 },
	{ 0xe3320000, 0x00004000 },
	{ 0xe4020000, 0x00001000 },
	{ 0xe40b0000, 0x00001000 },
	{ 0xe40c0000, 0x00010000 },
	{ 0xe40d0000, 0x00010000 },
	{ 0xe40e0000, 0x00001000 },
	{ 0xe40f0000, 0x00010000 },
	{ 0xe5000000, 0x00010000 },
	{ 0xe5010000, 0x00010000 },
	{ 0xe5020000, 0x00001000 },
	{ 0xe5021000, 0x00001000 },
	{ 0xe5022000, 0x00001000 },
	{ 0xe5030000, 0x00001000 },
	{ 0xe5031000, 0x00001000 },
	{ 0xe5032000, 0x00001000 },
	{ 0xe5040000, 0x00001000 },
	{ 0xe5050000, 0x00010000 },
	{ 0xe5060000, 0x00010000 },
	{ 0xe5070000, 0x00001000 },
	{ 0xe5071000, 0x00001000 },
	{ 0xe50c0000, 0x00010000 },
	{ 0xe50d0000, 0x00010000 },
	{ 0xe5800000, 0x00010000 },
	{ 0xe5810000, 0x00010000 },
	{ 0xe8400000, 0x00004000 },
	{ 0xe8404000, 0x00004000 },
	{ 0xe8408000, 0x00004000 },
	{ 0xe840c000, 0x00004000 },
	{ 0xe8410000, 0x00004000 },
	{ 0xe8414000, 0x00004000 },
	{ 0xec000000, 0x00040000 },
	{ 0xec100000, 0x00040000 },
	{ 0xec200000, 0x00040000 },
	{ 0xec300000, 0x00040000 },
	{ 0xec400000, 0x00040000 },
	{ 0xec500000, 0x00040000 },
	{ 0xec600000, 0x00040000 },
	{ 0xed000000, 0x00040000 },
	{ 0xed100000, 0x00040000 },
	{ 0xed200000, 0x00040000 },
	{ 0xed300000, 0x00040000 },
	{ 0xed400000, 0x00040000 },
	{ 0xed500000, 0x00040000 },
	{ 0xed600000, 0x00040000 },
	{ 0xed700000, 0x00040000 },
	{ 0xed800000, 0x00040000 },
	{ 0xed900000, 0x00040000 },
	{ 0xee000000, 0x00040000 },
	{ 0xee100000, 0x00040000 },
	{ 0xee200000, 0x00040000 },
	{ 0xee300000, 0x00040000 },
	{ 0xee400000, 0x00040000 },
	{ 0xee500000, 0x00040000 },
	{ 0xee600000, 0x00040000 },
};

__attribute__((noinline))
static int checkPhysicalMemRange(uintptr_t paddr, SceSize size)
{
	uintptr_t end = paddr + size;
	int i;

	for (i = 0; i < 138; i++) {
		uintptr_t base = s_phyMemRanges[i].base;

		if (paddr >= base && end <= base + s_phyMemRanges[i].size && paddr <= end)
			return 0;
	}
	return SCE_KERNEL_ERROR_NO_IOADDR;
}

int sceKernelPhysicalMemReadForDebugger(void *dest, uintptr_t psrc, SceSize size, SceSize width)
{
	uintptr_t mask;
	uint32_t pteAttr;
	int intr;
	uintptr_t window;
	uint32_t *pte;
	int result;

	if (width == 8 || width == 16) {
		mask = 3;
		width = 4;
	} else {
		mask = width - 1;
		if (width != 4 && mask > 1)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (((uintptr_t)dest | psrc | size) & mask)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	result = checkPhysicalMemRange(psrc, size);
	if (result < 0)
		return result;
	if (psrc <= 0x401fffff || psrc + size > 0x80000000 || psrc > psrc + size)
		pteAttr = 0x413;
	else
		pteAttr = 0x453;
	intr = sceKernelCpuSuspendIntr();
	result = size;
	window = s_phyAccessWindow[sceKernelCpuId()];
	pte = s_phyAccessPte[sceKernelCpuId()];
	if (size != 0) {
		uintptr_t vsrc = 0;
		uintptr_t mapped = 1;
		uintptr_t paddr = psrc;
		uintptr_t end = psrc + size;
		SceSize done = 0;

		for (; paddr < end; paddr += width) {
			uintptr_t page = (paddr >> 12) << 12;
			void *vdest = (uint8_t *)dest + (paddr - psrc);

			if (page != mapped) {
				uint32_t dacr;

				vsrc = window | (paddr & 0xfff);
				dacr = readDacr();
				writeDacr(0x17450000);
				*pte = pteAttr | page;
				dmb();
				writeDccimvac((uintptr_t)pte);
				dsb();
				writeTlbimvaais(window);
				isb();
				writeDacr(dacr);
				mapped = page;
			}
			done += width;
			switch (width) {
			case 1:
				result = sceKernelVMRead8(vdest, (const void *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 422,
						       "sceKernelVMRead8(psrc 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			case 2:
				result = sceKernelVMRead16(vdest, (const void *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 431,
						       "sceKernelVMRead16(psrc 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			case 4:
				result = sceKernelVMRead32(vdest, (const void *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 440,
						       "sceKernelVMRead32(psrc 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			}
			vsrc += width;
		}
		result = done;
	}
out:
	{
		uint32_t dacr = readDacr();

		writeDacr(0x17450000);
		*pte = 0;
		dmb();
		writeDccimvac((uintptr_t)pte);
		dsb();
		writeTlbimvaais(window);
		isb();
		writeDacr(dacr);
	}
	sceKernelCpuResumeIntr(intr);
	return result;
}

int sceKernelPhysicalMemWriteForDebugger(uintptr_t pdest, const void *src, SceSize size,
					 SceSize width)
{
	uintptr_t mask;
	int intr;
	uintptr_t window;
	uint32_t *pte;
	int result;

	if (width == 8 || width == 16) {
		mask = 3;
		width = 4;
	} else {
		mask = width - 1;
		if (width != 4 && mask > 1)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if ((pdest | size | (uintptr_t)src) & mask)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	result = checkPhysicalMemRange(pdest, size);
	if (result < 0)
		return result;
	intr = sceKernelCpuSuspendIntr();
	result = size;
	window = s_phyAccessWindow[sceKernelCpuId()];
	pte = s_phyAccessPte[sceKernelCpuId()];
	if (size != 0) {
		uintptr_t vdest = 0;
		uintptr_t mapped = 1;
		uintptr_t paddr = pdest;
		uintptr_t end = pdest + size;
		SceSize done = 0;

		for (; paddr < end; paddr += width) {
			uintptr_t page = (paddr >> 12) << 12;
			const void *vsrc = (const uint8_t *)src + (paddr - pdest);

			if (page != mapped) {
				uint32_t dacr;

				vdest = window | (paddr & 0xfff);
				dacr = readDacr();
				writeDacr(0x17450000);
				*pte = page | 0x413;
				dmb();
				writeDccimvac((uintptr_t)pte);
				dsb();
				writeTlbimvaais(window);
				isb();
				writeDacr(dacr);
				mapped = page;
			}
			done += width;
			switch (width) {
			case 1:
				result = sceKernelVMWrite8((void *)vdest, *(const uint8_t *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 551,
						       "sceKernelVMWrite8(pdest 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			case 2:
				result = sceKernelVMWrite16((void *)vdest, *(const uint16_t *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 560,
						       "sceKernelVMWrite16(pdest 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			case 4:
				result = sceKernelVMWrite32((void *)vdest, *(const uint32_t *)vsrc);
				if (result < 0) {
					SCE_DBG_PRINTF(0xb, 569,
						       "sceKernelVMWrite32(pdest 0x%08x) failed 0x%08x\n",
						       paddr, result);
					goto out;
				}
				break;
			}
			vdest += width;
		}
		result = done;
	}
out:
	{
		uint32_t dacr = readDacr();

		writeDacr(0x17450000);
		*pte = 0;
		dmb();
		writeDccimvac((uintptr_t)pte);
		dsb();
		writeTlbimvaais(window);
		isb();
		writeDacr(dacr);
	}
	sceKernelCpuResumeIntr(intr);
	return result;
}

int sdbgpInitPhysicalAccess(int unused)
{
	SceKernelAllocMemBlockKernelOpt opt;
	AddressSpaceObject *as;
	SceUID uid;
	uintptr_t vbase;
	int i;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR |
		   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.paddr = 0;
	opt.alignment = 0x20000;
	uid = sceKernelAllocMemBlockForKernel("SceKernelPhyAccessWindow", 0x20100206,
					      0x20000, &opt);
	if (uid < 0)
		return uid;
	sceKernelGetMemBlockBase(uid, (void **)&vbase);
	s_phyAccessWindow[0] = vbase;
	s_phyAccessWindow[1] = vbase + 0x8000;
	s_phyAccessWindow[2] = vbase + 0x10000;
	s_phyAccessWindow[3] = vbase + 0x18000;
	as = sceKernelGetKernelAddressSpace();
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		Partition *part = as->partition[i];
		uintptr_t addr;
		L2PageTable *l2pt;
		SceUInt32 slot;
		int intr;

		if (part == NULL)
			continue;
		addr = vbase;
		if (addr < part->tiny.vbase || addr - part->tiny.vbase > part->tiny.size)
			continue;
		intr = SceCpuForDriver_4C38CE4D(&part->l2Lock);
		slot = part->tiny.context->l2Tables[addr >> 20];
		l2pt = (slot & 3) == 1 ? (L2PageTable *)(slot & ~3u) : NULL;
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&part->l2Lock, intr);
		if (l2pt != NULL && l2pt->table != NULL) {
			uint32_t *pte = &l2pt->table[(vbase >> 12) & 0xff];
			uint32_t dacr;
			int j;

			s_phyAccessPte[0] = pte;
			s_phyAccessPte[1] = pte + 8;
			s_phyAccessPte[2] = pte + 16;
			s_phyAccessPte[3] = pte + 24;
			intr = sceKernelCpuSuspendIntr();
			dacr = readDacr();
			writeDacr(0x17450000);
			for (j = 0; j < 32; j++)
				pte[j] = 0;
			writeDacr(dacr);
			sceKernelCpuResumeIntr(intr);
			return 0;
		}
	}
	return 0;
}
