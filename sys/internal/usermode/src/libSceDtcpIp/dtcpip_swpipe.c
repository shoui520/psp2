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
int dtcpIpIpcInitShared(void *stream, int *outShared)
{
	int *shared = stream;

	outShared[0] = shared[1];
	outShared[1] = shared[2];
	outShared[2] = shared[3];
	return 0;
}

/* take the protocol's mutex: the created object, or one the middleware owns by name */
int dtcpIpSwCreateMutex(SceUID *outMutex, const char *name, int create)
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

int dtcpIpSwDeleteMutex(SceUID mutex, int destroy)
{
	if (destroy != 0)
		sceKernelDeleteMutex(mutex);
	else
		sceKernelCloseMutex(mutex);
	return 0;
}

int dtcpIpSwLock(SceUID mutex)
{
	if (sceKernelLockMutex(mutex, 1, NULL) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

int dtcpIpSwUnlock(SceUID mutex)
{
	if (sceKernelUnlockMutex(mutex, 1) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

/* create a request pipe the middleware opens by name */
int dtcpIpSwCreatePipe(const char *name, SceUID *outPipe)
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

int dtcpIpSwDeletePipe(SceUID pipe)
{
	sceKernelDeleteMsgPipe(pipe);
	return 0;
}

/* open a request pipe the middleware created */
int dtcpIpSwOpenPipe(const char *name, SceUID *outPipe)
{
	SceUID pipe;

	pipe = sceKernelOpenMsgPipe(name);
	if (pipe < 0)
		return SW_IPC_ERROR_PIPE;
	*outPipe = pipe;
	return 0;
}

int dtcpIpSwClosePipe(SceUID pipe)
{
	sceKernelCloseMsgPipe(pipe);
	return 0;
}

/* send a 0x20-byte message block over a pipe */
int dtcpIpSwSend(SceUID pipe, const void *message)
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
int dtcpIpSwReceive(SceUID pipe, void *message, int timeout)
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
int dtcpIpSwSignal(SceUID event, int value)
{
	/* the value travels as the event's user data (its high word) */
	if (sceKernelSetEvent(event, SW_IPC_EVENT_SIGNAL, (SceUInt64)value << 32) != 0)
		return SW_IPC_ERROR_PIPE;
	return 0;
}

/* wait for the peer's event flag */
int dtcpIpSwWait(SceUID event, int *outValue, int timeout)
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
int dtcpIpSwFreeLaunch(void *launch, int *slot)
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
int dtcpIpSwCloseShared(struct SwIpcShared *shared)
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
