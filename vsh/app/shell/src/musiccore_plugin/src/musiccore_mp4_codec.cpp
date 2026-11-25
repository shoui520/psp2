/*
 * SceShell musiccore_plugin - Mp4Factory and Mp4Parser (MP4/AAC decoder
 * pipeline factory and frame-by-frame / trick-play parser filter).
 * Original address range (3.65-CEX): 0x811F8C7C - 0x811FA247
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
int sce_paf_strcmp(const char *lhs, const char *rhs);
}

namespace sce {
namespace MusicCore {

static const uint32_t FOURCC_MP4 = 0x2034504Du; /* ' MP4' */

static const int PARSER_ERR_OK = 0;
static const int PARSER_ERR_NO_SOURCE = 4;
static const int PARSER_ERR_EOS = 5;
static const int PARSER_ERR_OPEN_FAIL = 17;
static const int PARSER_ERR_CLOSE_FAIL = 18;
static const int PARSER_ERR_READ_IO = 19;
static const int PARSER_ERR_OPEN_IO = 20;
static const int PARSER_ERR_UNSUPPORTED_AUDIO = 21;
static const int PARSER_ERR_BAD_STATE = 33;
static const int PARSER_ERR_SYNC_LOST = 69;
static const int PARSER_ERR_UNSUPPORTED = 81;
static const int PARSER_ERR_BAD_DURATION = 82;
static const int PARSER_ERR_CORRUPT = 95;
static const int PARSER_ERR_BAD_PARAM = 241;

static const int MODE_LOCAL_PLAY = 1;
static const int MODE_STREAM_PLAY = 5;
static const int MODE_DIRECT_PLAY = 16;

static const uint32_t MP4_PARSER_HEAP_SIZE = 54272u;
static const uint32_t MP4_DECODER_HEAP_SIZE = 74752u;
static const uint32_t MP4_IO_HEAP_SIZE = 0x4000u;

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
extern void *mp4MediaInfoConstruct(void *mem, const char *path, int ioPriority);

/* Mp4StreamParser interface (implemented in musiccore_mp4_media.cpp) */
struct Mp4FrameInfo {
    void *frameData;          /* +0 */
    uint32_t frameSize;       /* +4 */
    uint32_t sampleRate;      /* +8 */
    uint32_t samplesPerFrame; /* +12 */
    uint8_t channelMode;      /* +16 */
    uint8_t channelCount;     /* +17 */
    uint8_t padChan[2];       /* +18..+19 */
    int frameFlags;           /* +20 */
    uint16_t leadingSkip;     /* +24 */
    uint16_t trailingSkip;    /* +26 */
};

struct Mp4StreamParserView {
    void *vtablePtr;
    uint32_t streamState;
    uint32_t totalSamples;
    int currentSampleIndex; /* +12 */
};

extern int mp4StreamOpen(Mp4StreamParserView *stream, uint32_t *outErr);
extern const char *mp4StreamGetPath(Mp4StreamParserView *stream);
extern void mp4StreamSetReadMode(Mp4StreamParserView *stream, int mode, int paramPrimary,
                                 int paramSecondary);
extern int mp4StreamGetDurationMs(Mp4StreamParserView *stream, int *outDurationMs,
                                  uint32_t *outErr);
extern int mp4StreamGetSampleRate(Mp4StreamParserView *stream, uint32_t *outSampleRate,
                                  uint32_t *outErr);
extern int mp4StreamGetExtSampleRate(Mp4StreamParserView *stream, uint32_t *outExtRate,
                                     uint32_t *outErr);
extern int mp4StreamGetSbrFlag(Mp4StreamParserView *stream, uint8_t *outSbrFlag,
                               uint32_t *outErr);
extern int mp4StreamGetSamplesPerFrame(Mp4StreamParserView *stream, uint16_t *outSamples,
                                       uint32_t *outErr);
extern int mp4StreamReadFirstFrame(Mp4StreamParserView *stream, Mp4FrameInfo *outFrame,
                                   uint32_t *outErr);
extern int mp4StreamReadNextFrame(Mp4StreamParserView *stream, int skipFrames,
                                  Mp4FrameInfo *outFrame, uint32_t *outErr);
extern int mp4StreamReadPrevFrame(Mp4StreamParserView *stream, int skipFrames,
                                  Mp4FrameInfo *outFrame, uint32_t *outErr);
extern int mp4StreamSeekAndReadFrame(Mp4StreamParserView *stream, uint32_t targetMs,
                                     Mp4FrameInfo *outFrame, int *outFrameIndex,
                                     uint32_t *outErr);
extern int mp4StreamQueryMetadata(Mp4StreamParserView *stream, int titleCap, int artistCap,
                                  int albumCap, int genreCap, int artCap, void *metaOut,
                                  int extraCap, uint32_t *outErr);

/* Cache I/O interface matching LocalCacheIo (musiccore_sync_cache_io.cpp) */
class LocalCacheIoInterface {
public:
    virtual ~LocalCacheIoInterface() {}
    virtual int isInitialized() const = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() const = 0;
    virtual int getFileSize() const = 0;
    virtual const char *getPath() const = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(uint32_t priorityMode) = 0;
};

struct Mp4MediaInfoView : public MediaInfoHandle {
    uint8_t pathStorage[2052];
    Mp4StreamParserView *streamParser; /* +2064 */
    LocalCacheIoInterface *cacheIo;    /* +2068 */
};

