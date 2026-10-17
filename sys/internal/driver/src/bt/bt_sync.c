/* Bluetooth synchronization; the original source filename is unavailable. */
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <sysmem/atomic.h>
#include "bt_transport.h"

int BtCreateSignal(BtSignal *signal, unsigned int events, const char *name)
{
	SceUID eventFlag = ksceKernelCreateEventFlag(name, 0x9000, 0, NULL);

	signal->eventFlag = eventFlag;
	signal->events = events;
	signal->waiters = 0;
	return eventFlag;
}

int BtDeleteSignal(BtSignal *signal)
{
	ksceKernelDeleteEventFlag(signal->eventFlag);
	return 0;
}

int BtSetSignal(BtSignal *signal)
{
	uint32_t events;

	AtomicFullBarrier();
	do {
		events = AtomicLoadReserved32(&signal->events);
	} while (AtomicStoreReserved32(&signal->events, events | 1));
	if (signal->waiters)
		return ksceKernelSetEventFlag(signal->eventFlag, 1);
	return 0;
}

int BtWaitSignal(BtSignal *signal, uint32_t timeout)
{
	uint32_t waiters;
	uint32_t events;
	int result = 0;

	AtomicFullBarrier();
	do {
		waiters = AtomicLoadReserved32(&signal->waiters);
	} while (AtomicStoreReserved32(&signal->waiters, waiters + 1));
	while (result >= 0) {
		AtomicFullBarrier();
		do {
			events = AtomicLoadReserved32(&signal->events);
		} while (AtomicStoreReserved32(&signal->events, 0));
		if (events)
			break;
		result = ksceKernelWaitEventFlag(signal->eventFlag, 1, 5, NULL,
			(int32_t)timeout >= 0 ? &timeout : NULL);
	}
	AtomicFullBarrier();
	do {
		waiters = AtomicLoadReserved32(&signal->waiters);
	} while (AtomicStoreReserved32(&signal->waiters, waiters - 1));
	if (result == (int)0x80028005)
		return 0;
	return result < 0 ? result : 0;
}

int BtLockTransport(void)
{
	SceUID thread = ksceKernelGetThreadId();
	int result;

	if (gBtTransport.lockOwner == thread) {
		gBtTransport.lockDepth++;
		return 0;
	}
	result = BtWaitSignal(&gBtTransport.lockSignal, UINT32_MAX);
	if (result >= 0)
		gBtTransport.lockOwner = thread;
	return result;
}
