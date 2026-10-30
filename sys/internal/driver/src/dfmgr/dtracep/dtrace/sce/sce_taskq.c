/*
 * The task queues' threads (Sony's dtrace/sce/sce_taskq.c): a queue's worker
 * threads and the per-entry threads of dynamic queues, as thread manager
 * threads.
 */
#include <sys/dtrace_impl.h>
#include <sys/taskq_impl.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>

uid_t dtrace_taskq_thread_create(taskq_t *tq, pri_t pri, int idx)
{
	SceKernelThreadOptParam opt = {
		.size = sizeof(opt),
		.attr = 1,
		.kStackMemType = 0x10f0d006,
	};
	char name[256];
	SceUID thid;
	int res;

	snprintf(name, sizeof(name), "SceDeci4pDtracepTaskQ%d", idx);
	thid = sceKernelCreateThread(name, (SceKernelThreadEntry)taskq_thread, pri, 0x4000, 0,
				     0x80000, &opt);
	if (thid < 0) {
		DTRACE_LOG_ERROR_AT(41, "failed to create taskq thread %s with priority %d (0x%08x)",
				    name, pri, thid);
		SCE_DBG_ASSERT(thid >= 0, 42);
	}
	res = sceKernelStartThread(thid, sizeof(tq), &tq);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(49, "failed to start taskq thread %s (0x%08x)", name, res);
		SCE_DBG_ASSERT(res >= 0, 50);
	}
	return thid;
}

uid_t dtrace_taskq_ent_thread_create(int32_t initPriority)
{
	SceKernelThreadOptParam opt = {
		.size = sizeof(opt),
		.attr = 1,
		.kStackMemType = 0x10f0d006,
	};
	SceUID thid;

	thid = sceKernelCreateThread("SceDeci4pDtracepTaskQEnt", (SceKernelThreadEntry)taskq_d_thread,
				     initPriority, 0x4000, 0, 0x80000, &opt);
	if (thid < 0) {
		DTRACE_LOG_ERROR_AT(79, "failed to create taskq thread %s with priority %d (0x%08x)",
				    "SceDeci4pDtracepTaskQEnt", initPriority, thid);
		SCE_DBG_ASSERT(thid >= 0, 80);
	}
	return thid;
}
