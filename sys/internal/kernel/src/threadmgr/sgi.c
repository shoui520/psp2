/* Interrupt-request queues and masks recovered from native sgi.c. */
#include <string.h>
#include "private.h"
#include <intrmgr/intrmgr.h>
#include <threadmgr/uid_waitable.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include "fast_mutex.h"
#include "priority_change.h"
#include "scheduler.h"
#include <sysmem/sysmem.h>
#include "syscall_context.h"
#include "event.h"
#include "timer.h"
#include "affinity.h"

static inline struct SceKernelThreadmgrPrivate *loadIntrRequestManager(void)
{
	struct SceKernelThreadmgrPrivate *volatile *slot;

	slot = &s_threadmgrRuntime.manager;
	return *slot;
}

static inline int IntrDisabled(void)
{
	uint32_t cpsr;

	__asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
	return (cpsr >> 7) & 1;
}

/* Native callers cross these function frames; keep their entry LR observable. */
__attribute__((noinline))
void issueIntrRequest(unsigned int cpuId, struct ThreadmgrIntrRequest *request)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrIntrRequest *oldHead;

	cpu = &loadIntrRequestManager()->cpuCB[cpuId];
	if (cpuId > 3) {
		SCE_DBG_PRINTF(0, 399,
			      "*** fatal error: issueIntrRequest(), cpuId=0x%08X\n",
			      cpuId);
		SCE_PANIC(SCE_LINE(401, 400));
	}
	do {
		oldHead = cpu->intrRequestHead;
		request->next = oldHead;
	} while (SceCpuForDriver_CDA96E81(
		 (SceInt32 *)&cpu->intrRequestHead, (intptr_t)oldHead,
		 (intptr_t)request) != (intptr_t)oldHead);
	if (!oldHead && sceKernelGenerateSoftIntr(2, 0, 1u << cpuId) < 0) {
		SCE_DBG_PRINTF(0xb, 420,
			      " fatal error: scheduling request SGI failed.\n");
		SCE_PANIC(SCE_LINE(422, 421));
	}
}

__attribute__((noinline))
struct ThreadmgrIntrRequest *allocIntrRequest(uint32_t kind)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadmgrIntrRequest *request;

	for (;;) {
		manager = loadIntrRequestManager();
		request = manager->freeIntrRequests;
		if (!request) {
			request = sceKernelAllocHeapMemory(manager->heapId, sizeof(*request));
			if (!request) {
				SCE_DBG_PRINTF(0xb, 867,
					      "*** fatal error: can not alloc intr request.\n");
				SCE_PANIC(SCE_LINE(869, 868));
			}
			request->fromPool = 0;
			break;
		}
		if (SceCpuForDriver_CDA96E81(
		    (SceInt32 *)&manager->freeIntrRequests, (intptr_t)request,
		    (intptr_t)request->next) == (intptr_t)request)
			break;
	}
	request->kind = kind;
	return request;
}

__attribute__((noinline))
void addIntrRequest(unsigned int cpuId, uint32_t mask)
{
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrTraceHook hook;

	cpu = &loadIntrRequestManager()->cpuCB[cpuId];
	SCE_DBG_ASSERT_CHECK(IntrDisabled(), 437);
	if (cpuId > 3) {
		SCE_DBG_PRINTF(0, 440,
			      "*** fatal error: addIntrRequest(), cpuId=0x%08X\n",
			      cpuId);
		SCE_PANIC(SCE_LINE(442, 441));
	}
	hook = s_threadmgrRuntime.schedTraceHook;
	if (hook)
		hook(61 - (mask ? __builtin_clz(mask) : 32), 1, cpuId);
	if (SceCpuForDriver_2A40BB93((SceInt32 *)&cpu->intrRequestMask,
				    mask) == 0 &&
	    sceKernelGenerateSoftIntr(2, 0, 1u << cpuId) < 0) {
		SCE_DBG_PRINTF(0xb, 453,
			      " fatal error: scheduling request SGI failed.\n");
		SCE_PANIC(SCE_LINE(455, 454));
	}
}

void SceThreadmgrForKernel_8D53CA9B(unsigned int cpuId)
{
	SceKernelIntrStatus intrState;

	intrState = SceCpuForDriver_821FC0EE();
	addIntrRequest(cpuId, INTR_MASK_SIMPLE);
	sceKernelCpuResumeIntr(intrState);
}