/* Pipeline / filter infrastructure */
extern void parserFilterBaseInit(void *self, int filterKind, const char *threadName);
extern void parserFilterBaseDestroy(void *self);
extern int activeFilterIsReady(const void *activeBase);
extern void parserFilterSetOutputPipe(void *parser, void *pipe);
extern void decoderFilterSetInputPipe(void *decoder, void *pipe);
extern void decoderFilterSetOutputSink(void *decoder, void *audioSink);
extern void *mp4DecoderConstruct(void *mem, const char *threadName);
extern void *createBufRefPipe(int slotCount, void *producer, void *consumer);
extern void *createAudioOutputFilter(int grainSize, int slotCount, void *producer,
                                     int audioOptPrimary, int audioOptSecondary,
                                     int audioOptVolume, int audioOptBalance,
                                     int audioOptChannel);
extern void *createDecoderPipeline(void *parser, void *pipe, void *decoder, void *audioSink);
extern bool decoderPipelineIsReady(const void *pipeline);

class FilterLifecycleIf {
public:
    virtual ~FilterLifecycleIf() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
};

class ActiveFilterListenerIf {
public:
    virtual ~ActiveFilterListenerIf() {}
    virtual void setEventListener(void *listener) = 0;
};

class DecoderFilterView {
public:
    virtual ~DecoderFilterView() {}
    virtual int openDecoder() = 0;
    virtual int closeDecoder() = 0;
    virtual int decodePacket(void *inputPacket, void *outputPcm) = 0;
    virtual int flushDecoder() = 0;
    virtual int resetDecoder() = 0;
    virtual int setInputPipe(void *pipe) = 0;
    virtual int setOutputSink(void *sink) = 0;
    virtual int setActiveMode(int mode) = 0;
    virtual int getCodecType() = 0;
    virtual int setCodecConfig(void *configData, uint32_t configSize) = 0;
    virtual int isReady() = 0;

    void *asPipeConsumer() { return (uint8_t *)this + 4; }
    ActiveFilterListenerIf *asActiveFilter() { return (ActiveFilterListenerIf *)((uint8_t *)this + 8); }
};

struct ParserPacketOut {
    void *frameData;            /* +0 */
    uint32_t maxBufferSize;     /* +4 */
    uint32_t payloadSize;       /* +8 */
    float timestampMs;          /* +12 */
    float frameDurationMs;      /* +16 */
    uint32_t sampleRate;        /* +20 */
    int channelConfig;          /* +24 */
    int packetFlags;            /* +28: 0=normal, 1=trick, 2=eos, 4=skip */
    uint8_t discontinuity;      /* +32 */
    uint8_t padDiscont[3];      /* +33..+35 */
    int frameFlags;             /* +36 */
    uint16_t leadingSkip;       /* +40 */
    uint16_t trailingSkip;      /* +42 */
    uint8_t padMid[20];         /* +44..+63 */
    uint8_t channelCount;       /* +64 */
};

struct Mp4CodecConfigPayload {
    uint32_t outputSampleRate;  /* +116 */
    uint32_t coreSampleRate;    /* +120 */
    uint32_t errorCode;         /* +124 */
    uint8_t sbrFlag;            /* +128 */
    uint8_t padConfig[3];       /* +129..+131 */
};

/* Cached last MP4 sample rate for fast trick-play skipping (0x8154500C) */
static uint32_t g_lastMp4SampleRate = 0;

/* 0x811F9214 */
int mapMp4ParserError(uint32_t errCode)
{
    if (errCode <= 0x8010A007u) {
        if (errCode == 0u) {
            return PARSER_ERR_OK;
        }
        if (errCode > 0x8010A000u) {
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
            case 0x8010A006u:
                return -1;
            case 0x8010A007u:
                return PARSER_ERR_EOS;
            default:
                break;
            }
        }
        return -1;
    }

    if (errCode <= 0x80280020u) {
        if (errCode == 0x8010AFFFu) {
            return PARSER_ERR_NO_SOURCE;
        }
        if (errCode == 0x8010AFF1u) {
            return PARSER_ERR_BAD_PARAM;
        }
        return -1;
    }

    if (errCode > 0x8028002Au) {
        return -1;
    }

    switch (errCode) {
    case 0x80280021u:
    case 0x80280023u:
        return PARSER_ERR_UNSUPPORTED;
    case 0x80280022u:
    case 0x80280028u:
        return PARSER_ERR_CORRUPT;
    case 0x80280024u:
    case 0x80280025u:
        return PARSER_ERR_BAD_STATE;
    case 0x80280026u:
    case 0x80280027u:
        return -1;
    case 0x80280029u:
        return PARSER_ERR_SYNC_LOST;
    case 0x8028002Au:
        return PARSER_ERR_BAD_DURATION;
    default:
        return -1;
    }
}

static int mapMp4OpenError(uint32_t errCode)
{
    switch (errCode) {
    case 0x8010A001u:
        return PARSER_ERR_OPEN_FAIL;
    case 0x80280021u:
        return PARSER_ERR_UNSUPPORTED;
    case 0x80280022u:
        return PARSER_ERR_CORRUPT;
    case 0x8028002Au:
        return PARSER_ERR_UNSUPPORTED_AUDIO;
    case 0xFFFFFFFFu:
        return -1;
    default:
        return mapMp4ParserError(errCode);
    }
}

static int mapMp4ReadIoError(uint32_t errCode)
{
    switch (errCode) {
    case 0x8010A003u:
        return PARSER_ERR_OPEN_IO;
    case 0x8010A004u:
        return PARSER_ERR_READ_IO;
    case 0x80280021u:
        return PARSER_ERR_UNSUPPORTED;
    case 0x80280022u:
        return PARSER_ERR_CORRUPT;
    case 0xFFFFFFFFu:
        return -1;
    default:
        return mapMp4ParserError(errCode);
    }
}

/* ===== Mp4Parser (0x811F8F24 - 0x811FA247, size 136 bytes) ===== */

