/*
 * The AT9 player (SCE_NGS_AT9_ID): ATRAC9 superframes from up to four user
 * buffers, looped and chained like the PCM player's, decoded by the VLIW
 * decoder and resampled to the system rate. A RIFF header met in the data
 * is skipped; the samples each buffer asks to discard at its start and end
 * are dropped. Decoded samples beyond what a block needs wait in the
 * overflow buffer for the next one.
 */
#include "ngs.h"

#define SCE_NGS_AT9_MAX_BUFFERS		4
#define SCE_NGS_AT9_LOOP_CONTINUOUS	(-1)
#define SCE_NGS_AT9_NO_NEXT_BUFFER	(-1)
#define SCE_NGS_AT9_MAX_PCM_CHANNELS	2
#define SCE_NGS_AT9_NULL_CHANNEL	(-1)
#define SCE_NGS_AT9_END_OF_DATA		0
#define SCE_NGS_AT9_SWAPPED_BUFFER	1
#define SCE_NGS_AT9_HEADER_ERROR	2
#define SCE_NGS_AT9_DECODE_ERROR	3
#define SCE_NGS_AT9_LOOPED_BUFFER	4

#define MAX_PLAYBACK_FREQUENCY		192000.0f
#define RATE_ONE			0x8000
/* the passes over the buffer chain a read or a move may take */
#define MAX_PASSES			51
/* RIFF chunks looked at before the data, and the largest skipped */
#define MAX_CHUNKS			10
#define MAX_CHUNK_SIZE			512
#define OVERFLOW_SIZE			1056
#define DECODER_WORK_SIZE		544
/* in work memory: the superframe read, after the samples */
#define SUPERFRAME_OFFSET		8192
#define DECODED_OFFSET			9216

/* SceNgsAT9BufferParams */
typedef struct NgsAt9Buffer {
	uint32_t pBuffer;
	int32_t nNumBytes;
	int16_t nLoopCount;
	int16_t nNextBuff;
	int16_t nSamplesDiscardStart;
	int16_t nSamplesDiscardEnd;
} NgsAt9Buffer;

/* SceNgsAT9Params */
typedef struct NgsAt9UserParams {
	SceNgsParamsDescriptor desc;
	NgsAt9Buffer buffs[SCE_NGS_AT9_MAX_BUFFERS];
	float fPlaybackFrequency;
	float fPlaybackScalar;
	int32_t nLeadInSamples;
	int32_t nLimitNumberOfSamplesPlayed;
	int8_t nChannels;
	int8_t nChannelMap[SCE_NGS_AT9_MAX_PCM_CHANNELS];
	int8_t reserved;
	uint32_t configData;
} NgsAt9UserParams;

/* SceNgsAT9States */
typedef struct NgsAt9States {
	int32_t nCurrentBytePositionInBuffer;
	int32_t nCurrentBuffer;
	int32_t nSamplesGeneratedSinceKeyOn;
	int32_t nBytesConsumedSinceKeyOn;
	int32_t nSamplesGeneratedTotal;
	int32_t nBytesConsumedTotal;
} NgsAt9States;

typedef struct NgsAt9Settings {
	NgsAt9UserParams user;
	NgsUserWindow windows[SCE_NGS_AT9_MAX_BUFFERS][2];
	NgsUserWindow extraWindows[2];
	int32_t rate;			/* 1.15 */
	int8_t currentBuffer;
	uint8_t reserved[3];
	int32_t bytePosition;
	int16_t loops;
	int16_t enabled;
	uint32_t fraction;
	int16_t history[SCE_NGS_AT9_MAX_PCM_CHANNELS];
	int32_t leadIn;
	uint8_t decoder[96];
	int32_t ended;
	int16_t overflowBytes;
	int16_t overflowUsed;
	int32_t superframeBytes;	/* 0 until the decoder is opened */
	int32_t frame;			/* in the superframe */
	int32_t framesPerSuperframe;
	int32_t limit;
	uint32_t overflowSize;
	int32_t frameBytes;		/* set with superframeBytes; not read here */
	int32_t frameSamples;
	uint8_t reserved2[8];
	uint8_t overflow[OVERFLOW_SIZE];
	uint8_t decoderWork[2][DECODER_WORK_SIZE];
} NgsAt9Settings;

