/*
 * PS Vita ePSP 6.61 libasfparser.prx (sceASFParser)
 * Internal structures, error codes, and prototypes.
 */
#ifndef ASF_INT_H
#define ASF_INT_H

#include <stddef.h>
#include <common/types.h>

/* Kernel_Library imports */
void *sceKernelMemset(void *dst, int c, u32 len);
void *sceKernelMemcpy(void *dst, const void *src, u32 len);

/* Error codes */
#define SCE_ASF_ERROR_INVALID_PARAM         ((s32)0x86400001)
#define SCE_ASF_ERROR_UNALIGNED_WORKBUF     ((s32)0x86400002)
#define SCE_ASF_ERROR_HEADER_SIZE           ((s32)0x86400101)
#define SCE_ASF_ERROR_OBJ_MIN_FILE_PROP     ((s32)0x86400111)
#define SCE_ASF_ERROR_OBJ_MIN_STREAM_PROP   ((s32)0x86400112)
#define SCE_ASF_ERROR_OBJ_MIN_HEADER_EXT    ((s32)0x86400113)
#define SCE_ASF_ERROR_OBJ_MIN_CODEC_LIST    ((s32)0x86400114)
#define SCE_ASF_ERROR_OBJ_MIN_SCRIPT_CMD    ((s32)0x86400115)
#define SCE_ASF_ERROR_OBJ_MIN_MARKER        ((s32)0x86400116)
#define SCE_ASF_ERROR_OBJ_MIN_BITRATE_MUTEX ((s32)0x86400117)
#define SCE_ASF_ERROR_OBJ_MIN_ERR_CORR      ((s32)0x86400118)
#define SCE_ASF_ERROR_OBJ_MIN_CONTENT_DESC  ((s32)0x86400119)
#define SCE_ASF_ERROR_OBJ_MIN_EXT_CONTENT   ((s32)0x86400120)
#define SCE_ASF_ERROR_OBJ_MIN_STREAM_BITR   ((s32)0x86400121)
#define SCE_ASF_ERROR_OBJ_MIN_CONTENT_BRAND ((s32)0x86400122)
#define SCE_ASF_ERROR_OBJ_MIN_CONTENT_ENC   ((s32)0x86400123)
#define SCE_ASF_ERROR_OBJ_MIN_EXT_ENC       ((s32)0x86400124)
#define SCE_ASF_ERROR_OBJ_MIN_DIG_SIG       ((s32)0x86400125)
#define SCE_ASF_ERROR_EXT_STREAM_PROP       ((s32)0x86400126)
#define SCE_ASF_ERROR_UNKNOWN_OBJ_SIZE      ((s32)0x86400127)
#define SCE_ASF_ERROR_INVALID_STREAM_ID     ((s32)0x86400151)
#define SCE_ASF_ERROR_AUDIO_HEADER_TOO_SHORT ((s32)0x86400152)
#define SCE_ASF_ERROR_WMA_HEADER_TOO_LONG   ((s32)0x86400153)
#define SCE_ASF_ERROR_INVALID_HEADER_GUID   ((s32)0x86400171)
#define SCE_ASF_ERROR_NO_FILE_PROPERTIES    ((s32)0x86400172)
#define SCE_ASF_ERROR_DUP_FILE_PROPERTIES   ((s32)0x86400173)
#define SCE_ASF_ERROR_DUP_STREAM_ID         ((s32)0x86400174)
#define SCE_ASF_ERROR_DATA_OBJ_INVALID      ((s32)0x86400201)
#define SCE_ASF_ERROR_DATA_OBJ_GUID         ((s32)0x86400202)
#define SCE_ASF_ERROR_DATA_PACKET_MISMATCH  ((s32)0x86400203)
#define SCE_ASF_ERROR_ERR_CORR_UNSUPPORTED  ((s32)0x86400204)
#define SCE_ASF_ERROR_UNSUPPORTED_STREAM    ((s32)0x86400401)
#define SCE_ASF_ERROR_STREAM_RANGE          ((s32)0x86400402)
#define SCE_ASF_ERROR_STREAM_NOT_FOUND      ((s32)0x86400403)
#define SCE_ASF_ERROR_END_OF_STREAM         ((s32)0x86400404)
#define SCE_ASF_ERROR_DRM_NOT_INITIALIZED   ((s32)0x86400405)
#define SCE_ASF_ERROR_DRM_READ_RANGE        ((s32)0x86400406)
#define SCE_ASF_ERROR_EOF_TIME              ((s32)0x86400501)
#define SCE_ASF_ERROR_NOT_SEEKABLE          ((s32)0x86400502)
#define SCE_ASF_ERROR_OBJECT_NOT_ENABLED    ((s32)0x86400601)
#define SCE_ASF_ERROR_OBJECT_NOT_FOUND      ((s32)0x86400602)
#define SCE_ASF_ERROR_STREAM_IDX_OUT_OF_RANGE ((s32)0x86400641)
#define SCE_ASF_ERROR_INVALID_OBJECT_MASK   ((s32)0x86400691)
#define SCE_ASF_ERROR_DRM_NO_DECRYPT_CB     ((s32)0x86400750)
#define SCE_ASF_ERROR_WMA_INVALID_PARAM     ((s32)0x86401001)
#define SCE_ASF_ERROR_WMA_BAD_CHANNELS      ((s32)0x86401011)
#define SCE_ASF_ERROR_WMA_BAD_SAMPLERATE    ((s32)0x86401012)
#define SCE_ASF_ERROR_WMA_BAD_BITRATE       ((s32)0x86401013)
#define SCE_ASF_ERROR_WMA_BAD_BLOCK_ALIGN   ((s32)0x86401014)
#define SCE_ASF_ERROR_WMA_BAD_BLOCK_LEN     ((s32)0x86401021)
#define SCE_ASF_ERROR_WMA_COEF_OVERFLOW     ((s32)0x86401031)
#define SCE_ASF_ERROR_WMA_NOT_READY         ((s32)0x86401071)
#define SCE_ASF_ERROR_WMA_NOISE_BANDS       ((s32)0x86401072)
#define SCE_ASF_ERROR_WMA_FRAME_CORRUPT     ((s32)0x86401073)
#define SCE_ASF_ERROR_WMA_BITSTREAM_END     ((s32)0x86401998)
#define SCE_ASF_ERROR_WMA_SEQ_MISMATCH      ((s32)0x86401999)
#define SCE_ASF_ERROR_DRM_MODE_MISMATCH     ((s32)0x80000105)
#define SCE_ASF_ERROR_DRM_INVALID_ARG       ((s32)0x800001ff)
#define SCE_ASF_ERROR_DRM_PAYLOAD_TOO_LARGE ((s32)0x80010069)

