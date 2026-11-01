/*
 * SceGxm: notifications.
 *
 * The GPU writes notification values into a block of shared memory; a
 * thread waits for one by sleeping on its GPU signal until the word holds
 * the value.
 */

#include "gxm_internal.h"
#include <kernel/error.h>

#define GXM_TLS_GPU_SIGNAL	0x44

static inline void *gxmGetSignal(void)
{
	sceKernelGetTLSAddr(GXM_TLS_GPU_SIGNAL);
	return sceKernelGetTLSAddr(GXM_TLS_GPU_SIGNAL);
}

volatile SceUInt32 *SceGxmInternal_9F7503E8(void)
{
	if (!gxmGlobals.initialized)
		return NULL;
	if (gxmGlobals.top.notifications == NULL)
		return NULL;
	return gxmGlobals.top.notifications->pvLinAddr;
}

volatile SceUInt32 *SceGxmInternal_338E3186(void)
{
	if (!gxmGlobals.initialized)
		return NULL;
	if (gxmGlobals.top.notifications == NULL)
		return NULL;
	return (volatile SceUInt32 *)gxmGlobals.top.notifications->pvLinAddr + 4;
}

volatile SceUInt32 *sceGxmGetNotificationRegion(void)
{
	if (!gxmGlobals.initialized)
		return NULL;
	if (gxmGlobals.top.notifications == NULL)
		return NULL;
	return (volatile SceUInt32 *)gxmGlobals.top.notifications->pvLinAddr + 8;
}

int gxmNotificationCheckAddress(const volatile SceUInt32 *address)
{
	const PVRSRV_CLIENT_MEM_INFO *block = gxmGlobals.top.notifications;

	SceUInt32 base = (SceUInt32)block->pvLinAddr;

	if (((SceUInt32)address < base) | ((SceUInt32)address >= base + block->uAllocSize)) {
		GXM_DBG_ERROR(154, "Notification address %p is not within the notification region.",
			      address);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return 0;
}

int sceGxmNotificationWait(const SceGxmNotification *notification)
{
	volatile SceUInt32 *address;
	SceUInt32 value;

	/* the display queue thread must not block on the GPU here */
	if (gxmDisplayQueueIsCurrentThread()) {
		GXM_DBG_ERROR(164, "The function sceGxmNotificationWait cannot be called from the "
			      "display queue thread.");
		return SCE_GXM_ERROR_INVALID_THREAD;
	}
	if (notification == NULL) {
		GXM_DBG_ERROR_NULL(168, sceGxmNotificationWait, notification);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmNotificationCheckAddress(notification->address) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	gxmRazorMarker(0x1b000000, gxmGlobals.frameIndex);
	address = notification->address;
	value = notification->value;
	while (value != *address)
		sceGpuSignalWait(gxmGetSignal(), 100000);
	gxmRazorMarker(0x1c000000, gxmGlobals.frameIndex);
	return 0;
}

int gxmThreadStateInit(SceUInt32 *state, SceUInt8 *flag)
{
	*state = *(SceUInt32 *)sceKernelGetTLSAddr(GXM_TLS_GPU_SIGNAL);
	*flag = 0;
	return 0;
}

int sceGxmWaitEvent(void)
{
	int err = sceGpuSignalWait(gxmGetSignal(), 100000);

	if (err == 0 || err == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return 0;
	return SCE_GXM_ERROR_DRIVER;
}
