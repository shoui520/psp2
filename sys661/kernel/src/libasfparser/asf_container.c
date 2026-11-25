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

/* Forward declarations matching Sony's definition order */
static u32 asfMaxStreamsFromFlags(u64 objectFlags);
static u64 asfSingleObjBits(u64 objectFlags);
static u32 asfCodecInfoSize(u64 codecFlags);
static u32 asfCodecCtxSize(u64 codecFlags);
static u32 asfCountSingleObjs(u64 objectFlags);
static u32 asfCalcSingleObjTableSize(u64 objectFlags);
static u32 asfCalcObjTableSize(u64 objectFlags);
static s32 asfValidateObjMask(const SceAsfParser *parser, u64 objMask);

u32 __attribute__((noinline)) asfExtractStreamId(u32 flags)
{
	return flags & 0x7F;
}

s32 asfIsDrmEnabled(const SceAsfParser *parser)
{
	if ((parser->drmEnableFlags & 1) == 0)
		return 0;
	return (parser->objectFlags & ASF_OBJ_MASK_DRM_ANY) != 0;
}

static u32 __attribute__((noinline)) asfCalcCodecWorkSize(const SceAsfParser *parser)
{
	u32 count = asfMaxStreamsFromFlags(parser->objectFlags) + 1;
	u32 infoSz = asfCodecInfoSize(parser->codecFlags);
	u32 ctxSz = asfCodecCtxSize(parser->codecFlags);
	return (infoSz + ctxSz) * count;
}

static u32 __attribute__((noinline)) asfCalcStreamObjTableSize(u64 objectFlags)
{
	return asfMaxStreamsFromFlags(objectFlags) * sizeof(SceAsfObjEntry);
}

static u32 __attribute__((noinline)) asfCalcSingleObjTableSize(u64 objectFlags)
{
	return asfCountSingleObjs(objectFlags) * sizeof(SceAsfObjEntry);
}

static u32 __attribute__((noinline)) asfCalcObjTableSize(u64 objectFlags)
{
	return asfCalcStreamObjTableSize(objectFlags) + asfCalcSingleObjTableSize(objectFlags);
}

static u32 __attribute__((noinline)) asfCalcDrmHeaderBufSize(const SceAsfParser *parser)
{
	return asfIsDrmEnabled(parser) ? ASF_DRM_HEADER_BUF_SIZE : 0;
}

static u32 __attribute__((noinline)) asfCalcDrmDecryptBufSize(const SceAsfParser *parser)
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

s32 asfGetStreamObjEntry(const SceAsfParser *parser, u32 streamIdx, SceAsfObjEntry **outEntry)
{
	if (streamIdx >= asfMaxStreamsFromFlags(parser->objectFlags))
		return SCE_ASF_ERROR_STREAM_IDX_OUT_OF_RANGE;
	u32 baseBytes = asfCalcSingleObjTableSize(parser->objectFlags & ASF_OBJ_MASK_FILE_PROP);
	u8 *tableBytes = (u8 *)parser->objTable;
	*outEntry = (SceAsfObjEntry *)(tableBytes + baseBytes) + streamIdx;
	return 0;
}

s32 asfGetHeaderObjEntry(const SceAsfParser *parser, u64 objMask, SceAsfObjEntry **outEntry)
{
	s32 ret = asfValidateObjMask(parser, objMask);
	if (ret < 0)
		return ret;
	u64 lowerMask = (objMask - 1ULL) & parser->objectFlags;
	u32 byteOff = asfCalcObjTableSize(lowerMask);
	u8 *tableBytes = (u8 *)parser->objTable;
	*outEntry = (SceAsfObjEntry *)(tableBytes + byteOff);
	return ret;
}

static u32 __attribute__((noinline)) asfCodecInfoSize(u64 codecFlags)
{
	return (codecFlags & ASF_CODEC_FLAG_WMA) ? sizeof(SceAsfWmaWaveFormat) : 0;
}

static u32 __attribute__((noinline)) asfCodecCtxSize(u64 codecFlags)
{
	return (codecFlags & ASF_CODEC_FLAG_WMA) ? sizeof(SceWmaParserCtx) : 0;
}