/* ASF object bitmask constants */
#define ASF_OBJ_MASK_FILE_PROP      0x00000001ULL
#define ASF_OBJ_MASK_STREAM_PROP    0x000000FEULL
#define ASF_OBJ_MASK_HEADER_EXT     0x00000100ULL
#define ASF_OBJ_MASK_CODEC_LIST     0x00000200ULL
#define ASF_OBJ_MASK_SCRIPT_CMD     0x00000400ULL
#define ASF_OBJ_MASK_MARKER         0x00000800ULL
#define ASF_OBJ_MASK_BITRATE_MUTEX  0x00001000ULL
#define ASF_OBJ_MASK_ERR_CORR       0x00002000ULL
#define ASF_OBJ_MASK_CONTENT_DESC   0x00004000ULL
#define ASF_OBJ_MASK_EXT_CONTENT    0x00008000ULL
#define ASF_OBJ_MASK_STREAM_BITRATE 0x00010000ULL
#define ASF_OBJ_MASK_CONTENT_BRAND  0x00020000ULL
#define ASF_OBJ_MASK_CONTENT_ENC    0x00040000ULL
#define ASF_OBJ_MASK_EXT_ENC        0x00080000ULL
#define ASF_OBJ_MASK_DIGITAL_SIG    0x00100000ULL
#define ASF_OBJ_MASK_SINGLE_VALID   0x001FFF01ULL
#define ASF_OBJ_MASK_DRM_ANY        0x000C0000ULL

#define ASF_CODEC_FLAG_WMA          0x00000001ULL
#define ASF_CODEC_ID_WMA_V2         353

