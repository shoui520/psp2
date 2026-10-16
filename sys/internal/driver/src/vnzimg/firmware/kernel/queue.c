/*
 * The run queues: doubly linked lists of threads.
 */
#include "kernel.h"

void vkQueueAppend(VkQueue *queue, VkThread *thread)
{
	thread->next = NULL;
	if (queue->head) {
		thread->previous = queue->tail;
		queue->tail->next = thread;
	} else {
		thread->previous = NULL;
		queue->head = thread;
	}
	queue->tail = thread;
}

void vkQueuePrepend(VkQueue *queue, VkThread *thread)
{
	if (queue->head) {
		thread->previous = NULL;
		thread->next = queue->head;
		queue->head->previous = thread;
	} else {
		thread->next = NULL;
		thread->previous = NULL;
		queue->tail = thread;
	}
	queue->head = thread;
}

void vkQueueRemove(VkQueue *queue, VkThread *thread)
{
	if (thread->next && thread->previous) {
		thread->previous->next = thread->next;
		thread->next->previous = thread->previous;
	} else if (thread->next) {		/* the head */
		queue->head = thread->next;
		thread->next->previous = NULL;
	} else if (thread->previous) {		/* the tail */
		thread->previous->next = NULL;
		queue->tail = thread->previous;
	} else {				/* the only one */
		queue->head = NULL;
		queue->tail = NULL;
	}
}