class Mp4Parser {
public:
    Mp4Parser(const char *threadName, int playMode);
    virtual ~Mp4Parser();

    bool isReady() const;
    int findCachedMediaInfo(const char *path, Mp4MediaInfoView **outInfo);
    int setMediaInfo(Mp4MediaInfoView *mediaInfo);
    int prefetchMediaInfo(Mp4MediaInfoView *mediaInfo);
    Mp4CodecConfigPayload *getCodecConfig(uint32_t *outSize);
    void setPlaySpeed(int speed);
    int setStartPosition(int startMs);
    int setEndPosition(int endMs);
    int getDurationMs();
    int seekPosition(int positionMs);
    int resetPosition();
    int queryMetadata(void *metaOut);
    int onStart();
    int onStop();
    int readFrame(ParserPacketOut *outPkt);

    ActiveFilterListenerIf *asActiveFilter()
    {
        return (ActiveFilterListenerIf *)((uint8_t *)this + 4);
    }

    uint8_t m_basePrefix[16];            /* +4..+19 */
    SyncMutexStorage m_mutex;            /* +20..+27 */
    uint32_t m_baseStateWord;            /* +28 */
    int m_filterState;                   /* +32 */
    uint8_t m_baseTail[12];              /* +36..+47 */
    int m_playMode;                      /* +48 */
    int m_requestedSpeed;                /* +52 */
    int m_startPosMs;                    /* +56 */
    int m_endPosMs;                      /* +60 */
    int m_durationMs;                    /* +64 */
    int m_activeSpeed;                   /* +68 */
    int m_currentFrameIndex;             /* +72 */
    float m_frameDurationMs;             /* +76 */
    uint8_t m_seekPending;               /* +80 */
    uint8_t m_padSeek[3];                /* +81..+83 */
    uint32_t m_unusedStreamSlot;         /* +84 */
    LocalCacheIoInterface *m_activeCacheIo; /* +88 */
    LocalCacheIoInterface *m_nextCacheIo;   /* +92 */
    Mp4StreamParserView *m_activeStream; /* +96 */
    Mp4StreamParserView *m_nextStream;   /* +100 */
    Mp4MediaInfoView *m_activeMedia;     /* +104 */
    Mp4MediaInfoView *m_nextMedia;       /* +108 */
    uint8_t m_parserInitialized;         /* +112 */
    uint8_t m_padInit[3];                /* +113..+115 */
    Mp4CodecConfigPayload m_codecConfig; /* +116..+131 */
    int m_trickFrameCounter;             /* +132 */
};

/* 0x811F8F24 */
Mp4Parser::Mp4Parser(const char *threadName, int playMode)
    : m_playMode(playMode)
    , m_requestedSpeed(0)
    , m_startPosMs(-1)
    , m_endPosMs(-1)
    , m_durationMs(-1)
    , m_activeSpeed(0)
    , m_currentFrameIndex(-1)
    , m_frameDurationMs(0.0f)
    , m_seekPending(0)
    , m_unusedStreamSlot(0)
    , m_activeCacheIo(NULL)
    , m_nextCacheIo(NULL)
    , m_activeStream(NULL)
    , m_nextStream(NULL)
    , m_activeMedia(NULL)
    , m_nextMedia(NULL)
    , m_parserInitialized(1)
    , m_trickFrameCounter(0)
{
    parserFilterBaseInit(this, 1, threadName);
    m_codecConfig.outputSampleRate = 0;
    m_codecConfig.coreSampleRate = 0;
    m_codecConfig.errorCode = 0;
    m_codecConfig.sbrFlag = 0;
}

/* 0x811F8F8C / 0x811F9010 */
Mp4Parser::~Mp4Parser()
{
    if (m_nextMedia != NULL) {
        if (m_activeMedia == m_nextMedia) {
            m_activeMedia = NULL;
            m_activeCacheIo = NULL;
            m_activeStream = NULL;
        }
        MediaInfoHandle *nextHandle = m_nextMedia;
        decoderManagerReleaseMediaInfo(&nextHandle, 1);
        m_nextMedia = NULL;
        m_nextCacheIo = NULL;
        m_nextStream = NULL;
    }

    if (m_activeMedia != NULL) {
        MediaInfoHandle *curHandle = m_activeMedia;
        decoderManagerReleaseMediaInfo(&curHandle, 1);
        m_activeMedia = NULL;
        m_activeCacheIo = NULL;
        m_activeStream = NULL;
    }

    m_codecConfig.sbrFlag = 0;
    m_codecConfig.coreSampleRate = 0;
    m_codecConfig.errorCode = 0;
    m_codecConfig.outputSampleRate = 0;
    parserFilterBaseDestroy(this);
}

/* 0x811F9020 */
bool Mp4Parser::isReady() const
{
    if (m_parserInitialized == 0) {
        return false;
    }
    return activeFilterIsReady((const uint8_t *)this + 4) != 0;
}

/* 0x811F903A */
int Mp4Parser::findCachedMediaInfo(const char *path, Mp4MediaInfoView **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    syncMutexLock(&m_mutex);
    if (m_activeStream != NULL && sce_paf_strcmp(mp4StreamGetPath(m_activeStream), path) == 0) {
        syncMutexUnlock(&m_mutex);
        *outInfo = m_activeMedia;
        return PARSER_ERR_OK;
    }
    if (m_nextStream != NULL && sce_paf_strcmp(mp4StreamGetPath(m_nextStream), path) == 0) {
        *outInfo = m_nextMedia;
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_OK;
    }

    syncMutexUnlock(&m_mutex);
    *outInfo = NULL;
    return PARSER_ERR_OK;
}

