/* Sony's run-status API takes one pointer; vitasdk's public prototype differs. */
#include "libkernel_backends.h"
#include <kernel/threadmgr.h>

struct ThreadStatusRequest {
	SceSize size;
};


int sceKernelGetThreadRunStatus(SceKernelThreadRunStatus *status)
{
	struct ThreadStatusRequest request = { status ? status->size : 0 };

	return _sceKernelGetThreadRunStatus(status, &request);
}
