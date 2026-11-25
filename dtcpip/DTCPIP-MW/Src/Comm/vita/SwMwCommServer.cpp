/*
 * swagner's communication server (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp, from the
 * error strings' path and lines): the middleware application's side of the SW IPC the
 * library connects to. It owns the control mutex, the two request pipes, the shared
 * memory block (two 16520-byte channel slices) and two channels, and it runs the loop that
 * reads the library's requests and hands them to the manager.
 */

#include <stddef.h>
#include <scetypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/memblock.h>
#include <appmgr.h>
#include <libnetctl.h>
#include <net.h>
#include <sysmodule/sysmodule.h>
#include <libkernel/libkernel.h>

#define SWMW_COMM_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Comm\\vita\\SwMwCommServer.cpp"

/* the middleware's trace (SwMwTrace.c's [SWMW] print, flushes every message) */
extern "C" int swMwTrace(const char *format, ...);

/* the server's shared memory: two channel slices of 16520 bytes, four 4128-byte stream
 * slices in each, the request block at 4104 and the reply payload at 2056 */
#define SWMW_COMM_SHARED_SIZE		33040
#define SWMW_COMM_CHANNEL_STRIDE		16520
#define SWMW_COMM_CHANNEL_COUNT		2
#define SWMW_COMM_STREAM_STRIDE		4128
#define SWMW_COMM_PAYLOAD_OFFSET		8
#define SWMW_COMM_REPLY_OFFSET		2056
#define SWMW_COMM_BLOCK_OFFSET		4104

/* the 8-dword request block: the library writes it as three 64-bit pairs */
struct SwMwIpcBlock {
	unsigned int header[2];			/* 0x00: the protocol leaves them unused */
	unsigned long long channelCommand;	/* 0x08: channel << 24 | size, command << 32 */
	unsigned long long value1Value0;	/* 0x10: value1, value0 << 32 */
	unsigned long long value3Value2;	/* 0x18: value3, value2 << 32 */
};

/* a control message as it travels over the server's pipes (0x20 bytes) */
struct SwMwIpcWireMessage {
	int header[2];		/* 0x00: the message pipe's own */
	int kind;		/* 0x08: 1 connect, 2 connected, 3 disconnect, 4 open, 5 opened, ... */
	int result;		/* 0x0C */
	int value0;		/* 0x10 */
	int value1;		/* 0x14 */
	int value2;		/* 0x18 */
	int value3;		/* 0x1C */
};

/* one request as the server sees it (0x38 bytes, the library's message layout) */
struct SwMwIpcMessage {
	int command;		/* 0x00 */
	unsigned char channel;	/* 0x04 */
	unsigned char padAfterChannel;
	unsigned short size;	/* 0x06 */
	void *payload;		/* 0x08 */
	int value0;		/* 0x0C */
	int value1;		/* 0x10 */
	int value2;		/* 0x14 */
	int value3;		/* 0x18 */
	int result;		/* 0x1C */
	unsigned short outSize;	/* 0x20 */
	unsigned short padAfterOutSize;
	void *outPayload;	/* 0x24 */
	int out0;		/* 0x28 */
	int out1;		/* 0x2C */
	int out2;		/* 0x30 */
	int out3;		/* 0x34 */
};

/* the middleware's identity, from the launch parameter (main.cpp) */
extern char g_swagnerIdentity[16];

/* a request's payload: the SW core's 256-byte block after the request's header */
struct SwMwCommRequest {
	int reserved;			/* 0x00 */
	int value[3];			/* 0x04: the request's header words */
	unsigned char payload[256];	/* 0x10 */
};

/* the network setup the server makes before its pipes: SceNet's 64 KiB work memory */
#define SWMW_NET_MEMORY_SIZE	0x10000
#define SWMW_NET_ERROR_MEMORY	22

extern "C" int SwMwNetInit(void)
{
	SceNetInitParam param;
	int result;

	if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0)
		return SWMW_NET_ERROR_MEMORY;
	param.memory = malloc(SWMW_NET_MEMORY_SIZE);
	param.size = SWMW_NET_MEMORY_SIZE;
	param.flags = 0;
	if (param.memory == NULL) {
		result = SWMW_NET_ERROR_MEMORY;
	} else {
		result = sceNetInit(&param);
		if (result >= 0) {
			result = sceNetCtlInit();
			if (result >= 0)
				return 0;
		}
	}
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	return result;
}

