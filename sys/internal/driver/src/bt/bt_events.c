/* Bluetooth event delivery; original source filename unavailable. */
#include <string.h>
#include <arm_neon.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include "bt_internal.h"

BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);

uintptr_t BtNotifyHidVuEvent(uint64_t deviceAddress, uint8_t eventId,
			   uint8_t status, const void *data, uint16_t length)
{
	BtHidVuSubscriber *subscriber = &gBtStack.hidVuSubscriber;
	uintptr_t result = deviceAddress;

	if (subscriber->threadId) {
		if (subscriber->count < subscriber->capacity) {
			BtHidVuEvent *event =
				&subscriber->events[subscriber->writePosition];
			int writePosition;

			event->eventId = eventId;
			event->status = status;
			event->deviceAddress = deviceAddress;
			if (data && length) {
				memcpy(event->data, data, length);
				event->length = length;
			} else {
				event->length = 0;
			}
			writePosition = subscriber->writePosition + 1;
			subscriber->count++;
			if (writePosition >= subscriber->capacity)
				writePosition = 0;
			subscriber->writePosition = writePosition;
		}
		result = subscriber->callbackId;
		if (subscriber->callbackId != -1)
			result = ksceKernelNotifyCallback(subscriber->callbackId, 0);
	}
	return result;
}

uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress)
{
	BtEventSubscriber *subscriber = gBtStack.subscribers;
	uint64_t eventMask = vget_lane_u64(vshl_u64(vdup_n_u64(1),
						 vdup_n_s64(eventId)), 0);
	uintptr_t result = eventId;

	do {
		if (gBtStack.mode & 4) {
			subscriber->droppedEvents = 0;
			subscriber->count = 0;
			subscriber->writePosition = 0;
			subscriber->readPosition = 0;
		} else if (subscriber->threadId &&
			   (subscriber->eventMask & eventMask)) {
			if (subscriber->count >= subscriber->capacity) {
				subscriber->droppedEvents++;
			} else {
				SceBtEvent *event =
					&subscriber->events[subscriber->writePosition];
				int writePosition;

				event->id = eventId;
				event->status = status;
				event->mac0 = deviceAddress;
				event->mac1 = deviceAddress >> 32;
				event->argument = argument;
				writePosition = subscriber->writePosition + 1;
				if (writePosition >= subscriber->capacity)
					writePosition = 0;
				subscriber->count++;
				subscriber->writePosition = writePosition;
			}
			result = subscriber->callbackId;
			if (subscriber->callbackId != -1)
				result = ksceKernelNotifyCallback(subscriber->callbackId, 0);
		}
		subscriber++;
	} while (subscriber < gBtStack.subscribers + 8);
	if ((eventId == 5 && status == 0) || eventId == 6) {
		BtConnection *connection =
			BtFindConnection(&gBtStack, 1, deviceAddress);

		result = (uintptr_t)connection;
		if (connection && connection->device &&
		    connection->device->vendorProductId == 0x03290e6f) {
			if (eventId == 5) {
				if (connection->remoteFeatures & 0x4000000)
					connection->hidVuReportCapacity = 78;
				else if (connection->remoteFeatures & 0x2000000)
					connection->hidVuReportCapacity = 49;
				else
					connection->hidVuReportCapacity = 22;
				result = 1;
			} else {
				result = 2;
			}
			if (gBtStack.hidVuSubscriber.threadId)
				result = BtNotifyHidVuEvent(deviceAddress, result,
							  status, NULL, 0);
		}
	}
	return result;
}
