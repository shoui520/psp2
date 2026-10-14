#include "atrac_internal.h"

/* ---------------------------------------------------------------------- */
/* outputable samples                                                     */

/* bytes of data the decoder needs to output samples more samples */
SceUInt32 atracStreamBytesForSamples(AtracStream *stream, SceUInt32 samples)
{
	SceUInt32 blockSamples, inBlock, size = 0, rest;

	rest = samples + stream->skipSamples;
	if (rest > stream->pendingSamples) {
		rest -= stream->pendingSamples;
		blockSamples = stream->blockSamples;
		inBlock = stream->decodedSample % blockSamples;
		if (inBlock != 0) {
			inBlock = blockSamples - inBlock;
			size = stream->blockRemain;
			rest = rest >= inBlock ? rest - inBlock : 0;
		}
		size = (rest + blockSamples - 1) / blockSamples * stream->blockAlign + size;
	} else if (stream->loopEnd != 0 && atracStreamCurrentSample(stream) <= stream->loopEnd &&
		   atracStreamCurrentSample(stream) + samples > stream->loopEnd &&
		   stream->loopNum != 0 && stream->blockRemain != stream->blockAlign) {
		size = stream->blockRemain;
	}
	return size;
}

/* samples outputable from the data in the buffer, at most limit */
static inline SceUInt32 atracStreamBufferedOutput(const AtracStream *stream, SceUInt32 limit)
{
	SceUInt32 data = stream->dataSize;
	SceUInt32 skip = stream->skipSamples;
	SceUInt32 pending = stream->pendingSamples;
	SceUInt32 blockRemain = stream->blockRemain;
	SceUInt32 blockAlign = stream->blockAlign;
	SceUInt32 decodedSample = stream->decodedSample;
	SceUInt32 blockSamples = stream->blockSamples;
	SceUInt32 samples, count = 0;

	if (blockAlign > blockRemain && blockRemain <= data) {
		pending += alignSizeUp(decodedSample, blockSamples) - decodedSample;
		data -= blockRemain;
	}
	samples = data / blockAlign * blockSamples + pending;
	if (samples >= skip)
		count = samples - skip;
	if (count > limit)
		count = limit;
	return count;
}

/* mode 6: a sub buffer holds the loop part */
SceInt64 atracStreamOutputableSamplesSub(AtracStream *stream)
{
	SceUInt32 dataSize = stream->dataSize;
	SceUInt32 rest, count, left, perLoop, loopBytes, loopStartPos, offset, samples, end;
	SceInt32 loopNum;
	SceInt64 result;

	if (stream->subState == ATRAC_SUB_UNUSED) {
		if (atracStreamCurrentSample(stream) <= stream->loopEnd) {
			loopNum = stream->loopNum;
			rest = stream->loopEnd - atracStreamCurrentSample(stream) + 1;
			count = atracStreamBufferedOutput(stream, rest);
			result = count;
			if (count < rest)
				return result;
			left = atracStreamBytesForSamples(stream, count);
			if (left > dataSize)
				return result;
			left = dataSize - left;
			if (loopNum != 0) {
				perLoop = stream->loopEnd - stream->loopStart + 1;
				loopStartPos = atracStreamSampleFilePos(stream, stream->loopStart);
				loopBytes = atracStreamBlockEnd(stream, stream->loopEnd) - loopStartPos + 1;
				do {
					if (loopBytes > left)
						break;
					left -= loopBytes;
					if (loopNum > 0)
						loopNum--;
					result += perLoop;
				} while (loopNum != 0);
			}
			if (loopNum != 0) {
				/* stopped inside a loop pass: the superframes left */
				offset = atracStreamLoopOffset(stream, stream->loopStart);
				samples = left / stream->blockAlign * stream->blockSamples;
				if (samples > offset)
					result += samples - offset;
			} else {
				/* the rest of the loop end superframe and what the sub buffer holds */
				end = alignSizeUp(stream->loopEnd + 1, stream->blockSamples);
				result += end - stream->loopEnd - 1;
				samples = stream->bufferSize[1] / stream->blockAlign * stream->blockSamples;
				if (stream->endSample - end + 1 < samples)
					samples = stream->endSample - end + 1;
				result += samples;
			}
		} else if (atracStreamBlockEnd(stream, stream->loopEnd) - stream->filePos + 1 >
			   stream->dataSize) {
			result = stream->pendingSamples;
		} else {
			end = stream->bufferSize[1] / stream->blockAlign * stream->blockSamples +
			      alignSizeUp(stream->loopEnd + 1, stream->blockSamples);
			if (end > stream->endSample + 1)
				end = stream->endSample + 1;
			result = end - atracStreamCurrentSample(stream);
		}
	} else {
		result = atracStreamBufferedOutput(stream, (SceUInt32)atracStreamRemainSamples(stream));
	}
	return result;
}

