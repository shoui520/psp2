#include <sce/libkernel_backends.h>
#include "lw_sync.h"

struct LwMutexCreateOptions {
	int initialCount;
	const SceKernelLwMutexOptParam *option;
};

struct LwCondCreateOptions {
	SceKernelLwMutexWork *mutex;
	const SceKernelLwCondOptParam *option;
};

struct LwInfoRequest {
	SceSize size;
};


int _sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name,
		unsigned int attributes, int initialCount,
		const SceKernelLwMutexOptParam *option)
{
	struct LwMutexCreateOptions options = { initialCount, option };

	return __sceKernelCreateLwMutex(work, name, attributes, &options);
}

int sceKernelDeleteLwMutex(SceKernelLwMutexWork *work)
{
	return _sceKernelDeleteLwMutex(work);
}

int sceKernelGetLwMutexInfoById(SceUID id, SceKernelLwMutexInfo *info)
{
	struct LwInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetLwMutexInfoById(id, info, &request);
}

int sceKernelCreateLwCond(SceKernelLwCondWork *work, const char *name,
		unsigned int attributes, SceKernelLwMutexWork *mutex,
		const SceKernelLwCondOptParam *option)
{
	struct LwCondCreateOptions options = { mutex, option };

	return _sceKernelCreateLwCond(work, name, attributes, &options);
}

int sceKernelDeleteLwCond(SceKernelLwCondWork *work)
{
	return _sceKernelDeleteLwCond(work);
}

int sceKernelWaitLwCond(SceKernelLwCondWork *work, unsigned int *timeout)
{
	return _sceKernelWaitLwCond(work, timeout);
}

int sceKernelWaitLwCondCB(SceKernelLwCondWork *work, unsigned int *timeout)
{
	return _sceKernelWaitLwCondCB(work, timeout);
}

int sceKernelSignalLwCond(SceKernelLwCondWork *work)
{
	return _sceKernelSignalLwCond(work);
}

int sceKernelSignalLwCondAll(SceKernelLwCondWork *work)
{
	return _sceKernelSignalLwCondAll(work);
}

int sceKernelSignalLwCondTo(SceKernelLwCondWork *work, SceUID threadId)
{
	return _sceKernelSignalLwCondTo(work, threadId);
}

int sceKernelGetLwCondInfo(SceKernelLwCondWork *work, SceKernelLwCondInfo *info)
{
	return _sceKernelGetLwCondInfo(work, info);
}

int sceKernelGetLwCondInfoById(SceUID id, SceKernelLwCondInfo *info)
{
	struct LwInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetLwCondInfoById(id, info, &request);
}
