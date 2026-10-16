/* Sony venezia/system.c: DSP execution-group and performance-monitor setup. */
#include <sce/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include "venezia_internal.h"

/* Names unknown: DSP supplies eight busy/total counter pairs and ten results. */
typedef struct VeneziaPerformanceCounter {
	unsigned int busy;
	unsigned int total;
} VeneziaPerformanceCounter;



typedef struct VeneziaPerformanceMonitorState {
	struct ThreadmgrFastMutex mutex;
	VeneziaPerformanceCounter counters[8];
	VeneziaPerformanceResult result;
	int active;
	unsigned int rpcArgument;
} VeneziaPerformanceMonitorState;

_Static_assert(sizeof(VeneziaPerformanceMonitorState) == 0xb0, "Performance monitor state size");

VeneziaPerformanceMonitorState g_veneziaPerformanceMonitor = { .rpcArgument = 166 };
unsigned int g_veneziaThreadExecGroups[1];
unsigned int g_veneziaPreferredClock = 166;
int SceCodecEngineWrapperForDriver_0D3BB164(unsigned int command, ...);
int packVeneziaRpcWord(VeneziaRpcThunk *thunk, const unsigned int *arguments,
	unsigned int *packed);
void unpackVeneziaRpcMemory(VeneziaRpcThunk *thunk);
int packVeneziaRpcBuffer64(VeneziaRpcThunk *thunk, const unsigned int *arguments,
	unsigned int *packed);
int callVeneziaRpcThunkWithoutClockLock(unsigned int command, const unsigned int *input,
	VeneziaRpcPack pack, VeneziaRpcUnpack unpack);
int veneziaRpcWithoutClockLock(unsigned int command, ...)
	__asm__("callVeneziaRpcWithoutClockLock");
int veneziaDebugRpc(unsigned int command, ...) __asm__("callVeneziaDebugRpc");
int callVeneziaDebugRpcThunk(unsigned int command, const unsigned int *input,
	VeneziaRpcPack pack, VeneziaRpcUnpack unpack);
int _lockProcessSuspendCore(void);
int _unlockProcessSuspendCore(void);

/* name unknown: establish the performance-monitor communication word. */
int initCodecEnginePerformanceMonitor(void)
{
	unsigned int arguments[4] __attribute__((aligned(8)));
	int ret;

	ret = ksceKernelInitializeFastMutex(&g_veneziaPerformanceMonitor.mutex,
		"SceCodecEnginePerformanceMonitor", 0x8002, NULL);
	if (ret >= 0) {
		arguments[0] = (uintptr_t)g_veneziaThreadExecGroups;
		ret = callVeneziaRpcThunkWithoutClockLock(503, arguments,
			packVeneziaRpcWord, unpackVeneziaRpcMemory);
		if (ret >= 0)
			ret = veneziaRpcWithoutClockLock(505, 6);
	}
	return ret;
}

int sceCodecEngineDsp0GetThreadExecGroup(unsigned int thread)
{
#if FW_DEBUG
	ksceKernelAssert(thread == 0,
		SCE_DBGINFO_FUNC(155, "sceCodecEngineDsp0GetThreadExecGroup"), __builtin_return_address(0));
#endif
	return g_veneziaThreadExecGroups[thread];
}

/* name unknown: this firmware has no performance-monitor resume work. */
int resumePerformanceMonitor(void)
{
	return 0;
}

/* name unknown: this firmware has no performance-monitor suspend work. */
int startPerformanceMonitorSuspend(void)
{
	return 0;
}

/* name unknown: the performance monitor is always suspended in this firmware. */
int isPerformanceMonitorSuspended(void)
{
	return 1;
}

/* name unknown: begin a DSP performance-counter sampling interval. */
int startCodecEnginePerformanceMonitor(void)
{
	int ret = _lockProcessSuspendCore();

	if (ret >= 0) {
		ret = ksceKernelLockFastMutex(&g_veneziaPerformanceMonitor.mutex);
		if (ret >= 0) {
			if (!g_veneziaPerformanceMonitor.active) {
				ret = veneziaDebugRpc(2);
				if (ret >= 0)
					g_veneziaPerformanceMonitor.active = 1;
			}
			ksceKernelUnlockFastMutex(&g_veneziaPerformanceMonitor.mutex);
		}
		_unlockProcessSuspendCore();
	}
	return ret;
}

