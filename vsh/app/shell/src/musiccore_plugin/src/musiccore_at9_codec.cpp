/*
 * SceShell musiccore_plugin - At9Factory and At9Parser (ATRAC9 pipeline
 * factory, RIFF/WAVE AT9 frame reader, and sample-accurate smpl loop engine).
 * Original address range (3.65-CEX): 0x811FAE28 - 0x811FBF43
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
int sce_paf_strcmp(const char *lhs, const char *rhs);
}

namespace sce {
namespace MusicCore {

static const uint32_t FOURCC_AT9 = 0x20415439u; /* ' AT9' */

static const int PARSER_ERR_OK = 0;
static const int PARSER_ERR_NO_SOURCE = 4;
static const int PARSER_ERR_EOS = 5;
static const int PARSER_ERR_OPEN_FAIL = 17;
static const int PARSER_ERR_CLOSE_FAIL = 18;
static const int PARSER_ERR_READ_IO = 19;
static const int PARSER_ERR_OPEN_IO = 20;
static const int PARSER_ERR_UNSUPPORTED_BOX = 21;
static const int PARSER_ERR_BAD_STATE = 33;
static const int PARSER_ERR_SYNC_LOST = 69;
static const int PARSER_ERR_UNSUPPORTED = 81;
static const int PARSER_ERR_CORRUPT = 95;
static const int PARSER_ERR_BAD_PARAM = 241;

static const int IO_ERR_OPEN_FAIL = -2146394111;     /* 0x8010A001 */
static const int IO_ERR_OPEN_IO = -2146394109;       /* 0x8010A003 */
static const int IO_ERR_READ_IO = -2146394108;       /* 0x8010A004 */
static const int WAVE_ERR_UNSUPPORTED = -2144862159; /* 0x80280031 */
static const int WAVE_ERR_CORRUPT = -2144862158;     /* 0x80280032 */

static const int MODE_LOCAL_PLAY = 1;
static const int MODE_STREAM_PLAY = 5;
static const int MODE_DIRECT_PLAY = 16;

extern void *allocFromHeap(size_t size);
extern void *allocSecondaryHeap(size_t size);
extern void freeSecondaryHeap(void *ptr);
extern void delayWorkerThread(SceUInt delayUs);

struct SyncMutexStorage {
    void *vtablePtr;
    SceUID semaId;
};

extern int syncMutexLock(SyncMutexStorage *mutex);
extern int syncMutexUnlock(SyncMutexStorage *mutex);

struct MediaInfoHandle {
    void *vtablePtr;
    uint32_t mediaFourcc;
    uint8_t sharedFlag;
};

extern void decoderManagerReleaseMediaInfo(MediaInfoHandle **ioHandle, int forceRelease);
extern void *at9MediaInfoConstruct(void *mem, const char *path, int loopMode);

struct At9FmtHeader {
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint16_t cbSize;
    uint16_t samplesPerBlock;
    uint32_t channelMask;
    uint8_t subFormatGuid[16];
    uint32_t versionInfo;
    uint8_t configData[4];
};

struct At9SmplLoopInfo {
    uint32_t loopStart;
    uint32_t loopEnd;
};

class At9CacheIoView {
public:
    virtual ~At9CacheIoView() {}
    virtual int initialize() = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() = 0;
    virtual int getFileSize() = 0;
    virtual const char *getPath() = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(int mode) = 0;
    virtual int applyOpenFilePriority(int mode) = 0;
    virtual int flushCache() = 0;
    virtual int setCacheHint(int hintMode, int extra) = 0;

    uint8_t baseStorage[60];
    uint32_t dataSize; /* +64 */
};

extern int at9CacheScanChunks(At9CacheIoView *io, uint8_t targetMask);
extern int at9CacheReadFmtHeader(At9CacheIoView *io, At9FmtHeader *outFmt);
extern int at9CacheReadSmplLoop(At9CacheIoView *io, At9SmplLoopInfo *outLoop);
extern int wavCacheReadDataRange(void *io, uint32_t offset, uint32_t size,
                                 void **outPtr, uint32_t *outSize);

struct At9MediaInfoView : public MediaInfoHandle {
    uint8_t pathStorage[2052];
    At9CacheIoView *cacheIo; /* +2064 */
};

