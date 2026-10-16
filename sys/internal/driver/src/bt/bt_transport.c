/* Bluetooth SDIO transport; the original source filename is unavailable. */
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <string.h>
#include "sce/atomic.h"
#include "bt_transport.h"

void BtWriteLe24(uint8_t *destination, unsigned int value);
void BtPollTransport(void *argument);
int BtTransportThread(SceSize argumentSize, void *argument);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);

#if FW_VERSION >= 0x04000000
void *__memcpy_chk(void *destination, const void *source, size_t length,
		   size_t capacity);
#define TRANSPORT_COPY(destination, source, length) \
	__memcpy_chk(destination, source, length, 2044)
#else
#define TRANSPORT_COPY(destination, source, length) \
	memcpy(destination, source, length)
#endif

BtSdioPacketBuffer gBtTransmitPacket;
BtTransport gBtTransport;
BtSdioPacketBuffer gBtReceivePacket;

int BtPumpTransmit(BtTransport *transport)
{
	int result = transport->transmitActive;
	BtTxRequest *head = transport->transmitHead;
	uint8_t wakeup[1];
	int awakened;
	int length;

	if (result) {
		result = 0;
	} else if (head) {
		if (transport->asleep) {
			if (!transport->wakeupPending) {
				wakeup[0] = 2;
				awakened = SceSdifForDriver_3C4CDC8B(transport->function,
					0, 1, wakeup, 1);
				BtQueueTimer(&transport->wakeupTimer, BtPollTransport,
					transport, 1000000);
				if (awakened < 0)
					result = awakened;
				else
					transport->wakeupPending = 1;
			}
		} else {
			transport->transmitActive = 1;
			BtWriteLe24(gBtTransmitPacket.packet.length, head->length + 4);
			gBtTransmitPacket.packet.packetType = head->packetType;
			TRANSPORT_COPY(gBtTransmitPacket.packet.payload,
				head->data, head->length);
			length = head->length + 4;
			if (length > 128)
				length = (head->length + 131) & ~127;
			result = SceSdifForDriver_3C4CDC8B(transport->function,
				transport->ioPort, length, gBtTransmitPacket.bytes, 2);
		}
	}
	return result;
}

int BtHandleSdioInterrupt(void *subInterruptArgument, void *registerArgument,
			unsigned char priority)
{
	BtTransport *transport = registerArgument;
	uint32_t events;

	(void)subInterruptArgument;
	(void)priority;
	AtomicFullBarrier();
	do {
		events = AtomicLoadReserved32(&transport->interruptSignal.events);
	} while (AtomicStoreReserved32(&transport->interruptSignal.events,
		events | 1));
	if (transport->interruptSignal.waiters)
		ksceKernelSetEventFlag(transport->interruptSignal.eventFlag, 1);
	return 0;
}

int BtIsAsleep(void)
{
	return gBtTransport.asleep;
}

int BtUnlockTransport(int result)
{
	SceUID thread = ksceKernelGetThreadId();
	uint32_t available;

	if (gBtTransport.lockOwner != thread)
		return (int)0x802f0b01;
	if (gBtTransport.lockDepth) {
		gBtTransport.lockDepth--;
		return 0;
	}
	gBtTransport.lockOwner = 0;
	AtomicFullBarrier();
	do {
		available = AtomicLoadReserved32(&gBtTransport.lockSignal.events);
	} while (AtomicStoreReserved32(&gBtTransport.lockSignal.events, available | 1));
	if (gBtTransport.lockSignal.waiters)
		ksceKernelSetEventFlag(gBtTransport.lockSignal.eventFlag, 1);
	return result;
}

