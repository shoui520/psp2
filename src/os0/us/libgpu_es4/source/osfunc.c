/*
 * SceGpuEs4User: user heap exports and memory helpers.
 */

#include <psp2/kernel/clib.h>
#include "gpu_es4_user.h"

void *SceGpuEs4User_7AF40138(SceSize size)
{
	return PVRSRVAllocUserModeMem(size);
}

void SceGpuEs4User_1BE5985A(void *ptr)
{
	PVRSRVFreeUserModeMem(ptr);
}

/* name unknown: copies transfer commands */
void *GpuMemCopyCommands(void *dst, const void *src, SceSize size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

/* IMG services.h PVRSRVMemSet */
void *PVRSRVMemSet(void *dst, int c, SceSize size)
{
	return sceClibMemset(dst, c, size);
}

/* IMG services.h PVRSRVMemCopy */
void *PVRSRVMemCopy(void *dst, const void *src, SceSize size)
{
	return sceClibMemcpy_safe(dst, src, size);
}
