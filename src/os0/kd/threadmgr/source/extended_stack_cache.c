/* Per-CPU cache of reusable extended kernel-stack memblocks. */
#include "private.h"
#include "sce/cpu.h"

volatile struct ThreadmgrExtendedStackCache s_extendedStackCaches[4];

SceUID popExtendedStackCache(void **base)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	SceKernelIntrStatus intrState;
	uint32_t count;
	SceUID uid = -1;
	unsigned int slot;

	intrState = ksceKernelCpuSuspendIntr();
	cache = &s_extendedStackCaches[ksceKernelCpuId()];
	count = cache->count;
	if (!count)
		goto done;
	for (slot = 0; slot < 3; slot++) {
		uid = cache->memblockIds[slot];
		if (uid != -1)
			break;
	}
	if (slot == 3)
		goto done;
	*base = (void *)cache->mappedBases[slot];
	cache->memblockIds[slot] = -1;
	cache->mappedBases[slot] = 0;
	cache->count = count - 1;
done:
	ksceKernelCpuResumeIntr(intrState);
	return uid;
}

int pushExtendedStackCache(SceUID uid, void *base)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	SceKernelIntrStatus intrState;
	uint32_t count;
	unsigned int slot;
	int result = -1;

	intrState = ksceKernelCpuSuspendIntr();
	cache = &s_extendedStackCaches[ksceKernelCpuId()];
	count = cache->count;
	if (cache->capacity <= count)
		goto done;
	for (slot = 0; slot < 3; slot++) {
		if (cache->memblockIds[slot] == -1)
			break;
	}
	if (slot == 3)
		goto done;
	cache->memblockIds[slot] = uid;
	cache->mappedBases[slot] = (uintptr_t)base;
	cache->count = count + 1;
	result = 0;
done:
	ksceKernelCpuResumeIntr(intrState);
	return result;
}

/* the per-CPU stack caches empty, and no hooks */
int module_start(SceSize arglen, const void *argp)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	unsigned int cpu;

	for (cpu = 0; cpu < 4; cpu++) {
		cache = &s_extendedStackCaches[cpu];
		cache->count = 0;
		cache->capacity = 3;
		cache->memblockIds[0] = -1;
		cache->mappedBases[0] = 0;
		cache->memblockIds[1] = -1;
		cache->mappedBases[1] = 0;
		cache->memblockIds[2] = -1;
		cache->mappedBases[2] = 0;
	}
	s_threadmgrRuntime.switchHook = NULL;
	s_threadmgrRuntime.schedTraceHook = NULL;
	s_threadmgrRuntime.threadTraceHook = NULL;
	s_threadmgrRuntime.onCpuHook = NULL;
	s_threadmgrRuntime.offCpuHook = NULL;
	s_threadmgrRuntime.priorityChangeHook = NULL;
	s_threadmgrRuntime.vfpExceptionHook = NULL;
	s_threadmgrRuntime.alarmTraceHook = NULL;
	return 0;
}
