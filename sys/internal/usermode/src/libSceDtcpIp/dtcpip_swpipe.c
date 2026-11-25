/*
 * SceDtcpIp: the SW IPC primitives. The middleware owns a set of kernel objects the
 * library reaches by name: the control mutex ("SW_IPC_Sys_Mutex"), the request pipes
 * ("SW_IPC_Sys_Pipe_CtoS"/"StoC"), the shared memory ("SW_IPC_Sys_ShMem") and the
 * per-stream objects. The names and the protocol are the middleware's; the function
 * names are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem/memblock.h>
#include <appmgr.h>
#include <string.h>
#include <libkernel/libkernel.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the shared stream buffer of one channel: a 4 KiB header, then the stream data */
#define SW_IPC_STREAM_STRIDE		16520
#define SW_IPC_CHANNEL_COUNT		2
#define SW_IPC_STREAM_COUNT		4

/* the SW IPC statuses the protocol answers with (ours) */
#define SW_IPC_OK			0
#define SW_IPC_ERROR_FATAL		12002
#define SW_IPC_ERROR_PARAM		12003
#define SW_IPC_ERROR_MEMORY		12008
#define SW_IPC_ERROR_STREAM		12009
#define SW_IPC_ERROR_LAUNCH		12010
#define SW_IPC_ERROR_PIPE		12011
#define SW_IPC_ERROR_SHARED		12012
#define SW_IPC_ERROR_BUSY		12004
#define SW_IPC_ERROR_CLOSED		12017
#define SW_IPC_ERROR_TIMEOUT		(-2147319803)

/* the message pipe's events */
#define SW_IPC_EVENT_DATA		0x00000002
#define SW_IPC_EVENT_CLOSED		0x00000008
#define SW_IPC_EVENT_CONNECT		0x00000010
#define SW_IPC_EVENT_ERROR		0x00000200
#define SW_IPC_EVENT_SIGNAL		0x01000000

/* the shared memory block's object: its name, its memblock and the base */
struct SwIpcShared {
	char name[32];		/* 0x00: the memblock's name */
	SceSize size;		/* 0x20: its size */
	SceUID memBlock;	/* 0x24: the memblock */
	void *base;		/* 0x28: its base */
};

struct SwIpcClient;

/* one channel of a client: its pipes and its stream buffer */
struct SwIpcChannel {
	struct SwIpcClient *client;	/* 0x00: the client it belongs to */
	unsigned int index;		/* 0x04: its index (0 or 1) */
	int reserved0;			/* 0x08 */
	void *sharedBase;		/* 0x0C: the shared memory's base */
	struct SwIpcShared *shared;	/* 0x10: the shared memory object */
	SceUID pipeSingle;		/* 0x14: the single stream's CtoS pipe */
	SceSize size;			/* 0x18: nonzero: the streams use the shared buffer */
	SceUID pipeStoC[4];		/* 0x1C: the streams' StoC pipes */
	SceUID pipeCtoS[4];		/* 0x2C: the streams' CtoS pipes */
};

/* the client: its mutex, its shared memory, its launch handle and its channels */
struct SwIpcClient {
	SceUID mutex;			/* 0x00: "SW_IPC_Sys_Mutex" */
	struct SwIpcShared *shared;	/* 0x04: "SW_IPC_Sys_ShMem" */
	void *launch;			/* 0x08: the middleware's launch handle */
	void *sharedBase;		/* 0x0C: the shared memory's base */
	SceUID pipeCtoS;		/* 0x10: "SW_IPC_Sys_Pipe_CtoS" */
	SceUID pipeStoC;		/* 0x14: "SW_IPC_Sys_Pipe_StoC" */
	struct SwIpcChannel channels[2];	/* 0x18 */
};

/* a control message as it travels over the message pipe (0x20 bytes) */
struct SwIpcWireMessage {
	int reserved0[2];	/* 0x00 */
	int kind;		/* 0x08: 1 connect, 2 connected, 3 disconnect, ... */
	int result;		/* 0x0C: the status */
	int value0;		/* 0x10 */
	int value1;		/* 0x14 */
	int value2;		/* 0x18 */
	int value3;		/* 0x1C */
};

