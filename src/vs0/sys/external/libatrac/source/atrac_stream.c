#include "atrac_internal.h"

/* ---------------------------------------------------------------------- */
/* AtracStream                                                            */

/* file offset of the last byte of the superframe holding sample */
SceUInt32 atracStreamBlockEnd(const AtracStream *stream, SceUInt32 sample)
{
	return streamBlockEnd(stream, sample);
}

SceUInt32 atracFrameSamples(SceUInt32 samplingRate)
{
	switch (samplingRate) {
	case 48000:
	case 44100:
	case 32000:
		return 256;
	case 24000:
	case 22050:
	case 16000:
		return 128;
	case 12000:
	case 11025:
		return 64;
	}
	return 256;
}


SceInt32 atracStreamInit(AtracStream *stream, const AtracHeader *header,
				SceUChar8 *buffer, SceUInt32 readSize, SceUInt32 bufferSize)
{
	SceUInt32 frameSamples = atracFrameSamples(header->samplingRate);
	SceUInt32 endSample, blockAlign, blockSamples, dataOffset, dataEnd, readable;
	SceUInt32 loopStart = 0, loopEnd = 0;
	SceUInt32 loopBlockEnd, samples, need, end;
	SceUInt16 channels;

	stream->atracType = header->atracType;
	stream->samplingRate = header->samplingRate;
	stream->startSample = header->delaySamples;
	endSample = header->delaySamples + header->totalSamples - 1;
	stream->endSample = endSample;
	blockSamples = header->blockSamples;
	blockAlign = header->blockAlign;
	channels = header->channels;
	dataOffset = header->dataOffset;
	if (header->loopStart >= 0) {
		loopStart = header->loopStart;
		loopEnd = header->loopEnd;
	}
	stream->loopStart = loopStart;
	stream->loopEnd = loopEnd;
	stream->blockAlign = blockAlign;
	dataEnd = header->dataSize + dataOffset;
	stream->blockSamples = blockSamples;
	stream->frameSamples = frameSamples;
	stream->dataOffset = dataOffset;
	stream->dataEnd = dataEnd;
	stream->channels = channels;
	stream->subState = ATRAC_SUB_UNUSED;
	stream->readPos[0] = dataOffset;
	stream->readPos[1] = 0;
	stream->buffer[1] = NULL;
	stream->bufferSize[1] = 0;
	stream->decodedSample = 0;
	stream->skipSamples = header->delaySamples;
	stream->buffer[0] = buffer;
	stream->bufferSize[0] = bufferSize;
	readable = readSize - dataOffset;
	stream->pendingSamples = 0;
	stream->filePos = dataOffset;
	stream->blockRemain = blockAlign;
	stream->loopNum = 0;
	stream->dataSize = readable;
	stream->outputSamples = frameSamples;

	if (loopEnd > endSample)
		return SCE_ATRAC_ERROR_INVALID_DATA;
	if (endSample / blockSamples * blockAlign >= dataEnd - dataOffset)
		return SCE_ATRAC_ERROR_INVALID_DATA;

	if (bufferSize >= dataEnd) {
		if (readSize >= dataEnd)
			stream->bufferMode = ATRAC_MODE_ALL_ON_MEMORY;
		else
			stream->bufferMode = ATRAC_MODE_FILLING;
		stream->moveSize = 0;
	} else {
		/* the main buffer must hold the superframes of 48 frames */
		samples = blockSamples;
		if (loopEnd != 0 && loopEnd - loopStart + 1 < samples)
			samples = loopEnd - loopStart + 1;
		need = (samples + frameSamples * 48 - 1) / samples * blockAlign + dataOffset;
		if (bufferSize < ((need + 0xff) & ~0xff))
			return SCE_ATRAC_ERROR_MAIN_BUFFER_SIZE_IS_TOO_SMALL;
		if (readable < blockAlign)
			return SCE_ATRAC_ERROR_READ_SIZE_IS_TOO_SMALL;
		if (loopEnd == 0) {
			stream->bufferMode = ATRAC_MODE_STREAM;
		} else {
			loopBlockEnd = loopEnd / blockSamples * blockAlign + blockAlign + dataOffset - 1;
			if (loopBlockEnd < endSample / blockSamples * blockAlign + blockAlign + dataOffset - 1) {
				stream->bufferMode = ATRAC_MODE_STREAM_SUB;
				end = loopBlockEnd - dataOffset + 1;
				if (readable > end)
					stream->dataSize = end;
			} else {
				stream->bufferMode = ATRAC_MODE_STREAM_LOOP;
			}
		}
		end = atracRingNextBlock(dataOffset, blockAlign, blockAlign, bufferSize);
		if (stream->dataOffset + stream->dataSize > end)
			stream->moveSize = stream->dataOffset + stream->dataSize - end;
		else
			stream->moveSize = 0;
	}
	stream->resetDecoder = 0;
	return 0;
}

