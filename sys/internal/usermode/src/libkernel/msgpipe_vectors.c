#include "libkernel_backends.h"
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>

struct MsgPipeCreateOptions {
	SceSize size;
	const SceKernelMsgPipeOptParam *option;
	void *caller;
};

struct MsgPipeWaitOptions {
	SceUInt32 mode;
	SceSize *result;
	SceUInt32 *timeout;
};

struct MsgPipeTryOptions {
	SceUInt32 mode;
	SceSize *result;
};

struct MsgPipeInfoRequest {
	SceSize size;
};


SceUID sceKernelCreateMsgPipeWithLR(const char *name, SceUInt32 type,
		SceUInt32 attributes, SceSize size,
		const SceKernelMsgPipeOptParam *option, void *caller)
{
	struct MsgPipeCreateOptions options = { size, option, caller };

	return _sceKernelCreateMsgPipeWithLR(name, type, attributes, &options);
}

int sceKernelSendMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	struct MsgPipeWaitOptions options = { mode, result, timeout };

	return _sceKernelSendMsgPipeVector(id, vectors, count, &options);
}

int sceKernelSendMsgPipeVectorCB(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	struct MsgPipeWaitOptions options = { mode, result, timeout };

	return _sceKernelSendMsgPipeVectorCB(id, vectors, count, &options);
}

int sceKernelTrySendMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result)
{
	struct MsgPipeTryOptions options = { mode, result };

	return _sceKernelTrySendMsgPipeVector(id, vectors, count, &options);
}

int sceKernelReceiveMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	struct MsgPipeWaitOptions options = { mode, result, timeout };

	return _sceKernelReceiveMsgPipeVector(id, vectors, count, &options);
}

int sceKernelReceiveMsgPipeVectorCB(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	struct MsgPipeWaitOptions options = { mode, result, timeout };

	return _sceKernelReceiveMsgPipeVectorCB(id, vectors, count, &options);
}

int sceKernelTryReceiveMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result)
{
	struct MsgPipeTryOptions options = { mode, result };

	return _sceKernelTryReceiveMsgPipeVector(id, vectors, count, &options);
}

int sceKernelCancelMsgPipe(SceUID id, SceUInt32 *sendWaiting, SceUInt32 *receiveWaiting)
{
	return _sceKernelCancelMsgPipe(id, sendWaiting, receiveWaiting);
}

int sceKernelGetMsgPipeInfo(SceUID id, SceKernelMsgPipeInfo *info)
{
	struct MsgPipeInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetMsgPipeInfo(id, info, &request);
}
