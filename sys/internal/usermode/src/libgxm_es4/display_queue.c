/*
 * SceGxm: the display queue.
 *
 * Flips wait in a ring until the GPU has finished the frame; a thread
 * then hands each one to the application's callback in order.
 */

#include "gxm_internal.h"

/* sceGxmDisplayQueueCreate's limit on maxPendingCount */
#define GXM_DISPLAY_QUEUE_MAX_PENDING_COUNT	16

/* flip every entry whose frame is done; stops at a wait error */
static inline int gxmDisplayQueueFlip(GxmDisplayQueue *queue)
{
	GxmDisplayQueueEntry *entry;
	SceUInt32 mask = queue->mask;
	int err;

	while (queue->readIndex != queue->writeIndex) {
		entry = &queue->entries[queue->readIndex & mask];
		err = SceGpuEs4User_5C3A6DFE(entry->syncHandle, 0);
		if (err < 0)
			return err;
		queue->callback(entry->callbackData);
		SceGpuEs4User_116449D2(gxmGlobals.gpuDevice, entry->syncHandle);
		queue->readIndex++;
		sceKernelSetEventFlag(queue->completeEvent, 1);
	}
	return 0;
}

/* what the display queue thread is started with (Sony's assertion names the type) */
typedef struct DisplayQueueThreadData {
	GxmDisplayQueue *queue;
} DisplayQueueThreadData;

