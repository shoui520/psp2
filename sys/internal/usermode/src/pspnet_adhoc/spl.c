/*
 * spl.c: BSD-style SPL lock and heap allocator wrappers for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

static int g_adhocSplMutexInit = -1;
static SceKernelLwMutexWork g_adhocSplMutex;

__attribute__((noinline))
int adhocSplInit(void)
{
	int ret;

	ret = sceKernelCreateLwMutex(&g_adhocSplMutex, "SceNetSpl",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	g_adhocSplMutexInit = ret;
	if (ret >= 0)
		return 0;
	return ret;
}

__attribute__((noinline))
void adhocSplTerm(void)
{
	if (g_adhocSplMutexInit >= 0)
		sceKernelDeleteLwMutex(&g_adhocSplMutex);
	g_adhocSplMutexInit = -1;
}

__attribute__((noinline))
int adhocSplNet(void)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_adhocSplMutex, 1, NULL);
	if (ret >= 0)
		return 1;
	return ret;
}

__attribute__((noinline))
int adhocSplX(int s)
{
	if (s > 0)
		return sceKernelUnlockLwMutex(&g_adhocSplMutex, 1);
	return s;
}

__attribute__((noinline))
void *adhocMalloc(SceSize size)
{
	return SceNetInternal_A2C2BC5F(size, 0);
}

__attribute__((noinline))
void adhocFree(void *ptr)
{
	if (ptr != NULL)
		SceNetInternal_E62A2FAA(ptr);
}