/* 0x811F90A6 */
int Mp4Parser::setMediaInfo(Mp4MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_MP4) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->streamParser == NULL || mediaInfo->cacheIo == NULL) {
        return -1;
    }

    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }

    const char *newPath = mp4StreamGetPath(mediaInfo->streamParser);
    if (m_activeStream != NULL) {
        if (sce_paf_strcmp(mp4StreamGetPath(m_activeStream), newPath) == 0) {
            if (m_activeStream == m_nextStream) {
                m_nextStream = NULL;
                m_nextCacheIo = NULL;
                m_nextMedia = NULL;
            }
            syncMutexUnlock(&m_mutex);
            delayWorkerThread(20000u);
            return PARSER_ERR_OK;
        }

        if (m_activeMedia != NULL) {
            MediaInfoHandle *oldHandle = m_activeMedia;
            decoderManagerReleaseMediaInfo(&oldHandle, 1);
            m_activeMedia = NULL;
        }
        if (m_nextStream == m_activeStream) {
            m_nextStream = NULL;
            m_activeMedia = NULL;
        }
        m_activeStream = NULL;
        m_activeCacheIo = NULL;
    }

    m_currentFrameIndex = -1;

    if (m_nextStream != NULL) {
        if (sce_paf_strcmp(mp4StreamGetPath(m_nextStream), newPath) == 0) {
            m_activeStream = m_nextStream;
            m_activeMedia = m_nextMedia;
            m_activeCacheIo = m_nextCacheIo;
            if (m_activeCacheIo != NULL) {
                m_activeCacheIo->setPriorityMode(0u);
            }
            m_nextStream = NULL;
            m_nextCacheIo = NULL;
            m_nextMedia = NULL;
            m_durationMs = -1;
            syncMutexUnlock(&m_mutex);
            return PARSER_ERR_OK;
        }

        if (m_nextMedia != NULL) {
            MediaInfoHandle *oldNext = m_nextMedia;
            decoderManagerReleaseMediaInfo(&oldNext, 1);
            m_nextMedia = NULL;
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
    }

    if (m_activeMedia != NULL || m_nextMedia != NULL) {
        syncMutexUnlock(&m_mutex);
        return -1;
    }

    mediaInfo->sharedFlag = 1;
    m_activeMedia = mediaInfo;
    m_activeStream = mediaInfo->streamParser;
    m_activeCacheIo = mediaInfo->cacheIo;
    m_durationMs = -1;
    syncMutexUnlock(&m_mutex);
    return PARSER_ERR_OK;
}

/* 0x811F92F6 */
int Mp4Parser::prefetchMediaInfo(Mp4MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_MP4) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->streamParser == NULL || mediaInfo->cacheIo == NULL) {
        return -1;
    }

    syncMutexLock(&m_mutex);
    if (m_playMode == MODE_STREAM_PLAY) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_OK;
    }

    const char *newPath = mp4StreamGetPath(mediaInfo->streamParser);
    if (m_nextStream != NULL) {
        if (sce_paf_strcmp(mp4StreamGetPath(m_nextStream), newPath) == 0) {
            syncMutexUnlock(&m_mutex);
            return PARSER_ERR_OK;
        }

        if (m_activeStream != NULL
            && sce_paf_strcmp(mp4StreamGetPath(m_nextStream), mp4StreamGetPath(m_activeStream)) == 0) {
            m_nextStream = m_activeStream;
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            syncMutexUnlock(&m_mutex);
            return PARSER_ERR_OK;
        }

        if (m_activeStream == m_nextStream) {
            m_nextMedia = NULL;
        } else if (m_nextMedia != NULL) {
            MediaInfoHandle *oldNext = m_nextMedia;
            decoderManagerReleaseMediaInfo(&oldNext, 1);
            m_nextMedia = NULL;
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
    }

    if (m_nextMedia != NULL) {
        syncMutexUnlock(&m_mutex);
        return -1;
    }

    mediaInfo->sharedFlag = 1;
    m_nextMedia = mediaInfo;
    m_nextStream = mediaInfo->streamParser;
    m_nextCacheIo = mediaInfo->cacheIo;
    m_nextCacheIo->setPriorityMode(1u);

    uint32_t streamErr = 0xFFFFFFFFu;
    int durationMs = 0;
    if (mp4StreamOpen(m_nextStream, &streamErr) < 0) {
        syncMutexUnlock(&m_mutex);
        return mapMp4OpenError(streamErr);
    }

    if (mp4StreamGetDurationMs(m_nextStream, &durationMs, &streamErr) != 0) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_OK;
    }

    int mappedErr = mapMp4ReadIoError(streamErr);
    syncMutexUnlock(&m_mutex);
    return mappedErr;
}