extern "C" int SwMwNetTerm(void)
{
	sceNetCtlTerm();
	sceNetTerm();
	return sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

/*
 * hand the SW core a request's header and payload: its three header words, then the
 * 256-byte payload, in the core's own block (0x81000c38; called from the core's dispatch).
 */
extern "C" int SwMwCommServerGetRequest(const struct SwMwCommRequest *request, int *out)
{
	out[0] = request->value[0];
	out[1] = request->value[1];
	out[2] = request->value[2];
	memcpy(out + 3, request->payload, sizeof(request->payload));
	return 0;
}

/* the SW core's reply: its 12-byte header, then the 64-byte value a request carries */
struct SwMwCoreReply {
	int header[3];			/* 0x00 */
	unsigned char value[64];	/* 0x0C */
};

/* store the 64-byte value a SW core reply carries in a request's payload (0x81000c78) */
extern "C" int SwMwCommServerSetRequestValue(struct SwMwCommRequest *request, const void *value)
{
	const struct SwMwCoreReply *reply = (const struct SwMwCoreReply *)value;

	memcpy(request->payload, reply->value, sizeof(reply->value));
	return 0;
}

/*
 * poll the application manager's events (0x81000ca0): the run loop calls it while a
 * receive times out. A quit request for this application's own identity ends the loop.
 */
extern "C" int SwMwCommServerCheckAppEvent(void)
{
	struct SceAppMgrAppEvent event;
	int count = 0;
	int quit = 0;
	int i;

	sceAppMgrReceiveEventNum(&count);
	for (i = 0; i < count; ++i) {
		sceAppMgrReceiveEvent(&event);
		swMwTrace("sw mw:Event[%d]: EventID = 0x%08x, AppID = 0x%08x\n", i, event.eventId,
			  event.appId);
		if (event.eventId == 0x20000001) {
			if (g_swagnerIdentity[0] != 0) {
				swMwTrace("sw mw:kill name %s\n", g_swagnerIdentity);
				sceAppMgrDestroyAppByName(g_swagnerIdentity);
			}
			quit = 1;
		}
	}
	return quit;
}

/*
 * The server's IPC primitives: the kernel objects the library reaches by name (the control
 * mutex "SW_IPC_Sys_Mutex", the request pipes "SW_IPC_Sys_Pipe_CtoS"/"StoC") and the
 * message-pipe event protocol the two sides share.
 */

/* the status the server answers with when a kernel object cannot be had */
#define SWMW_IPC_ERROR			12011
/* the status it answers with when a wait timed out */
#define SWMW_IPC_ERROR_TIMEOUT		12017

/* the message pipe's event bits */
#define SWMW_IPC_EVENT_DATA		0x002
#define SWMW_IPC_EVENT_CLOSED		0x008
#define SWMW_IPC_EVENT_CONNECT		0x010
#define SWMW_IPC_EVENT_ERROR		0x200

/* the event bits a receive waits for: data, closed, connect, error */
#define SWMW_IPC_EVENT_MASK		538
/* the bits a stream request's wait takes: the same, plus the stream's signal bit */
#define SWMW_IPC_STREAM_WAIT_MASK	792
/* the bits the shared-memory stream wait takes: the library's signal, error, connect, close */
#define SWMW_IPC_SHARED_WAIT_MASK	0x1000218

/* the message pipe the server creates: its stack, its buffer and its options */
#define SWMW_IPC_PIPE_STACK		64
#define SWMW_IPC_PIPE_ATTRIBUTE		387
#define SWMW_IPC_PIPE_BUFFER		0x1000
#define SWMW_IPC_MESSAGE_SIZE		0x20

/* take the control mutex: the created one, or the middleware's by name */
extern "C" int SwMwIpcOpenMutex(SceUID *mutex, const char *name, int create)
{
	SceUID handle;

	if (create != 0)
		handle = sceKernelCreateMutex(name, 0x2080, 0, NULL);
	else
		handle = sceKernelOpenMutex(name);
	if (handle < 0)
		return SWMW_IPC_ERROR;
	*mutex = handle;
	return 0;
}

/* release the control mutex */
extern "C" int SwMwIpcCloseMutex(SceUID mutex, int destroy)
{
	if (destroy != 0)
		sceKernelDeleteMutex(mutex);
	else
		sceKernelCloseMutex(mutex);
	return 0;
}

/* create the pipe the library sends its requests on */
extern "C" int SwMwIpcCreatePipe(const char *name, SceUID *pipe)
{
	SceKernelMsgPipeOptParam option = {
		.size = 20,
		.attr = 1,
		.reserved = { 133, 0 },
		.openLimit = 0,
	};
	SceUID handle;

	handle = sceKernelCreateMsgPipe(name, SWMW_IPC_PIPE_STACK, SWMW_IPC_PIPE_ATTRIBUTE,
					SWMW_IPC_PIPE_BUFFER, &option);
	if (handle < 0)
		return SWMW_IPC_ERROR;
	*pipe = handle;
	return 0;
}

/* delete a pipe the server created */
extern "C" int SwMwIpcDeletePipe(SceUID pipe)
{
	sceKernelDeleteMsgPipe(pipe);
	return 0;
}

/* open the pipe the library created for the server's replies */
extern "C" int SwMwIpcOpenPipe(const char *name, SceUID *pipe)
{
	SceUID handle;

	handle = sceKernelOpenMsgPipe(name);
	if (handle < 0)
		return SWMW_IPC_ERROR;
	*pipe = handle;
	return 0;
}

/* close a pipe the library created */
extern "C" int SwMwIpcClosePipe(SceUID pipe)
{
	sceKernelCloseMsgPipe(pipe);
	return 0;
}

/*
 * wait on a pipe's event: the caller's timeout pointer is passed only when it is nonzero
 * (a zero timeout waits forever), and a timed-out wait is the protocol's own status.
 */
extern "C" int SwMwIpcWaitEvent(SceUID event, int timeout)
{
	SceUInt32 timeoutValue = timeout;
	SceUInt32 flags = 0;
	SceUInt64 eventValue = 0;
	int result;

	result = sceKernelWaitEvent(event, SWMW_IPC_STREAM_WAIT_MASK, &flags, &eventValue,
				    timeout != 0 ? &timeoutValue : NULL);
	if (result != 0) {
		if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return SWMW_IPC_ERROR_TIMEOUT;
		return SWMW_IPC_ERROR;
	}
	if ((flags & 0x100) != 0)
		return 0;
	return SWMW_IPC_ERROR;
}

/*
 * wait for a stream's shared-memory event: the signal bit carries the number of bytes the
 * library moved in the user data's high word; a timed-out wait is the protocol's status.
 */
extern "C" int SwMwIpcWaitSharedEvent(SceUID event, int *outSize, int timeout)
{
	SceUInt32 timeoutValue = timeout;
	SceUInt32 flags = 0;
	SceUInt64 eventValue = 0;
	int result;

	result = sceKernelWaitEvent(event, SWMW_IPC_SHARED_WAIT_MASK, &flags, &eventValue,
				    timeout != 0 ? &timeoutValue : NULL);
	if (result != 0) {
		if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return SWMW_IPC_ERROR_TIMEOUT;
		return SWMW_IPC_ERROR;
	}
	if ((flags & SWMW_IPC_EVENT_CLOSED) != 0)
		return SWMW_IPC_ERROR;
	if ((flags & SWMW_IPC_EVENT_CONNECT) != 0) {
		if ((int)eventValue == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return SWMW_IPC_ERROR_TIMEOUT;
		return SWMW_IPC_ERROR;
	}
	if ((flags & SWMW_IPC_EVENT_ERROR) != 0)
		return SWMW_IPC_ERROR;
	if ((flags & 0x1000000) != 0) {
		*outSize = (int)(eventValue >> 32);
		return 0;
	}
	return SWMW_IPC_ERROR;
}

/* send one 0x20-byte message block over a pipe */
extern "C" int SwMwIpcSend(SceUID pipe, const void *message)
{
	unsigned int sent = 0;

	if (sceKernelSendMsgPipe(pipe, (void *)message, SWMW_IPC_MESSAGE_SIZE, 1, &sent, NULL) != 0)
		return SWMW_IPC_ERROR;
	if (sent == SWMW_IPC_MESSAGE_SIZE)
		return 0;
	return SWMW_IPC_ERROR;
}

/*
 * receive one 0x20-byte message block: the pipe's event says what arrived — data (the
 * event's user data carries the moved size in its high word), the peer's close, its
 * connect or an error — and anything else is the protocol's fatal error.
 */
extern "C" int SwMwIpcReceive(SceUID pipe, void *message, int timeout)
{
	SceUInt32 timeoutValue = timeout;
	SceUInt32 received = 0;
	SceUInt32 flags = 0;
	SceUInt64 eventValue = 0;
	unsigned int size;

	if (sceKernelReceiveMsgPipe(pipe, message, SWMW_IPC_MESSAGE_SIZE, 17, &received,
				    timeout != 0 ? &timeoutValue : NULL) != 0)
		return SWMW_IPC_ERROR;
	if (sceKernelWaitEvent(pipe, SWMW_IPC_EVENT_MASK, &flags, &eventValue, NULL) != 0)
		return SWMW_IPC_ERROR;
	if ((flags & SWMW_IPC_EVENT_CLOSED) != 0)
		return SWMW_IPC_ERROR;
	if ((flags & SWMW_IPC_EVENT_CONNECT) != 0)
		return (int)eventValue == SCE_KERNEL_ERROR_WAIT_TIMEOUT ? SWMW_IPC_ERROR_TIMEOUT
								       : SWMW_IPC_ERROR;
	if ((flags & SWMW_IPC_EVENT_ERROR) != 0)
		return SWMW_IPC_ERROR;
	if ((flags & SWMW_IPC_EVENT_DATA) != 0) {
		size = (unsigned int)(eventValue >> 32);
		return size == SWMW_IPC_MESSAGE_SIZE ? 0 : SWMW_IPC_ERROR;
	}
	printf("SwMwIPCReceiveEventMsgPipe : fatal error\n");
	return SWMW_IPC_ERROR;
}

/*
 * The shared memory block the server and the library exchange their stream buffers in: a
 * named block of two 16520-byte channel slices. Its control object carries the block's
 * name, its rounded size, the memblock and its base.
 */
struct SwMwSharedMemory {
	char name[32];			/* 0x00: the memblock's name */
	SceUInt32 size;			/* 0x20: its size, rounded to a page */
	SceUID memBlock;		/* 0x24: the memblock */
	void *base;			/* 0x28: its base */
};

/* the memory types an allocation request can name: a 256-bit set */
#define SWMW_MEMORY_TYPE_BITS		256

/* the status a memory type out of range answers with */
#define SWMW_MEMORY_TYPE_ERROR		(-2147352571)

/* clear a memory-type set */
extern "C" int SwMwMemoryTypeClear(SceUInt32 *typeSet)
{
	typeSet[0] = 0;
	typeSet[1] = 0;
	typeSet[2] = 0;
	typeSet[3] = 0;
	typeSet[4] = 0;
	typeSet[5] = 0;
	typeSet[6] = 0;
	typeSet[7] = 0;
	return 0;
}

/* add one memory type to a set */
extern "C" int SwMwMemoryTypeAdd(SceUInt32 *typeSet, int type)
{
	if (type < 0 || type > SWMW_MEMORY_TYPE_BITS)
		return SWMW_MEMORY_TYPE_ERROR;
	typeSet[type / 32] |= 1u << (type % 32);
	return 0;
}

/* free the shared memory: close it (the library created it) or free it, then the object */
extern "C" int SwMwSharedMemoryFree(struct SwMwSharedMemory *shared, int close)
{
	if (close != 0)
		sceKernelCloseMemBlock(shared->memBlock);
	else
		sceKernelFreeMemBlock(shared->memBlock);
	delete shared;
	return 0;
}

/* the allocation's attribute word as this SDK writes it (0x4020) and its open flags (48) */
#define SWMW_MEMBLOCK_ATTR_CONTIGUOUS	0x4000
#define SWMW_MEMBLOCK_ATTR_KEEP		0x20
#define SWMW_MEMBLOCK_OPEN_FLAGS	48

/* the capability this middleware's block asks for */
#define SWMW_MEMBLOCK_CAPABILITY	133

/* the status a shared memory block that cannot be had answers with */
#define SWMW_SHARED_ERROR		12012

/*
 * Get the shared memory block: the library creates it and the server opens it by name
 * (open != 0), or the server creates it (open == 0). The block's size is rounded to a page
 * and its base handed back. A failure releases whatever was taken and answers 12012.
 */
extern "C" int SwMwCommServerCreateShared(struct SwMwSharedMemory **out, const char *name,
					  int size, void **outBase, int open)
{
	struct SwMwSharedMemory *shared;
	MemBlockUserOpt option;
	union {
		MemBlockUserOpt option;
		SceKernelAllocMemBlockOpt sdkOption;
	} blockOption;

	shared = new struct SwMwSharedMemory;
	if (shared == NULL)
		return SWMW_SHARED_ERROR;
	memset(shared, 0, 32);
	shared->size = (size + 4095) & 0xFFFFF000;
	shared->memBlock = 0;
	shared->base = NULL;
	if (open != 0) {
		strncpy(shared->name, name, 32);
		shared->memBlock = sceKernelOpenMemBlock(name, SWMW_MEMBLOCK_OPEN_FLAGS);
		if (shared->memBlock < 0) {
			delete shared;
			return SWMW_SHARED_ERROR;
		}
	} else {
		memset(&option, 0, sizeof(option));
		option.size = sizeof(option);
		option.attr = SWMW_MEMBLOCK_ATTR_CONTIGUOUS;
		option.attr |= SWMW_MEMBLOCK_ATTR_KEEP;
		option.openFlags = SWMW_MEMBLOCK_OPEN_FLAGS;
		SwMwMemoryTypeClear(option.capabilities);
		SwMwMemoryTypeAdd(option.capabilities, SWMW_MEMBLOCK_CAPABILITY);
		strncpy(shared->name, name, 32);
		/* the SDK 3.65 option is the 0x40-byte MemBlockUserOpt above; the tree's SDK
		 * header still declares the older 0x14-byte form of the same argument */
		memcpy(&blockOption.option, &option, sizeof(option));
		shared->memBlock = sceKernelAllocMemBlock(
			name, 0xC20D060, shared->size, &blockOption.sdkOption);
		if (shared->memBlock < 0) {
			delete shared;
			return SWMW_SHARED_ERROR;
		}
	}
	if (sceKernelGetMemBlockBase(shared->memBlock, &shared->base) >= 0) {
		*outBase = shared->base;
		*out = shared;
		return 0;
	}
	if (open != 0)
		sceKernelCloseMemBlock(shared->memBlock);
	else
		sceKernelFreeMemBlock(shared->memBlock);
	delete shared;
	return SWMW_SHARED_ERROR;
}

/*
 * A stream runs in its own thread: the entry, the thread and the argument the entry gets.
 * The channel keeps one per stream slot (and one of its own for the single-stream case).
 */
struct SwMwCommThread {
	int (*entry)(void *argument);	/* 0x00: the thread's entry */
	SceUID id;			/* 0x04: its id, once started */
	void *argument;			/* 0x08: what the entry receives */
};

/* one stream of a channel: the pipes and the thread that carries it */
struct SwMwCommStreamSlot {
	SceUID pipeStoC;		/* 0x00: the request pipe the server created */
	SceUID pipeCtoS;		/* 0x04: the reply pipe the server opened */
	struct SwMwCommThread thread;	/* 0x08 */
};

struct SwMwCommServer;

/* one of the server's two channels: 184 bytes, its pipes, its shared block and the
 * stream groups the library opens on it */
struct SwMwCommChannel {
	struct SwMwCommServer *server;	/* 0x00 */
	int index;			/* 0x04 */
	void *sharedBase;		/* 0x08: the shared memory's base */
	int bufferSize;			/* 0x0C: its size */
	unsigned int reserved[16];	/* 0x10: the original never reads or writes them */
	struct SwMwSharedMemory *shared;	/* 0x50: the shared memory control object */
	SceUID singlePipe;		/* 0x54: the single-stream CtoS pipe */
	struct SwMwCommThread singleThread;	/* 0x58 */
	int streamCount;		/* 0x64: 0: one stream, else four stream slots */
	struct SwMwCommStreamSlot slots[4];	/* 0x68 */
};

/* the server: its control objects, its shared memory, its two channels and the manager
 * dispatch its stream threads call */
struct SwMwCommServer {
	SceUID mutex;			/* 0x000 */
	SceUID pipeCtoS;		/* 0x004 */
	SceUID pipeStoC;		/* 0x008 */
	void *shared;			/* 0x00C: the shared memory control object */
	void *base;			/* 0x010: its base */
	struct SwMwCommChannel channels[2];	/* 0x014 */
	void (*dispatch)(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
			 void *context);	/* 0x184 */
	void *dispatchContext;		/* 0x188 */
	int timeout;			/* 0x18C: a request receive's timeout, in milliseconds */
};

/* what a stream thread's argument points to: its channel and its request pipe */
struct SwMwThreadArgument {
	struct SwMwCommChannel *channel;	/* 0x00 */
	SceUID pipe;			/* 0x04 */
};

/* stop a stream's thread: wait for it to end, then delete it */
extern "C" int SwMwCommStreamStop(struct SwMwCommThread *thread)
{
	int result;

	result = sceKernelWaitThreadEnd(thread->id, NULL, NULL);
	sceKernelDeleteThread(thread->id);
	return result;
}

/*
 * The receive path the stream threads share: wait on the channel's shared event (its
 * stream slices) or on the message pipe (the single-stream case), then decode the
 * 8-dword block into the request the dispatch reads.
 */
extern "C" int SwMwCommServerReceive(struct SwMwCommChannel *channel, SceUID pipe,
				     struct SwMwIpcMessage *request, int timeout)
{
	struct SwMwIpcBlock block;
	struct SwMwIpcBlock *received;
	int streamIndex = 0;
	int result;

	if (channel->streamCount != 0)
		result = SwMwIpcWaitSharedEvent(pipe, &streamIndex, 1000 * timeout);
	else
		result = SwMwIpcReceive(pipe, &block, 1000 * timeout);
	if (result != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 384);
		return result;
	}
	if (channel->streamCount != 0)
		received = (struct SwMwIpcBlock *)((char *)channel->server->base +
			 SWMW_COMM_CHANNEL_STRIDE * channel->index +
			 SWMW_COMM_STREAM_STRIDE * streamIndex + SWMW_COMM_BLOCK_OFFSET);
	else
		received = &block;
	request->command = (int)(received->channelCommand >> 32);
	request->channel = (unsigned char)(received->channelCommand >> 24);
	request->size = (unsigned short)received->channelCommand;
	request->value0 = (int)(received->value1Value0 >> 32);
	request->value1 = (int)received->value1Value0;
	request->value2 = (int)(received->value3Value2 >> 32);
	request->value3 = (int)received->value3Value2;
	if (request->size != 0)
		request->payload = (char *)channel->server->base +
				   SWMW_COMM_CHANNEL_STRIDE * channel->index +
				   SWMW_COMM_STREAM_STRIDE * request->channel +
				   SWMW_COMM_PAYLOAD_OFFSET;
	else
		request->payload = NULL;
	if (request->channel == 255)
		request->outPayload = NULL;
	else
		request->outPayload = (char *)channel->server->base +
				      SWMW_COMM_CHANNEL_STRIDE * channel->index +
				      SWMW_COMM_STREAM_STRIDE * request->channel +
				      SWMW_COMM_REPLY_OFFSET;
	return 0;
}

/*
 * Receive a request for a stream thread: a timeout is the protocol's stop word (the
 * request's command becomes 1), anything else an error; the reply fields are cleared and
 * the caller learns whether the loop goes on.
 */
extern "C" int SwMwCommServerReceiveRequest(struct SwMwCommChannel *channel, SceUID pipe,
					    struct SwMwIpcMessage *request, int *outDone)
{
	int result;

	result = SwMwCommServerReceive(channel, pipe, request, channel->server->timeout);
	if (result != 0) {
		if (result != SWMW_IPC_ERROR_TIMEOUT) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 789);
			return result;
		}
		request->command = 1;
	}
	request->result = 0;
	request->outSize = 0;
	request->outPayload = NULL;
	request->out0 = 0;
	request->out1 = 0;
	request->out2 = 0;
	request->out3 = 0;
	*outDone = request->command == 0;
	return 0;
}