int BtQueueTransmit(unsigned int packetType, const uint8_t *data, int length,
		    void (*completion)(void *argument), void *argument)
{
	BtTxRequest *request = gBtTransport.freeRequests;
	BtTxRequest *tail;
	BtTxRequest **link;
	BtTxRequest *head;
	unsigned int active;
	int transferLength;
	int result;
	uint8_t wakeup[1];

	if (!request)
		return -1;
	tail = gBtTransport.transmitTail;
	active = gBtTransport.transmitActive;
	link = tail ? &tail->next : &gBtTransport.transmitHead;
	gBtTransport.freeRequests = request->next;
	request->packetType = packetType;
	request->data = data;
	request->length = length;
	request->completion = completion;
	request->prev = tail;
	request->argument = argument;
	*link = request;
	request->next = NULL;
	head = gBtTransport.transmitHead;
	gBtTransport.transmitTail = request;
	if (active)
		return 0;
	if (!head)
		return 0;
	if (gBtTransport.asleep) {
		if (gBtTransport.wakeupPending)
			return 0;
		wakeup[0] = 2;
		result = SceSdifForDriver_3C4CDC8B(gBtTransport.function,
			0, 1, wakeup, 1);
		BtQueueTimer(&gBtTransport.wakeupTimer, BtPollTransport,
			&gBtTransport, 1000000);
		if (result < 0)
			return result;
		gBtTransport.wakeupPending = 1;
		return 0;
	}
	gBtTransport.transmitActive = 1;
	BtWriteLe24(gBtTransmitPacket.packet.length, head->length + 4);
	gBtTransmitPacket.packet.packetType = head->packetType;
	TRANSPORT_COPY(gBtTransmitPacket.packet.payload, head->data, head->length);
	transferLength = head->length + 4;
	if (transferLength > 128)
		transferLength = (head->length + 131) & ~127;
	return SceSdifForDriver_3C4CDC8B(gBtTransport.function,
		gBtTransport.ioPort, transferLength, gBtTransmitPacket.bytes, 2);
}

int BtIsNewCardRevision(void)
{
	return gBtTransport.cardRevision > 63;
}

void BtWakeTransport(void)
{
	uint32_t events;

	AtomicFullBarrier();
	do {
		events = AtomicLoadReserved32(&gBtTransport.interruptSignal.events);
	} while (AtomicStoreReserved32(&gBtTransport.interruptSignal.events,
		events | 1));
	if (gBtTransport.interruptSignal.waiters)
		ksceKernelSetEventFlag(gBtTransport.interruptSignal.eventFlag, 1);
}

int module_start(SceSize argumentSize, const void *argument)
{
	BtTxRequest *request;
	SceUID thread;
	int result;

	(void)argumentSize;
	(void)argument;
	gBtTransport.interruptSignal.eventFlag = ksceKernelCreateEventFlag(
		"SceBtIntr", 0x9000, 0, NULL);
	gBtTransport.interruptSignal.events = 0;
	gBtTransport.interruptSignal.waiters = 0;
	if (gBtTransport.interruptSignal.eventFlag < 0)
		return 0;
	result = ksceKernelCreateEventFlag("SceBtSync", 0x9000, 0, NULL);
	gBtTransport.lockSignal.waiters = 0;
	gBtTransport.lockSignal.eventFlag = result;
	gBtTransport.lockSignal.events = 1;
	if (result < 0)
		goto deleteInterrupt;
	gBtTransport.lockDepth = 0;
	gBtTransport.freeRequests = gBtTransport.requests;
	request = gBtTransport.requests;
	do {
		request->next = request < &gBtTransport.requests[31] ?
			request + 1 : NULL;
		request++;
	} while (request < &gBtTransport.requests[32]);
	gBtTransport.transmitActive = 0;
	thread = ksceKernelCreateThread("SceBtIntr", BtTransportThread,
		18, 0x2000, 0, 0x80000, NULL);
	if (thread >= 0) {
		result = ksceKernelStartThread(thread, 0, NULL);
		if (result >= 0) {
			gBtTransport.interruptThread = thread;
			return 0;
		}
		thread = result;
	}
	gBtTransport.interruptThread = thread;
	ksceKernelDeleteEventFlag(gBtTransport.lockSignal.eventFlag);
deleteInterrupt:
	ksceKernelDeleteEventFlag(gBtTransport.interruptSignal.eventFlag);
	return 0;
}
