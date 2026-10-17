/* SceSdif command queues (Sony source: cmd_queue.c). */
#include "sdif_internal.h"
#include <sysmem/debug.h>

void sdifCommandQueueInitialize(SdifCommandQueue *queue)
{
	queue->head = NULL;
	queue->tail = NULL;
}

void sdifCommandQueuePush(SdifCommandQueue *queue, SdifCommand *command)
{
	SdifCommand *tail = queue->tail;

	command->next = NULL;
	if (tail) {
		tail->next = command;
		queue->tail = command;
	} else {
		queue->head = command;
		queue->tail = command;
	}
}

void sdifCommandQueuePushFront(SdifCommandQueue *queue, SdifCommand *command)
{
	SdifCommand *head = queue->head;
	SdifCommand *tail = queue->tail;

	command->next = head;
	queue->head = command;
	if (!tail)
		queue->tail = command;
}

SdifCommand *sdifCommandQueuePop(SdifCommandQueue *queue)
{
	SdifCommand *command = queue->head;

	if (command) {
		SdifCommand *next = command->next;

		queue->head = next;
		if (!next)
			queue->tail = NULL;
	}
	return command;
}

SdifCommand *sdifCommandQueueRemove(SdifCommandQueue *queue, unsigned int index)
{
	SdifCommand *command = queue->head;
	SdifCommand *next;

	if (!command)
		return command;
	if (command->index == index) {
		next = command->next;
		queue->head = next;
		if (!next)
			queue->tail = NULL;
		return command;
	}
	while ((next = command->next) != NULL) {
		if (next->index == index) {
			SdifCommand *tail = queue->tail;

			command->next = next->next;
			if (tail == next)
				queue->tail = command;
			return next;
		}
		command = command->next;
	}
	return next;
}

SdifCommand *sdifCommandQueueFind(SdifCommandQueue *queue, unsigned int index)
{
	SdifCommand *command = queue->head;

	if (!command)
		return command;
	if (command->index == index)
		return command;
	while ((command = command->next) != NULL) {
		if (command->index == index)
			break;
	}
	return command;
}

void _sceSdifCmdQueueDump(SdifCommandQueue *queue)
{
#if FW_DEBUG
	SdifCommand *command = queue->head;
	int index = 0;

	while (command) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(138),
			"cmd[%d]:%p idx:%d(0x%x) arg:0x%x\n", index++,
			command, command->command, command->command,
			command->argument);
		command = command->next;
	}
#else
	(void)queue;
#endif
}