static u32 __attribute__((noinline)) asfCountSingleObjs(u64 objectFlags)
{
	u64 bits = asfSingleObjBits(objectFlags);
	u32 count = 0;
	u64 bit;
	for (bit = 1; bit != 0; bit <<= 1) {
		u32 lo = (u32)(bits & bit);
		u32 hi = (u32)((bits & bit) >> 32);
		count += ((lo | hi) != 0);
	}
	return count;
}

static s32 __attribute__((noinline)) asfValidateObjMask(const SceAsfParser *parser, u64 objMask)
{
	if (objMask != asfSingleObjBits(objMask))
		return SCE_ASF_ERROR_INVALID_OBJECT_MASK;
	u32 bits = asfCountSingleObjs(objMask);
	if (bits == 0)
		return SCE_ASF_ERROR_INVALID_OBJECT_MASK;
	s32 ret = (bits < 2) ? 0 : SCE_ASF_ERROR_INVALID_OBJECT_MASK;
	if ((parser->objectFlags & objMask) == 0)
		ret = SCE_ASF_ERROR_OBJECT_NOT_ENABLED;
	return ret;
}

static u32 __attribute__((noinline)) asfMaxStreamsFromFlags(u64 objectFlags)
{
	return (u32)((objectFlags & 0xFEULL) >> 1);
}

static u64 __attribute__((noinline)) asfSingleObjBits(u64 objectFlags)
{
	return objectFlags & ASF_OBJ_MASK_SINGLE_VALID;
}

s32 asfReadObjEntryInfo(const SceAsfObjEntry *entry, u64 *outOffset, u64 *outSize)
{
	if (entry->count == 0)
		return SCE_ASF_ERROR_OBJECT_NOT_FOUND;
	*outOffset = entry->offset;
	*outSize = entry->size;
	return 0;
}

static void __attribute__((noinline)) asfClearObjEntry(SceAsfObjEntry *entry)
{
	entry->count = 0;
	entry->offset = 0;
	entry->size = 0;
}

static void __attribute__((noinline)) asfClearHeaderObjEntry(const SceAsfParser *parser, u64 objMask)
{
	SceAsfObjEntry *entry;
	if (asfGetHeaderObjEntry(parser, objMask, &entry) >= 0)
		asfClearObjEntry(entry);
}

static void __attribute__((noinline)) asfClearStreamObjEntry(const SceAsfParser *parser, u32 streamIdx)
{
	SceAsfObjEntry *entry;
	if (asfGetStreamObjEntry(parser, streamIdx, &entry) >= 0)
		asfClearObjEntry(entry);
}

static void __attribute__((noinline)) asfClearAllHeaderObjEntries(const SceAsfParser *parser)
{
	u64 bits = asfSingleObjBits(parser->objectFlags);
	u64 bit;
	for (bit = 1; bit != 0; bit <<= 1) {
		if ((bits & bit) != 0)
			asfClearHeaderObjEntry(parser, bit);
	}
}

static void __attribute__((noinline)) asfClearAllStreamObjEntries(const SceAsfParser *parser)
{
	u32 maxStreams = asfMaxStreamsFromFlags(parser->objectFlags);
	u32 i;
	for (i = 0; i < maxStreams; i++)
		asfClearStreamObjEntry(parser, i);
}

static s32 __attribute__((noinline)) asfInitDrmState(SceAsfParser *parser)
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
	return 0;
}

static void __attribute__((noinline)) asfResetParsedState(SceAsfParser *parser)
{
	asfClearAllHeaderObjEntries(parser);
	asfClearAllStreamObjEntries(parser);
	asfInitDrmState(parser);
}

static void __attribute__((noinline)) asfAssignStreamWorkMem(SceAsfParser *parser)
{
	u32 maxStreams = asfMaxStreamsFromFlags(parser->objectFlags);
	u32 infoSz = asfCodecInfoSize(parser->codecFlags);
	u32 ctxSz = asfCodecCtxSize(parser->codecFlags);
	u8 *ptr = (u8 *)parser->codecWorkBase;
	u32 i;
	for (i = 0; i < maxStreams; i++) {
		parser->streams[i].codecInfo = ptr;
		parser->streams[i].codecCtx = ptr + infoSz;
		ptr += infoSz + ctxSz;
	}
	parser->tempStream.codecInfo = ptr;
	parser->tempStream.codecCtx = ptr + infoSz;
}