#define ASF_STREAM_TYPE_AUDIO       0
#define ASF_STREAM_TYPE_DRM_MEDIA   1
#define ASF_STREAM_TYPE_VIDEO       16
#define ASF_STREAM_TYPE_COMMAND     32
#define ASF_STREAM_TYPE_JFIF        48
#define ASF_STREAM_TYPE_DEGRAD_JPEG 49
#define ASF_STREAM_TYPE_FILE_XFER   64
#define ASF_STREAM_TYPE_BINARY      65
#define ASF_STREAM_TYPE_UNKNOWN     0xFFFF

#define ASF_MAX_STREAMS             127
#define ASF_DRM_HEADER_BUF_SIZE     10240
#define ASF_DRM_DECRYPT_BUF_SIZE    0x8000

#define ASF_SEEK_SET                0
#define ASF_SEEK_CUR                1

/* Callbacks */
typedef s32 (*SceAsfReadCallback)(void *userData, void *buf, u32 size);
typedef s64 (*SceAsfSeekCallback)(void *userData, void *unused, s64 offset, s32 whence);

typedef s32 (*SceAsfDrmInitCb)(void *session);
typedef s32 (*SceAsfDrmSetIvCb)(void *session, void *ivBuf, u32 payloadSize);
typedef s32 (*SceAsfDrmDecryptCb)(void *session, void *buf, u32 blockSize);
typedef s32 (*SceAsfDrmTermCb)(void *session, u32 flags);

typedef struct SceAsfDrmCallbacks {
	SceAsfDrmInitCb initCb;
	SceAsfDrmSetIvCb setIvCb;
	SceAsfDrmDecryptCb decryptCb;
	SceAsfDrmTermCb termCb;
	void *userArg;
} SceAsfDrmCallbacks;

typedef struct SceAsfGuid {
	u32 data1;
	u16 data2;
	u16 data3;
	u8 data4[8];
} SceAsfGuid;

typedef struct SceAsfObjEntry {
	u64 count;
	u64 offset;
	u64 size;
} SceAsfObjEntry;

typedef struct SceAsfIoContext {
	void *userData;
	SceAsfReadCallback readCb;
	SceAsfSeekCallback seekCb;
} SceAsfIoContext;

typedef struct SceAsfDrmSession {
	u32 reserved0;
	void *userArg;
	u8 *contentEncData;
	u32 contentEncSize;
	u8 *protectionType;
	u32 protectionTypeSize;
	u8 *keyId;
	u32 keyIdSize;
	u8 *licenseUrl;
	u32 licenseUrlSize;
	u32 encType;
} SceAsfDrmSession;

typedef struct SceAsfDrmContext {
	u16 drmStreamId;
	u16 blockAlign;
	SceAsfDrmSession session;
	u32 initialized;
	u8 ivBuf[16];
	u32 ivSize;
	SceAsfDrmInitCb initCb;
	SceAsfDrmSetIvCb setIvCb;
	SceAsfDrmDecryptCb decryptCb;
	SceAsfDrmTermCb termCb;
	void *userArg;
	u8 *headerBuf;
	u8 *decryptBuf;
	u32 decryptBufSize;
	u32 decryptedBytes;
	u32 payloadTotalSize;
	s64 payloadFileOffset;
} SceAsfDrmContext;

typedef struct SceAsfHeaderInfo {
	u64 headerSize;
	u64 dataSize;
	u64 reserved10;
	u64 dataPacketsCount;
	u64 playDuration;
	u64 sendDuration;
	u64 preroll;
	u32 flags;
	u32 minPacketSize;
	u32 maxPacketSize;
	u32 maxBitrate;
} SceAsfHeaderInfo;

typedef struct SceAsfPacketState {
	u8 streamId;
	u8 payloadFlags;
	u8 errCorrFlags;
	u8 lengthTypeFlags;
	u8 propertyFlags;
	u8 pad0[3];
	u32 packetLength;
	u32 paddingLength;
	u32 sendTime;
	u16 duration;
	u8 subPayloadDelta;
	u8 payloadCountFlags;
	u32 reserved18;
	u32 payloadIndex;
	u32 subPayloadIndex;
	u32 packetOffset;
	u32 subPayloadEnd;
	u32 payloadEnd;
	u64 packetNumber;
	SceAsfHeaderInfo *headerInfo;
	SceAsfIoContext *io;
	SceAsfDrmContext *drm;
	u32 pad44;
	s64 savedFilePos;
} SceAsfPacketState;

