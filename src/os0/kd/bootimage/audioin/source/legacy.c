/* Legacy 8 kHz capture queue; original source filename unavailable. */
#include "audioin_internal.h"
#include <psp2kern/kernel/sysclib.h>

void initializeAudioInLegacyQueue(void)
{
	g_audioInLegacyQueue.targetSamples = 0;
	g_audioInLegacyQueue.queuedSamples = 0;
	g_audioInLegacyQueue.readCursor = 0;
	g_audioInLegacyQueue.writeCursor = 0;
	g_audioInLegacyQueue.previousSample = 0;
}

int readAudioInLegacyQueue(int16_t *samples, unsigned int count)
{
	int available = g_audioInLegacyQueue.queuedSamples;
	int previous;
	int16_t sample, cursor;
	unsigned int remaining;

	if (available < (int)count) {
		available = ~available;
		goto done;
	}
	previous = g_audioInLegacyQueue.previousSample;
	remaining = count - 1;
	if (count) {
		cursor = g_audioInLegacyQueue.readCursor;
		samples += 2;
		do {
			remaining--;
			samples += 2;
			sample = g_audioInLegacyQueue.samples[cursor];
			samples[-3] = sample;
			samples[-4] = (previous + sample) / 2;
			previous = sample;
			available = --g_audioInLegacyQueue.queuedSamples;
			--g_audioInLegacyQueue.targetSamples;
			cursor = (int16_t)(g_audioInLegacyQueue.readCursor + 1) % 360;
			g_audioInLegacyQueue.readCursor = cursor;
		} while (remaining != ~0u);
	} else {
		sample = previous;
	}
	g_audioInLegacyQueue.previousSample = sample;
done:
	return available;
}

int writeAudioInLegacyQueue(const int16_t *samples, int count)
{
	int available = g_audioInLegacyQueue.queuedSamples;
	int delta, remaining;
	int16_t cursor, queued;
	const int16_t *end;

	g_audioInLegacyQueue.targetSamples += 120;
	delta = g_audioInLegacyQueue.targetSamples - (available + count);
	if (delta > 120) {
		cursor = g_audioInLegacyQueue.writeCursor;
		remaining = delta - 1;
		do {
			remaining--;
			g_audioInLegacyQueue.samples[cursor] = 0;
			cursor = (int16_t)(cursor + 1) % 360;
		} while (remaining != -1);
		g_audioInLegacyQueue.writeCursor = cursor;
		queued = available + delta;
		g_audioInLegacyQueue.queuedSamples = queued;
		available = queued;
	} else {
		if (delta < -120) {
			count = g_audioInLegacyQueue.targetSamples - available;
			if (count < 0)
				count = 0;
		}
		if (count) {
			cursor = g_audioInLegacyQueue.writeCursor;
			queued = g_audioInLegacyQueue.queuedSamples;
			end = samples + count;
			do {
				int16_t previousCursor = g_audioInLegacyQueue.writeCursor;
				int16_t sample = *samples++;

				g_audioInLegacyQueue.queuedSamples = ++queued;
				g_audioInLegacyQueue.samples[cursor] = sample;
				cursor = (int16_t)(previousCursor + 1) % 360;
				g_audioInLegacyQueue.writeCursor = cursor;
			} while (samples != end);
			available = (int16_t)(available + count);
		}
	}
	if (available > 360) {
		queued = available - 120;
		g_audioInLegacyQueue.queuedSamples = queued;
		g_audioInLegacyQueue.readCursor =
			(int16_t)(g_audioInLegacyQueue.readCursor + 120) % 360;
		available = queued;
	}
	return available;
}

void resetAudioInLegacyQueue(void)
{
	memset(g_audioInLegacyQueue.samples, 0, sizeof(g_audioInLegacyQueue.samples));
}
