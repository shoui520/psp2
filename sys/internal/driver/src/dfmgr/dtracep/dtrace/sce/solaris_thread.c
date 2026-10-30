/*
 * Solaris threads: delays, and the task queue a thread works for, kept in its
 * DTrace state.
 */
#include <dtrace/sce/solaris_hrtime.h>
#include <dtrace/sce/solaris_thread.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>

int maxclsyspri = 16;

void delay(clock_t ticks)
{
	if (ticks >= 0)
		sceKernelDelayThread(ticks);
}

void dtrace_set_thread_taskq(int thread, struct taskq *tq)
{
	SceObjectBase *object;
	ThreadCB *context;
	int intr;

	intr = sceKernelCpuSuspendIntr();
	if (sceGUIDReferObject(thread, &object) >= 0) {
		context = &((struct SceUIDThreadObject *)object)->context;
		sceKernelSpinlockLowLock(&context->lock);
		context->dtraceState->taskq = tq;
		sceKernelSpinlockLowUnlock(&context->lock);
		sceGUIDReleaseObject(thread);
	}
	sceKernelCpuResumeIntr(intr);
}
