/*
 * DTrace's high-resolution time (Sony's dtrace/sce/dtrace_hrtime.c):
 * nanoseconds since the first read, from the global timer's ticks.
 */
#include <sys/dtrace_impl.h>
#include <processmgr/processmgr.h>
#include "../../global_timer.h"

/* the global timer counts 333 ticks every 2000 ns */
#define DTRACE_HRTIME_TICKS	333
#define DTRACE_HRTIME_NS	2000

static uint64_t s_hrtimeBase;

void dtrace_hrtime_init(void)
{
}

void dtrace_hrtime_exit(void)
{
}

hrtime_t dtrace_gethrtime(void)
{
	uint64_t now, base;

	now = GlobalTimerGetCount();
	base = s_hrtimeBase;
	if (base == 0) {
		s_hrtimeBase = now;
		return base;
	}
	return (now - base) * DTRACE_HRTIME_NS / DTRACE_HRTIME_TICKS;
}

hrtime_t dtrace_gethrestime(void)
{
	ProcessTimeval tv;
	int res;

	res = sceKernelLibcGettimeofday(&tv, NULL);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(233, "failed to get time of day: 0x%x", res);
		return 0;
	}
	return (uint64_t)sceKernelLibcTime(NULL) * NANOSEC + (int64_t)tv.usec * 1000;
}
