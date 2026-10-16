/* OSA queue, event-flag and semaphore interface adapters. */
#include <stddef.h>
#include "robin/sync.h"

int osaMsgQueueCreate(struct OsaMessageQueue **queue, const char *name,
		      uint32_t messageSize, uint32_t capacity,
		      uint32_t allocationClass)
{
	if (!queue)
		for (;;)
			;
	if (!*queue)
		for (;;)
			;
	return osaCreateMessageQueue(*queue, name, messageSize, capacity, NULL,
				     allocationClass);
}

int osaMsgQueueSend(struct OsaMessageQueue *queue, uint32_t messageSize,
		    uintptr_t message, uint32_t timeout)
{
	return osaSendMessageQueue(queue, messageSize, message, timeout);
}

int osaMsgQueueReceive(struct OsaMessageQueue *queue, uintptr_t *message,
		       uint32_t messageSize, uint32_t timeout)
{
	return osaReceiveMessageQueue(queue, message, messageSize, timeout);
}

int osaEventFlagsInit(struct OsaEventFlags **flags)
{
	if (!flags)
		for (;;)
			;
	if (!*flags)
		for (;;)
			;
	return osaCreateEventFlags(*flags);
}

int osaEventFlagsSet(struct OsaEventFlags *flags, uint32_t events, uint32_t mode)
{
	return osaSetEventFlags(flags, events, mode);
}

int osaEventFlagsWait(struct OsaEventFlags *flags, uint32_t requested,
		      uint32_t mode, uint32_t *actual, uint32_t timeout)
{
	return osaWaitEventFlags(flags, requested, mode, actual, timeout);
}

int osaSemaphoreInit(struct OsaSemaphore **semaphore, uint32_t count,
		     uint32_t allocationClass)
{
	if (!semaphore)
		for (;;)
			;
	if (!*semaphore)
		for (;;)
			;
	return osaCreateSemaphore(*semaphore, count);
}

int osaSemaphoreWait(struct OsaSemaphore *semaphore, uint32_t timeout)
{
	int result = osaWaitSemaphore(semaphore, timeout);

	if (result == 17)
		return 18;
	return result;
}

int osaSemaphoreRelease(struct OsaSemaphore *semaphore)
{
	return osaReleaseSemaphore(semaphore);
}