/* SGI kind 0x10: change another CPU's running thread's PMU counter enables */
void issuePMonThreadChangeCounterEnableIntrRequest(unsigned int cpuId, SceUID threadId,
						   uint32_t mask, int enable)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_PMON_THREAD_CHANGE_COUNTER_ENABLE);
	ThreadmgrTraceHook hook = s_threadmgrRuntime.schedTraceHook;

	request->payload[0] = threadId;
	request->payload[1] = mask;
	request->payload[2] = enable;
	if (hook != NULL)
		hook(0x25, 3, threadId, mask, enable);
	issueIntrRequest(cpuId, request);
}

/* SGI kind 0x11: set a PMU counter of another CPU's running thread (request: pmon.c) */
void issuePMonThreadSetCounterIntrRequest(unsigned int cpuId, const void *pmonRequest)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_PMON_THREAD_SET_COUNTER);
	const struct ThreadmgrPmonRequest *counter = pmonRequest;

	request->payload[0] = (uintptr_t)pmonRequest;
	if (s_threadmgrRuntime.schedTraceHook)
		(s_threadmgrRuntime.schedTraceHook)(0x26, 3, counter->threadId,
			counter->counter, counter->value);
	issueIntrRequest(cpuId, request);
}

/* SGI kind 0x12: read a PMU counter of another CPU's running thread */
void issuePMonThreadGetCounterIntrRequest(unsigned int cpuId, const void *pmonRequest)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_PMON_THREAD_GET_COUNTER);
	const struct ThreadmgrPmonRequest *counter = pmonRequest;

	request->payload[0] = (uintptr_t)pmonRequest;
	if (s_threadmgrRuntime.schedTraceHook)
		(s_threadmgrRuntime.schedTraceHook)(0x27, 3, counter->counter,
			counter->value, counter->done);
	issueIntrRequest(cpuId, request);
}

static inline int IntrEnabled(void)
{
	uint32_t cpsr;

	__asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
	return ((cpsr ^ 0x80) >> 7) & 1;
}

/*
 * SGI kind 3 to every other CPU; each answers by setting its bit in ack.
 * Returns once all four CPUs have.
 */
void issueSimpleIntrRequestToOtherCpuAndWait(void)
{
	struct ThreadmgrIntrRequest *request;
	volatile uint32_t ack = 0;
	unsigned int self, cpu;
	int intr;

	SCE_DBG_ASSERT_CHECK(IntrEnabled(), 526);
	intr = SceCpuForDriver_821FC0EE();
	cpu = 0;
	self = sceKernelCpuId();
	ack = 1 << self;
	do {
		if (cpu != self) {
			request = allocIntrRequest(INTR_REQUEST_ACKNOWLEDGE);
			request->payload[0] = (uintptr_t)&ack;
			issueIntrRequest(cpu, request);
		}
	} while (++cpu != 4);
	sceKernelCpuResumeIntr(intr);
	while (ack != 0xf)
		;
}

/*
 * SGI kind 0x18: run fn on cpuId later, with a heap copy of size bytes of
 * arg (freed by the handler).
 */
void issueDTraceAsyncXCallIntrRequest(unsigned int cpuId, void (*fn)(void *), const void *arg,
				      SceSize size)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_DTRACE_ASYNC_XCALL);
	void *copy;

	if (arg == NULL || size == 0) {
		copy = NULL;
	} else {
		copy = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, size);
		if (copy == NULL) {
			SCE_DBG_PRINTF(0xb, 782,
				" fatal error: can not alloc intr request argument.\n");
			SCE_PANIC(SCE_LINE(784, 783));
		}
		memcpy(copy, arg, size);
	}
	request->payload[0] = (uintptr_t)fn;
	request->payload[1] = (uintptr_t)copy;
	if (s_threadmgrRuntime.schedTraceHook)
		(s_threadmgrRuntime.schedTraceHook)(0x35, 0);
	issueIntrRequest(cpuId, request);
}

/* SGI kind 0x19: run fn(arg) on cpuId, then unlock done (the caller waits on it) */
void issueDTraceSyncXCallIntrRequest(unsigned int cpuId, void (*fn)(void *), void *arg,
				     SceKernelSpinlock *done)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_DTRACE_SYNC_XCALL);
	ThreadmgrTraceHook hook = s_threadmgrRuntime.schedTraceHook;

	request->payload[0] = (uintptr_t)fn;
	request->payload[1] = (uintptr_t)arg;
	request->payload[2] = (uintptr_t)done;
	if (hook != NULL)
		hook(0x36, 0);
	issueIntrRequest(cpuId, request);
}

