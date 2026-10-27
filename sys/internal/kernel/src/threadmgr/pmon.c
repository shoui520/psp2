/*
 * SceKernelThreadMgr: pmon.c, per-thread PMU (performance monitor) state.
 * A thread's counters live in its register context while it is switched
 * out; the running thread's are in the CPU. Another CPU's running thread
 * is reached with an SGI and a request the caller waits on.
 */

#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "syscall_context.h"
#include <intrmgr/intrmgr.h>

#define PMON_COUNTER_MASK		0x8000003f	/* event counters 0..5 and the cycle counter */
#define PMON_CYCLE_COUNTER		31
#define PMON_EVENT_COUNTERS		6

static inline int PmonCounterValid(uint32_t counter)
{
	return counter < 32 && ((PMON_COUNTER_MASK >> counter) & 1);
}

/* the thread this CPU runs (VBAR's CPU state) */
static inline struct ThreadCB *PmonCpuThread(void)
{
	return (struct ThreadCB *)currentCpuState()->pRunningThreadCB;
}

static inline void PmonWriteSelect(uint32_t counter)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(counter));
}

static inline void PmonWriteEventType(uint32_t type)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(type));
}

static inline uint32_t PmonReadEventCount(void)
{
	uint32_t value;

	__asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(value));
	return value;
}

static inline void PmonWriteEventCount(uint32_t value)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(value));
}

static inline uint32_t PmonReadCycleCount(void)
{
	uint32_t value;

	__asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(value));
	return value;
}

static inline void PmonWriteCycleCount(uint32_t value)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 0" : : "r"(value));
}

static inline uint32_t PmonReadEnableSet(void)
{
	uint32_t value;

	__asm__ volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(value));
	return value;
}

static inline void PmonWriteEnableSet(uint32_t mask)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(mask));
}

static inline void PmonWriteEnableClear(uint32_t mask)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(mask));
}

static inline void PmonWriteControl(uint32_t value)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(value));
}

static inline void PmonWriteUserEnable(uint32_t value)
{
	__asm__ volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(value));
}

/* selects the event an event counter counts (the caller's thread, or one switched out) */
int pmonCpuSetEventSelf(SceUID threadId, uint32_t counter, uint32_t eventType)
{
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	struct ThreadCB *thread;
	int inIntr, intr, ret;

	inIntr = sceKernelIsIntrContext();
	if (counter >= PMON_EVENT_COUNTERS)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (inIntr && threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	intr = sceKernelCpuSuspendIntr();
	thread = PmonCpuThread();
	if (threadId == 0 || thread->threadId == threadId) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->pRegisterContext->pmevent[counter].eventType = eventType;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
		if (!inIntr) {
			SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 67);
			PmonWriteSelect(counter);
			PmonWriteEventType(eventType);
			ret = 0;
		}
	} else if (sceGUIDReferObjectWithClass(threadId,
			s_threadmgrRuntime.manager->threadClass,
			&pObject) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		object = (struct SceUIDThreadObject *)pObject;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		object->context.pRegisterContext->pmevent[counter].eventType = eventType;
		if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING)
			addIntrRequest(object->context.currentCpuId, INTR_MASK_SIMPLE);
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForKernel_6DC1BE7A */
int SceThreadmgrForKernel_6DC1BE7A(void)
{
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* turns a thread's counters (mask) on or off; interrupts are already off */
int pmonThreadChangeEnableCounter(SceUID threadId, uint32_t mask, int enable)
{
	struct ThreadCB *thread = PmonCpuThread();
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	uint32_t enabled;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 190);
	mask &= PMON_COUNTER_MASK;
	if (threadId == 0 || thread->threadId == threadId) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		enabled = PmonReadEnableSet();
		if (!enable) {
			thread->pRegisterContext->pmcntenset = enabled & ~mask;
			if (!sceKernelIsIntrContext())
				PmonWriteEnableClear(mask);
		} else {
			thread->pRegisterContext->pmcntenset = enabled | mask;
			if (!sceKernelIsIntrContext())
				PmonWriteEnableSet(mask);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		return 0;
	}
	if (sceGUIDReferObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
					 &pObject) < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	object = (struct SceUIDThreadObject *)pObject;
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING)
		issuePMonThreadChangeCounterEnableIntrRequest(object->context.currentCpuId, threadId,
							      mask, enable);
	else if (!enable)
		object->context.pRegisterContext->pmcntenset &= ~mask;
	else
		object->context.pRegisterContext->pmcntenset |= mask;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
	return 0;
}

