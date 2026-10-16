/* HCI command transmission; original source filename unavailable. */
#include <stdarg.h>
#include "bt_internal.h"

int BtVPack(uint8_t *destination, int remaining, const char *format,
	    va_list arguments);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtQueueTransmit(unsigned int packetType, const uint8_t *data, int length,
		    void (*completion)(void *argument), void *argument);
void BtHciCommandTransmitted(void *argument);

void BtHciCommandTransmitted(void *argument)
{
	BtStack *stack = argument;

	stack->commandLength = BtRingConsume(&stack->commandQueue,
					     stack->commandLength);
}

static inline int BtPumpHciCommands(BtStack *stack)
{
	BtRingBuffer *queue = &stack->commandQueue;
	uint16_t queuedLength;
	uint8_t *destination;
	unsigned int packetType, ready;
	int used = queue->used;

	while (used > 1) {
		BtRingRead(queue, (uint8_t *)&queuedLength, 2, 0);
		used = queue->used;
		ready = queuedLength & 0x8000;
		queuedLength &= 0x7fff;
		if (used < queuedLength) {
			BtRingRewind(queue, 2);
			return 0;
		}
		if (ready) {
			destination = queue->data + queue->readPosition;
			stack->commandLength = queuedLength;
			stack->commandBusy = 1;
			packetType = (destination[1] & 0xfc) == 0xfc ? 254 : 1;
			return BtQueueTransmit(packetType, destination, queuedLength,
					       BtHciCommandTransmitted, stack);
		}
		BtRingConsume(queue, queuedLength);
		used = queue->used;
	}
	if (used != 1) {
		queue->writePosition = 0;
		queue->readPosition = 0;
		queue->used = 0;
	}
	return 0;
}

void BtContinueHciCommands(BtStack *stack)
{
	int pendingLength = stack->commandLength;

	stack->commandBusy = 0;
	if (!pendingLength)
		BtPumpHciCommands(stack);
}

int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...)
{
	BtRingBuffer *queue = &stack->commandQueue;
	va_list arguments, payload;
	uint8_t *destination;
	uint16_t recordLength;
	int payloadLength, totalLength, packetLength, position, used;
	int result = 0;

	va_start(arguments, format);
	va_copy(payload, arguments);
	payloadLength = BtVPack(NULL, 0, format, payload);
	totalLength = payloadLength + 5;
	va_copy(payload, arguments);
	if (totalLength > queue->capacity - queue->used) {
		result = (int)0x802f0b02;
		goto done;
	}
	recordLength = queue->capacity - queue->writePosition;
	if (totalLength > recordLength) {
		BtRingWrite(queue, (uint8_t *)&recordLength, 2, 0);
		BtRingWrite(queue, NULL, recordLength, 0);
		if (totalLength > queue->capacity - queue->used) {
			result = (int)0x802f0b02;
			goto done;
		}
	}
	recordLength = (totalLength - 2) | 0x8000;
	BtRingWrite(queue, (uint8_t *)&recordLength, 2, 0);
	destination = queue->data + queue->writePosition;
	BtPack(destination, 3, "21", opcode, payloadLength);
	BtVPack(destination + 3, payloadLength, format, payload);
	packetLength = payloadLength + 3;
	position = queue->writePosition + packetLength;
	if (queue->capacity <= position)
		position -= queue->capacity;
	used = queue->used + packetLength;
	queue->writePosition = position;
	queue->used = used;
	if (stack->commandLength || stack->commandBusy)
		goto done;
	result = BtPumpHciCommands(stack);
done:
	va_end(payload);
	va_end(arguments);
	return result;
}