/* SGI kind 8: debug-suspend a thread running on cpuId; the handler stores result, unlocks done */
void issueDebugSuspendThreadIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t reason,
					int *result, SceKernelSpinlock *done)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_DEBUG_SUSPEND_THREAD);
	ThreadmgrTraceHook hook = s_threadmgrRuntime.schedTraceHook;

	request->payload[0] = threadId;
	request->payload[1] = reason;
	request->payload[2] = (uintptr_t)result;
	request->payload[3] = (uintptr_t)done;
	if (hook != NULL)
		hook(0x31, 4, threadId, reason, result, done);
	issueIntrRequest(cpuId, request);
}

/* SGI kinds 0xb-0xe: debugger.c's register requests for a thread running on cpuId */
void issueSetCpuRegisterIntrRequest(unsigned int cpuId, void *debugRequest)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_SET_CPU_REGISTER);

	request->payload[0] = (uintptr_t)debugRequest;
	issueIntrRequest(cpuId, request);
}

void issueGetCpuRegisterIntrRequest(unsigned int cpuId, void *debugRequest)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_GET_CPU_REGISTER);

	request->payload[0] = (uintptr_t)debugRequest;
	issueIntrRequest(cpuId, request);
}

void issueSetVfpRegisterIntrRequest(unsigned int cpuId, void *debugRequest)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_SET_VFP_REGISTER);

	request->payload[0] = (uintptr_t)debugRequest;
	issueIntrRequest(cpuId, request);
}

void issueGetVfpRegisterIntrRequest(unsigned int cpuId, void *debugRequest)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_GET_VFP_REGISTER);

	request->payload[0] = (uintptr_t)debugRequest;
	issueIntrRequest(cpuId, request);
}

/* SGI kind 0xa: suspend for the VM a thread running on cpuId (virtual_machine.c) */
void issueSuspendThreadForVMIntrRequest(unsigned int cpuId, SceUID threadId, int *result,
					SceKernelSpinlock *done)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_SUSPEND_THREAD_FOR_VM);
	ThreadmgrTraceHook hook = s_threadmgrRuntime.schedTraceHook;

	request->payload[0] = threadId;
	request->payload[1] = (uintptr_t)result;
	request->payload[2] = (uintptr_t)done;
	if (hook != NULL)
		hook(0x34, 1, threadId);
	issueIntrRequest(cpuId, request);
}

/* intrmgr.h gives it an argument it does not take */

typedef int (*ThreadmgrIntrRequestHandler)(SceSize size, void *message);

/*
 * The threads no active CPU could run, on the manager's stagnant list: those
 * this CPU may run go to its ready queue (or back to the debug-suspended
 * list), the rest stay. The list lock is retried with a pause while a
 * thread's lock is busy.
 */
static inline void salvageExecutableStagnantThread(void)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadMgrCpuCB *cpu;
	struct ThreadCB *thread, *first;
	SceProcessObject *process;
	uint32_t self, mask, state;
	volatile int spin;
	unsigned int cpuId;

	cpuId = sceKernelCpuId();
	cpu = currentCpuState();
	SCE_DBG_ASSERT_LEVEL(0, SceIntrmgrForDriver_182EE3E3() != 0, 63);
	SCE_DBG_ASSERT_CHECK(IntrDisabled(), 66);
	first = NULL;
	for (;;) {
		SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
		for (;;) {
			manager = s_threadmgrRuntime.manager;
			thread = (struct ThreadCB *)manager->stagnantNext;
			if ((uintptr_t)thread == (uintptr_t)&manager->stagnantNext || thread == first) {
				sceKernelSpinlockLowUnlock(
					&s_threadmgrRuntime.manager->threadMgrCBLock);
				if (s_threadmgrRuntime.schedTraceHook)
					(s_threadmgrRuntime.schedTraceHook)(4, 0);
				return;
			}
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 78);
			if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0)
				break;
			process = thread->process;
			((ListNode *)thread->next)->prev =
				(ListNode *)thread->prev;
			((ListNode *)thread->prev)->next =
				(ListNode *)thread->next;
			self = (uintptr_t)thread;
			if (process->threadsStopped != 0)
				goto keep;
			mask = thread->currentCpuAffinityMask;
			if (mask == 0)
				mask = process->cpuAffinity;
			if (!(mask & (1u << cpuId) & 0xf))
				goto keep;
			state = thread->status;
			if ((int)state < 0) {
				/* debug-suspended: back to the debugger's list */
				thread->status = ((state & 0x7fff) << 16) | 0x80008000;
				thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
				thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
				s_threadmgrRuntime.manager->debugSuspendedPrev = self;
				((ListNode *)thread->prev)->next =
					(ListNode *)thread;
			} else {
				thread->status = (state & 0xffff0000) | SCE_KERNEL_THREAD_STATUS_READY;
				addReady(thread);
				if (thread->currentPriority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 106);
					cpu->nextRunningThread = 0;
				}
			}
			goto unlock;
