/*
 * The video library's RPCs (96-100) and its server threads.
 *
 * The ARM allocates a 256-byte context (RPC 100), creates the library in it
 * (RPC 97): the context becomes the library thread's VideoServer, which
 * initializes the library service on the work memory and then creates and
 * deletes the codecs' server threads for the ARM's commands.
 */
#include "video.h"

/* the video unit's registers; register 3 is the address of its input */

/* the library thread's commands */
#define LIBRARY_INITIALIZE		1
#define LIBRARY_CREATE_AVCDEC		AVCDEC_INITIALIZE
#define LIBRARY_DELETE_AVCDEC		AVCDEC_TERMINATE
#define LIBRARY_CREATE_M4VDEC		514
#define LIBRARY_DELETE_M4VDEC		520
#define LIBRARY_CREATE_AVCENC		771
#define LIBRARY_DELETE_AVCENC		777

#define DELETE_TRIES			50

typedef struct VideoServerEntry {
	void (*entry)(VideoServer *server, uint32_t argument2);
	uint32_t priority;
	uint32_t stackSize;
} VideoServerEntry;

static void libraryThread(VideoServer *server, uint32_t argument2);

static const VideoServerEntry videoServerEntries[4] = {
	{ libraryThread, 4, 0x1000 },
	{ avcdecThread, 4, 0x1000 },
	{ m4vdecThread, 4, 0x1000 },
	{ avcencThread, 4, 0x1000 },
};

/* the library service's callbacks, both unused */
static uint32_t libraryCallback(void)
{
	return 0;
}

static uint32_t libraryCallback2(void)
{
	return 0;
}

/* RPC 100: dsp0VideocodecAllocMemory, a zeroed context */
uint32_t rpcVideoAllocMemory(uint32_t kind, uint32_t size)
{
	uint32_t memory, address;

	memory = rpcAllocate(kind, size);
	if (!memory)
		return 0;
	size = (size + 255) & ~255;
	address = enterMemoryAccessProtocol(memory, size);
	if (!address)
		return 0;
	address = openPublicMemory(address, size);
	if (!address)
		return 0;
	memset((void *)address, 0, size);
	address = closePublicMemory(address, size);
	if (!address)
		return 0;
	return leaveMemoryAccessProtocol(address, size);
}

