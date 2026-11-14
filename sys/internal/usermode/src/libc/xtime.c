/*
 * xtime.c: the threads library's times: the time of day, in microseconds and as an xtime
 * with nanoseconds, and the timeout until a time
 */
#include <time.h>

#include <processmgr/processmgr.h>

#include "xthrpsp2.h"

#define NSEC_PER_SEC	1000000000L
#define NSEC_PER_USEC	1000L
#define USEC_PER_SEC	1000000L

long long _Xtime_get_ticks(void)
{
	ProcessTimeval tv;
	ProcessTimezone tz;

	sceKernelLibcGettimeofday(&tv, &tz);
	return (time_t)tv.sec * (long long)_XTIME_TICKS_PER_TIME_T + tv.usec;
}

int xtime_get(xtime *xt, int type)
{
	ProcessTimeval tv;
	ProcessTimezone tz;

	if (type != TIME_UTC || xt == NULL) {
		type = 0;
	} else {
		sceKernelLibcGettimeofday(&tv, &tz);
		xt->sec = tv.sec;
		xt->nsec = tv.usec * NSEC_PER_USEC;
	}
	return type;
}

SceUInt32 _Xtime_diff_to_usec(const xtime *xt)
{
	ProcessTimeval tv;
	ProcessTimezone tz;
	xtime now;
	xtime diff;

	sceKernelLibcGettimeofday(&tv, &tz);
	now.sec = tv.sec;
	now.nsec = tv.usec * NSEC_PER_USEC;
	diff = *xt;
	while (diff.nsec < 0) {	/* normalize */
		diff.sec -= 1;
		diff.nsec += NSEC_PER_SEC;
	}
	while (NSEC_PER_SEC <= diff.nsec) {
		diff.sec += 1;
		diff.nsec -= NSEC_PER_SEC;
	}
	if (diff.nsec < now.nsec) {	/* avoid underflow */
		diff.sec -= now.sec + 1;
		diff.nsec += NSEC_PER_SEC - now.nsec;
	} else {
		diff.sec -= now.sec;
		diff.nsec -= now.nsec;
	}
	if (diff.sec < 0 || (diff.sec == 0 && diff.nsec <= 0)) {	/* the time is past */
		diff.sec = 0;
		diff.nsec = 0;
	}
	return diff.sec * USEC_PER_SEC + (diff.nsec + NSEC_PER_USEC - 1) / NSEC_PER_USEC;
}