keep:
			if (first == NULL)
				first = thread;
			thread->next = (uintptr_t)&s_threadmgrRuntime.manager->stagnantNext;
			thread->prev = s_threadmgrRuntime.manager->stagnantPrev;
			s_threadmgrRuntime.manager->stagnantPrev = self;
			((ListNode *)thread->prev)->next =
				(ListNode *)thread;
unlock:
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
		spin = 10;
		do
			spin--;
		while (spin != 0);
	}
}

/*
 * SGI 2's handler: this CPU's request mask (reschedule, preempt, salvage,
 * deactivate), then its queued requests one by one.
 */
int intrRequestSGIHandler(int code, void *arg)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	ThreadmgrIntrRequestHandler handler;
	struct ThreadmgrIntrRequest *request, *head;
	const struct ThreadmgrPmonRequest *counter;
	uint32_t mask, message[4];
	int intr;

	intr = SceCpuForDriver_821FC0EE();
	mask = SceCpuForDriver_0EE04C03((SceInt32 *)&cpu->intrRequestMask, 0);
	if (mask != 0) {
		if (mask & INTR_MASK_SCHEDULING) {
			SCE_DBG_ASSERT_LEVEL_FUNC(0, cpu == currentCpuState(), 155,
						  "intrRequestSGIHandler");
			cpu->nextRunningThread = 0;
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(1, 0);
		}
		if (mask & INTR_MASK_SALVAGE)
			salvageExecutableStagnantThread();
		if ((mask & INTR_MASK_DEACTIVATE)) {
			deactivateCpu();
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(5, 0);
		}
		if ((mask & INTR_MASK_SIMPLE) && s_threadmgrRuntime.schedTraceHook)
			(s_threadmgrRuntime.schedTraceHook)(0, 0);
	}
	sceKernelCpuResumeIntr(intr);
	for (;;) {
		do {
			request = cpu->intrRequestHead;
			if (request == NULL) {
				if (mask == 0 && s_threadmgrRuntime.schedTraceHook)
					(s_threadmgrRuntime.schedTraceHook)(0x19, 0);
				return -1;
			}
		} while ((struct ThreadmgrIntrRequest *)SceCpuForDriver_CDA96E81(
				 (SceInt32 *)&cpu->intrRequestHead, (SceInt32)request,
				 (SceInt32)request->next) != request);
		intr = SceCpuForDriver_821FC0EE();
		switch (request->kind) {
		case INTR_REQUEST_CHANGE_THREAD_CPU_AFFINITY:
			changeThreadCpuAffinityMask(request->payload[0], request->payload[1], NULL);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(2, 3, request->payload[0],
					request->payload[1], 0);
			break;
		case INTR_REQUEST_CHANGE_THREAD_PRIORITY:
			changeThreadPrioritySGIHandler(request->payload[0], request->payload[1]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(3, 2, request->payload[0],
					request->payload[1]);
			break;
		case INTR_REQUEST_CHANGE_THREAD_PRIORITY_AFFINITY:
			changeThreadPriorityAndCpuAffinityMaskSGIHandler(request->payload[0],
				request->payload[1], request->payload[2]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x1a, 3, request->payload[0],
					request->payload[1], request->payload[2]);
			break;
		case INTR_REQUEST_ACKNOWLEDGE:
			/* issueSimpleIntrRequestToOtherCpuAndWait: answer */
			SceCpuForDriver_BC248C30((SceInt32 *)request->payload[0],
						   1u << sceKernelCpuId());
			break;
		default:
			SCE_DBG_PRINTF_FUNC(0xb, 368, "intrRequestSGIHandler",
					    " error: unknown intr request.\n");
			SCE_PANIC_FUNC(SCE_LINE(370, 369), "intrRequestSGIHandler");
		case INTR_REQUEST_DEBUG_SUSPEND_THREAD:
			debugSuspendThreadIntr(request->payload[0], request->payload[1],
					       (int *)request->payload[2],
					       (SceKernelSpinlock *)request->payload[3]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x13, 4, request->payload[0],
					request->payload[1], *(int *)request->payload[2],
					*(int *)request->payload[3]);
			break;
		case INTR_REQUEST_STOP_THREAD:
			stopThread(request->payload[0], request->payload[1], request->payload[2]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x14, 3, request->payload[0],
					request->payload[1], request->payload[2]);
			break;
		case INTR_REQUEST_SUSPEND_THREAD_FOR_VM:
			suspendThreadForVMIntr(request->payload[0], (int *)request->payload[1],
					       (SceKernelSpinlock *)request->payload[2]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x16, 1, request->payload[0]);
			break;
		case INTR_REQUEST_SET_CPU_REGISTER:
			setCpuRegisterForDebuggerIntr((void *)request->payload[0]);
			break;
		case INTR_REQUEST_GET_CPU_REGISTER:
			getCpuRegisterForDebuggerIntr((void *)request->payload[0]);
			break;
		case INTR_REQUEST_SET_VFP_REGISTER:
			setVfpRegisterForDebuggerIntr((void *)request->payload[0]);
			break;
		case INTR_REQUEST_GET_VFP_REGISTER:
			getVfpRegisterForDebuggerIntr((void *)request->payload[0]);
			break;
		case INTR_REQUEST_PMON_THREAD_CHANGE_COUNTER_ENABLE:
			pmonThreadChangeEnableCounter(request->payload[0], request->payload[1],
						      request->payload[2]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(7, 3, request->payload[0],
					request->payload[1], request->payload[2]);
			break;
		case INTR_REQUEST_PMON_THREAD_SET_COUNTER:
			pmonThreadSetCounterIntr((struct ThreadmgrPmonRequest *)request->payload[0]);
			counter = (const struct ThreadmgrPmonRequest *)request->payload[0];
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(8, 3,
					counter->threadId, counter->counter, counter->value);
			break;
		case INTR_REQUEST_PMON_THREAD_GET_COUNTER:
			pmonThreadGetCounterIntr((struct ThreadmgrPmonRequest *)request->payload[0]);
			counter = (const struct ThreadmgrPmonRequest *)request->payload[0];
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(9, 3,
					counter->counter, *(const uint32_t *)counter->value,
					*counter->done);
			break;
		case INTR_REQUEST_CALL_PA_SCHEDULE_HOOK:
			/* the slots SceThreadmgrForDriver_1378F6EF claims */
			handler = (ThreadmgrIntrRequestHandler)s_threadmgrRuntime.manager->paHooks[0];
			if (handler != NULL) {
				message[0] = request->payload[0];
				message[1] = request->payload[1];
				message[2] = request->payload[2];
				message[3] = request->payload[3];
				handler(sizeof(message), message);
			}
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0xe, 4, request->payload[0],
					request->payload[1], request->payload[2], request->payload[3]);
			break;
		case INTR_REQUEST_CALL_PA_PRIORITY_HOOK:
			handler = (ThreadmgrIntrRequestHandler)s_threadmgrRuntime.manager->paHooks[1];
			if (handler != NULL) {
				message[0] = request->payload[0];
				message[1] = request->payload[1];
				message[2] = request->payload[2];
				message[3] = request->payload[3];
				handler(sizeof(message), message);
			}
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0xf, 4, request->payload[0],
					request->payload[1], request->payload[2], request->payload[3]);
			break;
		case INTR_REQUEST_DTRACE_ASYNC_XCALL:
			dtraceAsyncXCallIntr((void (*)(void *))request->payload[0],
					     (void *)request->payload[1]);
			if (request->payload[1] != 0)
				sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId,
							 (void *)request->payload[1]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x17, 0);
			break;
		case INTR_REQUEST_DTRACE_SYNC_XCALL:
			dtraceSyncXCallIntr((void (*)(void *))request->payload[0],
					    (void *)request->payload[1],
					    (SceKernelSpinlock *)request->payload[2]);
			if (s_threadmgrRuntime.schedTraceHook)
				(s_threadmgrRuntime.schedTraceHook)(0x18, 0);
			break;
		}
		if (request->fromPool == 0) {
			mask = 1;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, request);
			sceKernelCpuResumeIntr(intr);
		} else {
			/* a preallocated one goes back to the free list */
			do {
				head = s_threadmgrRuntime.manager->freeIntrRequests;
				request->next = head;
			} while ((struct ThreadmgrIntrRequest *)SceCpuForDriver_CDA96E81(
					 (SceInt32 *)&s_threadmgrRuntime.manager->freeIntrRequests,
					 (SceInt32)head, (SceInt32)request) != head);
			mask = 1;
			sceKernelCpuResumeIntr(intr);
		}
	}
}

