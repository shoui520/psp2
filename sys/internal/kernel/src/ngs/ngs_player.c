/*
 * Player buffer preparation: before the codec engine plays a voice, the
 * data it is about to read is cleaned from the data cache and its physical
 * pieces are recorded in the player's DSP context.
 */

#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>

#include "ngs_cache.h"
#include "ngs_player.h"

#define NGS_PLAYER_OUTPUT_RATE		48000.0f
#define NGS_PLAYER_ADPCM_RATIO		3.5f

/* Maps [address, address + size) into the two pieces of ranges. */
static inline __attribute__((always_inline)) void ngsMapPlayerRange(NgsPlayerRange *ranges,
	unsigned int address, int size, SceKernelPARange *paRanges)
{
	SceKernelVARange range;
	SceKernelPAVector vector;

	range.addr = address;
	range.size = size;
	vector.size = sizeof(vector);
	vector.capacity = NGS_PLAYER_RANGES_PER_BUFFER;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = paRanges;
	if (sceKernelVARangeToPAVector(&range, &vector) != 0)
		vector.capacity = 0;
	if (vector.nDataInVector == 0) {
		ranges[0].address = 0;
		ranges[0].physicalAddress = 0;
		ranges[0].size = 0;
	} else {
		ranges[0].address = address;
		ranges[0].physicalAddress = vector.pRanges[0].addr;
		ranges[0].size = vector.pRanges[0].size;
		if (vector.nDataInVector > 1) {
			ranges[1].address = address + vector.pRanges[0].size;
			ranges[1].physicalAddress = vector.pRanges[1].addr;
			ranges[1].size = vector.pRanges[1].size;
			return;
		}
	}
	ranges[1].address = 0;
	ranges[1].physicalAddress = 0;
	ranges[1].size = 0;
}

/*
 * Skips the part of the window [*cursor, *cursor + *window) that ranges
 * already map. Returns 0 when nothing of the window is left.
 */
static inline __attribute__((always_inline)) int ngsSkipMappedRanges(const NgsPlayerRange *ranges, int count,
	unsigned int *cursor, int *window, int *remaining)
{
	unsigned int end;
	int i;

	for (i = 0; i < count; i++) {
		if (ranges[i].address <= *cursor) {
			end = ranges[i].address + ranges[i].size;
			if (end > *cursor) {
				*window -= end - *cursor;
				if (remaining != NULL)
					*remaining -= end - *cursor;
				*cursor = end;
			}
		}
		if (*window <= 0)
			return 0;
	}
	return 1;
}

void ngsPreparePcmPlayerBuffers(NgsPcmPlayerContext *context,
	const SceNgsPlayerParams *params, int force, int updated,
	unsigned int granularity, int restart, unsigned int sdkVersion)
{
	static SceKernelPARange bufferPaRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	static SceKernelPARange windowPaRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	const unsigned char *data;
	unsigned int cursor, windowCursor;
	int buffer, offset, remaining, available, window, bytes, extra, size;
	int channels, headerBytes, changed, b;
	float rate, need;

	if (restart) {
		buffer = params->nStartBuffer;
		if (buffer < 0)
			buffer = 0;
		else if (buffer > SCE_NGS_PLAYER_MAX_BUFFERS - 1)
			buffer = SCE_NGS_PLAYER_MAX_BUFFERS - 1;
		offset = params->nStartByte;
		if (offset < 0)
			offset = 0;
	} else {
		buffer = context->status.currentBuffer;
		offset = context->status.bufferOffset;
	}
	if (offset >= params->buffs[buffer].nNumBytes)
		offset = params->buffs[buffer].nNumBytes;
	remaining = params->buffs[buffer].nNumBytes - offset;

	data = params->buffs[buffer].pBuffer;
	if ((unsigned int)buffer < SCE_NGS_PLAYER_MAX_BUFFERS && data != NULL) {
		channels = params->nChannels;
		rate = (float)channels * (params->fPlaybackFrequency *
			params->fPlaybackScalar) * 2.0f;
		headerBytes = channels * 16;
		if (params->nType == 1) {
			need = rate / NGS_PLAYER_ADPCM_RATIO * granularity /
				NGS_PLAYER_OUTPUT_RATE + headerBytes + headerBytes;
			if (sdkVersion >= 0x2000000)
				need = need + headerBytes + 64.0f;
		} else {
			need = granularity * rate / NGS_PLAYER_OUTPUT_RATE +
				headerBytes;
		}
		bytes = (int)need;
		if (need - (float)bytes > 0.0f)
			bytes++;

		available = remaining < 0 ? 0 : remaining;
		if (available < bytes) {
			/* The data runs into the buffers played next. */
			extra = bytes - available;
			ngsCleanDcacheRange((void *)(data + offset), available);
			for (b = 0; b < SCE_NGS_PLAYER_MAX_BUFFERS; b++) {
				if (params->buffs[b].pBuffer == NULL ||
				    params->buffs[b].nNumBytes == 0)
					continue;
				size = extra + 32;
				if (size > params->buffs[b].nNumBytes)
					size = params->buffs[b].nNumBytes;
				ngsCleanDcacheRange(
					(void *)params->buffs[b].pBuffer, size);
			}
		} else {
			ngsCleanDcacheRange((void *)(data + offset), bytes);
		}
		remaining = params->buffs[buffer].nNumBytes - offset;
	}

	cursor = (unsigned int)params->buffs[buffer].pBuffer + offset;
	if (remaining < 0)
		remaining = 0;
	window = remaining < NGS_PLAYER_WINDOW_SIZE ?
		remaining : NGS_PLAYER_WINDOW_SIZE;

	if (updated == 1) {
		for (b = 0; b < SCE_NGS_PLAYER_MAX_BUFFERS; b++) {
			const void *address = params->buffs[b].pBuffer;

			bytes = params->buffs[b].nNumBytes;
			changed = !(context->buffs[b].nNumBytes == bytes &&
				(unsigned int)address ==
				context->bufferRanges[b][0].address);
			if ((changed | force) == 0 || bytes == 0)
				continue;
			size = params->nChannels << 12;
			ngsCleanDcacheRange((void *)address,
				bytes > size ? size : bytes);
			if (bytes > 0)
				ngsMapPlayerRange(context->bufferRanges[b],
					(unsigned int)address, bytes,
					bufferPaRanges);
		}
	}

	if (!ngsSkipMappedRanges(&context->bufferRanges[0][0],
	    SCE_NGS_PLAYER_MAX_BUFFERS * NGS_PLAYER_RANGES_PER_BUFFER,
	    &cursor, &window, &remaining))
		return;
	windowCursor = cursor;
	if (!ngsSkipMappedRanges(context->windowRanges,
	    NGS_PLAYER_RANGES_PER_BUFFER, &windowCursor, &window, NULL))
		return;
	ngsMapPlayerRange(context->windowRanges, cursor, remaining,
		windowPaRanges);
}

