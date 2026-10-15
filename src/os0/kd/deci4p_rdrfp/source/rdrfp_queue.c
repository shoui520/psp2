/*
 * SceDeci4pRDrfp request queues (Sony's rdrfp_queue.c): a list guarded by a
 * mutex, with an event flag set whenever something is queued.
 */
#include <psp2kern/kernel/threadmgr.h>
#include "rdrfp.h"

int rdrfpInitQueue(RdrfpQueue *queue)
{
	int ret;

	queue->head.next = &queue->head;
	queue->head.prev = &queue->head;
	ret = ksceKernelCreateMutex("SceDeci4pRDrfp", 0x2000, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 38, "CreateMutex failed (0x%08X)\n", ret);
		return ret;
	}
	queue->mutex = ret;
	ret = ksceKernelCreateEventFlag("SceDeci4pRDrfp", 0, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 49, "CreateEventFlag failed (0x%08X)\n", ret);
		return ret;
	}
	queue->evf = ret;
	queue->count = 0;
	return 0;
}

void rdrfpEnqueue(RdrfpQueue *queue, RdrfpRequest *req)
{
	RdrfpLink *link = &req->link;
	RdrfpLink *head = &queue->head;
	int count;

	ksceKernelLockMutex(queue->mutex, 1, NULL);
	SCE_DBG_ASSERT_LEVEL(0, &req->link == req->link.next, 81);
	count = queue->count;
	link->next = head;
	link->prev = head->prev;
	head->prev = link;
	link->prev->next = link;
	queue->count = count + 1;
	ksceKernelSetEventFlag(queue->evf, 1);
	ksceKernelUnlockMutex(queue->mutex, 1);
}

void rdrfpRemoveFromQueue(RdrfpQueue *queue, RdrfpRequest *req)
{
	ksceKernelLockMutex(queue->mutex, 1, NULL);
	if (&req->link != req->link.next) {
		req->link.next->prev = req->link.prev;
		req->link.prev->next = req->link.next;
		req->link.next = &req->link;
		req->link.prev = &req->link;
		queue->count--;
	}
	ksceKernelUnlockMutex(queue->mutex, 1);
}

/* the oldest request; with wait, sleep until there is one */
RdrfpRequest *rdrfpDequeue(RdrfpQueue *queue, int wait)
{
	RdrfpRequest *req;
	int ret = 0;

	ksceKernelLockMutex(queue->mutex, 1, NULL);
	if (wait == 1) {
		while (queue->count == 0) {
			ksceKernelUnlockMutex(queue->mutex, 1);
			ret = ksceKernelWaitEventFlagCB(queue->evf, 1,
							SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT,
							NULL, NULL);
			ksceKernelLockMutex(queue->mutex, 1, NULL);
			if (ret < 0)
				break;
		}
	}
	if (ret < 0 || queue->count == 0) {
		req = NULL;
	} else {
		req = (RdrfpRequest *)queue->head.next;
		req->link.next->prev = req->link.prev;
		req->link.prev->next = req->link.next;
		req->link.next = &req->link;
		req->link.prev = &req->link;
		queue->count--;
	}
	ksceKernelUnlockMutex(queue->mutex, 1);
	return req;
}

int rdrfpIsQueueEmpty(const RdrfpQueue *queue)
{
	return queue->count == 0;
}