/* a stream thread's trampoline: hand the thread's argument to its entry */
extern "C" int SwMwThreadTrampoline(SceSize args, void *argument)
{
	struct SwMwCommThread *thread = (struct SwMwCommThread *)argument;

	thread->entry(thread->argument);
	return 0;
}

/* a stream thread: receive this stream's requests until one times out or fails, and let
 * the manager's dispatch handle each one */
extern "C" int SwMwCommServerThread(void *argument)
{
	struct SwMwThreadArgument *thread = (struct SwMwThreadArgument *)argument;
	struct SwMwCommChannel *channel = thread->channel;
	struct SwMwCommServer *server = channel->server;
	struct SwMwIpcMessage request;
	int done = 0;
	int result;

	for (;;) {
		result = SwMwCommServerReceiveRequest(channel, thread->pipe, &request, &done);
		if (result != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 851);
			done = 1;
		}
		if (done != 0)
			break;
		server->dispatch(channel, &request, server->dispatchContext);
	}
	free(thread);
	return 0;
}

/* the process-wide SW settings the thread helper reads */
static int g_swsToolMode;
static int g_swsAffinity;

extern "C" int SwsGetToolMode(void)
{
	return g_swsToolMode;
}

extern "C" void SwsSetToolMode(int value)
{
	g_swsToolMode = value;
}