typedef struct SceAsfStream {
	u32 streamNumber;
	u16 streamType;
	u16 codecId;
	u32 stateFlags;
	void *codecInfo;
	void *codecCtx;
	u32 reserved14;
	SceAsfPacketState pkt;
} SceAsfStream;

typedef struct SceAsfWmaWaveFormat {
	u16 channels;
	u16 pad02;
	u32 sampleRate;
	u32 avgBytesPerSec;
	u16 blockAlign;
	u16 encodeOptions;
} SceAsfWmaWaveFormat;

typedef struct SceAsfWmaFrame {
	u8 *data;
	u32 ptsMs;
	u32 bitOffset;
	u32 bitLength;
	u32 flags;
	u32 prevBlockLen;
	u32 curBlockLen;
} SceAsfWmaFrame;

typedef struct SceWmaBitReader {
	u32 packetBits;
	u32 consumedBits;
	u32 curBit;
	u32 endBit;
	const u8 *buf;
} SceWmaBitReader;

typedef struct SceWmaChannel {
	const u16 *coefVlc;
	u32 reserved04;
	u32 coefsCount;
	u32 coded;
} SceWmaChannel;

typedef struct SceWmaParserCtx {
	u32 packetSeq;
	u32 frameLenBits;
	u16 channels;
	u16 coefBitDepth;
	u32 useNoiseCoding;
	u32 sampleRate;
	u32 useBitReservoir;
	u32 useVariableBlockLen;
	u32 blockLenBits;
	u32 minBlockLen;
	u32 frameLenLog2;
	u32 frameLen;
	u32 coefsEnd;
	u32 maxCoefsEnd;
	u32 useExpVlc;
	u32 coefVlcSet;
	s32 curBandCount;
	const s32 *curBandBounds;
	SceWmaChannel ch[2];
	s32 bandCounts[5];
	s32 bandBounds[5][26];
	float highBandStartFreq;
	s32 curHighBandStart;
	s32 curHighBandCoefStart;
	s32 highBandStart[5];
	s16 subframeIndex;
	u16 prevBlockLen;
	u16 curBlockLen;
	u16 nextBlockLen;
	u16 blockPos;
	s16 framesLeft;
	u32 packetReady;
	u32 bitReservoirSkipBits;
	SceWmaBitReader br;
	u32 pad2C8;
	u32 curTimeMs;
	u32 frameDurationMs;
	u32 flagDiscontinuity;
	u32 flagNextDiscontinuity;
	u32 flagPacketLoss;
} SceWmaParserCtx;

typedef struct SceAsfParser {
	u64 codecFlags;
	u64 objectFlags;
	u32 drmEnableFlags;
	u32 reserved14;
	u32 reserved18;
	u32 reserved1C;
	u32 needMem;
	void *workBuf;
	SceAsfHeaderInfo header;
	u32 streamCount;
	u32 reserved74;
	SceAsfStream streams[ASF_MAX_STREAMS];
	SceAsfStream tempStream;
	SceAsfStream *streamById[ASF_MAX_STREAMS];
	SceAsfIoContext io;
	void *codecWorkBase;
	SceAsfObjEntry *objTable;
	SceAsfDrmContext drm;
} SceAsfParser;

_Static_assert(sizeof(SceAsfWmaWaveFormat) == 16, "SceAsfWmaWaveFormat size");
_Static_assert(sizeof(SceWmaParserCtx) == 736, "SceWmaParserCtx size");
_Static_assert(sizeof(SceAsfHeaderInfo) == 72, "SceAsfHeaderInfo size");
_Static_assert(sizeof(SceAsfPacketState) == 80, "SceAsfPacketState size");
_Static_assert(sizeof(SceAsfStream) == 104, "SceAsfStream size");
_Static_assert(offsetof(SceAsfParser, streamById) == 13432, "streamById offset");
_Static_assert(offsetof(SceAsfParser, io) == 13940, "io offset");
_Static_assert(offsetof(SceAsfParser, drm) == 13960, "drm offset");

