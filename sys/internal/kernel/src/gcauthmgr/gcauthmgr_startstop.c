/* Module lifecycle. */
#include <sysmem/procevent.h>

unsigned char gcAuthProcessStopBlocked;

int GcAuthInitializeDrmBb(void);
int GcAuthInitializePcact(void);
int GcAuthInitializeAdhocBb(void);
int GcAuthInitializeMsSaveBb(void);
int GcAuthResetState(void);
int GcAuthInitializeGcAuth(void);

int GcAuthProcessStopHandler(SceUID pid, int eventType,
			   ProcEventParam *param, int unused)
{
	if (eventType != 0x202)
		return 0;
	return gcAuthProcessStopBlocked != 0 ? -1 : 0;
}

int module_start(SceSize arglen, const void *argp)
{
	SceKernelProcEventHandler handler;

	gcAuthProcessStopBlocked = 0;
	handler.create = 0;
	handler.exit = 0;
	handler.kill = 0;
	handler.resume = 0;
	handler.notify = 0;
	handler.suspend = GcAuthProcessStopHandler;
	handler.size = sizeof(handler);
	sceKernelRegisterProcEventHandler("SceSblGcAuthMgr", &handler, 0);
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
