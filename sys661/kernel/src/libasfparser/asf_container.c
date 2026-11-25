/*
 * PS Vita ePSP 6.61 libasfparser.prx (sceASFParser)
 * ASF container header, GUID dispatch, stream properties, DRM header, and work memory layout.
 */
#include "asf_int.h"

/* GUID indices into g_asfGuids[40] */
enum {
	GUID_HEADER             = 0,
	GUID_DATA               = 1,
	GUID_FILE_PROPERTIES    = 2,
	GUID_STREAM_PROPERTIES  = 3,
	GUID_EXT_STREAM_ALT     = 4,
	GUID_HEADER_EXTENSION   = 5,
	GUID_CODEC_LIST         = 6,
	GUID_SCRIPT_COMMAND     = 7,
	GUID_MARKER             = 8,
	GUID_BITRATE_MUTEX      = 9,
	GUID_ERROR_CORRECTION   = 10,
	GUID_CONTENT_DESC       = 11,
	GUID_EXT_CONTENT_DESC   = 12,
	GUID_CONTENT_BRANDING   = 13,
	GUID_STREAM_BITRATE     = 14,
	GUID_CONTENT_ENCRYPTION = 15,
	GUID_EXT_CONTENT_ENC    = 16,
	GUID_DIGITAL_SIGNATURE  = 17,
	GUID_PADDING            = 18,
	GUID_EXT_STREAM_PROP    = 19,
	/* 20..31: Header Extension sub-objects to skip */
	GUID_MEDIA_AUDIO        = 32,
	GUID_MEDIA_VIDEO        = 33,
	GUID_MEDIA_COMMAND      = 34,
	GUID_MEDIA_JFIF         = 35,
	GUID_MEDIA_DEGRAD_JPEG  = 36,
	GUID_MEDIA_FILE_XFER    = 37,
	GUID_MEDIA_BINARY       = 38,
	GUID_MEDIA_DRM          = 39,
};

typedef struct AsfMinSizeRule {
	u64 mask;
	u32 minSize;
	s32 errCode;
} AsfMinSizeRule;

typedef struct AsfSimpleObjRule {
	const u8 *guid;
	u64 mask;
} AsfSimpleObjRule;

static const AsfMinSizeRule g_asfMinSizeTable[15] = {
	{ ASF_OBJ_MASK_FILE_PROP,      104, SCE_ASF_ERROR_OBJ_MIN_FILE_PROP },
	{ ASF_OBJ_MASK_STREAM_PROP,     78, SCE_ASF_ERROR_OBJ_MIN_STREAM_PROP },
	{ ASF_OBJ_MASK_HEADER_EXT,      46, SCE_ASF_ERROR_OBJ_MIN_HEADER_EXT },
	{ ASF_OBJ_MASK_CODEC_LIST,      44, SCE_ASF_ERROR_OBJ_MIN_CODEC_LIST },
	{ ASF_OBJ_MASK_SCRIPT_CMD,      44, SCE_ASF_ERROR_OBJ_MIN_SCRIPT_CMD },
	{ ASF_OBJ_MASK_MARKER,          48, SCE_ASF_ERROR_OBJ_MIN_MARKER },
	{ ASF_OBJ_MASK_BITRATE_MUTEX,   42, SCE_ASF_ERROR_OBJ_MIN_BITRATE_MUTEX },
	{ ASF_OBJ_MASK_ERR_CORR,        44, SCE_ASF_ERROR_OBJ_MIN_ERR_CORR },
	{ ASF_OBJ_MASK_CONTENT_DESC,    34, SCE_ASF_ERROR_OBJ_MIN_CONTENT_DESC },
	{ ASF_OBJ_MASK_EXT_CONTENT,     26, SCE_ASF_ERROR_OBJ_MIN_EXT_CONTENT },
	{ ASF_OBJ_MASK_STREAM_BITRATE,  26, SCE_ASF_ERROR_OBJ_MIN_STREAM_BITR },
	{ ASF_OBJ_MASK_CONTENT_BRAND,   40, SCE_ASF_ERROR_OBJ_MIN_CONTENT_BRAND },
	{ ASF_OBJ_MASK_CONTENT_ENC,     40, SCE_ASF_ERROR_OBJ_MIN_CONTENT_ENC },
	{ ASF_OBJ_MASK_EXT_ENC,         24, SCE_ASF_ERROR_OBJ_MIN_EXT_ENC },
	{ ASF_OBJ_MASK_DIGITAL_SIG,     32, SCE_ASF_ERROR_OBJ_MIN_DIG_SIG },
};

static const AsfSimpleObjRule g_asfSimpleObjTable[12] = {
	{ g_asfGuids[GUID_CODEC_LIST],         ASF_OBJ_MASK_CODEC_LIST },
	{ g_asfGuids[GUID_SCRIPT_COMMAND],     ASF_OBJ_MASK_SCRIPT_CMD },
	{ g_asfGuids[GUID_MARKER],             ASF_OBJ_MASK_MARKER },
	{ g_asfGuids[GUID_BITRATE_MUTEX],      ASF_OBJ_MASK_BITRATE_MUTEX },
	{ g_asfGuids[GUID_ERROR_CORRECTION],   ASF_OBJ_MASK_ERR_CORR },
	{ g_asfGuids[GUID_CONTENT_DESC],       ASF_OBJ_MASK_CONTENT_DESC },
	{ g_asfGuids[GUID_EXT_CONTENT_DESC],   ASF_OBJ_MASK_EXT_CONTENT },
	{ g_asfGuids[GUID_STREAM_BITRATE],     ASF_OBJ_MASK_STREAM_BITRATE },
	{ g_asfGuids[GUID_CONTENT_BRANDING],   ASF_OBJ_MASK_CONTENT_BRAND },
	{ g_asfGuids[GUID_CONTENT_ENCRYPTION], ASF_OBJ_MASK_CONTENT_ENC },
	{ g_asfGuids[GUID_EXT_CONTENT_ENC],    ASF_OBJ_MASK_EXT_ENC },
	{ g_asfGuids[GUID_DIGITAL_SIGNATURE],  ASF_OBJ_MASK_DIGITAL_SIG },
};