/* SGI kind 9: stopThread on the CPU the thread runs on */
void issueStopThreadIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t arg1, uint32_t arg2)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_STOP_THREAD);
	ThreadmgrTraceHook hook = s_threadmgrRuntime.schedTraceHook;

	request->payload[0] = threadId;
	request->payload[1] = arg1;
	request->payload[2] = arg2;
	if (hook != NULL)
		hook(0x32, 3, threadId, arg1, arg2);
	issueIntrRequest(cpuId, request);
}

/* the preallocated requests (fromPool set: they go back to the free list after use) */
#define INTR_REQUEST_POOL_SIZE		128

static struct ThreadmgrIntrRequest s_intrRequestPool[INTR_REQUEST_POOL_SIZE];

void initIntrRequestPool(void)
{
	struct ThreadmgrIntrRequest *request;

	s_threadmgrRuntime.manager->freeIntrRequests = s_intrRequestPool;
	for (request = s_intrRequestPool; request != &s_intrRequestPool[INTR_REQUEST_POOL_SIZE];
	     request++) {
		request->next = request + 1;
		request->fromPool = 1;
	}
	s_intrRequestPool[INTR_REQUEST_POOL_SIZE - 1].next = NULL;
}

#define TRACE_EVENT_AFFINITY		0x20

