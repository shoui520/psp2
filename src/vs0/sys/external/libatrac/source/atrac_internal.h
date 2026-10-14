#ifndef _ATRAC_INTERNAL_H
#define _ATRAC_INTERNAL_H

/*
 * SceAtrac (vs0:sys/external/libatrac.suprx): ATRAC9 streaming on top of
 * SceAudiodec.  A decoder group of up to 16 channels is created once; each
 * handle owns a slot with an audiodec decoder, a 1 KiB PCM work buffer
 * taken from the group's work memory, and the state of its input stream
 * (a RIFF/WAVE file in a main buffer, optionally with a sub buffer that
 * holds the loop part while the main buffer streams past it).
 *
 * The library was built with SNC; its static functions have no recorded
 * names, so the internal names here are descriptive.
 * Positions are in samples, offsets and sizes in bytes.  Stream sample
 * positions count from the decoder's priming frame: the first frame of
 * the file decodes to positions [0, frameSamples), so block arithmetic
 * subtracts frameSamples from sample positions.
 */

#ifdef __SNC__
/* Sony built the library without automatic inlining (SNC -Xautoinlinesize=0) */
#pragma control autoinlinesize=0
#endif

#include <stddef.h>
#include <psp2/types.h>
#include <psp2/atrac.h>
#include <psp2/audiodec.h>
#include <psp2/kernel/threadmgr.h>

/* SDK kernel/threadmgr.h, missing from the vitasdk headers */
#define SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO	0x00002000U
#define SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE	0x00000002U

#define ATRAC_MAX_DECODERS		16
#define ATRAC_PCM_BUFFER_SIZE		0x400	/* one frame, stereo 16 bit */
#define ATRAC_RIFF_HEADER_SIZE		12

/* SceAudiodec internal export: decode samples [start, start + count) of the next frame */
SceInt32 sceAudiodecPartlyDecode(SceAudiodecCtrl *pCtrl, SceUInt32 start, SceUInt32 count);

/* bufferMode: how the data of the file reaches the decoder */
enum {
	ATRAC_MODE_ALL_ON_MEMORY = 1,	/* the whole file is in the buffer */
	ATRAC_MODE_FILLING = 2,		/* the whole file fits, still being added */
	ATRAC_MODE_STREAM = 4,		/* streaming, no loop */
	ATRAC_MODE_STREAM_LOOP = 5,	/* streaming, the loop runs to the end */
	ATRAC_MODE_STREAM_SUB = 6,	/* streaming, the loop part needs a sub buffer */
};

/* subState: which buffer the decoder reads (mode 6) */
enum {
	ATRAC_SUB_UNUSED = 0,		/* main buffer, loop part not reached */
	ATRAC_SUB_READING = 1,		/* the loop start is read from the sub buffer */
	ATRAC_SUB_DONE = 2,		/* back on the main buffer */
};

/* A RIFF/WAVE (WAVE_FORMAT_EXTENSIBLE, ATRAC9) header, parsed. */
typedef struct AtracHeader {
	SceUInt32 atracType;		/* 0x00 SCE_ATRAC_TYPE_AT9 once a valid fmt chunk is seen */
	SceUInt16 channels;		/* 0x04 */
	SceUInt16 blockAlign;		/* 0x06 bytes per superframe */
	SceUInt16 blockSamples;		/* 0x08 samples per superframe */
	SceUInt16 unk0A;
	SceUInt32 samplingRate;		/* 0x0c */
	SceUInt8 configData[4];		/* 0x10 ATRAC9 config word */
	SceUInt32 dataOffset;		/* 0x14 offset of the data chunk's data */
	SceUInt32 dataSize;		/* 0x18 */
	SceUInt32 totalSamples;		/* 0x1c fact chunk */
	SceUInt32 delaySamples;		/* 0x20 fact chunk: encoder delay */
	SceInt32 loopStart;		/* 0x24 smpl chunk, -1 without a loop */
	SceInt32 loopEnd;		/* 0x28 */
} AtracHeader;