void atracStreamReset(AtracStream *stream)
{
	stream->subState = 0;
	stream->bufferMode = 0;
	stream->atracType = 0;
	stream->samplingRate = 0;
	stream->startSample = 0;
	stream->endSample = 0;
	stream->loopStart = 0;
	stream->loopEnd = 0;
	stream->blockAlign = 0;
	stream->blockSamples = 0;
	stream->frameSamples = 0;
	stream->dataOffset = 0;
	stream->dataEnd = 0;
	stream->channels = 0;
	stream->readPos[0] = 0;
	stream->readPos[1] = 0;
	stream->buffer[0] = NULL;
	stream->buffer[1] = NULL;
	stream->bufferSize[0] = 0;
	stream->bufferSize[1] = 0;
	stream->decodedSample = 0;
	stream->skipSamples = 0;
	stream->pendingSamples = 0;
	stream->filePos = 0;
	stream->blockRemain = 0;
	stream->loopNum = 0;
	stream->dataSize = 0;
	stream->outputSamples = 0;
	stream->moveSize = 0;
	stream->resetDecoder = 0;
}

/* the decoder consumed size bytes of a streaming buffer */
void atracStreamConsume(AtracStream *stream, SceUInt32 size)
{
	SceUInt32 blockRemain, pos, sub;

	stream->dataSize -= size;
	blockRemain = stream->blockRemain;
	if (stream->moveSize != 0 && stream->moveSize <= stream->readPos[0] + size) {
		/* the read position wrapped: move the data read past the end to the start */
		pos = atracRingNextBlock(stream->readPos[0] + size, blockRemain,
					 stream->blockAlign, stream->bufferSize[0]);
		copyBytes(stream->buffer[0], stream->buffer[0] + pos, stream->moveSize);
		blockRemain = stream->blockRemain;
		stream->moveSize = 0;
	}
	if (stream->subState == ATRAC_SUB_READING) {
		stream->readPos[1] += size;
		if (stream->readPos[1] + blockRemain > stream->bufferSize[1]) {
			stream->subState = ATRAC_SUB_DONE;
			stream->readPos[0] = 0;
			stream->readPos[1] = 0;
		}
	} else {
		pos = stream->readPos[0] + size;
		if (pos + blockRemain > stream->bufferSize[0])
			pos = 0;
		stream->readPos[0] = pos;
		if (stream->loopEnd != 0 && atracStreamCurrentSample(stream) <= stream->loopEnd)
			return;
		if (stream->bufferMode == ATRAC_MODE_STREAM_SUB && stream->subState == ATRAC_SUB_UNUSED &&
		    stream->filePos > streamBlockEnd(stream, stream->loopEnd)) {
			/* past the loop: continue in the sub buffer, its head goes to the main buffer */
			sub = stream->bufferSize[1];
			pos = sub % stream->blockAlign;
			stream->dataSize = sub;
			stream->subState = ATRAC_SUB_READING;
			stream->readPos[1] = 0;
			copyBytes(stream->buffer[0], stream->buffer[1] + (sub - pos), pos);
		}
	}
}


/* the pending samples were output */
void atracStreamConsumePending(AtracStream *stream)
{
	SceUInt32 size = 0;

	stream->pendingSamples = 0;
	if (stream->loopNum != 0 && stream->loopEnd < atracStreamCurrentSample(stream)) {
		if (stream->blockAlign != stream->blockRemain) {
			stream->resetDecoder = 1;
			size = stream->blockRemain;
		}
		stream->decodedSample = atracStreamLoopBlock(stream, stream->loopStart);
		stream->skipSamples = atracStreamLoopOffset(stream, stream->loopStart);
		stream->filePos = atracStreamBlockFilePos(stream, stream->loopStart);
		stream->blockRemain = stream->blockAlign;
		if (stream->loopNum > 0)
			stream->loopNum--;
	}
	if (stream->bufferMode & ATRAC_MODE_STREAM)
		atracStreamConsume(stream, size);
	else
		stream->readPos[0] = stream->filePos;
}

