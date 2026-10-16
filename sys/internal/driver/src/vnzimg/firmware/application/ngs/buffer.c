/*
 * A voice's channel buffers, built in work memory from a cursor: the header
 * with its channel pointers, then each channel's samples, all aligned to
 * 256. With measure set, only the cursor moves.
 */
#include "ngs.h"

#define ALIGNMENT	256

static NgsBuffer *bufferHeaderCreate(int32_t channels, uint32_t sampleSize, uint32_t samples,
	uint32_t *cursor, uint32_t measure)
{
	uint32_t channelSize = sampleSize * samples;
	NgsBuffer *buffer;

	buffer = (NgsBuffer *)ngsCursorAllocate(cursor,
		channels * sizeof(uint8_t *) + sizeof(NgsBuffer), ALIGNMENT);
	if (measure)
		return buffer;
	buffer->channels = channels;
	buffer->channelSize = channelSize;
	buffer->samples = samples;
	buffer->channelData = buffer->data;
	buffer->reserved3 = 0;
	buffer->flags = 0;
	buffer->reserved = 0;
	memset(buffer, 0, sizeof(buffer->header));
	return buffer;
}

static void bufferChannelsCreate(NgsBuffer *buffer, int32_t channels, uint32_t sampleSize,
	uint32_t samples, uint32_t *cursor, uint32_t measure)
{
	uint32_t size = sampleSize * samples, data;
	int32_t i;

	for (i = 0; i < channels; i++) {
		data = ngsCursorAllocate256(cursor, size);
		if (!measure)
			buffer->channelData[i] = (uint8_t *)data;
	}
}

NgsBuffer *ngsBufferCreate(int32_t channels, uint32_t sampleSize, uint32_t samples,
	uint32_t *cursor, uint32_t measure)
{
	NgsBuffer *buffer = bufferHeaderCreate(channels, sampleSize, samples, cursor, measure);

	bufferChannelsCreate(buffer, channels, sampleSize, samples, cursor, measure);
	return buffer;
}

/* the work memory ngsBufferCreate takes */
uint32_t ngsBufferSize(int32_t channels, uint32_t sampleSize, uint32_t samples)
{
	return ngsRoundUp256(channels * sizeof(uint8_t *) + sizeof(NgsBuffer)) +
		ngsRoundUp256(channels * sampleSize * samples);
}
