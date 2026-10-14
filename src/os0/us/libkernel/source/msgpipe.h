#ifndef LIBKERNEL_MSGPIPE_H
#define LIBKERNEL_MSGPIPE_H

#include <psp2/types.h>

typedef struct SceKernelMsgPipeVector {
	void *base;
	SceSize size;
} SceKernelMsgPipeVector;

typedef struct SceKernelMsgPipeOptParam {
	SceSize size;
	SceUInt32 attributes;
	SceInt32 reserved[2];
	SceUInt32 openLimit;
} SceKernelMsgPipeOptParam;

typedef struct SceKernelMsgPipeInfo {
	SceSize size;
	SceUID msgPipeId;
	char name[32];
	SceUInt32 attributes;
	SceSize bufferSize;
	SceSize freeSize;
	SceUInt32 numSendWaitThreads;
	SceUInt32 numReceiveWaitThreads;
} SceKernelMsgPipeInfo;

SceUID sceKernelCreateMsgPipeWithLR(const char *name, SceUInt32 type,
		SceUInt32 attributes, SceSize size,
		const SceKernelMsgPipeOptParam *option, void *caller);
int sceKernelSendMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout);
int sceKernelSendMsgPipeVectorCB(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout);
int sceKernelTrySendMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result);
int sceKernelReceiveMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout);
int sceKernelReceiveMsgPipeVectorCB(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result, SceUInt32 *timeout);
int sceKernelTryReceiveMsgPipeVector(SceUID id, const SceKernelMsgPipeVector *vectors,
		SceUInt32 count, SceUInt32 mode, SceSize *result);

#endif
