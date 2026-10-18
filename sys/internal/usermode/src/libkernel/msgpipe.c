#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>

SceUID sceKernelCreateMsgPipe(const char *name, SceUInt32 type,
		SceUInt32 attributes, SceSize size, const SceKernelMsgPipeOptParam *option)
{
	return sceKernelCreateMsgPipeWithLR(name, type, attributes, size, option,
					  __builtin_return_address(0));
}

int sceKernelSendMsgPipe(SceUID id, const void *buffer, SceSize size,
		SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector = { (void *)buffer, size };

	return sceKernelSendMsgPipeVector(id, &vector, 1, mode, result, timeout);
}

int sceKernelSendMsgPipeCB(SceUID id, const void *buffer, SceSize size,
		SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector = { (void *)buffer, size };

	return sceKernelSendMsgPipeVectorCB(id, &vector, 1, mode, result, timeout);
}

int sceKernelTrySendMsgPipe(SceUID id, const void *buffer, SceSize size,
			  SceUInt32 mode, SceSize *result)
{
	SceKernelMsgPipeVector vector = { (void *)buffer, size };

	return sceKernelTrySendMsgPipeVector(id, &vector, 1, mode, result);
}

int sceKernelReceiveMsgPipe(SceUID id, void *buffer, SceSize size,
		SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector = { buffer, size };

	return sceKernelReceiveMsgPipeVector(id, &vector, 1, mode, result, timeout);
}

int sceKernelReceiveMsgPipeCB(SceUID id, void *buffer, SceSize size,
		SceUInt32 mode, SceSize *result, SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector = { buffer, size };

	return sceKernelReceiveMsgPipeVectorCB(id, &vector, 1, mode, result, timeout);
}

int sceKernelTryReceiveMsgPipe(SceUID id, void *buffer, SceSize size,
			     SceUInt32 mode, SceSize *result)
{
	SceKernelMsgPipeVector vector = { buffer, size };

	return sceKernelTryReceiveMsgPipeVector(id, &vector, 1, mode, result);
}
