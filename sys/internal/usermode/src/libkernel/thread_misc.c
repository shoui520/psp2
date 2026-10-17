#include "libkernel_backends.h"
#include <psp2/types.h>

/* These wrappers inspect only the size prefix of the opaque caller records. */
struct ThreadSizedRecord {
	SceSize size;
};

struct ThreadRecordRequest {
	SceSize size;
	SceUInt reserved;
};

struct ThreadContextRequest {
	SceSize cpuSize;
	SceSize vfpSize;
	SceUInt reserved[2];
};

struct SceKernelSimpleEventOptParam {
	SceSize size;
};


int sceKernelGetThreadEventInfo(SceUID id, struct ThreadSizedRecord *info)
{
	struct ThreadRecordRequest request;

	request.size = info ? info->size : 0;
	return _sceKernelGetThreadEventInfo(id, info, &request);
}

int sceKernelSetThreadContextForVM(SceUID id,
		const struct ThreadSizedRecord *cpu,
		const struct ThreadSizedRecord *vfp)
{
	struct ThreadContextRequest request;

	request.cpuSize = cpu ? cpu->size : 0;
	request.vfpSize = vfp ? vfp->size : 0;
	return _sceKernelSetThreadContextForVM(id, cpu, vfp, &request);
}

int sceKernelGetThreadContextForVM(SceUID id, struct ThreadSizedRecord *cpu,
		struct ThreadSizedRecord *vfp)
{
	struct ThreadContextRequest request;

	request.cpuSize = cpu ? cpu->size : 0;
	request.vfpSize = vfp ? vfp->size : 0;
	return _sceKernelGetThreadContextForVM(id, cpu, vfp, &request);
}

int sceKernelPMonThreadGetCounter(SceUID id, unsigned int counter,
		SceUInt32 *value)
{
	return _sceKernelPMonThreadGetCounter(id, counter, value);
}

int sceKernelGetSystemTime(SceUInt64 *time)
{
	return _sceKernelGetSystemTime(time);
}

int sceKernelWaitException(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout)
{
	struct ThreadRecordRequest request;

	request.size = info ? info->size : 0;
	return _sceKernelWaitException(id, info, timeout, &request);
}

int sceKernelWaitExceptionCB(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout)
{
	struct ThreadRecordRequest request;

	request.size = info ? info->size : 0;
	return _sceKernelWaitExceptionCB(id, info, timeout, &request);
}

SceUID sceKernelCreateSimpleEvent(const char *name, SceUInt attributes,
		SceUInt pattern, const struct SceKernelSimpleEventOptParam *option)
{
	return _sceKernelCreateSimpleEvent(name, attributes, pattern, option);
}

int sceKernelWaitSignal(SceUInt64 value, SceUInt *timeout)
{
	return _sceKernelWaitSignal(value, timeout);
}

int sceKernelWaitSignalCB(SceUInt64 value, SceUInt *timeout)
{
	return _sceKernelWaitSignalCB(value, timeout);
}