/* a frame was decoded: consumed bytes, decoded samples (frame aligned), output samples */
void atracStreamAdvance(AtracStream *stream, SceUInt32 consumed, SceUInt32 decoded,
			       SceUInt32 output)
{
	SceUInt32 skip = stream->skipSamples;
	SceUInt32 decodedSample = stream->decodedSample;
	SceInt32 loopNum = stream->loopNum;
	SceUInt32 filePos = stream->filePos;
	SceUInt32 blockRemain = stream->blockRemain;
	SceUInt32 pending = 0;
	SceUInt32 used, block;

	if (decoded < output + skip) {
		pending = 0;
		skip = skip >= decoded ? skip - decoded : 0;
	} else {
		pending = decoded - output - skip;
		skip = 0;
	}
	stream->skipSamples = skip;
	filePos += consumed;
	stream->decodedSample = decodedSample + decoded;
	stream->pendingSamples = pending;
	stream->filePos = filePos;
	block = blockRemain;
	used = consumed;
	if (blockRemain <= consumed) {
		block = stream->blockAlign;
		used = (consumed - blockRemain) % block;
	}
	stream->blockRemain = block - used;
	if (loopNum != 0) {
		if (stream->loopEnd < atracStreamCurrentSample(stream)) {
			if (stream->blockRemain < stream->blockAlign) {
				stream->resetDecoder = 1;
				consumed += stream->blockRemain;
			}
			stream->decodedSample = atracStreamLoopBlock(stream, stream->loopStart);
			stream->skipSamples = atracStreamLoopOffset(stream, stream->loopStart);
			filePos = atracStreamBlockFilePos(stream, stream->loopStart);
			stream->pendingSamples = 0;
			stream->filePos = filePos;
			stream->blockRemain = stream->blockAlign;
			if (stream->loopNum > 0)
				stream->loopNum--;
		} else {
			filePos = stream->filePos;
		}
	}
	if (stream->bufferMode & ATRAC_MODE_STREAM)
		atracStreamConsume(stream, consumed);
	else
		stream->readPos[0] = filePos;
}

/* stream data in the buffer the decoder has not reached yet */
static inline SceUInt32 streamUnreadSize(AtracStream *stream)
{
	SceUInt32 end, size = 0;

	if (atracStreamCurrentSample(stream) > stream->loopEnd &&
	    stream->subState == ATRAC_SUB_READING) {
		end = atracRingNextBlock(0, stream->blockAlign, stream->blockAlign, stream->bufferSize[1]);
		if (end >= stream->readPos[1]) {
			end -= stream->readPos[1];
			if (stream->dataSize > end)
				size = stream->dataSize - end;
		}
	} else {
		size = stream->dataSize;
	}
	return size;
}

SceUInt32 atracStreamUnreadSize(AtracStream *stream)
{
	return streamUnreadSize(stream);
}

SceInt32 atracStreamGetVacantSize(AtracStream *stream, SceUInt32 *pVacantSize)
{
	SceUInt32 blockAlign, bufferSize, data, pos, remain, end, size;
	signed char subState;

	*pVacantSize = 0;
	switch (stream->bufferMode) {
	case ATRAC_MODE_ALL_ON_MEMORY:
	case ATRAC_MODE_FILLING:
		*pVacantSize = stream->bufferSize[0] - (stream->dataOffset + stream->dataSize);
		break;
	case ATRAC_MODE_STREAM:
	case ATRAC_MODE_STREAM_LOOP:
	case ATRAC_MODE_STREAM_SUB:
		data = streamUnreadSize(stream);
		blockAlign = stream->blockAlign;
		subState = stream->subState;
		bufferSize = stream->bufferSize[0];
		pos = 0;
		remain = blockAlign;
		if (subState != ATRAC_SUB_READING) {
			pos = stream->readPos[0];
			remain = stream->blockRemain;
		}
		end = atracRingNextBlock(pos, remain, blockAlign, bufferSize);
		if (end < data) {
			size = 0;
		} else {
			size = end - data;
			if (bufferSize / blockAlign * blockAlign < pos) {
				pos -= end;
				size = pos > size ? 0 : size - pos;
			}
		}
		*pVacantSize = size;
		break;
	}
	return 0;
}

SceUInt32 atracStreamDecoderStatus(AtracStream *stream)
{
	SceUInt32 status = 0;
	SceInt64 remain;

	if (stream->bufferMode & ATRAC_MODE_STREAM) {
		if (stream->loopEnd == 0 || atracStreamCurrentSample(stream) > stream->loopEnd) {
			if (atracStreamRemainSamples(stream) == atracStreamOutputableSamples(stream))
				status = SCE_ATRAC_DECODER_STATUS_NONLOOP_PART_IS_ON_MEMORY;
		} else {
			remain = atracStreamRemainSamples(stream);
			if (remain >= 0 && remain - stream->endSample + stream->loopEnd <=
			    atracStreamOutputableSamples(stream))
				status = SCE_ATRAC_DECODER_STATUS_LOOP_PART_IS_ON_MEMORY;
		}
	}
	if (stream->bufferMode == ATRAC_MODE_ALL_ON_MEMORY)
		status |= SCE_ATRAC_DECODER_STATUS_ALL_DATA_IS_ON_MEMORY;
	if (atracStreamCurrentSample(stream) > stream->endSample)
		status |= SCE_ATRAC_DECODER_STATUS_ALL_DATA_WAS_DECODED;
	return status;
}