/* 0x811F94EA */
Mp4CodecConfigPayload *Mp4Parser::getCodecConfig(uint32_t *outSize)
{
    uint32_t openErr = 0xFFFFFFFFu;
    m_codecConfig.sbrFlag = 0;
    m_codecConfig.coreSampleRate = 0;
    m_codecConfig.errorCode = 0;
    m_codecConfig.outputSampleRate = 0;

    mp4StreamOpen(m_activeStream, &openErr);
    if (openErr != 0u) {
        m_codecConfig.errorCode = openErr;
        m_codecConfig.coreSampleRate = 0;
        m_codecConfig.outputSampleRate = 0;
        m_codecConfig.sbrFlag = 0;
        *outSize = sizeof(Mp4CodecConfigPayload);
        return &m_codecConfig;
    }

    if (m_codecConfig.errorCode == 0u) {
        uint32_t coreRate = 0;
        mp4StreamGetSampleRate(m_activeStream, &coreRate, &m_codecConfig.errorCode);
        m_codecConfig.coreSampleRate = coreRate;
        if (m_codecConfig.errorCode == 0u) {
            uint8_t sbrFlag = 0;
            mp4StreamGetSbrFlag(m_activeStream, &sbrFlag, &m_codecConfig.errorCode);
            m_codecConfig.sbrFlag = sbrFlag;
            if (m_codecConfig.errorCode == 0u) {
                if (m_codecConfig.sbrFlag == 1u) {
                    uint32_t extRate = 0;
                    mp4StreamGetExtSampleRate(m_activeStream, &extRate, &m_codecConfig.errorCode);
                    if (m_codecConfig.errorCode != 0u) {
                        m_codecConfig.errorCode = 0;
                        m_codecConfig.outputSampleRate = m_codecConfig.coreSampleRate;
                    } else if (extRate != 0u) {
                        m_codecConfig.outputSampleRate = extRate;
                    } else {
                        m_codecConfig.outputSampleRate = m_codecConfig.coreSampleRate;
                    }
                } else {
                    m_codecConfig.outputSampleRate = m_codecConfig.coreSampleRate;
                }
            }
        }
    }

    *outSize = sizeof(Mp4CodecConfigPayload);
    return &m_codecConfig;
}

/* 0x811F95C8 */
void Mp4Parser::setPlaySpeed(int speed)
{
    m_requestedSpeed = speed;
}

/* 0x811F95CC */
int Mp4Parser::setStartPosition(int startMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_startPosMs = startMs;
    m_currentFrameIndex = -1;
    syncMutexUnlock(&m_mutex);
    return PARSER_ERR_OK;
}

/* 0x811F95FE */
int Mp4Parser::setEndPosition(int endMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_endPosMs = endMs;
    m_currentFrameIndex = -1;
    syncMutexUnlock(&m_mutex);
    return PARSER_ERR_OK;
}

/* 0x811F9630 */
int Mp4Parser::getDurationMs()
{
    syncMutexLock(&m_mutex);
    if (m_durationMs >= 0) {
        int cached = m_durationMs;
        syncMutexUnlock(&m_mutex);
        return cached;
    }
    if (m_activeStream == NULL) {
        syncMutexUnlock(&m_mutex);
        return -1;
    }

    uint32_t streamErr = 0xFFFFFFFFu;
    if (mp4StreamOpen(m_activeStream, &streamErr) < 0) {
        syncMutexUnlock(&m_mutex);
        if (streamErr == 0x80280021u) {
            return PARSER_ERR_UNSUPPORTED;
        }
        if (streamErr == 0x80280022u) {
            return PARSER_ERR_CORRUPT;
        }
        return (int)streamErr;
    }

    if (mp4StreamGetDurationMs(m_activeStream, &m_durationMs, &streamErr) != 0) {
        int duration = m_durationMs;
        syncMutexUnlock(&m_mutex);
        return duration;
    }

    syncMutexUnlock(&m_mutex);
    if (streamErr == 0x80280021u || streamErr == 0x80280022u || streamErr == 0x80280028u) {
        return 0;
    }
    return (int)streamErr;
}

/* 0x811F9706 */
int Mp4Parser::seekPosition(int positionMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }

    if (positionMs >= 0) {
        if (m_frameDurationMs > 0.0f) {
            int targetFrame = (int)((float)positionMs / m_frameDurationMs);
            if (targetFrame != m_currentFrameIndex) {
                m_seekPending = 1;
                m_currentFrameIndex = targetFrame;
            }
        } else if (m_currentFrameIndex != -1) {
            m_seekPending = 1;
            m_currentFrameIndex = -1;
        }
    }

    syncMutexUnlock(&m_mutex);
    return PARSER_ERR_OK;
}

/* 0x811F9794 */
int Mp4Parser::resetPosition()
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_currentFrameIndex = -1;
    syncMutexUnlock(&m_mutex);
    return PARSER_ERR_OK;
}

/* 0x811F97C2 */
int Mp4Parser::queryMetadata(void *metaOut)
{
    if (metaOut == NULL) {
        return PARSER_ERR_OK;
    }
    uint32_t streamErr = 0xFFFFFFFFu;
    mp4StreamQueryMetadata(m_activeStream, -1, -1, -1, -1, -1, metaOut, -1, &streamErr);
    return (streamErr != 0u) ? -1 : PARSER_ERR_OK;
}

/* 0x811F9816 */
int Mp4Parser::onStart()
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }

    m_activeCacheIo->setPriorityMode(0u);
    uint32_t streamErr = 0xFFFFFFFFu;
    if (mp4StreamOpen(m_activeStream, &streamErr) >= 0) {
        m_activeSpeed = 0;
        return PARSER_ERR_OK;
    }
    return mapMp4OpenError(streamErr);
}

/* 0x811F98B2 */
int Mp4Parser::onStop()
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    m_activeCacheIo->setPriorityMode(1u);
    return PARSER_ERR_OK;
}

