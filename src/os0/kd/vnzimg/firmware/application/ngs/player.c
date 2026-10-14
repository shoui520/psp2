/*
 * The PCM player (SCE_NGS_PLAYER_ID): 16-bit PCM or HEVAG ADPCM from up to
 * four user buffers, each looped and chained to the next, resampled to the
 * system rate by the VLIW code.
 *
 * The user's buffers are read through address windows the ARM sets in the
 * settings; reading only peeks, and the position moves when the samples
 * used are known.
 */
#include "ngs.h"

#define SCE_NGS_PLAYER_MAX_BUFFERS		4
#define SCE_NGS_PLAYER_MAX_PCM_CHANNELS		2
#define SCE_NGS_PLAYER_NULL_CHANNEL		(-1)
#define SCE_NGS_PLAYER_LOOP_CONTINUOUS		(-1)
#define SCE_NGS_PLAYER_NO_NEXT_BUFFER		(-1)
#define SCE_NGS_PLAYER_TYPE_PCM			0
#define SCE_NGS_PLAYER_TYPE_ADPCM		1
#define SCE_NGS_PLAYER_END_OF_DATA		0
#define SCE_NGS_PLAYER_SWAPPED_BUFFER		1
#define SCE_NGS_PLAYER_LOOPED_BUFFER		2

/* the highest rate the player plays at */
#define MAX_PLAYBACK_FREQUENCY			192000.0f
/* rates in 1.15: 0x8000 plays at the system rate */
#define RATE_ONE				0x8000

/* HEVAG: blocks of 16 bytes, 28 samples; flags 7 ends the data */
#define HEVAG_BLOCK_SIZE			16
#define HEVAG_BLOCK_SAMPLES			28
#define HEVAG_END				7
#define HEVAG_MAX_SKIPPED			8
/* the samples decoded per channel in work memory, and where blocks are read */
#define DECODED_SAMPLES				2464
#define BLOCKS_OFFSET				9984

/* SceNgsPlayerBufferParams */
typedef struct NgsPlayerBuffer {
	uint32_t pBuffer;
	int32_t nNumBytes;
	int16_t nLoopCount;
	int16_t nNextBuff;
} NgsPlayerBuffer;

/* SceNgsPlayerParams */
typedef struct NgsPlayerUserParams {
	SceNgsParamsDescriptor desc;
	NgsPlayerBuffer buffs[SCE_NGS_PLAYER_MAX_BUFFERS];
	float fPlaybackFrequency;
	float fPlaybackScalar;
	int32_t nLeadInSamples;
	int32_t nLimitNumberOfSamplesPlayed;
	int32_t nStartByte;
	int8_t nChannels;
	int8_t nChannelMap[SCE_NGS_PLAYER_MAX_PCM_CHANNELS];
	int8_t nType;
	int8_t reserved;
	int8_t nStartBuffer;
	int8_t pad[2];
} NgsPlayerUserParams;

/* SceNgsPlayerStates */
typedef struct NgsPlayerStates {
	int32_t nCurrentBytePositionInBuffer;
	int32_t nCurrentBuffer;
	int32_t nSamplesGeneratedSinceKeyOn;
	int32_t nBytesConsumedSinceKeyOn;
	int32_t nSamplesGeneratedTotal;
	int32_t nBytesConsumedTotal;
} NgsPlayerStates;

typedef struct NgsPlayerSettings {
	NgsPlayerBuffer buffs[SCE_NGS_PLAYER_MAX_BUFFERS];
	NgsUserWindow windows[SCE_NGS_PLAYER_MAX_BUFFERS][2];
	NgsUserWindow extraWindows[2];
	int32_t rate;			/* 1.15 */
	int8_t currentBuffer;
	uint8_t reserved[3];
	int32_t bytePosition;
	int16_t loops;
	int16_t enabled;
	uint32_t fraction;		/* of the resampler */
	int16_t history[SCE_NGS_PLAYER_MAX_PCM_CHANNELS];	/* the next sample */
	int32_t leadIn;			/* samples of silence still to give */
	int8_t blockOffset;		/* samples of the HEVAG block used */
	uint8_t reserved2[3];
	int32_t hevag[SCE_NGS_PLAYER_MAX_PCM_CHANNELS][4];	/* decoder history */
	int8_t nChannels;
	int8_t nChannelMap[SCE_NGS_PLAYER_MAX_PCM_CHANNELS];
	int8_t nType;
	int32_t limit;
} NgsPlayerSettings;

