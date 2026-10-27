/* Bluetooth transport thread; original source filename unavailable. */
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include "bt_transport.h"

int BtTransportThread(SceSize argumentSize, void *argument)
{
	BtTransport *transport = &gBtTransport;
	BtHciEventPacket *event;
	BtTxRequest *request;
	void (*completion)(void *argument);
	void *completionArgument;
	uint32_t mode;
	uint8_t interruptStatus[1], units[1];
	int waitResult, started;
	int receivedLength, transferLength, packetLength, payloadLength;
	unsigned int packetType;

	(void)argumentSize;
	(void)argument;
	if (BtLockTransport() < 0)
		goto exit;
	transport->stack = &gBtStack;
	BtInitializeServices("SceBtLog", "SceBtTimer");
	if (BtReadRegisteredDevices(transport->stack) < 0)
		goto exit;
	for (;;) {
		mode = transport->stack->mode;
		if (mode & 8)
			break;
		transport->stack->mode = mode | 0x100;
		BtUnlockTransport(0);
		waitResult = BtWaitSignal(&transport->interruptSignal, UINT32_MAX);
		if (BtLockTransport() < 0)
			goto exit;
		if (waitResult)
			goto exit;
	}
	transport->stack->mode = (mode & ~0x110U) | 0x10;
	started = BtStartTransport();
	if (started < 0) {
		transport->stack->lastError = started;
		mode = transport->stack->mode | 0x40;
		transport->stack->mode = mode;
		BtNotifyEvent(21, 0, (uint8_t)mode,
			(uint64_t)(uintptr_t)transport->stack);
		goto exit;
	}
	BtInitializeQueues(transport->stack);
	for (;;) {
		BtUnlockTransport(0);
		waitResult = BtWaitSignal(&transport->interruptSignal, UINT32_MAX);
		if (BtLockTransport() < 0)
			goto exit;
		if (waitResult)
			goto exit;
		if (sceSdioRead(transport->function,
			3, 1, interruptStatus, 1) < 0)
			goto exit;
		if (interruptStatus[0] & 2) {
			request = transport->transmitHead;
			if (transport->transmitActive && request) {
				transport->transmitHead = request->next;
				if (transport->transmitHead)
					transport->transmitHead->prev = NULL;
				else
					transport->transmitTail = NULL;
				completion = request->completion;
				completionArgument = request->argument;
				request->next = transport->freeRequests;
				transport->freeRequests = request;
				if (completion)
					completion(completionArgument);
				transport->transmitActive = 0;
				BtPumpTransmit(transport);
			}
		}
		if (!(interruptStatus[0] & 1))
			goto enableInterrupt;
		if (sceSdioRead(transport->function, 98, 1, units, 1) < 0)
			goto enableInterrupt;
		receivedLength = units[0] << transport->receiveUnitShift;
		if ((unsigned int)(receivedLength - 1) >= 2048)
			goto enableInterrupt;
		transferLength = receivedLength;
		if (receivedLength > 128)
			transferLength = (receivedLength + 127) & ~127;
		if (sceSdioRead(transport->function, transport->ioPort,
			transferLength, gBtReceivePacket.bytes, 2) < 0 || receivedLength <= 3)
			goto enableInterrupt;
		packetLength = BtReadLe24(gBtReceivePacket.packet.length);
		if (receivedLength < packetLength)
			goto enableInterrupt;
		payloadLength = packetLength - 4;
		if (payloadLength <= 0)
			goto enableInterrupt;
		packetType = gBtReceivePacket.packet.packetType;
		if (packetType == 3) {
			BtReceiveScoPacket(transport->stack, gBtReceivePacket.packet.payload,
				payloadLength);
		} else if (packetType <= 3) {
			if (packetType == 2)
				BtReceiveAclPacket(transport->stack,
					gBtReceivePacket.packet.payload, payloadLength);
		} else if (packetType == 4 || packetType == 254) {
			event = (BtHciEventPacket *)gBtReceivePacket.packet.payload;
			if (payloadLength > 3 && event->eventCode == 255 &&
			    event->parameterLength > 1 &&
			    ((event->parameters[0] == 32 && event->parameters[1] == 1) ||
			     (event->parameters[0] == 90 && event->parameters[1] == 0))) {
				transport->asleep = 1;
				transport->wakeupPending = 0;
				BtReceiveHciEvent(transport->stack,
					gBtReceivePacket.packet.payload, payloadLength);
				goto cancelTimer;
			}
			BtReceiveHciEvent(transport->stack,
				gBtReceivePacket.packet.payload, payloadLength);
		}
		if (transport->asleep) {
			transport->asleep = 0;
			BtPumpTransmit(transport);
		}
cancelTimer:
		transport->wakeupTimer = BtCancelTimer(transport->wakeupTimer);
enableInterrupt:
		if (transport->interruptEnabled &&
		    sceKernelEnableSubIntr(222, transport->subInterrupt) < 0)
			goto exit;
	}
exit:
	BtUnlockTransport(0);
	sceKernelDisableSubIntr(222, transport->subInterrupt);
	transport->interruptEnabled = 0;
	sceKernelExitThread(0);
	return 0;
}