/* Helpers for codec & object mask decoding */
static u32 asfMaxStreamsFromFlags(u64 objectFlags)
{
	return (u32)((objectFlags & 0xFEULL) >> 1);
}

static u64 asfSingleObjBits(u64 objectFlags)
{
	return objectFlags & ASF_OBJ_MASK_SINGLE_VALID;
}

s32 asfIsDrmEnabled(const SceAsfParser *parser)
{
	if ((parser->drmEnableFlags & 1) == 0)
		return 0;
	return (parser->objectFlags & ASF_OBJ_MASK_DRM_ANY) != 0;
}

static u32 asfCodecInfoSize(u64 codecFlags)
{
	return (codecFlags & ASF_CODEC_FLAG_WMA) ? sizeof(SceAsfWmaWaveFormat) : 0;
}

static u32 asfCodecCtxSize(u64 codecFlags)
{
	return (codecFlags & ASF_CODEC_FLAG_WMA) ? sizeof(SceWmaParserCtx) : 0;
}

static u32 asfCountSingleObjs(u64 objectFlags)
{
	u64 bits = asfSingleObjBits(objectFlags);
	u32 count = 0;
	u64 bit;
	for (bit = 1; bit != 0; bit <<= 1) {
		if ((bits & bit) != 0)
			count++;
	}
	return count;
}

static u32 asfCalcCodecWorkSize(const SceAsfParser *parser)
{
	u32 count = asfMaxStreamsFromFlags(parser->objectFlags) + 1;
	return (asfCodecInfoSize(parser->codecFlags) + asfCodecCtxSize(parser->codecFlags)) * count;
}

static u32 asfCalcObjTableSize(u64 objectFlags)
{
	return (asfMaxStreamsFromFlags(objectFlags) + asfCountSingleObjs(objectFlags)) * sizeof(SceAsfObjEntry);
}

static u32 asfCalcDrmHeaderBufSize(const SceAsfParser *parser)
{
	return asfIsDrmEnabled(parser) ? ASF_DRM_HEADER_BUF_SIZE : 0;
}

static u32 asfCalcDrmDecryptBufSize(const SceAsfParser *parser)
{
	return asfIsDrmEnabled(parser) ? ASF_DRM_DECRYPT_BUF_SIZE : 0;
}

u32 asfCalcNeedMem(const SceAsfParser *parser)
{
	return asfCalcCodecWorkSize(parser)
	     + asfCalcObjTableSize(parser->objectFlags)
	     + asfCalcDrmHeaderBufSize(parser)
	     + asfCalcDrmDecryptBufSize(parser);
}

void *asfMemmove(void *dst, const void *src, s32 len)
{
	u8 *d = (u8 *)dst;
	const u8 *s = (const u8 *)src;
	if (d == NULL)
		return NULL;
	if (d < s) {
		while (len > 0) {
			*d++ = *s++;
			len--;
		}
	} else if (s < d) {
		d += len;
		s += len;
		while (len > 0) {
			*d-- = *s--;
			len--;
		}
	}
	return dst;
}

static s32 asfValidateObjMask(const SceAsfParser *parser, u64 objMask)
{
	if (objMask != asfSingleObjBits(objMask))
		return SCE_ASF_ERROR_INVALID_OBJECT_MASK;
	u32 bits = asfCountSingleObjs(objMask);
	if (bits != 1)
		return SCE_ASF_ERROR_INVALID_OBJECT_MASK;
	if ((parser->objectFlags & objMask) == 0)
		return SCE_ASF_ERROR_OBJECT_NOT_ENABLED;
	return 0;
}

s32 asfGetStreamObjEntry(const SceAsfParser *parser, u32 streamIdx, SceAsfObjEntry **outEntry)
{
	if (streamIdx >= asfMaxStreamsFromFlags(parser->objectFlags))
		return SCE_ASF_ERROR_STREAM_IDX_OUT_OF_RANGE;
	u32 baseSingleCount = asfCountSingleObjs(parser->objectFlags & ASF_OBJ_MASK_FILE_PROP);
	*outEntry = &parser->objTable[baseSingleCount + streamIdx];
	return 0;
}

s32 asfGetHeaderObjEntry(const SceAsfParser *parser, u64 objMask, SceAsfObjEntry **outEntry)
{
	s32 ret = asfValidateObjMask(parser, objMask);
	if (ret < 0)
		return ret;
	u64 lowerMask = (objMask - 1ULL) & parser->objectFlags;
	u32 idx = asfMaxStreamsFromFlags(lowerMask) + asfCountSingleObjs(lowerMask);
	*outEntry = &parser->objTable[idx];
	return 0;
}