SceInt32 atracStreamGetStreamInfo(AtracStream *stream, SceAtracStreamInfo *pStreamInfo)
{
	pStreamInfo->pWritePosition = (SceUChar8 *)atracStreamWritePosition(stream);
	pStreamInfo->readPosition = atracStreamReadPosition(stream);
	pStreamInfo->writableSize = atracStreamWritableSize(stream);
	return 0;
}

SceInt32 atracStreamAddStreamData(AtracStream *stream, SceUInt32 addSize)
{
	SceUInt32 writable;
	SceInt32 ret = 0;

	writable = atracStreamWritableSize(stream);
	if (addSize > writable) {
		ret = SCE_ATRAC_ERROR_ADDED_DATA_IS_TOO_BIG;
	} else {
		stream->dataSize += addSize;
		if (stream->bufferMode == ATRAC_MODE_FILLING && writable == addSize)
			stream->bufferMode = ATRAC_MODE_ALL_ON_MEMORY;
	}
	return ret;
}

SceInt32 atracStreamIsSubBufferNeeded(AtracStream *stream)
{
	return stream->bufferMode == ATRAC_MODE_STREAM_SUB;
}

SceInt32 atracStreamGetSubBufferInfo(AtracStream *stream, SceUInt32 *pReadPosition,
					    SceUInt32 *pMinSubBufferSize, SceUInt32 *pDataSize)
{
	SceUInt32 blockAlign, size, rest;
	SceInt32 ret = 0;
	SceUInt32 dataSize = 0;

	if (stream->bufferMode == ATRAC_MODE_STREAM_SUB) {
		blockAlign = stream->blockAlign;
		*pReadPosition = stream->loopEnd / stream->blockSamples * blockAlign + blockAlign +
				 stream->dataOffset;
		size = stream->frameSamples * 16 / stream->blockSamples * stream->blockAlign;
		rest = stream->dataEnd - (stream->loopEnd / stream->blockSamples * stream->blockAlign +
					  stream->blockAlign + stream->dataOffset);
		if (rest < size)
			size = rest;
		*pMinSubBufferSize = (size + 0xff) & ~0xff;
		dataSize = stream->dataEnd - *pReadPosition;
	} else {
		ret = SCE_ATRAC_ERROR_NO_NEED_SUB_BUFFER;
		*pReadPosition = dataSize;
		*pMinSubBufferSize = dataSize;
	}
	*pDataSize = dataSize;
	return ret;
}

SceInt32 atracStreamSetSubBuffer(AtracStream *stream, SceUChar8 *pSubBuffer,
					SceUInt32 subBufferSize)
{
	SceUInt32 readPosition, minSubBufferSize, dataSize;
	SceInt32 ret;

	if (stream->bufferMode != ATRAC_MODE_STREAM_SUB)
		return SCE_ATRAC_ERROR_NO_NEED_SUB_BUFFER;
	ret = atracStreamGetSubBufferInfo(stream, &readPosition, &minSubBufferSize, &dataSize);
	if (ret < 0)
		return ret;
	if (minSubBufferSize > subBufferSize)
		return SCE_ATRAC_ERROR_SUB_BUFFER_SIZE_IS_TOO_SMALL;
	stream->buffer[1] = pSubBuffer;
	stream->bufferSize[1] = subBufferSize;
	stream->readPos[1] = 0;
	return ret;
}

/* file offset of the superframe holding sample */
SceUInt32 atracStreamSampleFilePos(const AtracStream *stream, SceUInt32 sample)
{
	SceUInt32 blockAlign = stream->blockAlign;

	return blockAlign * ((sample - stream->frameSamples) / stream->blockSamples) +
	       stream->dataOffset;
}

SceInt32 atracStreamGetContentInfo(const AtracStream *stream,
					  SceAtracContentInfo *pContentInfo)
{
	SceUInt32 samplingRate = stream->samplingRate;
	SceUInt32 loopEnd, frameSamples, blockAlign, blockSamples, offset;
	SceInt32 loopStartSample, loopEndSample;
	SceUInt32 loopBlockOffset, loopBlockSize;

	pContentInfo->atracType = stream->atracType;
	pContentInfo->channel = stream->channels;
	pContentInfo->samplingRate = samplingRate;
	pContentInfo->endSample = stream->endSample - stream->startSample;
	loopEnd = stream->loopEnd;
	frameSamples = stream->frameSamples;
	blockSamples = stream->blockSamples;
	if (loopEnd == 0) {
		loopEndSample = -1;
		blockAlign = stream->blockAlign;
		loopStartSample = -1;
	} else {
		loopStartSample = stream->loopStart - stream->startSample;
		blockAlign = stream->blockAlign;
		loopEndSample = loopEnd - stream->startSample;
	}
	pContentInfo->loopStartSample = loopStartSample;
	pContentInfo->loopEndSample = loopEndSample;
	pContentInfo->bitRate = blockAlign * samplingRate * 8 / blockSamples;
	pContentInfo->fixedEncBlockSize = blockAlign;
	pContentInfo->fixedEncBlockSample = blockSamples;
	pContentInfo->frameSample = frameSamples;
	loopBlockOffset = 0;
	loopBlockSize = 0;
	if (loopEnd != 0) {
		offset = blockAlign * ((stream->loopStart - frameSamples) / blockSamples);
		loopBlockOffset = stream->dataOffset + offset;
		loopBlockSize = loopEnd / blockSamples * blockAlign + blockAlign - offset;
	}
	pContentInfo->loopBlockOffset = loopBlockOffset;
	pContentInfo->loopBlockSize = loopBlockSize;
	return 0;
}

