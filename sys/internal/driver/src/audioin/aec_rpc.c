/* AEC RPC packing and transfers; original source filename unavailable. */
#include "audioin_internal.h"
#include <vnz_wrapper/venezia_rpc.h>

extern int sceCodecEngineDsp0ConvertPhysicalToVirtual(VeneziaRpcThunk *thunk);
extern int sceCodecEngineDsp0ConvertVirtualToPhysical(VeneziaRpcThunk *thunk,
		void *base, SceSize size, const VeneziaMemoryOption *option);
extern int sceCodecEngineDsp0AllocCommunicationMemories(VeneziaRpcThunk *thunk);
extern int sceCodecEngineDsp0MemcpyChain(VeneziaRpcThunk *thunk);
extern int sceCodecEngineDsp0FreeCommunicationMemories(VeneziaRpcThunk *thunk);
extern int sceCodecEngineDsp0GetThreadExecGroup(unsigned int thread);
extern int SceCodecEngineWrapperForDriver_391BD4FD(unsigned int command,
		const unsigned int *input, VeneziaRpcPack pack, VeneziaRpcUnpack unpack);

void unpackAudioInAecContext(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

void unpackAudioInAecSamples(VeneziaRpcThunk *thunk)
{
	AudioInAecRpcContext *context = thunk->memories[0].virtualBase;

	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
	context->sampleBuffers[0] = thunk->memories[1].virtualBase;
	context->sampleBuffers[1] = thunk->memories[2].virtualBase;
}

int packAudioInAecContext(VeneziaRpcThunk *thunk, const unsigned int *arguments,
		unsigned int *output)
{
	VeneziaMemoryOption option;
	int result;

	option.flags = 1;
	option.allocationSize = sizeof(AudioInAecRpcContext);
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk,
			(void *)(uintptr_t)arguments[0], sizeof(AudioInAecRpcContext), &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	result = sceCodecEngineDsp0MemcpyChain(thunk);
	output[0] = thunk->memories[0].physicalBase;
	output[1] = arguments[1];
	output[2] = arguments[2];
	output[3] = arguments[3];
	return result;
}

int packAudioInAecSamples(VeneziaRpcThunk *thunk, const unsigned int *arguments,
		unsigned int *output)
{
	AudioInAecRpcContext *context = (void *)(uintptr_t)arguments[0];
	VeneziaMemoryOption option;
	int result;

	option.flags = 1;
	option.allocationSize = sizeof(*context);
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, context, sizeof(*context), &option);
	option.flags = 1;
	option.allocationSize = context->context[0];
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, context->sampleBuffers[0],
			context->context[0], &option);
	option.flags = 1;
	option.allocationSize = context->context[0];
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, context->sampleBuffers[1],
			context->context[0], &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	context->sampleBuffers[0] = (void *)(uintptr_t)thunk->memories[1].physicalBase;
	context->sampleBuffers[1] = (void *)(uintptr_t)thunk->memories[2].physicalBase;
	result = sceCodecEngineDsp0MemcpyChain(thunk);
	output[0] = thunk->memories[0].physicalBase;
	output[1] = arguments[1];
	output[2] = arguments[2];
	output[3] = arguments[3];
	return result;
}

int packAudioInAecControl(VeneziaRpcThunk *thunk, const unsigned int *arguments,
		unsigned int *output)
{
	VeneziaMemoryOption option;
	int result;

	option.flags = 1;
	option.allocationSize = sizeof(AudioInAecRpcContext);
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk,
			(void *)(uintptr_t)arguments[0], sizeof(AudioInAecRpcContext), &option);
	option.flags = 17;
	option.allocationSize = 64;
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk,
			(void *)(uintptr_t)arguments[1], 64, &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	result = sceCodecEngineDsp0MemcpyChain(thunk);
	output[0] = thunk->memories[0].physicalBase;
	output[1] = thunk->memories[1].physicalBase;
	output[2] = arguments[2];
	output[3] = arguments[3];
	return result;
}

void unpackAudioInAecControl(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

int queryAudioInAecWorkspace(AudioInAecRpcContext *context)
{
	unsigned int arguments[5];

	arguments[0] = (uintptr_t)context;
	return SceCodecEngineWrapperForDriver_391BD4FD(416, arguments,
			packAudioInAecContext, unpackAudioInAecContext);
}

int initializeAudioInAecContextRpc(AudioInAecRpcContext *context)
{
	unsigned int arguments[5];

	arguments[0] = (uintptr_t)context;
	arguments[1] = sceCodecEngineDsp0GetThreadExecGroup(0);
	return SceCodecEngineWrapperForDriver_391BD4FD(417, arguments,
			packAudioInAecContext, unpackAudioInAecContext);
}

int processAudioInAecContextRpc(AudioInAecRpcContext *context, int gain,
		unsigned int flags)
{
	unsigned int arguments[5];

	arguments[2] = flags;
	arguments[0] = (uintptr_t)context;
	arguments[1] = gain;
	return SceCodecEngineWrapperForDriver_391BD4FD(420, arguments,
			packAudioInAecSamples, unpackAudioInAecSamples);
}

int configureAudioInAecContextRpc(AudioInAecRpcContext *context, const void *control)
{
	unsigned int arguments[5];

	arguments[0] = (uintptr_t)context;
	arguments[1] = (uintptr_t)control;
	return SceCodecEngineWrapperForDriver_391BD4FD(421, arguments,
			packAudioInAecControl, unpackAudioInAecControl);
}