/* sceKernelPMonThreadSetEnableCounter: enables a thread's counters */
int sceKernelPMonThreadSetEnableCounter(SceUID threadId, uint32_t mask)
{
	int intr, ret;

	if (sceKernelIsIntrContext() && threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	intr = sceKernelCpuSuspendIntr();
	ret = pmonThreadChangeEnableCounter(threadId, mask, 1);
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* sceKernelPMonThreadClearEnableCounter: disables a thread's counters */
int sceKernelPMonThreadClearEnableCounter(SceUID threadId, uint32_t mask)
{
	int intr, ret;

	if (sceKernelIsIntrContext() && threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	intr = sceKernelCpuSuspendIntr();
	ret = pmonThreadChangeEnableCounter(threadId, mask, 0);
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForKernel_F46783E2 */
int SceThreadmgrForKernel_F46783E2(void)
{
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* the SGI side of a set: the target thread may have moved on again */
void pmonThreadSetCounterIntr(struct ThreadmgrPmonRequest *request)
{
	struct ThreadCB *thread;
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	SceUID threadId = request->threadId;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 358);
	SCE_DBG_ASSERT_CHECK(PmonCounterValid(request->counter), 360);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 362);
	thread = PmonCpuThread();
	if (thread != NULL && PmonCpuThread()->threadId == threadId) {
		thread = PmonCpuThread();
		if (request->counter == PMON_CYCLE_COUNTER)
			thread->pRegisterContext->pmccntr = request->value;
		else
			thread->pRegisterContext->pmevent[request->counter].count = request->value;
		ret = 0;
	} else {
		ret = sceGUIDReferObjectWithClass(threadId,
			s_threadmgrRuntime.manager->threadClass, &pObject);
		if (ret < 0) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			goto done;
		}
		object = (struct SceUIDThreadObject *)pObject;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			issuePMonThreadSetCounterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return;
		}
		if (request->counter == PMON_CYCLE_COUNTER)
			object->context.pRegisterContext->pmccntr = request->value;
		else
			object->context.pRegisterContext->pmevent[request->counter].count = request->value;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
}

/* sceKernelPMonThreadSetCounter: sets a counter (31: the cycle counter) of a thread */
int sceKernelPMonThreadSetCounter(SceUID threadId, uint32_t counter, uint32_t value)
{
	struct ThreadCB *thread;
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	struct ThreadmgrPmonRequest request;
	SceKernelSpinlock done;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (!PmonCounterValid(counter)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	if (sceKernelIsIntrContext() && threadId == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = sceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	thread = PmonCpuThread();
	if (threadId == 0 || thread->threadId == threadId) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (counter == PMON_CYCLE_COUNTER) {
			thread->pRegisterContext->pmccntr = value;
			if (!sceKernelIsIntrContext())
				PmonWriteCycleCount(value);
		} else {
			thread->pRegisterContext->pmevent[counter].count = value;
			if (!sceKernelIsIntrContext()) {
				PmonWriteSelect(counter);
				PmonWriteEventCount(value);
			}
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	object = (struct SceUIDThreadObject *)pObject;
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		request.threadId = threadId;
		request.counter = counter;
		request.value = value;
		done = 1;
		request.result = &ret;
		request.done = &done;
		__sync_synchronize();
		issuePMonThreadSetCounterIntrRequest(object->context.currentCpuId, &request);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
		sceKernelCpuResumeIntr(intr);
		sceKernelSpinlockLowLock(&done);	/* until the target CPU unlocks it */
		goto restore;
	}
	if (counter == PMON_CYCLE_COUNTER)
		object->context.pRegisterContext->pmccntr = value;
	else
		object->context.pRegisterContext->pmevent[counter].count = value;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the SGI side of a get */
void pmonThreadGetCounterIntr(struct ThreadmgrPmonRequest *request)
{
	struct ThreadCB *thread;
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	SceUID threadId = request->threadId;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 546);
	SCE_DBG_ASSERT_CHECK(PmonCounterValid(request->counter), 548);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 550);
	thread = PmonCpuThread();
	if (thread != NULL && PmonCpuThread()->threadId == threadId) {
		thread = PmonCpuThread();
		ret = 0;
		if (request->counter == PMON_CYCLE_COUNTER)
			*(uint32_t *)request->value = thread->pRegisterContext->pmccntr;
		else
			*(uint32_t *)request->value = thread->pRegisterContext->pmevent[request->counter].count;
	} else {
		ret = sceGUIDReferObjectWithClass(threadId,
			s_threadmgrRuntime.manager->threadClass, &pObject);
		if (ret < 0) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			goto done;
		}
		object = (struct SceUIDThreadObject *)pObject;
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			issuePMonThreadGetCounterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return;
		}
		if (request->counter == PMON_CYCLE_COUNTER)
			*(uint32_t *)request->value = object->context.pRegisterContext->pmccntr;
		else
			*(uint32_t *)request->value = object->context.pRegisterContext->pmevent[request->counter].count;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
}

/* SceThreadmgrForKernel_170F69D6: reads a counter (31: the cycle counter) of a thread */
int SceThreadmgrForKernel_170F69D6(SceUID threadId, uint32_t counter, uint32_t *value)
{
	struct ThreadCB *thread;
	struct SceUIDThreadObject *object;
	SceObjectBase *pObject;
	struct ThreadmgrPmonRequest request;
	SceKernelSpinlock done;
	uint32_t saved;
	int inIntr, intr, ret;

	inIntr = sceKernelIsIntrContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (!PmonCounterValid(counter)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	if (value == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (inIntr && threadId == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = sceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	if (inIntr && PmonCpuThread()->threadId == threadId) {
		/* the interrupted thread: its saved registers */
		thread = PmonCpuThread();
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (counter == PMON_CYCLE_COUNTER)
			*value = thread->pRegisterContext->pmccntr;
		else
			*value = thread->pRegisterContext->pmevent[counter].count;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
		goto resume;
	}
	if (threadId == 0 || PmonCpuThread()->threadId == threadId) {
		if (counter == PMON_CYCLE_COUNTER) {
			*value = PmonReadCycleCount();
		} else {
			PmonWriteSelect(counter);
			*value = PmonReadEventCount();
		}
		ret = 0;
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	object = (struct SceUIDThreadObject *)pObject;
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		request.threadId = threadId;
		request.counter = counter;
		request.value = (uintptr_t)value;
		done = 1;
		request.result = &ret;
		request.done = &done;
		__sync_synchronize();
		issuePMonThreadGetCounterIntrRequest(object->context.currentCpuId, &request);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
		sceKernelCpuResumeIntr(intr);
		sceKernelSpinlockLowLock(&done);	/* until the target CPU unlocks it */
		goto restore;
	}
	if (counter == PMON_CYCLE_COUNTER)
		*value = object->context.pRegisterContext->pmccntr;
	else
		*value = object->context.pRegisterContext->pmevent[counter].count;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Sets PMUSERENR in every thread of a process (and the caller's CPU when
 * it is the caller's process), then has the other CPUs reload theirs.
 */
int sceKernelPMonSetUserEnableRegister(SceUID pid, uint32_t value)
{
	struct ThreadCB *thread;
	SceProcessObject *process;
	ListNode *link;
	int intr, ret;

	if (currentThreadContext() == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (ThreadmgrIntrDisabled()) {
		SCE_DBG_ASSERT_PRINTF(0, 833, "*** error: irq suspended.\n");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	if (pid == 0) {
		process = currentThreadContext()->process;
		ret = 0;
	} else {
		ret = sceGUIDGetObjectWithClass(pid,
			s_threadmgrRuntime.manager->processClass, (SceObjectBase **)&process);
		if (ret < 0)
			return ret;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->lock);
	for (link = process->threads.next; link != &process->threads; link = link->next) {
		thread = THREAD_FROM_PROCESS_LINK(link);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->pRegisterContext != NULL) {
			thread->pRegisterContext->pmuserenr = value;
			if (thread->pRegisterContext->callbackContext != NULL)
				thread->pRegisterContext->callbackContext->pmuserenr = value;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	if (pid == 0)
		PmonWriteUserEnable(value);
	issueSimpleIntrRequestToOtherCpuAndWait();
	return ret;
}

/* the same for PMCR */
int sceKernelPMonSetControlRegister(SceUID pid, uint32_t value)
{
	struct ThreadCB *thread;
	SceProcessObject *process;
	ListNode *link;
	int intr, ret;

	if (currentThreadContext() == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (ThreadmgrIntrDisabled()) {
		SCE_DBG_ASSERT_PRINTF(0, 904, "*** error: irq suspended.\n");
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	if (pid == 0) {
		process = currentThreadContext()->process;
		ret = 0;
	} else {
		ret = sceGUIDGetObjectWithClass(pid,
			s_threadmgrRuntime.manager->processClass, (SceObjectBase **)&process);
		if (ret < 0)
			return ret;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->lock);
	for (link = process->threads.next; link != &process->threads; link = link->next) {
		thread = THREAD_FROM_PROCESS_LINK(link);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->pRegisterContext != NULL) {
			thread->pRegisterContext->pmcr = value;
			if (thread->pRegisterContext->callbackContext != NULL)
				thread->pRegisterContext->callbackContext->pmcr = value;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	if (pid == 0)
		PmonWriteControl(value);
	issueSimpleIntrRequestToOtherCpuAndWait();
	return ret;
}
