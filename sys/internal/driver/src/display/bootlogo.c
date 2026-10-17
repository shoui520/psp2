/*
 * SceDisplay: the boot logo shown until a process sets a frame buffer.
 * File name unknown; the module links its parts in this order.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/systimer.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>
#include <psp2kern/lowio/iftu.h>
#include <display/display.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include "display_internal.h"
#include "display_imports.h"

#include "boot_logo.h"
#include <compat/compat_regs.h>

static SceUID bootLogoBlock;

/* Show the boot logo on a head and fade it in. */
int showBootLogo(SceUInt32 head) /* name unknown */
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceDisplayFrameBuf frameBuf;
	void *base;
	SceUID block;
	int alpha, ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = COMPAT_SRAM_BASE;
	block = ksceKernelAllocMemBlock("SceDisplay", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW,
				       COMPAT_SRAM_SIZE, &opt);
	if (block < 0)
		return block;
	bootLogoBlock = block;
	ksceKernelGetMemBlockBase(block, &base);
	if (ksceGzipDecompress(base, 960 * 544 * 4, bootLogo, NULL) < 0) {
		ksceKernelFreeMemBlock(block);
		return -1;
	}
	ksceKernelDcacheCleanInvalidateRange(base, 960 * 544 * 4);
	frameBuf.size = sizeof(frameBuf);
	frameBuf.base = base;
	frameBuf.pitch = 960;
	frameBuf.pixelformat = 0;
	frameBuf.width = 960;
	frameBuf.height = 544;
	ret = ksceDisplaySetFrameBufInternal(head, 0, &frameBuf, 1);
	if (ret < 0)
		return ret;
	for (alpha = 0; alpha != 0x102; alpha += 3) {
		ksceDisplaySetMergeConf(head, 4, alpha);
		ksceDisplayWaitVblankStart();
	}
	return 0;
}

void freeBootLogo(SceUInt32 head) /* name unknown */
{
	SceUID block;

	block = ksceKernelAtomicGetAndAnd32(&bootLogoBlock, 0);
	if (block > 0) {
		ksceDisplaySetFrameBufInternal(head, 0, NULL, 1);
		ksceDisplaySetOwner(head, 0, -1);
		ksceKernelFreeMemBlock(block);
	}
}