s32 asfReadObjEntryInfo(const SceAsfObjEntry *entry, u64 *outOffset, u64 *outSize)
{
	if (entry->count == 0)
		return SCE_ASF_ERROR_OBJECT_NOT_FOUND;
	*outOffset = entry->offset;
	*outSize = entry->size;
	return 0;
}

static void asfClearObjEntry(SceAsfObjEntry *entry)
{
	entry->count = 0;
	entry->offset = 0;
	entry->size = 0;
}

static void asfInitDrmState(SceAsfParser *parser)
{
	parser->drm.drmStreamId = 0;
	sceKernelMemset(&parser->drm.session, 0, sizeof(SceAsfDrmSession));
	parser->drm.initialized = 0;
	sceKernelMemset(parser->drm.ivBuf, 0, 15);
	parser->drm.ivSize = 0;
	if (asfIsDrmEnabled(parser)) {
		sceKernelMemset(parser->drm.headerBuf, 0, asfCalcDrmHeaderBufSize(parser));
		sceKernelMemset(parser->drm.decryptBuf, 0, asfCalcDrmDecryptBufSize(parser));
	}
}

s32 asfInitWorkMem(SceAsfParser *parser)
{
	u32 i;
	s32 ret = 0;
	u8 *base = (u8 *)parser->workBuf;

	parser->streamCount = 0;
	for (i = 0; i < ASF_MAX_STREAMS; i++) {
		sceKernelMemset(&parser->streams[i], 0, sizeof(SceAsfStream));
		parser->streamById[i] = NULL;
	}

	parser->codecWorkBase = base;
	if (((u32)base & 7) != 0)
		ret = SCE_ASF_ERROR_UNALIGNED_WORKBUF;

	/* Assign per-stream codecInfo and codecCtx pointers */
	u32 maxStreams = asfMaxStreamsFromFlags(parser->objectFlags);
	u32 infoSz = asfCodecInfoSize(parser->codecFlags);
	u32 ctxSz = asfCodecCtxSize(parser->codecFlags);
	u8 *ptr = base;
	for (i = 0; i < maxStreams; i++) {
		parser->streams[i].codecInfo = ptr;
		parser->streams[i].codecCtx = ptr + infoSz;
		ptr += infoSz + ctxSz;
	}
	parser->tempStream.codecInfo = ptr;
	parser->tempStream.codecCtx = ptr + infoSz;

	u32 codecBytes = asfCalcCodecWorkSize(parser);
	parser->objTable = (SceAsfObjEntry *)(base + codecBytes);
	u32 off = codecBytes + asfCalcObjTableSize(parser->objectFlags);
	if (asfIsDrmEnabled(parser)) {
		parser->drm.headerBuf = base + off;
		parser->drm.decryptBuf = base + off + asfCalcDrmHeaderBufSize(parser);
		parser->drm.decryptBufSize = asfCalcDrmDecryptBufSize(parser);
	}

	if (ret < 0)
		return ret;

	/* Clear all indexed object entries */
	u64 bits = asfSingleObjBits(parser->objectFlags);
	u64 bit;
	for (bit = 1; bit != 0; bit <<= 1) {
		if ((bits & bit) != 0) {
			SceAsfObjEntry *entry;
			if (asfGetHeaderObjEntry(parser, bit, &entry) >= 0)
				asfClearObjEntry(entry);
		}
	}
	for (i = 0; i < maxStreams; i++) {
		SceAsfObjEntry *entry;
		if (asfGetStreamObjEntry(parser, i, &entry) >= 0)
			asfClearObjEntry(entry);
	}

	asfInitDrmState(parser);
	return 0;
}

/* GUID I/O and comparison */
static s32 asfReadGuid(SceAsfIoContext *io, u8 guid[16])
{
	u32 d1;
	u16 d2, d3;
	s32 ret;
	s32 i;

	ret = asfIoReadU32(io, &d1);
	if (ret < 0)
		return ret;
	guid[0] = (u8)(d1 & 0xFF);
	guid[1] = (u8)((d1 >> 8) & 0xFF);
	guid[2] = (u8)((d1 >> 16) & 0xFF);
	guid[3] = (u8)((d1 >> 24) & 0xFF);

	ret = asfIoReadU16(io, &d2);
	if (ret < 0)
		return ret;
	guid[4] = (u8)(d2 & 0xFF);
	guid[5] = (u8)((d2 >> 8) & 0xFF);

	ret = asfIoReadU16(io, &d3);
	if (ret < 0)
		return ret;
	guid[6] = (u8)(d3 & 0xFF);
	guid[7] = (u8)((d3 >> 8) & 0xFF);

	for (i = 0; i < 8; i++) {
		ret = asfIoReadU8(io, &guid[8 + i]);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static s32 asfCompareGuid(const u8 *a, const u8 *b)
{
	s32 i;
	for (i = 0; i < 16; i++) {
		if (a[i] != b[i])
			return (a[i] < b[i]) ? -1 : 1;
	}
	return 0;
}

static u16 asfStreamTypeFromGuid(const u8 *guid)
{
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_AUDIO]) == 0)
		return ASF_STREAM_TYPE_AUDIO;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_VIDEO]) == 0)
		return ASF_STREAM_TYPE_VIDEO;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_COMMAND]) == 0)
		return ASF_STREAM_TYPE_COMMAND;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_JFIF]) == 0)
		return ASF_STREAM_TYPE_JFIF;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_DEGRAD_JPEG]) == 0)
		return ASF_STREAM_TYPE_DEGRAD_JPEG;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_FILE_XFER]) == 0)
		return ASF_STREAM_TYPE_FILE_XFER;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_BINARY]) == 0)
		return ASF_STREAM_TYPE_BINARY;
	if (asfCompareGuid(guid, g_asfGuids[GUID_MEDIA_DRM]) == 0)
		return ASF_STREAM_TYPE_DRM_MEDIA;
	return ASF_STREAM_TYPE_UNKNOWN;
}