/* RPC 99: dsp0VideocodecInitializeMemory, the versions */
int32_t rpcVideoInitializeMemory(uint32_t paddr)
{
	VnzMemory memory;
	uint32_t *versions;
	int32_t result;

	videoInvalidate(paddr, 16);
	vnzMemoryCreate(&memory, paddr, 16);
	if (!videoMemoryOpenPrivate(&memory)) {
		result = VIDEO_ERROR_MEMORY;
	} else {
		versions = (uint32_t *)memory.vaddrVnz;
		versions[0] = avcencVersion();
		versions[1] = m4vdecVersion();
		versions[2] = videoEngineVersion();
		versions[3] = 0;
		result = 0;
	}
	if (memory.vaddrVnz && !videoMemoryClosePrivate(&memory) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/* RPC 96: dsp0VideocodecQueryMemory, the work memory's size */
int32_t rpcVideoQueryMemory(uint32_t paddr)
{
	VideoLibraryCommand *command;
	VnzMemory memory;
	int32_t result;

	videoInvalidate(paddr, 64);
	vnzMemoryCreate(&memory, paddr, 64);
	if (!videoMemoryOpenPrivate(&memory)) {
		result = VIDEO_ERROR_MEMORY;
	} else {
		command = (VideoLibraryCommand *)memory.vaddrVnz;
		command->workSize = videoLibraryQueryMemory(&command->param);
		result = 0;
	}
	if (memory.vaddrVnz && !videoMemoryClosePrivate(&memory) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

static void libraryThread(VideoServer *server, uint32_t argument2)
{
	VideoLibraryParam *param;
	uint32_t running = 1;
	VideoServer *codec;
	uint32_t kind, command;

	vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	do {
		if (!(vnzThreadWaitFlags(VIDEO_FLAG_COMMAND) & VIDEO_FLAG_COMMAND))
			continue;
		switch (server->command) {
		case LIBRARY_INITIALIZE:
			param = (VideoLibraryParam *)server->argument[1];
			param->resource = server->argument[4];
			param->core = 0;
			param->core2 = 1;
			param->callback[0] = libraryCallback;
			param->callback[1] = libraryCallback2;
			param->callback[2] = 0;
			VIDEO_UNIT[VIDEO_UNIT_ADDRESS] = 0;
			server->result = videoLibraryInitialize((uint32_t *)server->argument[0], param,
				server->argument[2], server->argument[3]);
			break;
		case VIDEO_COMMAND_EXIT:
			if (server->argument[0])
				videoLibraryTerminate((VideoLibrary *)server->argument[0]);
			server->result = 0;
			running = 0;
			break;
		case LIBRARY_CREATE_AVCDEC:
		case LIBRARY_CREATE_M4VDEC:
		case LIBRARY_CREATE_AVCENC:
			command = server->command;
			kind = command == LIBRARY_CREATE_AVCDEC ? VIDEO_SERVER_AVCDEC :
				command == LIBRARY_CREATE_M4VDEC ? VIDEO_SERVER_M4VDEC : VIDEO_SERVER_AVCENC;
			codec = (VideoServer *)server->argument[0];
			server->result = videoServerCreate(kind, codec);
			if (server->result)
				break;
			server->result = videoServerCall(codec, command, server->argument[1],
				server->argument[2], server->argument[3], server->argument[4], 0);
			if (server->result)
				videoServerDelete(codec, 0);
			break;
		case LIBRARY_DELETE_AVCDEC:
		case LIBRARY_DELETE_M4VDEC:
		case LIBRARY_DELETE_AVCENC:
			server->result = videoServerDelete((VideoServer *)server->argument[0],
				server->argument[1]);
			break;
		default:
			server->result = -1;
			break;
		}
		vnzThreadSetFlags(server->client, VIDEO_FLAG_DONE);
	} while (running);
	vnzThreadExit();
}

/* gives a server a command and waits for its result */
int32_t videoServerCall(VideoServer *server, uint32_t command, uint32_t argument,
	uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5)
{
	server->command = command;
	server->argument[1] = argument2;
	server->argument[0] = argument;
	server->argument[2] = argument3;
	server->argument[3] = argument4;
	server->argument[4] = argument5;
	server->client = vnzThreadCurrent();
	if (vnzThreadSetFlags(server->thread, VIDEO_FLAG_COMMAND) == -1)
		return VIDEO_ERROR_MEMORY;
	vnzThreadWaitFlags(VIDEO_FLAG_DONE);
	return server->result;
}

/* starts a server thread of a kind on this core and waits until it runs */
int32_t videoServerCreate(uint32_t kind, VideoServer *server)
{
	const VideoServerEntry *entry;
	int32_t thread;

	memset(server, 0, sizeof(*server));
	server->lock = vnzLockAllocate();
	if (!server->lock)
		return -1;
	entry = &videoServerEntries[kind - 1];
	thread = vnzThreadCreate(entry->entry, vnzCurrentCoreId(), entry->priority,
		entry->stackSize);
	if (thread == -1) {
		vnzLockFree(server->lock);
		return VIDEO_ERROR_MEMORY;
	}
	server->thread = (VkThread *)thread;
	server->client = vnzThreadCurrent();
	vnzThreadStart((VkThread *)thread, (uint32_t)server, 0);
	vnzThreadWaitFlags(VIDEO_FLAG_DONE);
	return 0;
}

/* tells a server thread to exit and deletes it once it has */
int32_t videoServerDelete(VideoServer *server, uint32_t argument)
{
	VkThread *thread = server->thread;
	uint32_t deleted = 0, tries = 0;
	int32_t result;

	result = videoServerCall(server, VIDEO_COMMAND_EXIT, argument, 0, 0, 0, 0);
	do {
		vnzThreadYield();
		if (!vnzThreadIsAlive(thread)) {
			vnzThreadDelete(thread);
			deleted = 1;
		}
		tries++;
	} while (tries < DELETE_TRIES && !deleted);
	vnzLockFree(server->lock);
	return result;
}

/* RPC 97: dsp0VideocodecCreateDecoder, the library thread and the library */
int32_t rpcVideoCreateLibrary(uint32_t paddr, uint32_t resource)
{
	VnzMemory memory, context, work;
	VideoLibraryCommand *command;
	VideoServer *server;
	int32_t result;

	videoInvalidate(paddr, 64);
	vnzMemoryCreate(&memory, paddr, 64);
	if (!videoMemoryOpenPublic(&memory)) {
		result = VIDEO_ERROR_MEMORY;
		goto out;
	}
	command = (VideoLibraryCommand *)memory.vaddrVnz;

	videoInvalidate(command->context, 256);
	vnzMemoryCreate(&context, command->context, 256);
	if (!videoMemoryOpenPublic(&context)) {
		result = VIDEO_ERROR_MEMORY;
		goto out;
	}
	command->context = context.vaddrVnz;

	videoInvalidate(command->workAddress, command->workSize);
	vnzMemoryCreate(&work, command->workAddress, command->workSize);
	if (!videoMemoryOpenPublic(&work)) {
		result = VIDEO_ERROR_MEMORY;
		goto out;
	}
	command->workAddress = work.vaddrVnz;

	server = (VideoServer *)command->context;
	result = videoServerCreate(VIDEO_SERVER_LIBRARY, server);
	if (result)
		goto out;
	result = videoServerCall(server, LIBRARY_INITIALIZE, (uint32_t)&command->library,
		(uint32_t)&command->param, command->workAddress, command->workSize, resource);
	if (result)
		videoServerDelete(server, 0);
out:
	/* the context and the work memory stay open for the library */
	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

/*
 * RPC 98: dsp0VideocodecDeleteDecoder. Sony's code goes on to the work and
 * context memory when the command cannot be mapped, through a stale
 * register; here that case only fails.
 */
int32_t rpcVideoDeleteLibrary(uint32_t paddr)
{
	VnzMemory memory, context, work;
	VideoLibraryCommand *command;
	int32_t result;

	videoInvalidate(paddr, 64);
	vnzMemoryCreate(&memory, paddr, 64);
	if (!videoMemoryOpenPublic(&memory))
		return VIDEO_ERROR_MEMORY;
	command = (VideoLibraryCommand *)memory.vaddrVnz;
	videoServerDelete((VideoServer *)command->context, command->library);
	result = 0;

	videoInvalidate(command->workAddress, command->workSize);
	vnzMemoryCreate(&work, command->workAddress, command->workSize);
	if (!videoMemoryClosePublic(&work, 1) && result >= 0)
		result = VIDEO_ERROR_MEMORY;

	videoInvalidate(command->context, 256);
	vnzMemoryCreate(&context, command->context, 256);
	if (!videoMemoryClosePublic(&context, 1) && result >= 0)
		result = VIDEO_ERROR_MEMORY;

	if (memory.vaddrVnz && !videoMemoryClosePublic(&memory, 0) && result >= 0)
		result = VIDEO_ERROR_MEMORY;
	return result;
}

int32_t videoMemoryOpenPublic(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address)
			memory->vaddrVnz = address;
	}
	return address;
}

/* closes the mapping, or with mapped set the address given as already open */
int32_t videoMemoryClosePublic(VnzMemory *memory, uint32_t mapped)
{
	uint32_t size = memory->size, address;

	address = closePublicMemory(mapped ? memory->paddr : memory->vaddrVnz, size);
	if (address) {
		memory->vaddrVnz = 0;
		address = leaveMemoryAccessProtocol(address, size);
		videoBusSync();
	}
	return address;
}

int32_t videoMemoryOpenPrivate(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address) {
			address = openPrivateMemory(address, size);
			if (address)
				memory->vaddrVnz = address;
		}
	}
	return address;
}

int32_t videoMemoryClosePrivate(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = closePrivateMemory(memory->vaddrVnz, size);
	if (address) {
		address = closePublicMemory(address, size);
		if (address) {
			memory->vaddrVnz = 0;
			address = leaveMemoryAccessProtocol(address, size);
		}
	}
	return address;
}

int32_t videoMemoryOpenProtected(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address) {
			address = openProtectedMemory(address, size);
			if (address)
				memory->vaddrVnz = address;
		}
	}
	return address;
}