int32_t ngsAt9Init(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsAt9UserParams *user = (NgsAt9UserParams *)params->userParams[0];
	NgsAt9Settings *settings = (NgsAt9Settings *)params->data[0];
	NgsAt9States *states = (NgsAt9States *)params->data[1];
	float frequency;
	int32_t channel;

	memcpy(&settings->user, user, sizeof(settings->user));
	settings->enabled = 1;
	frequency = settings->user.fPlaybackScalar * settings->user.fPlaybackFrequency;
	if (!(frequency <= MAX_PLAYBACK_FREQUENCY))
		frequency = MAX_PLAYBACK_FREQUENCY;
	settings->rate = (int32_t)(frequency / (float)(int32_t)params->sampleRate * 32768.0f);
	for (channel = 0; channel < SCE_NGS_AT9_MAX_PCM_CHANNELS; channel++) {
		int8_t map = settings->user.nChannelMap[channel];

		if (map != 0 && map != 1 && map != SCE_NGS_AT9_NULL_CHANNEL)
			settings->user.nChannelMap[channel] = SCE_NGS_AT9_NULL_CHANNEL;
	}
	for (channel = 0; channel < SCE_NGS_AT9_MAX_PCM_CHANNELS; channel++)
		if (settings->user.nChannelMap[channel] == 1 && settings->user.nChannels == 1)
			settings->user.nChannelMap[channel] = SCE_NGS_AT9_NULL_CHANNEL;
	settings->overflowSize = OVERFLOW_SIZE;
	settings->limit = user->nLimitNumberOfSamplesPlayed;

	if (!(params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE))
		return 0;
	settings->superframeBytes = 0;
	settings->frameBytes = 0;
	settings->currentBuffer = 0;
	settings->bytePosition = 0;
	settings->loops = 0;
	settings->fraction = 0;
	for (channel = 0; channel < SCE_NGS_AT9_MAX_PCM_CHANNELS; channel++)
		settings->history[channel] = 0;
	settings->ended = 0;
	settings->leadIn = user->nLeadInSamples;
	states->nCurrentBytePositionInBuffer = 0;
	states->nCurrentBuffer = 0;
	states->nBytesConsumedSinceKeyOn = 0;
	states->nSamplesGeneratedSinceKeyOn = 0;
	settings->overflowUsed = 0;
	settings->overflowBytes = 0;
	return 0;
}

static int32_t isRiff(const char *chunk)
{
	return chunk[0] == 'R' && chunk[1] == 'I' && chunk[2] == 'F' && chunk[3] == 'F';
}

static void at9Read(uint8_t *destination, NgsAt9Settings *settings, int32_t count,
	NgsModuleParams *params);
static int32_t at9Advance(NgsAt9Settings *settings, int32_t bytes, NgsModuleParams *params);

/* past a RIFF header to its data chunk */
static void at9SkipHeader(NgsAt9Settings *settings, NgsModuleParams *params)
{
	struct {
		char id[4];
		int32_t size;
	} chunk;
	int32_t i;

	for (i = 0; i < MAX_CHUNKS; i++) {
		at9Read((uint8_t *)&chunk, settings, sizeof(chunk), params);
		if (isRiff(chunk.id)) {
			at9Advance(settings, 12, params);
			continue;
		}
		if (chunk.id[0] == 'd' && chunk.id[1] == 'a' && chunk.id[2] == 't' &&
			chunk.id[3] == 'a') {
			at9Advance(settings, sizeof(chunk), params);
			break;
		}
		if (chunk.size < 0)
			chunk.size = 0;
		if (chunk.size > MAX_CHUNK_SIZE)
			chunk.size = MAX_CHUNK_SIZE;
		at9Advance(settings, chunk.size + sizeof(chunk), params);
	}
	settings->frame = 0;
}

