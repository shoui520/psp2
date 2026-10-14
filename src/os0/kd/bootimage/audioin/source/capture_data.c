#include "audioin_internal.h"
#include <psp2kern/kernel/proc_event.h>

/* This compiler emits these definitions in reverse order. */
/* Command 421 expands sixteen byte values into a 512-byte interpolated curve.
 * Its 64-byte transport includes adjacent data that the DSP does not consume.
 */
const unsigned char g_audioInAlternateAecControl[16] = {
	255, 255, 255, 255, 255, 255, 255, 127,
	63, 31, 31, 31, 31, 63, 127, 255
};
const int16_t g_audioInSilenceSamples[768] = { 0 };
#if FW_VERSION >= 0x04000000
const unsigned char g_audioInExternalAecControl[16] = {
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255
};
#endif
const char g_audioInAecMutexName[] = "SceAudioAec";
const char g_audioInModuleName[] = "SceAudioIn";
const char g_audioInAecThreadName[] = "SceAudioInAEC";
const char g_audioInInputThreadName[] = "SceAudioInInput";
const char g_audioInAecRequestEventName[] = "SceAudioInReq2";
const char g_audioInRequestEventName[] = "SceAudioInReq";
const char g_audioInMutexName[] = "SceAudioInLock";
const char g_audioInProcessNameFormat[] = "SceAudioInProc%08x";
#if FW_VERSION < 0x04000000
const unsigned char g_audioInExternalAecControl[16] = {
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255
};
#endif
const unsigned char g_audioInInternalAecControl[16] = {
	255, 255, 255, 255, 255, 255, 255, 127,
	63, 31, 31, 31, 31, 63, 127, 255
};
const SceProcEventHandler g_audioInProcEventHandler = {
	.size = sizeof(SceProcEventHandler),
	.create = (int (*)(SceUID, SceProcEventInvokeParam2 *, int))audioInReturnZero,
	.exit = (int (*)(SceUID, SceProcEventInvokeParam1 *, int))audioInProcessExitHandler,
	.kill = (int (*)(SceUID, SceProcEventInvokeParam1 *, int))audioInProcessKillHandler,
	.stop = (int (*)(SceUID, int, SceProcEventInvokeParam1 *, int))audioInProcessStopHandler,
	.start = (int (*)(SceUID, int, SceProcEventInvokeParam1 *, int))audioInProcessStartHandler,
	.switch_process = NULL
};

unsigned int g_audioInAecSampleCount = 0;
unsigned int g_audioInAecWorkspaceSize = 0;
void *g_audioInAecWorkspace = NULL;
struct ThreadmgrFastMutex g_audioInAecLock = { 0 };
AudioInResamplerState g_audioInResamplerState[2] __attribute__((aligned(8))) = { 0 };
int g_audioInDeviceChangeCountdown = 0;
int g_audioInDeviceStateEnabled = 0;
int g_audioInOwnerUnmuteDelay = 0;
int g_audioInInitialized = 0;
int g_audioInOwnerPid = 0;
AudioInCaptureBuffers g_audioInBuffers = { 0 };
unsigned int g_audioInReadyViews = 0;
AudioInSampleView g_audioInSampleViews[9] = { 0 };
AudioInAecContext g_audioInAecContext = { 0 };
unsigned int g_audioInUnidentifiedState = 0;
volatile unsigned int g_audioInAecFlags = 0;
int16_t *g_audioInAecSamples[2] = { NULL, NULL };
unsigned int g_audioInCaptureFreq = 0;
unsigned int g_audioInCaptureGrain = 0;
unsigned int g_audioInRequestFlags = 0;
int g_audioInAecEnabled = 0;
int g_audioInAecEventFlag = 0;
int g_audioInRequestEventFlag = 0;
int g_audioInAecThreadId = 0;
int g_audioInInputThreadId = 0;
int g_audioInAppliedInput = 0;
unsigned int g_audioInOpenPortCount = 0;
int g_audioInChannelSelector = 0;
unsigned int g_audioInRequestedInput = 0;
int g_audioInCaptureDevice = 0;
struct ThreadmgrFastMutex g_audioInLock = { 0 };
int16_t g_audioInCombined8Samples[256] = { 0 };
int16_t g_audioInAecReferenceSamples[1024] = { 0 };
int16_t g_audioInCombined16Samples[1024] = { 0 };
int16_t g_audioInVoice8Samples[256] = { 0 };
int16_t g_audioInSecondary16Samples[1024] = { 0 };
int16_t g_audioInPrimary16Samples[1024] = { 0 };
/* SceAudioForDriver_75915003 DMA destinations must be 16-byte aligned. */
int16_t g_audioInInputSamples[6144] __attribute__((aligned(16))) = { 0 };
int16_t g_audioInSecondary48Samples[1536] = { 0 };
int16_t g_audioInPrimary48Samples[1536] = { 0 };
int g_audioInStartupSilenceFrames = 0;
void (*g_audioInPortClosed)(int pid) = NULL;
#if FW_VERSION >= 0x04000000
void (*g_audioInPortOpened)(int pid) = NULL;
unsigned int g_audioInLegacyCaptureControl = 0;
#else
unsigned int g_audioInLegacyCaptureControl = 0;
void (*g_audioInPortOpened)(int pid) = NULL;
#endif
AudioInPort g_audioInPorts[4] = { 0 };
AudioInLegacyQueue g_audioInLegacyQueue = { 0 };
