#include "libkernel_backends.h"
#include <scetypes.h>
#include <kernel/openpsid.h>
#include <libkernel/libkernel.h>

struct RandomNumberRequest {
	SceSize size;
	SceUInt reserved;
};


int sceKernelGetOpenPsId(SceKernelOpenPsId *id)
{
	return _sceKernelGetOpenPsId(id);
}

SceInt32 sceSblACMgrIsGameProgram(SceBool *result)
{
	return _sceSblACMgrIsGameProgram(result);
}

int sceKernelGetRandomNumber(void *output, SceSize size)
{
	struct RandomNumberRequest request;

	request.size = size;
	return _sceKernelGetRandomNumber(output, size, &request);
}
