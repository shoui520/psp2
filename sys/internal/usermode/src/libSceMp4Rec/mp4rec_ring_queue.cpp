/*
 * SceLibMp4Recorder thread-safe ring queue for buffered file writes.
 */
#include "mp4rec_internal.h"

int32_t BufferQueueReset(BufferQueue *queue)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		for (uint32_t i = 0; i < queue->capacity; i++)
			queue->entries[i].size = queue->entrySize;
		queue->writeIndex = 0;
		queue->readIndex = 0;
		queue->count = 0;
		sceKernelUnlockLwMutex(&queue->mutex, 1);
	}
	return ret;
}

int32_t BufferQueueInit(BufferQueue *queue, uint32_t entrySize, uint32_t capacity,
	BufferQueueEntry *entries, const char *mutexName)
{
	memset(queue, 0, sizeof(*queue));
	int32_t ret = sceKernelCreateLwMutex(&queue->mutex, mutexName, 2U, 0, NULL);
	if (ret >= 0) {
		queue->entries = entries;
		queue->flags |= 1U;
		queue->entrySize = entrySize;
		queue->capacity = capacity;
		ret = BufferQueueReset(queue);
	}
	return ret;
}

int32_t BufferQueueTerm(BufferQueue *queue)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	int32_t ret = sceKernelDeleteLwMutex(&queue->mutex);
	queue->flags = 0;
	return ret;
}

int32_t BufferQueueGetFreeCount(BufferQueue *queue)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		int32_t freeCount = (int32_t)(queue->capacity - queue->count);
		sceKernelUnlockLwMutex(&queue->mutex, 1);
		return freeCount;
	}
	return ret;
}

int32_t BufferQueueGetUsedCount(BufferQueue *queue)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		int32_t used = (int32_t)queue->count;
		sceKernelUnlockLwMutex(&queue->mutex, 1);
		return used;
	}
	return ret;
}

int32_t BufferQueueGetCapacity(BufferQueue *queue)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		int32_t cap = (int32_t)queue->capacity;
		sceKernelUnlockLwMutex(&queue->mutex, 1);
		return cap;
	}
	return ret;
}

int32_t BufferQueuePeekWrite(BufferQueue *queue, uint32_t offset, BufferQueueEntry **outEntry)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		int32_t freeCount = BufferQueueGetFreeCount(queue);
		if (offset >= (uint32_t)freeCount) {
			*outEntry = NULL;
			ret = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		} else {
			uint32_t idx = (queue->writeIndex + offset) % queue->capacity;
			*outEntry = &queue->entries[idx];
		}
		sceKernelUnlockLwMutex(&queue->mutex, 1);
	}
	return ret;
}

int32_t BufferQueuePeekRead(BufferQueue *queue, uint32_t offset, BufferQueueEntry **outEntry)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		if (offset >= queue->count) {
			*outEntry = NULL;
			ret = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		} else {
			uint32_t idx = (queue->readIndex + offset) % queue->capacity;
			*outEntry = &queue->entries[idx];
		}
		sceKernelUnlockLwMutex(&queue->mutex, 1);
	}
	return ret;
}

int32_t BufferQueueCommitWrite(BufferQueue *queue, uint32_t count)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		int32_t freeCount = BufferQueueGetFreeCount(queue);
		if ((int32_t)count > freeCount) {
			ret = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		} else {
			queue->writeIndex = (queue->writeIndex + count) % queue->capacity;
			queue->count += count;
		}
		sceKernelUnlockLwMutex(&queue->mutex, 1);
	}
	return ret;
}

int32_t BufferQueueCommitRead(BufferQueue *queue, uint32_t count)
{
	if ((queue->flags & 1U) == 0)
		return (int32_t)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t ret = sceKernelLockLwMutex(&queue->mutex, 1, NULL);
	if (ret >= 0) {
		if (count > queue->count) {
			ret = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		} else {
			queue->readIndex = (queue->readIndex + count) % queue->capacity;
			queue->count -= count;
		}
		sceKernelUnlockLwMutex(&queue->mutex, 1);
	}
	return ret;
}

int32_t BufferPoolNotifyCallbacks(BufferPoolCallbackList *cbList, int32_t event,
	int32_t count, int32_t capacity)
{
	if (cbList != NULL) {
		for (uint32_t i = 0; i < cbList->count; i++) {
			if (cbList->entries[i].fn != NULL)
				cbList->entries[i].fn(event, count, capacity, cbList->entries[i].userArg);
		}
	}
	return 0;
}