/* Constant tables (asf_tables.c) */
extern const u16 g_wmaBarkFreqs[25];
extern const u16 g_wmaVlcScale[442];
extern const u16 g_wmaVlcNoise[288];
extern const u16 g_wmaVlcCoef0[1036];
extern const u16 g_wmaVlcCoef1[988];
extern const u16 g_wmaVlcCoef2[2680];
extern const u16 g_wmaVlcCoef3[2102];
extern const u16 g_wmaVlcCoef4[1392];
extern const u16 g_wmaVlcCoef5[1220];
extern const u8 g_asfGuids[40][16];

/* I/O helpers (asf_packet.c) */
s32 asfIoInit(SceAsfParser *parser, void *userData, SceAsfReadCallback readCb, SceAsfSeekCallback seekCb);
s32 asfIoSeekAbs(SceAsfParser *parser, u64 pos);
s64 asfIoTell(SceAsfIoContext *io);
s32 asfIoReadU8(SceAsfIoContext *io, u8 *out);
s32 asfIoReadU16(SceAsfIoContext *io, u16 *out);
s32 asfIoReadU32(SceAsfIoContext *io, u32 *out);
s32 asfIoReadU64(SceAsfIoContext *io, u64 *out);
s32 asfIoReadBytes(SceAsfIoContext *io, void *buf, u32 size);
s32 asfIoSkip(SceAsfIoContext *io, s64 offset);
s64 asfIoSeek(SceAsfIoContext *io, s64 offset, s32 whence);

/* Packet demuxer (asf_packet.c) */
s32 asfInitAllPacketStreams(SceAsfParser *parser);
s32 asfPacketPeekPayload(void *dst, SceAsfPacketState *pkt, u32 size);
s32 asfPacketAdvancePayload(SceAsfPacketState *pkt);
s32 asfPacketIsPayloadDone(SceAsfPacketState *pkt);
s32 asfPacketSeekToTime(SceAsfPacketState *pkt, u32 targetTimeMs);
s32 asfPacketTimeDelta(SceAsfPacketState *pkt, u32 targetTimeMs);
s32 asfPacketSeekToPacket(SceAsfPacketState *pkt, u64 packetNum);
s32 asfPacketSkipBytes(SceAsfPacketState *pkt, u32 count);
u32 asfPacketGetSendTime(SceAsfPacketState *pkt);

/* Container & memory layout (asf_container.c) */
s32 asfIsDrmEnabled(const SceAsfParser *parser);
u32 asfCalcNeedMem(const SceAsfParser *parser);
s32 asfInitWorkMem(SceAsfParser *parser);
s32 asfGetStreamObjEntry(const SceAsfParser *parser, u32 streamIdx, SceAsfObjEntry **outEntry);
s32 asfGetHeaderObjEntry(const SceAsfParser *parser, u64 objMask, SceAsfObjEntry **outEntry);
s32 asfReadObjEntryInfo(const SceAsfObjEntry *entry, u64 *outOffset, u64 *outSize);
s32 asfParseHeaderObject(SceAsfParser *parser);
s32 asfParseDataObject(SceAsfParser *parser);
s32 asfInitAllCodecs(SceAsfParser *parser);
s32 asfReadNextFrame(SceAsfStream *stream, u64 codecFlags, SceAsfWmaFrame *frame);
s32 asfSeekStreamFrame(SceAsfStream *stream, u64 codecFlags, u32 *timeMs);
void asfGetStreamTime(SceAsfStream *stream, u64 codecFlags, u32 *timeMs);
s32 asfSetDrmCallbacks(SceAsfParser *parser, const SceAsfDrmCallbacks *cbs);
void *asfMemmove(void *dst, const void *src, s32 len);

/* WMA bitstream/frame parser (asf_wma.c) */
s32 wmaInitParser(SceWmaParserCtx *ctx, const SceAsfWmaWaveFormat *fmt);
s32 wmaReadNextFrame(SceWmaParserCtx *ctx, SceAsfStream *stream, SceAsfWmaFrame *frame);
s32 wmaSeekFrame(SceWmaParserCtx *ctx, SceAsfStream *stream, u32 *timeMs);
void wmaGetCurTime(const SceWmaParserCtx *ctx, u32 *timeMs);

#endif /* ASF_INT_H */