/* the decoder for the user's configuration */
static int32_t at9Open(NgsAt9Settings *settings)
{
	uint8_t *work[2] = { settings->decoderWork[0], settings->decoderWork[1] };
	int32_t rateIndex = 7, channelConfig = 0;
	uint32_t superframeBytes = 0, frames = 0;

	if (ngsAt9DecoderInit(settings->decoder, settings->user.nChannels, work,
		&settings->user.configData, 0) < 0)
		return -1;
	ngsAt9DecoderReset(settings->decoder);
	ngsAt9ConfigInfo(&settings->user.configData, &rateIndex, &channelConfig, &superframeBytes,
		&frames);
	if (rateIndex == 7)
		settings->frameSamples = 256;
	else if (rateIndex == 4)
		settings->frameSamples = 128;
	else if (rateIndex == 1)
		settings->frameSamples = 64;
	else
		return -1;
	settings->framesPerSuperframe = frames;
	settings->frame = 0;
	settings->superframeBytes = superframeBytes;
	settings->frameBytes = superframeBytes;
	return 0;
}

static int32_t at9Run(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);

int32_t ngsAt9Process(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsAt9Settings *settings = (NgsAt9Settings *)params->data[0];
	int32_t output, channel;

	if (params->flags & NGS_RUN_BYPASSED) {
		for (output = 0; output < outputs->count; output++) {
			for (channel = 0; channel < outputs->buffers[output]->channels; channel++)
				memset(outputs->buffers[output]->channelData[channel], 0,
					outputs->buffers[output]->samples * NGS_SAMPLE_SIZE);
			/* the original marks the first output each time */
			outputs->buffers[0]->flags |= NGS_BUFFER_VALID;
		}
		return 0;
	}
	if (settings->enabled == 0) {
		params->flags &= ~NGS_RUN_ENABLED;
		return 0;
	}
	if (settings->superframeBytes == 0 && at9Open(settings) != 0) {
		params->callbackRequested = 1;
		params->callbackData = SCE_NGS_AT9_HEADER_ERROR;
		params->callbackData2 = 0;
		params->callbackPtr = 0;
		return 0;
	}
	if (settings->superframeBytes != 0)
		return at9Run(inputs, outputs, params);
	return 0;
}

static uint32_t windowMap(NgsAt9Settings *settings, uint32_t address, int32_t buffer,
	int32_t *left, NgsModuleParams *params)
{
	NgsUserWindow *window;
	uint32_t part, i;

	for (part = 0; part < 2; part++) {
		for (i = 0; i < SCE_NGS_AT9_MAX_BUFFERS; i++) {
			window = &settings->windows[i][part];
			if (address < window->address || address >= window->address + window->size)
				continue;
			*left = window->address + window->size - address;
			return address - window->address + window->mapped;
		}
	}
	for (i = 0; i < 2; i++) {
		window = &settings->extraWindows[i];
		if (address < window->address || address >= window->address + window->size)
			continue;
		*left = window->address + window->size - address;
		return address - window->address + window->mapped;
	}
	return 0;
}

static void purgeUser(uint32_t address, int32_t size)
{
	uint32_t start = address & ~255;
	int32_t length = (address + size - start + 255) & ~255;

	if (length >= 1)
		cachePurgeInvalidate(start, length);
}

/* the next count bytes, without moving; silence after the data ends */
static void at9Read(uint8_t *destination, NgsAt9Settings *settings, int32_t count,
	NgsModuleParams *params)
{
	int32_t position = settings->bytePosition, buffer = settings->currentBuffer;
	int32_t loops = settings->loops, copied = 0, remaining = count, passes = 0;
	int32_t left, length, next;
	NgsAt9Buffer *current;
	uint32_t mapped;

	if (remaining == 0)
		return;
	do {
		current = &settings->user.buffs[buffer];
		if (current->nNumBytes < position)
			position = current->nNumBytes;
		left = remaining;
		length = current->nNumBytes - position;
		mapped = windowMap(settings, current->pBuffer + position, buffer, &left, params);
		if (mapped == 0)
			return;
		length = min(min(length, remaining), left);
		purgeUser(mapped, length);
		memcpy(destination + copied, (void *)mapped, length);
		position += length;
		copied += length;
		remaining -= length;

		if (position >= current->nNumBytes) {
			position = 0;
			if (loops >= current->nLoopCount &&
				current->nLoopCount != SCE_NGS_AT9_LOOP_CONTINUOUS) {
				next = current->nNextBuff;
				if (next < 0 || next >= SCE_NGS_AT9_MAX_BUFFERS ||
					settings->user.buffs[next].pBuffer == 0 ||
					settings->user.buffs[next].nNumBytes == 0) {
					memset(destination + copied, 0, count - copied);
					return;
				}
				loops = 0;
				buffer = next;
			} else {
				loops++;
				if (current->nNumBytes < 1) {
					memset(destination + copied, 0, count - copied);
					return;
				}
			}
		}
		if (passes++ >= MAX_PASSES)
			return;
	} while (remaining);
}