/* 0x811F98CA */
int Mp4Parser::readFrame(ParserPacketOut *outPkt)
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (outPkt == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    Mp4FrameInfo frame;
    frame.frameData = NULL;
    frame.frameSize = outPkt->maxBufferSize;
    frame.sampleRate = 0;
    frame.samplesPerFrame = 0;
    frame.channelMode = 0;
    frame.channelCount = 0;
    frame.frameFlags = 0;
    frame.leadingSkip = 0;
    frame.trailingSkip = 0;

    uint32_t streamErr = 0xFFFFFFFFu;
    bool fastStreamSkip = (m_playMode == MODE_STREAM_PLAY
                           && (m_requestedSpeed > 20 || m_requestedSpeed < -20));

    syncMutexLock(&m_mutex);
    Mp4StreamParserView *stream = m_activeStream;
    if (m_requestedSpeed == 0
        || (m_playMode != MODE_DIRECT_PLAY && m_playMode != MODE_STREAM_PLAY
            && m_playMode != MODE_LOCAL_PLAY)) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_PARAM;
    }

    int playMode = m_playMode;
    if (m_requestedSpeed != m_activeSpeed) {
        if (m_requestedSpeed == 1) {
            mp4StreamSetReadMode(stream, 1, 0, 0);
        } else if (m_requestedSpeed > 0) {
            mp4StreamSetReadMode(stream, 2, 0, 0);
        } else {
            mp4StreamSetReadMode(stream, 4, 0, 0);
        }
        m_activeSpeed = m_requestedSpeed;
    }
    syncMutexUnlock(&m_mutex);

    stream = m_activeStream;
    int readOk = 0;

    if (m_currentFrameIndex < 0) {
        if (mp4StreamGetDurationMs(stream, &m_durationMs, &streamErr) == 0) {
            return mapMp4ReadIoError(streamErr);
        }
        if (m_durationMs < 0) {
            return PARSER_ERR_BAD_DURATION;
        }

        if (m_activeSpeed <= 0) {
            if (m_endPosMs >= 0) {
                if ((float)m_endPosMs > (float)m_durationMs * 1000.0f) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                int soughtFrame = -1;
                readOk = mp4StreamSeekAndReadFrame(m_activeStream, (uint32_t)m_endPosMs,
                                                   &frame, &soughtFrame, &streamErr);
                if (readOk != 0) {
                    m_currentFrameIndex = soughtFrame;
                    outPkt->discontinuity = 1;
                    m_frameDurationMs = ((float)frame.samplesPerFrame * 1000.0f) / (float)frame.sampleRate;
                    if (playMode == MODE_LOCAL_PLAY || playMode == MODE_DIRECT_PLAY) {
                        m_trickFrameCounter = 1;
                    } else if (playMode == MODE_STREAM_PLAY) {
                        m_trickFrameCounter = 20;
                    }
                }
            } else {
                uint32_t sampleRate = 0;
                uint16_t samplesPerFrame = 0;
                if (mp4StreamGetSampleRate(m_activeStream, &sampleRate, &streamErr) == 0) {
                    return mapMp4ReadIoError(streamErr);
                }
                if (mp4StreamGetSamplesPerFrame(m_activeStream, &samplesPerFrame, &streamErr) == 0) {
                    return mapMp4ReadIoError(streamErr);
                }
                float tailOffsetMs = ((float)samplesPerFrame * 1000.0f) / (float)sampleRate;
                uint32_t startSeekMs = (uint32_t)((float)(uint32_t)m_durationMs - tailOffsetMs);
                int soughtFrame = -1;
                readOk = mp4StreamSeekAndReadFrame(m_activeStream, startSeekMs,
                                                   &frame, &soughtFrame, &streamErr);
                if (readOk != 0) {
                    m_currentFrameIndex = soughtFrame;
                    outPkt->discontinuity = 1;
                    m_frameDurationMs = ((float)frame.samplesPerFrame * 1000.0f) / (float)frame.sampleRate;
                    if (playMode == MODE_LOCAL_PLAY || playMode == MODE_DIRECT_PLAY) {
                        m_trickFrameCounter = 1;
                    } else if (playMode == MODE_STREAM_PLAY) {
                        m_trickFrameCounter = 20;
                    }
                }
            }
        } else if (m_startPosMs >= 0) {
            int soughtFrame = -1;
            readOk = mp4StreamSeekAndReadFrame(m_activeStream, (uint32_t)m_startPosMs,
                                               &frame, &soughtFrame, &streamErr);
            if (readOk != 0) {
                m_currentFrameIndex = soughtFrame;
                outPkt->discontinuity = 1;
                m_frameDurationMs = ((float)frame.samplesPerFrame * 1000.0f) / (float)frame.sampleRate;
                m_trickFrameCounter = (m_activeSpeed != 1) ? 1 : 0;
            }
        } else {
            readOk = mp4StreamReadFirstFrame(m_activeStream, &frame, &streamErr);
            if (readOk != 0) {
                m_currentFrameIndex = 1;
                outPkt->discontinuity = 0;
                m_frameDurationMs = ((float)frame.samplesPerFrame * 1000.0f) / (float)frame.sampleRate;
                m_trickFrameCounter = (m_activeSpeed != 1) ? 1 : 0;
            }
        }
    } else {
        int speed = m_activeSpeed;
        if (speed <= 0) {
            if (m_startPosMs >= (int)((float)m_currentFrameIndex * m_frameDurationMs)) {
                outPkt->payloadSize = 0;
                outPkt->packetFlags = 2;
                return PARSER_ERR_EOS;
            }
        } else if (m_endPosMs >= 0
                   && m_endPosMs <= (int)((float)m_currentFrameIndex * m_frameDurationMs)) {
            outPkt->payloadSize = 0;
            outPkt->packetFlags = 2;
            return PARSER_ERR_EOS;
        }

        if (fastStreamSkip) {
            int nextPosMs = (int)((float)(m_currentFrameIndex + speed) * m_frameDurationMs);
            if (nextPosMs > 0 && nextPosMs < m_durationMs) {
                if (g_lastMp4SampleRate == 0u) {
                    g_lastMp4SampleRate = 44100u;
                }
                outPkt->payloadSize = 0;
                outPkt->frameFlags = 0;
                outPkt->timestampMs = (float)m_currentFrameIndex * m_frameDurationMs;
                outPkt->sampleRate = g_lastMp4SampleRate;
                outPkt->packetFlags = 4;
                outPkt->frameDurationMs = m_frameDurationMs;
                outPkt->channelCount = frame.channelCount;
                m_currentFrameIndex += speed;
                int updatedIdx = stream->currentSampleIndex + speed;
                stream->currentSampleIndex = (updatedIdx < 0) ? 0 : updatedIdx;
                return PARSER_ERR_OK;
            }
        }

        if (m_seekPending != 0u) {
            if (speed < 0) {
                if (m_endPosMs >= 0) {
                    if (m_endPosMs < (int)((float)m_currentFrameIndex * m_frameDurationMs)) {
                        m_currentFrameIndex = (int)((float)m_endPosMs / m_frameDurationMs);
                    }
                } else {
                    uint32_t dur = (uint32_t)m_durationMs;
                    if (dur < (uint32_t)((float)m_currentFrameIndex * m_frameDurationMs)) {
                        m_currentFrameIndex = (int)(((float)dur - m_frameDurationMs) / m_frameDurationMs);
                    }
                }
            }
            uint32_t seekMs = (uint32_t)((float)m_currentFrameIndex * m_frameDurationMs);
            int soughtFrame = -1;
            readOk = mp4StreamSeekAndReadFrame(stream, seekMs, &frame, &soughtFrame, &streamErr);
            if (readOk != 0) {
                m_currentFrameIndex = soughtFrame;
                outPkt->discontinuity = 1;
                m_frameDurationMs = ((float)frame.samplesPerFrame * 1000.0f) / (float)frame.sampleRate;
                if (m_activeSpeed == 1) {
                    m_trickFrameCounter = 0;
                } else if (m_activeSpeed > 0) {
                    m_trickFrameCounter = 1;
                } else if (playMode == MODE_LOCAL_PLAY || playMode == MODE_DIRECT_PLAY) {
                    m_trickFrameCounter = 1;
                } else if (playMode == MODE_STREAM_PLAY) {
                    m_trickFrameCounter = 20;
                }
            }
        } else if (speed == 1) {
            readOk = mp4StreamReadNextFrame(stream, 0, &frame, &streamErr);
            if (readOk != 0) {
                m_trickFrameCounter = 0;
                m_currentFrameIndex += 1;
                outPkt->discontinuity = 0;
            }
        } else {
            int nextFrameIndex = m_currentFrameIndex + speed;
            if (speed > 0) {
                if (m_endPosMs >= 0
                    && m_endPosMs <= (int)((float)nextFrameIndex * m_frameDurationMs)) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
            } else {
                if (m_startPosMs >= (int)((float)nextFrameIndex * m_frameDurationMs)) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
            }

            bool needBurstJump = false;
            if (playMode == MODE_DIRECT_PLAY || playMode == MODE_LOCAL_PLAY) {
                needBurstJump = (m_trickFrameCounter > 0);
            } else if (playMode == MODE_STREAM_PLAY) {
                needBurstJump = (m_trickFrameCounter >= 20);
            }

            if (!needBurstJump) {
                readOk = mp4StreamReadNextFrame(stream, 0, &frame, &streamErr);
                if (readOk != 0) {
                    m_currentFrameIndex = (nextFrameIndex <= 0) ? 1 : nextFrameIndex;
                    outPkt->discontinuity = 0;
                    m_trickFrameCounter += 1;
                }
            } else {
                if (speed <= 0) {
                    int skipCount = (playMode == MODE_STREAM_PLAY) ? (-20 * speed - 20) : (-speed - 1);
                    readOk = mp4StreamReadPrevFrame(stream, skipCount, &frame, &streamErr);
                } else {
                    int skipCount = (playMode == MODE_STREAM_PLAY) ? (20 * speed - 20) : (speed - 1);
                    readOk = mp4StreamReadNextFrame(stream, skipCount, &frame, &streamErr);
                }
                if (readOk != 0) {
                    m_currentFrameIndex = (nextFrameIndex <= 0) ? 1 : nextFrameIndex;
                    outPkt->discontinuity = 1;
                    m_trickFrameCounter = 1;
                }
            }
        }
    }

    if (readOk != 0) {
        m_seekPending = 0;
        g_lastMp4SampleRate = m_codecConfig.coreSampleRate;
        outPkt->frameData = frame.frameData;
        outPkt->payloadSize = frame.frameSize;
        outPkt->timestampMs = (float)m_currentFrameIndex * m_frameDurationMs;
        outPkt->frameFlags = frame.frameFlags;
        uint32_t validSamples = frame.samplesPerFrame - frame.leadingSkip - frame.trailingSkip;
        outPkt->frameDurationMs = ((float)validSamples * 1000.0f) / (float)frame.sampleRate;
        outPkt->sampleRate = frame.sampleRate;
        outPkt->channelCount = frame.channelCount;

        if (frame.channelMode <= 1u) {
            outPkt->channelConfig = (frame.channelMode > 0u) ? 1 : 0;
        } else if (frame.channelMode <= 3u) {
            outPkt->channelConfig = 2;
        } else {
            return -1;
        }

        outPkt->leadingSkip = frame.leadingSkip;
        outPkt->trailingSkip = frame.trailingSkip;
        outPkt->packetFlags = (m_activeSpeed != 1) ? 1 : 0;
        return PARSER_ERR_OK;
    }

    if (streamErr == 0x8010A007u) {
        outPkt->payloadSize = 0;
        outPkt->packetFlags = 2;
        return PARSER_ERR_EOS;
    }

    switch (streamErr) {
    case 0x8010A003u:
        return PARSER_ERR_OPEN_IO;
    case 0x8010A004u:
        return PARSER_ERR_READ_IO;
    case 0x80280021u:
        return PARSER_ERR_UNSUPPORTED;
    case 0x80280022u:
    case 0x80280028u:
        return PARSER_ERR_CORRUPT;
    case 0x80280029u:
        return PARSER_ERR_SYNC_LOST;
    case 0xFFFFFFFFu:
        return -1;
    default:
        return mapMp4ParserError(streamErr);
    }
}