static s32 __attribute__((noinline)) asfAssignWorkMem(SceAsfParser *parser)
{
	u8 *base = (u8 *)parser->workBuf;
	s32 ret = (((u32)base & 7) != 0) ? SCE_ASF_ERROR_UNALIGNED_WORKBUF : 0;

	parser->codecWorkBase = base;
	asfAssignStreamWorkMem(parser);

	u32 codecBytes = asfCalcCodecWorkSize(parser);
	parser->objTable = (SceAsfObjEntry *)(base + codecBytes);
	u32 off = codecBytes + asfCalcObjTableSize(parser->objectFlags);
	if (asfIsDrmEnabled(parser)) {
		parser->drm.headerBuf = base + off;
		parser->drm.decryptBuf = base + off + asfCalcDrmHeaderBufSize(parser);
		parser->drm.decryptBufSize = asfCalcDrmDecryptBufSize(parser);
	}
	return ret;
}

static void __attribute__((noinline)) asfCopyCodecInfo(const SceAsfParser *parser, void *dst, const void *src)
{
	sceKernelMemcpy(dst, src, asfCodecInfoSize(parser->codecFlags));
}

static void __attribute__((noinline)) asfCopyCodecCtx(const SceAsfParser *parser, void *dst, const void *src)
{
	sceKernelMemcpy(dst, src, asfCodecCtxSize(parser->codecFlags));
}

s32 asfInitWorkMem(SceAsfParser *parser)
{
	u32 i;

	parser->streamCount = 0;
	for (i = 0; i < ASF_MAX_STREAMS; i++) {
		sceKernelMemset(&parser->streams[i], 0, sizeof(SceAsfStream));
		parser->streamById[i] = NULL;
	}

	s32 ret = asfAssignWorkMem(parser);
	if (ret < 0)
		return ret;

	asfResetParsedState(parser);
	return 0;
}

/* GUID I/O and comparison */
static s32 __attribute__((noinline)) asfReadGuid(SceAsfIoContext *io, SceAsfGuid *guid)
{
	s32 ret;
	s32 i;

	ret = asfIoReadU32(io, &guid->timeLow);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &guid->timeMid);
	if (ret < 0)
		return ret;
	ret = asfIoReadU16(io, &guid->timeHiAndVer);
	if (ret < 0)
		return ret;
	for (i = 0; i < 8; i++) {
		ret = asfIoReadU8(io, &guid->clockSeqAndNode[i]);
		if (ret < 0)
			return ret;
	}
	return ret;
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

static void __attribute__((noinline)) asfLookupObjMinSize(u64 objMask, u32 *outMinSize, s32 *outErrCode)
{
	u32 i;
	*outMinSize = 24;
	*outErrCode = SCE_ASF_ERROR_UNKNOWN_OBJ_SIZE;

	for (i = 0; i < 15; i++) {
		if ((g_asfMinSizeTable[i].mask & objMask) != 0) {
			*outMinSize = g_asfMinSizeTable[i].minSize;
			*outErrCode = g_asfMinSizeTable[i].errCode;
			return;
		}
	}
}

static s32 __attribute__((noinline)) asfCheckObjMinSize(u64 objMask, u64 objSize)
{
	u32 minSize;
	s32 errCode;

	asfLookupObjMinSize(objMask, &minSize, &errCode);
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
	ret = asfGetStreamObjEntry(parser, streamIdx, &entry);
	if (ret < 0)
		return 0;
	asfRecordObjEntry(entry, (u64)(pos - 24), *outSize);
	return ret;
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
	ret = asfGetHeaderObjEntry(parser, objMask, &entry);
	if (ret < 0)
		return 0;
	asfRecordObjEntry(entry, (u64)(pos - 24), *outSize);
	return ret;
}

static s32 __attribute__((noinline)) asfSkipGenericObj(SceAsfIoContext *io, u64 *outSize)
{
	s32 ret = asfIoReadU64(io, outSize);
	if (ret < 0)
		return ret;
	if (*outSize < 24)
		return SCE_ASF_ERROR_UNKNOWN_OBJ_SIZE;
	return asfIoSkip(io, (s64)(*outSize - 24));
}