/* move on by bytes, a frame at a time; 1 at the end of the data */
static int32_t at9Advance(NgsAt9Settings *settings, int32_t bytes, NgsModuleParams *params)
{
	NgsAt9States *states = (NgsAt9States *)params->data[1];
	int32_t passes = 0, length, next;
	NgsAt9Buffer *current;

	if (++settings->frame == settings->framesPerSuperframe)
		settings->frame = 0;
	current = &settings->user.buffs[settings->currentBuffer];
	if (current->nNumBytes < settings->bytePosition)
		settings->bytePosition = current->nNumBytes;
	if (bytes == 0)
		return 0;

	do {
		current = &settings->user.buffs[settings->currentBuffer];
		length = min(current->nNumBytes - settings->bytePosition, bytes);
		bytes -= length;
		settings->bytePosition += length;
		states->nBytesConsumedSinceKeyOn += length;
		states->nBytesConsumedTotal += length;
		current = &settings->user.buffs[settings->currentBuffer];
		if (settings->bytePosition < current->nNumBytes)
			continue;

		settings->frame = 0;
		if (settings->loops >= current->nLoopCount &&
			current->nLoopCount != SCE_NGS_AT9_LOOP_CONTINUOUS) {
			settings->bytePosition = 0;
			settings->loops = 0;
			next = current->nNextBuff;
			if (next == SCE_NGS_AT9_NO_NEXT_BUFFER ||
				settings->user.buffs[next].pBuffer == 0 ||
				settings->user.buffs[next].nNumBytes == 0)
				goto end;
			params->callbackData = SCE_NGS_AT9_SWAPPED_BUFFER;
			params->callbackRequested = 1;
			params->callbackPtr = settings->user.buffs[next].pBuffer;
			params->callbackData2 = settings->currentBuffer;
			settings->currentBuffer = next;
		} else {
			settings->bytePosition = 0;
			settings->loops++;
			if (current->nNumBytes < 1)
				goto end;
			params->callbackRequested = 1;
			params->callbackData = SCE_NGS_AT9_LOOPED_BUFFER;
			params->callbackPtr = current->pBuffer;
			params->callbackData2 = settings->loops;
		}
	} while (passes++ < MAX_PASSES && bytes);
	return 0;

end:
	params->callbackRequested = 1;
	params->callbackData = SCE_NGS_AT9_END_OF_DATA;
	params->callbackPtr = 0;
	params->callbackData2 = 0;
	return 1;
}

/* the samples of this superframe's frame to drop at its start and its end */
static void at9Discards(int32_t *start, int32_t *end, NgsAt9Settings *settings)
{
	NgsAt9Buffer *current = &settings->user.buffs[settings->currentBuffer];
	int32_t remaining;

	if (current->nNumBytes < settings->bytePosition)
		settings->bytePosition = current->nNumBytes;
	current = &settings->user.buffs[settings->currentBuffer];
	remaining = current->nNumBytes - settings->bytePosition;
	*start = 0;
	*end = 0;

	if (settings->framesPerSuperframe == 1) {
		*start = current->nSamplesDiscardStart;
		*start = current->nSamplesDiscardStart - settings->frameSamples *
			(settings->bytePosition / settings->superframeBytes);
		*end = current->nSamplesDiscardEnd -
			(remaining / settings->superframeBytes - 1) * settings->frameSamples;
		*start = max(*start, 0);
		if (*end < 0)
			*end = 0;
		return;
	}

	/* four frames to a superframe: the first two superframes and the last two */
	if (settings->bytePosition < settings->superframeBytes) {
		*start = current->nSamplesDiscardStart - settings->frameSamples * settings->frame;
		if (*start < 0)
			*start = 0;
	} else if (settings->bytePosition < settings->superframeBytes * 2) {
		*start = current->nSamplesDiscardStart -
			(settings->frame + 4) * settings->frameSamples;
		if (*start < 0)
			*start = 0;
	}
	if (settings->superframeBytes >= remaining) {
		*end = current->nSamplesDiscardEnd - (3 - settings->frame) * settings->frameSamples;
		if (*end < 0)
			*end = 0;
	} else if (settings->superframeBytes * 2 >= remaining) {
		*end = current->nSamplesDiscardEnd - (7 - settings->frame) * settings->frameSamples;
		if (*end < 0)
			*end = 0;
	}
}