/* ===== Mp4Factory (0x811F8C7C - 0x811F8F23) ===== */

class Mp4Factory {
public:
    Mp4Factory();
    virtual ~Mp4Factory();

    virtual uint32_t getHeapSizes(uint32_t *outParserHeap, uint32_t *outDecoderHeap,
                                  uint32_t *outIoHeap);
    virtual void onReservedInit();
    virtual void onReservedClose();
    virtual void *createPipeline(int playMode, int audioOptPrimary, int audioOptSecondary,
                                 int audioOptVolume, int audioOptBalance, int audioOptChannel);
    virtual void *createMediaInfo(const char *path, int ioPriority);
};

/* 0x811F8C9C */
Mp4Factory::Mp4Factory()
{
}

/* 0x811F8CAC */
Mp4Factory::~Mp4Factory()
{
}

/* 0x811F8C86 */
uint32_t Mp4Factory::getHeapSizes(uint32_t *outParserHeap, uint32_t *outDecoderHeap,
                                  uint32_t *outIoHeap)
{
    *outParserHeap = MP4_PARSER_HEAP_SIZE;
    *outDecoderHeap = MP4_DECODER_HEAP_SIZE;
    *outIoHeap = MP4_IO_HEAP_SIZE;
    return MP4_IO_HEAP_SIZE;
}

