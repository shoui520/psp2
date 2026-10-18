/*
 * SceKernelThreadMgr (threadmgr.c): thread signals. A thread sleeps in
 * waitSignal on the manager's signal object until another sends it a
 * signal; a signal sent to a thread not waiting stays pending (one deep).
 */

#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "priority_change.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include <threadmgr/uid_waitable.h>

int sceKernelWaitSignalCBForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout);
int sceKernelWaitSignalForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout);

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define WAIT_TYPE_SIGNAL		0x00000004
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c
#define SCE_KERNEL_ERROR_THREAD_SUSPENDED	0x8002802d
#define SCE_KERNEL_ERROR_SIGNAL_PENDING	0x80028121	/* PLACEHOLDER: name */
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */

int sceKernelCheckThreadUserStackSelf(void);
int callCallbackAll(struct ThreadCB *thread);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);
extern const char s_signalSleepCodeSite[];

/* _sceKernelWaitSignal: the timeout comes in and goes back out */
int _sceKernelWaitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSignalForUser(arg0, arg1, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSignalForUser(arg0, arg1, buf);
	err = sceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitSignalCB */
int _sceKernelWaitSignalCB(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSignalCBForUser(arg0, arg1, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSignalCBForUser(arg0, arg1, buf);
	err = sceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}
