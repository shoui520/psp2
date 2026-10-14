/*
 * fw2 runtime: rings shared with the other processors (see ring.h). Opening and
 * closing a cursor hold the ring's semaphore; moving data does not, since each
 * cursor has only one owner.
 *
 * Not in FW2: readRing, isRingEmpty, getRing, setRing and getRingReaderState1
 * are in the image, but nothing calls them.
 */
#include "ring.h"
#include "../../fw1/common/dma.h"

#define SEMAPHORE_TRIES		50

/*
 * Take a semaphore shared with the other processors, with interrupts off; -1 if
 * it stays taken. Reading the semaphore takes it when it reads 0.
 */
int takeSemaphore(volatile uint32_t *semaphore, uint32_t *interrupts)
{
	uint32_t tries, pause;

	*interrupts = vipReadControl(VIP_CR_PSW) & VIP_PSW_IEC;
	mep_di();
	for (tries = 0; *semaphore; tries++) {
		if (tries >= SEMAPHORE_TRIES) {
			if (*interrupts)
				mep_ei();
			return -1;
		}
		for (pause = 0; pause < 10; pause++)
			__asm__ volatile ("nop");
	}
	return 0;
}

void releaseSemaphore(volatile uint32_t *semaphore, uint32_t interrupts)
{
	*semaphore = 0;
	if (interrupts)
		mep_ei();
}

/*
 * The reader farthest behind the writer; the ring's start if no reader is open.
 * FW2 still looks at all of fw1's reader cursors, the one it never opens too.
 */
static uint32_t findSlowestReader(const VipRing *ring)
{
	uint32_t writer = ring->cursors[0].position;
	uint32_t slowest = ring->start;
	uint32_t farthest = 0, distance, i;

	for (i = 1; i < VIP_RING_CURSORS; i++) {
		const VipRingCursor *reader = &ring->cursors[i];

		if (!reader->open)
			continue;
		if (writer < reader->position)
			distance = ring->size - (reader->position - writer);
		else
			distance = writer - reader->position;
		if (distance < farthest)
			continue;
		farthest = distance;
		slowest = reader->position;
	}
	return slowest;
}

/*
 * The handle's cursor. FW2 dropped fw1's range check (reader is unused), so the
 * callers' checks for none never fire.
 */
static VipRingCursor *getCursor(VipRingHandle *handle, uint32_t reader)
{
	(void)reader;
	return &handle->ring->cursors[handle->cursor - 1];
}

/*
 * Take the ring's writer cursor, which resets every cursor to the start, or a
 * free reader cursor, at the start. -2 if the handle already has an open cursor.
 */
int openRing(VipRingHandle *handle, uint32_t mode)
{
	VipRing *ring = handle->ring;
	VipRingCursor *cursor;
	uint32_t interrupts = 0, start, i;
	int result = 0;

	if (takeSemaphore(ring->semaphore, &interrupts))
		return -1;
	start = ring->start;
	cursor = getCursor(handle, mode != VIP_RING_OPEN_WRITER);
	if (cursor && cursor->open) {
		result = -2;
		goto release;
	}
	if (mode == VIP_RING_OPEN_WRITER) {
		if (ring->cursors[0].open)
			goto fail;
		for (i = 0; i <= VIP_RING_READERS; i++)
			ring->cursors[i].position = start;
		ring->cursors[0].open = 1;
		handle->cursor = VIP_RING_WRITER;
		goto release;
	}
	if (mode == VIP_RING_OPEN_READER) {
		for (i = 1; i <= VIP_RING_READERS; i++) {
			if (ring->cursors[i].open)
				continue;
			ring->cursors[i].position = start;
			ring->cursors[i].open = 1;
			handle->cursor = i + 1;
			goto release;
		}
	}
fail:
	result = -1;
	handle->cursor = -1;
release:
	releaseSemaphore(ring->semaphore, interrupts);
	return result;
}

/* Give up the handle's cursor; -2 if it had none open. */
int closeRing(VipRingHandle *handle)
{
	VipRing *ring = handle->ring;
	VipRingCursor *cursor;
	uint32_t interrupts = 0;
	int result = -2;

	if (takeSemaphore(ring->semaphore, &interrupts))
		return -1;
	cursor = getCursor(handle, 0);
	if (cursor) {
		if (cursor->open) {
			result = 0;
			cursor->open = 0;
		}
		if (handle->cursor == VIP_RING_WRITER) {
			ring->writerState = 0;
		} else if (!ring->cursors[1].open && !ring->cursors[2].open) {
			ring->readerState[1] = 0;
			ring->readerState[0] = 0;
		}
	}
	handle->cursor = -1;
	releaseSemaphore(ring->semaphore, interrupts);
	return result;
}