extern "C" int SwsGetAffinity(void)
{
	return g_swsAffinity;
}

extern "C" void SwsSetAffinity(int value)
{
	g_swsAffinity = value;
}

/*
 * Start one of the middleware's threads: the type picks the name and the stack size,
 * the tool mode the priority (111, or the system's 0x10000040), the SW_DEP type the
 * affinity the channel open asked for (192 when it is set and not the system's).
 */
extern "C" int SwMwThreadCreate(struct SwMwCommThread *thread, int (*entry)(void *),
				void *argument, int type)
{
	char name[32];
	SceSize stackSize = 0x4000;
	int priority = 111;
	int affinity = 0;
	int id;

	thread->entry = entry;
	thread->argument = argument;
	memcpy(name, "SWS_THREAD_NO_NAME", 0x13);
	name[19] = 0;
	if (SwsGetToolMode() == 0)
		priority = 0x10000040;
	switch (type) {
	case 0:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 0x1F);
		break;
	case 1:
		strncpy(name, "SWS_THREAD_TYPE_AKE_CMD_TIMER", 0x1F);
		break;
	case 2:
		strncpy(name, "SWS_THREAD_TYPE_KX_EXP", 0x1F);
		break;
	case 3:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AUTH", 0x1F);
		stackSize = 0x8000;
		break;
	case 4:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AKE_TIMER", 0x1F);
		break;
	case 5:
		strncpy(name, "SWS_THREAD_TYPE_SINK_CKC", 0x1F);
		break;
	case 6:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH", 0x1F);
		stackSize = 0x8000;
		break;
	case 7:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH_SERVER", 0x1F);
		stackSize = 0x8000;
		break;
	case 8:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AKE_TIMER", 0x1F);
		stackSize = 0x8000;
		break;
	case 9:
		strncpy(name, "SWS_THREAD_TYPE_RTT_REG", 0x1F);
		stackSize = 0x8000;
		break;
	case 10:
		strncpy(name, "SWS_THREAD_TYPE_TTL_CHECK", 0x1F);
		break;
	case 11:
		strncpy(name, "SWS_THREAD_TYPE_MEDIA_DISCONNECT", 0x1F);
		break;
	case 16:
		strncpy(name, "SWS_THREAD_TYPE_SW_TIMER", 0x1F);
		break;
	case 17:
		strncpy(name, "SWS_THREAD_TYPE_SW_DEP", 0x1F);
		affinity = SwsGetAffinity();
		if (affinity != 0) {
			affinity |= 0x80000;
			if (affinity != 0x80000)
				priority = 192;
		}
		break;
	case 18:
		stackSize = 0x20000;
		strncpy(name, "SWS_THREAD_TYPE_TOOL", 0x1F);
		break;
	case 19:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 0x1F);
		affinity = SwsGetAffinity();
		if (affinity != 0) {
			affinity |= 0x80000;
			if (affinity != 0x80000)
				priority = 192;
		}
		break;
	default:
		break;
	}
	id = sceKernelCreateThread(name, SwMwThreadTrampoline, priority, stackSize, 0,
				   affinity, NULL);
	if (id >= 0) {
		thread->id = id;
		return sceKernelStartThread(id, 0xC, thread);
	}
	return id;
}

