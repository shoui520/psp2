/*
 * SceUlobjMgrForDriver for the synchronisation objects: lists, sizes and
 * information records (Sony's driver_api_for_ult_sync_obj.c).
 * Types: 0x11 mutex, 0x12 condition variable, 0x13 semaphore, 0x14
 * reader/writer lock, 0x15 queue, 0x16 waiting queue resource pool, 0x17
 * queue data resource pool.
 */
#include "ult_sync_obj.h"

#define ULOBJ_SYNC_OBJ_MUTEX	0x11
#define ULOBJ_SYNC_OBJ_COND	0x12
#define ULOBJ_SYNC_OBJ_SEMA	0x13
#define ULOBJ_SYNC_OBJ_RWLOCK	0x14
#define ULOBJ_SYNC_OBJ_QUEUE	0x15
#define ULOBJ_SYNC_OBJ_WQRP	0x16
#define ULOBJ_SYNC_OBJ_QDRP	0x17

static inline __attribute__((always_inline)) SceUInt8 getSceUltObjectType(int type)
{
	switch (type) {
	case ULOBJ_SYNC_OBJ_MUTEX:
		return ULT_OBJECT_MUTEX;
	case ULOBJ_SYNC_OBJ_COND:
		return ULT_OBJECT_COND;
	case ULOBJ_SYNC_OBJ_SEMA:
		return ULT_OBJECT_SEMA;
	case ULOBJ_SYNC_OBJ_RWLOCK:
		return ULT_OBJECT_RWLOCK;
	case ULOBJ_SYNC_OBJ_QUEUE:
		return ULT_OBJECT_QUEUE;
	case ULOBJ_SYNC_OBJ_WQRP:
		return ULT_OBJECT_WAITING_QUEUE_RESOURCE_POOL;
	case ULOBJ_SYNC_OBJ_QDRP:
		return ULT_OBJECT_QUEUE_DATA_RESOURCE_POOL;
	default:
		SCE_DBG_ASSERT(0, 59);
		return 0;
	}
}

int sceUlobjMgrGetUltSyncObjId(SceUID pid, SceUInt32 *ids, SceUInt32 maxIds, SceUInt32 skip,
			       int type, UlobjProcCB *procCB)
{
	struct {
		UltObjectHeader header;
		SceUInt32 reserved;
	} obj;
	SceUInt32 i, entry, n = 0;
	SceUInt8 objectType;

	SCE_DBG_ASSERT(procCB != NULL, 68);
	objectType = getSceUltObjectType(type);
	for (i = 0; i < procCB->numEntries; i++) {
		entry = procCB->infoArray->entry[i];
		if (!(entry & ULOBJ_ENTRY_USED) ||
		    (entry & ULOBJ_ENTRY_KIND_MASK) != ULOBJ_KIND_SYNC_OBJECT)
			continue;
		if (sceKernelCopyFromUserProc(pid, &obj, (const void *)ULOBJ_ENTRY_ADDRESS(entry),
					       sizeof(obj)) != 0)
			continue;
		if (obj.header.type != objectType)
			continue;
		if (ids == NULL) {
			n++;
			continue;
		}
		if (skip != 0) {
			skip--;
			continue;
		}
		ids[n++] = ULOBJ_ENTRY_ADDRESS(entry);
		if (n >= maxIds)
			break;
	}
	return n;
}

static inline int isValidListArgs(void *info, SceUInt32 *ids, SceUInt32 *pNumIds)
{
	return info != NULL && ids != NULL && pNumIds != NULL && (SceInt32)*pNumIds >= 0;
}

/* a mutex and the threads waiting for it (up to *pNumIds) */
int SceUlobjMgrForDriver_6D2C8928(SceUID pid, SceUIntPtr address, UlobjMutexInfo *info,
				  SceUInt32 *ids, SceUInt32 *pNumIds)
{
	UltMutex mutex;
	int capacity, n, ret;

	if (!isValidListArgs(info, ids, pNumIds))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltMutexInfo(pid, address, &mutex);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, mutex.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, mutex.waitList, 0, 0, capacity, *pNumIds, ids);
	if (n < 0)
		return n;
	*pNumIds = (SceUInt32)n < *pNumIds ? (SceUInt32)n : *pNumIds;
	return packUltMutexInfo(pid, info, address, &mutex, n);
}