/* the message pipe's option block */
struct SwIpcPipeOpt {
	int size;
	int waitMode;
	int flags;
	int reserved[2];
};

/* the memblock option this SDK's modules pass (0x38 bytes; the tree's SDK header
 * carries the older 0x14-byte block, so the library's is its own) */
struct SwIpcMemBlockOpt {
	SceSize size;			/* 0x00: 0x40 */
	SceUInt32 attr;			/* 0x04: 0x4020 */
	SceUInt32 reserved0[3];		/* 0x08 */
	SceUInt32 flags;		/* 0x14: the allocation's flags (48) */
	SceUInt32 reserved1[4];		/* 0x18 */
	SceUInt32 blockSize;		/* 0x28: the block's unit (32) */
	SceUInt32 reserved2[6];		/* 0x2C */
};

/* copy a shared memory object's words (its memblock and its base) */
__attribute__((noinline)) int dtcpIpIpcInitShared(void *stream, int *outShared)
{
	int *shared = stream;

	outShared[0] = shared[1];
	outShared[1] = shared[2];
	outShared[2] = shared[3];
	return 0;
}

/* take the protocol's mutex: the created object, or one the middleware owns by name */
__attribute__((noinline)) int dtcpIpSwCreateMutex(SceUID *outMutex, const char *name, int create)
{
	SceUID mutex;

	if (create != 0)
		mutex = sceKernelCreateMutex(name, 0x2080, 0, NULL);
	else
		mutex = sceKernelOpenMutex(name);
	if (mutex < 0)
		return SW_IPC_ERROR_PIPE;
	*outMutex = mutex;
	return 0;
}

__attribute__((noinline)) int dtcpIpSwDeleteMutex(SceUID mutex, int destroy)
{
	if (destroy != 0)
		sceKernelDeleteMutex(mutex);
	else
		sceKernelCloseMutex(mutex);
	return 0;
}

