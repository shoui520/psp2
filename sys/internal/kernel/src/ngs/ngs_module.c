/* SceNgs process lifecycle integration. Internal Sony names are unavailable. */

#include <sysmem/procevent.h>
#include <stddef.h>

int ngsProcessRelease(SceUID pid);
int ngsProcessInitialize(SceUID pid);
int ngsProcessSuspend(SceUID pid);
int ngsProcessResume(SceUID pid);
int ngsPlatformInitialize(void);
int ngsPlatformShutdown(void);

int ngsProcessKill(SceUID pid, ProcEventParam *event, void *common)
{
	int result = ngsProcessRelease(pid);

	return result & (result >> 31);
}

int ngsProcessExit(SceUID pid, ProcEventParam *event, void *common)
{
	int result = ngsProcessRelease(pid);

	return result & (result >> 31);
}

int ngsProcessCreate(SceUID pid, ProcEventCreateParam *event, void *common)
{
	int result = ngsProcessInitialize(pid);

	return result & (result >> 31);
}

int ngsProcessStart(SceUID pid, int eventType,
		    ProcEventParam *event, void *common)
{
	int result;

	if (eventType != 0x20001)
		return 0;
	result = ngsProcessResume(pid);
	return result & (result >> 31);
}

int ngsProcessStop(SceUID pid, int eventType,
		   ProcEventParam *event, void *common)
{
	int result;

	if (eventType != 0x100)
		return 0;
	result = ngsProcessSuspend(pid);
	return result & (result >> 31);
}

int module_start(SceSize arglen, const void *argp)
{
	SceKernelProcEventHandler handler;
	int result;

	if (ngsPlatformInitialize() >= 0) {
		handler.create = ngsProcessCreate;
		handler.exit = ngsProcessExit;
		handler.kill = ngsProcessKill;
		handler.suspend = ngsProcessStop;
		handler.resume = ngsProcessStart;
		handler.notify = NULL;
		handler.size = sizeof(handler);
		if (sceKernelRegisterProcEventHandler("SceNgs", &handler, NULL) >= 0)
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