static s32 asfCheckObjMinSize(u64 objMask, u64 objSize)
{
	u32 minSize = 24;
	s32 errCode = SCE_ASF_ERROR_UNKNOWN_OBJ_SIZE;
	u32 i;

	for (i = 0; i < 15; i++) {
		if ((g_asfMinSizeTable[i].mask & objMask) != 0) {
			minSize = g_asfMinSizeTable[i].minSize;
			errCode = g_asfMinSizeTable[i].errCode;
			break;
		}
	}
	if (objSize < (u64)minSize)
		return errCode;
	return 0;
}

static void asfRecordObjEntry(SceAsfObjEntry *entry, u64 objOffset, u64 objSize)
{
	entry->count++;
	if (entry->count == 1) {
		entry->offset = objOffset;
		entry->size = objSize;
	}
}

static s32 asfReadAndRecordStreamObj(SceAsfParser *parser, u32 streamIdx, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	s32 ret = asfIoReadU64(io, outSize);
	if (ret < 0)
		return ret;

	s64 pos = asfIoTell(io);
	if (pos < 0)
		return (s32)pos;

	ret = asfCheckObjMinSize(ASF_OBJ_MASK_STREAM_PROP, *outSize);
	if (ret < 0)
		return ret;

	SceAsfObjEntry *entry;
	if (asfGetStreamObjEntry(parser, streamIdx, &entry) >= 0)
		asfRecordObjEntry(entry, (u64)(pos - 24), *outSize);
	return 0;
}

static s32 asfReadAndRecordHeaderObj(SceAsfParser *parser, u64 objMask, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	s32 ret = asfIoReadU64(io, outSize);
	if (ret < 0)
		return ret;

	s64 pos = asfIoTell(io);
	if (pos < 0)
		return (s32)pos;

	ret = asfCheckObjMinSize(objMask, *outSize);
	if (ret < 0)
		return ret;

	SceAsfObjEntry *entry;
	if (asfGetHeaderObjEntry(parser, objMask, &entry) >= 0)
		asfRecordObjEntry(entry, (u64)(pos - 24), *outSize);
	return 0;
}

static s32 asfSkipUnknownObj(SceAsfIoContext *io, u64 *outSize)
{
	s32 ret = asfIoReadU64(io, outSize);
	if (ret < 0)
		return ret;
	if (*outSize < 24)
		return SCE_ASF_ERROR_UNKNOWN_OBJ_SIZE;
	return asfIoSkip(io, (s64)(*outSize - 24));
}

/* Parse Content Encryption Object (0x40000) */
static s32 asfParseContentEncryption(SceAsfParser *parser, u64 *remSize)
{
	SceAsfDrmSession *sess = &parser->drm.session;
	SceAsfIoContext *io = &parser->io;
	s32 ret;

	if (!asfIsDrmEnabled(parser) || (parser->objectFlags & ASF_OBJ_MASK_CONTENT_ENC) == 0)
		return 0;
	if (sess->encType == 2 && sess->contentEncSize != 0)
		return 0;

	u32 off = 0;

	ret = asfIoReadU32(io, &sess->contentEncSize);
	if (ret < 0)
		return ret;
	*remSize -= 4;
	sess->contentEncData = parser->drm.headerBuf + off;
	ret = asfIoReadBytes(io, sess->contentEncData, sess->contentEncSize);
	if (ret < 0)
		return ret;
	*remSize -= sess->contentEncSize;
	off += sess->contentEncSize;

	ret = asfIoReadU32(io, &sess->protectionTypeSize);
	if (ret < 0)
		return ret;
	*remSize -= 4;
	sess->protectionType = parser->drm.headerBuf + off;
	ret = asfIoReadBytes(io, sess->protectionType, sess->protectionTypeSize);
	if (ret < 0)
		return ret;
	*remSize -= sess->protectionTypeSize;
	off += sess->protectionTypeSize;

	ret = asfIoReadU32(io, &sess->keyIdSize);
	if (ret < 0)
		return ret;
	*remSize -= 4;
	sess->keyId = parser->drm.headerBuf + off;
	ret = asfIoReadBytes(io, sess->keyId, sess->keyIdSize);
	if (ret < 0)
		return ret;
	*remSize -= sess->keyIdSize;
	off += sess->keyIdSize;

	ret = asfIoReadU32(io, &sess->licenseUrlSize);
	if (ret < 0)
		return ret;
	*remSize -= 4;
	sess->licenseUrl = parser->drm.headerBuf + off;
	ret = asfIoReadBytes(io, sess->licenseUrl, sess->licenseUrlSize);
	if (ret < 0)
		return ret;
	*remSize -= sess->licenseUrlSize;

	sess->encType = 1;
	return 0;
}