int32_t videoMemoryCloseProtected(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = closeProtectedMemory(memory->vaddrVnz, size);
	if (address) {
		address = closePublicMemory(address, size);
		if (address) {
			memory->vaddrVnz = 0;
			address = leaveMemoryAccessProtocol(address, size);
		}
	}
	return address;
}

/* a read from each of the three buses, for writes before it to land */
void videoBusSync(void)
{
	(void)*(volatile uint8_t *)0xf2000000;
	(void)*(volatile uint8_t *)0xf4000000;
	(void)*(volatile uint8_t *)0xf6800000;
}

uint32_t videoWritebackToPhysical(uint32_t address, uint32_t size)
{
	uint32_t start = address & ~255;

	if (!address)
		return 0;
	vnzL2Writeback(start, (size + (address - start) + 255) & ~255);
	videoBusSync();
	return vnzToPhysical(address);
}

uint32_t videoPurgeToPhysical(uint32_t address, uint32_t size)
{
	uint32_t start = address & ~255;

	if (!address)
		return 0;
	vnzL2Purge(start, (size + (address - start) + 255) & ~255);
	videoBusSync();
	return vnzToPhysical(address);
}

/* private memory closed and opened again: written back, seen fresh */
uint32_t videoReopenPrivate(uint32_t address, uint32_t size)
{
	uint32_t physical;

	address = closePrivateMemory(address, size);
	if (!address)
		return 0;
	address = closePublicMemory(address, size);
	if (!address)
		return 0;
	physical = leaveMemoryAccessProtocol(address, size);
	if (!physical)
		return 0;
	address = enterMemoryAccessProtocol(physical, size);
	if (!address)
		return 0;
	address = openPublicMemory(address, size);
	if (!address)
		return 0;
	if (!openPrivateMemory(address, size))
		return 0;
	return physical;
}

/* drops what the caches hold of the ARM's memory */
uint32_t videoInvalidate(uint32_t address, uint32_t size)
{
	uint32_t physical = vnzToPhysical(address);

	size = (size + 255) & ~255;
	if (physical) {
		vnzCachePurgeAllCores(physical, size);
		vnzL2Invalidate(physical, size);
	}
	return physical;
}

/* the job, on the context, waited for */
int32_t videoVThreadExecute(VkVThreadContext *context, uint32_t index, VkJobFunction entry,
	uint32_t argument, uint32_t argument2, uint32_t argument3, uint32_t argument4)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);
	int32_t result;

	result = vkVThreadJobSetNotify(locked, index, 0, entry, argument, argument2, argument3,
		argument4, vnzThreadCurrent(), 0x80000000);
	result |= vkVThreadContextUnlock(locked);
	if (result)
		return VIDEO_ERROR_MEMORY;
	vnzThreadWaitFlags(0x80000000);
	return 0;
}
