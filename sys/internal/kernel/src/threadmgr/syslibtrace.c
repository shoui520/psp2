/*
 * SceKernelThreadMgr: syslibtrace.c, the per-thread state of the system
 * library tracer. It lives in the DTrace heap (dtrace.c creates it).
 */

#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#define THREAD_ID_LIST_ALL		0x00010021	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b

extern void *memset(void *dst, int c, SceSize n);
extern int ksceKernelGetThreadIdList(SceUID type, SceUID *ids, SceSize size, SceSize *count);

/* allocates a thread's syslibtrace state if it has none (its lock held) */
int syslibtraceThreadCBInit(struct ThreadCB *thread)
{
	SyslibtraceThreadCB *state;

	if (thread->syslibtraceState != NULL)
		return 0;
	state = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId,
					  sizeof(*state));
	thread->syslibtraceState = state;
	if (state == NULL) {
		SCE_DBG_PRINTF(0, 96,
			"error: failed to allocate syslibtrace thread state for 0x%08x\n",
			thread->threadId);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	*state = (SyslibtraceThreadCB){ 0 };
	SCE_DBG_PRINTF_LEVEL(2, 0, 102, "debug: allocated syslibtrace thread state for 0x%08x\n",
			     thread->threadId);
	return 0;
}

/* turns per-thread syslibtrace state on (allocating it for every thread) or off */
int sceKernelSetSyslibtraceThreadCBEnable(int enable)
{
	struct ThreadmgrThreadObject *object;
	SceSize count, i;
	SceUID *ids, id;
	int intr, ret;

	SCE_DBG_PRINTF_LEVEL(2, 0, 19, "debug: %s allocation of per-thread syslibtrace state\n",
			     enable ? "enabling" : "disabling");
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	s_threadmgrRuntime.manager->syslibtraceThreadCBEnable = enable;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	if (!enable)
		return 0;
	ret = ksceKernelGetThreadIdList(THREAD_ID_LIST_ALL, NULL, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 32, "error: failed to get thread id count (0x%08x)\n", ret);
		return ret;
	}
	count = ret;
	ids = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId,
					count * sizeof(SceUID));
	if (ids == NULL) {
		SCE_DBG_PRINTF(0, 39, "error: failed to allocate thread id list\n");
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	memset(ids, 0, count * sizeof(SceUID));
	ret = ksceKernelGetThreadIdList(THREAD_ID_LIST_ALL, ids, count * sizeof(SceUID), &count);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 47, "error: failed to get thread id list (0x%08x)\n", ret);
		goto free;
	}
	for (i = 0; i < count; i++) {
		id = ids[i];
		ret = ksceGUIDReferObjectWithClass(id,
			s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 58, "error: failed to lookup thread 0x%08x (0x%08x)\n", id,
				       ret);
			continue;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = syslibtraceThreadCBInit(&object->context);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 67, "warning: failed to init syslibtrace thread state for "
				"thread id 0x%08x (0x%08x)\n", id, ret);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(id);
	}
free:
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId, ids);
	return ret;
}

/* frees a thread's syslibtrace state (its lock held) */
int syslibtraceThreadCBExit(struct ThreadCB *thread)
{
	if (thread->syslibtraceState != NULL) {
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId,
					 thread->syslibtraceState);
		thread->syslibtraceState = NULL;
		SCE_DBG_PRINTF_LEVEL(2, 0, 117,
			"debug: deallocated syslibtrace thread state for 0x%08x\n",
			thread->threadId);
	}
	return 0;
}