/* 0x811F8C9A */
void Mp4Factory::onReservedInit()
{
}

/* 0x811F8CAA / 0x811F8CC0 */
void Mp4Factory::onReservedClose()
{
}

/* 0x811F8CC2 */
void *Mp4Factory::createPipeline(int playMode, int audioOptPrimary, int audioOptSecondary,
                                 int audioOptVolume, int audioOptBalance, int audioOptChannel)
{
    Mp4Parser *parser = NULL;
    void *parserMem = allocFromHeap(sizeof(Mp4Parser));
    if (parserMem != NULL) {
        parser = new (parserMem) Mp4Parser("SceShellMusicCoreMp4Parser", playMode);
    }

    DecoderFilterView *decoder = NULL;
    void *decoderMem = allocSecondaryHeap(0xD0u);
    if (decoderMem != NULL) {
        decoder = (DecoderFilterView *)mp4DecoderConstruct(decoderMem, "SceShellMusicCoreMp4Decoder");
    }

    int pipeSlots = 0;
    if (playMode == MODE_LOCAL_PLAY || playMode == MODE_DIRECT_PLAY) {
        pipeSlots = 6;
    } else if (playMode == MODE_STREAM_PLAY) {
        pipeSlots = 64;
    } else {
        return NULL;
    }

    void *decoderConsumer = (decoder != NULL) ? decoder->asPipeConsumer() : NULL;
    FilterLifecycleIf *pipe = (FilterLifecycleIf *)createBufRefPipe(pipeSlots, parser, decoderConsumer);
    FilterLifecycleIf *audioSink = (FilterLifecycleIf *)createAudioOutputFilter(
        0x2000, 16, decoder, audioOptPrimary, audioOptSecondary,
        audioOptVolume, audioOptBalance, audioOptChannel);

    if (parser != NULL) {
        parserFilterSetOutputPipe(parser, pipe);
    }
    if (decoder != NULL) {
        decoderFilterSetInputPipe(decoder, pipe);
        void *sinkInput = (audioSink != NULL) ? (void *)((uint8_t *)audioSink + 4) : NULL;
        decoderFilterSetOutputSink(decoder, sinkInput);
    }

    FilterLifecycleIf *pipeline = (FilterLifecycleIf *)createDecoderPipeline(
        parser, pipe, decoder, audioSink);
    if (pipeline == NULL || parser == NULL || decoder == NULL || audioSink == NULL || pipe == NULL) {
        if (pipeline != NULL) {
            delete pipeline;
        }
        if (parser != NULL) {
            delete parser;
        }
        if (decoder != NULL) {
            decoder->~DecoderFilterView();
            freeSecondaryHeap(decoder);
        }
        if (audioSink != NULL) {
            delete audioSink;
        }
        return NULL;
    }

    if (decoderPipelineIsReady(pipeline)
        && parser->isReady()
        && decoder->isReady() != 0
        && audioSink->isReady() != 0
        && pipe->isReady() != 0) {
        parser->asActiveFilter()->setEventListener(pipeline);
        decoder->asActiveFilter()->setEventListener(pipeline);
        audioSink->setEventListener(pipeline);
        return pipeline;
    }

    delete pipeline;
    delete parser;
    decoder->~DecoderFilterView();
    freeSecondaryHeap(decoder);
    delete audioSink;
    delete pipe;
    return NULL;
}

/* 0x811F8ED0 */
void *Mp4Factory::createMediaInfo(const char *path, int ioPriority)
{
    void *mem = allocFromHeap(2072u);
    if (mem == NULL) {
        return NULL;
    }
    return mp4MediaInfoConstruct(mem, path, ioPriority);
}

/* 0x8156F3F4 / 0x811F8C7C / 0x811F8EFA */
static Mp4Factory g_mp4Factory;

Mp4Factory *getMp4FactoryInstance()
{
    return &g_mp4Factory;
}

} /* namespace MusicCore */
} /* namespace sce */