/* decoded bytes not needed now, kept when the overflow buffer is empty */
static void overflowKeep(const uint8_t *source, uint32_t bytes, NgsAt9Settings *settings,
	uint8_t *overflow)
{
	if (settings->overflowBytes != settings->overflowUsed)
		return;
	if (settings->overflowSize < bytes)
		return;
	settings->overflowBytes = bytes;
	settings->overflowUsed = 0;
	memcpy(overflow, source, bytes);
}

/* up to bytes from the overflow buffer; taken, or only looked at */
static int32_t overflowTake(uint8_t *destination, int32_t bytes, NgsAt9Settings *settings,
	uint8_t *overflow, int32_t take)
{
	int32_t length = min(settings->overflowBytes - settings->overflowUsed, bytes);

	memcpy(destination, overflow + settings->overflowUsed, length);
	if (take)
		settings->overflowUsed += length;
	return length;
}

/* one frame of samples (all channels), less its discards; -1 on an error */
static int32_t at9DecodeFrame(int16_t *destination, NgsAt9Settings *settings,
	NgsModuleParams *params, uint8_t *superframe, int32_t channels)
{
	int16_t *decoded = (int16_t *)((uint8_t *)params->scratch + settings->frameSamples +
		DECODED_OFFSET);
	int32_t used = 0, start, end, skip, result;

	if (settings->ended == 1) {
		memset(destination, 0, settings->frameSamples * 2 * channels);
		return settings->frameSamples * channels;
	}
	at9Read(superframe, settings, settings->superframeBytes, params);
	at9Discards(&start, &end, settings);
	skip = start >= settings->frameSamples * 2;
	if (settings->frameSamples < start)
		start = settings->frameSamples;
	if (settings->frameSamples < end)
		end = settings->frameSamples;
	if (settings->frameSamples < start + end) {
		end = max(settings->frameSamples - start, 0);
		if (settings->frameSamples < end)
			end = settings->frameSamples;
	}

	if (isRiff((const char *)superframe)) {
		at9SkipHeader(settings, params);
		ngsAt9DecoderReset(settings->decoder);
		return 0;
	}
	if (skip) {
		result = ngsAt9Skip(settings->decoder, superframe, &used);
	} else {
		result = ngsAt9Decode(settings->decoder, superframe, &used, decoded);
		memcpy(destination, decoded + channels * start,
			(settings->frameSamples - start - end) * channels * 2);
	}
	if (result != 0)
		return -1;
	if (at9Advance(settings, used, params))
		settings->ended = 1;
	return (settings->frameSamples - start - end) * channels;
}

static void at9Failed(NgsBufferList *outputs, NgsModuleParams *params)
{
	outputs->buffers[0]->flags |= NGS_BUFFER_END;
	params->flags = 0;
}

/*
 * samples per channel of 16-bit PCM into destination; then the next two
 * (three with extra set) looked at, for the resampler. 1 when the data
 * ended, -1 on an error.
 */