/* name unknown: collect the DSP's performance-counter pairs. */
int stopCodecEnginePerformanceMonitor(void)
{
	unsigned int arguments[4] __attribute__((aligned(8)));
	int ret = _lockProcessSuspendCore();

	if (ret >= 0) {
		ret = ksceKernelLockFastMutex(&g_veneziaPerformanceMonitor.mutex);
		if (ret >= 0) {
			if (g_veneziaPerformanceMonitor.active) {
				arguments[0] = (uintptr_t)g_veneziaPerformanceMonitor.counters;
				arguments[1] = g_veneziaPerformanceMonitor.rpcArgument;
				ret = callVeneziaDebugRpcThunk(3, arguments,
					packVeneziaRpcBuffer64, unpackVeneziaRpcMemory);
				if (ret >= 0)
					g_veneziaPerformanceMonitor.active = 0;
			}
			ksceKernelUnlockFastMutex(&g_veneziaPerformanceMonitor.mutex);
		}
		_unlockProcessSuspendCore();
	}
	return ret;
}

/* name unknown: convert the DSP counter pairs to integer percentages. */
int getCodecEnginePerformanceResults(VeneziaPerformanceResult *result)
{
	VeneziaPerformanceMonitorState *monitor = &g_veneziaPerformanceMonitor;
	unsigned int sum = 0, maximum = 0, usage;
	int ret = ksceKernelLockFastMutex(&monitor->mutex);

	if (ret >= 0) {
		if (monitor->counters[0].total == ~0u) {
			maximum = ~0u;
			monitor->result.usage[0] = ~0u;
		} else {
			sum = (unsigned int)((float)monitor->counters[0].busy * 100.0f /
				(float)monitor->counters[0].total);
			maximum = sum;
			monitor->result.usage[0] = sum;
		}
#define COUNTER_PERCENTAGE(index) \
		do { \
			if (monitor->counters[index].total == ~0u) { \
				monitor->result.usage[index] = ~0u; \
				maximum = ~0u; \
			} else { \
				usage = (unsigned int)((float)monitor->counters[index].busy * 100.0f / \
					(float)monitor->counters[index].total); \
				sum += usage; \
				if (maximum < usage) \
					maximum = usage; \
				monitor->result.usage[index] = usage; \
			} \
		} while (0)
		COUNTER_PERCENTAGE(1);
		COUNTER_PERCENTAGE(2);
		COUNTER_PERCENTAGE(3);
		COUNTER_PERCENTAGE(4);
		COUNTER_PERCENTAGE(5);
		COUNTER_PERCENTAGE(6);
#undef COUNTER_PERCENTAGE
		if (monitor->counters[7].total == ~0u) {
			monitor->result.usage[7] = ~0u;
			maximum = ~0u;
			monitor->result.maximum = ~0u;
		} else {
			monitor->result.maximum = maximum;
			monitor->result.usage[7] = (unsigned int)((float)monitor->counters[7].busy *
				100.0f / (float)monitor->counters[7].total);
			if (maximum != ~0u)
				maximum = sum / 6;
		}
		monitor->result.average = maximum;
		if (result)
			*result = monitor->result;
		ksceKernelUnlockFastMutex(&monitor->mutex);
	}
	return ret;
}

/* name unknown: clear the DSP's accumulated counter pairs. */
int clearCodecEnginePerformanceCounters(void)
{
	int ret = ksceKernelLockFastMutex(&g_veneziaPerformanceMonitor.mutex);

	if (ret >= 0) {
		memset(g_veneziaPerformanceMonitor.counters, 0,
			sizeof(g_veneziaPerformanceMonitor.counters));
		ksceKernelUnlockFastMutex(&g_veneziaPerformanceMonitor.mutex);
	}
	return ret;
}

/* name unknown: clamp and apply the DSP clock selector. */
int setVeneziaClockSelector(unsigned int selector)
{
	unsigned int bounded = selector < 4 ? 4 : selector;
	int ret;

	if (bounded >= 7)
		bounded = 7;
	ret = SceCodecEngineWrapperForDriver_0D3BB164(505, bounded);
	if (ret < 0)
		return ret & (ret >> 31);
	return bounded;
}

/* name unknown: return the stored preferred DSP frequency. */
unsigned int getVeneziaPreferredClock(void)
{
	return g_veneziaPreferredClock;
}

/* name unknown: map the supported MHz rates to DSP clock selectors. */
int veneziaClockSelector(int frequency)
{
	switch (frequency) {
	case 41:
		return 1;
	case 55:
		return 2;
	case 83:
		return 3;
	case 111:
		return 4;
	case 166:
		return 5;
	case 222:
		return 6;
	case 333:
		return 7;
	default:
		return 0;
	}
}
