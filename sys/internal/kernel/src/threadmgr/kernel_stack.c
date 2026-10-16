/*
 * SceKernelThreadMgr (threadmgr.c): running a function on a bigger kernel
 * stack. What fits in the thread's free stack runs there; otherwise a new
 * stack ("SceExt", or one from the per-CPU cache) stands in for the
 * thread's stack for the call.
 */

#include <string.h>
#include "private.h"
#include "sce/intrmgr.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREAD_ATTR_UNK10000000		0x10000000	/* PLACEHOLDER: its stack is only measured with DIPSW 0xd8 */
#define DIPSW_STACK_CHECK		0xd8
#define EXTEND_STACK_MIN			0x200
#define STACK_PAGE			0x1000
#define STACK_SAFETY			0x10		/* kept free below the caller */
#define STACK_LOW_WARNING		0x400
#define STACK_LOW_PANIC			0x300
#define STACK_CACHED_SIZE		0x2000		/* the size the per-CPU cache holds */
#define MEMBLOCK_TYPE_KERNEL_STACK	0x1020d006	/* PLACEHOLDER: name */
#define MEMBLOCK_ATTR_EXT_STACK		((4 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define TRACE_EVENT_EXTEND_STACK	0x10
#define TRACE_EVENT_EXTEND_STACK_END	0x11
#define SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE	0x80028024

typedef int64_t (*ThreadmgrStackFunction)(void *arg);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelGetMemBlockMappedBase(SceUID uid, void **base);
extern int ksceKernelFreeMemBlock(SceUID uid);
extern int ksceKernelCheckDipsw(unsigned int bit);
/* kernel_stack.S */
extern int64_t callWithStack(void *arg, uintptr_t stackTop, int *intrState,
			     ThreadmgrStackFunction function);

/* The early query is emitted separately in kernel_stack_space.c. */
SceSize SceThreadmgrForDriver_D74F66BD(void);

/*
 * sceKernelExtendKernelStackWide: function(arg) runs with at least size
 * bytes of kernel stack; returns what it returns.
 */
int64_t sceKernelExtendKernelStackWide(SceSize size, ThreadmgrStackFunction function, void *arg)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrExtendedStack frame, *saved;
	SceKernelAllocMemBlockKernelOpt opt;
	ThreadmgrTraceHook trace;
	SceSize free, pages, oldSize;
	SceUID memBlockId;
	void *base;
	int64_t ret;
	int intr, err, cached;

	free = SceThreadmgrForDriver_D74F66BD();
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_EXTEND_STACK, 5, thread->processId, thread->threadId, function,
		      size, free);
	if (free >= STACK_SAFETY)
		free -= STACK_SAFETY;
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 5368, "*** error: illegal context\n");
		ret = (int)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto out;
	}
	if (size < EXTEND_STACK_MIN) {
		ret = (int)SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
		goto out;
	}
	pages = (size + STACK_PAGE - 1) & ~(STACK_PAGE - 1);
	if (pages <= free) {
		/* it fits where we are */
		intr = ksceKernelCpuSuspendIntr();
		oldSize = thread->kStackSize;
		thread->kStackSize = pages;
		ret = callWithStack(arg, thread->pKStack + pages, &intr, function);
		thread->kStackSize = oldSize;
		ksceKernelCpuResumeIntr(intr);
		goto out;
	}
	SCE_DBG_ASSERT_PRINTF(free >= STACK_LOW_WARNING, 5394,
		"*** sceKernelExtendKernelStackWide(), Remaining stack size = 0x%08X\n", free);
	cached = 0;
	if (size == STACK_CACHED_SIZE &&
	    ((int)thread->currentAttr < 0 ||
	     thread->stackMemBlockType == MEMBLOCK_TYPE_KERNEL_STACK)) {
		memBlockId = popExtendedStackCache(&base);
		cached = 1;
		if (memBlockId > 0)
			goto have;
	}
	if (free < STACK_LOW_PANIC)
		SCE_PANIC(SCE_LINE(5407, 5413));
	memset(&opt, 0, sizeof(opt));
	opt.extraLow = STACK_PAGE;	/* a guard page below */
	opt.attr = MEMBLOCK_ATTR_EXT_STACK;
	opt.size = sizeof(opt);
	memBlockId = ksceKernelAllocMemBlock("SceExt",
		(int)thread->currentAttr < 0 ? MEMBLOCK_TYPE_KERNEL_STACK : thread->stackMemBlockType,
		pages + STACK_PAGE, &opt);
	if (memBlockId < 0) {
		ret = memBlockId;
		goto out;
	}
	ksceKernelGetMemBlockMappedBase(memBlockId, &base);
have:
	if (!(thread->currentAttr & THREAD_ATTR_UNK10000000) || ksceKernelCheckDipsw(DIPSW_STACK_CHECK))
		memset(base, 0xff, pages);
	*(SceUID *)base = thread->threadId;
	/* the new stack stands in for the thread's while the function runs */
	intr = ksceKernelCpuSuspendIntr();
	frame.stackBase = thread->pKStack;
	frame.memBlockId = thread->kStackMemblkId;
	frame.next = thread->extendedStacks;
	thread->pKStack = (uintptr_t)base;
	frame.stackSize = thread->kStackSize;
	thread->extendedStacks = &frame;
	thread->kStackSize = pages;
	thread->kStackMemblkId = memBlockId;
	ret = callWithStack(arg, (uintptr_t)base + pages, &intr, function);
	saved = thread->extendedStacks;
	thread->extendedStacks = saved->next;
	thread->pKStack = saved->stackBase;
	thread->kStackSize = saved->stackSize;
	thread->kStackMemblkId = saved->memBlockId;
	ksceKernelCpuResumeIntr(intr);
	if (!cached || pushExtendedStackCache(memBlockId, base) < 0) {
		err = ksceKernelFreeMemBlock(memBlockId);
		if (err < 0) {
			SCE_DBG_ASSERT_FAIL(5478);
			ret = err;
		}
	}
out:
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_EXTEND_STACK_END, 4, thread->processId, thread->threadId,
		      (uint32_t)(ret >> 32), (uint32_t)ret);
	return ret;
}

/* ksceKernelRunWithStack */
int64_t ksceKernelRunWithStack(SceSize size, ThreadmgrStackFunction function, void *arg)
{
	return sceKernelExtendKernelStackWide(size, function, arg);
}