static s32 __attribute__((noinline)) asfSkipPaddingObj(SceAsfIoContext *io, const SceAsfGuid *guid, u64 *outSize)
{
	(void)guid;
	return asfSkipGenericObj(io, outSize);
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
	return ret;
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
	return ret;
}

static s32 __attribute__((noinline)) asfParseEncryptionBody(SceAsfParser *parser, u64 objMask, u64 *remSize)
{
	if (objMask == ASF_OBJ_MASK_CONTENT_ENC)
		return asfParseContentEncryption(parser, remSize);
	if (objMask == ASF_OBJ_MASK_EXT_ENC)
		return asfParseExtContentEncryption(parser, remSize);
	return 0;
}

static s32 __attribute__((noinline)) asfParseSimpleHeaderObj(SceAsfParser *parser, u64 objMask, u64 *outSize)
{
	s32 ret = asfReadAndRecordHeaderObj(parser, objMask, outSize);
	if (ret < 0)
		return ret;

	u64 rem = *outSize - 24;
	ret = asfParseEncryptionBody(parser, objMask, &rem);
	if (ret < 0)
		return ret;
	return asfIoSkip(&parser->io, (s64)rem);
}

/* Parse WMA version 2 WAVEFORMATEX codec-specific data */
static s32 __attribute__((noinline)) asfParseWmaWaveFormat(SceAsfWmaWaveFormat *fmt, SceAsfIoContext *io,
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
	(void)asfGetMaxBlockAlign();
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
	return ret;
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
	SceAsfGuid streamTypeGuid;
	SceAsfGuid errCorrGuid;
	u64 timeOffset;
	u32 typeSpecLen;
	u32 errCorrLen;
	u16 flags;
	u32 reserved;
	s32 ret;

	ret = asfReadGuid(io, &streamTypeGuid);
	if (ret < 0)
		return ret;
	stream->streamType = asfStreamTypeFromGuid((const u8 *)&streamTypeGuid);

	ret = asfReadGuid(io, &errCorrGuid);
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

	u32 streamNum = asfExtractStreamId(flags);
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
		asfCopyCodecInfo(parser, dst->codecInfo, parser->tempStream.codecInfo);
		asfCopyCodecCtx(parser, dst->codecCtx, parser->tempStream.codecCtx);
		dst->reserved = parser->tempStream.reserved;
		sceKernelMemcpy(&dst->pkt, &parser->tempStream.pkt, sizeof(SceAsfPacketState));

		u32 curIdx = parser->streamCount;
		parser->streamCount = curIdx + 1;

		ret = asfRegisterStream(parser, &parser->streams[curIdx]);
		if (ret < 0)
			return ret;
	}

	if (bytesRead < *outSize)
		return asfIoSkip(io, (s64)(*outSize - bytesRead));
	return ret;
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

typedef s32 (*AsfObjHandlerFn)(SceAsfParser *parser, u64 *outSize);

typedef struct AsfObjHandlerRule {
	const u8 *guid;
	AsfObjHandlerFn handler;
} AsfObjHandlerRule;

static const u8 *g_asfPaddingGuidPtr = g_asfGuids[GUID_PADDING];

static const u8 *g_asfHeaderExtSkipGuids[12] = {
	g_asfGuids[20], g_asfGuids[21], g_asfGuids[22], g_asfGuids[23],
	g_asfGuids[24], g_asfGuids[25], g_asfGuids[26], g_asfGuids[27],
	g_asfGuids[28], g_asfGuids[29], g_asfGuids[30], g_asfGuids[31],
};

static s32 __attribute__((noinline)) asfParseHeaderExtSubObject(SceAsfParser *parser, u64 *outSize)
{
	static AsfObjHandlerRule extHandler = {
		g_asfGuids[GUID_EXT_STREAM_PROP], asfParseExtStreamProperties
	};
	SceAsfIoContext *io = &parser->io;
	SceAsfGuid guid;
	u32 i;

	s32 ret = asfReadGuid(io, &guid);
	if (ret < 0)
		return ret;

	if (asfCompareGuid((const u8 *)&guid, extHandler.guid) == 0)
		return extHandler.handler(parser, outSize);

	for (i = 0; i < 12; i++) {
		if (asfCompareGuid((const u8 *)&guid, g_asfHeaderExtSkipGuids[i]) == 0)
			return asfSkipGenericObj(io, outSize);
	}
	return asfSkipPaddingObj(io, &guid, outSize);
}

