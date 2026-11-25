/*
 * SceRazorCapture: module entry points and public API exports.
 */

#include <stddef.h>
#include <scetypes.h>
#include <razor_capture.h>
#include <kernel/modulemgr.h>

int sceClibPrintf(const char *fmt, ...);
void sceGxmSetInitializeParamInternal(SceUInt32 magic, const void *callbacks);

void razorCaptureGetInitCallbacks(SceUInt32 *magic, void *callbacks);
void razorCapturePreInit(void);
void razorCaptureSetTriggerImpl(SceUInt32 startFrame, SceUInt32 endFrame,
				const char *filename, const char *funcName, SceBool fromHost);
void razorCaptureSetTriggerNextFrameImpl(const char *filename, const char *funcName,
					 SceBool fromHost);
SceBool razorCaptureIsCapturingImpl(void);
void razorCaptureSetCaptureAllMemoryImpl(SceBool enable);
void razorCaptureSetCaptureBeforeKickImpl(SceBool enable);
void razorCaptureEnableSalvageImpl(const char *filename);

int module_start(SceSize args, const void *argp)
{
	SceUInt32 magic;
	SceUInt8 callbacks[0x48];

	(void)args;
	(void)argp;

	razorCaptureGetInitCallbacks(&magic, callbacks);
	sceGxmSetInitializeParamInternal(magic, callbacks);
	razorCapturePreInit();
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;

	sceGxmSetInitializeParamInternal(0, NULL);
	return SCE_KERNEL_STOP_SUCCESS;
}

void sceRazorGpuCaptureSetTrigger(uint32_t frameIndex, const char *captureFilename)
{
	razorCaptureSetTriggerImpl(frameIndex, frameIndex, captureFilename,
				   "sceRazorGpuCaptureSetTrigger", SCE_FALSE);
}

void sceRazorGpuCaptureSetTriggerNextFrame(const char *captureFilename)
{
	razorCaptureSetTriggerNextFrameImpl(captureFilename,
					    "sceRazorGpuCaptureSetTriggerNextFrame", SCE_FALSE);
}

bool sceRazorGpuCaptureIsInProgress(void)
{
	return razorCaptureIsCapturingImpl() != SCE_FALSE;
}

void sceRazorGpuCaptureSetCaptureAllMemory(bool enable)
{
	razorCaptureSetCaptureAllMemoryImpl(enable ? SCE_TRUE : SCE_FALSE);
}

void sceRazorGpuCaptureSetCaptureBeforeKick(bool enable)
{
	razorCaptureSetCaptureBeforeKickImpl(enable ? SCE_TRUE : SCE_FALSE);
}

void sceRazorGpuCaptureStartSalvageMode(void)
{
	sceClibPrintf("razor_capture: Starting salvage mode\n");
	razorCaptureEnableSalvageImpl("host0:salvage.sgx");
}

void sceRazorGpuCaptureEnableSalvage(const char *salvageFilename)
{
	sceClibPrintf("razor_capture: Starting salvage mode\n");
	razorCaptureEnableSalvageImpl(salvageFilename);
}

void sceRazorCaptureSetTrigger(uint32_t frameIndex, const char *captureFilename)
{
	razorCaptureSetTriggerImpl(frameIndex, frameIndex, captureFilename,
				   "sceRazorCaptureSetTrigger", SCE_FALSE);
}

void sceRazorCaptureSetTriggerNextFrame(const char *captureFilename)
{
	razorCaptureSetTriggerNextFrameImpl(captureFilename,
					    "sceRazorCaptureSetTriggerNextFrame", SCE_FALSE);
}

bool sceRazorCaptureIsInProgress(void)
{
	return razorCaptureIsCapturingImpl() != SCE_FALSE;
}
