/*
 * ATRAC9 section detail computation for SceNgsUser.
 */
#include "ngs_internal.h"

SceInt32 ngsAt9GetSectionDetailsImpl(SceInt32 startSampleOffset, SceInt32 numSamples,
				     SceInt32 configData, SceNgsAT9SkipBufferInfo *at9InfoBuffer)
{
	SceUInt8 syncByte = (SceUInt8)(configData & 0xFF);
	SceUInt32 sampleRateCode = ((SceUInt16)configData) >> 12;
	SceUInt32 framesPerSuper = 1U << ((configData >> 27) & 2);
	SceUInt32 frameBytesField = (((SceUInt32)((configData >> 24) & 0xFF) << 8) |
				     ((SceUInt32)((configData >> 16) & 0xFF) << 16)) >> 13;
	SceBool useNewSdkBehavior = SceLibKernel_9F793F84() >= 0x01500000U;
	SceInt32 frameSamples;
	SceInt32 superframeSamples;
	SceInt32 startSample = startSampleOffset;

	if (sampleRateCode == 1U) {
		frameSamples = 64;
		superframeSamples = 256;
	} else if (sampleRateCode == 4U) {
		frameSamples = 128;
		superframeSamples = 512;
	} else if (sampleRateCode == 7U) {
		frameSamples = 256;
		superframeSamples = 1024;
	} else {
		return SCE_NGS_ERROR;
	}

	if (useNewSdkBehavior) {
		SceBool fromHeader = (startSampleOffset & (SceInt32)SCE_NGS_SAMPLE_OFFSET_FROM_AT9_HEADER) != 0;
		startSample = startSampleOffset & 0x7FFFFFFF;
		if (fromHeader) {
			startSample -= frameSamples;
			if (startSample < 0)
				startSample = 0;
		}
		if (numSamples < 0)
			return SCE_NGS_ERROR_INVALID_PARAM;
	}

	if (at9InfoBuffer == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;

	ngsMemset(at9InfoBuffer, 0, sizeof(*at9InfoBuffer));
	if (syncByte != 0xFEU)
		return SCE_NGS_ERROR;

	SceUInt32 superframeBytes = framesPerSuper + frameBytesField * framesPerSuper;

	if (framesPerSuper == 1U) {
		SceInt32 effectiveStart = useNewSdkBehavior ? startSample : startSampleOffset;
		SceInt32 firstFrame = effectiveStart / frameSamples;
		SceInt16 startSkip = (SceInt16)(frameSamples + effectiveStart - firstFrame * frameSamples);
		SceInt32 totalFrames = (numSamples + startSkip + frameSamples - 1) / frameSamples;

		at9InfoBuffer->nIsSuperPacket = 0;
		at9InfoBuffer->nStartByteOffset = (SceInt32)(firstFrame * superframeBytes);
		at9InfoBuffer->nStartSkip = startSkip;
		at9InfoBuffer->nNumBytes = (SceInt32)(totalFrames * superframeBytes);
		at9InfoBuffer->nEndSkip = (SceInt16)(totalFrames * frameSamples - (numSamples + startSkip));
		return SCE_NGS_OK;
	}

	SceInt32 adjustedStart = (useNewSdkBehavior ? startSample : startSampleOffset) + frameSamples;
	SceInt32 mask = -superframeSamples;
	SceInt32 firstSuper = (adjustedStart & mask) / superframeSamples;
	SceInt32 endSuper = ((adjustedStart + numSamples + superframeSamples - 1) & mask) / superframeSamples;
	SceInt16 startSkip = (SceInt16)(adjustedStart - firstSuper * superframeSamples);
	SceInt32 startByteOffset = (SceInt32)(superframeBytes * firstSuper);
	SceInt32 numBytes = (SceInt32)(superframeBytes * (endSuper - firstSuper));

	at9InfoBuffer->nIsSuperPacket = 1;
	at9InfoBuffer->nStartByteOffset = startByteOffset;
	at9InfoBuffer->nNumBytes = numBytes;
	at9InfoBuffer->nStartSkip = startSkip;
	at9InfoBuffer->nEndSkip = (SceInt16)(endSuper * superframeSamples - (adjustedStart + numSamples));

	if (startSkip < frameSamples && startByteOffset != 0) {
		at9InfoBuffer->nStartByteOffset = startByteOffset - (SceInt32)superframeBytes;
		at9InfoBuffer->nNumBytes = numBytes + (SceInt32)superframeBytes;
		at9InfoBuffer->nStartSkip = (SceInt16)(startSkip + superframeSamples);
	}

	return SCE_NGS_OK;
}
