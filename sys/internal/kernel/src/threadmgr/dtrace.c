/*
 * SceKernelThreadMgr: dtrace.c, per-thread DTrace state and cross-CPU calls
 * for the DTrace provider.
 */

#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#define DTRACE_STATE_SIZE		0x28
#define DTRACE_HEAP_SIZE		0x20000
#define THREAD_ID_LIST_ALL		0x00010021	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b

extern void *memset(void *dst, int c, SceSize n);
extern int ksceKernelGetThreadIdList(SceUID type, SceUID *ids, SceSize size, SceSize *count);
extern SceUID ksceKernelCreateHeap(const char *name, SceSize size, SceKernelHeapCreateOpt *opt);
void issueDTraceAsyncXCallIntrRequest(unsigned int cpuId, void (*fn)(void *), const void *arg,
				      SceSize size);
void issueDTraceSyncXCallIntrRequest(unsigned int cpuId, void (*fn)(void *), void *arg,
				     SceKernelSpinlock *done);

/* allocates a thread's DTrace state if it has none */
int dtraceThreadCBInit(struct ThreadCB *thread)
{
	SceDtraceThreadCB *state;
	int ret;

	ksceKernelSpinlockLowLock(&thread->dtraceLock);
	if (thread->dtraceState != NULL) {
		ret = 0;
		goto unlock;
	}
	state = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId,
					  DTRACE_STATE_SIZE);
	thread->dtraceState = state;
	if (state == NULL) {
		SCE_DBG_PRINTF(0, 113, "error: failed to allocate dtrace thread state for 0x%08x\n",
			       thread->threadId);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto unlock;
	}
	memset(state, 0, DTRACE_STATE_SIZE);
	thread->dtraceState->flags = DTRACE_THREAD_START;
	SCE_DBG_PRINTF_LEVEL(2, 0, 122, "debug: allocated dtrace thread state for 0x%08x\n",
			     thread->threadId);
	ksceKernelSpinlockLowUnlock(&thread->dtraceLock);
	return 0;
unlock:
	ksceKernelSpinlockLowUnlock(&thread->dtraceLock);
	return ret;
}

/* turns per-thread DTrace state on (allocating it for every thread) or off */
int sceKernelSetDTraceThreadCBEnable(int enable)
{
	struct ThreadmgrThreadObject *object;
	SceKernelHeapCreateOpt opt;
	SceSize count, i;
	SceUID *ids, id;
	int intr, ret;

	if (!enable) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 19, "debug: %s allocation of per-thread dtrace state\n",
				     "disabling");
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		s_threadmgrRuntime.manager->dtraceThreadCBEnable = 0;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(
			&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
		return 0;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 19, "debug: %s allocation of per-thread dtrace state\n",
			     "enabling");
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	s_threadmgrRuntime.manager->dtraceThreadCBEnable = enable;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	if (s_threadmgrRuntime.manager->dtraceHeapId == 0) {
		opt.size = sizeof(opt);
		opt.attr = 0x400;
		opt.memtype = 0x10f0d006;
		s_threadmgrRuntime.manager->dtraceHeapId =
			ksceKernelCreateHeap("SceDtraceThreadCB", DTRACE_HEAP_SIZE, &opt);
		if (s_threadmgrRuntime.manager->dtraceHeapId < 0)
			SCE_PANIC(38);
	}
	ret = ksceKernelGetThreadIdList(THREAD_ID_LIST_ALL, NULL, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 44, "error: failed to get thread id count (0x%08x)\n", ret);
		return ret;
	}
	count = ret;
	ids = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId,
					count * sizeof(SceUID));
	if (ids == NULL) {
		SCE_DBG_PRINTF(0, 51, "error: failed to allocate thread id list\n");
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	memset(ids, 0, count * sizeof(SceUID));
	ret = ksceKernelGetThreadIdList(THREAD_ID_LIST_ALL, ids, count * sizeof(SceUID), &count);
	if (ret < 0) {
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId, ids);
		SCE_DBG_PRINTF(0, 60, "error: failed to get thread id list (0x%08x)\n", ret);
		return ret;
	}
	for (i = 0; i < count; i++) {
		id = ids[i];
		ret = ksceGUIDReferObjectWithClass(id,
			s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 70, "error: failed to lookup thread 0x%08x (0x%08x)\n", id,
				       ret);
			continue;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = dtraceThreadCBInit(&object->context);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 79, "warning: failed to init dtrace thread state for thread "
				"id 0x%08x (0x%08x)\n", id, ret);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(id);
	}
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId, ids);
	return ret;
}

/* frees a thread's DTrace state */
int dtraceThreadCBExit(struct ThreadCB *thread)
{
	ksceKernelSpinlockLowLock(&thread->dtraceLock);
	if (thread->dtraceState != NULL) {
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->dtraceHeapId, thread->dtraceState);
		thread->dtraceState = NULL;
		SCE_DBG_PRINTF_LEVEL(2, 0, 145, "debug: deallocated dtrace thread state for 0x%08x\n",
				     thread->threadId);
	}
	ksceKernelSpinlockLowUnlock(&thread->dtraceLock);
	return 0;
}

/*
 * SceThreadmgrForKernel_0788C9BC: runs fn(arg) on cpuId, now when that is
 * this CPU, else later through an SGI with a copy of size bytes of arg.
 */
int SceThreadmgrForKernel_0788C9BC(unsigned int cpuId, void (*fn)(void *), void *arg, SceSize size)
{
	int intr, ret;

	if (cpuId >= 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		ret = 0;
		if (ksceKernelCpuId() == cpuId) {
			fn(arg);
		} else {
			ksceKernelCpuResumeIntr(intr);
			issueDTraceAsyncXCallIntrRequest(cpuId, fn, arg, size);
			intr = ksceKernelCpuSuspendIntr();
		}
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForKernel_DF59DF99: runs fn(arg) on cpuId and waits for it */
int SceThreadmgrForKernel_DF59DF99(unsigned int cpuId, void (*fn)(void *), void *arg)
{
	SceKernelSpinlock done;
	int intr, ret;

	if (cpuId >= 4)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else if (ksceKernelCpuId() == cpuId) {
		fn(arg);
		ret = 0;
	} else {
		done = 1;
		__sync_synchronize();
		issueDTraceSyncXCallIntrRequest(cpuId, fn, arg, &done);
		ret = 0;
		ksceKernelCpuResumeIntr(intr);
		ksceKernelSpinlockLowLock(&done);	/* until the target CPU unlocks it */
		intr = ksceKernelCpuSuspendIntr();
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* the SGI side of an async call */
void dtraceAsyncXCallIntr(void (*fn)(void *), void *arg)
{
	if (fn != NULL)
		fn(arg);
}

/* the SGI side of a sync call */
void dtraceSyncXCallIntr(void (*fn)(void *), void *arg, SceKernelSpinlock *done)
{
	if (fn != NULL)
		fn(arg);
	ksceKernelSpinlockLowUnlock(done);
}

/* SceThreadmgrForKernel_60798313 */
int SceThreadmgrForKernel_60798313(uint32_t *value)
{
	*value = s_threadmgrRuntime.manager->syslibtraceThreadCBEnable;
	return 0;
}