/* cpuId sets threadId's affinity */
void issueChangeThreadCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t mask)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_CPU_AFFINITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = threadId;
	request->payload[1] = mask;
	if (hook != NULL)
		hook(TRACE_EVENT_AFFINITY, 3, threadId, mask, 0);
	issueIntrRequest(cpuId, request);
}

#define TRACE_EVENT_PRIORITY		0x21

/* cpuId changes threadId's priority */
void issueChangeThreadPriorityIntrRequest(unsigned int cpuId, SceUID threadId, int priority)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_PRIORITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = threadId;
	request->payload[1] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_PRIORITY, 2, threadId, priority);
	issueIntrRequest(cpuId, request);
}

#define TRACE_EVENT_SCHED		0x2c

/* PLACEHOLDER: name. Tells cpuId of a scheduling event (with the trace hook) */
void issueCallPAScheduleHookIntrRequest(unsigned int cpuId, SceUID processId, SceUID threadId,
				uint32_t event, int priority)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_CALL_PA_SCHEDULE_HOOK);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = processId;
	request->payload[1] = threadId;
	request->payload[2] = event;
	request->payload[3] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_SCHED, 4, processId, threadId, event, priority);
	issueIntrRequest(cpuId, request);
}

/* Native intr-kind 0x14 producer and scheduling probe metadata. */
/* The first descriptor word's framework role remains unproved. */
static uint32_t s_schedChangePriorityProbeId;
volatile uintptr_t s_schedChangePriorityHandler;
extern const char s_schedChangePriorityCodeSite[];

const struct ThreadmgrSchedProbeDescriptor s_schedChangePriorityDescriptor = {
	.probeId = &s_schedChangePriorityProbeId,
	.provider = "__sched_",
	.name = "change__pri",
	.codeSite = s_schedChangePriorityCodeSite,
	.handler = &s_schedChangePriorityHandler,
	.reserved = { 0 },
};

/* The native name is unavailable; the payload establishes this helper's role. */
__attribute__((noinline))
void issueCallPAPriorityHookIntrRequest(unsigned int cpuId, SceUID processId,
					SceUID threadId, int oldPriority,
					int newPriority)
{
	struct ThreadmgrIntrRequest *request;
	ThreadmgrTraceHook hook;

	request = allocIntrRequest(INTR_REQUEST_CALL_PA_PRIORITY_HOOK);
	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = processId;
	request->payload[1] = threadId;
	request->payload[2] = oldPriority;
	request->payload[3] = newPriority;
	if (hook)
		hook(0x2d, 4, processId, threadId, oldPriority, newPriority);
	issueIntrRequest(cpuId, request);
}
