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

/* the server's shared memory: two channel slices of 16520 bytes */
#define SWMW_COMM_SHARED_SIZE		33040
#define SWMW_COMM_CHANNEL_STRIDE		16520
#define SWMW_COMM_CHANNEL_COUNT		2

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

/* store the 64-byte value a SW core reply carries (after its 12-byte header) in a
 * request's payload (0x81000c78) */
extern "C" int SwMwCommServerSetRequestValue(struct SwMwCommRequest *request, const void *value)
{
	memcpy(request->payload, (const char *)value + 12, 64);
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
		shared->memBlock = sceKernelAllocMemBlock(
			name, 0xC20D060, shared->size,
			(const SceKernelAllocMemBlockOpt *)&option);
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
	void (*entry)(void *argument);	/* 0x00: the thread's entry */
	SceUID id;			/* 0x04: its id, once started */
	void *argument;			/* 0x08: what the entry receives */
};

/* one stream of a channel: the pipes and the thread that carries it */
struct SwMwCommStreamSlot {
	SceUID createdPipe;		/* 0x00: the request pipe the server created */
	SceUID openedPipe;		/* 0x04: the reply pipe the library opened */
	struct SwMwCommThread thread;	/* 0x08 */
};

/* stop a stream's thread: wait for it to end, then delete it */
extern "C" int SwMwCommStreamStop(struct SwMwCommThread *thread)
{
	int result;

	result = sceKernelWaitThreadEnd(thread->id, NULL, NULL);
	sceKernelDeleteThread(thread->id);
	return result;
}
