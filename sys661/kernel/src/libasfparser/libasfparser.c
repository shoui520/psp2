/*
 * PS Vita ePSP 6.61 libasfparser.prx (sceASFParser)
 * Exported sceAsfParser API entry points.
 */
#include "asf_int.h"

static s32 asfGetReadyStream(SceAsfParser *parser, u32 streamId, SceAsfStream **outStream)
{
	*outStream = NULL;
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;
	if (streamId - 1 >= ASF_MAX_STREAMS)
		return SCE_ASF_ERROR_STREAM_RANGE;

	SceAsfStream *stream = parser->streamById[streamId - 1];
	*outStream = stream;
	if (stream == NULL)
		return SCE_ASF_ERROR_STREAM_NOT_FOUND;
	if (stream->stateFlags != 3)
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;
	return 0;
}

static s32 asfIsTimePastEnd(const SceAsfParser *parser, u32 timeMs)
{
	return parser->header.sendDuration < (u64)timeMs * 10000ULL;
}

/* 0x91b11f3a: Initialize ASF parser (non-DRM mode) */
s32 sceAsfParser_91B11F3A(SceAsfParser *parser, void *userData,
                          SceAsfReadCallback readCb, SceAsfSeekCallback seekCb)
{
	s32 ret;

	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;
	if (asfIsDrmEnabled(parser) != 0)
		return SCE_ASF_ERROR_DRM_MODE_MISMATCH;

	ret = asfIoInit(parser, userData, readCb, seekCb);
	if (ret < 0)
		return ret;
	ret = asfInitWorkMem(parser);
	if (ret < 0)
		return ret;
	ret = asfParseHeaderObject(parser);
	if (ret < 0)
		return ret;
	ret = asfParseDataObject(parser);
	if (ret < 0)
		return ret;
	ret = asfInitAllPacketStreams(parser);
	if (ret < 0)
		return ret;
	return asfInitAllCodecs(parser);
}

/* 0xbc85f4d8: Initialize ASF parser (DRM mode) */
s32 sceAsfParser_BC85F4D8(SceAsfParser *parser, void *userData,
                          SceAsfReadCallback readCb, SceAsfSeekCallback seekCb,
                          const SceAsfDrmCallbacks *drmCbs)
{
	s32 ret;

	if (parser == NULL || drmCbs == NULL)
		return SCE_ASF_ERROR_DRM_INVALID_ARG;
	if (asfIsDrmEnabled(parser) == 0)
		return SCE_ASF_ERROR_DRM_MODE_MISMATCH;

	ret = asfSetDrmCallbacks(parser, drmCbs);
	if (ret < 0)
		return ret;
	ret = asfIoInit(parser, userData, readCb, seekCb);
	if (ret < 0)
		return ret;
	ret = asfInitWorkMem(parser);
	if (ret < 0)
		return ret;
	ret = asfParseHeaderObject(parser);
	if (ret < 0)
		return ret;
	ret = asfParseDataObject(parser);
	if (ret < 0)
		return ret;
	ret = asfInitAllPacketStreams(parser);
	if (ret < 0)
		return ret;
	return asfInitAllCodecs(parser);
}

/* 0xf2306e89: Terminate ASF parser / close DRM session */
s32 sceAsfParser_F2306E89(SceAsfParser *parser)
{
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;
	if (parser->drm.initialized == 1 && parser->drm.termCb != NULL)
		return parser->drm.termCb(&parser->drm.session, 0);
	return 0;
}

/* 0x6b3ae857: Seek stream to target timestamp in milliseconds */
s32 sceAsfParser_6B3AE857(SceAsfParser *parser, u32 streamId, u32 *timeMs)
{
	SceAsfStream *stream;
	s32 ret = asfGetReadyStream(parser, streamId, &stream);
	if (ret < 0)
		return ret;

	if (asfIsTimePastEnd(parser, *timeMs) != 0) {
		asfGetStreamTime(stream, parser->codecFlags, timeMs);
		return SCE_ASF_ERROR_EOF_TIME;
	}
	if ((parser->header.flags & 2) == 0)
		return SCE_ASF_ERROR_NOT_SEEKABLE;
	return asfSeekStreamFrame(stream, parser->codecFlags, timeMs);
}

/* 0xc6d98c54: Get offset and size of indexed Stream Properties object */
s32 sceAsfParser_C6D98C54(SceAsfParser *parser, u32 streamIdx, u64 *outOffset, u64 *outSize)
{
	SceAsfObjEntry *entry;
	s32 ret;

	*outOffset = 0;
	*outSize = 0;
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;

	ret = asfGetStreamObjEntry(parser, streamIdx, &entry);
	if (ret < 0)
		return ret;
	return asfReadObjEntryInfo(entry, outOffset, outSize);
}

/* 0x685e0da7: Get offset and size of indexed ASF Header object by mask */
s32 sceAsfParser_685E0DA7(SceAsfParser *parser, u64 objMask, u64 *outOffset, u64 *outSize)
{
	SceAsfObjEntry *entry;
	s32 ret;

	*outOffset = 0;
	*outSize = 0;
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;

	ret = asfGetHeaderObjEntry(parser, objMask, &entry);
	if (ret < 0)
		return ret;
	return asfReadObjEntryInfo(entry, outOffset, outSize);
}

/* 0x95ad7b34: Compute required work buffer size */
s32 sceAsfParser_95AD7B34(SceAsfParser *parser)
{
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;
	parser->needMem = asfCalcNeedMem(parser);
	return 0;
}

/* 0x2cb147f7: Read next decoded frame metadata and bitstream payload */
s32 sceAsfParser_2CB147F7(SceAsfParser *parser, u32 streamId, SceAsfWmaFrame *frame)
{
	SceAsfStream *stream;
	s32 ret = asfGetReadyStream(parser, streamId, &stream);
	if (ret < 0)
		return ret;
	return asfReadNextFrame(stream, parser->codecFlags, frame);
}

/* 0x41322f4a: Seek to absolute position and read little-endian u64 */
s32 sceAsfParser_41322F4A(SceAsfParser *parser, u64 pos, u64 *outVal)
{
	s32 ret = asfIoSeekAbs(parser, pos);
	if (ret < 0)
		return ret;
	return asfIoReadU64(&parser->io, outVal);
}

/* 0x24db62b1: Seek to absolute position and read little-endian u32 */
s32 sceAsfParser_24DB62B1(SceAsfParser *parser, u64 pos, u32 *outVal)
{
	s32 ret = asfIoSeekAbs(parser, pos);
	if (ret < 0)
		return ret;
	return asfIoReadU32(&parser->io, outVal);
}

/* 0x320202ab: Seek to absolute position and read little-endian u16 */
s32 sceAsfParser_320202AB(SceAsfParser *parser, u64 pos, u16 *outVal)
{
	s32 ret = asfIoSeekAbs(parser, pos);
	if (ret < 0)
		return ret;
	return asfIoReadU16(&parser->io, outVal);
}

/* 0x6da63281: Seek to absolute position and read u8 */
s32 sceAsfParser_6DA63281(SceAsfParser *parser, u64 pos, u8 *outVal)
{
	s32 ret = asfIoSeekAbs(parser, pos);
	if (ret < 0)
		return ret;
	return asfIoReadU8(&parser->io, outVal);
}

/* 0x3caac462: Seek to absolute position and read arbitrary byte range */
s32 sceAsfParser_3CAAC462(SceAsfParser *parser, u64 pos, void *buf, u32 size)
{
	s32 ret = asfIoSeekAbs(parser, pos);
	if (ret < 0)
		return ret;
	return asfIoReadBytes(&parser->io, buf, size);
}
