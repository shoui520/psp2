/*
 * Reading libult's synchronisation objects and packing them into the
 * information records the drivers get (Sony's ult_sync_obj.h).
 */
#pragma once

#include <sysmem/sysmem.h>
#include "ulobjmgr_internal.h"
#include "util.h"

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "ult_sync_obj.h"

/* a thread reference: bit 0 set means tid << 1, else the address of the tid */
static inline __attribute__((always_inline))
int getTidUltId(SceUID pid, SceUInt32 thread, SceUInt32 *pTid)
{
	SCE_DBG_ASSERT(pTid != NULL, 171);
	if (thread & 1) {
		*pTid = thread >> 1;
		return 0;
	}
	return sceKernelCopyFromUserProc(pid, pTid, (const void *)thread, sizeof(*pTid));
}

/* name unknown: the number of threads the object's waiting queue pool holds */
static inline __attribute__((always_inline))
int getUltWaitingQueueCapacity(SceUID pid, SceUIntPtr address)
{
	UltWaitingQueueResourcePool pool;
	int ret;

	ret = sceKernelCopyFromUserProc(pid, &pool, (const void *)address, sizeof(pool));
	if (ret != 0)
		return ret;
	if (pool.header.size < sizeof(pool) ||
	    pool.header.type != ULT_OBJECT_WAITING_QUEUE_RESOURCE_POOL ||
	    pool.numThreads > ULT_MAX_NUM_THREADS)
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
	return pool.numThreads;
}

#define ULT_GET_AND_CHECK(_name, _type, _code, _line)				\
static inline __attribute__((always_inline))					\
int _name(SceUID pid, SceUIntPtr address, _type *obj)				\
{										\
	int ret;								\
										\
	SCE_DBG_ASSERT(obj != NULL, _line);					\
	ret = sceKernelCopyFromUserProc(pid, obj, (const void *)address,	\
					 sizeof(*obj));				\
	if (ret != 0)								\
		return ret;							\
	if (obj->header.size < sizeof(*obj) || obj->header.type != (_code))	\
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;				\
	return 0;								\
}

ULT_GET_AND_CHECK(getAndCheckUltMutexInfo, UltMutex, ULT_OBJECT_MUTEX, 215)
ULT_GET_AND_CHECK(getAndCheckUltCondInfo, UltCond, ULT_OBJECT_COND, 273)
ULT_GET_AND_CHECK(getAndCheckUltSemaInfo, UltSema, ULT_OBJECT_SEMA, 309)
ULT_GET_AND_CHECK(getAndCheckUltQueueDataResourcePoolInfo, UltQueueDataResourcePool,
		  ULT_OBJECT_QUEUE_DATA_RESOURCE_POOL, 346)
ULT_GET_AND_CHECK(getAndCheckUltWaitingQueueResourcePoolInfo, UltWaitingQueueResourcePool,
		  ULT_OBJECT_WAITING_QUEUE_RESOURCE_POOL, 394)
ULT_GET_AND_CHECK(getAndCheckUltRwlockInfo, UltRwlock, ULT_OBJECT_RWLOCK, 440)
ULT_GET_AND_CHECK(getAndCheckUltQueueInfo, UltQueue, ULT_OBJECT_QUEUE, 522)

/* the common head of every record: type, address and name */
#define ULT_PACK_HEADER(_info, _code, _address, _obj, _lobj, _linfo, _lname) do {	\
	SCE_DBG_ASSERT((_obj) != NULL, _lobj);					\
	SCE_DBG_ASSERT((_info) != NULL, _linfo);					\
	(_info)->header.type = (_code);						\
	(_info)->header.address = (_address);					\
	SCE_DBG_ASSERT((_obj)->header.name != NULL, _lname);			\
	copy32bWithNullTermination((_info)->header.name, (_obj)->header.name);	\
} while (0)

static inline __attribute__((always_inline))
int packUltMutexInfo(SceUID pid, UlobjMutexInfo *info, SceUIntPtr address,
		     const UltMutex *mutex, SceUInt32 numWaitThreads)
{
	SceUInt32 tid;
	int ret;

	ULT_PACK_HEADER(info, ULOBJ_INFO_MUTEX, address, mutex, 234, 235, 239);
	info->attr = mutex->attr & 1;
	info->ownerThreadType = mutex->ownerThreadType;
	info->waitingQueueResourcePool = mutex->waitingQueueResourcePool;
	if (mutex->lockCount == 0) {
		info->locked = 0;
		info->ownerThreadId = 0;
	} else {
		ret = getTidUltId(pid, mutex->owner, &tid);
		if (ret != 0)
			return ret;
		info->ownerThreadId = tid;
		info->locked = 1;
	}
	info->numWaitThreads = numWaitThreads;
	return 0;
}

static inline __attribute__((always_inline))
void packUltCondInfo(UlobjCondInfo *info, SceUIntPtr address, const UltCond *cond,
		     SceUInt32 numWaitThreads)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_COND, address, cond, 293, 294, 298);
	info->reserved0 = 0;
	info->mutex = cond->mutex;
	info->numWaitThreads = numWaitThreads;
}