SceInt32 atracStreamGetNextOutputPosition(AtracStream *stream, SceUInt32 *pNextOutputSample)
{
	SceUInt32 current;
	SceInt32 ret = 0;

	current = atracStreamCurrentSample(stream);
	if (current > stream->endSample)
		ret = SCE_ATRAC_ERROR_ALL_DATA_WAS_DECODED;
	*pNextOutputSample = current - stream->startSample;
	return ret;
}

SceInt32 atracStreamGetLoopInfo(AtracStream *stream, SceInt32 *pLoopNum,
				       SceUInt32 *pLoopStatus)
{
	SceUInt32 status;

	*pLoopNum = stream->loopNum;
	if (stream->loopEnd == 0 || atracStreamCurrentSample(stream) > stream->loopEnd)
		status = SCE_ATRAC_LOOP_STATUS_NON_RESETABLE_PART;
	else
		status = SCE_ATRAC_LOOP_STATUS_RESETABLE_PART;
	*pLoopStatus = status;
	return 0;
}

SceInt32 atracStreamSetLoopNum(AtracStream *stream, SceInt32 loopNum)
{
	SceInt32 ret = 0;

	if (stream->loopEnd == 0 || atracStreamCurrentSample(stream) > stream->loopEnd)
		ret = SCE_ATRAC_ERROR_INVALID_LOOP_STATUS;
	else
		stream->loopNum = loopNum;
	return ret;
}

SceInt32 atracStreamResetNextOutputPosition(AtracStream *stream, SceUInt32 resetSample)
{
	SceUInt32 sample, blockAlign, blockSamples, dataOffset, filePos, loopBlockEnd, end;

	if (stream->bufferMode == ATRAC_MODE_STREAM_SUB && stream->bufferSize[1] == 0)
		return SCE_ATRAC_ERROR_NEED_SUB_BUFFER;
	sample = resetSample + stream->startSample;
	if (sample > stream->endSample)
		return SCE_ATRAC_ERROR_INVALID_SAMPLE;
	stream->decodedSample = atracStreamLoopBlock(stream, sample);
	stream->skipSamples = atracStreamLoopOffset(stream, sample);
	blockSamples = stream->blockSamples;
	stream->loopNum = 0;
	blockAlign = stream->blockAlign;
	dataOffset = stream->dataOffset;
	filePos = blockAlign * ((sample - stream->frameSamples) / blockSamples) + dataOffset;
	stream->filePos = filePos;
	stream->resetDecoder = blockAlign != stream->blockRemain;
	stream->moveSize = 0;
	stream->pendingSamples = 0;
	stream->blockRemain = blockAlign;
	switch (stream->bufferMode) {
	case ATRAC_MODE_FILLING:
		stream->subState = ATRAC_SUB_UNUSED;
		break;
	case ATRAC_MODE_STREAM:
	case ATRAC_MODE_STREAM_LOOP:
		stream->subState = ATRAC_SUB_UNUSED;
		stream->readPos[0] = 0;
		stream->dataSize = 0;
		break;
	case ATRAC_MODE_STREAM_SUB:
		loopBlockEnd = stream->loopEnd / blockSamples * blockAlign + blockAlign + dataOffset - 1;
		if (loopBlockEnd > filePos) {
			stream->subState = ATRAC_SUB_UNUSED;
			stream->readPos[0] = 0;
			stream->dataSize = 0;
		} else {
			end = atracRingNextBlock(0, blockAlign, blockAlign, stream->bufferSize[1]) +
			      loopBlockEnd;
			if (filePos < end) {
				stream->subState = ATRAC_SUB_READING;
				stream->readPos[1] = filePos - loopBlockEnd - 1;
				stream->dataSize = end - filePos + 1;
			} else {
				stream->subState = ATRAC_SUB_DONE;
				stream->readPos[0] = 0;
				stream->dataSize = 0;
			}
		}
		break;
	}
	return 0;
}

SceUInt32 atracStreamGetOutputSamples(const AtracStream *stream)
{
	return stream->outputSamples;
}