static s32 __attribute__((noinline)) asfParseHeaderExtLoop(SceAsfParser *parser, u64 dataSize)
{
	u64 consumed = 0;
	s32 ret = 0;
	while (consumed < dataSize) {
		u64 subSize = 0;
		ret = asfParseHeaderExtSubObject(parser, &subSize);
		if (ret < 0)
			return ret;
		consumed += subSize;
	}
	if (consumed != dataSize)
		return SCE_ASF_ERROR_HEADER_SIZE;
	return ret;
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

	return asfParseHeaderExtLoop(parser, (u64)dataSize);
}

static s32 asfParseHeaderChildObject(SceAsfParser *parser, u64 *outSize, u32 *filePropCount)
{
	static AsfObjHandlerRule headerHandlers[4] = {
		{ g_asfGuids[GUID_FILE_PROPERTIES],   asfParseFileProperties },
		{ g_asfGuids[GUID_STREAM_PROPERTIES], asfParseStreamProperties },
		{ g_asfGuids[GUID_EXT_STREAM_ALT],    asfParseStreamProperties },
		{ g_asfGuids[GUID_HEADER_EXTENSION],  asfParseHeaderExtension },
	};
	SceAsfIoContext *io = &parser->io;
	SceAsfGuid guid;
	u32 i;

	*outSize = 0;
	s32 ret = asfReadGuid(io, &guid);
	if (ret < 0)
		return ret;

	for (i = 0; i < 4; i++) {
		if (asfCompareGuid((const u8 *)&guid, headerHandlers[i].guid) == 0) {
			AsfObjHandlerFn fn = headerHandlers[i].handler;
			ret = fn(parser, outSize);
			if (fn == asfParseFileProperties)
				(*filePropCount)++;
			return ret;
		}
	}

	for (i = 0; i < 12; i++) {
		if (asfCompareGuid((const u8 *)&guid, g_asfSimpleObjTable[i].guid) == 0)
			return asfParseSimpleHeaderObj(parser, g_asfSimpleObjTable[i].mask, outSize);
	}

	if (asfCompareGuid((const u8 *)&guid, g_asfPaddingGuidPtr) == 0)
		return asfSkipGenericObj(io, outSize);

	return asfSkipPaddingObj(io, &guid, outSize);
}

