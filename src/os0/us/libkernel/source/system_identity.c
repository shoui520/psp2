#include <sce/libkernel_backends.h>
#include <psp2/types.h>
#include <psp2/kernel/openpsid.h>
#include <psp2/kernel/rng.h>
#include <psp2/sblacmgr.h>

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
