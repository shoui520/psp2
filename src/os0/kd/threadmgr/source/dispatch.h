/*
 * SceKernelThreadMgr (dispatch.c): what both context selectors share.
 */

#ifndef THREADMGR_DISPATCH_H
#define THREADMGR_DISPATCH_H

#include "private.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"

/* Sony's dispatch.c, like the files that include this */
#undef __FILE_NAME__
#define __FILE_NAME__ "dispatch.c"

#define DISPATCH_CPU_COUNT		4
#define DISPATCH_PRIORITY_SHARED_LOW	0x80	/* priorities 0x80-0xfb share the ready queues */
#define DISPATCH_PRIORITY_SHARED_RANGE	0x7c
#define DISPATCH_STATE_READY		0x00000002
#define DISPATCH_STATE_STANDBY		0x00000004
#define DISPATCH_STATE_HIGH_MASK	0xffff0000

/*
 * The preempted running thread of cpu goes back: to the ready queues, or to
 * the standby list of the CPU running the least urgent thread it may run on.
 * Sony inlined it into both context selectors; caller is theirs.
 */
static inline __attribute__((always_inline)) void distributePreemptedThread(
	struct ThreadMgrCpuCB *cpu,
	struct ThreadCB *thread, unsigned int cpuId, const void *caller)
{
	struct SceKernelThreadmgrPrivate *manager;
	uint32_t affinity, target = UINT32_MAX;
	unsigned int other;
	int worst = 1;

	SCE_DBG_ASSERT_CHECK_LR(thread->lock != 0, 253, caller);
	affinity = thread->currentCpuAffinityMask;
	if (affinity == 0)
		affinity = thread->process->cpuAffinity;
	affinity &= ~(1u << cpuId);
	SCE_DBG_ASSERT_CHECK_LR(thread->currentCpuId == (int)ksceKernelCpuId(), 260, caller);
	manager = s_threadmgrRuntime.manager;
	if ((uint32_t)thread->currentPriority - DISPATCH_PRIORITY_SHARED_LOW < DISPATCH_PRIORITY_SHARED_RANGE) {
		thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
				     DISPATCH_STATE_READY;
		addReadyTop((struct ThreadCB *)thread);
		manager = s_threadmgrRuntime.manager;
		for (other = 0; other < DISPATCH_CPU_COUNT; other++) {
			if (((1u << other) & affinity) && worst < manager->cpuCB[other].priority) {
				worst = manager->cpuCB[other].priority;
				target = other;
			}
		}
		if (worst <= thread->currentPriority)
			target = UINT32_MAX;
	} else {
		for (other = 0; other < DISPATCH_CPU_COUNT; other++) {
			if (((1u << other) & affinity) && worst < manager->cpuCB[other].priority) {
				worst = manager->cpuCB[other].priority;
				target = other;
			}
		}
		if (thread->currentPriority < worst) {
			thread->currentCpuId = target;
			thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
					     DISPATCH_STATE_STANDBY;
			addStandby((struct ThreadMgrCpuCB *)&manager->cpuCB[target],
				   (struct ThreadCB *)thread);
		} else {
			thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
					     DISPATCH_STATE_READY;
			addReadyTop((struct ThreadCB *)thread);
			target = UINT32_MAX;
		}
	}
	thread = (struct ThreadCB *)cpu->runningThread;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (target != UINT32_MAX)
		addIntrRequest(target, INTR_MASK_SCHEDULING);
}

#endif
