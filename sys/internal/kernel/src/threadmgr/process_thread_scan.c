/*
 * SceKernelThreadMgr (threadmgr.c): taking a process's threads off this
 * CPU (processmgr suspends a process CPU by CPU). Its ready threads leave
 * this CPU's queues and the global ones, and its running thread the CPU;
 * all go on the manager's stagnant list.
 */

#include "private.h"
#include "affinity.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define CPU_ID_GLOBAL			0x7fff
#define PRIORITY_SHARED_LOW		0x80
#define PREEMPT_INTR			2
#define LOCKED_RETRY_DELAY		10

/* the thread goes on the manager's stagnant list */
static inline __attribute__((always_inline)) void listStagnantThread(struct ThreadCB *thread)
{
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->stagnantNext;
	thread->prev = s_threadmgrRuntime.manager->stagnantPrev;
	s_threadmgrRuntime.manager->stagnantPrev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
	ksceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* sceKernelCpuScanThreadForProcessmgr: processId's threads leave this CPU */
int sceKernelCpuScanThreadForProcessmgr(SceUID processId)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *current = currentThreadContext(), *thread, *next;
	struct ThreadCB *head;
	SceKernelIntrStatus intr = ksceKernelCpuSuspendIntr();
	uint32_t bits, priority, word;
	volatile unsigned int delay;
	bool retry;

	SCE_DBG_ASSERT_CHECK((intr >> 7) & 1, 10065);
	if (current == NULL)
		current = (struct ThreadCB *)currentCpuState()->runningThread;
	else
		SCE_DBG_ASSERT_CHECK(current->process->threadsStopped == 0,
				     10068);
	moveStandbyToReady();

	/* this CPU's queues */
	for (word = 0; word < 4; word++) {
		bits = cpu->readyBitmap[word];
		while (bits != 0) {
			priority = __builtin_clz(bits) + word * 32;
			head = (struct ThreadCB *)&cpu->readyQueue[priority];
			for (thread = (struct ThreadCB *)head->next;
			     thread != head; thread = next) {
				next = (struct ThreadCB *)thread->next;
				SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 10087);
				ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
				if (thread->processId == processId) {
					SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_REQUEST_MASK),
							     10093);
					removeReady((struct ThreadCB *)thread, priority);
					thread->status = (thread->status & THREAD_STATE_HIGH_MASK) |
							     SCE_KERNEL_THREAD_STATUS_STAGNANT;
					SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 10098);
					thread->currentCpuId = -1;
					listStagnantThread(thread);
				}
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			}
			bits &= ~(0x80000000u >> (priority % 32));
		}
	}

	/* the global queues, from a thread (a locked thread: all over again shortly) */
	if (currentThreadContext() != NULL) {
		do {
			retry = false;
			ksceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->globalReadyLock);
			for (word = 0; word < 4 && !retry; word++) {
				bits = s_threadmgrRuntime.manager->globalReadyBitmap[word];
				while (bits != 0 && !retry) {
					priority = __builtin_clz(bits) + PRIORITY_SHARED_LOW + word * 32;
					head = (struct ThreadCB *)&s_threadmgrRuntime.
						manager->globalReadyQueue[priority - PRIORITY_SHARED_LOW];
					for (thread = (struct ThreadCB *)head->next;
					     thread != head; thread = next) {
						next = (struct ThreadCB *)thread->next;
						SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC,
								     10127);
						if (ksceKernelSpinlockLowTrylock(THREAD_LOCK(thread)) < 0) {
							retry = true;
							break;
						}
						SCE_DBG_ASSERT_CHECK(
							s_threadmgrRuntime.manager->globalReadyLock != 0 ||
							thread->lock != 0, 10134);
						if (thread->processId == processId) {
							removeCommonRaw((struct ThreadCB *)thread,
									priority);
							thread->status =
								(thread->status & THREAD_STATE_HIGH_MASK) |
								SCE_KERNEL_THREAD_STATUS_STAGNANT;
							SCE_DBG_ASSERT_CHECK(thread->currentCpuId == CPU_ID_GLOBAL, 10140);
							thread->currentCpuId = -1;
							listStagnantThread(thread);
						}
						ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					}
					bits &= ~(0x80000000u >> (priority % 32));
				}
			}
			ksceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->globalReadyLock);
			if (retry) {
				for (delay = LOCKED_RETRY_DELAY; delay != 0; delay--)
					;
			}
		} while (retry);
	}

	/* the running thread */
	if (current != NULL) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(current));
		if (current->processId == processId && (current->status & SCE_KERNEL_THREAD_STATUS_RUNNING)) {
			runningThreadPreemptByIntr((struct ThreadCB *)current,
						   PREEMPT_INTR);
			SCE_DBG_ASSERT_CHECK(current->currentCpuId == (int)ksceKernelCpuId(), 10199);
			current->currentCpuId = -1;
			current->status = (current->status & THREAD_STATE_HIGH_MASK) |
					      SCE_KERNEL_THREAD_STATUS_STAGNANT;
			listStagnantThread(current);
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(current));
	}
	ksceKernelCpuResumeIntr(intr);
	return 0;
}