/* Drop what the reader has not read; the writer's state is cleared too. */
int ringSkipToWriter(VipRingHandle *handle)
{
	VipRing *ring = handle->ring;
	VipRingCursor *cursor = getCursor(handle, 1);

	if (!cursor)
		return -1;
	cursor->position = ring->cursors[0].position;
	ring->writerState = 0;
	return 0;
}

/*
 * Write up to size bytes from local RAM into the ring, by DMA, as far as the
 * first reader allows, and advance the writer; the number of bytes written.
 */
int writeRing(VipRingHandle *handle, uint32_t local, uint32_t size)
{
	volatile uint32_t done = 0;
	VipRing *ring = handle->ring;
	VipRingCursor *writer = &ring->cursors[0];
	uint32_t start, ringSize, writeOffset, limitOffset, wrapLimit, control;
	uint32_t transferred = 0, last = 0;

	limitOffset = ring->cursors[VIP_RING_FIRST_READER].position;
	writeOffset = writer->position;
	start = ring->start;
	ringSize = ring->size;
	writeOffset -= start;
	limitOffset -= start;
	wrapLimit = ringSize;
	if (limitOffset < VIP_RING_GAP)
		wrapLimit -= VIP_RING_GAP - limitOffset;
	control = VIP_DMA_WRITE;
	if (local & VIP_LOCAL_BANK1)
		control |= VIP_DMA_BANK1;
	for (;;) {
		uint32_t end = writeOffset + size;
		uint32_t external = start + writeOffset;
		uint32_t length, localLength;

		if (writeOffset < limitOffset) {
			if (end < limitOffset - VIP_RING_GAP) {
				length = size;
				size = 0;
				writeOffset = end;
			} else {
				length = limitOffset - VIP_RING_GAP - writeOffset;
				size = end - (limitOffset - VIP_RING_GAP);
				writeOffset = limitOffset - VIP_RING_GAP;
			}
			last = 1;
		} else if (end <= wrapLimit) {
			length = size;
			size = 0;
			writeOffset = end;
			last = 1;
		} else {
			length = wrapLimit - writeOffset;
			size = end - wrapLimit;
			writeOffset = wrapLimit;
			if (wrapLimit != ringSize)
				last = 1;
			else
				writeOffset = 0;
		}
		if ((int32_t)length < 1)
			break;
		localLength = (local + transferred) << 16 | length;
		transferred += length;
		if (last) {
			queueDma(control, external, localLength, &done);
			break;
		}
		queueDma(control, external, localLength, NULL);
	}
	if (writeOffset == ringSize)
		writeOffset = 0;
	while (done == 1)
		;
	writer->position = start + writeOffset;
	return transferred;
}

/* Describe what the reader can read: up to count segments; their number. */
int getRingReadable(VipRingHandle *handle, VipRingSegment *segments, uint32_t count)
{
	const VipRing *ring = handle->ring;
	VipRingCursor *cursor = getCursor(handle, 1);
	uint32_t start, readOffset, writeOffset, n = 0, i;

	if (!cursor)
		return -1;
	readOffset = cursor->position;
	writeOffset = ring->cursors[0].open ? ring->cursors[0].position : readOffset;
	start = ring->start;
	readOffset -= start;
	writeOffset -= start;
	for (i = 0; i < count; i++) {
		VipRingSegment *segment = &segments[n++];

		segment->start = start + readOffset;
		if (writeOffset >= readOffset) {
			segment->size = writeOffset - readOffset;
			break;
		}
		segment->size = ring->size - readOffset;
		if (!writeOffset)
			break;
		readOffset = 0;
	}
	return n;
}

/* The bytes the reader can read; -1 if the handle has no cursor. */
int getRingAvailable(VipRingHandle *handle)
{
	const VipRing *ring = handle->ring;
	const VipRingCursor *cursor = getCursor(handle, 1);
	uint32_t writer;

	if (!cursor)
		return -1;
	writer = ring->cursors[0].position;
	if (writer < cursor->position)
		return ring->size - (cursor->position - writer);
	return writer - cursor->position;
}

/*
 * Describe where the writer can write up to limit, a reader's cursor: up to
 * count segments; their number, 0 if limit is 0.
 */
static int getRingWritableBefore(const VipRing *ring, uint32_t limit,
	VipRingSegment *segments, uint32_t count)
{
	uint32_t start, ringSize, writeOffset, limitOffset, n = 0, i;

	writeOffset = ring->cursors[0].position;
	if (!limit)
		return 0;
	start = ring->start;
	ringSize = ring->size;
	writeOffset -= start;
	limitOffset = limit - start;
	for (i = 0; i < count; i++) {
		VipRingSegment *segment = &segments[n++];
		uint32_t size;

		segment->start = start + writeOffset;
		if (writeOffset < limitOffset) {
			segment->size = limitOffset - writeOffset - VIP_RING_GAP;
			break;
		}
		size = ringSize - writeOffset;
		if (limitOffset < VIP_RING_GAP)
			size -= VIP_RING_GAP - limitOffset;
		segment->size = size;
		if (limitOffset <= VIP_RING_GAP)
			break;
		writeOffset = 0;
	}
	return n;
}