static inline __attribute__((always_inline))
void packUltSemaInfo(UlobjSemaInfo *info, SceUIntPtr address, const UltSema *sema,
		     SceUInt32 numWaitThreads)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_SEMA, address, sema, 328, 329, 333);
	info->reserved0 = 0;
	info->waitingQueueResourcePool = sema->waitingQueueResourcePool;
	info->count = sema->count;
	info->numWaitThreads = numWaitThreads;
}

static inline __attribute__((always_inline))
void packUltQueueDataResourcePoolInfo(UlobjQueueDataResourcePoolInfo *info, SceUIntPtr address,
				      const UltQueueDataResourcePool *pool, int numUsedData,
				      int numFreeQueues)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL, address, pool, 367, 368, 372);
	info->reserved0 = 0;
	info->numData = pool->numData;
	info->numUsedData = numUsedData;
	info->dataSize = pool->dataSize;
	info->numQueues = pool->numQueues;
	if (numFreeQueues == -1)
		info->numUsedQueues = -1;
	else
		info->numUsedQueues = pool->numQueues - numFreeQueues;
}

static inline __attribute__((always_inline))
void packUltWaitingQueueResourcePoolInfo(UlobjWaitingQueueResourcePoolInfo *info,
					 SceUIntPtr address,
					 const UltWaitingQueueResourcePool *pool,
					 int numFreeThreads, int numFreeSyncObjects)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL, address, pool,
			415, 416, 420);
	info->numThreads = pool->numThreads;
	info->numFreeThreads = numFreeThreads;
	info->numSyncObjects = pool->numSyncObjects;
	if (numFreeSyncObjects == -1)
		info->numUsedSyncObjects = -1;
	else
		info->numUsedSyncObjects = pool->numSyncObjects - numFreeSyncObjects;
}

static inline SceUInt32 ultRwlockState(const UltRwlock *rwlock)
{
	return (rwlock->numReaders != 0 ? 2 : 0) | (rwlock->writeLocked != 0 ? 1 : 0);
}

static inline __attribute__((always_inline))
void packUltRwlockInfo(UlobjRwlockInfo *info, SceUIntPtr address, const UltRwlock *rwlock,
		       SceUInt32 numReadWaitThreads, SceUInt32 numWriteWaitThreads)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_RWLOCK, address, rwlock, 460, 461, 465);
	info->reserved0 = 0;
	info->waitingQueueResourcePool = rwlock->waitingQueueResourcePool;
	info->state = ultRwlockState(rwlock);
	info->numReaders = rwlock->numReaders;
	info->numReadWaitThreads = numReadWaitThreads;
	info->numWriteWaitThreads = numWriteWaitThreads;
}

/* also the writer's thread ID (0 while not write locked) */
static inline __attribute__((always_inline))
int packUltRwlockInfo2(SceUID pid, UlobjRwlockInfo *info, SceUIntPtr address,
		       const UltRwlock *rwlock, SceUInt32 numReadWaitThreads,
		       SceUInt32 numWriteWaitThreads, SceUInt32 *pWriterThreadId)
{
	SceUInt32 tid;
	int ret;

	SCE_DBG_ASSERT(rwlock != NULL, 485);
	SCE_DBG_ASSERT(info != NULL, 486);
	SCE_DBG_ASSERT(pWriterThreadId != NULL, 487);
	info->header.type = ULOBJ_INFO_RWLOCK;
	info->header.address = address;
	SCE_DBG_ASSERT(rwlock->header.name != NULL, 491);
	copy32bWithNullTermination(info->header.name, rwlock->header.name);
	info->reserved0 = 0;
	info->waitingQueueResourcePool = rwlock->waitingQueueResourcePool;
	info->state = ultRwlockState(rwlock);
	info->numReaders = rwlock->numReaders;
	info->numReadWaitThreads = numReadWaitThreads;
	info->numWriteWaitThreads = numWriteWaitThreads;
	if (rwlock->writeLocked == 0) {
		*pWriterThreadId = 0;
		return 0;
	}
	ret = getTidUltId(pid, rwlock->writer, &tid);
	if (ret != 0)
		return ret;
	*pWriterThreadId = tid;
	return 0;
}

static inline __attribute__((always_inline))
void packUltQueueInfo(UlobjQueueInfo *info, SceUIntPtr address, const UltQueue *queue,
		      SceUInt32 numSendWaitThreads, SceUInt32 numReceiveWaitThreads,
		      SceUInt32 numData)
{
	ULT_PACK_HEADER(info, ULOBJ_INFO_QUEUE, address, queue, 542, 543, 547);
	info->reserved0 = 0;
	info->waitingQueueResourcePool = queue->waitingQueueResourcePool;
	info->queueDataResourcePool = queue->queueDataResourcePool;
	info->dataSize = queue->dataSize;
	info->numSendWaitThreads = numSendWaitThreads;
	info->numReceiveWaitThreads = numReceiveWaitThreads;
	info->numData = numData;
}

#pragma pop_macro("__FILE_NAME__")
