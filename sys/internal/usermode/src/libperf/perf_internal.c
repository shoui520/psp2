/*
 * ScePerfInternal: thin user-mode wrappers over ScePamgr syscalls.
 */

#include "libperf_internal.h"

int ScePerfInternal_8AE83E02(void *data)
{
	return _sceKernelPaGetGpuSampledData(data);
}

int ScePerfInternal_26A6DF2C(SceUInt32 key, const void *param)
{
	return _sceKernelPaSetupTraceBufferByKey(key, param);
}

int ScePerfInternal_275F2475(SceUInt32 key, const void *param)
{
	return _sceKernelPaAddCounterTraceByKey(key, param);
}

int ScePerfInternal_35405E9B(SceUInt32 key, const void *param)
{
	return _sceKernelPaAddArmTraceByKey(key, param);
}

int ScePerfInternal_9D1E1CEE(SceUInt32 key, const void *param)
{
	return _sceKernelPaAddBusTraceByKey(key, param);
}

int ScePerfInternal_37D9191E(SceUInt32 key, const void *param)
{
	return _sceKernelPaAddGpuTraceByKey(key, param);
}
