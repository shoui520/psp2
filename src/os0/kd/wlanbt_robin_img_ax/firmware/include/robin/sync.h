#ifndef ROBIN_SYNC_H
#define ROBIN_SYNC_H

#include <stdint.h>

struct OsaMessageQueue;
struct OsaEventFlags;
struct OsaSemaphore;

int osaMsgQueueCreate(struct OsaMessageQueue **queue, const char *name,
		      uint32_t messageSize, uint32_t capacity,
		      uint32_t allocationClass);
int osaMsgQueueSend(struct OsaMessageQueue *queue, uint32_t messageSize,
		    uintptr_t message, uint32_t timeout);
int osaMsgQueueReceive(struct OsaMessageQueue *queue, uintptr_t *message,
		       uint32_t messageSize, uint32_t timeout);
int osaEventFlagsInit(struct OsaEventFlags **flags);
int osaEventFlagsSet(struct OsaEventFlags *flags, uint32_t events, uint32_t mode);
int osaEventFlagsWait(struct OsaEventFlags *flags, uint32_t requested,
		      uint32_t mode, uint32_t *actual, uint32_t timeout);
int osaSemaphoreInit(struct OsaSemaphore **semaphore, uint32_t count,
		     uint32_t allocationClass);
int osaSemaphoreWait(struct OsaSemaphore *semaphore, uint32_t timeout);
int osaSemaphoreRelease(struct OsaSemaphore *semaphore);

/* RAM provider adapters; their external control-object layouts remain opaque. */
int osaCreateMessageQueue(struct OsaMessageQueue *queue, const char *name,
			  uint32_t messageSize, uint32_t capacity,
			  void *storage, uint32_t allocationClass);
int osaSendMessageQueue(struct OsaMessageQueue *queue, uint32_t messageSize,
			uintptr_t message, uint32_t timeout);
int osaReceiveMessageQueue(struct OsaMessageQueue *queue, uintptr_t *message,
			   uint32_t messageSize, uint32_t timeout);
int osaCreateEventFlags(struct OsaEventFlags *flags);
int osaSetEventFlags(struct OsaEventFlags *flags, uint32_t events, uint32_t mode);
int osaWaitEventFlags(struct OsaEventFlags *flags, uint32_t requested,
		     uint32_t mode, uint32_t *actual, uint32_t timeout);
int osaCreateSemaphore(struct OsaSemaphore *semaphore, uint32_t count);
int osaWaitSemaphore(struct OsaSemaphore *semaphore, uint32_t timeout);
int osaReleaseSemaphore(struct OsaSemaphore *semaphore);

#endif /* ROBIN_SYNC_H */
