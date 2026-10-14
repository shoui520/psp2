/* L2CAP packet transmission; original source filename unavailable. */
#include <stdarg.h>
#include "bt_internal.h"

int BtVPack(uint8_t *destination, int remaining, const char *format,
	    va_list arguments);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtPumpAclQueue(BtStack *stack);

int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...)
{
	BtStack *stack;
	BtRingBuffer *queue;
	uint8_t *destination;
	unsigned int handle, boundaryFlags;
	int payloadLength, recordLength;
	uint16_t queuedLength;
	va_list arguments, payload;
	int result;

	if (connection) {
		handle = connection->aclHandle;
		boundaryFlags = 0x2000;
		stack = connection->stack;
	} else {
		stack = &gBtStack;
		if (stack->localAclHandle & 0xf000) {
			int candidate = 0xeff;

			for (;;) {
				for (connection = stack->connections;
				     connection < stack->connections + 7; connection++) {
					if (connection->deviceAddress &&
					    (connection->aclHandle == candidate ||
					     connection->scoHandle == candidate))
						break;
				}
				if (connection == stack->connections + 7)
					break;
				candidate--;
			}
			stack->localAclHandle = candidate;
		}
		handle = stack->localAclHandle;
		boundaryFlags = 0x6000;
	}
	queue = &stack->aclQueue;
	va_start(arguments, format);
	va_copy(payload, arguments);
	payloadLength = BtVPack(NULL, 0, format, payload);
	va_end(payload);
	recordLength = payloadLength + 10;
	if (recordLength > queue->capacity - queue->used) {
		result = 0x802f0b03;
		goto out;
	}
	queuedLength = queue->capacity - queue->writePosition;
	if (recordLength > queuedLength) {
		BtRingWrite(queue, (uint8_t *)&queuedLength, 2, 0);
		BtRingWrite(queue, NULL, queuedLength, 0);
		if (recordLength > queue->capacity - queue->used) {
			result = 0x802f0b03;
			goto out;
		}
	}
	if (connection) {
		BtPacketCompletion *completion;
		uint32_t profile = 0;
		uintptr_t argument = 0;

		if (connection->completionCount > 254) {
			result = 0x802f0b04;
			goto out;
		}
		if (*format == 'C') {
			va_copy(payload, arguments);
			profile = va_arg(payload, unsigned int);
			argument = va_arg(payload, uintptr_t);
			va_end(payload);
		}
		completion = &connection->completions[connection->completionWritePosition];
		completion->packetCount = 1;
		completion->profile = profile;
		completion->argument = argument;
		connection->completionCount++;
		if (++connection->completionWritePosition > 255)
			connection->completionWritePosition -= 256;
	}
	queuedLength = (recordLength - 2) | 0x8000;
	BtRingWrite(queue, (uint8_t *)&queuedLength, 2, 0);
	destination = queue->data + queue->writePosition;
	BtPack(destination, 8, "22 22", (handle & 0xfff) | boundaryFlags,
	       payloadLength + 4, payloadLength, channelId);
	va_copy(payload, arguments);
	BtVPack(destination + 8, payloadLength, format, payload);
	va_end(payload);
	queue->used += payloadLength + 8;
	queue->writePosition += payloadLength + 8;
	if (queue->writePosition >= queue->capacity)
		queue->writePosition -= queue->capacity;
	result = BtPumpAclQueue(stack);
out:
	va_end(arguments);
	return result;
}