/* where the writer can write before the slowest reader */
int getRingWritable(VipRingHandle *handle, VipRingSegment *segments, uint32_t count)
{
	const VipRing *ring = handle->ring;

	return getRingWritableBefore(ring, findSlowestReader(ring), segments, count);
}

/* where the writer can write before the first reader, as writeRing does */
int getRingWritableBeforeFirstReader(VipRingHandle *handle, VipRingSegment *segments,
	uint32_t count)
{
	const VipRing *ring = handle->ring;

	return getRingWritableBefore(ring, ring->cursors[VIP_RING_FIRST_READER].position,
		segments, count);
}

/* the room from the writer up to limit, a cursor */
static int getRingSpaceBefore(const VipRing *ring, uint32_t limit)
{
	uint32_t writeOffset = ring->cursors[0].position - ring->start;
	uint32_t limitOffset = limit - ring->start;

	if (writeOffset < limitOffset)
		return limitOffset - writeOffset - VIP_RING_GAP;
	return ring->size - writeOffset + limitOffset - VIP_RING_GAP;
}

/*
 * The room the writer has: before the slowest reader for the writer's handle,
 * before this reader for a reader's.
 */
int getRingSpace(VipRingHandle *handle)
{
	const VipRing *ring = handle->ring;
	uint32_t limit = 0;

	if (handle->cursor == VIP_RING_WRITER) {
		limit = findSlowestReader(ring);
	} else {
		const VipRingCursor *cursor = getCursor(handle, 1);

		if (cursor)
			limit = cursor->position;
	}
	return getRingSpaceBefore(ring, limit);
}

/* the room the writer has before the first reader */
int getRingSpaceBeforeFirstReader(VipRingHandle *handle)
{
	const VipRing *ring = handle->ring;

	return getRingSpaceBefore(ring, ring->cursors[VIP_RING_FIRST_READER].position);
}

/* the room the writer has before this reader */
int getRingSpaceBeforeReader(VipRingHandle *handle)
{
	const VipRingCursor *cursor = getCursor(handle, 1);

	return getRingSpaceBefore(handle->ring, cursor->position);
}

/* whether space, the writer's room, means the ring is empty */
int isRingSpaceEmpty(VipRingHandle *handle, int space)
{
	return handle->ring->size == (uint32_t)space + VIP_RING_GAP;
}

int advanceRingReader(VipRingHandle *handle, uint32_t size)
{
	const VipRing *ring = handle->ring;
	VipRingCursor *cursor = getCursor(handle, 1);
	uint32_t offset;

	if (!cursor)
		return -1;
	offset = cursor->position - ring->start + size;
	if (offset >= ring->size)
		offset -= ring->size;
	cursor->position = ring->start + offset;
	return 0;
}

int setRingReader(VipRingHandle *handle, uint32_t position)
{
	VipRingCursor *cursor = getCursor(handle, 1);

	if (!cursor)
		return -1;
	cursor->position = position;
	return 0;
}

int advanceRingWriter(VipRingHandle *handle, uint32_t size)
{
	VipRing *ring = handle->ring;
	uint32_t offset;

	offset = ring->cursors[0].position - ring->start + size;
	if (offset >= ring->size)
		offset -= ring->size;
	ring->cursors[0].position = ring->start + offset;
	return 0;
}

/* Move the writer; its state is cleared. */
int setRingWriter(VipRingHandle *handle, uint32_t position)
{
	VipRing *ring = handle->ring;

	ring->cursors[0].position = position;
	ring->writerState = 0;
	return 0;
}

/* How far to is ahead of from in the ring; -1 if from is not in it. */
int getRingDistance(VipRingHandle *handle, uint32_t from, uint32_t to)
{
	const VipRing *ring = handle->ring;
	uint32_t end = ring->start + ring->size;

	if (from < ring->start || from >= end)
		return -1;
	if (to < ring->start && to >= end)
		return -1;
	if (to < from)
		return ring->size - (from - to);
	return to - from;
}

/* the states the processors keep in the ring, a byte each */
uint32_t getRingReaderState0(VipRingHandle *handle)
{
	return handle->ring->readerState[0];
}

void setRingReaderState0(VipRingHandle *handle, uint32_t state)
{
	handle->ring->readerState[0] = state;
}

void setRingReaderState1(VipRingHandle *handle, uint32_t state)
{
	handle->ring->readerState[1] = state;
}

uint32_t getRingWriterState(VipRingHandle *handle)
{
	return handle->ring->writerState;
}

void setRingWriterState(VipRingHandle *handle, uint32_t state)
{
	handle->ring->writerState = state;
}

uint32_t getRingWriterPosition(VipRingHandle *handle)
{
	return handle->ring->cursors[0].position;
}