/*
 * Set a channel up (0x81001832): open the single stream's CtoS pipe, create the four
 * streams' request pipes (StoC) and, when the channel carries streams, open their CtoS
 * pipes, then create or open the shared memory block. A failure tears the pipes down and
 * answers 12009.
 */
extern "C" int SwMwCommChannelSetup(struct SwMwCommChannel *channel, int bufferSize)
{
	char name[32];
	int result = 0;
	unsigned int i;

	channel->singlePipe = -1;
	if (channel->streamCount == 0) {
		sprintf(name, "%s_%02d", "SW_IPC_Strm_Pipe_CtoS", channel->index);
		result = SwMwIpcOpenPipe(name, &channel->singlePipe);
		if (result != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 275);
			goto fail;
		}
	}
	for (i = 0; i < 4; ++i) {
		channel->slots[i].pipeStoC = -1;
		channel->slots[i].pipeCtoS = -1;
	}
	for (i = 0; i < 4; ++i) {
		sprintf(name, "%s_%02d_%02d", "SW_IPC_Strm_Pipe_StoC", channel->index, i);
		result = SwMwIpcCreatePipe(name, &channel->slots[i].pipeStoC);
		if (result != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 286);
			goto fail;
		}
		if (channel->streamCount != 0) {
			sprintf(name, "%s_%02d_%02d", "SW_IPC_Strm_Pipe_CtoS", channel->index, i);
			result = SwMwIpcOpenPipe(name, &channel->slots[i].pipeCtoS);
			if (result != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 291);
				goto fail;
			}
		}
	}
	if (bufferSize == 0) {
		channel->sharedBase = NULL;
		channel->bufferSize = 0;
		return 0;
	}
	sprintf(name, "%s_%02d", "SW_IPC_Strm_ShMem", channel->index);
	result = SwMwCommServerCreateShared(&channel->shared, name, bufferSize,
					    &channel->sharedBase, 0);
	if (result != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 299);
		goto fail;
	}
	channel->bufferSize = bufferSize;
	return 0;
