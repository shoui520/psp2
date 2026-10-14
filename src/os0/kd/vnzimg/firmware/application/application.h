/*
 * The application ("VK1", at +0x11000 of the image): the codec engine's
 * services for the ARM, run on the kernel's threads and VThreads.
 */
#pragma once

#include "api.h"
#include "unmanaged.h"

/* the codec engine's errors (Sony's SDK, codecengine.h) */
#define SCE_CODECENGINE_ERROR_INVALID_POINTER		((int32_t)0x80600000)
#define SCE_CODECENGINE_ERROR_INVALID_SIZE		((int32_t)0x80600001)
#define SCE_CODECENGINE_ERROR_INVALID_VALUE		((int32_t)0x80600009)
/* not in the SDK: a VThread context could not be made */
#define VNZ_THREAD_CONTEXT_ERROR_CREATE			((int32_t)0x80600080)
/* not in the SDK: a state change of the memory protocol failed */
#define VNZ_EXT_MEMORY_ERROR_OPEN			((int32_t)0x80600081)
#define VNZ_EXT_MEMORY_ERROR_CLOSE			((int32_t)0x80600082)

/* memory opened in one call (application/memory.c; VVNZRunner's names) */
typedef struct VnzExtMemory {
	uint32_t paddr;			/* the range given, unmanaged */
	uint32_t vaddrManagedHeader;	/* entered into the protocol */
	uint32_t vaddrPublicVnz;
	uint32_t vaddrPrivateVnz;
	uint32_t vaddrProtectedVnz;
	uint32_t size;
} VnzExtMemory;

#define VNZ_MEMORY_MODE_PUBLIC		0x1
#define VNZ_MEMORY_MODE_PRIVATE		0x10
#define VNZ_MEMORY_MODE_PROTECTED	0x100

/* application/main.c */
uint32_t getState(void);
void setState(uint32_t state);
uint32_t getCoreCount(void);
void setCoreCount(uint32_t count);
uint32_t rpcAllocate(uint32_t kind, uint32_t size);
int32_t rpcFree(uint32_t kind, uint32_t memory, uint32_t size);
uint32_t rpcAllocatePublic(uint32_t kind, uint32_t size);
int32_t rpcFreePublic(uint32_t kind, uint32_t memory, uint32_t size);
int32_t rpcInvalidate(uint32_t memory, uint32_t size);
uint32_t rpcOpenPublic(uint32_t memory, uint32_t size);
uint32_t rpcClosePublic(uint32_t memory, uint32_t size);
int32_t rpcVThreadResourceCreate(uint32_t holder);
int32_t rpcVThreadResourceDelete(uint32_t holder);
int32_t rpcVThreadSetCores(uint32_t count);
int32_t rpcPowerDown(void);
int32_t rpcNothing(void);
void applicationStart(void);

/* application/memory.c */
uint32_t enterMemoryAccessProtocol(uint32_t address, uint32_t size);
uint32_t leaveMemoryAccessProtocol(uint32_t address, uint32_t size);
uint32_t openRawMemory(uint32_t address, uint32_t size);
uint32_t closeRawMemory(uint32_t address, uint32_t size);
uint32_t openPublicMemory(uint32_t address, uint32_t size);
uint32_t closePublicMemory(uint32_t address, uint32_t size);
uint32_t openProtectedMemory(uint32_t address, uint32_t size);
uint32_t closeProtectedMemory(uint32_t address, uint32_t size);
uint32_t openPrivateMemory(uint32_t address, uint32_t size);
uint32_t closePrivateMemory(uint32_t address, uint32_t size);
void vnzExtMemoryCreate(VnzExtMemory *memory);
int32_t vnzExtMemoryOpen(VnzExtMemory *memory, uint32_t isManagedMemory, uint32_t mode,
	uint32_t *address, uint32_t size);
int32_t vnzExtMemoryClose(VnzExtMemory *memory, uint32_t *address);

/* application/cache.c */
void cacheInvalidateFlush(uint32_t address, uint32_t size);
void cacheFlushRange(uint32_t address, uint32_t size);
void cacheFlushWriteback(uint32_t address, uint32_t size);
void cacheL2Writeback(uint32_t address, uint32_t size);
void cachePurgeRange(uint32_t address, uint32_t size);
void cachePurgeInvalidate(uint32_t address, uint32_t size);
void cacheL2Invalidate(uint32_t address, uint32_t size);

/*
 * The audio decoders' engines (MP3 0x84d430-, AAC 0x84d9d8-): an instance is
 * made in memory the caller gives; control gets (positive keys) or sets
 * (negative keys) count settings.
 */
typedef struct AudioDecoderEngine {
	int32_t (*create)(void **handle, void *memory, uint32_t size);
	int32_t (*control)(void *handle, const int32_t *keys, uint32_t count, int32_t *values,
		uint32_t *work);
	int32_t (*decode)(void *handle, const uint8_t **input, uint32_t *inputSize, uint32_t count,
		int16_t **output, uint32_t *outputSize, uint32_t *frames);
	int32_t (*destroy)(void *handle);
	uint32_t instanceSize;
} AudioDecoderEngine;

const AudioDecoderEngine *mp3Engine(void);
const AudioDecoderEngine *aacEngine(void);

/* memory taken in one call each way (application/vnzlib.c; VVNZRunner's names) */
typedef struct VnzMemory {
	uint32_t paddr;
	uint32_t size;
	uint32_t vaddrVnz;		/* while open */
} VnzMemory;

void vnzMemoryCreate(VnzMemory *memory, uint32_t paddr, uint32_t size);
uint32_t vnzMemoryOpenPublic(VnzMemory *memory);
uint32_t vnzMemoryClosePublic(VnzMemory *memory);
uint32_t vnzMemoryOpenPrivate(VnzMemory *memory);
uint32_t vnzMemoryClosePrivate(VnzMemory *memory);
uint32_t vnzMemoryOpenProtected(VnzMemory *memory);
uint32_t vnzMemoryCloseProtected(VnzMemory *memory);
int32_t vnzVThreadExecute(VkVThreadContext *context, uint32_t index, VkJobFunction entry,
	uint32_t argument, uint32_t argument2, uint32_t argument3, uint32_t argument4);
