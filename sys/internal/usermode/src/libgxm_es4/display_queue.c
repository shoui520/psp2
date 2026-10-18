/*
 * SceGxm: the display queue.
 *
 * Flips wait in a ring until the GPU has finished the frame; a thread
 * then hands each one to the application's callback in order.
 */

#include "gxm_internal.h"

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

static int gxmDisplayQueueThread(SceSize args, void *argp)
{
	GxmDisplayQueue *queue = *(GxmDisplayQueue **)argp;
	SceUInt8 flag;
	int err;

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
		sceKernelDelayThread(10000);
		err = gxmDisplayQueueFlip(queue);
		if (err < 0)
			sceClibPrintf("Wait error %08x\n", err);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

int gxmDisplayQueueCreate(SceUInt32 maxPending, SceGxmDisplayQueueCallback *callback,
			  SceSize callbackDataSize, GxmDisplayQueue **queueOut, SceUInt32 cpuAffinity)
{
	GxmDisplayQueue *queue;
	SceUInt32 count, i;
	int err;

	if (maxPending - 1 >= 16)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (callback == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (callbackDataSize == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (queueOut == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	/* the ring holds a power of two entries */
	count = 1;
	while (count < maxPending)
		count *= 2;
	queue = SceGpuEs4User_7AF40138(sizeof(*queue) + count * (sizeof(GxmDisplayQueueEntry) +
								callbackDataSize));
	if (queue == NULL)
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	sceClibMemset(queue, 0, sizeof(*queue));
	queue->maxPending = maxPending;
	queue->readIndex = 0;
	queue->writeIndex = 0;
	queue->entries = queue->storage;
	queue->mask = count - 1;
	for (i = 0; i < count; i++) {
		queue->entries[i].syncHandle = 0;
		queue->entries[i].callbackData = (char *)&queue->entries[count] + i * callbackDataSize;
	}
	queue->callback = callback;
	queue->callbackDataSize = callbackDataSize;

	for (i = 0; i < count; i++) {
		if (SceGpuEs4User_416C8813(gxmGlobals.gpuDevice, &queue->entries[i].syncHandle) != 0) {
			for (i = 0; i < count; i++) {
				if (queue->storage[i].syncHandle != 0)
					SceGpuEs4User_7DCCBCAF(gxmGlobals.gpuDevice,
							       queue->storage[i].syncHandle);
			}
			err = SCE_GXM_ERROR_DRIVER;
			goto fail;
		}
	}

	queue->exit = 0;
	err = queue->readyEvent = sceKernelCreateEventFlag("SceGxmDisplayQueueReady", 0, 0, NULL);
	if (err < 0)
		goto fail;
	err = queue->completeEvent = sceKernelCreateEventFlag("SceGxmDisplayQueueComplete", 0, 0, NULL);
	if (err < 0)
		goto fail;
	if (!sceKernelIsGameBudget())
		cpuAffinity = 0;
	err = queue->thread = sceKernelCreateThread("SceGxmDisplayQueue", gxmDisplayQueueThread,
						    0x40, 0x1000, 0, cpuAffinity, NULL);
	if (err < 0)
		goto fail;
	err = sceKernelStartThread(queue->thread, sizeof(queue), &queue);
	if (err != 0)
		goto fail;
	/* the thread is running once it signals */
	sceKernelWaitEventFlag(queue->completeEvent, 1,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       NULL, NULL);
	*queueOut = queue;
	return 0;

fail:
	SceGpuEs4User_1BE5985A(queue);
	return err;
}

int sceGxmDisplayQueueAddEntry(void *oldBuffer, void *newBuffer, const void *callbackData)
{
	GxmDisplayQueue *queue = gxmGlobals.displayQueue;
	SceUInt32 frame = gxmGlobals.frameIndex;
	GxmDisplayQueueEntry *entry;
	void *syncObjects[2];
	SceUInt32 count;

	gxmGlobals.frameIndex = (frame + 1) & 0xffffff;
	if (queue == NULL)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (oldBuffer == NULL || newBuffer == NULL ||
	    (callbackData == NULL && queue->callbackDataSize != 0))
		return SCE_GXM_ERROR_INVALID_POINTER;
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
	if (SceGpuEs4User_0D27286F(gxmGlobals.gpuDevice, entry->syncHandle, syncObjects, count,
				   2, NULL, NULL) != 0)
		return SCE_GXM_ERROR_DRIVER;
	sceClibMemcpy(entry->callbackData, callbackData, queue->callbackDataSize);
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

	if (queue == NULL)
		return SCE_GXM_ERROR_UNINITIALIZED;
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
	SceUInt32 i;

	if (queue == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	queue->exit = 1;
	sceKernelSetEventFlag(queue->readyEvent, 1);
	sceKernelWaitThreadEnd(queue->thread, NULL, NULL);
	sceKernelDeleteEventFlag(queue->completeEvent);
	sceKernelDeleteEventFlag(queue->readyEvent);
	for (i = 0; i <= queue->mask; i++)
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
