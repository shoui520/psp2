/*
 * dtrace/sce/dtrace_bookmark.c: Sony's DTrace addition that copies trace
 * records into the Perfmon block (SceDeci4pDtracepPaReg), where the
 * hardware trace stores them next to its own.
 */
#include <sys/dtrace_impl.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <pamgr/perfmon_regs.h>

/* a bookmark word: a header with the record size, then the record's bytes */
#define DTRACE_BOOKMARK_TAG	0xa0

typedef union dtrace_bookmark_word {
	struct {
		uint32_t size : 24;
		uint32_t tag : 8;
	} header;
	uint8_t bytes[4];
	uint32_t word;
} dtrace_bookmark_word_t;

static volatile struct {
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
	uid = sceKernelAllocMemBlockForKernel("SceDeci4pDtracepPaReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
				      2 * PERFMON_PAGE_SIZE, &opt);
	if (uid < 0)
		return uid;
	s_bookmark.uid = uid;
	sceKernelGetMemBlockBase(uid, &base);
	s_bookmark.regs = base;
	return 0;
}

int dtrace_bookmark_fini(void)
{
	sceKernelFreeMemBlock(s_bookmark.uid);
	return 0;
}

void dtrace_print_bookmark_stats(void)
{
}

void dtrace_reset_bookmark_stats(void)
{
}

void dtrace_buffer_flush_bookmarks(dtrace_buffer_t *buf)
{
	dtrace_bookmark_word_t w;
	uint8_t *p, *end;
	uint32_t left;

	if (!(buf->dtb_flags & DTRACEBUF_HWTRACE))
		return;
	if (buf->dtb_hw_offset == buf->dtb_offset)
		return;
	if (buf->dtb_offset < buf->dtb_hw_offset) {
		DTRACE_LOG_ERROR_AT(128, "bad buffer state - dtb_offset:%lld < dtb_hw_offset:%lld",
				    buf->dtb_offset, buf->dtb_hw_offset);
		return;
	}

	left = buf->dtb_offset - buf->dtb_hw_offset;
	p = (uint8_t *)buf->dtb_tomax + buf->dtb_hw_offset;
	end = p + left;

	w.word = 0;
	w.header.tag = DTRACE_BOOKMARK_TAG;
	w.header.size = left;
	s_bookmark.regs->registerBank1[0] = w.word;

	while (p < end) {
		w.word = 0;
		if (left >= 4) {
			w.bytes[0] = p[0];
			w.bytes[1] = p[1];
			w.bytes[2] = p[2];
			w.bytes[3] = p[3];
			p += 4;
			left -= 4;
		} else if (left == 3) {
			w.bytes[0] = p[0];
			w.bytes[1] = p[1];
			w.bytes[2] = p[2];
			p += 3;
			left = 0;
		} else if (left == 2) {
			w.bytes[0] = p[0];
			w.bytes[1] = p[1];
			p += 2;
			left = 0;
		} else {
			w.bytes[0] = *p++;
			left--;
		}
		s_bookmark.regs->registerBank1[0] = w.word;
	}

	buf->dtb_offset = 0;
	buf->dtb_hw_offset = 0;
}
