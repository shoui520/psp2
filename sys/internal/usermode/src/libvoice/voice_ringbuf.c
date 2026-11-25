/*
 * Ring buffer primitives for SceVoice.
 */

#include "voice_internal.h"

void voiceRingInit(VoiceRingBuffer *ring, void *buffer, uint16_t capacity)
{
	ring->buffer = (uint8_t *)buffer;
	ring->capacity = capacity;
	ring->readIdx = 0;
	ring->readCount = 0;
	ring->writeIdx = 0;
	ring->writeCount = 0;
}

void voiceRingTerm(VoiceRingBuffer *ring)
{
	ring->readIdx = 0;
	ring->buffer = (uint8_t *)(uintptr_t)VOICE_RING_POISON_ADDR;
	ring->readCount = 0;
	ring->writeIdx = 0;
	ring->writeCount = 0;
	ring->capacity = 0;
}

void voiceRingReset(VoiceRingBuffer *ring)
{
	ring->readIdx = 0;
	ring->readCount = 0;
	ring->writeIdx = 0;
	ring->writeCount = 0;
}

uint32_t voiceRingWriteAt(VoiceRingBuffer *ring, uint32_t offset, const void *src, uint32_t size)
{
	const uint8_t *srcBytes = (const uint8_t *)src;
	uint32_t capacity = ring->capacity;
	uint32_t freeBytes = (uint16_t)(capacity - (uint16_t)(ring->writeCount - ring->readCount));
	uint32_t toWrite = (freeBytes >= size) ? size : freeBytes;

	if (offset + toWrite > capacity) {
		uint32_t firstPart = capacity - offset;
		voiceMemcpy(ring->buffer + offset, srcBytes, firstPart);
		voiceMemcpy(ring->buffer, srcBytes + firstPart, toWrite - firstPart);
	} else {
		voiceMemcpy(ring->buffer + offset, srcBytes, toWrite);
	}
	return toWrite;
}

uint32_t voiceRingRead(VoiceRingBuffer *ring, void *dst, uint32_t size)
{
	uint8_t *dstBytes = (uint8_t *)dst;
	uint32_t capacity = ring->capacity;
	uint32_t usedBytes = (uint16_t)(ring->writeCount - ring->readCount);
	uint32_t toRead = (usedBytes >= size) ? size : usedBytes;
	uint32_t readPos = ring->readIdx % capacity;

	if (readPos + toRead > capacity) {
		uint32_t firstPart = capacity - readPos;
		voiceMemcpy(dstBytes, ring->buffer + readPos, firstPart);
		voiceMemcpy(dstBytes + firstPart, ring->buffer, toRead - firstPart);
	} else {
		voiceMemcpy(dstBytes, ring->buffer + readPos, toRead);
	}

	ring->readIdx = (uint16_t)((ring->readIdx + toRead) % ring->capacity);
	ring->readCount = (uint16_t)(ring->readCount + toRead);
	return toRead;
}

uint32_t voiceRingWrite(VoiceRingBuffer *ring, const void *src, uint32_t size)
{
	const uint8_t *srcBytes = (const uint8_t *)src;
	uint32_t capacity = ring->capacity;
	uint32_t freeBytes = (uint16_t)(capacity - (uint16_t)(ring->writeCount - ring->readCount));
	uint32_t toWrite = (freeBytes >= size) ? size : freeBytes;
	uint32_t writePos = ring->writeIdx % capacity;

	if (writePos + toWrite > capacity) {
		uint32_t firstPart = capacity - writePos;
		voiceMemcpy(ring->buffer + writePos, srcBytes, firstPart);
		voiceMemcpy(ring->buffer, srcBytes + firstPart, toWrite - firstPart);
	} else {
		voiceMemcpy(ring->buffer + writePos, srcBytes, toWrite);
	}

	ring->writeIdx = (uint16_t)((ring->writeIdx + toWrite) % ring->capacity);
	ring->writeCount = (uint16_t)(ring->writeCount + toWrite);
	return toWrite;
}
