/*
 * SceKernelThreadMgr: the running thread's VFP exception mask, kept on the
 * thread and in the word the VFP exception handler reads (VBAR+0x24).
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define VFP_EXCEPTION_ILLEGAL		0xf7ffff60	/* bits a caller may not change */
#define CHANGE_VFP_FUNC			"sceKernelChangeThreadVfpException"

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* ksceKernelChangeThreadVfpException: clear then set; returns the old mask */
uint32_t ksceKernelChangeThreadVfpException(uint32_t clear, uint32_t set)
{
	struct ThreadCB *thread = currentThreadContext();
	volatile uint32_t *word;
	SceKernelIntrStatus intr;
	uint32_t saved, previous, value;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 10725, CHANGE_VFP_FUNC, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (((clear | set) & VFP_EXCEPTION_ILLEGAL) || (clear & set)) {
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	SCE_DBG_ASSERT_CHECK_FUNC(thread != NULL, 10745, CHANGE_VFP_FUNC);
	intr = ksceKernelCpuSuspendIntr();
	word = currentVfpException();
	SCE_DBG_ASSERT_CHECK_FUNC(word != NULL, 10750, CHANGE_VFP_FUNC);
	previous = thread->vfpException;
	value = set | (previous & ~clear);
	thread->vfpException = value;
	*word = value;
	__asm__ volatile("dsb sy" : : : "memory");
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return previous;
}

/* sceKernelGetCurrentThreadVfpException: the running thread's mask */
uint32_t sceKernelGetCurrentThreadVfpException(void)
{
	struct ThreadCB *thread = currentThreadContext();
	uint32_t saved, value;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 10777, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	SCE_DBG_ASSERT_CHECK(thread != NULL, 10782);
	SCE_DBG_ASSERT_CHECK(thread->vfpException == *currentVfpException(), 10783);
	value = thread->vfpException;
	ThreadmgrSetUserRoWord(saved);
	return value;
}