/* Parse Extended Content Encryption Object (0x80000) */
static s32 asfParseExtContentEncryption(SceAsfParser *parser, u64 *remSize)
{
	SceAsfDrmSession *sess = &parser->drm.session;
	SceAsfIoContext *io = &parser->io;
	s32 ret;

	if (!asfIsDrmEnabled(parser) || (parser->objectFlags & ASF_OBJ_MASK_EXT_ENC) == 0)
		return 0;

	ret = asfIoReadU32(io, &sess->contentEncSize);
	if (ret < 0)
		return ret;
	*remSize -= 4;
	sess->contentEncData = parser->drm.headerBuf;
	ret = asfIoReadBytes(io, sess->contentEncData, sess->contentEncSize);
	if (ret < 0)
		return ret;
	*remSize -= sess->contentEncSize;
	sess->encType = 2;
	return 0;
}

static s32 asfParseSimpleHeaderObj(SceAsfParser *parser, u64 objMask, u64 *outSize)
{
	s32 ret = asfReadAndRecordHeaderObj(parser, objMask, outSize);
	if (ret < 0)
		return ret;

	u64 rem = *outSize - 24;
	if (objMask == ASF_OBJ_MASK_CONTENT_ENC)
		ret = asfParseContentEncryption(parser, &rem);
	else if (objMask == ASF_OBJ_MASK_EXT_ENC)
		ret = asfParseExtContentEncryption(parser, &rem);

	if (ret < 0)
		return ret;
	return asfIoSkip(&parser->io, (s64)rem);
}

/* Parse WMA v2 WAVEFORMATEX codec-specific data */
static s32 asfParseWmaWaveFormat(SceAsfWmaWaveFormat *fmt, SceAsfIoContext *io,
                                 u64 *bytesRead, u32 typeSpecLen, u32 errCorrLen)
{
	s32 ret;

	if (typeSpecLen >= 29)
		return SCE_ASF_ERROR_WMA_HEADER_TOO_LONG;

	ret = asfIoReadU16(io, &fmt->channels);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &fmt->sampleRate);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &fmt->avgBytesPerSec);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &fmt->blockAlign);
	if (ret < 0)
		return ret;
	/* Skip wBitsPerSample (2) + cbSize (2) + dwSamplesPerBlock (4) */
	ret = asfIoSkip(io, 8);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &fmt->encodeOptions);
	if (ret < 0)
		return ret;
	ret = asfIoSkip(io, (s64)(typeSpecLen + errCorrLen - 24));
	if (ret < 0)
		return ret;

	*bytesRead += (u64)(typeSpecLen + errCorrLen - 2);
	return 0;
}

static s32 asfParseAudioStreamFormat(SceAsfStream *stream, u64 codecFlags, SceAsfIoContext *io,
                                     u64 *bytesRead, u32 typeSpecLen, u32 errCorrLen)
{
	s32 ret;

	if (typeSpecLen < 18)
		return SCE_ASF_ERROR_AUDIO_HEADER_TOO_SHORT;

	ret = asfIoReadU16(io, &stream->codecId);
	if (ret < 0)
		return ret;
	*bytesRead += 2;

	if (stream->codecId == ASF_CODEC_ID_WMA_V2 && (codecFlags & ASF_CODEC_FLAG_WMA) != 0) {
		stream->stateFlags = 1;
		ret = asfParseWmaWaveFormat((SceAsfWmaWaveFormat *)stream->codecInfo, io,
		                            bytesRead, typeSpecLen, errCorrLen);
		if (ret >= 0)
			stream->stateFlags |= 2;
		return ret;
	}

	ret = asfIoSkip(io, (s64)(typeSpecLen + errCorrLen - 2));
	if (ret >= 0)
		*bytesRead += (u64)(typeSpecLen + errCorrLen - 2);
	return ret;
}

static s32 asfParseStreamSpecificData(SceAsfStream *stream, u64 codecFlags, SceAsfIoContext *io,
                                      u64 *bytesRead, u32 typeSpecLen, u32 errCorrLen)
{
	if (stream->streamType == ASF_STREAM_TYPE_AUDIO)
		return asfParseAudioStreamFormat(stream, codecFlags, io, bytesRead, typeSpecLen, errCorrLen);

	stream->codecId = 0;
	u32 totalSkip = typeSpecLen + errCorrLen;
	s32 ret = asfIoSkip(io, (s64)totalSkip);
	if (ret >= 0)
		*bytesRead += totalSkip;
	return ret;
}

static s32 asfParseStreamBody(SceAsfStream *stream, u64 codecFlags, SceAsfIoContext *io, u64 *bytesRead)
{
	u8 streamTypeGuid[16];
	u8 errCorrGuid[16];
	u64 timeOffset;
	u32 typeSpecLen;
	u32 errCorrLen;
	u16 flags;
	u32 reserved;
	s32 ret;

	ret = asfReadGuid(io, streamTypeGuid);
	if (ret < 0)
		return ret;
	stream->streamType = asfStreamTypeFromGuid(streamTypeGuid);

	ret = asfReadGuid(io, errCorrGuid);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &timeOffset);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &typeSpecLen);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &errCorrLen);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &flags);
	if (ret < 0)
		return ret;

	u32 streamNum = flags & 0x7F;
	stream->streamNumber = streamNum;
	if (streamNum == 0)
		return SCE_ASF_ERROR_INVALID_STREAM_ID;

	ret = asfIoReadU32(io, &reserved);
	if (ret < 0)
		return ret;

	*bytesRead += 54;
	return asfParseStreamSpecificData(stream, codecFlags, io, bytesRead, typeSpecLen, errCorrLen);
}