/* samples sceAtracDecode can output from the data in the buffer, -1 for infinite */
SceInt64 atracStreamOutputableSamples(AtracStream *stream)
{
	SceUInt32 samples, current, rest, count, left, perLoop, loopBytes, loopStartPos, offset;
	SceInt32 loopNum;
	SceInt64 result;

	switch (stream->bufferMode) {
	case ATRAC_MODE_ALL_ON_MEMORY:
		return atracStreamRemainSamples(stream);
	case ATRAC_MODE_FILLING:
		result = 0;
		samples = stream->dataSize / stream->blockAlign * stream->blockSamples;
		current = atracStreamCurrentSample(stream);
		if (current < samples) {
			result = samples - current;
			if (stream->loopEnd != 0 && stream->loopEnd < samples && stream->loopNum != 0) {
				if (stream->loopNum < 0)
					result = -1;
				else
					result += (SceUInt64)(stream->loopEnd - stream->loopStart + 1) *
						  (SceInt64)stream->loopNum;
			}
		}
		return result;
	case ATRAC_MODE_STREAM:
		rest = stream->endSample - atracStreamCurrentSample(stream) + 1;
		return atracStreamBufferedOutput(stream, rest);
	case ATRAC_MODE_STREAM_LOOP:
		if (atracStreamCurrentSample(stream) > stream->loopEnd) {
			if (atracStreamBlockEnd(stream, stream->loopEnd) - stream->filePos + 1 >
			    stream->dataSize)
				return stream->pendingSamples;
			return stream->endSample - atracStreamCurrentSample(stream) + 1;
		}
		loopNum = stream->loopNum;
		rest = stream->loopEnd - atracStreamCurrentSample(stream) + 1;
		count = atracStreamBufferedOutput(stream, rest);
		result = count;
		if (count < rest)
			return result;
		left = atracStreamBytesForSamples(stream, count);
		if (left > stream->dataSize)
			return result;
		left = stream->dataSize - left;
		if (loopNum != 0) {
			perLoop = stream->loopEnd - stream->loopStart + 1;
			loopStartPos = atracStreamSampleFilePos(stream, stream->loopStart);
			loopBytes = atracStreamBlockEnd(stream, stream->loopEnd) - loopStartPos + 1;
			do {
				if (loopBytes > left)
					break;
				left -= loopBytes;
				if (loopNum > 0)
					loopNum--;
				result += perLoop;
			} while (loopNum != 0);
		}
		if (loopNum == 0)
			return result + (stream->endSample - stream->loopEnd);
		/* stopped inside a loop pass: the superframes left */
		offset = atracStreamLoopOffset(stream, stream->loopStart);
		samples = left / stream->blockAlign * stream->blockSamples;
		if (samples > offset)
			result += samples - offset;
		return result;
	case ATRAC_MODE_STREAM_SUB:
		return atracStreamOutputableSamplesSub(stream);
	}
	return 0;
}