void ngsPrepareAt9PlayerBuffers(NgsAt9PlayerContext *context,
	const SceNgsAT9Params *params, int force, int updated,
	unsigned int granularity, int restart)
{
	static SceKernelPARange windowPaRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	static SceKernelPARange bufferPaRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	const unsigned char *data;
	unsigned int cursor, windowCursor;
	int buffer, offset, remaining, available, window, bytes, extra, size;
	int channels, frameSamples, blocks, changed, b;
	float rate, scale, need;

	if (restart) {
		buffer = 0;
		offset = 0;
	} else {
		buffer = context->status.currentBuffer;
		offset = context->status.bufferOffset;
	}
	if (offset >= params->buffs[buffer].nNumBytes)
		offset = params->buffs[buffer].nNumBytes;
	frameSamples = context->frameSamples;
	if (frameSamples < 0) {
		frameSamples = 0;
		context->frameSamples = 0;
	} else if (frameSamples > 2048) {
		frameSamples = 2048;
		context->frameSamples = 2048;
	}

	data = params->buffs[buffer].pBuffer;
	if ((unsigned int)buffer < SCE_NGS_AT9_MAX_BUFFERS && data != NULL) {
		channels = params->nChannels;
		rate = (float)channels * (params->fPlaybackFrequency *
			params->fPlaybackScalar) * 2.0f;
		if (restart) {
			need = rate * granularity / NGS_PLAYER_OUTPUT_RATE +
				256.0f;
		} else {
			scale = 1.0f;
			if (frameSamples != 0) {
				blocks = 1 << ((params->configData >> 27) & 3);
				if (blocks == 1)
					scale = 512.0f / frameSamples;
				else if (blocks == 4)
					scale = 2048.0f / frameSamples;
				else
					scale = 512.0f / frameSamples;
			}
			need = rate / scale * granularity /
				NGS_PLAYER_OUTPUT_RATE;
			if (frameSamples != 0)
				need = need + (float)(frameSamples *
					(channels * 2));
			else
				need = need + 256.0f;
		}
		bytes = (int)need;
		if (need - (float)bytes > 0.0f)
			bytes++;

		remaining = params->buffs[buffer].nNumBytes - offset;
		available = remaining < 0 ? 0 : remaining;
		if (available < bytes) {
			/* The data runs into the buffers played next. */
			extra = bytes - available;
			ngsCleanDcacheRange((void *)(data + offset), available);
			for (b = 0; b < SCE_NGS_AT9_MAX_BUFFERS; b++) {
				if (params->buffs[b].pBuffer == NULL ||
				    params->buffs[b].nNumBytes == 0)
					continue;
				size = extra + 32;
				if (size > params->buffs[b].nNumBytes)
					size = params->buffs[b].nNumBytes;
				ngsCleanDcacheRange(
					(void *)params->buffs[b].pBuffer, size);
			}
		} else {
			ngsCleanDcacheRange((void *)(data + offset), bytes);
		}
	}

	cursor = (unsigned int)params->buffs[buffer].pBuffer + offset;
	remaining = params->buffs[buffer].nNumBytes - offset;
	if (remaining < 0)
		remaining = 0;
	window = remaining < NGS_PLAYER_WINDOW_SIZE ?
		remaining : NGS_PLAYER_WINDOW_SIZE;

	if (updated == 1) {
		for (b = 0; b < SCE_NGS_AT9_MAX_BUFFERS; b++) {
			const void *address = params->buffs[b].pBuffer;

			bytes = params->buffs[b].nNumBytes;
			changed = !(context->params.buffs[b].nNumBytes == bytes &&
				(unsigned int)address ==
				context->bufferRanges[b][0].address);
			if ((changed | force) == 0 || bytes == 0)
				continue;
			size = params->nChannels << 11;
			ngsCleanDcacheRange((void *)address,
				bytes > size ? size : bytes);
			if (bytes > 0)
				ngsMapPlayerRange(context->bufferRanges[b],
					(unsigned int)address, bytes,
					bufferPaRanges);
		}
	}

	if (!ngsSkipMappedRanges(&context->bufferRanges[0][0],
	    SCE_NGS_AT9_MAX_BUFFERS * NGS_PLAYER_RANGES_PER_BUFFER,
	    &cursor, &window, &remaining))
		return;
	windowCursor = cursor;
	if (!ngsSkipMappedRanges(context->windowRanges,
	    NGS_PLAYER_RANGES_PER_BUFFER, &windowCursor, &window, NULL))
		return;
	ngsMapPlayerRange(context->windowRanges, cursor, remaining,
		windowPaRanges);
}