static s32 asfRegisterStream(SceAsfParser *parser, SceAsfStream *stream)
{
	u32 idx = stream->streamNumber - 1;
	if (parser->streamById[idx] != NULL)
		return SCE_ASF_ERROR_DUP_STREAM_ID;
	parser->streamById[idx] = stream;
	return 0;
}

/* Parse File Properties Object */
static s32 asfParseFileProperties(SceAsfParser *parser, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	s32 ret;

	ret = asfReadAndRecordHeaderObj(parser, ASF_OBJ_MASK_FILE_PROP, outSize);
	if (ret < 0)
		return ret;
	/* Skip File ID (16) + File Size (8) + Creation Date (8) = 32 bytes */
	ret = asfIoSkip(io, 32);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &parser->header.dataPacketsCount);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &parser->header.playDuration);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &parser->header.sendDuration);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &parser->header.preroll);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &parser->header.flags);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &parser->header.minPacketSize);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &parser->header.maxPacketSize);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &parser->header.maxBitrate);
	if (ret < 0)
		return ret;

	if (*outSize > 104)
		return asfIoSkip(io, (s64)(*outSize - 104));
	return 0;
}

/* Parse Stream Properties Object */
static s32 asfParseStreamProperties(SceAsfParser *parser, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	u64 bytesRead = 24;
	s32 ret;

	ret = asfReadAndRecordStreamObj(parser, parser->streamCount, outSize);
	if (ret < 0)
		return ret;

	u32 maxStreams = asfMaxStreamsFromFlags(parser->objectFlags);
	ret = asfParseStreamBody(&parser->tempStream, parser->codecFlags, io, &bytesRead);
	if (ret < 0)
		return ret;

	if (parser->tempStream.streamType == ASF_STREAM_TYPE_DRM_MEDIA)
		parser->drm.drmStreamId = (u16)parser->tempStream.streamNumber;

	if (parser->streamCount < maxStreams) {
		SceAsfStream *dst = &parser->streams[parser->streamCount];
		dst->streamNumber = parser->tempStream.streamNumber;
		dst->streamType = parser->tempStream.streamType;
		dst->codecId = parser->tempStream.codecId;
		dst->stateFlags = parser->tempStream.stateFlags;
		sceKernelMemcpy(dst->codecInfo, parser->tempStream.codecInfo, asfCodecInfoSize(parser->codecFlags));
		sceKernelMemcpy(dst->codecCtx, parser->tempStream.codecCtx, asfCodecCtxSize(parser->codecFlags));
		dst->reserved14 = parser->tempStream.reserved14;
		sceKernelMemcpy(&dst->pkt, &parser->tempStream.pkt, sizeof(SceAsfPacketState));
		parser->streamCount++;

		ret = asfRegisterStream(parser, dst);
		if (ret < 0)
			return ret;
	}

	if (bytesRead < *outSize)
		return asfIoSkip(io, (s64)(*outSize - bytesRead));
	return 0;
}

/* Extended Stream Properties Object helpers */
static s32 asfSkipStreamNames(SceAsfIoContext *io, u32 count, u64 *outBytes)
{
	u64 total = 0;
	u32 i;
	for (i = 0; i < count; i++) {
		u16 langIdx, nameLen;
		s32 ret = asfIoReadU16(io, &langIdx);
		if (ret < 0)
			return ret;
		ret = asfIoReadU16(io, &nameLen);
		if (ret < 0)
			return ret;
		ret = asfIoSkip(io, (s64)nameLen);
		if (ret < 0)
			return ret;
		total += (u64)nameLen + 4;
	}
	*outBytes = total;
	return 0;
}

static s32 asfSkipPayloadExtensions(SceAsfIoContext *io, u32 count, u64 *outBytes)
{
	u64 total = 0;
	u32 i;
	for (i = 0; i < count; i++) {
		u32 infoLen;
		/* Skip Extension System ID (16) + Data Size (2) = 18 bytes */
		s32 ret = asfIoSkip(io, 18);
		if (ret < 0)
			return ret;
		ret = asfIoReadU32(io, &infoLen);
		if (ret < 0)
			return ret;
		ret = asfIoSkip(io, (s64)infoLen);
		if (ret < 0)
			return ret;
		total += (u64)infoLen + 22;
	}
	*outBytes = total;
	return 0;
}

static s32 asfParseExtStreamProperties(SceAsfParser *parser, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	u16 nameCount, extCount;
	u64 subBytes, innerSize;
	s32 ret;

	ret = asfIoReadU64(io, outSize);
	if (ret < 0)
		return ret;
	if (*outSize < 88)
		return SCE_ASF_ERROR_EXT_STREAM_PROP;

	ret = asfIoSkip(io, 60);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &nameCount);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &extCount);
	if (ret < 0)
		return ret;

	ret = asfSkipStreamNames(io, nameCount, &subBytes);
	if (ret < 0)
		return ret;
	u64 consumed = 88 + subBytes;
	if (*outSize < consumed)
		return SCE_ASF_ERROR_EXT_STREAM_PROP;

	ret = asfSkipPayloadExtensions(io, extCount, &subBytes);
	if (ret < 0)
		return ret;
	consumed += subBytes;
	if (*outSize < consumed)
		return SCE_ASF_ERROR_EXT_STREAM_PROP;

	if (consumed < *outSize) {
		/* Embedded Stream Properties Object: skip 16-byte GUID then parse body */
		ret = asfIoSkip(io, 16);
		if (ret < 0)
			return ret;
		ret = asfParseStreamProperties(parser, &innerSize);
		if (ret < 0)
			return ret;
		consumed += innerSize;
	}

	if (consumed != *outSize)
		return SCE_ASF_ERROR_EXT_STREAM_PROP;
	return 0;
}