extern void parserFilterBaseInit(void *self, int filterKind, const char *threadName);
extern void parserFilterBaseDestroy(void *self);
extern void parserFilterSetOutputPipe(void *parser, void *pipe);
extern void decoderFilterSetInputPipe(void *decoder, void *pipe);
extern void decoderFilterSetOutputSink(void *decoder, void *audioSink);
extern void *at9DecoderConstruct(void *mem, const char *threadName);
extern void *createFixedFramePipe(int frameSize, int slotCount, void *producer, void *consumer);
extern void *createAudioOutputFilter(int grainSize, int slotCount, void *producer,
                                     int p1, int p2, int p3, int p4, int p5);
extern void *createDecoderPipeline(void *parser, void *pipe, void *decoder, void *audioSink);
extern bool decoderPipelineIsReady(const void *pipeline);

struct FilterDestroyable {
    virtual ~FilterDestroyable() {}
};

struct ActiveFilterAttach {
    virtual ~ActiveFilterAttach() {}
    virtual void setEventListener(void *listener) = 0;
};

struct FilterReadyCheck {
    virtual ~FilterReadyCheck() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
};

struct At9PacketOut {
    void *frameBuffer;        /* +0 */
    uint32_t maxBufferSize;   /* +4 */
    uint32_t payloadSize;     /* +8 */
    float timestampMs;        /* +12 */
    float frameDurationMs;    /* +16 */
    uint32_t sampleRate;      /* +20 */
    uint8_t reservedMid[40];  /* +24..+63 */
    uint8_t channelCount;     /* +64 */
    uint8_t padChan[3];       /* +65..+67 */
    uint32_t trimStartSample; /* +68 */
    uint32_t trimEndSample;   /* +72 */
};

struct At9CodecConfigPayload {
    int status;
    uint8_t configData[4];
};

/* 0x811FBE12 */
int mapAt9ParserError(uint32_t errCode)
{
    if (errCode == 0u) {
        return PARSER_ERR_OK;
    }
    switch (errCode) {
    case 0x8010A001u:
        return PARSER_ERR_OPEN_FAIL;
    case 0x8010A002u:
        return PARSER_ERR_CLOSE_FAIL;
    case 0x8010A003u:
        return PARSER_ERR_OPEN_IO;
    case 0x8010A004u:
        return PARSER_ERR_READ_IO;
    case 0x8010A005u:
        return PARSER_ERR_BAD_STATE;
    case 0x8010A007u:
        return PARSER_ERR_EOS;
    case 0x8010AFF1u:
        return PARSER_ERR_BAD_PARAM;
    case 0x80280028u:
    case 0x80280032u:
    case 0x80280033u:
        return PARSER_ERR_CORRUPT;
    case 0x80280029u:
        return PARSER_ERR_SYNC_LOST;
    case 0x8028002Au:
        return PARSER_ERR_UNSUPPORTED_BOX;
    case 0x80280031u:
        return PARSER_ERR_UNSUPPORTED;
    default:
        return -1;
    }
}

/* ===== At9Parser (0x811FB0FC - 0x811FBF43, size 168 bytes) ===== */

class At9Parser {
public:
    At9Parser(const char *threadName, int playMode, int loopCount);
    virtual ~At9Parser();

    int findCachedMediaInfo(const char *path, At9MediaInfoView **outInfo);
    int setMediaInfo(At9MediaInfoView *mediaInfo);
    int prefetchMediaInfo(At9MediaInfoView *mediaInfo);
    int setStartPosition(int startMs);
    int parseAt9Header(uint8_t scanMask);
    int setEndPosition(int endMs);
    int resetPosition();
    int getDurationMs();
    int onStart();
    int onStop();
    void initFramePosition(At9PacketOut *outPkt, int frameIdx, uint32_t sampleOffset, int startMs);
    int readFrame(At9PacketOut *outPkt);
    At9CodecConfigPayload *getCodecConfig(uint32_t *outSize);

