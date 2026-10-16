/* Module lifecycle. */
#include <psp2kern/kernel/proc_event.h>

unsigned char gcAuthProcessStopBlocked;

int GcAuthInitializeDrmBb(void);
int GcAuthInitializePcact(void);
int GcAuthInitializeAdhocBb(void);
int GcAuthInitializeMsSaveBb(void);
int GcAuthResetState(void);
int GcAuthInitializeGcAuth(void);

int GcAuthProcessStopHandler(SceUID pid, int eventType,
			   SceProcEventInvokeParam1 *param, int unused)
{
	if (eventType != 0x202)
		return 0;
	return gcAuthProcessStopBlocked != 0 ? -1 : 0;
}

int module_start(SceSize arglen, const void *argp)
{
	SceProcEventHandler handler;

	gcAuthProcessStopBlocked = 0;
	handler.create = 0;
	handler.exit = 0;
	handler.kill = 0;
	handler.start = 0;
	handler.switch_process = 0;
	handler.stop = GcAuthProcessStopHandler;
	handler.size = sizeof(handler);
	ksceKernelRegisterProcEventHandler("SceSblGcAuthMgr", &handler, 0);
	if (GcAuthInitializeDrmBb() != 0 ||
	    GcAuthInitializePcact() != 0 ||
	    GcAuthInitializeAdhocBb() != 0 ||
	    GcAuthInitializeMsSaveBb() != 0)
		return 1;
	GcAuthResetState();
	return GcAuthInitializeGcAuth() != 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	return 0;
}
