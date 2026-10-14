/*
 * The sample ring of one port: one writer (the sampling timer), up to
 * MOTION_RING_READERS readers, each a process reading that port.
 */
#include <string.h>
#include <sce/motion_dev.h>

int motionRingInit(MotionRing *ring, SceMotionDevData *samples, SceUID *owners,
		   unsigned int capacity)
{
	ring->writeIndex = 0;
	memset(ring->readers, -1, sizeof(ring->readers));
	ring->capacity = capacity;
	ring->samples = samples;
	ring->owners = owners;
	return 0;
}

int motionRingOpenReader(MotionRing *ring)
{
	int i;

	for (i = 0; i < MOTION_RING_READERS; i++)
		if (ring->readers[i] == -1)
			break;
	if (i == MOTION_RING_READERS)
		return MOTION_ERROR_NO_RESOURCE;
	ring->readers[i] = ring->writeIndex;
	return i;
}

int motionRingCloseReader(MotionRing *ring, unsigned int reader)
{
	if (reader >= MOTION_RING_READERS)
		return MOTION_ERROR_NOT_FOUND;
	ring->readers[reader] = -1;
	return 0;
}

/* the sample @offset places after the reader's position */
int motionRingPeek(MotionRing *ring, unsigned int reader, unsigned int offset,
		   SceMotionDevData **sample, SceUID **owner)
{
	unsigned int capacity = ring->capacity;
	unsigned int index;
	int read, write;

	*sample = NULL;
	*owner = NULL;
	if (offset >= capacity)
		return MOTION_ERROR_NOT_FOUND;
	if (reader >= MOTION_RING_READERS)
		return MOTION_ERROR_INVALID_ARG;
	read = ring->readers[reader];
	if (read < 0)
		return MOTION_ERROR_NOT_FOUND;
	write = ring->writeIndex;
	if (write == read)
		return 0;
	if (read >= write)
		write += capacity;
	if (write - read < offset + 1)
		return MOTION_ERROR_NOT_FOUND;
	index = read + offset;
	if (index >= capacity)
		index -= capacity;
	*sample = &ring->samples[index];
	*owner = &ring->owners[index];
	return 0;
}

/* everything written so far has been read */
void motionRingConsume(MotionRing *ring, unsigned int reader)
{
	if (reader < MOTION_RING_READERS && ring->readers[reader] >= 0)
		ring->readers[reader] = ring->writeIndex;
}

int motionRingCount(MotionRing *ring, unsigned int reader)
{
	int read, write;

	if (reader >= MOTION_RING_READERS)
		return MOTION_ERROR_INVALID_ARG;
	read = ring->readers[reader];
	if (read < 0)
		return MOTION_ERROR_INVALID_ARG;
	write = ring->writeIndex;
	if (read > write)
		write += ring->capacity;
	return write - read;
}

void motionRingGetWriteSlot(MotionRing *ring, SceMotionDevData **sample, SceUID **owner)
{
	int write = ring->writeIndex;

	*sample = &ring->samples[write];
	*owner = &ring->owners[write];
}

/* a full ring drops the oldest sample of the readers it catches up with */
void motionRingAdvance(MotionRing *ring)
{
	unsigned int capacity = ring->capacity;
	int write = (ring->writeIndex + 1) % capacity;
	int i;

	ring->writeIndex = write;
	for (i = 0; i < MOTION_RING_READERS; i++)
		if (ring->readers[i] != -1 && ring->readers[i] == write)
			ring->readers[i] = (write + 1) % capacity;
}

int motionRingResetReader(MotionRing *ring, unsigned int reader)
{
	if (reader >= MOTION_RING_READERS)
		return MOTION_ERROR_INVALID_ARG;
	ring->readers[reader] = ring->writeIndex;
	return 0;
}
