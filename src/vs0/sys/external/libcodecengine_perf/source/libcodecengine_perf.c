/*
 * SceCodecEnginePerf: the CodecEngine debug and performance entry points, each
 * a forward to the SceCodecEngineWrapper syscall of the same name.
 *
 * The public names of the six NID-only exports follow their syscalls. The
 * kernel side of those six ignores its arguments and returns 0, so their
 * argument lists are not known (PLACEHOLDER: argument lists).
 */
#include <psp2/types.h>

typedef struct SceCodecEnginePmonProcessorLoad {
	SceUInt32 size;
	SceUInt32 average;
} SceCodecEnginePmonProcessorLoad;

/* what the syscall is told of the caller's structure */
typedef struct SceCodecEnginePmonProcessorLoadOpt {
	SceUInt32 size;			/* the caller's size field, 0 without a structure */
	SceUInt32 reserved;		/* not set */
} SceCodecEnginePmonProcessorLoadOpt;

extern SceInt32 _sceCodecEnginePmonStart(void);
extern SceInt32 _sceCodecEnginePmonStop(void);
extern SceInt32 _sceCodecEnginePmonGetProcessorLoad(SceCodecEnginePmonProcessorLoad *pProcessorLoad,
						    SceCodecEnginePmonProcessorLoadOpt *opt);
extern SceInt32 _sceCodecEnginePmonReset(void);
extern SceInt32 _sceCodecEngineGetNumRpcCalled(void);
extern SceInt32 _sceCodecEngineResetNumRpcCalled(void);
extern SceInt32 _sceCodecEngineGetProcessorLoad(void *pLoad);
extern SceInt32 _sceCodecEngineGetMemoryState(void *pState);
extern SceInt32 _sceCodecEngineChangeNumWorkerCores(void);
extern SceInt32 _sceCodecEngineSetClockFrequency(void);

SceInt32 sceCodecEnginePmonStart(void)
{
	return _sceCodecEnginePmonStart();
}

SceInt32 sceCodecEnginePmonStop(void)
{
	return _sceCodecEnginePmonStop();
}

SceInt32 sceCodecEnginePmonGetProcessorLoad(SceCodecEnginePmonProcessorLoad *pProcessorLoad)
{
	SceCodecEnginePmonProcessorLoadOpt opt;

	opt.size = pProcessorLoad != NULL ? pProcessorLoad->size : 0;
	return _sceCodecEnginePmonGetProcessorLoad(pProcessorLoad, &opt);
}

SceInt32 sceCodecEnginePmonReset(void)
{
	return _sceCodecEnginePmonReset();
}

SceInt32 sceCodecEngineGetNumRpcCalled(void)
{
	return _sceCodecEngineGetNumRpcCalled();
}

SceInt32 sceCodecEngineResetNumRpcCalled(void)
{
	return _sceCodecEngineResetNumRpcCalled();
}

SceInt32 sceCodecEngineGetProcessorLoad(void *pLoad)
{
	return _sceCodecEngineGetProcessorLoad(pLoad);
}

SceInt32 sceCodecEngineGetMemoryState(void *pState)
{
	return _sceCodecEngineGetMemoryState(pState);
}

SceInt32 sceCodecEngineChangeNumWorkerCores(void)
{
	return _sceCodecEngineChangeNumWorkerCores();
}

SceInt32 sceCodecEngineSetClockFrequency(void)
{
	return _sceCodecEngineSetClockFrequency();
}