SceInt32 atracStreamSetOutputSamples(AtracStream *stream, SceUInt32 outputSamples)
{
	SceUInt32 max = stream->frameSamples * SCE_ATRAC_AT9_MAX_OUTPUT_FRAMES;
	SceInt32 ret = 0;

	if (outputSamples == 0 || outputSamples % stream->frameSamples != 0 || outputSamples > max)
		ret = SCE_ATRAC_ERROR_INVALID_MAX_OUTPUT_SAMPLES;
	else
		stream->outputSamples = outputSamples;
	return ret;
}

/* 1 when the buffer holds the data of the frames still to be skipped */
SceInt32 atracStreamIsDecodable(AtracStream *stream)
{
	SceUInt32 skip = stream->skipSamples;
	SceUInt32 blockAlign = stream->blockAlign;
	SceUInt32 blockSamples = stream->blockSamples;
	signed char bufferMode = stream->bufferMode;
	SceUInt32 partial = 0, inBlock, need, available, loopBlockEnd;
	SceInt32 decodable = 1;

	if (stream->blockRemain != blockAlign) {
		inBlock = blockSamples - stream->decodedSample % blockSamples;
		partial = stream->blockRemain;
		skip = inBlock < skip ? skip - inBlock : 0;
	}
	need = (skip + blockSamples - 1) / blockSamples * blockAlign + partial;
	if (bufferMode & ATRAC_MODE_STREAM) {
		available = stream->dataSize;
		if (bufferMode == ATRAC_MODE_STREAM_SUB && stream->subState == ATRAC_SUB_UNUSED &&
		    stream->loopNum == 0) {
			loopBlockEnd = stream->loopEnd / blockSamples * blockAlign + blockAlign +
				       stream->dataOffset;
			if (stream->filePos + available > loopBlockEnd - 1)
				available = loopBlockEnd + stream->bufferSize[1] - stream->filePos;
		}
		if (need > available)
			decodable = 0;
	}
	if (bufferMode == ATRAC_MODE_FILLING &&
	    stream->dataSize + stream->dataOffset < stream->filePos + need)
		decodable = 0;
	return decodable;
}

/*
 * The ring buffer offset at which the superframe after the one at pos
 * (remain bytes of it left) can no longer be placed whole, i.e. the end of
 * the usable part of a buffer of size bytes.
 */
SceUInt32 atracRingNextBlock(SceUInt32 pos, SceUInt32 remain, SceUInt32 blockAlign,
			     SceUInt32 size)
{
	return ringNextBlock(pos, remain, blockAlign, size);
}

/* data position + size, wrapped at the usable end of the ring */
static inline SceUInt32 atracRingWrap(SceUInt32 pos, SceUInt32 size, SceUInt32 remain,
				      SceUInt32 blockAlign, SceUInt32 bufferSize)
{
	SceUInt32 write = pos + size;
	SceUInt32 end = ringNextBlock(pos, remain, blockAlign, bufferSize);

	if (write >= end)
		write = end > size ? write - end : 0;
	return write;
}

SceUInt32 atracStreamWritePosition(AtracStream *stream)
{
	SceUInt32 offset = 0;

	switch (stream->bufferMode) {
	case ATRAC_MODE_ALL_ON_MEMORY:
		break;
	case ATRAC_MODE_FILLING:
		offset = stream->dataOffset + stream->dataSize;
		break;
	case ATRAC_MODE_STREAM:
		if (stream->filePos + stream->dataSize < stream->dataEnd)
			offset = atracRingWrap(stream->readPos[0], stream->dataSize, stream->blockRemain,
					       stream->blockAlign, stream->bufferSize[0]);
		break;
	case ATRAC_MODE_STREAM_SUB:
		if (atracStreamCurrentSample(stream) > stream->loopEnd &&
		    stream->subState != ATRAC_SUB_DONE) {
			if (stream->subState == ATRAC_SUB_READING) {
				offset = atracRingWrap(0, atracStreamUnreadSize(stream),
						       stream->blockAlign, stream->blockAlign,
						       stream->bufferSize[0]);
			} else if (atracStreamBlockEnd(stream, stream->loopEnd) - stream->filePos + 1 >
				   stream->dataSize) {
				offset = atracRingWrap(stream->readPos[0], stream->dataSize,
						       stream->blockRemain, stream->blockAlign,
						       stream->bufferSize[0]);
			}
			break;
		}
		/* fall through */
	case ATRAC_MODE_STREAM_LOOP:
		offset = atracRingWrap(stream->readPos[0], stream->dataSize, stream->blockRemain,
				       stream->blockAlign, stream->bufferSize[0]);
		break;
	}
	return (SceUInt32)(stream->buffer[0] + offset);
}

