/*
 * SceKernelThreadMgr: whether a user thread's stack pointer is inside its
 * stack (the bounds the user TLS holds).
 */

#include "private.h"
#include "scheduler.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define USER_TLS_STACK_RANGE		0x7f8	/* below the user TLS: the stack top and base */
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000	/* older SDKs are not checked */

/* checkThreadUserStack: the free stack left (0: overflowed; 1: not checked) */
int checkThreadUserStack(struct ThreadCB *thread)
{
	uint32_t sp, bounds[2], version;

	SCE_DBG_ASSERT_CHECK(thread != NULL, 7710);
	SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 7711);
	SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_IN_CALLBACK), 7712);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 7713);
	if (thread->userFrame != NULL)
		sp = thread->userFrame->sp;
	else
		sp = thread->pRegisterContext->sp;
	if (ksceKernelCopyFromUserProc(thread->processId, bounds,
				       (void *)(thread->tls - USER_TLS_STACK_RANGE),
				       sizeof(bounds)) < 0)
		return 0;
	if (sp <= bounds[0] && bounds[1] < sp)
		return sp - bounds[1];
	if (thread->processId != THREADMGR_KERNEL_PID &&
	    (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0 ||
	     version < SDK_VERSION_DEFAULT_AFFINITY))
		return 1;
	if (sp > bounds[0])
		SCE_DBG_PRINTF(0, 7743, " k: checkThreadUserStack(), threadId=0x%08X[%s], "
			"sp=0x%08X, bottom=0x%08X, top=0x%08X\n", thread->threadId,
			ksceGUIDName(thread->threadId), sp, bounds[0], bounds[1]);
	else
		SCE_DBG_PRINTF(0, 7761, " k: checkThreadUserStack(), threadId=0x%08X[%s], "
			"sp=0x%08X, bottom=0x%08X, top=0x%08X\n", thread->threadId,
			ksceGUIDName(thread->threadId), sp, bounds[0], bounds[1]);
	return 0;
}
