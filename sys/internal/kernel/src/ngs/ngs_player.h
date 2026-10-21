/* The DSP contexts of the PCM and ATRAC9 player modules. */

#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <ngs/modules/at9_player.h>
#include <ngs/modules/pcm_player.h>

/* A physically contiguous piece of a buffer, as the codec engine reads it. */
typedef struct NgsPlayerRange {
	unsigned int address;
	unsigned int physicalAddress;
	SceSize size;
} NgsPlayerRange;

/* The status a player module keeps in its DSP context. */
typedef struct NgsPlayerStatus {
	signed char currentBuffer;
	unsigned char padding[3];
	unsigned int bufferOffset;
	unsigned short loopCount;
} NgsPlayerStatus;

/*
 * The pieces a player maps for the codec engine: up to two for each of its
 * buffers, and two for the window ahead of the playback position.
 */
#define NGS_PLAYER_RANGES_PER_BUFFER	2
#define NGS_PLAYER_WINDOW_SIZE		0x2000

typedef struct NgsPcmPlayerContext {
	SceNgsPlayerBufferParams buffs[SCE_NGS_PLAYER_MAX_BUFFERS];
	NgsPlayerRange bufferRanges[SCE_NGS_PLAYER_MAX_BUFFERS][NGS_PLAYER_RANGES_PER_BUFFER];
	NgsPlayerRange windowRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	unsigned int reserved;
	NgsPlayerStatus status;
} NgsPcmPlayerContext;

_Static_assert(offsetof(NgsPcmPlayerContext, bufferRanges) == 48, "bufferRanges");
_Static_assert(offsetof(NgsPcmPlayerContext, windowRanges) == 144, "windowRanges");
_Static_assert(offsetof(NgsPcmPlayerContext, status) == 172, "status");

typedef struct NgsAt9PlayerContext {
	SceNgsAT9Params params;
	NgsPlayerRange bufferRanges[SCE_NGS_AT9_MAX_BUFFERS][NGS_PLAYER_RANGES_PER_BUFFER];
	NgsPlayerRange windowRanges[NGS_PLAYER_RANGES_PER_BUFFER];
	unsigned int reserved;
	NgsPlayerStatus status;
	unsigned char decoderState[136];
	int frameSamples;
} NgsAt9PlayerContext;

_Static_assert(offsetof(NgsAt9PlayerContext, bufferRanges) == 96, "bufferRanges");
_Static_assert(offsetof(NgsAt9PlayerContext, windowRanges) == 192, "windowRanges");
_Static_assert(offsetof(NgsAt9PlayerContext, status) == 220, "status");
_Static_assert(offsetof(NgsAt9PlayerContext, frameSamples) == 368, "frameSamples");

void ngsPreparePcmPlayerBuffers(NgsPcmPlayerContext *context,
	const SceNgsPlayerParams *params, int force, int updated,
	unsigned int granularity, int restart, unsigned int sdkVersion);
void ngsPrepareAt9PlayerBuffers(NgsAt9PlayerContext *context,
	const SceNgsAT9Params *params, int force, int updated,
	unsigned int granularity, int restart);