int SceUlobjMgrForDriver_362C9488(SceUID pid, SceUIntPtr address, UlobjCondInfo *info,
				  SceUInt32 *ids, SceUInt32 *pNumIds)
{
	UltCond cond;
	UltMutex mutex;
	int capacity, n, ret;

	if (!isValidListArgs(info, ids, pNumIds))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltCondInfo(pid, address, &cond);
	if (ret != 0)
		return ret;
	ret = getAndCheckUltMutexInfo(pid, cond.mutex, &mutex);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, mutex.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, cond.waitList, 0, 0, capacity, *pNumIds, ids);
	if (n < 0)
		return n;
	*pNumIds = (SceUInt32)n < *pNumIds ? (SceUInt32)n : *pNumIds;
	packUltCondInfo(info, address, &cond, n);
	return 0;
}

int SceUlobjMgrForDriver_74B9199C(SceUID pid, SceUIntPtr address, UlobjSemaInfo *info,
				  SceUInt32 *ids, SceUInt32 *pNumIds)
{
	UltSema sema;
	int capacity, n, ret;

	if (!isValidListArgs(info, ids, pNumIds))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltSemaInfo(pid, address, &sema);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, sema.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, sema.waitList, 0, 0, capacity, *pNumIds, ids);
	if (n < 0)
		return n;
	*pNumIds = (SceUInt32)n < *pNumIds ? (SceUInt32)n : *pNumIds;
	packUltSemaInfo(info, address, &sema, n);
	return 0;
}

int SceUlobjMgrForDriver_6BCB4568(SceUID pid, SceUIntPtr address,
				  UlobjWaitingQueueResourcePoolInfo *info)
{
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getUltWaitingQueueResourcePoolInfo(pid, address, info);
	return ret < 0 ? ret : 0;
}

int SceUlobjMgrForDriver_ADAAA961(SceUID pid, SceUIntPtr address,
				  UlobjQueueDataResourcePoolInfo *info)
{
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getUltQueueDataResourcePoolInfo(pid, address, info);
	return ret < 0 ? ret : 0;
}

/* a reader/writer lock and its waiting readers, then writers */
int SceUlobjMgrForDriver_3685D4FB(SceUID pid, SceUIntPtr address, UlobjRwlockInfo *info,
				  SceUInt32 maxIds, SceUInt32 *ids, SceUInt32 *pNumReaders,
				  SceUInt32 *pNumWriters)
{
	UltRwlock rwlock;
	int capacity, numReaders, numWriters, ret;
	SceUInt32 k;

	if (ids == NULL || info == NULL || pNumReaders == NULL || pNumWriters == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltRwlockInfo(pid, address, &rwlock);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, rwlock.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numReaders = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_READ, 0, capacity,
					 maxIds, ids);
	if (numReaders < 0)
		return numReaders;
	k = (SceUInt32)numReaders < maxIds ? (SceUInt32)numReaders : maxIds;
	maxIds -= k;
	*pNumReaders = k;
	numWriters = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_WRITE, 0,
					 capacity, maxIds, ids + k);
	if (numWriters < 0)
		return numWriters;
	*pNumWriters = (SceUInt32)numWriters < maxIds ? (SceUInt32)numWriters : maxIds;
	packUltRwlockInfo(info, address, &rwlock, numReaders, numWriters);
	return 0;
}

/* the same, plus the writer's thread ID */
int SceUlobjMgrForDriver_0C29AFEC(SceUID pid, SceUIntPtr address, UlobjRwlockInfo *info,
				  SceUInt32 *pWriterThreadId, SceUInt32 maxIds, SceUInt32 *ids,
				  SceUInt32 *pNumReaders, SceUInt32 *pNumWriters)
{
	UltRwlock rwlock;
	int capacity, numReaders, numWriters, ret;
	SceUInt32 k;

	if (pWriterThreadId == NULL || info == NULL || pNumReaders == NULL || ids == NULL ||
	    pNumWriters == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltRwlockInfo(pid, address, &rwlock);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, rwlock.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numReaders = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_READ, 0, capacity,
					 maxIds, ids);
	if (numReaders < 0)
		return numReaders;
	k = (SceUInt32)numReaders < maxIds ? (SceUInt32)numReaders : maxIds;
	maxIds -= k;
	*pNumReaders = k;
	numWriters = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_WRITE, 0,
					 capacity, maxIds, ids + k);
	if (numWriters < 0)
		return numWriters;
	*pNumWriters = (SceUInt32)numWriters < maxIds ? (SceUInt32)numWriters : maxIds;
	return packUltRwlockInfo2(pid, info, address, &rwlock, numReaders, numWriters,
				  pWriterThreadId);
}