fail:
	for (i = 0; i < 4; ++i) {
		if (channel->slots[i].pipeStoC != -1) {
			result = SwMwIpcDeletePipe(channel->slots[i].pipeStoC);
			if (result != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_COMM_SOURCE, 312);
			channel->slots[i].pipeStoC = -1;
		}
		if (channel->slots[i].pipeCtoS != -1) {
			result = SwMwIpcClosePipe(channel->slots[i].pipeCtoS);
			if (result != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_COMM_SOURCE, 317);
			channel->slots[i].pipeCtoS = -1;
		}
	}
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 321);
	return 12009;
}

/* tear a channel down (0x81001b9c): free its shared block, then its stream pipes */
extern "C" int SwMwCommChannelTerm(struct SwMwCommChannel *channel)
{
	int result;
	int status;
	unsigned int i;

	if (channel->shared != NULL) {
		result = SwMwSharedMemoryFree((struct SwMwSharedMemory *)channel->shared, 0);
		if (result != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 336);
		channel->shared = NULL;
		channel->sharedBase = NULL;
		channel->bufferSize = 0;
	}
	for (i = 0; i < 4; ++i) {
		if (channel->slots[i].pipeStoC != -1) {
			status = SwMwIpcDeletePipe(channel->slots[i].pipeStoC);
			if (status != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_COMM_SOURCE, 345);
			channel->slots[i].pipeStoC = -1;
		}
		if (channel->slots[i].pipeCtoS != -1) {
			status = SwMwIpcClosePipe(channel->slots[i].pipeCtoS);
			if (status != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_COMM_SOURCE, 350);
			channel->slots[i].pipeCtoS = -1;
		}
	}
	if (channel->singlePipe != -1) {
		SwMwIpcClosePipe(channel->singlePipe);
		channel->singlePipe = -1;
	}
	return 0;
}

