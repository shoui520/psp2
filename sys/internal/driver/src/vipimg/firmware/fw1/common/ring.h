/*
 * fw1 runtime: rings in DRAM shared with the other processors. A ring has one
 * writer and up to three readers, each with a cursor (a DRAM address in the ring).
 * The writer stops 8 bytes short of the slowest reader, so a full ring is never
 * mistaken for an empty one.
 */
#pragma once

#include "runtime.h"

#define VIP_RING_CURSORS	4
#define VIP_RING_WRITER		1	/* VipRingHandle.cursor; readers are 2 to 4 */
#define VIP_RING_GAP		8
#define SEMAPHORE_TRIES		50	/* ring.c: the tries for a ring's semaphore */

typedef struct VipRingCursor {
	int8_t open;
	uint8_t reserved[3];
	uint32_t position;		/* DRAM */
} VipRingCursor;

typedef struct VipRing {
	volatile uint32_t *semaphore;	/* held while a cursor is opened or closed */
	uint32_t start;			/* DRAM */
	uint32_t size;
	uint8_t readerState[2];		/* cleared when the last reader closes */
	uint8_t writerState;		/* cleared when the writer closes */
	uint8_t reserved;
	VipRingCursor cursors[VIP_RING_CURSORS];	/* the writer's, then the readers' */
} VipRing;

/* a processor's use of a ring */
typedef struct VipRingHandle {
	VipRing *ring;
	int32_t cursor;			/* VIP_RING_WRITER, a reader, or -1 */
} VipRingHandle;

/* a part of a ring: a ring's readable or writable data is one or two of these */
typedef struct VipRingSegment {
	uint32_t start;			/* DRAM */
	uint32_t size;
} VipRingSegment;

/* how a handle opens a ring */
#define VIP_RING_OPEN_READER	0
#define VIP_RING_OPEN_WRITER	1

int takeSemaphore(volatile uint32_t *semaphore, uint32_t *interrupts);
void releaseSemaphore(volatile uint32_t *semaphore, uint32_t interrupts);
int openRing(VipRingHandle *handle, uint32_t mode);
int closeRing(VipRingHandle *handle);
int ringReadersClosed(VipRingHandle *handle);
int ringSkipToWriter(VipRingHandle *handle);
int readRing(VipRingHandle *handle, uint32_t local, uint32_t size);
int writeRing(VipRingHandle *handle, uint32_t local, uint32_t size);
int getRingReadable(VipRingHandle *handle, VipRingSegment *segments, uint32_t count);
int getRingAvailable(VipRingHandle *handle);
int getRingWritable(VipRingHandle *handle, VipRingSegment *segments, uint32_t count);
int getRingSpace(VipRingHandle *handle);
int isRingEmpty(VipRingHandle *handle);
int advanceRingReader(VipRingHandle *handle, uint32_t size);
int setRingReader(VipRingHandle *handle, uint32_t position);
int advanceRingWriter(VipRingHandle *handle, uint32_t size);
int setRingWriter(VipRingHandle *handle, uint32_t position);
int getRingDistance(VipRingHandle *handle, uint32_t from, uint32_t to);
int getRing(VipRingHandle *handle, VipRing **ring);
int setRing(VipRingHandle *handle, VipRing *ring);
uint32_t getRingReaderState0(VipRingHandle *handle);
void setRingReaderState0(VipRingHandle *handle, uint32_t state);
uint32_t getRingReaderState1(VipRingHandle *handle);
void setRingReaderState1(VipRingHandle *handle, uint32_t state);
uint32_t getRingWriterState(VipRingHandle *handle);
void setRingWriterState(VipRingHandle *handle, uint32_t state);