int32_t ngsPlayerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPlayerUserParams *user = (NgsPlayerUserParams *)params->userParams[0];
	NgsPlayerSettings *settings = (NgsPlayerSettings *)params->data[0];
	NgsPlayerStates *states = (NgsPlayerStates *)params->data[1];
	float frequency;
	int32_t channel;

	if (user->nType == -1) {
		settings->enabled = 0;
		return 0;
	}
	settings->enabled = 1;
	settings->nChannels = user->nChannels;
	settings->nChannelMap[0] = user->nChannelMap[0];
	settings->nChannelMap[1] = user->nChannelMap[1];
	settings->nType = user->nType;
	for (channel = 0; channel < SCE_NGS_PLAYER_MAX_PCM_CHANNELS; channel++) {
		int8_t map = settings->nChannelMap[channel];

		if (map != 0 && map != 1 && map != SCE_NGS_PLAYER_NULL_CHANNEL)
			settings->nChannelMap[channel] = SCE_NGS_PLAYER_NULL_CHANNEL;
	}
	/* a mono source has no right channel */
	for (channel = 0; channel < SCE_NGS_PLAYER_MAX_PCM_CHANNELS; channel++)
		if (settings->nChannelMap[channel] == 1 && settings->nChannels == 1)
			settings->nChannelMap[channel] = SCE_NGS_PLAYER_NULL_CHANNEL;

	frequency = user->fPlaybackScalar * user->fPlaybackFrequency;
	if (!(frequency <= MAX_PLAYBACK_FREQUENCY))
		frequency = MAX_PLAYBACK_FREQUENCY;
	settings->rate = (int32_t)(frequency / (float)(int32_t)params->sampleRate * 32768.0f);
	memcpy(settings->buffs, user->buffs, sizeof(settings->buffs));
	settings->limit = user->nLimitNumberOfSamplesPlayed;

	if (!(params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE))
		return 0;
	settings->currentBuffer = user->nStartBuffer;
	settings->bytePosition = user->nStartByte;
	if (settings->currentBuffer < 0)
		settings->currentBuffer = 0;
	if (settings->bytePosition < 0)
		settings->bytePosition = 0;
	if (settings->currentBuffer >= SCE_NGS_PLAYER_MAX_BUFFERS)
		settings->currentBuffer = SCE_NGS_PLAYER_MAX_BUFFERS - 1;
	settings->loops = 0;
	settings->fraction = 0;
	for (channel = 0; channel < SCE_NGS_PLAYER_MAX_PCM_CHANNELS; channel++)
		settings->history[channel] = 0;
	settings->blockOffset = 0;
	settings->leadIn = user->nLeadInSamples;
	states->nBytesConsumedSinceKeyOn = 0;
	states->nSamplesGeneratedSinceKeyOn = 0;
	return 0;
}

static int32_t playerPcm(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
static int32_t playerAdpcm(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);

int32_t ngsPlayerProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPlayerSettings *settings = (NgsPlayerSettings *)params->data[0];
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
		params->flags = 0;
		return 0;
	}
	if (settings->nType == SCE_NGS_PLAYER_TYPE_ADPCM)
		return playerAdpcm(inputs, outputs, params);
	if (settings->nType == SCE_NGS_PLAYER_TYPE_PCM)
		return playerPcm(inputs, outputs, params);
	return SCE_NGS_ERROR;
}