static s32 asfParseHeaderExtSubObject(SceAsfParser *parser, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	u8 guid[16];
	u32 i;

	s32 ret = asfReadGuid(io, guid);
	if (ret < 0)
		return ret;

	if (asfCompareGuid(guid, g_asfGuids[GUID_EXT_STREAM_PROP]) == 0)
		return asfParseExtStreamProperties(parser, outSize);

	for (i = 20; i < 32; i++) {
		if (asfCompareGuid(guid, g_asfGuids[i]) == 0)
			return asfSkipUnknownObj(io, outSize);
	}
	return asfSkipUnknownObj(io, outSize);
}

static s32 asfParseHeaderExtension(SceAsfParser *parser, u64 *outSize)
{
	SceAsfIoContext *io = &parser->io;
	u32 dataSize;
	s32 ret;

	ret = asfReadAndRecordHeaderObj(parser, ASF_OBJ_MASK_HEADER_EXT, outSize);
	if (ret < 0)
		return ret;
	/* Skip Reserved Field 1 (16) + Reserved Field 2 (2) = 18 bytes */
	ret = asfIoSkip(io, 18);
	if (ret < 0)
		return ret;
	ret = asfIoReadU32(io, &dataSize);
	if (ret < 0)
		return ret;

	u64 consumed = 0;
	while (consumed < (u64)dataSize) {
		u64 subSize = 0;
		ret = asfParseHeaderExtSubObject(parser, &subSize);
		if (ret < 0)
			return ret;
		consumed += subSize;
	}
	if (consumed != (u64)dataSize)
		return SCE_ASF_ERROR_HEADER_SIZE;
	return 0;
}

static s32 asfParseHeaderChildObject(SceAsfParser *parser, u64 *outSize, u32 *filePropCount)
{
	SceAsfIoContext *io = &parser->io;
	u8 guid[16];
	u32 i;

	*outSize = 0;
	s32 ret = asfReadGuid(io, guid);
	if (ret < 0)
		return ret;

	if (asfCompareGuid(guid, g_asfGuids[GUID_FILE_PROPERTIES]) == 0) {
		ret = asfParseFileProperties(parser, outSize);
		(*filePropCount)++;
		return ret;
	}
	if (asfCompareGuid(guid, g_asfGuids[GUID_STREAM_PROPERTIES]) == 0 ||
	    asfCompareGuid(guid, g_asfGuids[GUID_EXT_STREAM_ALT]) == 0) {
		return asfParseStreamProperties(parser, outSize);
	}
	if (asfCompareGuid(guid, g_asfGuids[GUID_HEADER_EXTENSION]) == 0)
		return asfParseHeaderExtension(parser, outSize);

	for (i = 0; i < 12; i++) {
		if (asfCompareGuid(guid, g_asfSimpleObjTable[i].guid) == 0)
			return asfParseSimpleHeaderObj(parser, g_asfSimpleObjTable[i].mask, outSize);
	}

	return asfSkipUnknownObj(io, outSize);
}

s32 asfParseHeaderObject(SceAsfParser *parser)
{
	SceAsfIoContext *io = &parser->io;
	u8 guid[16];
	u64 headerSize;
	u32 numSubObjs;
	u32 filePropCount = 0;
	s32 ret;

	ret = asfReadGuid(io, guid);
	if (ret < 0)
		return ret;
	if (asfCompareGuid(guid, g_asfGuids[GUID_HEADER]) != 0)
		return SCE_ASF_ERROR_INVALID_HEADER_GUID;

	ret = asfIoReadU64(io, &headerSize);
	if (ret < 0)
		return ret;
	if (headerSize < 30)
		return SCE_ASF_ERROR_HEADER_SIZE;

	ret = asfIoReadU32(io, &numSubObjs);
	if (ret < 0)
		return ret;
	/* Skip Reserved1 (1) + Reserved2 (1) */
	ret = asfIoSkip(io, 2);
	if (ret < 0)
		return ret;

	u64 consumed = 30;
	while (consumed < headerSize) {
		u64 childSize = 0;
		ret = asfParseHeaderChildObject(parser, &childSize, &filePropCount);
		if (ret < 0)
			return ret;
		consumed += childSize;
	}
	if (consumed != headerSize)
		return SCE_ASF_ERROR_HEADER_SIZE;

	parser->header.headerSize = headerSize;
	if (filePropCount == 0)
		return SCE_ASF_ERROR_NO_FILE_PROPERTIES;
	if (filePropCount >= 2)
		return SCE_ASF_ERROR_DUP_FILE_PROPERTIES;

	if (asfIsDrmEnabled(parser) && parser->drm.session.contentEncSize != 0) {
		parser->drm.session.userArg = parser->drm.userArg;
		if (parser->drm.initCb != NULL) {
			ret = parser->drm.initCb(&parser->drm.session);
			if (ret >= 0)
				parser->drm.initialized = 1;
		}
	}
	return ret;
}

