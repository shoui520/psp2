/*
 * libctime.c (file name unknown) - the kernel libc time functions; another
 * module (SceProcessmgrForDriver_4140D633) provides the implementation
 */

#include "processmgr_internal.h"

static const ProcessmgrLibcOps *g_libcOps;

/* name unknown: register the libc time implementation (once) */
int SceProcessmgrForDriver_4140D633(const ProcessmgrLibcOps *ops)
{
	if (g_libcOps != NULL)
		return 0;
	g_libcOps = ops;
	return 0;
}

/* name unknown: clock(), the process run time */
SceUInt32 ProcessmgrLibcClock(void)
{
	return SceProcessmgrForDriver_82D94BE9();
}

SceKernelTime SceProcessmgrForKernel_B0E076D6(SceKernelTime *t)
{
	if (g_libcOps->time != NULL)
		return g_libcOps->time(t);
	return -1;
}

int SceProcessmgrForKernel_014F5113(ProcessTimeval *tv, ProcessTimezone *tz)
{
	if (g_libcOps->gettimeofday != NULL)
		return g_libcOps->gettimeofday(tv, tz);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* names unknown: gmtime_r, localtime_r and mktime */
int ProcessmgrLibcGmtime_r(const SceKernelTime *t, ProcessTm *tm)
{
	if (g_libcOps->gmtime_r != NULL)
		return g_libcOps->gmtime_r(t, tm);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int ProcessmgrLibcLocaltime_r(const SceKernelTime *t, ProcessTm *tm)
{
	if (g_libcOps->localtime_r != NULL)
		return g_libcOps->localtime_r(t, tm);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int ProcessmgrLibcMktime(ProcessTm *tm, SceKernelTime *t, ProcessTimezone *tz)
{
	if (g_libcOps->mktime != NULL)
		return g_libcOps->mktime(tm, t, tz);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* names unknown: the RTC's current tick and current secure tick */
int SceProcessmgrForKernel_D231AE5B(struct SceRtcTick *tick)
{
	if (g_libcOps->getCurrentTick != NULL)
		return g_libcOps->getCurrentTick(tick);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int SceProcessmgrForKernel_3EBC93BB(struct SceRtcTick *tick)
{
	if (g_libcOps->getCurrentSecureTick != NULL)
		return g_libcOps->getCurrentSecureTick(tick);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}
