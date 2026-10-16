#include <string.h>

#include "bt_internal.h"

void BtRingReset(BtRingBuffer *buffer)
{
	memset(buffer, 0, sizeof(*buffer));
}

int BtRingWrite(BtRingBuffer *buffer, const uint8_t *source, int length,
		int userMode)
{
	int tail, first, second, remaining, position;

	if (buffer == NULL)
		return 0;
	tail = buffer->capacity - buffer->writePosition;
	first = buffer->capacity - buffer->used;
	if (tail < first) {
		second = first - tail;
		first = tail;
	} else {
		second = 0;
	}
	if (first >= length)
		first = length;
	if (first <= 0)
		return 0;
	if (source != NULL) {
		if (BtCopyIn(source, buffer->data + buffer->writePosition,
			     first, userMode) != 0)
			return (int)0x802f0b09;
		source += first;
	} else {
		memset(buffer->data + buffer->writePosition, 0, first);
	}
	remaining = length - first;
	buffer->used += first;
	position = buffer->writePosition + first;
	if (position >= buffer->capacity)
		position = 0;
	buffer->writePosition = position;
	if (remaining >= second)
		remaining = second;
	if (remaining <= 0)
		return first;
	if (source == NULL) {
		memset(buffer->data + position, 0, remaining);
	} else if (BtCopyIn(source, buffer->data + position,
			    remaining, userMode) != 0) {
		return (int)0x802f0b09;
	}
	buffer->used += remaining;
	buffer->writePosition += remaining;
	return first + remaining;
}

int BtRingRead(BtRingBuffer *buffer, uint8_t *destination, int length,
	       int userMode)
{
	int position, first, tail, second, remaining, used;

	if (buffer == NULL) {
		if (!userMode)
			memset(destination, 0, length);
		return length;
	}
	position = buffer->readPosition;
	first = buffer->used;
	tail = buffer->capacity - position;
	if (tail < first) {
		second = first - tail;
		first = tail;
	} else {
		second = 0;
	}
	if (first >= length)
		first = length;
	if (first <= 0)
		return 0;
	if (BtCopyOut(buffer->data + position, destination, first, userMode) != 0)
		return (int)0x802f0b0a;
	remaining = length - first;
	if (remaining < second)
		second = remaining;
	position = buffer->readPosition + first;
	used = buffer->used - first;
	if (position >= buffer->capacity)
		position = 0;
	buffer->readPosition = position;
	buffer->used = used;
	if (second <= 0)
		return first;
	if (BtCopyOut(buffer->data, destination + first, second, userMode) != 0)
		return (int)0x802f0b0a;
	buffer->readPosition += second;
	buffer->used -= second;
	return first + second;
}

void BtRingRewind(BtRingBuffer *buffer, int length)
{
	int position = buffer->readPosition - length;
	int used = buffer->used + length;

	buffer->readPosition = position;
	buffer->used = used;
	if (position < 0)
		buffer->readPosition = position + buffer->capacity;
}

int BtRingConsume(BtRingBuffer *buffer, int length)
{
	int position = buffer->readPosition + length;
	int used = buffer->used - length;
	int capacity = buffer->capacity;

	buffer->used = used;
	if (capacity <= position)
		position -= capacity;
	buffer->readPosition = position;
	return 0;
}
