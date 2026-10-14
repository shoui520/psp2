/* SceNgs process lifecycle integration. Internal Sony names are unavailable. */

#include <psp2kern/kernel/proc_event.h>
#include <stddef.h>

int ngsProcessRelease(SceUID pid);
int ngsProcessInitialize(SceUID pid);
int ngsProcessSuspend(SceUID pid);
int ngsProcessResume(SceUID pid);
int ngsPlatformInitialize(void);
int ngsPlatformShutdown(void);

int ngsProcessKill(SceUID pid, SceProcEventInvokeParam1 *event, int flags)
{
	int result = ngsProcessRelease(pid);

	return result & (result >> 31);
}

int ngsProcessExit(SceUID pid, SceProcEventInvokeParam1 *event, int flags)
{
	int result = ngsProcessRelease(pid);

	return result & (result >> 31);
}

int ngsProcessCreate(SceUID pid, SceProcEventInvokeParam2 *event, int flags)
{
	int result = ngsProcessInitialize(pid);

	return result & (result >> 31);
}

int ngsProcessStart(SceUID pid, int eventType,
		    SceProcEventInvokeParam1 *event, int flags)
{
	int result;

	if (eventType != 0x20001)
		return 0;
	result = ngsProcessResume(pid);
	return result & (result >> 31);
}

int ngsProcessStop(SceUID pid, int eventType,
		   SceProcEventInvokeParam1 *event, int flags)
{
	int result;

	if (eventType != 0x100)
		return 0;
	result = ngsProcessSuspend(pid);
	return result & (result >> 31);
}

int module_start(SceSize arglen, const void *argp)
{
	SceProcEventHandler handler;
	int result;

	if (ngsPlatformInitialize() >= 0) {
		handler.create = ngsProcessCreate;
		handler.exit = ngsProcessExit;
		handler.kill = ngsProcessKill;
		handler.stop = ngsProcessStop;
		handler.start = ngsProcessStart;
		handler.switch_process = NULL;
		handler.size = sizeof(handler);
		if (ksceKernelRegisterProcEventHandler("SceNgs", &handler, 0) >= 0)
			result = 0;
		else
			result = 2;
	} else {
		result = 2;
	}
	return result;
}

int module_stop(SceSize arglen, const void *argp)
{
	return ngsPlatformShutdown();
}
