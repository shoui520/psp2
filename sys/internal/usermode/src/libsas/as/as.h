/*
 * The AS (audio system) object model SceSasUser is built on: handles of a few classes
 * (CORE, BUSS and the RACKs), created from configuration strings into a buffer the caller
 * gives, kept in a tree under their parents and destroyed child first.
 */
#ifndef SCE_SAS_AS_H
#define SCE_SAS_AS_H

#include <scetypes.h>
#include <kernel/threadmgr.h>

#include "as_util.h"

struct SceSasAsTaskList;

/* the magic of a live handle ('ASHS') and of its subsystem word ('ASFT') */
#define SCE_SAS_AS_HANDLE_MAGIC		0x41534853
#define SCE_SAS_AS_SUBSYSTEM_MAGIC	0x41534654

/* class IDs */
#define SCE_SAS_AS_CLASS_CORE		0x434f5245	/* 'CORE' */
#define SCE_SAS_AS_CLASS_BUSS		0x42555353	/* 'BUSS' */
#define SCE_SAS_AS_CLASS_RACK		0x5241434b	/* 'RACK' */

#define SCE_SAS_AS_CLASS_VERSION	0x1000

/* the AS runtime's errors (with as_util.h's) */
#define SCE_SAS_AS_ERROR_BUSY		(-1)
#define SCE_SAS_AS_ERROR_NO_LOG_HANDLER	(-2)
#define SCE_SAS_AS_ERROR_INVALID_HANDLE	(-6)
#define SCE_SAS_AS_ERROR_NOT_LOCKED	(-7)
#define SCE_SAS_AS_ERROR_NOT_FOUND	(-9)
#define SCE_SAS_AS_ERROR_BROKEN		(-100)

/* log levels */
#define SCE_SAS_AS_LOG_ERROR		0
#define SCE_SAS_AS_LOG_WARNING		1
#define SCE_SAS_AS_LOG_INFO		2

#define SCE_SAS_AS_LOG_HANDLER_COUNT	4
#define SCE_SAS_AS_LOG_LENGTH		1024

typedef struct SceSasAsHandle SceSasAsHandle;

typedef int (*SceSasAsLogHandler)(int level, int code, const char *message);

/* what a render gives each rack */
typedef struct SceSasAsRenderInfo {
	SceUInt32 sampleRate;
	SceUInt32 granularity;
	SceUInt8 numChannels;
	SceUInt8 numBusses;
	SceUInt8 processor;
	SceUInt8 numProcessors;
	void **bussBuffers;
	void *output;
	SceUInt8 outputSampleSize;
	SceUInt8 outputChannels;
} SceSasAsRenderInfo;

#define SCE_SAS_AS_SAMPLE_RATE		48000

/*
 * What a handle can do, from its class (or the defaults): lock one of its parameter
 * blocks for writing and unlock it, take a synth event and report a state, and the three
 * steps of a render: update with the render's parameters, render into the job list, and
 * the end of the render.
 */
typedef struct SceSasAsHandleOps {
	int (*lock)(SceSasAsHandle *handle, SceUInt32 block, void **pointer);
	int (*unlock)(SceSasAsHandle *handle, SceUInt32 block);
	int (*reserved)(SceSasAsHandle *handle);	/* no class has one, none calls it */
	int (*setEvent)(SceSasAsHandle *handle, SceUInt32 event, SceUInt32 value);
	int (*getState)(SceSasAsHandle *handle, SceUInt32 state, void *value);
	int (*update)(SceSasAsHandle *handle, const SceSasAsRenderInfo *render);
	int (*render)(SceSasAsHandle *handle, const SceSasAsRenderInfo *render,
		      struct SceSasAsTaskList *tasks);
	int (*renderEnd)(SceSasAsHandle *handle, const SceSasAsRenderInfo *render);
} SceSasAsHandleOps;