static int gxmDisplayQueueThread(SceSize size, void *argp)
{
	GxmDisplayQueue *queue;
	SceUInt8 flag;
	int err;

	GXM_DBG_ASSERT(113, size == sizeof(DisplayQueueThreadData));
	queue = ((DisplayQueueThreadData *)argp)->queue;
	gxmThreadStateInit(&queue->threadState, &flag);
	sceKernelSetEventFlag(queue->completeEvent, 1);

	while (!queue->exit) {
		err = gxmDisplayQueueFlip(queue);
		if (err < 0) {
			sceClibPrintf("Wait error %08x\n", err);
			continue;
		}
		sceKernelWaitEventFlag(queue->readyEvent, 1,
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       NULL, NULL);
	}

	/* drain what is left before exiting */
	while (queue->readIndex != queue->writeIndex) {
		GXM_DBG_WARNING(149, "Waiting for display queue to finish");
		sceKernelDelayThread(10000);
		err = gxmDisplayQueueFlip(queue);
		if (err < 0)
			sceClibPrintf("Wait error %08x\n", err);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

/* set up a new queue of entryCount entries and start its thread */
static inline int gxmDisplayQueueInit(GxmDisplayQueue *queue, SceUInt32 maxPendingCount,
				      SceUInt32 entryCount, SceGxmDisplayQueueCallback *callback,
				      SceSize callbackDataSize, SceUInt32 cpuAffinity)
{
	DisplayQueueThreadData threadData;
	SceUInt32 i;
	int err;

	GXM_DBG_ASSERT(172, SCE_GXM_IS_POW2(entryCount));
	sceClibMemset(queue, 0, sizeof(*queue));
	queue->maxPending = maxPendingCount;
	queue->readIndex = 0;
	queue->writeIndex = 0;
	queue->entries = queue->storage;
	queue->mask = entryCount - 1;
	for (i = 0; i < entryCount; i++) {
		queue->storage[i].syncHandle = 0;
		queue->storage[i].callbackData = (char *)&queue->storage[entryCount] +
						 i * callbackDataSize;
	}
	queue->callback = callback;
	queue->callbackDataSize = callbackDataSize;

	for (i = 0; i < entryCount; i++) {
		err = SceGpuEs4User_416C8813(gxmGlobals.gpuDevice, &queue->storage[i].syncHandle);
		if (err != 0) {
			for (i = 0; i < entryCount; i++) {
				if (queue->storage[i].syncHandle != 0)
					SceGpuEs4User_7DCCBCAF(gxmGlobals.gpuDevice,
							       queue->storage[i].syncHandle);
			}
			GXM_DBG_ERROR(204, "Failed due to error in driver layer (%08X).", err);
			return SCE_GXM_ERROR_DRIVER;
		}
	}

	queue->exit = 0;
	queue->readyEvent = sceKernelCreateEventFlag("SceGxmDisplayQueueReady", 0, 0, NULL);
	if (queue->readyEvent < 0) {
		GXM_DBG_ERROR(219, "Failed to create display queue ready event flag.");
		return queue->readyEvent;
	}
	queue->completeEvent = sceKernelCreateEventFlag("SceGxmDisplayQueueComplete", 0, 0, NULL);
	if (queue->completeEvent < 0) {
		GXM_DBG_ERROR(228, "Failed to create display queue complete event flag.");
		return queue->completeEvent;
	}
	if (!sceKernelIsGameBudget())
		cpuAffinity = 0;
	queue->thread = sceKernelCreateThread("SceGxmDisplayQueue", gxmDisplayQueueThread,
					      0x40, 0x1000, 0, cpuAffinity, NULL);
	if (queue->thread < 0) {
		GXM_DBG_ERROR(243, "Failed to create display queue thread.");
		return queue->thread;
	}
	threadData.queue = queue;
	err = sceKernelStartThread(queue->thread, sizeof(threadData), &threadData);
	if (err != 0)
		return err;
	/* the thread is running once it signals */
	sceKernelWaitEventFlag(queue->completeEvent, 1,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       NULL, NULL);
	return 0;
}

int gxmDisplayQueueCreate(SceUInt32 maxPendingCount, SceGxmDisplayQueueCallback *callback,
			  SceSize callbackDataSize, GxmDisplayQueue **displayQueue,
			  SceUInt32 cpuAffinity)
{
	GxmDisplayQueue *queue;
	SceUInt32 entryCount;
	int err;

	if (maxPendingCount - 1 >= GXM_DISPLAY_QUEUE_MAX_PENDING_COUNT) {
		GXM_DBG_ERROR(281, "Parameter maxPendingCount to function sceGxmDisplayQueueCreate has "
			      "value %u, which is not in the range [1, %u].", maxPendingCount,
			      GXM_DISPLAY_QUEUE_MAX_PENDING_COUNT);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (callback == NULL) {
		GXM_DBG_ERROR_NULL(285, sceGxmDisplayQueueCreate, callback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (callbackDataSize == 0) {
		GXM_DBG_ERROR(289, "Parameter callbackDataSize to function sceGxmDisplayQueueCreate "
			      "cannot be 0.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (displayQueue == NULL) {
		GXM_DBG_ERROR_NULL(293, sceGxmDisplayQueueCreate, displayQueue);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	/* the ring holds a power of two entries */
	entryCount = 1;
	while (entryCount < maxPendingCount)
		entryCount *= 2;
	queue = SceGpuEs4User_7AF40138(sizeof(*queue) +
				       entryCount * (sizeof(GxmDisplayQueueEntry) + callbackDataSize));
	if (queue == NULL) {
		GXM_DBG_ERROR(308, "Host memory allocation for the display queue returned NULL.");
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	}
	err = gxmDisplayQueueInit(queue, maxPendingCount, entryCount, callback, callbackDataSize,
				  cpuAffinity);
	if (err != 0) {
		SceGpuEs4User_1BE5985A(queue);
		return err;
	}
	*displayQueue = queue;
	return 0;
}

int sceGxmDisplayQueueAddEntry(void *oldBuffer, void *newBuffer, const void *callbackData)
{
	GxmDisplayQueue *queue = gxmGlobals.displayQueue;
	SceUInt32 frame = gxmGlobals.frameIndex;
	GxmDisplayQueueEntry *entry;
	void *syncObjects[2];
	SceUInt32 count;
	int err;

	gxmGlobals.frameIndex = (frame + 1) & 0xffffff;
	if (queue == NULL) {
		GXM_DBG_ERROR(362, "Unable to call sceGxmDisplayQueueAddEntry without a display queue "
			      "callback set in SceGxmInitializeParameters.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (oldBuffer == NULL) {
		GXM_DBG_ERROR_NULL(366, sceGxmDisplayQueueAddEntry, oldBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (newBuffer == NULL) {
		GXM_DBG_ERROR_NULL(370, sceGxmDisplayQueueAddEntry, newBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (callbackData == NULL && queue->callbackDataSize != 0) {
		GXM_DBG_ERROR(374, "Parameter callbackData to function sceGxmDisplayQueueAddEntry is "
			      "NULL, and the amount of callback data required is non-zero.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	gxmRazorMarker(0x15000000, frame);

	while (queue->writeIndex - queue->readIndex == queue->maxPending)
		sceKernelWaitEventFlag(queue->completeEvent, 1,
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       NULL, NULL);
	entry = &queue->entries[queue->writeIndex & queue->mask];
	syncObjects[0] = oldBuffer;
	if (oldBuffer == newBuffer) {
		count = 1;
	} else {
		syncObjects[1] = newBuffer;
		count = 2;
	}
	err = SceGpuEs4User_0D27286F(gxmGlobals.gpuDevice, entry->syncHandle, syncObjects, count, 2,
				     NULL, NULL);
	if (err != 0) {
		GXM_DBG_ERROR(413, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	sceClibMemcpy_safe(entry->callbackData, callbackData, queue->callbackDataSize);
	queue->writeIndex++;
	sceKernelSetEventFlag(queue->readyEvent, 1);

	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC) {
		gxmRazorMarker(0x16000000, frame);
		gxmRazorCallbacks.displayQueueAddEntry();
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.displayQueueAddEntry(0);
	gxmGlobals.sceneIndex = 0;
	return 0;
}

int sceGxmDisplayQueueFinish(void)
{
	GxmDisplayQueue *queue = gxmGlobals.displayQueue;

	if (queue == NULL) {
		GXM_DBG_ERROR(476, "Unable to call sceGxmDisplayQueueFinish() without a display queue "
			      "callback set in SceGxmInitializeParameters.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	gxmRazorMarker(0x17000000, gxmGlobals.frameIndex);
	while (queue->writeIndex != queue->readIndex)
		sceKernelWaitEventFlag(queue->completeEvent, 1,
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       NULL, NULL);
	gxmRazorMarker(0x18000000, gxmGlobals.frameIndex);
	return 0;
}

SceUID sceGxmGetDisplayQueueThreadIdInternal(void)
{
	if (gxmGlobals.displayQueue == NULL)
		return -1;
	return gxmGlobals.displayQueue->thread;
}

int gxmDisplayQueueDestroy(GxmDisplayQueue *queue)
{
	SceUInt32 count, i;

	if (queue == NULL) {
		GXM_DBG_ERROR(526, "Parameter displayQueue to function sceGxmDisplayQueueDestroy is "
			      "NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	queue->exit = 1;
	sceKernelSetEventFlag(queue->readyEvent, 1);
	sceKernelWaitThreadEnd(queue->thread, NULL, NULL);
	sceKernelDeleteEventFlag(queue->completeEvent);
	sceKernelDeleteEventFlag(queue->readyEvent);
	count = queue->mask + 1;
	for (i = 0; i != count; i++)
		SceGpuEs4User_7DCCBCAF(gxmGlobals.gpuDevice, queue->entries[i].syncHandle);
	SceGpuEs4User_1BE5985A(queue);
	return 0;
}

SceUInt32 gxmDisplayQueueGetMaxPending(const GxmDisplayQueue *queue)
{
	return queue->maxPending;
}

SceBool gxmDisplayQueueIsCurrentThread(void)
{
	SceUID current = sceKernelGetThreadId();

	return current == (gxmGlobals.displayQueue != NULL ? gxmGlobals.displayQueue->thread : -1);
}
