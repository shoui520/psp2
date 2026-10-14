#include "lw_sync.h"

int sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name,
		unsigned int attributes, int initialCount,
		const SceKernelLwMutexOptParam *option)
{
	return _sceKernelCreateLwMutex(work, name, attributes, initialCount, option);
}