typedef struct SceSasAsClass {
	SceUInt32 id;
	const char *name;
	SceUInt32 version;
	SceUInt32 reserved[2];
	int (*create)(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
		      SceSasAsHandle **handle);
	int (*destroy)(SceSasAsHandle *handle);
	void (*removeChild)(SceSasAsHandle *handle, SceSasAsHandle *child);
	SceSasAsHandleOps ops;
} SceSasAsClass;

/* the head of every AS object, the class's fields after it */
struct SceSasAsHandle {
	SceUInt32 magic;
	SceUInt32 classId;
	const SceSasAsClass *class;
	SceSasAsHandle *parent;
	void *buffer;
	SceSize size;
	volatile SceInt32 referenceCount;	/* -1 once destroyed */
	volatile SceUInt32 lock;		/* bit 0 guards children */
	SceSasAsHandle *children;
	SceSasAsHandle *next;
	SceUInt32 numChannels;
	SceUInt32 numProcessors;
	SceUInt32 numGranularities;
	SceSasAsHandleOps ops;
	SceUInt32 *subsystem;
};

/* a parameter block a lock gives: its owner and number, the block after them */
typedef struct SceSasAsLockedBlock {
	SceSasAsHandle *handle;
	SceUInt32 block;
} SceSasAsLockedBlock;

/* exclusive access (no barriers, as Sony's code) */
static inline SceUInt32 sceSasAsLoadExclusive(volatile SceUInt32 *address)
{
	SceUInt32 value;

	__asm__ volatile("ldrex %0, [%1]" : "=r"(value) : "r"(address) : "memory");
	return value;
}

static inline int sceSasAsStoreExclusive(volatile SceUInt32 *address, SceUInt32 value)
{
	int failed;

	__asm__ volatile("strex %0, %2, [%1]" : "=&r"(failed) : "r"(address), "r"(value) : "memory");
	return failed;
}

/* stores desired while *address holds expected; the value found */
static inline SceUInt32 sceSasAsCompareAndSwap(volatile SceUInt32 *address, SceUInt32 expected,
					       SceUInt32 desired)
{
	volatile SceUInt32 previous;

	do {
		previous = sceSasAsLoadExclusive(address);
		if (previous != expected)
			break;
	} while (sceSasAsStoreExclusive(address, desired) != 0);
	return previous;
}

/* bit 0 of *lock, waiting longer the longer it is held */
static inline void sceSasAsSpinLock(volatile SceUInt32 *lock)
{
	volatile SceUInt32 value;
	volatile SceUInt32 locked;
	volatile SceUInt32 spins;

	spins = 0;
	do {
		while ((value = *lock) & 1) {
			if (spins > 0x80)
				sceKernelDelayThread(1000);
			else if (spins > 0x40)
				sceKernelDelayThread(500);
			else if (spins > 0x20)
				sceKernelDelayThread(199);
			else if (spins > 0x10)
				sceKernelDelayThread(100);
			spins++;
		}
		locked = value | 1;
	} while (sceSasAsCompareAndSwap(lock, value, locked) != value);
}

static inline void sceSasAsSpinUnlock(volatile SceUInt32 *lock)
{
	volatile SceUInt32 value;
	volatile SceUInt32 unlocked;

	do {
		value = *lock;
		if ((value & 1) == 0)
			break;
		unlocked = value & ~1u;
	} while (sceSasAsCompareAndSwap(lock, value, unlocked) != value);
}

/* sets bits in *address; the value before */
static inline SceUInt32 sceSasAsAtomicOr(volatile SceUInt32 *address, SceUInt32 bits)
{
	volatile SceUInt32 value;
	volatile SceUInt32 set;

	do {
		value = *address;
		set = value | bits;
	} while (sceSasAsCompareAndSwap(address, value, set) != value);
	return value;
}