/* a user address to this side, and the bytes left in its window; 0 if none */
static uint32_t windowMap(NgsPlayerSettings *settings, uint32_t address, int32_t buffer,
	int32_t *left, uint32_t unused)
{
	NgsUserWindow *window;
	uint32_t part, i;

	for (part = 0; part < 2; part++) {
		for (i = 0; i < SCE_NGS_PLAYER_MAX_BUFFERS; i++) {
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

/*
 * The next count bytes of the user's buffers, following loops and the
 * buffer chain, without moving; silence after the data ends.
 */
static void playerPeek(uint8_t *destination, NgsPlayerSettings *settings, int32_t count,
	NgsModuleParams *params)
{
	int32_t loops = settings->loops, buffer = settings->currentBuffer, position;
	int32_t copied = 0, remaining, available, left, length, next;
	NgsPlayerBuffer *current;
	uint32_t mapped;

	if (settings->bytePosition >= settings->buffs[buffer].nNumBytes)
		settings->bytePosition = settings->buffs[buffer].nNumBytes;
	position = settings->bytePosition;
	if (count == 0)
		return;
	remaining = count;

	do {
		available = remaining;
		current = &settings->buffs[buffer];
		left = current->nNumBytes - position;
		if (left) {
			mapped = windowMap(settings, current->pBuffer + position, buffer, &available,
				(uint32_t)params);
			if (mapped == 0)
				return;
			length = min(min(left, remaining), available);
			purgeUser(mapped, length);
			memcpy(destination + copied, (void *)mapped, length);
			copied += length;
			position += length;
			remaining -= length;
		}
		if (position < current->nNumBytes)
			continue;

		position = 0;
		if (loops >= current->nLoopCount && current->nLoopCount != SCE_NGS_PLAYER_LOOP_CONTINUOUS) {
			next = current->nNextBuff;
			if (next < 0 || next >= SCE_NGS_PLAYER_MAX_BUFFERS ||
				settings->buffs[next].pBuffer == 0 ||
				settings->buffs[next].nNumBytes == 0) {
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
	} while (remaining);
}

/*
 * Move on by bytes; a callback at each loop, buffer change and the end.
 * Returns 1 at the end of the data.
 */
static int32_t playerAdvance(NgsPlayerSettings *settings, int32_t bytes, NgsModuleParams *params)
{
	NgsPlayerStates *states = (NgsPlayerStates *)params->data[1];
	NgsPlayerBuffer *current;
	int32_t length, next;

	while (bytes) {
		current = &settings->buffs[settings->currentBuffer];
		length = min(current->nNumBytes - settings->bytePosition, bytes);
		bytes -= length;
		settings->bytePosition += length;
		states->nBytesConsumedTotal += length;
		states->nBytesConsumedSinceKeyOn += length;
		if (settings->bytePosition < settings->buffs[settings->currentBuffer].nNumBytes)
			continue;

		current = &settings->buffs[settings->currentBuffer];
		if (settings->loops >= current->nLoopCount &&
			current->nLoopCount != SCE_NGS_PLAYER_LOOP_CONTINUOUS) {
			settings->bytePosition = 0;
			settings->loops = 0;
			next = current->nNextBuff;
			if (next == SCE_NGS_PLAYER_NO_NEXT_BUFFER || settings->buffs[next].pBuffer == 0 ||
				settings->buffs[next].nNumBytes == 0)
				goto end;
			params->callbackData = SCE_NGS_PLAYER_SWAPPED_BUFFER;
			params->callbackRequested = 1;
			params->callbackPtr = settings->buffs[next].pBuffer;
			params->callbackData2 = settings->currentBuffer;
			settings->currentBuffer = next;
		} else {
			settings->bytePosition = 0;
			settings->loops++;
			if (current->nNumBytes < 1)
				goto end;
			params->callbackRequested = 1;
			params->callbackData = SCE_NGS_PLAYER_LOOPED_BUFFER;
			params->callbackPtr = current->pBuffer;
			params->callbackData2 = settings->loops;
		}
	}
	return 0;

end:
	params->callbackRequested = 1;
	params->callbackData = SCE_NGS_PLAYER_END_OF_DATA;
	params->callbackPtr = 0;
	params->callbackData2 = 0;
	return 1;
}

/* 16-bit samples to 8.24, sixteen at a time */
void ngsPcmMono(int32_t *destination, const int16_t *source, int32_t samples)
{
	int32_t i, count = (samples >> 4) * 16;

	for (i = 0; i < count; i++)
		destination[i] = source[i] << 9;
}

/* interleaved stereo to two channels, eight frames at a time */
void ngsPcmSplit(int32_t *left, int32_t *right, const int16_t *source, int32_t samples)
{
	int32_t i, count = (samples >> 3) * 8;

	for (i = 0; i < count; i++) {
		left[i] = source[2 * i] << 9;
		right[i] = source[2 * i + 1] << 9;
	}
}

void ngsPcmLeft(int32_t *destination, const int16_t *source, int32_t samples)
{
	int32_t i, count = (samples >> 3) * 8;

	for (i = 0; i < count; i++)
		destination[i] = source[2 * i] << 9;
}

void ngsPcmRight(int32_t *destination, const int16_t *source, int32_t samples)
{
	int32_t i, count = (samples >> 3) * 8;

	for (i = 0; i < count; i++)
		destination[i] = source[2 * i + 1] << 9;
}

/* the lead-in, the counters and the sample limit, after a block */
static void playerFinish(NgsPlayerSettings *settings, NgsBufferList *outputs,
	NgsModuleParams *params)
{
	NgsPlayerStates *states = (NgsPlayerStates *)params->data[1];
	NgsBuffer *output = outputs->buffers[0];
	int32_t silent = settings->leadIn, generated, over, channel;

	if (output->samples < silent)
		silent = output->samples;
	settings->leadIn = max(settings->leadIn - (int32_t)outputs->buffers[0]->samples, 0);
	states->nCurrentBuffer = settings->currentBuffer;
	states->nCurrentBytePositionInBuffer = settings->bytePosition;
	output = outputs->buffers[0];
	states->nSamplesGeneratedSinceKeyOn += output->channels * (output->samples - silent);
	states->nSamplesGeneratedTotal += output->channels * (output->samples - silent);

	if (settings->limit < 1)
		return;
	generated = states->nSamplesGeneratedSinceKeyOn / outputs->buffers[0]->channels;
	if (generated < settings->limit)
		return;
	over = generated - settings->limit;
	for (channel = 0; channel < outputs->buffers[0]->channels; channel++)
		memset((int32_t *)outputs->buffers[0]->channelData[channel] +
			(outputs->buffers[0]->samples - over), 0, over * NGS_SAMPLE_SIZE);
	outputs->buffers[0]->flags |= NGS_BUFFER_END;
	params->flags &= ~NGS_RUN_ENABLED;
}

static int32_t playerPcm(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsPlayerSettings *settings = (NgsPlayerSettings *)params->data[0];
	int16_t *scratch = (int16_t *)params->scratch, *pcm = scratch;
	NgsBuffer *output = outputs->buffers[0];
	uint32_t fraction = settings->fraction;
	int32_t needed, samples, silent, channel, i;
	int32_t written = 0;
	int32_t *data;

	needed = (int32_t)(max((int32_t)output->samples - settings->leadIn, 0) * settings->rate +
		fraction) >> 15;
	needed = needed < 0 ? 0 : needed + 4;
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	outputs->buffers[0]->channelMask = 0;

	if (settings->leadIn == 0 && settings->rate == RATE_ONE) {
		samples = outputs->buffers[0]->samples;
		playerPeek((uint8_t *)pcm, settings, settings->nChannels * samples * 2, params);
		output = outputs->buffers[0];
		if (settings->nChannels == 1) {
			for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
				if (settings->nChannelMap[channel] == SCE_NGS_PLAYER_NULL_CHANNEL)
					continue;
				outputs->buffers[0]->channelMask |= 1 << channel;
				ngsPcmMono((int32_t *)outputs->buffers[0]->channelData[channel], pcm, samples);
				written = samples;
			}
		} else if (settings->nChannels == 2) {
			if (output->channels == 1) {
				if (settings->nChannelMap[0] == 0) {
					ngsPcmLeft((int32_t *)output->channelData[0], pcm, samples);
					written = samples;
					outputs->buffers[0]->channelMask = 1;
				} else if (settings->nChannelMap[0] == 1) {
					ngsPcmRight((int32_t *)output->channelData[0], pcm, samples);
					written = samples;
					outputs->buffers[0]->channelMask = 1;
				}
			} else if (settings->nChannelMap[1] == 0 && settings->nChannelMap[0] == 1) {
				ngsPcmSplit((int32_t *)output->channelData[1],
					(int32_t *)output->channelData[0], pcm, samples);
				written = samples;
				outputs->buffers[0]->channelMask = 3;
			} else {
				int32_t *left = (int32_t *)output->channelData[0];
				int32_t *right = (int32_t *)output->channelData[1];

				ngsPcmSplit(left, right, pcm, samples);
				written = samples;
				if (settings->nChannelMap[0] == 1)
					ngsCopy(left, right, samples * NGS_SAMPLE_SIZE);
				if (settings->nChannelMap[1] == 0)
					ngsCopy(right, left, samples * NGS_SAMPLE_SIZE);
				if (settings->nChannelMap[0] != SCE_NGS_PLAYER_NULL_CHANNEL)
					outputs->buffers[0]->channelMask |= 1;
				if (settings->nChannelMap[1] != SCE_NGS_PLAYER_NULL_CHANNEL)
					outputs->buffers[0]->channelMask |= 2;
			}
		}
	} else {
		/* the next samples after the ones kept from the last block */
		for (i = 0; i < settings->nChannels; i++)
			scratch[i] = settings->history[i];
		pcm = scratch + settings->nChannels;
		playerPeek((uint8_t *)pcm, settings, settings->nChannels * needed * 2, params);
		for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
			silent = settings->leadIn;
			if ((int32_t)outputs->buffers[0]->samples < silent)
				silent = outputs->buffers[0]->samples;
			if (settings->nChannelMap[channel] == SCE_NGS_PLAYER_NULL_CHANNEL)
				continue;
			outputs->buffers[0]->channelMask |= 1 << channel;
			data = (int32_t *)outputs->buffers[0]->channelData[channel];
			if (silent)
				memset(data, 0, silent * NGS_SAMPLE_SIZE);
			settings->fraction = fraction;
			written = ngsPlayerResample(&settings->fraction, data + silent, scratch,
				needed, outputs->buffers[0]->samples - silent, settings->rate,
				settings->nChannels, settings->nChannelMap[channel]);
		}
	}

	for (i = 0; i < settings->nChannels; i++)
		settings->history[i] = pcm[written * settings->nChannels - settings->nChannels + i];
	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	if (playerAdvance(settings, settings->nChannels * written * 2, params)) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags = 0;
	}
	playerFinish(settings, outputs, params);
	return 0;
}

/* HEVAG blocks that end the data are skipped, at most eight, history cleared */
static uint8_t *hevagSkipEnds(NgsPlayerSettings *settings, int32_t channel, uint8_t *block,
	int32_t *skipped, int32_t *consumed, int32_t *peeked, int32_t peekBytes, uint8_t *blocks,
	NgsModuleParams *params, uint32_t peek)
{
	int32_t count;

	for (count = 0; count < HEVAG_MAX_SKIPPED && block[1] == HEVAG_END; count++) {
		memset(settings->hevag[channel], 0, sizeof(settings->hevag[channel]));
		*consumed += HEVAG_BLOCK_SIZE;
		block += settings->nChannels * HEVAG_BLOCK_SIZE;
		if (!peek)
			continue;
		(*peeked)++;
		playerPeek(blocks, settings,
			settings->nChannels * (*peeked * HEVAG_BLOCK_SIZE + peekBytes), params);
	}
	*skipped = count;
	return block;
}

static int32_t hevagDecode(int32_t *history, const uint8_t *block, int16_t *samples)
{
	int32_t filter = (block[1] & 0xf0) | (block[0] >> 4);
	int32_t shift = block[0] & 0xf;

	if (filter < 0 || filter >= 129 || shift < 0 || shift >= 13) {
		samples[1] = 0;
		samples[0] = 0;
		return -1;
	}
	ngsHevagDecode(history, block, samples, (int16_t)filter, (int16_t)shift);
	return 0;
}

static int32_t playerAdpcm(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params)
{
	NgsPlayerSettings *settings = (NgsPlayerSettings *)params->data[0];
	int16_t *scratch = (int16_t *)params->scratch;
	uint8_t *blocks = (uint8_t *)(((uint32_t)params->scratch + BLOCKS_OFFSET + 15) & ~15);
	int32_t saved[2][SCE_NGS_PLAYER_MAX_PCM_CHANNELS][4];
	int16_t first[HEVAG_BLOCK_SAMPLES];
	uint32_t fraction = settings->fraction;
	int32_t needed, blockCount, peekBytes, peeked, consumed, skipped, used;
	int32_t channel, i, ended, written = 0, samples, silent, map;
	int16_t *pcm, *out;
	uint8_t *block;

	needed = (int32_t)(max((int32_t)outputs->buffers[0]->samples - settings->leadIn, 0) *
		settings->rate + fraction) >> 15;
	needed = max(needed, 0);
	blockCount = (needed + settings->blockOffset) / HEVAG_BLOCK_SAMPLES;
	outputs->buffers[0]->channelMask = 0;
	peekBytes = blockCount * HEVAG_BLOCK_SIZE;
	playerPeek(blocks, settings, settings->nChannels * (peekBytes + 4 * HEVAG_BLOCK_SIZE), params);
	for (i = 0; i < settings->nChannels; i++)
		scratch[i * DECODED_SAMPLES] = settings->history[i];

	peeked = 4;
	consumed = 0;
	for (channel = 0; channel < settings->nChannels; channel++) {
		pcm = scratch + channel * DECODED_SAMPLES + 1;
		block = blocks + channel * HEVAG_BLOCK_SIZE;
		blockCount = peekBytes / HEVAG_BLOCK_SIZE;
		memset(pcm, 0, blockCount * 28 + 448);

		/* the block in use, from the sample reached */
		block = hevagSkipEnds(settings, channel, block, &skipped, &consumed, &peeked,
			peekBytes, blocks, params, 1);
		if (skipped != HEVAG_MAX_SKIPPED) {
			hevagDecode(settings->hevag[channel], block, first);
			memcpy(pcm, first + settings->blockOffset,
				(HEVAG_BLOCK_SAMPLES - settings->blockOffset) * 2);
			block += settings->nChannels * HEVAG_BLOCK_SIZE;
		} else {
			memset(pcm, 0, (HEVAG_BLOCK_SAMPLES - settings->blockOffset) * 2);
		}
		out = pcm + HEVAG_BLOCK_SAMPLES - settings->blockOffset;

		/* the whole blocks */
		for (i = 1; i < blockCount; i++) {
			ended = 0;
			if (block[1] == HEVAG_END) {
				i--;
				block = hevagSkipEnds(settings, channel, block, &skipped, &consumed,
					&peeked, peekBytes, blocks, params, 1);
				ended = 1;
				if (skipped == HEVAG_MAX_SKIPPED) {
					memset(out, 0, ((blockCount - i) * HEVAG_BLOCK_SAMPLES + 224) * 2);
					break;
				}
			}
			(void)ended;
			hevagDecode(settings->hevag[channel], block, out);
			out += HEVAG_BLOCK_SAMPLES;
			block += settings->nChannels * HEVAG_BLOCK_SIZE;
		}

		/* two blocks ahead, for the resampler; the history kept before each */
		for (i = 0; i < 4; i++)
			saved[0][channel][i] = settings->hevag[channel][i];
		block = hevagSkipEnds(settings, channel, block, &skipped, &consumed, &peeked,
			peekBytes, blocks, params, 1);
		if (skipped != HEVAG_MAX_SKIPPED) {
			hevagDecode(settings->hevag[channel], block, out);
			block += settings->nChannels * HEVAG_BLOCK_SIZE;
		} else {
			memset(out, 0, HEVAG_BLOCK_SAMPLES * 2);
		}
		out += HEVAG_BLOCK_SAMPLES;
		for (i = 0; i < 4; i++)
			saved[1][channel][i] = settings->hevag[channel][i];
		block = hevagSkipEnds(settings, channel, block, &skipped, &consumed, &peeked,
			peekBytes, blocks, params, 0);
		if (skipped != HEVAG_MAX_SKIPPED)
			hevagDecode(settings->hevag[channel], block, out);
		else
			memset(out, 0, HEVAG_BLOCK_SAMPLES * 2);
	}

	if (settings->leadIn == 0 && settings->rate == RATE_ONE) {
		samples = outputs->buffers[0]->samples;
		if (settings->nChannels == 1) {
			for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
				map = settings->nChannelMap[channel];
				if (map == SCE_NGS_PLAYER_NULL_CHANNEL)
					continue;
				outputs->buffers[0]->channelMask |= 1 << channel;
				ngsPcmMono((int32_t *)outputs->buffers[0]->channelData[channel],
					scratch + map * DECODED_SAMPLES, samples);
				written = samples;
			}
		} else if (settings->nChannels == 2) {
			NgsBuffer *output = outputs->buffers[0];
			int32_t *left = (int32_t *)output->channelData[0];
			int32_t *right = (int32_t *)output->channelData[1];

			if (settings->nChannelMap[0] != SCE_NGS_PLAYER_NULL_CHANNEL) {
				ngsPcmMono(left, scratch + settings->nChannelMap[0] * DECODED_SAMPLES, samples);
				outputs->buffers[0]->channelMask |= 1;
			}
			if (outputs->buffers[0]->channels == 2 &&
				settings->nChannelMap[1] != SCE_NGS_PLAYER_NULL_CHANNEL) {
				ngsPcmMono(right, scratch + settings->nChannelMap[1] * DECODED_SAMPLES,
					samples);
				outputs->buffers[0]->channelMask |= 2;
			}
			written = samples;
		}
	} else {
		for (channel = 0; channel < outputs->buffers[0]->channels; channel++) {
			int32_t *data;

			silent = settings->leadIn;
			map = settings->nChannelMap[channel];
			if (map == SCE_NGS_PLAYER_NULL_CHANNEL)
				continue;
			outputs->buffers[0]->channelMask |= 1 << channel;
			if ((int32_t)outputs->buffers[0]->samples < silent)
				silent = outputs->buffers[0]->samples;
			data = (int32_t *)outputs->buffers[0]->channelData[channel];
			if (settings->leadIn)
				memset(data, 0, silent * NGS_SAMPLE_SIZE);
			settings->fraction = fraction;
			if ((int32_t)outputs->buffers[0]->samples - silent >= 1)
				written = ngsAdpcmResample(&settings->fraction, data + silent,
					scratch + map * DECODED_SAMPLES,
					outputs->buffers[0]->samples - silent, settings->rate);
		}
	}

	used = 0;
	if (written) {
		int32_t whole = peekBytes / HEVAG_BLOCK_SIZE * HEVAG_BLOCK_SAMPLES;
		int32_t reached = settings->blockOffset + written;

		for (i = 0; i < settings->nChannels; i++)
			settings->history[i] = scratch[written + i * DECODED_SAMPLES];
		used = peekBytes + 2 * HEVAG_BLOCK_SIZE;
		if (reached < whole + HEVAG_BLOCK_SAMPLES) {
			for (channel = 0; channel < settings->nChannels; channel++)
				for (i = 0; i < 4; i++)
					settings->hevag[channel][i] = saved[0][channel][i];
			settings->blockOffset = HEVAG_BLOCK_SAMPLES -
				(whole - settings->blockOffset - written + HEVAG_BLOCK_SAMPLES);
			used = peekBytes;
		} else if (reached < whole + 2 * HEVAG_BLOCK_SAMPLES) {
			for (channel = 0; channel < settings->nChannels; channel++)
				for (i = 0; i < 4; i++)
					settings->hevag[channel][i] = saved[1][channel][i];
			settings->blockOffset = HEVAG_BLOCK_SAMPLES -
				(whole - settings->blockOffset - written + 2 * HEVAG_BLOCK_SAMPLES);
			used = peekBytes + HEVAG_BLOCK_SIZE;
		}
	}

	if (playerAdvance(settings, consumed + settings->nChannels * used, params)) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags &= ~NGS_RUN_ENABLED;
	}
	outputs->buffers[0]->flags |= NGS_BUFFER_VALID;
	playerFinish(settings, outputs, params);
	return 0;
}
