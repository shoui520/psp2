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
	return ksceKernelGetProcessTimeWideCore();
}

SceKernelTime ksceKernelLibcTime(SceKernelTime *t)
{
	if (g_libcOps->time != NULL)
		return g_libcOps->time(t);
	return -1;
}

int ksceKernelLibcGettimeofday(void *tv, void *tz)
{
	if (g_libcOps->gettimeofday != NULL)
		return g_libcOps->gettimeofday(tv, tz);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* names unknown: gmtime_r, localtime_r and mktime */
int ProcessmgrLibcGmtime_r(const SceKernelTime *t, void *tm)
{
	if (g_libcOps->gmtime_r != NULL)
		return g_libcOps->gmtime_r(t, tm);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int ProcessmgrLibcLocaltime_r(const SceKernelTime *t, void *tm)
{
	if (g_libcOps->localtime_r != NULL)
		return g_libcOps->localtime_r(t, tm);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int ProcessmgrLibcMktime(void *tm, SceKernelTime *t, SceUInt64 *wide)
{
	if (g_libcOps->mktime != NULL)
		return g_libcOps->mktime(tm, t, wide);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

/* names unknown: the RTC's current tick and current secure tick */
int SceProcessmgrForKernel_D231AE5B(void *a, void *b, void *c)
{
	if (g_libcOps->getCurrentTick != NULL)
		return g_libcOps->getCurrentTick(a, b, c);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int SceProcessmgrForKernel_3EBC93BB(void *a, void *b, void *c)
{
	if (g_libcOps->getCurrentSecureTick != NULL)
		return g_libcOps->getCurrentSecureTick(a, b, c);
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}