/* clears bit 0 of a held *lock: SCE_SAS_AS_ERROR_NOT_LOCKED when it was not set */
static inline int sceSasAsSpinTryUnlock(volatile SceUInt32 *lock)
{
	volatile SceUInt32 value;
	volatile SceUInt32 unlocked;

	for (;;) {
		value = *lock;
		if ((value & 1) == 0)
			return SCE_SAS_AS_ERROR_NOT_LOCKED;
		unlocked = value & ~1u;
		if (sceSasAsCompareAndSwap(lock, value, unlocked) == value)
			return 0;
	}
}

/* a lightweight mutex that may have failed to be created */
typedef struct SceSasAsMutex {
	SceUInt8 created;
	SceKernelLwMutexWork work;
} SceSasAsMutex;

static inline void sceSasAsMutexCreate(SceSasAsMutex *mutex, const char *name)
{
	if (sceKernelCreateLwMutex(&mutex->work, name, SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0,
				   NULL) >= 0)
		mutex->created = 1;
}

static inline void sceSasAsMutexDelete(SceSasAsMutex *mutex)
{
	if (mutex != NULL && mutex->created) {
		sceKernelDeleteLwMutex(&mutex->work);
		mutex->created = 0;
	}
}

static inline void sceSasAsMutexLock(SceSasAsMutex *mutex)
{
	if (mutex != NULL && mutex->created)
		sceKernelLockLwMutex(&mutex->work, 1, NULL);
}

static inline void sceSasAsMutexUnlock(SceSasAsMutex *mutex)
{
	if (mutex != NULL && mutex->created)
		sceKernelUnlockLwMutex(&mutex->work, 1);
}

/* a reference on a handle not destroyed yet: 1, else 0 */
static inline int sceSasAsHandleTryAddRef(SceSasAsHandle *handle)
{
	volatile SceInt32 count;
	volatile SceInt32 added;

	for (;;) {
		if ((count = handle->referenceCount) < 0)
			return 0;
		added = count + 1;
		if ((SceInt32)sceSasAsCompareAndSwap((volatile SceUInt32 *)&handle->referenceCount,
						     count, added) == count)
			return 1;
	}
}

static inline void sceSasAsHandleDropRef(SceSasAsHandle *handle)
{
	volatile SceInt32 count;
	volatile SceInt32 dropped;

	for (;;) {
		count = handle->referenceCount;
		dropped = count - 1;
		if ((SceInt32)sceSasAsCompareAndSwap((volatile SceUInt32 *)&handle->referenceCount,
						     count, dropped) == count)
			break;
	}
}

int sceSasAsLog(int level, int code, const char *format, va_list arguments);
int sceSasAsInitialize(void);
int sceSasAsTerminate(void);
int sceSasAsRegisterLogHandler(SceSasAsLogHandler handler);
int sceSasAsUnregisterLogHandler(SceSasAsLogHandler handler);
int sceSasAsError(int code, const char *format, ...);
int sceSasAsWarning(const char *format, ...);
int sceSasAsInfo(const char *format, ...);
int sceSasAsHandleIsClass(const SceSasAsHandle *handle, SceUInt32 classId);
int sceSasAsHandleCheckSubsystem(const SceSasAsHandle *handle);
int sceSasAsHandleAddRef(SceSasAsHandle *handle);
void sceSasAsHandleRelease(SceSasAsHandle *handle);
int sceSasAsHandleGetSize(const char *className, const char *config, SceSize *size);
int sceSasAsHandleCreate(SceSasAsHandle *parent, const char *className, const char *config,
			 void *buffer, SceSize size, SceSasAsHandle **handle);
int sceSasAsHandleDestroy(SceSasAsHandle *handle, void **buffer, SceSize *size);
int sceSasAsHandleLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer);
int sceSasAsHandleUnlock(void *pointer);
int sceSasAsHandleSetEvent(SceSasAsHandle *handle, SceUInt32 event, SceUInt32 value);
int sceSasAsHandleGetState(SceSasAsHandle *handle, SceUInt32 state, void *value);

#endif