    uint8_t m_basePrefix[16];          /* +4..+19 */
    SyncMutexStorage m_mutex;          /* +20..+27 */
    uint32_t m_baseReserved;           /* +28 */
    int m_filterState;                 /* +32 */
    uint8_t m_baseTail[12];            /* +36..+47 */
    int m_playMode;                    /* +48 */
    int m_requestedSpeed;              /* +52 */
    int m_activeSpeed;                 /* +56 */
    int m_rawStartMs;                  /* +60 */
    int m_rawEndMs;                    /* +64 */
    int m_clampedStartMs;              /* +68 */
    int m_clampedEndMs;                /* +72 */
    int m_currentFrameIdx;             /* +76 */
    int m_loopJumpFrameIdx;            /* +80 */
    uint32_t m_blockAlign;             /* +84 */
    uint32_t m_totalFrames;            /* +88 */
    int m_formatTag;                   /* +92 */
    uint16_t m_channels;               /* +96 */
    uint16_t m_bitsPerSample;          /* +98 */
    uint32_t m_sampleRate;             /* +100 */
    float m_bytesPerMs;                /* +104 */
    uint16_t m_samplesPerBlock;        /* +108 */
    uint16_t m_padSamples;             /* +110 */
    float m_frameDurationMs;           /* +112 */
    uint32_t m_loopStartSample;        /* +116 */
    uint32_t m_loopEndSample;          /* +120 */
    uint32_t m_loopStartFrame;         /* +124 */
    uint32_t m_loopEndFrame;           /* +128 */
    int m_loopCount;                   /* +132 */
    int m_completedLoops;              /* +136 */
    At9CacheIoView *m_activeCacheIo;   /* +140 */
    At9CacheIoView *m_nextCacheIo;     /* +144 */
    At9MediaInfoView *m_activeMedia;   /* +148 */
    At9MediaInfoView *m_nextMedia;     /* +152 */
    uint32_t m_unusedStreamSlot;       /* +156 */
    At9CodecConfigPayload m_codecCfg;  /* +160..+167 */
};

/* 0x811FB0FC */
At9Parser::At9Parser(const char *threadName, int playMode, int loopCount)
    : m_playMode(playMode)
    , m_requestedSpeed(0)
    , m_activeSpeed(0)
    , m_rawStartMs(-1)
    , m_rawEndMs(-1)
    , m_clampedStartMs(-1)
    , m_clampedEndMs(-1)
    , m_currentFrameIdx(-1)
    , m_loopJumpFrameIdx(-1)
    , m_blockAlign(0xFFFFFFFFu)
    , m_totalFrames(0xFFFFFFFFu)
    , m_formatTag(-1)
    , m_channels(0)
    , m_bitsPerSample(0)
    , m_sampleRate(0)
    , m_bytesPerMs(0.0f)
    , m_samplesPerBlock(0)
    , m_padSamples(0)
    , m_frameDurationMs(0.0f)
    , m_loopStartSample(0)
    , m_loopEndSample(0)
    , m_loopStartFrame(0)
    , m_loopEndFrame(0)
    , m_loopCount(loopCount)
    , m_completedLoops(0)
    , m_activeCacheIo(NULL)
    , m_nextCacheIo(NULL)
    , m_activeMedia(NULL)
    , m_nextMedia(NULL)
    , m_unusedStreamSlot(0)
{
    parserFilterBaseInit(this, 1, threadName);
    m_codecCfg.status = 0;
    sce_paf_memset(m_codecCfg.configData, 0, sizeof(m_codecCfg.configData));
}

/* 0x811FB192 / 0x811FB216 */
At9Parser::~At9Parser()
{
    if (m_nextMedia != NULL) {
        MediaInfoHandle *nextHandle = m_nextMedia;
        decoderManagerReleaseMediaInfo(&nextHandle, 1);
        m_nextMedia = NULL;
        m_nextCacheIo = NULL;
    }
    if (m_activeMedia != NULL) {
        MediaInfoHandle *curHandle = m_activeMedia;
        decoderManagerReleaseMediaInfo(&curHandle, 1);
        m_activeMedia = NULL;
        m_activeCacheIo = NULL;
    }
    m_codecCfg.status = 0;
    sce_paf_memset(m_codecCfg.configData, 0, sizeof(m_codecCfg.configData));
    parserFilterBaseDestroy(this);
}

