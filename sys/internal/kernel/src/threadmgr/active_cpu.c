/*
 * SceKernelThreadMgr (threadmgr.c): the set of active CPUs. A CPU leaving
 * the set hands every thread it holds (standby, ready, running) to the
 * other CPUs; one joining it takes the stagnant threads it may run.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/error.h"
#include "scheduler.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define CPU_COUNT			4
#define CPU_MASK_ALL			0xf
#define CPU_MASK_ALT			0x000f0000	/* the same mask, shifted up 16 */
#define PERMISSION_SYSTEM		0x40	/* PLACEHOLDER: names of the permissions allowed */
#define PERMISSION_ROOT			0x80
#define DEACTIVATE_SWITCH		3	/* deactivateCpu: the running thread moved away */
#define SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK	0x80028025

extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);

/*
 * This CPU leaves the active set: its threads go to the others. Returns
 * DEACTIVATE_SWITCH when the running thread moved and the CPU must switch.
 */
int deactivateCpu(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread, *next;
	volatile int spin;
	uintptr_t head;
	int priority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 7084);
	/* the standby list; a busy thread is retried after a short spin */
	for (;;) {
		ksceKernelSpinlockLowLock(&cpu->standbyLock);
		for (;;) {
			thread = (void *)cpu->standbyNext;
			if ((uintptr_t)thread == (uintptr_t)&cpu->standbyNext)
				goto ready;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7099);
			if (ksceKernelSpinlockLowTrylock(THREAD_LOCK(thread)) < 0)
				break;
			((struct ThreadCB *)thread->next)->prev = thread->prev;
			((struct ThreadCB *)thread->prev)->next = thread->next;
			ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			distributeThread(selectExecCpu(thread), thread);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceKernelSpinlockLowLock(&cpu->standbyLock);
		}
		ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
		for (spin = 10; spin != 0; spin--)
			;
	}
ready:
	ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
	/* the ready queues this CPU keeps itself: the high priorities and the idle ones */
	for (priority = 0; priority < 0x80; priority++) {
		head = (uintptr_t)&cpu->readyQueue[priority];
		for (thread = (void *)cpu->readyQueue[priority].next; (uintptr_t)thread != head;
		     thread = next) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7126);
			next = (void *)thread->next;
			if (thread == cpu->idleThread)
				continue;
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			removeReady(thread, priority);
			distributeThread(selectExecCpu(thread), thread);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
	}
	for (priority = 0xfc; priority < 0x100; priority++) {
		head = (uintptr_t)&cpu->readyQueue[priority];
		for (thread = (void *)cpu->readyQueue[priority].next; (uintptr_t)thread != head;
		     thread = next) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7144);
			next = (void *)thread->next;
			if (thread == cpu->idleThread)
				continue;
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			removeReady(thread, priority);
			distributeThread(selectExecCpu(thread), thread);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
	}
	/* and the running thread */
	thread = (void *)cpu->runningThread;
	if (thread == cpu->idleThread)
		return 0;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	distributeThread(selectExecCpu(thread), thread);
	if (currentThreadContext() == NULL)
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	else
		thread->waitId = -1;
	SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 7179);
	cpu->nextRunningThread = 0;
	return DEACTIVATE_SWITCH;
}

/*
 * Sets the active CPUs (bits 0-3, or the same in bits 16-19). Only system
 * processes may call it.
 */
int sceKernelChangeActiveCpuMask(uint32_t mask)
{
	struct ThreadCB *thread = (void *)currentThreadContext();
	unsigned int cpuId, i;
	uint32_t saved, old, bit;
	int denied, intr, ret, moved;

	denied = 1;
	if (thread != NULL &&
	    (thread->permission == PERMISSION_SYSTEM || thread->permission == PERMISSION_ROOT))
		denied = 0;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	SCE_DBG_ASSERT_CHECK(!ThreadmgrInSyscall(), 7212);
	if (denied) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto restore;
	}
	if (mask & ~(CPU_MASK_ALL | CPU_MASK_ALT)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		goto restore;
	}
	if (mask & CPU_MASK_ALT) {
		if (mask & CPU_MASK_ALL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
			goto restore;
		}
		mask >>= 16;
	}
	moved = 0;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	cpuId = ksceKernelCpuId();
	old = s_threadmgrRuntime.manager->activeCpuMask;
	s_threadmgrRuntime.manager->activeCpuMask = mask & CPU_MASK_ALL;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	for (i = 0; i < CPU_COUNT; i++) {
		bit = 1 << i;
		if (!(bit & (mask ^ old)))
			continue;
		if (bit & mask) {
			if (i == cpuId) {
				SCE_DBG_PRINTF(0xb, 7256,
					" fatal error: should not be executed. CPU ID = %d\n", i);
				SCE_PANIC(SCE_LINE(7250, 7257));
			}
			addIntrRequest(i, INTR_MASK_SALVAGE);
		} else if (i == cpuId) {
			moved = deactivateCpu();
		} else {
			addIntrRequest(i, INTR_MASK_DEACTIVATE);
		}
	}
	if (moved && currentThreadContext() != NULL)
		switchThread(2);
	ksceKernelCpuResumeIntr(intr);
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_16D1961A: every other CPU reschedules, then this one */
int SceThreadmgrForKernel_16D1961A(void)
{
	unsigned int cpuId, i;
	int intr;

	intr = ksceKernelCpuSuspendIntr();
	cpuId = ksceKernelCpuId();
	for (i = 0; i < CPU_COUNT; i++) {
		if (i != cpuId)
			addIntrRequest(i, INTR_MASK_SCHEDULING);
	}
	currentCpuState()->nextRunningThread = 0;
	if (currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	return 0;
}
