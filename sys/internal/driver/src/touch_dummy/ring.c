/* SceTouchDummy: a process's ring of the last 64 samples of one panel.
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

/* flags of readRing */
#define READ_CONSUME	0x1		/* only what this reader has not read yet */
#define READ_OWNED	0x10		/* only reports in the caller's regions */
#define READ_SHARED	0x20		/* and reports nobody owns exclusively */
#define READ_FORCE	0x40		/* keep the force value */
#define READ_RAW_FORCE	0x80		/* keep it as the panel gave it */
#define READ_REPORTS	0x80000000	/* copy the reports at all */

int initializeRing(struct touchRing *ring) /* name unknown */
{
	ring->writeIndex = 0;
	ring->readers[0] = 0;
	ring->readers[1] = 0;
	return 0;
}

int resetRingReader(struct touchRing *ring, int kind) /* name unknown */
{
	if (kind > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	ring->readers[kind] = ring->writeIndex;
	return 0;
}

int readRing(struct touchRing *ring, int kind, SceTouchData *out, unsigned count,
	     int pid, unsigned regions, unsigned flags) /* name unknown */
{
	struct touchSample *sample;
	struct touchOwner *owner;
	SceTouchReport *report;
	SceTouchData *data;
	int index, reader, available, i, owned;
	unsigned j;

	if (!out || kind > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	index = ring->writeIndex;
	reader = ring->readers[kind];
	if (flags & READ_CONSUME) {
		available = index >= reader ? index - reader : index + 64 - reader;
		if (!available)
			return 0;
	} else {
		available = 64;
	}
	if ((unsigned)available >= count)
		available = count;
	for (i = available - 1; i >= 0; i--) {
		index = index ? index - 1 : 63;
		sample = &ring->samples[index];
		data = &out[i];
		data->timeStamp = sample->data.timeStamp;
		data->reportNum = 0;
		data->status = sample->data.status;
		if (!(flags & READ_REPORTS))
			continue;
		for (j = 0; j < sample->data.reportNum; j++) {
			owner = &sample->owners[j];
			owned = owner->pid == pid && ((1 << owner->region) & regions);
			if ((flags & READ_OWNED) && !owned &&
			    (!(flags & READ_SHARED) || (owner->attr & 1)))
				continue;
			report = &data->report[data->reportNum];
			*report = sample->data.report[j];
			if (!(flags & READ_FORCE))
				report->force = 0;
			else if (!(flags & READ_RAW_FORCE))
				report->force = 128;
			if (!(flags & READ_OWNED) || !owned)
				report->info = 0;
			data->reportNum++;
		}
	}
	if (flags & READ_CONSUME)
		ring->readers[kind] = ring->writeIndex;
	return available;
}

struct touchSample *nextRingSample(struct touchRing *ring) /* name unknown */
{
	return &ring->samples[ring->writeIndex];
}

void advanceRing(struct touchRing *ring) /* name unknown */
{
	ring->writeIndex = (ring->writeIndex + 1) % 64;
	if (ring->writeIndex == ring->readers[0])
		ring->readers[0] = (ring->writeIndex + 1) % 64;
	if (ring->writeIndex == ring->readers[1])
		ring->readers[1] = (ring->writeIndex + 1) % 64;
}

struct touchSample *lastRingSample(struct touchRing *ring) /* name unknown */
{
	if (!ring->writeIndex)
		return &ring->samples[63];
	return &ring->samples[ring->writeIndex - 1];
}