/* 0x811FB226 */
int At9Parser::findCachedMediaInfo(const char *path, At9MediaInfoView **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    syncMutexLock(&m_mutex);
    if (m_activeCacheIo != NULL && sce_paf_strcmp(m_activeCacheIo->getPath(), path) == 0) {
        syncMutexUnlock(&m_mutex);
        *outInfo = m_activeMedia;
        return 0;
    }
    if (m_nextCacheIo != NULL && sce_paf_strcmp(m_nextCacheIo->getPath(), path) == 0) {
        *outInfo = m_nextMedia;
        syncMutexUnlock(&m_mutex);
        return 0;
    }
    syncMutexUnlock(&m_mutex);
    *outInfo = NULL;
    return 0;
}

/* 0x811FB29E */
int At9Parser::setMediaInfo(At9MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_AT9) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->cacheIo == NULL) {
        MediaInfoHandle *badHandle = mediaInfo;
        decoderManagerReleaseMediaInfo(&badHandle, 1);
        return -1;
    }

    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }

    const char *newPath = mediaInfo->cacheIo->getPath();
    if (m_activeCacheIo != NULL) {
        if (sce_paf_strcmp(m_activeCacheIo->getPath(), newPath) == 0) {
            syncMutexUnlock(&m_mutex);
            delayWorkerThread(20000u);
            return 0;
        }
        if (m_activeMedia == m_nextMedia) {
            m_nextMedia = NULL;
            m_nextCacheIo = NULL;
        }
        if (m_activeMedia != NULL) {
            MediaInfoHandle *oldHandle = m_activeMedia;
            decoderManagerReleaseMediaInfo(&oldHandle, 1);
            m_activeMedia = NULL;
        }
        m_activeCacheIo = NULL;
    }

    m_currentFrameIdx = -1;
    m_loopJumpFrameIdx = -1;
    m_formatTag = -1;

    if (m_nextCacheIo != NULL && sce_paf_strcmp(m_nextCacheIo->getPath(), newPath) == 0) {
        m_activeCacheIo = m_nextCacheIo;
        m_activeMedia = m_nextMedia;
        if (m_activeCacheIo != NULL) {
            m_activeCacheIo->setPriorityMode(0);
        }
        m_nextCacheIo = NULL;
        m_nextMedia = NULL;
        syncMutexUnlock(&m_mutex);
        return 0;
    }

    if (m_nextCacheIo != NULL) {
        if (m_nextMedia != NULL) {
            MediaInfoHandle *nextHandle = m_nextMedia;
            decoderManagerReleaseMediaInfo(&nextHandle, 1);
            m_nextMedia = NULL;
        }
        m_nextCacheIo = NULL;
    }

    mediaInfo->sharedFlag = 1u;
    m_activeMedia = mediaInfo;
    m_activeCacheIo = mediaInfo->cacheIo;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811FB3FE */
int At9Parser::prefetchMediaInfo(At9MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_AT9) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->cacheIo == NULL) {
        return -1;
    }

    syncMutexLock(&m_mutex);
    const char *newPath = mediaInfo->cacheIo->getPath();
    if (m_nextCacheIo != NULL) {
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), newPath) == 0) {
            syncMutexUnlock(&m_mutex);
            return 0;
        }
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), m_activeCacheIo->getPath()) == 0) {
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            syncMutexUnlock(&m_mutex);
            return 0;
        }
    }

    if (m_nextMedia != NULL) {
        syncMutexUnlock(&m_mutex);
        return -1;
    }

    mediaInfo->sharedFlag = 1u;
    m_nextMedia = mediaInfo;
    m_nextCacheIo = mediaInfo->cacheIo;
    m_nextCacheIo->setPriorityMode(1);

    if (m_nextCacheIo->isOpen() != 1 && m_nextCacheIo->open() < 0) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_OPEN_FAIL;
    }

    int scanErr = at9CacheScanChunks(m_nextCacheIo, 7u);
    syncMutexUnlock(&m_mutex);
    if (scanErr == 0) {
        return 0;
    }
    return mapAt9ParserError((uint32_t)scanErr);
}