s32 asfParseDataObject(SceAsfParser *parser)
{
	SceAsfIoContext *io = &parser->io;
	u8 guid[16];
	u8 fileId[16];
	u64 dataObjSize;
	u64 totalPackets;
	u16 reserved;
	s32 ret;

	ret = asfReadGuid(io, guid);
	if (ret < 0)
		return ret;
	if (asfCompareGuid(guid, g_asfGuids[GUID_DATA]) != 0)
		return SCE_ASF_ERROR_DATA_OBJ_GUID;

	ret = asfIoReadU64(io, &dataObjSize);
	if (ret < 0)
		return ret;
	if (dataObjSize < 50)
		return SCE_ASF_ERROR_DATA_OBJ_INVALID;

	ret = asfReadGuid(io, fileId);
	if (ret < 0)
		return ret;
	ret = asfIoReadU64(io, &totalPackets);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &reserved);
	if (ret < 0)
		return ret;
	if (reserved != 0x0101)
		return SCE_ASF_ERROR_DATA_OBJ_INVALID;

	if (dataObjSize >= 51) {
		ret = asfIoSkip(io, (s64)(dataObjSize - 50));
		if (ret < 0)
			return ret;
	}

	if (parser->header.dataPacketsCount != totalPackets)
		ret = SCE_ASF_ERROR_DATA_PACKET_MISMATCH;
	parser->header.dataSize = dataObjSize;
	return ret;
}

/* Stream codec lifecycle and frame dispatch */
static s32 asfRestoreStreamFilePos(SceAsfStream *stream)
{
	s64 res = asfIoSeek(stream->pkt.io, stream->pkt.savedFilePos, ASF_SEEK_SET);
	if (res < 0)
		return (s32)res;
	return 0;
}

static s32 asfSaveStreamFilePos(SceAsfStream *stream)
{
	s64 pos = asfIoTell(stream->pkt.io);
	stream->pkt.savedFilePos = pos;
	if (pos < 0)
		return (s32)pos;
	return 0;
}

s32 asfInitAllCodecs(SceAsfParser *parser)
{
	u32 i;
	for (i = 0; i < parser->streamCount; i++) {
		SceAsfStream *stream = &parser->streams[i];
		if (stream->streamType == ASF_STREAM_TYPE_AUDIO &&
		    stream->codecId == ASF_CODEC_ID_WMA_V2 &&
		    (parser->codecFlags & ASF_CODEC_FLAG_WMA) != 0) {
			s32 ret = wmaInitParser((SceWmaParserCtx *)stream->codecCtx,
			                        (const SceAsfWmaWaveFormat *)stream->codecInfo);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

s32 asfReadNextFrame(SceAsfStream *stream, u64 codecFlags, SceAsfWmaFrame *frame)
{
	s32 ret = asfRestoreStreamFilePos(stream);
	if (ret < 0)
		return ret;

	if (stream->streamType != ASF_STREAM_TYPE_AUDIO ||
	    stream->codecId != ASF_CODEC_ID_WMA_V2 ||
	    (codecFlags & ASF_CODEC_FLAG_WMA) == 0) {
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;
	}

	SceAsfWmaWaveFormat *fmt = (SceAsfWmaWaveFormat *)stream->codecInfo;
	stream->pkt.drm->blockAlign = fmt->blockAlign;
	ret = wmaReadNextFrame((SceWmaParserCtx *)stream->codecCtx, stream, frame);
	if (ret >= 0)
		return asfSaveStreamFilePos(stream);
	return ret;
}

s32 asfSeekStreamFrame(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	s32 ret = asfRestoreStreamFilePos(stream);
	if (ret < 0)
		return ret;

	if (stream->streamType != ASF_STREAM_TYPE_AUDIO ||
	    stream->codecId != ASF_CODEC_ID_WMA_V2 ||
	    (codecFlags & ASF_CODEC_FLAG_WMA) == 0) {
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;
	}

	SceAsfWmaWaveFormat *fmt = (SceAsfWmaWaveFormat *)stream->codecInfo;
	stream->pkt.drm->blockAlign = fmt->blockAlign;
	ret = wmaSeekFrame((SceWmaParserCtx *)stream->codecCtx, stream, timeMs);
	if (ret >= 0)
		return asfSaveStreamFilePos(stream);
	return ret;
}

void asfGetStreamTime(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	if (stream->streamType == ASF_STREAM_TYPE_AUDIO &&
	    stream->codecId == ASF_CODEC_ID_WMA_V2 &&
	    (codecFlags & ASF_CODEC_FLAG_WMA) != 0) {
		wmaGetCurTime((const SceWmaParserCtx *)stream->codecCtx, timeMs);
	} else {
		*timeMs = 0;
	}
}

s32 asfSetDrmCallbacks(SceAsfParser *parser, const SceAsfDrmCallbacks *cbs)
{
	if (cbs->initCb == NULL || cbs->setIvCb == NULL ||
	    cbs->decryptCb == NULL || cbs->termCb == NULL ||
	    cbs->userArg == NULL) {
		return SCE_ASF_ERROR_DRM_INVALID_ARG;
	}
	parser->drm.initCb = cbs->initCb;
	parser->drm.setIvCb = cbs->setIvCb;
	parser->drm.decryptCb = cbs->decryptCb;
	parser->drm.termCb = cbs->termCb;
	parser->drm.userArg = cbs->userArg;
	return 0;
}