/* free a channel (0x81001cfc): its four stream pipe pairs and its single pipe */
extern "C" int SwMwCommChannelFree(struct SwMwCommChannel *channel)
{
	int result;
	unsigned int i;

	for (i = 0; i < 4; ++i) {
		if (channel->slots[i].pipeStoC != -1) {
			result = SwMwIpcDeletePipe(channel->slots[i].pipeStoC);
			if (result != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_COMM_SOURCE, 543);
			channel->slots[i].pipeStoC = -1;
		}
		if (channel->slots[i].pipeCtoS != -1) {
			result = SwMwIpcClosePipe(channel->slots[i].pipeCtoS);
			if (result != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_COMM_SOURCE, 548);
			channel->slots[i].pipeCtoS = -1;
		}
	}
	if (channel->singlePipe != -1) {
		SwMwIpcClosePipe(channel->singlePipe);
		channel->singlePipe = -1;
	}
	return 0;
}

/*
 * Open a channel (0x81001e0e): the request names the channel, its stream count and buffer
 * size; set the channel up, start one thread per stream (or the single one), and tell the
 * library the channel is open with a kind-5 control message. A failure tears it down.
 */
extern "C" int SwMwCommChannelOpen(struct SwMwCommServer *server,
				   struct SwMwIpcWireMessage *message)
{
	struct SwMwCommChannel *channel;
	struct SwMwThreadArgument *argument;
	struct SwMwIpcWireMessage control;
	int streams = message->value0;
	int index = message->value1;
	int bufferSize = message->value2;
	int affinity = message->value3;
	int result;
	int failed;
	unsigned int i;

