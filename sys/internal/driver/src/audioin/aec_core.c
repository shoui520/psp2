/* AEC context and workspace management; original source filename unavailable. */
#include "audioin_internal.h"
#include <psp2/audioin.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>

extern int SceCodecEngineWrapperForDriver_CB8703D8(void);
extern int _unlockProcessSuspendCore(void);
extern int SceCodecEngineWrapperForDriver_91535820(unsigned int command,
		unsigned int size);
extern int queryAudioInAecWorkspace(AudioInAecRpcContext *context);
extern int initializeAudioInAecContextRpc(AudioInAecRpcContext *context);
extern int processAudioInAecContextRpc(AudioInAecRpcContext *context, int gain,
		unsigned int flags);
extern int configureAudioInAecContextRpc(AudioInAecRpcContext *context,
		const void *control);

int initializeAudioInAecMutex(void)
{
	int result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&g_audioInAecLock,
			g_audioInAecMutexName, 0x8000, NULL);

	if (result >= 0)
		g_audioInAecSampleCount = 1024;
	return result;
}

int createAudioInAecContext(AudioInAecContext *context)
{
	AudioInAecRpcContext request;
	void *workspace;
	int result;

	if (!context)
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	if (context->size - 16 > 4)
		return SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		return result;
	result = lockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	if (result >= 0) {
		request = (AudioInAecRpcContext){ { NULL, NULL }, { 0, 0 }, NULL, 0 };
		result = queryAudioInAecWorkspace(&request);
		if (result >= 0) {
			g_audioInAecWorkspaceSize = request.workspaceSize;
			workspace = (void *)(uintptr_t)SceCodecEngineWrapperForDriver_91535820(
					1, request.workspaceSize);
			g_audioInAecWorkspace = workspace;
			result = SCE_AUDIO_IN_ERROR_OUT_OF_MEMORY;
			if (workspace) {
				request.sampleBuffers[0] = context->sampleBuffers[0];
				request.sampleBuffers[1] = context->sampleBuffers[1];
				request.context[0] = g_audioInAecSampleCount;
				request.context[1] = 0;
				request.workspace = workspace;
				request.workspaceSize = g_audioInAecWorkspaceSize;
				result = initializeAudioInAecContextRpc(&request);
				if (result >= 0) {
					context->context[0] = request.context[0];
					context->context[1] = request.context[1];
					context->context[0] = g_audioInAecSampleCount;
					result = 0;
				}
			}
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	}
	_unlockProcessSuspendCore();
	return result;
}

int processAudioInAecContext(AudioInAecContext *context, int gain,
		unsigned int flags)
{
	AudioInAecRpcContext request;
	int result;

	if (!context)
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	if (context->size - 16 > 4)
		return SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	if (!g_audioInAecWorkspace)
		return SCE_AUDIO_IN_ERROR_FATAL;
	if (!context->sampleBuffers[0] || !context->sampleBuffers[1])
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	if ((unsigned int)((gain >> 8) + 121) > 121u ||
	    (flags & 15) > 3)
		return SCE_AUDIO_IN_ERROR_FATAL;
	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		return result;
	result = lockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	if (result >= 0) {
		request.sampleBuffers[0] = context->sampleBuffers[0];
		request.workspace = g_audioInAecWorkspace;
		request.workspaceSize = g_audioInAecWorkspaceSize;
		request.sampleBuffers[1] = context->sampleBuffers[1];
		request.context[0] = g_audioInAecSampleCount;
		request.context[1] = 0;
		result = processAudioInAecContextRpc(&request, gain, flags);
		if (result >= 0) {
			context->context[0] = request.context[0];
			context->context[1] = request.context[1];
			result = 0;
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	}
	_unlockProcessSuspendCore();
	return result;
}

int configureAudioInAecContext(AudioInAecContext *context, const void *control)
{
	AudioInAecRpcContext request;
	int result;

	if (!context)
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	if (context->size - 16 > 4)
		return SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	if (!control)
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		return result;
	result = lockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	if (result >= 0) {
		request.workspaceSize = g_audioInAecWorkspaceSize;
		request.sampleBuffers[0] = context->sampleBuffers[0];
		request.sampleBuffers[1] = context->sampleBuffers[1];
		request.workspace = g_audioInAecWorkspace;
		request.context[0] = g_audioInAecSampleCount;
		request.context[1] = 0;
		result = configureAudioInAecContextRpc(&request, control);
		if (result >= 0) {
			context->context[0] = request.context[0];
			context->context[1] = request.context[1];
			result = 0;
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInAecLock);
	}
	_unlockProcessSuspendCore();
	return result;
}