/* 0x811FB554 */
int At9Parser::setStartPosition(int startMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_rawStartMs = startMs;
    m_clampedStartMs = (startMs >= 0) ? startMs : 0;
    m_currentFrameIdx = -1;
    m_loopJumpFrameIdx = -1;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811FB59E */
int At9Parser::parseAt9Header(uint8_t scanMask)
{
    int err = at9CacheScanChunks(m_activeCacheIo, scanMask);
    if (err != 0) {
        return err;
    }

    At9FmtHeader fmt;
    fmt.formatTag = 0;
    fmt.channels = 0;
    fmt.sampleRate = 0;
    fmt.avgBytesPerSec = 0;
    fmt.blockAlign = 0;
    fmt.bitsPerSample = 16;
    fmt.cbSize = 0;
    fmt.samplesPerBlock = 0;
    fmt.channelMask = 0;
    fmt.versionInfo = 0;
    sce_paf_memset(fmt.subFormatGuid, 0, sizeof(fmt.subFormatGuid));
    sce_paf_memset(fmt.configData, 0, sizeof(fmt.configData));

    err = at9CacheReadFmtHeader(m_activeCacheIo, &fmt);
    if (err != 0) {
        return err;
    }
    if (fmt.avgBytesPerSec == 0u || fmt.formatTag != 0xFFFEu) {
        return WAVE_ERR_UNSUPPORTED;
    }
    if (fmt.sampleRate != 48000u || fmt.bitsPerSample != 16u
        || (fmt.channels != 1u && fmt.channels != 2u)) {
        return WAVE_ERR_UNSUPPORTED;
    }

    m_blockAlign = fmt.blockAlign;
    uint32_t dataSize = m_activeCacheIo->dataSize;
    m_totalFrames = (dataSize / m_blockAlign) + ((dataSize % m_blockAlign != 0u) ? 1u : 0u);
    m_sampleRate = 48000u;
    m_samplesPerBlock = fmt.samplesPerBlock;
    m_formatTag = 0xFFFE;
    m_channels = fmt.channels;
    m_bitsPerSample = 16u;
    m_bytesPerMs = (float)fmt.avgBytesPerSec * 0.001f;
    m_frameDurationMs = ((float)m_samplesPerBlock / (float)m_sampleRate) * 1000.0f;

    At9SmplLoopInfo loopInfo;
    loopInfo.loopStart = 0;
    loopInfo.loopEnd = 0;
    err = at9CacheReadSmplLoop(m_activeCacheIo, &loopInfo);
    if (err == 0 || err == WAVE_ERR_CORRUPT) {
        m_loopStartSample = loopInfo.loopStart;
        m_loopEndSample = loopInfo.loopEnd;
        m_loopStartFrame = loopInfo.loopStart / m_samplesPerBlock;
        m_loopEndFrame = loopInfo.loopEnd / m_samplesPerBlock;
        return 0;
    }
    return err;
}

/* 0x811FB750 */
int At9Parser::setEndPosition(int endMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_rawEndMs = endMs;
    m_clampedEndMs = (endMs >= 0) ? endMs : 0;
    m_currentFrameIdx = -1;
    m_loopJumpFrameIdx = -1;

    if (m_formatTag >= 0 || parseAt9Header(7u) == 0) {
        if (m_clampedEndMs < 0) {
            m_clampedEndMs = 0;
        }
    }
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811FB7E2 */
int At9Parser::resetPosition()
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_currentFrameIdx = -1;
    m_loopJumpFrameIdx = -1;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811FB812 */
int At9Parser::getDurationMs()
{
    syncMutexLock(&m_mutex);
    if (m_activeCacheIo == NULL) {
        syncMutexUnlock(&m_mutex);
        return 0;
    }
    if (m_formatTag < 0) {
        if (m_activeCacheIo->isOpen() != 1 && m_activeCacheIo->open() < 0) {
            syncMutexUnlock(&m_mutex);
            return 0;
        }
        if (parseAt9Header(7u) != 0) {
            syncMutexUnlock(&m_mutex);
            return 0;
        }
    }
    int durationMs = (int)((float)m_activeCacheIo->dataSize / m_bytesPerMs);
    syncMutexUnlock(&m_mutex);
    return durationMs;
}

/* 0x811FB896 */
int At9Parser::onStart()
{
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    m_activeCacheIo->setPriorityMode(0);
    if (m_activeCacheIo->isOpen() != 1 && m_activeCacheIo->open() < 0) {
        return PARSER_ERR_OPEN_FAIL;
    }
    m_activeSpeed = 0;
    return 0;
}

/* 0x811FB8D2 */
int At9Parser::onStop()
{
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    m_activeCacheIo->setPriorityMode(1);
    return 0;
}

/* 0x811FB8EA */
void At9Parser::initFramePosition(At9PacketOut *outPkt, int frameIdx,
                                  uint32_t sampleOffset, int startMs)
{
    m_currentFrameIdx = frameIdx;
    outPkt->trimStartSample = sampleOffset;
    outPkt->trimEndSample = (uint32_t)m_samplesPerBlock - 1u;
    if ((uint32_t)m_requestedSpeed >= 2u && m_samplesPerBlock == 1024u) {
        outPkt->trimEndSample = sampleOffset + 256u;
    }

    if (startMs >= 0) {
        uint32_t startSample = (uint32_t)startMs * (m_sampleRate / 1000u);
        if (startSample < m_loopStartSample) {
            m_completedLoops = 0;
        } else if (startSample > m_loopEndSample) {
            m_completedLoops = m_loopCount - 1;
        }
    }
}

/* 0x811FB95C */
int At9Parser::readFrame(At9PacketOut *outPkt)
{
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (outPkt == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    if (m_formatTag < 0) {
        int err = parseAt9Header(7u);
        if (err != 0) {
            return mapAt9ParserError((uint32_t)err);
        }
    }
    if (m_activeCacheIo->dataSize == 0u) {
        return PARSER_ERR_EOS;
    }

    outPkt->trimStartSample = 0u;
    outPkt->trimEndSample = (uint32_t)m_samplesPerBlock - 1u;

    int startFrame = 0;
    uint32_t startSampleTrim = 0u;
    if (m_clampedStartMs >= 0) {
        startFrame = (int)((float)m_clampedStartMs / m_frameDurationMs);
        if ((int)m_totalFrames - 1 < startFrame) {
            startFrame = (int)m_totalFrames - 1;
        }
        if (m_samplesPerBlock == 1024u) {
            int remUs = (1000 * m_clampedStartMs) % (int)(m_frameDurationMs * 1000.0f);
            startSampleTrim = (uint32_t)((int)(((float)remUs / 1000.0f) / (m_frameDurationMs * 0.25f))) << 8;
        }
    }

    int endFrame = (int)m_totalFrames - 1;
    if (m_samplesPerBlock != 0u) {
        int loopEndFrameIdx = (int)(m_loopEndSample / m_samplesPerBlock);
        if (loopEndFrameIdx < endFrame) {
            endFrame = loopEndFrameIdx;
        }
    }

    if (m_loopJumpFrameIdx >= 0) {
        m_currentFrameIdx = m_loopJumpFrameIdx;
        m_loopJumpFrameIdx = -1;
    } else if (m_currentFrameIdx < 0) {
        initFramePosition(outPkt, startFrame, startSampleTrim, m_clampedStartMs);
    } else {
        m_currentFrameIdx += m_requestedSpeed;
        if (m_currentFrameIdx < startFrame || m_currentFrameIdx > endFrame) {
            return PARSER_ERR_EOS;
        }
    }

    syncMutexLock(&m_mutex);
    if (m_requestedSpeed == 0) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_PARAM;
    }
    if (m_requestedSpeed != m_activeSpeed) {
        m_activeCacheIo->setCacheHint(1, 0);
        m_activeSpeed = m_requestedSpeed;
    }
    syncMutexUnlock(&m_mutex);

    uint32_t byteOffset = m_blockAlign * (uint32_t)m_currentFrameIdx;
    void *rawData = NULL;
    uint32_t readBytes = 0;
    int readErr = wavCacheReadDataRange(m_activeCacheIo, byteOffset, m_blockAlign, &rawData, &readBytes);
    if (readErr != 0) {
        if (readErr == IO_ERR_READ_IO) {
            return PARSER_ERR_READ_IO;
        }
        if (readErr == IO_ERR_OPEN_IO) {
            return PARSER_ERR_OPEN_IO;
        }
        return -1;
    }

    if (outPkt->maxBufferSize < readBytes) {
        readBytes = outPkt->maxBufferSize;
    }
    sce_paf_memcpy(outPkt->frameBuffer, rawData, readBytes);

    outPkt->payloadSize = readBytes;
    outPkt->sampleRate = m_sampleRate;
    outPkt->channelCount = (uint8_t)m_channels;
    outPkt->timestampMs = (float)byteOffset / m_bytesPerMs;

    uint32_t loopStartFrame = m_loopStartSample / m_samplesPerBlock;
    uint32_t loopEndFrame = m_loopEndSample / m_samplesPerBlock;
    bool trimLoopStart = false;
    bool trimLoopEnd = false;

    if (m_completedLoops > 0
        && (m_loopCount == -1 || m_completedLoops < m_loopCount - 1)
        && (uint32_t)m_currentFrameIdx == loopStartFrame
        && (uint32_t)m_currentFrameIdx == loopEndFrame) {
        ++m_completedLoops;
        m_loopJumpFrameIdx = m_currentFrameIdx;
        trimLoopStart = true;
        trimLoopEnd = true;
    } else {
        if (m_completedLoops > 0 && (uint32_t)m_currentFrameIdx == loopStartFrame) {
            trimLoopStart = true;
        }
        if ((m_loopCount == -1 || m_completedLoops < m_loopCount - 1)
            && (uint32_t)m_currentFrameIdx == loopEndFrame) {
            ++m_completedLoops;
            m_loopJumpFrameIdx = (int)loopStartFrame;
            if (m_clampedStartMs > 0) {
                m_clampedStartMs = 0;
            }
            trimLoopEnd = true;
        }
    }

    if (trimLoopStart) {
        uint32_t loopStartRem = m_loopStartSample % m_samplesPerBlock;
        if (outPkt->trimStartSample < loopStartRem) {
            outPkt->trimStartSample = loopStartRem;
        }
    }
    if (trimLoopEnd) {
        uint32_t loopEndRem = m_loopEndSample % m_samplesPerBlock;
        if (loopEndRem < outPkt->trimEndSample) {
            outPkt->trimEndSample = loopEndRem;
        }
    }
    return 0;
}

/* 0x811FBD26 */
At9CodecConfigPayload *At9Parser::getCodecConfig(uint32_t *outSize)
{
    m_codecCfg.status = 0;
    sce_paf_memset(m_codecCfg.configData, 0, sizeof(m_codecCfg.configData));

    if (m_activeCacheIo->isOpen() != 1 && m_activeCacheIo->open() != 0) {
        m_codecCfg.status = IO_ERR_OPEN_FAIL;
        return &m_codecCfg;
    }

    At9FmtHeader fmt;
    fmt.formatTag = 0;
    fmt.channels = 0;
    fmt.sampleRate = 0;
    fmt.avgBytesPerSec = 0;
    fmt.blockAlign = 0;
    fmt.bitsPerSample = 16;
    fmt.cbSize = 0;
    fmt.samplesPerBlock = 0;
    fmt.channelMask = 0;
    fmt.versionInfo = 0;
    sce_paf_memset(fmt.subFormatGuid, 0, sizeof(fmt.subFormatGuid));
    sce_paf_memset(fmt.configData, 0, sizeof(fmt.configData));

    if (at9CacheReadFmtHeader(m_activeCacheIo, &fmt) != 0) {
        m_codecCfg.status = IO_ERR_OPEN_FAIL;
        sce_paf_memset(m_codecCfg.configData, 0, sizeof(m_codecCfg.configData));
    } else {
        sce_paf_memcpy(m_codecCfg.configData, fmt.configData, 4u);
        *outSize = sizeof(At9CodecConfigPayload);
    }
    return &m_codecCfg;
}

/* ===== At9Factory (0x811FAE28 - 0x811FB0FB) ===== */

class At9Factory {
public:
    At9Factory() {}
    virtual ~At9Factory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *unusedDecoderBuf, uint32_t *outAudioBuf);
    void *createPipeline(int mode, int p1, int p2, int p3, int p4, int p5, int loopCount);
    void *createMediaInfo(const char *path, int loopMode);
};

static At9Factory g_at9Factory; /* 0x8156F3FC */

/* 0x811FAE28 */
At9Factory *getAt9Factory()
{
    return &g_at9Factory;
}

/* 0x811FAE32 */
int At9Factory::getBufferSizes(uint32_t *outParserBuf, uint32_t *unusedDecoderBuf,
                               uint32_t *outAudioBuf)
{
    (void)unusedDecoderBuf;
    *outParserBuf = 54272u;
    *outAudioBuf = 0x4000u;
    return 0x4000;
}

/* 0x811FAE64 */
void *At9Factory::createPipeline(int mode, int p1, int p2, int p3, int p4, int p5, int loopCount)
{
    At9Parser *parser = NULL;
    void *parserMem = allocFromHeap(sizeof(At9Parser));
    if (parserMem != NULL) {
        parser = new (parserMem) At9Parser("SceShellMusicCoreAt9Parser", mode, loopCount);
    }

    void *decoder = NULL;
    void *decMem = allocSecondaryHeap(1196u);
    if (decMem != NULL) {
        decoder = at9DecoderConstruct(decMem, "SceShellMusicCoreAt9Decoder");
    }

    if (mode != MODE_LOCAL_PLAY && mode != MODE_STREAM_PLAY && mode != MODE_DIRECT_PLAY) {
        if (parser != NULL) {
            delete parser;
        }
        if (decoder != NULL) {
            ((FilterDestroyable *)decoder)->~FilterDestroyable();
            freeSecondaryHeap(decoder);
        }
        return NULL;
    }

    int pipeSlots = (mode == MODE_STREAM_PLAY) ? 64 : 6;
    void *decoderConsumer = (decoder != NULL) ? ((uint8_t *)decoder + 4) : NULL;
    void *pipe = createFixedFramePipe(512, pipeSlots, parser, decoderConsumer);
    void *audio = createAudioOutputFilter(4096, 16, decoder, p1, p2, p3, p4, p5);

    if (parser != NULL) {
        parserFilterSetOutputPipe(parser, pipe);
    }
    if (decoder != NULL) {
        decoderFilterSetInputPipe(decoder, pipe);
        void *audioConsumer = (audio != NULL) ? ((uint8_t *)audio + 4) : NULL;
        decoderFilterSetOutputSink(decoder, audioConsumer);
    }

    void *pipeline = createDecoderPipeline(parser, pipe, decoder, audio);
    if (pipeline == NULL || parser == NULL || decoder == NULL || audio == NULL || pipe == NULL) {
        if (pipeline != NULL) {
            delete (FilterDestroyable *)pipeline;
        }
        if (parser != NULL) {
            delete parser;
        }
        if (decoder != NULL) {
            ((FilterDestroyable *)decoder)->~FilterDestroyable();
            freeSecondaryHeap(decoder);
        }
        if (audio != NULL) {
            delete (FilterDestroyable *)audio;
        }
        return NULL;
    }

    FilterReadyCheck *audioReady = (FilterReadyCheck *)audio;
    FilterReadyCheck *pipeReady = (FilterReadyCheck *)pipe;
    if (decoderPipelineIsReady(pipeline)
        && audioReady->isReady() != 0
        && pipeReady->isReady() != 0) {
        ((ActiveFilterAttach *)((uint8_t *)parser + 4))->setEventListener(pipeline);
        ((ActiveFilterAttach *)((uint8_t *)decoder + 8))->setEventListener(pipeline);
        ((ActiveFilterAttach *)audio)->setEventListener(pipeline);
        return pipeline;
    }

    delete (FilterDestroyable *)pipeline;
    delete parser;
    ((FilterDestroyable *)decoder)->~FilterDestroyable();
    freeSecondaryHeap(decoder);
    delete (FilterDestroyable *)audio;
    delete (FilterDestroyable *)pipe;
    return NULL;
}

/* 0x811FB0AA */
void *At9Factory::createMediaInfo(const char *path, int loopMode)
{
    void *mem = allocFromHeap(2068u);
    if (mem == NULL) {
        return NULL;
    }
    return at9MediaInfoConstruct(mem, path, loopMode);
}

} /* namespace MusicCore */
} /* namespace sce */