__attribute__((noinline)) int dtcpIpSwLock(SceUID mutex)
{
	if (sceKernelLockMutex(mutex, 1, NULL) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

__attribute__((noinline)) int dtcpIpSwUnlock(SceUID mutex)
{
	if (sceKernelUnlockMutex(mutex, 1) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

/* create a request pipe the middleware opens by name */
__attribute__((noinline)) int dtcpIpSwCreatePipe(const char *name, SceUID *outPipe)
{
	struct SwIpcPipeOpt option;
	SceUID pipe;

	option.size = 20;
	option.waitMode = 1;
	option.flags = 133;
	pipe = sceKernelCreateMsgPipe(name, 64, 387, 0x1000, &option);
	if (pipe < 0)
		return SW_IPC_ERROR_PIPE;
	*outPipe = pipe;
	return 0;
}

__attribute__((noinline)) int dtcpIpSwDeletePipe(SceUID pipe)
{
	sceKernelDeleteMsgPipe(pipe);
	return 0;
}

/* open a request pipe the middleware created */
__attribute__((noinline)) int dtcpIpSwOpenPipe(const char *name, SceUID *outPipe)
{
	SceUID pipe;

	pipe = sceKernelOpenMsgPipe(name);
	if (pipe < 0)
		return SW_IPC_ERROR_PIPE;
	*outPipe = pipe;
	return 0;
}

__attribute__((noinline)) int dtcpIpSwClosePipe(SceUID pipe)
{
	sceKernelCloseMsgPipe(pipe);
	return 0;
}

/* send a 0x20-byte message block over a pipe */
__attribute__((noinline)) int dtcpIpSwSend(SceUID pipe, const void *message)
{
	unsigned int sent = 0;

	if (sceKernelSendMsgPipe(pipe, (void *)message, 0x20, 1, &sent, NULL) != 0)
		return SW_IPC_ERROR_PIPE;
	if (sent == 0x20)
		return 0;
	return SW_IPC_ERROR_PIPE;
}

/*
 * receive a 0x20-byte message block over a pipe: the pipe's events say what arrived
 * (data, the peer's close, its connect, an error); the timeout argument is the wait
 * time in microseconds (0 waits forever).
 */
__attribute__((noinline)) int dtcpIpSwReceive(SceUID pipe, void *message, int timeout)
{
	int waitTime = timeout;
	int sent = 0;
	unsigned int event = 0;
	int eventValue = 0;
	int *waitTimePointer;

	if (waitTime != 0)
		waitTimePointer = &waitTime;
	else
		waitTimePointer = NULL;
	if (sceKernelReceiveMsgPipe(pipe, message, 0x20, 17, &sent, waitTimePointer) != 0)
		return SW_IPC_ERROR_PIPE;
	if (sceKernelWaitEvent(pipe, 538, &event, &eventValue, NULL) != 0)
		return SW_IPC_ERROR_PIPE;
	if ((event & SW_IPC_EVENT_CLOSED) != 0)
		return SW_IPC_ERROR_PIPE;
	if ((event & SW_IPC_EVENT_CONNECT) != 0) {
		if (eventValue == SW_IPC_ERROR_TIMEOUT)
			return SW_IPC_ERROR_CLOSED;
		return SW_IPC_ERROR_PIPE;
	}
	if ((event & SW_IPC_EVENT_ERROR) != 0)
		return SW_IPC_ERROR_PIPE;
	if ((event & SW_IPC_EVENT_DATA) != 0) {
		if (sent == 0x20)
			return 0;
		return SW_IPC_ERROR_PIPE;
	}
	dtcpIpPrintf("SwMwIPCReceiveEventMsgPipe : fatal error\n");
	return SW_IPC_ERROR_PIPE;
}

/* signal the peer's event flag */
__attribute__((noinline)) int dtcpIpSwSignal(SceUID event, int value)
{
	/* the value travels as the event's user data (its high word) */
	if (sceKernelSetEvent(event, SW_IPC_EVENT_SIGNAL, (SceUInt64)value << 32) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

/* wait for the peer's event flag */
__attribute__((noinline)) int dtcpIpSwWait(SceUID event, int *outValue, int timeout)
{
	int waitTime = timeout;
	int eventValue = 0;
	unsigned int flags = 0;
	int *waitTimePointer;
	int ret;

	if (waitTime != 0)
		waitTimePointer = &waitTime;
	else
		waitTimePointer = NULL;
	ret = sceKernelWaitEvent(event, SW_IPC_EVENT_SIGNAL | SW_IPC_EVENT_ERROR |
				 SW_IPC_EVENT_CONNECT | SW_IPC_EVENT_CLOSED, &flags, &eventValue,
				 waitTimePointer);
	if (ret != 0) {
		if (ret == SW_IPC_ERROR_TIMEOUT)
			return SW_IPC_ERROR_CLOSED;
		return SW_IPC_ERROR_PIPE;
	}
	if ((flags & SW_IPC_EVENT_CLOSED) != 0)
		return SW_IPC_ERROR_PIPE;
	if ((flags & SW_IPC_EVENT_CONNECT) != 0) {
		if (eventValue == SW_IPC_ERROR_TIMEOUT)
			return SW_IPC_ERROR_CLOSED;
		return SW_IPC_ERROR_PIPE;
	}
	if ((flags & SW_IPC_EVENT_ERROR) != 0)
		return SW_IPC_ERROR_PIPE;
	if ((flags & SW_IPC_EVENT_SIGNAL) != 0) {
		*outValue = eventValue;
		return 0;
	}
	return SW_IPC_ERROR_PIPE;
}

/*
 * launch the middleware application: its URI is "dtcpip::" and its application
 * parameter (the 32 bytes of the caller's status at +12), and the launch handle keeps
 * the application alive while the library works.
 */
int dtcpIpIpcLaunchMiddleware(void **outLaunch)
{
	char status[264];
	void *launch;
	int ret;

	launch = dtcpIpAlloc(4);
	if (launch == NULL)
		return 0;	/* Sony's path returns 0 here, with no handle */
	*(int *)launch = 0;
	ret = sceAppMgrGetStatusByName(NULL, status);
	if (ret != 0)
		goto fail;
	memcpy(&status[0x80], "dtcpip:", 8);
	dtcpIpStrncpy(&status[0x87], ":", 1);
	dtcpIpStrncpy(&status[0x88], status + 12, 32);
	if (sceAppMgrLaunchAppByUri(0x10000, &status[0x80]) != 0) {
		ret = SW_IPC_ERROR_LAUNCH;
		goto fail;
	}
	*outLaunch = launch;
	return 0;
fail:
	dtcpIpFree(launch);
	return ret;
}

/* release a launch handle's slot */
__attribute__((noinline)) int dtcpIpSwFreeLaunch(void *launch, int *slot)
{
	*slot = 0;
	if (launch != NULL)
		dtcpIpFree(launch);
	return 0;
}

/*
 * the shared memory block: the middleware creates it and the library opens it by
 * name; the caller's base receives its address.
 */
int dtcpIpSwCreateShared(struct SwIpcShared **outShared, const char *name, SceSize size,
			 void **outBase, int open)
{
	struct SwIpcShared *shared;
	struct SwIpcMemBlockOpt option;
	SceUID memBlock;
	SceSize rounded;
	int ret;

	rounded = (size + 4095) & 0xFFFFF000;
	shared = dtcpIpAlloc(sizeof(*shared));
	if (shared == NULL)
		return SW_IPC_ERROR_SHARED;
	dtcpIpSwClear(shared, 0, 32);
	shared->size = rounded;
	shared->memBlock = 0;
	shared->base = NULL;
	if (open != 0) {
		dtcpIpStrncpy(shared->name, name, 32);
		memBlock = sceKernelOpenMemBlock(name, 48);
		shared->memBlock = memBlock;
		if (memBlock < 0)
			goto fail;
	} else {
		option.reserved0[0] = 0;
		option.reserved0[1] = 0;
		option.reserved0[2] = 0;
		option.reserved1[0] = 0;
		option.reserved1[1] = 0;
		option.reserved1[2] = 0;
		option.reserved1[3] = 0;
		option.reserved2[0] = 0;
		option.reserved2[1] = 0;
		option.reserved2[2] = 0;
		option.reserved2[3] = 0;
		option.reserved2[4] = 0;
		option.reserved2[5] = 0;
		option.size = 64;
		option.attr = 16416;
		option.flags = 48;
		option.blockSize = 32;
		dtcpIpStrncpy(shared->name, name, 32);
		memBlock = sceKernelAllocMemBlock(name, 0xC20D060, shared->size, &option);
		shared->memBlock = memBlock;
		if (memBlock < 0)
			goto fail;
	}
	if (sceKernelGetMemBlockBase(memBlock, &shared->base) >= 0) {
		*outBase = shared->base;
		*outShared = shared;
		return 0;
	}
	if (open != 0)
		sceKernelCloseMemBlock(shared->memBlock);
	else
		sceKernelFreeMemBlock(shared->memBlock);
fail:
	dtcpIpFree(shared);
	return SW_IPC_ERROR_SHARED;
}

/* open the middleware's shared memory block by name */
int dtcpIpSwOpenShared(struct SwIpcShared **outShared, const char *name, SceSize size,
		       void **outBase)
{
	return dtcpIpSwCreateShared(outShared, name, size, outBase, 1);
}

/* close a shared memory block */
__attribute__((noinline)) int dtcpIpSwCloseShared(struct SwIpcShared *shared)
{
	sceKernelCloseMemBlock(shared->memBlock);
	dtcpIpFree(shared);
	return 0;
}

/* bounded copies into the shared memory (the region's size is 1 GiB) */
__attribute__((noinline)) int dtcpIpSwCopy(void *destination, const void *source, SceSize size)
{
	return sceClibMemcpyChk(destination, source, size, 0x40000000);
}

__attribute__((noinline)) int dtcpIpSwClear(void *destination, int value, SceSize size)
{
	return sceClibMemsetChk(destination, value, size, 0x40000000);
}

/* the client's own layout (dtcpip_swpipe.c's structs are shared with the swipc.c
 * object model; the offsets are the ones the engines use) */
int dtcpIpSwRequest(struct SwIpcChannel *channel, struct SceDtcpIpIpcMessage *message,
		    SceUID pipe);

/*
 * create a client: the control mutex, the CtoS pipe, the StoC pipe (the middleware may
 * not have it yet: open it for a while), the shared memory, and the two channels'
 * bookkeeping.
 */
int dtcpIpSwInitClient(struct SwIpcClient **outClient, const unsigned int *config, SceSize size)
{
	struct SwIpcClient *client;
	struct SwIpcWireMessage message;
	void *base;
	int ret;
	int channel;
	int tries;
	unsigned int i;

	if (config == NULL || size < 4)
		return 12003;

	*outClient = NULL;
	client = dtcpIpMalloc(sizeof(*client));
	if (client == NULL)
		return 12008;

	client->mutex = -1;
	client->pipeCtoS = -1;
	client->pipeStoC = -1;
	ret = dtcpIpSwCreatePipe("SW_IPC_Sys_Pipe_CtoS", &client->pipeCtoS);
	if (ret != 0)
		goto fail;
	for (channel = 0; channel < 3; ++channel) {
		ret = dtcpIpIpcLaunchMiddleware(&client->launch);
		if (ret != 0)
			continue;
		for (tries = 0; tries < 70; ++tries) {
			ret = dtcpIpSwOpenPipe("SW_IPC_Sys_Pipe_StoC", &client->pipeStoC);
			if (ret == 0)
				break;
			sceKernelDelayThread(50000);
		}
		if (tries < 70)
			break;
	}
	if (channel == 3) {
		ret = 12009;
		goto fail;
	}
	message.kind = 1;
	message.result = 0;
	message.value0 = 2;
	message.value1 = 0;
	message.value2 = 0;
	message.value3 = 0;
	ret = dtcpIpSwSend(client->pipeCtoS, &message);
	if (ret != 0)
		goto fail;
	ret = dtcpIpSwReceive(client->pipeStoC, &message, 1000000);
	if (ret != 0)
		goto fail;
	if (message.kind != 2 || message.result != 0) {
		ret = 12004;
		goto fail;
	}
	ret = dtcpIpSwCreateMutex(&client->mutex, "SW_IPC_Sys_Mutex", 1);
	if (ret != 0)
		goto fail;
	ret = dtcpIpSwOpenShared(&client->shared, "SW_IPC_Sys_ShMem", 33040, &base);
	if (ret != 0)
		goto fail;
	client->sharedBase = base;
	{
		struct SwIpcChannel *ch = client->channels;
		unsigned int index = 0;

		do {
			ch->client = client;
			ch->index = index++;
			ch->size = 0;
			ch->shared = 0;
			ch->sharedBase = NULL;
			ch++;
			*(char *)((char *)client->sharedBase + (index - 1) * 16520) = 0;
		} while (index < 2);
	}
	*outClient = client;
	return 0;

fail:
	dtcpIpSwFreeLaunch(client->launch, (int *)&client->launch);
	if (client->pipeCtoS != -1) {
		dtcpIpSwDeletePipe(client->pipeCtoS);
		client->pipeCtoS = -1;
	}
	if (client->pipeStoC != -1) {
		dtcpIpSwClosePipe(client->pipeStoC);
		client->pipeStoC = -1;
	}
	if (client->mutex != -1) {
		dtcpIpSwDeleteMutex(client->mutex, 1);
		client->mutex = -1;
	}
	dtcpIpMspaceFree(client);
	return ret;
}

/* free a channel: its shared memory and its pipes */
int dtcpIpSwFiniChannel(struct SwIpcChannel *channel)
{
	struct SwIpcChannel *ch = channel;
	unsigned int i;

	if (ch->shared != NULL) {
		dtcpIpSwCloseShared(ch->shared);
		ch->sharedBase = NULL;
		ch->shared = NULL;
		ch->size = 0;
	}
	for (i = 0; i < 4; ++i) {
		if (ch->pipeStoC[i] != -1) {
			dtcpIpSwClosePipe(ch->pipeStoC[i]);
			ch->pipeStoC[i] = -1;
		}
		if (ch->pipeCtoS[i] != -1) {
			dtcpIpSwDeletePipe(ch->pipeCtoS[i]);
			ch->pipeCtoS[i] = -1;
		}
	}
	if (ch->pipeSingle != -1) {
		dtcpIpSwDeletePipe(ch->pipeSingle);
		ch->pipeSingle = -1;
	}
	return 0;
}

/* free a client: its channels, its mutex, its pipes and shared memory */
int dtcpIpSwFiniClient(struct SwIpcClient *client)
{
	struct SwIpcWireMessage message;
	unsigned int i;

	for (i = 0; i < 2; ++i) {
		if (*(char *)((char *)client->sharedBase + i * 16520) == 1)
			dtcpIpSwFiniChannel(&client->channels[i]);
	}
	dtcpIpSwCloseShared(client->shared);
	dtcpIpSwDeleteMutex(client->mutex, 1);
	message.kind = 3;
	message.result = 0;
	message.value0 = 0;
	message.value1 = 0;
	message.value2 = 0;
	message.value3 = 0;
	dtcpIpSwSend(client->pipeCtoS, &message);
	dtcpIpSwClosePipe(client->pipeStoC);
	while (dtcpIpSwOpenPipe("SW_IPC_Sys_Pipe_StoC", &client->pipeStoC) == 0) {
		dtcpIpSwClosePipe(client->pipeStoC);
		client->pipeStoC = -1;
		sceKernelDelayThread(50000);
	}
	dtcpIpSwFreeLaunch(client->launch, (int *)&client->launch);
	dtcpIpSwDeletePipe(client->pipeCtoS);
	dtcpIpMspaceFree(client);
	return 0;
}

/*
 * carry a message over the channel: build the 8-dword block, copy the payload into the
 * channel's shared slice, send it (or signal the stream), then read the reply's fields
 * back into the caller's message. A channel byte of 255 marks a control request: no
 * reply block is read.
 */
int dtcpIpSwRequest(struct SwIpcChannel *channel, struct SceDtcpIpIpcMessage *message, SceUID pipe)
{
	struct SwIpcClient *client = channel->client;
	unsigned int channelByte = message->channel;
	unsigned short size = message->size;
	unsigned int index = 0;
	unsigned int block[8];
	char *blockPointer;
	SceUID replyPipe;
	int ret;
	int replyIndex;
	char *reply;
	unsigned short outSize;
	unsigned int outSizeValue;

	if (channelByte != 255 && channelByte >= 4)
		return 12003;
	if (channelByte != 255)
		index = channelByte;
	if (channel->size != 0)
		blockPointer = (char *)client->sharedBase + 16520 * channel->index + 4128 * index + 4104;
	else
		blockPointer = (char *)block;
	*(unsigned int *)(blockPointer + 8) = (channelByte << 24) | size;
	*(unsigned int *)(blockPointer + 12) = message->command;
	*(unsigned int *)(blockPointer + 16) = message->value1;
	*(unsigned int *)(blockPointer + 20) = message->value0;
	*(unsigned int *)(blockPointer + 24) = message->value3;
	*(unsigned int *)(blockPointer + 28) = message->value2;
	if (size != 0 && size <= 2048 && channelByte != 255)
		dtcpIpSwCopy((char *)client->sharedBase + 16520 * channel->index +
				32 * (channelByte + (channelByte << 7)) + 8, message->payload, size);
	if (channel->size != 0)
		ret = dtcpIpSwSignal(pipe, index);
	else
		ret = dtcpIpSwSend(pipe, blockPointer);
	if (ret != 0)
		return ret;

	if (channelByte == 255) {
		message->result = 0;
		message->outSize = 0;
		message->outPayload = NULL;
		message->out0 = 0;
		message->out1 = 0;
		message->out2 = 0;
		message->out3 = 0;
	} else {
		replyPipe = channel->pipeStoC[index];
		if (channel->size != 0) {
			ret = dtcpIpSwWait(replyPipe, &replyIndex, 3000000);
			if (ret != 0)
				return ret;
			reply = (char *)client->sharedBase + 16520 * channel->index +
				4128 * replyIndex + 4104;
		} else {
			ret = dtcpIpSwReceive(replyPipe, block, 0);
			if (ret != 0)
				return ret;
			reply = (char *)block;
		}
		message->result = *(unsigned int *)(reply + 12);
		outSize = *(unsigned short *)(reply + 10);
		message->outSize = outSize;
		message->out0 = *(unsigned int *)(reply + 20);
		message->out1 = *(unsigned int *)(reply + 16);
		message->out2 = *(unsigned int *)(reply + 28);
		message->out3 = *(unsigned int *)(reply + 24);
		outSizeValue = outSize;
		if (outSizeValue != 0 && outSizeValue <= 0x800 && channelByte != 255)
			message->outPayload = (char *)client->sharedBase + 16520 * channel->index +
					      4128 * channelByte + 2056;
	}
	return 0;
}

/* carry a request over the channel: the caller's message picks the stream's pipe */
int dtcpIpIpcRequest(void *stream, struct SceDtcpIpIpcMessage *message)
{
	struct SwIpcChannel *channel = stream;
	unsigned int channelByte = message->channel;
	unsigned int index = 0;
	SceUID pipe;

	if (channelByte <= 4)
		index = channelByte;
	if (channel->size != 0)
		pipe = channel->pipeCtoS[index];
	else
		pipe = channel->pipeSingle;
	return dtcpIpSwRequest(channel, message, pipe);
}

/*
 * free a stream channel: close the stream's request (one per stream, or the single
 * pipe), tell the middleware the channel closes (kind 6, the reply's kind 7), mark the
 * channel free and free it.
 */
int dtcpIpIpcFiniStream(void *stream, int status)
{
	struct SwIpcChannel *channel = stream;
	struct SwIpcClient *client = channel->client;
	struct SceDtcpIpIpcMessage message;
	struct SwIpcWireMessage control;
	unsigned int reply[8];
	int ret = 0;
	unsigned int i;

	if (channel->shared != NULL) {
		dtcpIpSwCloseShared(channel->shared);
		channel->reserved0 = 0;
		channel->sharedBase = NULL;
		channel->shared = NULL;
	}
	message.command = 0;
	message.channel = 255;
	message.size = 0;
	message.value0 = 0;
	if (channel->size != 0) {
		for (i = 0; i < 4; ++i) {
			ret = dtcpIpSwRequest(channel, &message, channel->pipeCtoS[i]);
			if (ret != 0 && ret != 12011 && ret != 12017)
				return ret;
		}
	} else {
		ret = dtcpIpSwRequest(channel, &message, channel->pipeSingle);
		if (ret != 0 && ret != 12011 && ret != 12017)
			return ret;
	}
	control.kind = 6;
	control.result = 0;
	control.value0 = channel->index;
	control.value1 = 0;
	control.value2 = 0;
	control.value3 = 0;
	ret = dtcpIpSwSend(client->pipeCtoS, &control);
	if (ret == 0 || ret == 12011 || ret == 12017) {
		ret = dtcpIpSwReceive(client->pipeStoC, reply, 0);
		if (ret == 0 || ret == 12011 || ret == 12017) {
			if (ret != 0 || *(int *)(reply + 8) == 7) {
				dtcpIpSwFiniChannel(channel);
				dtcpIpSwLock(client->mutex);
				*(unsigned char *)((char *)client->sharedBase + 16520 * channel->index) = 0;
				dtcpIpSwUnlock(client->mutex);
				if (ret != 0)
					return 0;
			} else {
				ret = 12004;
			}
		}
	}
	return ret;
}

/*
 * create a stream channel on a client: take a free channel slot, create its request
 * pipes (four per stream, or one), tell the middleware the channel opens (kind 4; the
 * reply's kind 5 and value 0), attach the stream's shared memory, and open the four
 * StoC pipes. The caller's a3 is reserved (zero) and a6 travels as the open message's
 * value3 (the stream's affinity).
 */
int dtcpIpIpcInitStream(void **outChannel, void *session, int reserved, int bufferSize,
			int streams, int affinity)
{
	struct SwIpcClient *client = session;
	struct SwIpcChannel *channel;
	struct SwIpcWireMessage message;
	struct SwIpcShared *shared;
	char name[40];
	void *base;
	int ret = 0;
	int index;
	int i;
	unsigned int pipeIndex;
	int freeSlot;

	if (reserved != 0)
		return 12003;

	dtcpIpSwLock(client->mutex);
	freeSlot = 0;
	index = 0;
	for (i = 0; i < 2; ++i) {
		if (*(unsigned char *)((char *)client->sharedBase + i * 16520) == 0) {
			*(unsigned char *)((char *)client->sharedBase + i * 16520) = 1;
			freeSlot = i;
			index = 1;
			break;
		}
	}
	dtcpIpSwUnlock(client->mutex);
	if (index == 0)
		return 12002;

	channel = &client->channels[freeSlot];
	channel->size = streams;
	channel->pipeSingle = -1;
	for (i = 0; i < 4; ++i) {
		channel->pipeStoC[i] = -1;
		channel->pipeCtoS[i] = -1;
	}
	if (streams != 0) {
		for (pipeIndex = 0; pipeIndex < 4; ++pipeIndex) {
			dtcpIpSnprintf(name, "%s_%02d_%02d", "SW_IPC_Strm_Pipe_CtoS",
				       channel->index, pipeIndex);
			if (dtcpIpSwCreatePipe(name, &channel->pipeCtoS[pipeIndex]) != 0)
				goto pipesFailed;
		}
	} else {
		dtcpIpSnprintf(name, "%s_%02d", "SW_IPC_Strm_Pipe_CtoS", channel->index);
		if (dtcpIpSwCreatePipe(name, &channel->pipeSingle) != 0)
			goto pipesFailed;
	}
	goto openChannel;

pipesFailed:
	if (channel->pipeSingle != -1) {
		dtcpIpSwDeletePipe(channel->pipeSingle);
		channel->pipeSingle = -1;
	}
	for (i = 0; i < 4; ++i) {
		if (channel->pipeCtoS[i] != -1) {
			dtcpIpSwClosePipe(channel->pipeCtoS[i]);
			channel->pipeCtoS[i] = -1;
		}
	}
	ret = 12009;
	goto fail;

openChannel:
	message.kind = 4;
	message.result = 0;
	message.value0 = streams;
	message.value1 = freeSlot;
	message.value2 = bufferSize;
	message.value3 = affinity;
	ret = dtcpIpSwSend(client->pipeCtoS, &message);
	if (ret != 0 || (ret = dtcpIpSwReceive(client->pipeStoC, &message, 1000000)) != 0)
		goto failChannel;
	if (message.kind != 5 || message.result != 0) {
		ret = 12004;
		goto failChannel;
	}
	if (bufferSize != 0) {
		dtcpIpSnprintf(name, "%s_%02d", "SW_IPC_Strm_ShMem", channel->index);
		if (dtcpIpSwOpenShared(&shared, name, bufferSize, &base) != 0) {
			dtcpIpIpcFiniStream(channel, 0);
			return 12004;
		}
		channel->shared = shared;
		channel->sharedBase = base;
	} else {
		channel->sharedBase = NULL;
	}
	channel->reserved0 = bufferSize;
	for (i = 0; i < 4; ++i) {
		dtcpIpSnprintf(name, "%s_%02d_%02d", "SW_IPC_Strm_Pipe_StoC", freeSlot, i);
		if (dtcpIpSwOpenPipe(name, &channel->pipeStoC[i]) != 0) {
			dtcpIpIpcFiniStream(channel, 0);
			return 12004;
		}
	}
	*outChannel = channel;
	return 0;

failChannel:
	dtcpIpSwFiniChannel(channel);
fail:
	dtcpIpSwLock(client->mutex);
	*(unsigned char *)((char *)client->sharedBase + freeSlot * 16520) = 0;
	dtcpIpSwUnlock(client->mutex);
	return ret;
}