s32 asfParseHeaderObject(SceAsfParser *parser)
{
	SceAsfIoContext *io = &parser->io;
	SceAsfGuid guid;
	u64 headerSize;
	u32 numSubObjs;
	u32 filePropCount = 0;
	s32 ret;

	ret = asfReadGuid(io, &guid);
	if (ret < 0)
		return ret;
	if (asfCompareGuid((const u8 *)&guid, g_asfGuids[GUID_HEADER]) != 0)
		return SCE_ASF_ERROR_INVALID_HEADER_GUID;

	ret = asfIoReadU64(io, &headerSize);
	if (ret < 0)
		return ret;
	if (headerSize < 30)
		return SCE_ASF_ERROR_HEADER_SIZE;

	ret = asfIoReadU32(io, &numSubObjs);
	if (ret < 0)
		return ret;
	/* Skip header reserved bytes (2 bytes) */
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
	SceAsfGuid guid;
	SceAsfGuid fileId;
	u64 dataObjSize;
	u64 totalPackets;
	u16 reserved;
	s32 ret;

	ret = asfReadGuid(io, &guid);
	if (ret < 0)
		return ret;
	if (asfCompareGuid((const u8 *)&guid, g_asfGuids[GUID_DATA]) != 0)
		return SCE_ASF_ERROR_DATA_OBJ_GUID;

	ret = asfIoReadU64(io, &dataObjSize);
	if (ret < 0)
		return ret;
	if (dataObjSize < 50)
		return SCE_ASF_ERROR_DATA_OBJ_INVALID;

	ret = asfReadGuid(io, &fileId);
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
static s32 __attribute__((noinline)) asfRestoreStreamFilePos(SceAsfStream *stream)
{
	s64 res = asfIoSeek(stream->pkt.io, stream->pkt.savedFilePos, ASF_SEEK_SET);
	if (res < 0)
		return (s32)res;
	return 0;
}

static s32 __attribute__((noinline)) asfSaveStreamFilePos(SceAsfStream *stream)
{
	s64 pos = asfIoTell(stream->pkt.io);
	stream->pkt.savedFilePos = pos;
	if (pos < 0)
		return (s32)pos;
	return 0;
}

static void __attribute__((noinline)) asfClearTimeMs(u32 *timeMs)
{
	*timeMs = 0;
}

static s32 __attribute__((noinline)) asfInitAudioCodec(SceAsfStream *stream, u64 codecFlags)
{
	if (stream->codecId == ASF_CODEC_ID_WMA_V2 && (codecFlags & ASF_CODEC_FLAG_WMA) != 0)
		return wmaInitParser((SceWmaParserCtx *)stream->codecCtx,
		                     (const SceAsfWmaWaveFormat *)stream->codecInfo);
	return 0;
}

static s32 __attribute__((noinline)) asfInitStreamCodec(SceAsfStream *stream, u64 codecFlags)
{
	if (stream->streamType == ASF_STREAM_TYPE_AUDIO)
		return asfInitAudioCodec(stream, codecFlags);
	return 0;
}

s32 asfInitAllCodecs(SceAsfParser *parser)
{
	u32 i;
	for (i = 0; i < parser->streamCount; i++) {
		s32 ret = asfInitStreamCodec(&parser->streams[i], parser->codecFlags);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static s32 __attribute__((noinline)) asfReadAudioFrame(SceAsfStream *stream, u64 codecFlags, SceAsfWmaFrame *frame)
{
	if (stream->codecId != ASF_CODEC_ID_WMA_V2 || (codecFlags & ASF_CODEC_FLAG_WMA) == 0)
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;
	SceAsfWmaWaveFormat *fmt = (SceAsfWmaWaveFormat *)stream->codecInfo;
	stream->pkt.drm->blockAlign = fmt->blockAlign;
	return wmaReadNextFrame((SceWmaParserCtx *)stream->codecCtx, stream, frame);
}

s32 asfReadNextFrame(SceAsfStream *stream, u64 codecFlags, SceAsfWmaFrame *frame)
{
	s32 ret = asfRestoreStreamFilePos(stream);
	if (ret < 0)
		return ret;

	if (stream->streamType != ASF_STREAM_TYPE_AUDIO)
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;

	ret = asfReadAudioFrame(stream, codecFlags, frame);
	if (ret >= 0)
		return asfSaveStreamFilePos(stream);
	return ret;
}

static s32 __attribute__((noinline)) asfSeekAudioFrame(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	if (stream->codecId != ASF_CODEC_ID_WMA_V2 || (codecFlags & ASF_CODEC_FLAG_WMA) == 0)
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;
	SceAsfWmaWaveFormat *fmt = (SceAsfWmaWaveFormat *)stream->codecInfo;
	stream->pkt.drm->blockAlign = fmt->blockAlign;
	return wmaSeekFrame((SceWmaParserCtx *)stream->codecCtx, stream, timeMs);
}

s32 asfSeekStreamFrame(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	s32 ret = asfRestoreStreamFilePos(stream);
	if (ret < 0)
		return ret;

	if (stream->streamType != ASF_STREAM_TYPE_AUDIO)
		return SCE_ASF_ERROR_UNSUPPORTED_STREAM;

	ret = asfSeekAudioFrame(stream, codecFlags, timeMs);
	if (ret >= 0)
		return asfSaveStreamFilePos(stream);
	return ret;
}

static void __attribute__((noinline)) asfGetAudioStreamTime(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	if (stream->codecId == ASF_CODEC_ID_WMA_V2 && (codecFlags & ASF_CODEC_FLAG_WMA) != 0)
		wmaGetCurTime((const SceWmaParserCtx *)stream->codecCtx, stream, timeMs);
	else
		asfClearTimeMs(timeMs);
}

void asfGetStreamTime(SceAsfStream *stream, u64 codecFlags, u32 *timeMs)
{
	if (stream->streamType == ASF_STREAM_TYPE_AUDIO)
		asfGetAudioStreamTime(stream, codecFlags, timeMs);
	else
		asfClearTimeMs(timeMs);
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
