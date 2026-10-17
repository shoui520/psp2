/*
 * dtrace/sce/dtrace_bookmark.c: Sony's DTrace addition that copies trace
 * records into the Perfmon block (SceDeci4pDtracepPaReg), where the
 * hardware trace stores them next to its own.
 *
 * PARTIAL: the copy (0x81021390 in 4.00) reads DTrace's dtrace_buffer_t
 * and is written when DTrace is vendored. Internal names are unknown.
 */
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <pamgr/perfmon_regs.h>

static struct {
	ScePerfmonRegisters *regs;
	SceUID uid;
} s_bookmark;

int dtrace_bookmark_init(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;

	s_bookmark.regs = NULL;
	s_bookmark.uid = 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr = PERFMON_REG_BASE;
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	uid = SceSysmemForDriver_C94850C9("SceDeci4pDtracepPaReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
				      2 * PERFMON_PAGE_SIZE, &opt);
	if (uid < 0)
		return uid;
	s_bookmark.uid = uid;
	SceSysmemForDriver_A841EDDA(uid, &base);
	s_bookmark.regs = base;
	return 0;
}

int dtrace_bookmark_fini(void)
{
	SceSysmemForDriver_009E1C61(s_bookmark.uid);
	return 0;
}
