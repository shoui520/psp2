#include "atrac_internal.h"

/* ---------------------------------------------------------------------- */
/* SceAudiodec                                                            */

SceInt32 initializeAtracCodecLibrary(SceUInt32 atracType, SceUInt32 totalCh)
{
	SceAudiodecInitParam param;
	SceInt32 ret = SCE_ATRAC_ERROR_INVALID_TYPE;

	if (atracType == SCE_ATRAC_TYPE_AT9) {
		param.at9.size = sizeof(param.at9);
		param.at9.totalCh = totalCh;
		ret = sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9, &param);
	}
	return ret;
}

SceInt32 terminateAtracCodecLibrary(SceUInt32 atracType)
{
	SceInt32 ret = SCE_ATRAC_ERROR_INVALID_TYPE;

	if (atracType == SCE_ATRAC_TYPE_AT9)
		ret = sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9);
	return ret;
}

SceInt32 createAtracCodecDecoder(AtracDecoder *decoder, const AtracHeader *header)
{
	SceUInt32 atracType = decoder->stream.atracType;

	decoder->ctrl.size = sizeof(decoder->ctrl);
	decoder->ctrl.pInfo = (SceAudiodecInfo *)(void *)&decoder->info;
	decoder->ctrl.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;
	decoder->info.size = sizeof(decoder->info);
	copyBytes(decoder->info.configData, header->configData, sizeof(header->configData));
	return sceAudiodecCreateDecoder(&decoder->ctrl, atracType - 0x1000);
}

SceInt32 deleteAtracCodecDecoder(AtracDecoder *decoder)
{
	return sceAudiodecDeleteDecoder(&decoder->ctrl);
}


/*
 * Decodes count samples from offset start of the next frame into out (NULL:
 * decode and drop).  A frame that is cut short goes through the PCM buffer.
 */
SceInt32 decodeAtracFrame(AtracDecoder *decoder, SceUChar8 *out, SceUInt32 start,
				 SceUInt32 count)
{
	AtracStream *stream = &decoder->stream;
	SceAudiodecCtrl *ctrl = &decoder->ctrl;
	SceInt32 internalError;
	SceUInt32 end, decoded, decodeStart, decodeCount;
	SceUInt32 index;
	SceUChar8 *pcm;
	SceInt32 ret;

	if (stream->resetDecoder != 0) {
		ret = sceAudiodecClearContext(ctrl);
		if (ret < 0)
			return ret;
		stream->resetDecoder = 0;
	}
	end = start + count;
	decoded = alignSizeUp(end, stream->frameSamples);
	if ((stream->bufferMode & ATRAC_MODE_STREAM) == 0) {
		ctrl->pEs = stream->buffer[0] + stream->filePos;
	} else {
		index = stream->subState & 1;
		ctrl->pEs = stream->buffer[index] + stream->readPos[index];
	}
	if (decoded == stream->frameSamples && end < decoded) {
		pcm = decoder->pcmBuffer;
		decodeStart = 0;
		decodeCount = stream->frameSamples;
	} else {
		pcm = out;
		decodeStart = start;
		decodeCount = count;
	}
	ctrl->pPcm = pcm;
	if (out == NULL)
		ctrl->pPcm = decoder->pcmBuffer;
	ret = sceAudiodecPartlyDecode(ctrl, decodeStart, decodeCount);
	if (ret < 0) {
		sceAudiodecGetInternalError(ctrl, &internalError);
		return ret;
	}
	if (decoded == stream->frameSamples && end < decoded)
		copyBytes(out, decoder->pcmBuffer + start * stream->channels * 2,
			  count * stream->channels * 2);
	atracStreamAdvance(stream, ctrl->inputEsSize, decoded, count);
	return ret;
}


SceInt32 atracDecode(AtracDecoder *decoder, SceUChar8 *out, SceUInt32 *pOutputSamples,
			    SceUInt32 *pDecoderStatus)
{
	AtracStream *stream = &decoder->stream;
	SceInt32 ret = 0;
	SceInt64 outputable, remain;
	SceUInt32 samples, copied, skip, start, count, rest;

	if (stream->bufferMode == ATRAC_MODE_STREAM_SUB && stream->bufferSize[1] == 0)
		return SCE_ATRAC_ERROR_NEED_SUB_BUFFER;
	if (stream->endSample < atracStreamCurrentSample(stream))
		return SCE_ATRAC_ERROR_ALL_DATA_WAS_DECODED;

	outputable = atracStreamOutputableSamples(stream);
	if (outputable == -1 || outputable >= stream->outputSamples) {
		samples = stream->outputSamples;
	} else {
		remain = atracStreamRemainSamples(stream);
		if (remain == -1 || outputable < remain)
			return SCE_ATRAC_ERROR_DATA_SHORTAGE_IN_BUFFER;
		samples = (SceUInt32)outputable;
	}
	*pOutputSamples = samples;

	/* samples left in the PCM buffer by the last partial frame */
	copied = 0;
	if (stream->pendingSamples != 0) {
		skip = 0;
		if (stream->loopNum == 0) {
			if (stream->decodedSample > stream->endSample + 1)
				skip = stream->decodedSample - stream->endSample - 1;
		}
		if (stream->loopNum != 0) {
			if (stream->decodedSample > stream->loopEnd + 1)
				skip = stream->decodedSample - stream->loopEnd - 1;
		}
		copied = stream->pendingSamples - skip;
		copyBytes(out, decoder->pcmBuffer + (stream->frameSamples - stream->pendingSamples) *
			  stream->channels * 2, copied * stream->channels * 2);
		atracStreamConsumePending(stream);
	}
	out += stream->channels * copied * 2;

	for (samples -= copied; samples != 0; samples -= count) {
		start = atracStreamDecodeStart(stream);
		count = atracStreamDecodableSamples(stream);
		if (samples < count) {
			rest = (start + samples) % stream->frameSamples;
			count = samples;
			if (start + samples > stream->frameSamples && rest != 0) {
				if (rest > count) {
					start -= rest - count;
					count = 0;
				} else {
					count -= rest;
				}
			}
		}
		ret = decodeAtracFrame(decoder, out, start, count);
		if (ret < 0)
			return ret;
		out += count * stream->channels * 2;
	}
	*pDecoderStatus = atracStreamDecoderStatus(stream);
	return ret;
}

/* decodes and drops what lies before the output position */
SceInt32 atracSkipSamples(AtracDecoder *decoder)
{
	AtracStream *stream = &decoder->stream;
	SceUInt32 start;
	SceInt32 ret = 0;

	if (atracStreamIsDecodable(stream) == 0)
		return ret;
	while (stream->skipSamples != 0) {
		start = atracStreamDecodeStart(stream);
		if (start > stream->frameSamples)
			start -= start % stream->frameSamples;
		ret = decodeAtracFrame(decoder, NULL, start, 0);
		if (ret < 0)
			return ret;
	}
	return ret;
}

SceInt32 atracGetInternalError(AtracDecoder *decoder, SceInt32 *pInternalError)
{
	return sceAudiodecGetInternalError(&decoder->ctrl, pInternalError);
}