/* The input stream of one handle. */
typedef struct AtracStream {
	SceUInt32 atracType;		/* 0x00 */
	SceUInt32 samplingRate;		/* 0x04 */
	SceUInt32 startSample;		/* 0x08 first sample of the content */
	SceUInt32 endSample;		/* 0x0c last sample of the content */
	SceUInt32 loopStart;		/* 0x10 */
	SceUInt32 loopEnd;		/* 0x14 0: no loop */
	SceUInt32 blockAlign;		/* 0x18 bytes per superframe */
	SceUInt32 blockSamples;		/* 0x1c samples per superframe */
	SceUInt32 frameSamples;		/* 0x20 */
	SceUInt32 dataOffset;		/* 0x24 */
	SceUInt32 dataEnd;		/* 0x28 */
	SceUInt16 channels;		/* 0x2c */
	signed char bufferMode;		/* 0x2e ATRAC_MODE_* */
	signed char subState;		/* 0x2f ATRAC_SUB_* */
	SceUInt32 readPos[2];		/* 0x30 main and sub buffer read offsets */
	SceUChar8 *buffer[2];		/* 0x38 main and sub buffer */
	SceUInt32 bufferSize[2];	/* 0x40 */
	SceUInt32 decodedSample;	/* 0x48 decoder position, block aligned */
	SceUInt32 skipSamples;		/* 0x4c to drop before the next output */
	SceUInt32 pendingSamples;	/* 0x50 decoded into the PCM buffer, not output yet */
	SceUInt32 filePos;		/* 0x54 file offset the decoder reads next */
	SceUInt32 blockRemain;		/* 0x58 bytes left in the current superframe */
	SceInt32 loopNum;		/* 0x5c loops left, -1 infinite */
	SceUInt32 dataSize;		/* 0x60 bytes of stream data in the buffer */
	SceUInt32 outputSamples;	/* 0x64 samples per sceAtracDecode */
	SceUInt32 moveSize;		/* 0x68 bytes to move to the buffer start on wrap */
	SceUInt32 resetDecoder;		/* 0x6c clear the decoder context first */
} AtracStream;

/* One handle. */
typedef struct AtracDecoder {
	SceUInt32 usedCh;		/* 0x00 channels taken from the group, 0: free */
	SceAudiodecCtrl ctrl;		/* 0x04 */
	SceAudiodecInfoAt9 info;	/* 0x2c */
	SceUChar8 *pcmBuffer;		/* 0x48 one frame of PCM for partial output */
	AtracStream stream;		/* 0x4c */
	SceUChar8 reserved[0x100 - 0xbc];
} AtracDecoder;

/* The decoder group. */
typedef struct AtracDecoderGroup {
	SceUInt32 atracType;		/* 0x00 */
	SceUInt32 unk04;
	SceKernelLwMutexWork mutex;	/* 0x08 */
	SceUInt32 totalCh;		/* 0x28 0: not created */
	SceUInt32 vacantCh;		/* 0x2c */
	SceUInt32 wordLength;		/* 0x30 */
	AtracDecoder decoder[ATRAC_MAX_DECODERS];	/* 0x34 */
} AtracDecoderGroup;

extern AtracDecoderGroup g_at9DecoderGroup;

/*
 * The bodies of three small stream functions, which Sony's code has both
 * out of line and expanded in place in some callers.
 */
static inline SceUInt32 streamCurrentSample(const AtracStream *stream)
{
	SceUInt32 pending = stream->pendingSamples;

	if (pending != 0)
		return stream->decodedSample - pending;
	return stream->decodedSample + stream->skipSamples;
}

/* file offset of the last byte of the superframe holding sample */
static inline SceUInt32 streamBlockEnd(const AtracStream *stream, SceUInt32 sample)
{
	SceUInt32 blockAlign = stream->blockAlign;

	return sample / stream->blockSamples * blockAlign + blockAlign + stream->dataOffset - 1;
}

/*
 * The ring buffer offset at which the superframe after the one at pos
 * (remain bytes of it left) can no longer be placed whole, i.e. the end of
 * the usable part of a buffer of size bytes.
 */
static inline SceUInt32 ringNextBlock(SceUInt32 pos, SceUInt32 remain, SceUInt32 blockAlign,
				      SceUInt32 size)
{
	SceUInt32 next = pos - blockAlign + remain;

	if (next >= size)
		next %= size;
	return (size - next) / blockAlign * blockAlign + next;
}

/* atrac_util.c */
void copyBytes(void *dst, const void *src, SceUInt32 size);
SceUInt32 compareBytes(const void *a, const void *b, SceUInt32 size);
SceUInt32 alignSizeUp(SceUInt32 size, SceUInt32 align);
SceUInt32 alignSizeDown(SceUInt32 size, SceUInt32 align);

/* atrac_decoder.c */
SceInt32 initializeAtracCodecLibrary(SceUInt32 atracType, SceUInt32 totalCh);
SceInt32 terminateAtracCodecLibrary(SceUInt32 atracType);
SceInt32 createAtracCodecDecoder(AtracDecoder *decoder, const AtracHeader *header);
SceInt32 deleteAtracCodecDecoder(AtracDecoder *decoder);
SceInt32 decodeAtracFrame(AtracDecoder *decoder, SceUChar8 *out, SceUInt32 start, SceUInt32 count);
SceInt32 atracDecode(AtracDecoder *decoder, SceUChar8 *out, SceUInt32 *pOutputSamples,
		SceUInt32 *pDecoderStatus);
