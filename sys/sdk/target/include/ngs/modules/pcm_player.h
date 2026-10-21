/*
 * ngs/modules/pcm_player.h: the NGS PCM player module, under Sony's names (SDK Release
 * 03.570.011, target/include/ngs/modules/pcm_player.h).
 */
#ifndef _SCE_NGS_MODULE_PCM_PLAYER_H
#define _SCE_NGS_MODULE_PCM_PLAYER_H

#include <ngs/ngs_top.h>

#define SCE_NGS_PLAYER_ID			(0x5CE6)

#define SCE_NGS_PLAYER_PARAMS_STRUCT_ID		(SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_PLAYER_ID, 1, 1))

#define SCE_NGS_PLAYER_MAX_BUFFERS		(4)

typedef struct SceNgsPlayerBufferParams {
	const void *pBuffer;
	SceInt32 nNumBytes;
	SceInt16 nLoopCount;
	SceInt16 nNextBuff;
} SceNgsPlayerBufferParams;

typedef struct SceNgsPlayerParams {
	SceNgsParamsDescriptor desc;
	SceNgsPlayerBufferParams buffs[SCE_NGS_PLAYER_MAX_BUFFERS];
	SceFloat32 fPlaybackFrequency;
	SceFloat32 fPlaybackScalar;
	SceInt32 nLeadInSamples;
	SceInt32 nLimitNumberOfSamplesPlayed;
	SceInt32 nStartByte;
	SceInt8 nChannels;
	SceInt8 nChannelMap[2];
	SceInt8 nType;
	SceInt8 reserved;
	SceInt8 nStartBuffer;
	SceInt8 pad[2];
} SceNgsPlayerParams;

#endif /* _SCE_NGS_MODULE_PCM_PLAYER_H */