static int32_t at9Decode(int16_t *destination, NgsAt9Settings *settings, int32_t samples,
	int32_t channels, NgsModuleParams *params, uint8_t *superframe, NgsBufferList *outputs,
	NgsAt9States *states, uint32_t extra)
{
	uint8_t *out = (uint8_t *)destination;
	int32_t needed = samples * channels * 2, length, bytes, over;

	length = overflowTake(out, needed, settings, settings->overflow, 1);
	needed -= length;
	out += length;
	if (settings->overflowBytes == settings->overflowUsed && settings->ended == 1) {
		memset(out, 0, needed + channels * 4);
		return 1;
	}

	while (needed >= 1) {
		bytes = at9DecodeFrame((int16_t *)out, settings, params, superframe, channels);
		if (bytes == -1) {
			memset(out, 0, needed);
			at9Failed(outputs, params);
			return -1;
		}
		bytes *= 2;
		over = needed - bytes;
		if (over < 0)
			overflowKeep(out + needed, -over, settings, settings->overflow);
		bytes = min(bytes, needed);
		needed -= bytes;
		out += bytes;
		if (settings->ended == 1) {
			memset(out, 0, needed + channels * 4);
			at9Failed(outputs, params);
			break;
		}
	}

	/* the samples after, left in the overflow buffer */
	needed = channels * 4;
	if (extra)
		needed += channels * 2;
	length = overflowTake(out, needed, settings, settings->overflow, 0);
	needed -= length;
	out += length;
	if (length) {
		memmove(settings->overflow, settings->overflow + settings->overflowUsed,
			settings->overflowBytes - settings->overflowUsed);
		settings->overflowBytes -= settings->overflowUsed;
	} else {
		settings->overflowUsed = 0;
		settings->overflowBytes = 0;
	}
	settings->overflowUsed = 0;

	while (needed >= 1) {
		bytes = at9DecodeFrame((int16_t *)(settings->overflow + settings->overflowBytes),
			settings, params, superframe, channels);
		if (bytes == -1) {
			memset(out, 0, needed);
			at9Failed(outputs, params);
			return -1;
		}
		settings->overflowBytes += bytes * 2;
		length = overflowTake(out, needed, settings, settings->overflow, 0);
		needed -= length;
		out += length;
	}
	return 0;
}

static void at9DecodeError(NgsAt9Settings *settings, NgsModuleParams *params)
{
	params->callbackData = SCE_NGS_AT9_DECODE_ERROR;
	params->callbackRequested = 1;
	params->callbackPtr = settings->user.buffs[settings->currentBuffer].pBuffer;
	params->callbackData2 = settings->bytePosition;
}

