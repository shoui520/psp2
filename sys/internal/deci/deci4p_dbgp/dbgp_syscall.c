/*
 * The debugger's syscalls for test programs: suspend or resume an
 * application through SceAppMgr's debugger interface, from the global work
 * queue, and wait until the process has (Sony's dbgp_syscall.c).
 */

#include <acmgr/acmgr.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_rdrfp.h>
#include <deci4p_dbgp/dbgp.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <processmgr/processmgr.h>
#include <sceerror.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>

static int _suspendAppTask(void *arg)
{
	SceUID pid = *(SceUID *)arg;

	sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_TOOL_ID, arg);
	return sceAppMgrSuspendAppForDebugger(pid);
}

static int _resumeAppTask(void *arg)
{
	SceUID pid = *(SceUID *)arg;

	sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_TOOL_ID, arg);
	return sceAppMgrResumeAppForDebugger(pid);
}

int sceDeci4pDbgpSuspendAppForTestForUser(SceUID pid)
{
	SceUInt32 tls;
	SceUID *arg;
	int ret;

	tls = sceSyscallEnter();
	if (SceSblACMgrForDriver_AD717E7A(0) == 0) {
		sceSyscallLeave(tls);
		return (int)SCE_APPMGR_ERROR_NOEXEC;
	}
	arg = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_TOOL_ID, sizeof(*arg));
	if (arg == NULL) {
		sceSyscallLeave(tls);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	*arg = pid;
	ret = sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID,
					"SceDeci4pDbgpSuspendApp", _suspendAppTask,
					arg);
	if (ret < 0)
		goto out;
	ret = SceThreadmgrForDriver_12FC0FAB(ret, NULL);
	if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED) {
		sceSyscallLeave(tls);
		return SCE_KERNEL_ERROR_THREAD_STOPPED;
	}
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(94);
		goto out;
	}
	ret = sceKernelWaitProcessEventCompletion(pid, PROCESS_EVF_SUSPENDED, NULL);
	if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED) {
		sceSyscallLeave(tls);
		return SCE_KERNEL_ERROR_THREAD_STOPPED;
	}
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(104);
		goto out;
	}
	sceSyscallLeave(tls);
	return 0;

out:
	sceSyscallLeave(tls);
	return ret;
}

int sceDeci4pDbgpResumeAppForTestForUser(SceUID pid)
{
	SceUInt32 tls;
	SceUID *arg;
	int ret;

	tls = sceSyscallEnter();
	if (SceSblACMgrForDriver_AD717E7A(0) == 0) {
		sceSyscallLeave(tls);
		return (int)SCE_APPMGR_ERROR_NOEXEC;
	}
	arg = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_TOOL_ID, sizeof(*arg));
	if (arg == NULL) {
		sceSyscallLeave(tls);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	*arg = pid;
	ret = sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID,
					"SceDeci4pDbgpResumeApp", _resumeAppTask,
					arg);
	if (ret < 0)
		goto out;
	ret = SceThreadmgrForDriver_12FC0FAB(ret, NULL);
	if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED) {
		sceSyscallLeave(tls);
		return SCE_KERNEL_ERROR_THREAD_STOPPED;
	}
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(147);
		goto out;
	}
	ret = sceKernelWaitProcessEventCompletion(pid, PROCESS_EVF_RESUMED, NULL);
	if (ret == SCE_KERNEL_ERROR_THREAD_STOPPED) {
		sceSyscallLeave(tls);
		return SCE_KERNEL_ERROR_THREAD_STOPPED;
	}
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(157);
		goto out;
	}
	sceSyscallLeave(tls);
	return 0;

out:
	sceSyscallLeave(tls);
	return ret;
}

int SceDeci4pDbgp_6180DC1A(SceUID pid, SceUInt32 *timeout)
{
	SceUInt32 tls, t;
	int ret;

	tls = sceSyscallEnter();
	if (SceSblACMgrForDriver_AD717E7A(0) == 0) {
		ret = (int)SCE_APPMGR_ERROR_NOEXEC;
	} else if (timeout == NULL) {
		ret = sceKernelWaitProcessEventCompletion(pid, PROCESS_EVF_SUSPENDED,
							  NULL);
	} else {
		ret = sceKernelCopyFromUser(&t, timeout, sizeof(t));
		if (ret >= 0) {
			ret = sceKernelWaitProcessEventCompletion(pid,
				PROCESS_EVF_SUSPENDED, &t);
			if (ret >= 0)
				ret = sceKernelCopyToUser(timeout, &t, sizeof(t));
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int SceDeci4pDbgp_073A6B4B(SceUID pid, SceUInt32 *timeout)
{
	SceUInt32 tls, t;
	int ret;

	tls = sceSyscallEnter();
	if (SceSblACMgrForDriver_AD717E7A(0) == 0) {
		ret = (int)SCE_APPMGR_ERROR_NOEXEC;
	} else if (timeout == NULL) {
		ret = sceKernelWaitProcessEventCompletion(pid, PROCESS_EVF_RESUMED,
							  NULL);
	} else {
		ret = sceKernelCopyFromUser(&t, timeout, sizeof(t));
		if (ret >= 0) {
			ret = sceKernelWaitProcessEventCompletion(pid,
				PROCESS_EVF_RESUMED, &t);
			if (ret >= 0)
				ret = sceKernelCopyToUser(timeout, &t, sizeof(t));
		}
	}
	sceSyscallLeave(tls);
	return ret;
}
