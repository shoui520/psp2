#include <sce/libkernel_backends.h>
#include <psp2/kernel/threadmgr.h>

struct ThreadEventHandlerOptions {
	SceKernelThreadEventHandler handler;
	void *argument;
};


int sceKernelStartThread(SceUID id, SceSize size, void *argument)
{
	return _sceKernelStartThread(id, size, argument);
}

int sceKernelWaitThreadEnd(SceUID id, int *status, SceUInt *timeout)
{
	return _sceKernelWaitThreadEnd(id, status, timeout);
}

int sceKernelWaitThreadEndCB(SceUID id, int *status, SceUInt *timeout)
{
	return _sceKernelWaitThreadEndCB(id, status, timeout);
}

SceUID sceKernelRegisterThreadEventHandler(const char *name, SceUID id,
		SceInt32 mask, SceKernelThreadEventHandler handler, void *argument)
{
	struct ThreadEventHandlerOptions options = { handler, argument };

	return _sceKernelRegisterThreadEventHandler(name, id, mask, &options);
}