static int32_t at9Run(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsAt9Settings *settings = (NgsAt9Settings *)params->data[0];
	NgsAt9States *states = (NgsAt9States *)params->data[1];
	int16_t *scratch = (int16_t *)params->scratch, *pcm = scratch;
	uint8_t *superframe = (uint8_t *)params->scratch + SUPERFRAME_OFFSET +
		settings->frameSamples;
	uint32_t fraction = settings->fraction, position;
	int32_t needed, extra, written = 0, ended, samples, silent, channel, i, over, generated;
	int32_t output;
	int32_t *data;

	outputs->buffers[0]->channelMask = 0;
	position = max((int32_t)outputs->buffers[0]->samples - settings->leadIn, 0) *
		settings->rate + fraction;
	needed = (int32_t)position >> 15;
	extra = position & 0x7fff;

	if (params->flags & NGS_RUN_BYPASSED) {
		for (output = 0; output < outputs->count; output++) {
			for (channel = 0; channel < outputs->buffers[output]->channels; channel++)
				memset(outputs->buffers[output]->channelData[channel], 0,
					outputs->buffers[output]->samples * NGS_SAMPLE_SIZE);
			outputs->buffers[0]->flags |= NGS_BUFFER_VALID;
		}
		return 0;
	}
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;

	if (settings->leadIn == 0 && settings->rate == RATE_ONE) {
		samples = outputs->buffers[0]->samples;
		ended = at9Decode(scratch, settings, samples, settings->user.nChannels, params,
			superframe, outputs, states, extra);
		if (ended == -1) {
			at9DecodeError(settings, params);
			for (channel = 0; channel < outputs->buffers[0]->channels; channel++)
				memset(outputs->buffers[0]->channelData[channel], 0,
					samples * NGS_SAMPLE_SIZE);
			ended = 0;
		}
		if (settings->user.nChannels == 1) {
			for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
				if (settings->user.nChannelMap[channel] == SCE_NGS_AT9_NULL_CHANNEL)
					continue;
				outputs->buffers[0]->channelMask |= 1 << channel;
				ngsPcmMono((int32_t *)outputs->buffers[0]->channelData[channel],
					scratch, samples);
				written = samples;
			}
		} else if (settings->user.nChannels == 2) {
			NgsBuffer *out = outputs->buffers[0];

			if (out->channels == 1) {
				if (settings->user.nChannelMap[0] == 0) {
					ngsPcmLeft((int32_t *)out->channelData[0], scratch, samples);
					written = samples;
					outputs->buffers[0]->channelMask = 1;
				} else if (settings->user.nChannelMap[0] == 1) {
					ngsPcmRight((int32_t *)out->channelData[0], scratch, samples);
					written = samples;
					outputs->buffers[0]->channelMask = 1;
				}
			} else if (settings->user.nChannelMap[1] == 0 &&
				settings->user.nChannelMap[0] == 1) {
				ngsPcmSplit((int32_t *)out->channelData[1], (int32_t *)out->channelData[0],
					scratch, samples);
				written = samples;
				outputs->buffers[0]->channelMask = 3;
			} else {
				int32_t *left = (int32_t *)out->channelData[0];
				int32_t *right = (int32_t *)out->channelData[1];

				ngsPcmSplit(left, right, scratch, samples);
				written = samples;
				if (settings->user.nChannelMap[0] == 1)
					ngsCopy(left, right, samples * NGS_SAMPLE_SIZE);
				if (settings->user.nChannelMap[1] == 0)
					ngsCopy(right, left, samples * NGS_SAMPLE_SIZE);
				if (settings->user.nChannelMap[0] != SCE_NGS_AT9_NULL_CHANNEL)
					outputs->buffers[0]->channelMask |= 1;
				if (settings->user.nChannelMap[1] != SCE_NGS_AT9_NULL_CHANNEL)
					outputs->buffers[0]->channelMask |= 2;
			}
		}
	} else {
		/* after the samples kept from the last block */
		for (i = 0; i < settings->user.nChannels; i++)
			scratch[i] = settings->history[i];
		pcm = scratch + settings->user.nChannels;
		ended = 0;
		if (needed) {
			ended = at9Decode(pcm, settings, needed, settings->user.nChannels, params,
				superframe, outputs, states, extra);
			if (ended == -1) {
				at9DecodeError(settings, params);
				/* the original clears channels * needed bytes of each */
				for (channel = 0; channel < outputs->buffers[0]->channels; channel++)
					memset(outputs->buffers[0]->channelData[channel], 0,
						settings->user.nChannels * needed);
				return 0;
			}
		}
		for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
			silent = settings->leadIn;
			if ((int32_t)outputs->buffers[0]->samples < silent)
				silent = outputs->buffers[0]->samples;
			if (settings->user.nChannelMap[channel] == SCE_NGS_AT9_NULL_CHANNEL)
				continue;
			data = (int32_t *)outputs->buffers[0]->channelData[channel];
			if (silent)
				memset(data, 0, silent * NGS_SAMPLE_SIZE);
			settings->fraction = fraction;
			written = ngsPlayerResample(&settings->fraction, data + silent, scratch,
				needed, outputs->buffers[0]->samples - silent, settings->rate,
				settings->user.nChannels, settings->user.nChannelMap[channel]);
			outputs->buffers[0]->channelMask |= 1 << channel;
		}
	}

	for (i = 0; i < settings->user.nChannels; i++)
		settings->history[i] = pcm[written * settings->user.nChannels -
			settings->user.nChannels + i];
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	if (ended) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags &= ~NGS_RUN_ENABLED;
	}

	silent = settings->leadIn;
	if ((int32_t)outputs->buffers[0]->samples < silent)
		silent = outputs->buffers[0]->samples;
	settings->leadIn = max(settings->leadIn - (int32_t)outputs->buffers[0]->samples, 0);
	states->nCurrentBuffer = settings->currentBuffer;
	states->nCurrentBytePositionInBuffer = settings->bytePosition;
	states->nSamplesGeneratedSinceKeyOn += outputs->buffers[0]->channels *
		(outputs->buffers[0]->samples - silent);
	states->nSamplesGeneratedTotal += outputs->buffers[0]->channels *
		(outputs->buffers[0]->samples - silent);

	if (settings->limit < 1)
		return 0;
	generated = states->nSamplesGeneratedSinceKeyOn / outputs->buffers[0]->channels;
	if (generated < settings->limit)
		return 0;
	over = generated - settings->limit;
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++)
		memset((int32_t *)outputs->buffers[0]->channelData[channel] +
			(outputs->buffers[0]->samples - over), 0, over * NGS_SAMPLE_SIZE);
	outputs->buffers[0]->flags |= NGS_BUFFER_END;
	params->flags &= ~NGS_RUN_ENABLED;
	return 0;
}
