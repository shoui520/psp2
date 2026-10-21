/*
 * ngs/modules/at9_player.h: the NGS ATRAC9 player module, under Sony's names (SDK Release
 * 03.570.011, target/include/ngs/modules/at9_player.h).
 */
#ifndef _SCE_NGS_MODULE_AT9_PLAYER_H
#define _SCE_NGS_MODULE_AT9_PLAYER_H

#include <ngs/ngs_top.h>

#define SCE_NGS_AT9_ID				(0x5CAA)

#define SCE_NGS_AT9_PARAMS_STRUCT_ID		(SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_AT9_ID, 1, 1))

#define SCE_NGS_AT9_MAX_BUFFERS			(4)

typedef struct SceNgsAT9BufferParams {
	const void *pBuffer;
	SceInt32 nNumBytes;
	SceInt16 nLoopCount;
	SceInt16 nNextBuff;
	SceInt16 nSamplesDiscardStart;
	SceInt16 nSamplesDiscardEnd;
} SceNgsAT9BufferParams;

typedef struct SceNgsAT9Params {
	SceNgsParamsDescriptor desc;
	SceNgsAT9BufferParams buffs[SCE_NGS_AT9_MAX_BUFFERS];
	SceFloat32 fPlaybackFrequency;
	SceFloat32 fPlaybackScalar;
	SceInt32 nLeadInSamples;
	SceInt32 nLimitNumberOfSamplesPlayed;
	SceInt8 nChannels;
	SceInt8 nChannelMap[2];
	SceInt8 reserved;
	SceInt32 configData;
} SceNgsAT9Params;

#endif /* _SCE_NGS_MODULE_AT9_PLAYER_H */