SceInt32 atracSkipSamples(AtracDecoder *decoder);
SceInt32 atracGetInternalError(AtracDecoder *decoder, SceInt32 *pInternalError);

/* atrac_stream.c */
SceUInt32 atracStreamBlockEnd(const AtracStream *stream, SceUInt32 sample);
SceUInt32 atracFrameSamples(SceUInt32 samplingRate);
SceInt32 atracStreamInit(AtracStream *stream, const AtracHeader *header, SceUChar8 *buffer,
		SceUInt32 readSize, SceUInt32 bufferSize);
void atracStreamReset(AtracStream *stream);
void atracStreamConsume(AtracStream *stream, SceUInt32 size);
void atracStreamConsumePending(AtracStream *stream);
void atracStreamAdvance(AtracStream *stream, SceUInt32 consumed, SceUInt32 decoded,
		SceUInt32 output);
SceUInt32 atracStreamUnreadSize(AtracStream *stream);
SceInt32 atracStreamGetVacantSize(AtracStream *stream, SceUInt32 *pVacantSize);
SceUInt32 atracStreamDecoderStatus(AtracStream *stream);
SceInt32 atracStreamGetStreamInfo(AtracStream *stream, SceAtracStreamInfo *pStreamInfo);
SceInt32 atracStreamAddStreamData(AtracStream *stream, SceUInt32 addSize);
SceInt32 atracStreamIsSubBufferNeeded(AtracStream *stream);
SceInt32 atracStreamGetSubBufferInfo(AtracStream *stream, SceUInt32 *pReadPosition,
		SceUInt32 *pMinSubBufferSize, SceUInt32 *pDataSize);
SceInt32 atracStreamSetSubBuffer(AtracStream *stream, SceUChar8 *pSubBuffer,
		SceUInt32 subBufferSize);
SceUInt32 atracStreamSampleFilePos(const AtracStream *stream, SceUInt32 sample);
SceInt32 atracStreamGetContentInfo(const AtracStream *stream, SceAtracContentInfo *pContentInfo);
SceInt32 atracStreamGetNextOutputPosition(AtracStream *stream, SceUInt32 *pNextOutputSample);
SceInt32 atracStreamGetLoopInfo(AtracStream *stream, SceInt32 *pLoopNum, SceUInt32 *pLoopStatus);
SceInt32 atracStreamSetLoopNum(AtracStream *stream, SceInt32 loopNum);
SceInt32 atracStreamResetNextOutputPosition(AtracStream *stream, SceUInt32 resetSample);
SceUInt32 atracStreamGetOutputSamples(const AtracStream *stream);
SceInt32 atracStreamSetOutputSamples(AtracStream *stream, SceUInt32 outputSamples);
SceInt32 atracStreamIsDecodable(AtracStream *stream);
SceUInt32 atracRingNextBlock(SceUInt32 pos, SceUInt32 remain, SceUInt32 blockAlign, SceUInt32 size);
SceUInt32 atracStreamWritePosition(AtracStream *stream);
SceUInt32 atracStreamReadPosition(AtracStream *stream);
SceUInt32 atracStreamWritableSize(AtracStream *stream);
SceUInt32 atracStreamCurrentSample(const AtracStream *stream);
SceUInt32 atracStreamLoopBlock(const AtracStream *stream, SceUInt32 sample);
SceUInt32 atracStreamLoopOffset(const AtracStream *stream, SceUInt32 sample);
SceUInt32 atracStreamBlockFilePos(const AtracStream *stream, SceUInt32 sample);
SceInt64 atracStreamRemainSamples(const AtracStream *stream);
SceUInt32 atracStreamDecodeStart(AtracStream *stream);
SceUInt32 atracStreamLastSample(AtracStream *stream);
SceUInt32 atracStreamDecodableSamples(AtracStream *stream);
SceUInt32 atracStreamFirstFrameSkip(const AtracStream *stream);
SceUInt32 atracStreamFirstFrameSamples(const AtracStream *stream);
SceInt32 atracDeleteDecoderGroup(SceInt32 termAudiodecFlag, SceInt32 lock);

/* atrac.c */
SceInt32 atracResolveHandle(SceInt32 atracHandle, AtracDecoder **pDecoder);
SceUInt32 atracStreamBytesForSamples(AtracStream *stream, SceUInt32 samples);

/* atrac_outputable.c */
SceInt64 atracStreamOutputableSamplesSub(AtracStream *stream);
SceInt64 atracStreamOutputableSamples(AtracStream *stream);
SceInt32 atracParseHeader(AtracHeader *header, const SceUChar8 *buffer, SceUInt32 size);

/* atrac_header.c */

#endif /* _ATRAC_INTERNAL_H */
