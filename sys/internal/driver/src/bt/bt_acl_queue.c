/* ACL transmit queue; original source filename unavailable. */
#include "bt_internal.h"
#include "bt_transport.h"

void BtSendAclPacket(BtStack *stack, uint8_t *packet, int length);
void BtAclPacketTransmitted(void *argument);

int BtPumpAclQueue(BtStack *stack)
{
	BtRingBuffer *queue = &stack->aclQueue;
	uint16_t queuedLength;
	unsigned int ready;
	int used;

	if (stack->aclLength || !stack->aclPacketLength)
		return 0;
	used = queue->used;
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
			if (queuedLength > stack->aclPacketLength + 4) {
				BtAclPacket *packet = (BtAclPacket *)
					(queue->data + queue->readPosition);

				queuedLength = stack->aclPacketLength + 4;
				BtPack(packet->payloadLengthLe, 2, "2", queuedLength - 4);
			}
			BtRingRewind(queue, 2);
			BtSendAclPacket(stack, queue->data + queue->readPosition + 2,
					queuedLength);
			return 0;
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

void BtSendAclPacket(BtStack *stack, uint8_t *packet, int length)
{
	BtRingBuffer *queue = &stack->aclQueue;
	BtAclPacket *acl = (BtAclPacket *)packet;
	BtConnection *connection;
	unsigned int handle, localHandle;
	int capacity = stack->aclPacketCount;
	int outstanding = stack->outstandingAclPackets;
	uint16_t recordLength;
	uint64_t packets, bytes;

	stack->aclLength = length;
	if (outstanding >= capacity) {
		stack->pendingAclPacket = packet;
		return;
	}
	handle = (acl->handleAndFlagsLe[0] | acl->handleAndFlagsLe[1] << 8) & 0xfff;
	localHandle = stack->localAclHandle;
	stack->pendingAclPacket = NULL;
	if (localHandle != handle) {
		if (!handle)
			goto invalidHandle;
		connection = stack->connections;
		while (connection->aclHandle != handle) {
			++connection;
			if (connection >= stack->connections + 7)
				goto invalidHandle;
		}
		if (connection->flags & 0x80) {
			BtRingRead(queue, (uint8_t *)&recordLength, 2, 0);
			stack->aclLength = BtRingConsume(queue, recordLength & 0x7fff);
			BtPumpAclQueue(stack);
			return;
		}
		packets = connection->statistics.aclTransmitPackets;
		bytes = connection->statistics.aclTransmitBytes;
		connection->outstandingAclPackets++;
		connection->statistics.aclTransmitPackets = packets + 1;
		connection->statistics.aclTransmitBytes = bytes + length;
	}
	stack->outstandingAclPackets = outstanding + 1;
	BtQueueTransmit(2, packet, length, BtAclPacketTransmitted, stack);
	return;

invalidHandle:
	queue->writePosition = 0;
	queue->readPosition = 0;
	queue->used = 0;
	stack->aclLength = 0;
}

void BtCompleteAclPackets(BtStack *stack, unsigned int handle, int packetCount)
{
	BtConnection *connection;
	BtPacketCompletion *completion;
	uint8_t *pending;
	int remaining, processed = packetCount;
	int count, completed, position, nextPosition;

	remaining = (int32_t)((uint32_t)stack->outstandingAclPackets - packetCount);
	if (remaining < 0)
		remaining = 0;
	stack->outstandingAclPackets = remaining;
	if (stack->localAclHandle != handle && handle) {
		connection = stack->connections;
		while (connection->aclHandle != handle) {
			++connection;
			if (connection >= stack->connections + 7)
				goto pendingPacket;
		}
		remaining = (int32_t)((uint32_t)connection->outstandingAclPackets - packetCount);
		if (remaining < 0)
			remaining = 0;
		connection->outstandingAclPackets = remaining;
		count = connection->completionCount;
		while (count > 0 && processed) {
			completion = &connection->completions[connection->completionReadPosition];
			completed = processed < completion->packetCount ?
				processed : completion->packetCount;
			remaining = completion->packetCount - completed;
			completion->packetCount = remaining;
			if (remaining > 0)
				break;
			if (completion->argument)
				BtNotifyPacketCompletion(connection, completion->profile,
						 completion->argument);
			position = connection->completionReadPosition;
			processed = (uint16_t)(processed - completed);
			nextPosition = position + 1;
			count = connection->completionCount - 1;
			connection->completionCount = count;
			connection->completionReadPosition = nextPosition <= 255 ?
				nextPosition : position - 255;
		}
	}
pendingPacket:
	pending = stack->pendingAclPacket;
	if (pending)
		BtSendAclPacket(stack, pending, stack->aclLength);
}

void BtAclPacketTransmitted(void *argument)
{
	BtStack *stack = argument;
	BtRingBuffer *queue = &stack->aclQueue;
	BtConnection *connection;
	int readPosition = queue->readPosition;
	int remaining, handleAndFlags = 0;
	int nextLength, capacity;
	unsigned int handle;

	BtUnpack(queue->data + readPosition, 2, "2", &remaining);
	BtRingConsume(queue, 2);
	remaining = (remaining & 0x7fff) - stack->aclLength;
	if (remaining > 0)
		BtUnpack(queue->data + queue->readPosition, 2, "2", &handleAndFlags);
	stack->aclLength = BtRingConsume(queue, stack->aclLength);
	if (remaining <= 0) {
		BtPumpAclQueue(stack);
		return;
	}
	BtRingRewind(queue, 6);
	capacity = stack->aclPacketLength + 4;
	nextLength = remaining + 4;
	if (nextLength < capacity)
		capacity = nextLength;
	BtPack(queue->data + queue->readPosition, 6, "2 22",
	       nextLength | 0x8000, (handleAndFlags & 0x4fff) | 0x1000,
	       capacity - 4);
	if (!(handleAndFlags & 0x4000)) {
		handle = handleAndFlags & 0xfff;
		if (handle) {
			connection = stack->connections;
			while (connection->aclHandle != handle) {
				++connection;
				if (connection >= stack->connections + 7)
					goto sendPacket;
			}
			if (connection->completionCount > 0)
				connection->completions[connection->completionReadPosition].packetCount++;
		}
	}
sendPacket:
	BtSendAclPacket(stack, queue->data + queue->readPosition + 2, capacity);
}