SceUInt32 atracStreamReadPosition(AtracStream *stream)
{
	SceUInt32 position = 0;
	SceUInt32 end, loopStartPos, loopBlockEnd;

	switch (stream->bufferMode) {
	case ATRAC_MODE_ALL_ON_MEMORY:
		break;
	case ATRAC_MODE_FILLING:
		position = stream->dataOffset + stream->dataSize;
		break;
	case ATRAC_MODE_STREAM:
		end = stream->filePos + stream->dataSize;
		if (end < stream->dataEnd && stream->dataSize < stream->bufferSize[0])
			position = end;
		break;
	case ATRAC_MODE_STREAM_SUB:
		if (atracStreamCurrentSample(stream) > stream->loopEnd) {
			if (stream->subState != ATRAC_SUB_UNUSED) {
				SceUInt32 unread = atracStreamUnreadSize(stream);

				end = stream->filePos + stream->dataSize;
				if (end < stream->dataEnd && unread < stream->bufferSize[0])
					position = end;
				return position;
			}
			if (atracStreamBlockEnd(stream, stream->loopEnd) - stream->filePos + 1 <=
			    stream->dataSize)
				return 0;
			return stream->filePos + stream->dataSize;
		}
		/* fall through */
	case ATRAC_MODE_STREAM_LOOP:
		loopStartPos = atracStreamSampleFilePos(stream, stream->loopStart);
		loopBlockEnd = atracStreamBlockEnd(stream, stream->loopEnd);
		position = 0;
		if (stream->dataSize < stream->bufferSize[0]) {
			position = stream->filePos + stream->dataSize;
			end = loopBlockEnd + 1;
			if (position >= end)
				position = loopStartPos + (position - end) %
					   (loopBlockEnd - loopStartPos + 1);
		}
		break;
	}
	return position;
}

SceUInt32 atracStreamWritableSize(AtracStream *stream)
{
	SceUInt32 size = 0;
	SceUInt32 data, pos, remain, blockAlign, bufferSize, end, over, limit, loopBlockEnd;
	SceUInt32 loopStartPos, cap;
	signed char bufferMode, subState, mode;

	switch (stream->bufferMode) {
	case ATRAC_MODE_ALL_ON_MEMORY:
		break;
	case ATRAC_MODE_FILLING:
		size = stream->dataEnd - (stream->dataOffset + stream->dataSize);
		break;
	case ATRAC_MODE_STREAM:
	case ATRAC_MODE_STREAM_LOOP:
	case ATRAC_MODE_STREAM_SUB:
		data = atracStreamUnreadSize(stream);
		subState = stream->subState;
		bufferMode = stream->bufferMode;
		blockAlign = stream->blockAlign;
		bufferSize = stream->bufferSize[0];
		pos = 0;
		remain = blockAlign;
		if (subState != ATRAC_SUB_READING) {
			pos = stream->readPos[0];
			data += pos;
			remain = stream->blockRemain;
		}
		end = ringNextBlock(pos, remain, blockAlign, bufferSize);
		if (data < end) {
			size = end - data;
		} else {
			over = data - end;
			cap = bufferSize / blockAlign * blockAlign;
			if (cap < pos)
				pos = cap;
			size = 0;
			if (over < pos)
				size = pos - over;
		}

		mode = bufferMode;
		if (mode == ATRAC_MODE_STREAM_SUB) {
			mode = ATRAC_MODE_STREAM;
			if (atracStreamCurrentSample(stream) <= stream->loopEnd)
				mode = ATRAC_MODE_STREAM_LOOP;
			subState = stream->subState;
			bufferMode = stream->bufferMode;
		}
		limit = 0;
		switch (mode) {
		case ATRAC_MODE_STREAM:
			if (stream->filePos + stream->dataSize < stream->dataEnd)
				limit = stream->dataEnd - (stream->filePos + stream->dataSize);
			if (bufferMode == ATRAC_MODE_STREAM_SUB && subState == ATRAC_SUB_UNUSED) {
				loopBlockEnd = atracStreamBlockEnd(stream, stream->loopEnd) -
					       stream->filePos + 1;
				limit = 0;
				if (stream->dataSize < loopBlockEnd)
					limit = loopBlockEnd - stream->dataSize;
			}
			break;
		case ATRAC_MODE_STREAM_LOOP:
			loopBlockEnd = atracStreamBlockEnd(stream, stream->loopEnd);
			limit = loopBlockEnd + 1;
			over = stream->filePos + stream->dataSize;
			if (over >= limit) {
				loopStartPos = atracStreamSampleFilePos(stream, stream->loopStart);
				limit = loopBlockEnd - loopStartPos + 1;
				over = (stream->filePos + stream->dataSize - (loopBlockEnd + 1)) % limit;
			}
			limit -= over;
			break;
		}
		if (limit < size)
			size = limit;
		break;
	}
	return size;
}

