/*
 * Walking libult's lists in user memory, and the two resource pools
 * (Sony's ult_sync_obj.c).
 */
#include "ult_sync_obj.h"

/* the links after the head of a free list; -1 when there are more than max */
static inline __attribute__((always_inline))
int countAvailableQueueConnection(SceUID pid, SceUIntPtr freeList, SceUInt32 max, int *pCount)
{
	UltLink link;
	SceUIntPtr p;
	int n = 0, ret;

	SCE_DBG_ASSERT(pCount != NULL, 87);
	p = freeList & ~7;
	if (isValidUltPointer(p)) {
		for (;;) {
			ret = sceKernelCopyFromUserProc(pid, &link, (const void *)p, sizeof(link));
			if (ret != 0)
				return ret;
			if (!isValidUltPointer(link.next))
				break;
			if (++n == max + 1) {
				n = -1;
				break;
			}
			p = link.next;
		}
	}
	*pCount = n;
	return 0;
}

int getUltQueueDataResourcePoolInfo(SceUID pid, SceUIntPtr address,
				    UlobjQueueDataResourcePoolInfo *info)
{
	UltQueueDataResourcePool pool;
	SceUInt32 maxData, maxQueues;
	int numUsedData, numFreeQueues, ret;

	SCE_DBG_ASSERT(info != NULL, 204);
	ret = getAndCheckUltQueueDataResourcePoolInfo(pid, address, &pool);
	if (ret != 0)
		return ret;
	maxData = pool.numData < ULT_MAX_NUM_DATA ? pool.numData : ULT_MAX_NUM_DATA;
	numUsedData = copyUltQueueData(pid, pool.dataList, maxData, 0, 0, NULL);
	if (numUsedData < 0)
		return numUsedData;
	maxQueues = pool.numQueues < ULT_MAX_NUM_SYNC_OBJECTS ? pool.numQueues :
		    ULT_MAX_NUM_SYNC_OBJECTS;
	ret = countAvailableQueueConnection(pid, pool.queueList, maxQueues, &numFreeQueues);
	if (ret != 0)
		return ret;
	packUltQueueDataResourcePoolInfo(info, address, &pool, numUsedData, numFreeQueues);
	return sizeof(*info);
}

int getUltWaitingQueueResourcePoolInfo(SceUID pid, SceUIntPtr address,
				       UlobjWaitingQueueResourcePoolInfo *info)
{
	UltWaitingQueueResourcePool pool;
	SceUInt32 maxThreads, maxSyncObjects;
	int numFreeThreads, numFreeSyncObjects, ret;

	SCE_DBG_ASSERT(info != NULL, 244);
	ret = getAndCheckUltWaitingQueueResourcePoolInfo(pid, address, &pool);
	if (ret != 0)
		return ret;
	maxThreads = pool.numThreads < ULT_MAX_NUM_THREADS ? pool.numThreads :
		     ULT_MAX_NUM_THREADS;
	ret = countAvailableQueueConnection(pid, pool.threadList, maxThreads, &numFreeThreads);
	if (ret != 0)
		return ret;
	maxSyncObjects = pool.numSyncObjects < ULT_MAX_NUM_SYNC_OBJECTS ? pool.numSyncObjects :
			 ULT_MAX_NUM_SYNC_OBJECTS;
	ret = countAvailableQueueConnection(pid, pool.syncObjectList, maxSyncObjects,
					    &numFreeSyncObjects);
	if (ret != 0)
		return ret;
	packUltWaitingQueueResourcePoolInfo(info, address, &pool, numFreeThreads,
					    numFreeSyncObjects);
	return sizeof(*info);
}