	channel = &server->channels[index];
	channel->streamCount = streams;
	SwsSetAffinity(affinity);
	result = SwMwCommChannelSetup(channel, bufferSize);
	failed = 0;
	if (result != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 687);
		failed = 1;
	}
	if (failed == 0) {
		if (streams != 0) {
			i = 0;
			while (1) {
				argument = (struct SwMwThreadArgument *)malloc(8);
				if (argument == NULL)
					break;
				argument->channel = channel;
				argument->pipe = channel->slots[i].pipeCtoS;
				if (SwMwThreadCreate(&channel->slots[i].thread,
						     SwMwCommServerThread, argument, 19) != 0)
					break;
				if (++i >= 4) {
					control.kind = 5;
					control.result = 0;
					control.value0 = 0;
					control.value1 = 0;
					control.value2 = 0;
					control.value3 = 0;
					result = SwMwIpcSend(server->pipeStoC, &control);
					if (result != 0) {
						swMwTrace("[SWMW] Error %d in %s:%d\n", result,
							  SWMW_COMM_SOURCE, 724);
						goto fail;
					}
					return 0;
				}
			}
		} else {
			argument = (struct SwMwThreadArgument *)malloc(8);
			if (argument != NULL) {
				argument->channel = channel;
				argument->pipe = channel->singlePipe;
				if (SwMwThreadCreate(&channel->singleThread,
						     SwMwCommServerThread, argument, 0) == 0) {
					control.kind = 5;
					control.result = 0;
					control.value0 = 0;
					control.value1 = 0;
					control.value2 = 0;
					control.value3 = 0;
					result = SwMwIpcSend(server->pipeStoC, &control);
					if (result != 0) {
						swMwTrace("[SWMW] Error %d in %s:%d\n", result,
							  SWMW_COMM_SOURCE, 724);
						goto fail;
					}
					return 0;
				}
			}
		}
	}
fail:
	result = SwMwCommChannelTerm(channel);
	if (result != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_COMM_SOURCE, 732);
	return result;
}