/* a queue: threads waiting to send, to receive, then the data addresses */
int SceUlobjMgrForDriver_5D7A7925(SceUID pid, SceUIntPtr address, UlobjQueueInfo *info,
				  SceUInt32 maxIds, SceUInt32 *ids, SceUInt32 *pNumSenders,
				  SceUInt32 *pNumReceivers, SceUInt32 *pNumData)
{
	UltQueue queue;
	UltQueueDataResourcePool pool;
	int poolCapacity, capacity, numSenders, numReceivers, numData, ret;
	SceUInt32 k, maxData;

	if (ids == NULL || info == NULL || pNumSenders == NULL || pNumReceivers == NULL ||
	    pNumData == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getAndCheckUltQueueInfo(pid, address, &queue);
	if (ret != 0)
		return ret;
	ret = getAndCheckUltQueueDataResourcePoolInfo(pid, queue.queueDataResourcePool, &pool);
	if (ret != 0)
		return ret;
	poolCapacity = getUltWaitingQueueCapacity(pid, pool.waitingQueueResourcePool);
	if (poolCapacity < 0)
		return poolCapacity;
	capacity = getUltWaitingQueueCapacity(pid, queue.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numSenders = getUltWaitThreadIds(pid, pool.waitList, 0, address, poolCapacity, maxIds,
					 ids);
	if (numSenders < 0)
		return numSenders;
	k = (SceUInt32)numSenders < maxIds ? (SceUInt32)numSenders : maxIds;
	ids += k;
	maxIds -= k;
	*pNumSenders = k;
	numReceivers = getUltWaitThreadIds(pid, queue.waitList, 0, 0, capacity, maxIds, ids);
	if (numReceivers < 0)
		return numReceivers;
	k = (SceUInt32)numReceivers < maxIds ? (SceUInt32)numReceivers : maxIds;
	maxIds -= k;
	*pNumReceivers = k;
	maxData = pool.numData < ULT_MAX_NUM_DATA ? pool.numData : ULT_MAX_NUM_DATA;
	numData = getUltQueueDataIds(pid, queue.dataList, maxData, maxIds, ids + k);
	if (numData < 0)
		return numData;
	*pNumData = (SceUInt32)numData < maxIds ? (SceUInt32)numData : maxIds;
	packUltQueueInfo(info, address, &queue, numSenders, numReceivers, numData);
	return 0;
}

static inline SceSize alignedQueueDataSize(const UltQueue *queue)
{
	return (queue->dataSize + 3) & ~3;
}

/* name unknown: the size getUltQueueInfo needs */
int getUltQueueInfoSize(SceUID pid, SceUIntPtr address)
{
	UltQueue queue;
	UltQueueDataResourcePool pool;
	int capacity, numReceivers, numSenders, numData, ret;
	SceUInt32 maxData;

	ret = getAndCheckUltQueueInfo(pid, address, &queue);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, queue.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numReceivers = getUltWaitThreadIds(pid, queue.waitList, 0, 0, capacity, 0, NULL);
	if (numReceivers < 0)
		return numReceivers;
	ret = getAndCheckUltQueueDataResourcePoolInfo(pid, queue.queueDataResourcePool, &pool);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, pool.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numSenders = getUltWaitThreadIds(pid, pool.waitList, 0, address, capacity, 0, NULL);
	if (numSenders < 0)
		return numSenders;
	maxData = pool.numData < ULT_MAX_NUM_DATA ? pool.numData : ULT_MAX_NUM_DATA;
	numData = copyUltQueueData(pid, queue.dataList, maxData, 0, 0, NULL);
	if (numData < 0)
		return numData;
	return alignedQueueDataSize(&queue) * numData +
	       (numSenders + numReceivers + sizeof(UlobjQueueInfo) / 4) * 4;
}

/* the record, the waiting threads and a copy of the data, all in info */
int getUltQueueInfo(SceUID pid, SceUIntPtr address, UlobjQueueInfo *info)
{
	UltQueue queue;
	UltQueueDataResourcePool pool;
	int capacity, numSenders, numReceivers, numData, ret;
	SceUInt32 maxData;
	SceSize stride;

	SCE_DBG_ASSERT(info != NULL, 335);
	ret = getAndCheckUltQueueInfo(pid, address, &queue);
	if (ret != 0)
		return ret;
	ret = getAndCheckUltQueueDataResourcePoolInfo(pid, queue.queueDataResourcePool, &pool);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, pool.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numSenders = getUltWaitThreadIds(pid, pool.waitList, 0, address, capacity, capacity,
					 info->waitThreadId);
	if (numSenders < 0)
		return numSenders;
	capacity = getUltWaitingQueueCapacity(pid, queue.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numReceivers = getUltWaitThreadIds(pid, queue.waitList, 0, 0, capacity, capacity,
					   info->waitThreadId + numSenders);
	if (numReceivers < 0)
		return numReceivers;
	stride = alignedQueueDataSize(&queue);
	maxData = pool.numData < ULT_MAX_NUM_DATA ? pool.numData : ULT_MAX_NUM_DATA;
	numData = copyUltQueueData(pid, queue.dataList, maxData, queue.dataSize, stride,
				   info->waitThreadId + numSenders + numReceivers);
	if (numData < 0)
		return numData;
	packUltQueueInfo(info, address, &queue, numSenders, numReceivers, numData);
	return stride * numData + (numSenders + sizeof(UlobjQueueInfo) / 4 + numReceivers) * 4;
}

/* the buffer size SceUlobjMgrForDriver_B978DB70 needs for an object */
int SceUlobjMgrForDriver_CDAD91AD(SceUID pid, SceUIntPtr address, int type)
{
	union {
		UltMutex mutex;
		UltSema sema;
		UltRwlock rwlock;
	} obj;
	UltCond cond;
	UltMutex mutex;
	int capacity, n, n2, ret;

	switch (type) {
	case ULOBJ_SYNC_OBJ_MUTEX:
		ret = getAndCheckUltMutexInfo(pid, address, &obj.mutex);
		if (ret != 0)
			return ret;
		capacity = getUltWaitingQueueCapacity(pid, obj.mutex.waitingQueueResourcePool);
		if (capacity < 0)
			return capacity;
		n = getUltWaitThreadIds(pid, obj.mutex.waitList, 0, 0, capacity, 0, NULL);
		if (n < 0)
			return n;
		return (n + sizeof(UlobjMutexInfo) / 4) * 4;
	case ULOBJ_SYNC_OBJ_COND:
		ret = getAndCheckUltCondInfo(pid, address, &cond);
		if (ret != 0)
			return ret;
		ret = getAndCheckUltMutexInfo(pid, cond.mutex, &mutex);
		if (ret != 0)
			return ret;
		capacity = getUltWaitingQueueCapacity(pid, mutex.waitingQueueResourcePool);
		if (capacity < 0)
			return capacity;
		n = getUltWaitThreadIds(pid, cond.waitList, 0, 0, capacity, 0, NULL);
		if (n < 0)
			return n;
		return (n + sizeof(UlobjCondInfo) / 4) * 4;
	case ULOBJ_SYNC_OBJ_SEMA:
		ret = getAndCheckUltSemaInfo(pid, address, &obj.sema);
		if (ret != 0)
			return ret;
		capacity = getUltWaitingQueueCapacity(pid, obj.sema.waitingQueueResourcePool);
		if (capacity < 0)
			return capacity;
		n = getUltWaitThreadIds(pid, obj.sema.waitList, 0, 0, capacity, 0, NULL);
		if (n < 0)
			return n;
		return (n + sizeof(UlobjSemaInfo) / 4) * 4;
	case ULOBJ_SYNC_OBJ_RWLOCK:
		ret = getAndCheckUltRwlockInfo(pid, address, &obj.rwlock);
		if (ret != 0)
			return ret;
		capacity = getUltWaitingQueueCapacity(pid, obj.rwlock.waitingQueueResourcePool);
		if (capacity < 0)
			return capacity;
		n = getUltWaitThreadIds(pid, obj.rwlock.waitList, ULT_RWLOCK_WAIT_READ, 0,
					capacity, 0, NULL);
		if (n < 0)
			return n;
		n2 = getUltWaitThreadIds(pid, obj.rwlock.waitList, ULT_RWLOCK_WAIT_WRITE, 0,
					 capacity, 0, NULL);
		if (n2 < 0)
			return n2;
		return (n2 + n + sizeof(UlobjRwlockInfo) / 4 + 1) * 4;
	case ULOBJ_SYNC_OBJ_QUEUE:
		return getUltQueueInfoSize(pid, address);
	case ULOBJ_SYNC_OBJ_WQRP:
		return sizeof(UlobjWaitingQueueResourcePoolInfo);
	case ULOBJ_SYNC_OBJ_QDRP:
		return sizeof(UlobjQueueDataResourcePoolInfo);
	default:
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
	}
}

static inline __attribute__((always_inline))
int getUltMutexInfo(SceUID pid, SceUIntPtr address, UlobjMutexInfo *info)
{
	UltMutex mutex;
	int capacity, n, ret;

	SCE_DBG_ASSERT(info != NULL, 148);
	ret = getAndCheckUltMutexInfo(pid, address, &mutex);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, mutex.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, mutex.waitList, 0, 0, capacity, capacity,
				info->waitThreadId);
	if (n < 0)
		return n;
	info->numWaitThreads = n;
	ret = packUltMutexInfo(pid, info, address, &mutex, n);
	if (ret != 0)
		return ret;
	return (n + sizeof(*info) / 4) * 4;
}

static inline __attribute__((always_inline))
int getUltCondInfo(SceUID pid, SceUIntPtr address, UlobjCondInfo *info)
{
	UltCond cond;
	UltMutex mutex;
	int capacity, n, ret;

	SCE_DBG_ASSERT(info != NULL, 552);
	ret = getAndCheckUltCondInfo(pid, address, &cond);
	if (ret != 0)
		return ret;
	ret = getAndCheckUltMutexInfo(pid, cond.mutex, &mutex);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, mutex.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, cond.waitList, 0, 0, capacity, capacity,
				info->waitThreadId);
	if (n < 0)
		return n;
	packUltCondInfo(info, address, &cond, n);
	return (n + sizeof(*info) / 4) * 4;
}

static inline __attribute__((always_inline))
int getUltSemaInfo(SceUID pid, SceUIntPtr address, UlobjSemaInfo *info)
{
	UltSema sema;
	int capacity, n, ret;

	SCE_DBG_ASSERT(info != NULL, 215);
	ret = getAndCheckUltSemaInfo(pid, address, &sema);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, sema.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	n = getUltWaitThreadIds(pid, sema.waitList, 0, 0, capacity, capacity,
				info->waitThreadId);
	if (n < 0)
		return n;
	packUltSemaInfo(info, address, &sema, n);
	return (n + sizeof(*info) / 4) * 4;
}

static inline __attribute__((always_inline))
int getUltReaderWriterLockInfo(SceUID pid, SceUIntPtr address, UlobjRwlockInfo *info)
{
	UltRwlock rwlock;
	int capacity, numReaders, numWriters, ret;
	SceUInt32 *writer;

	SCE_DBG_ASSERT(info != NULL, 458);
	ret = getAndCheckUltRwlockInfo(pid, address, &rwlock);
	if (ret != 0)
		return ret;
	capacity = getUltWaitingQueueCapacity(pid, rwlock.waitingQueueResourcePool);
	if (capacity < 0)
		return capacity;
	numReaders = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_READ, 0,
					 capacity, capacity, info->waitThreadId);
	if (numReaders < 0)
		return numReaders;
	numWriters = getUltWaitThreadIds(pid, rwlock.waitList, ULT_RWLOCK_WAIT_WRITE, 0,
					 capacity, capacity, info->waitThreadId + numReaders);
	if (numWriters < 0)
		return numWriters;
	writer = info->waitThreadId + numReaders + numWriters;
	/* a failure to read the writer's ID is not reported */
	packUltRwlockInfo2(pid, info, address, &rwlock, numReaders, numWriters, writer);
	return (numReaders + sizeof(*info) / 4 + 1 + numWriters) * 4;
}

/* the information record of an object, with its thread IDs (and data) appended */
int SceUlobjMgrForDriver_B978DB70(SceUID pid, SceUIntPtr address, int type, void *info)
{
	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	switch (type) {
	case ULOBJ_SYNC_OBJ_MUTEX:
		return getUltMutexInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_COND:
		return getUltCondInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_SEMA:
		return getUltSemaInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_RWLOCK:
		return getUltReaderWriterLockInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_QUEUE:
		return getUltQueueInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_WQRP:
		return getUltWaitingQueueResourcePoolInfo(pid, address, info);
	case ULOBJ_SYNC_OBJ_QDRP:
		return getUltQueueDataResourcePoolInfo(pid, address, info);
	default:
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
	}
}