SceUInt32 atracStreamCurrentSample(const AtracStream *stream)
{
	return streamCurrentSample(stream);
}

/* decoder position of the superframe holding sample */
SceUInt32 atracStreamLoopBlock(const AtracStream *stream, SceUInt32 sample)
{
	SceUInt32 blockSamples = stream->blockSamples;

	return (sample - stream->frameSamples) / blockSamples * blockSamples;
}

/* samples to skip in that superframe to reach sample */
SceUInt32 atracStreamLoopOffset(const AtracStream *stream, SceUInt32 sample)
{
	SceUInt32 frameSamples = stream->frameSamples;

	return (sample - frameSamples) % stream->blockSamples + frameSamples;
}

SceUInt32 atracStreamBlockFilePos(const AtracStream *stream, SceUInt32 sample)
{
	return (sample - stream->frameSamples) / stream->blockSamples * stream->blockAlign +
	       stream->dataOffset;
}

/* samples left to output, -1 when looping forever */
SceInt64 atracStreamRemainSamples(const AtracStream *stream)
{
	SceUInt32 current = streamCurrentSample(stream);
	SceInt32 loopNum = stream->loopNum;
	SceUInt32 loops;

	if (loopNum < 0)
		return -1;
	loops = (stream->loopEnd - stream->loopStart) * loopNum + loopNum;
	return (SceUInt64)(stream->endSample - current + 1) + loops;
}

/* samples in the superframes the buffer holds, from the read position */
static inline SceUInt32 atracStreamBufferedSamples(const AtracStream *stream)
{
	SceUInt32 index = stream->subState & 1;
	SceUInt32 pos = stream->readPos[index];
	SceUInt32 end, blockSamples, samples;

	end = atracRingNextBlock(pos, stream->blockRemain, stream->blockAlign,
				 stream->bufferSize[index]);
	blockSamples = stream->blockSamples;
	samples = (end - pos) / stream->blockAlign * blockSamples + stream->pendingSamples;
	if (stream->blockAlign != stream->blockRemain)
		samples += blockSamples - stream->decodedSample % blockSamples;
	return samples;
}

/* offset in the next frame where output starts */
SceUInt32 atracStreamDecodeStart(AtracStream *stream)
{
	SceUInt32 start = stream->skipSamples;
	SceUInt32 samples;

	if (stream->bufferMode & ATRAC_MODE_STREAM) {
		samples = atracStreamBufferedSamples(stream);
		if (samples < start)
			start = samples;
	}
	return start;
}

/* the last sample the decoder may reach before it must loop or stop */
SceUInt32 atracStreamLastSample(AtracStream *stream)
{
	SceUInt32 last;

	if (stream->bufferMode != ATRAC_MODE_STREAM_SUB) {
		if (stream->loopNum == 0)
			return stream->endSample;
		return stream->loopEnd;
	}
	switch (stream->subState) {
	case ATRAC_SUB_UNUSED:
		if (stream->loopNum != 0)
			return stream->loopEnd;
		return alignSizeUp(stream->loopEnd + 1, stream->blockSamples) - 1;
	case ATRAC_SUB_READING:
		last = alignSizeUp(stream->loopEnd + 1, stream->blockSamples) - 1;
		last += stream->bufferSize[1] / stream->blockAlign * stream->blockSamples;
		if (last > stream->endSample)
			return stream->endSample;
		return last;
	}
	return stream->endSample;
}

/* samples the next decode call may produce */
SceUInt32 atracStreamDecodableSamples(AtracStream *stream)
{
	SceUInt32 current = streamCurrentSample(stream);
	SceUInt32 last = atracStreamLastSample(stream);
	SceUInt32 count = 0, buffered = 0, samples;

	if (last + 1 >= current)
		count = last - current + 1;
	if (stream->bufferMode & ATRAC_MODE_STREAM) {
		samples = atracStreamBufferedSamples(stream);
		if (samples >= stream->skipSamples)
			buffered = samples - stream->skipSamples;
		if (buffered < count)
			count = buffered;
	}
	return count;
}

/* name unknown: no caller */
SceUInt32 atracStreamFirstFrameSkip(const AtracStream *stream)
{
	SceUInt32 skip = stream->skipSamples;

	if (skip > stream->frameSamples)
		skip = stream->frameSamples;
	return skip;
}

/* name unknown: no caller */
SceUInt32 atracStreamFirstFrameSamples(const AtracStream *stream)
{
	SceUInt32 frameSamples = stream->frameSamples;
	SceUInt32 last = stream->loopNum == 0 ? stream->endSample : stream->loopEnd;
	SceUInt32 samples, skip;

	samples = last - stream->decodedSample + 1;
	if (samples > frameSamples)
		samples = frameSamples;
	skip = stream->skipSamples;
	if (skip > frameSamples)
		skip = frameSamples;
	return samples - skip;
}
