/* SCE suspend event helpers; no native source-file record survives. */
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <threadmgr/threadmgr.h>

/* Existing import name, NID EA211225; absent from vitasdk's NID table. */

static SceUID suspendWaitEvent;

int CreateSuspendWaitEvent(void)
{
	int event;

	event = ksceKernelCreateEventFlag("SceGpuSuspendWait", 0x9000, 0, NULL);
	if (event < 0)
		return event;
	suspendWaitEvent = event;
	return 0;
}

int sceGpuCancelSuspendWait(void)
{
	return sceKernelCancelEventFlag(suspendWaitEvent, 0, NULL);
}

int sceGpuSignalSuspendWait(void)
{
	return ksceKernelSetEventFlag(suspendWaitEvent, ~0U);
}

int sceGpuWaitSuspend(void)
{
	return ksceKernelWaitEventFlag(suspendWaitEvent, ~0U, SCE_EVENT_WAITOR,
			NULL, NULL);
}
