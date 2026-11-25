/*
 * SceShell musiccore_plugin - Mp3Factory and Mp3Parser (MP3 decoder pipeline
 * factory and frame-by-frame / trick-play parser filter).
 * Original address range (3.65-CEX): 0x811F7604 - 0x811F8C7B
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

static const uint32_t FOURCC_MP3 = 0x2033504Du; /* ' MP3' */

static const int PARSER_ERR_OK = 0;
static const int PARSER_ERR_NO_SOURCE = 4;
static const int PARSER_ERR_EOS = 5;
static const int PARSER_ERR_OPEN_FAIL = 17;
static const int PARSER_ERR_CLOSE_FAIL = 18;
static const int PARSER_ERR_READ_IO = 19;
static const int PARSER_ERR_OPEN_IO = 20;
static const int PARSER_ERR_BAD_STATE = 33;
static const int PARSER_ERR_SYNC_LOST = 69;
static const int PARSER_ERR_UNSUPPORTED = 81;
static const int PARSER_ERR_BAD_DURATION = 82;
static const int PARSER_ERR_CORRUPT = 95;
static const int PARSER_ERR_BAD_PARAM = 241;

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
extern void *mp3MediaInfoConstruct(void *mem, const char *path, int ioPriority);

/* Mp3StreamParser interface (implemented in musiccore_mp3_media.cpp) */
struct Mp3FramePacket {
    void *frameData;
    uint32_t frameSize;
    uint32_t sampleRate;
    uint32_t samplesPerFrame;
    uint8_t channelMode;
    uint16_t leadingSkip;
    uint16_t trailingSkip;
    uint8_t mpegVersion;
    int frameFlags;
};

class Mp3StreamParserView {
public:
    virtual ~Mp3StreamParserView() {}
    virtual int readFirstFrame(Mp3FramePacket *outPkt, int *outErr) = 0;
    virtual int readNextFrame(Mp3FramePacket *outPkt, int *outErr) = 0;
    virtual int readPrevFrame(Mp3FramePacket *outPkt, int *outErr) = 0;
    virtual int seekAndReadFrame(int targetMs, Mp3FramePacket *outPkt, int *outErr) = 0;
};

extern int mp3StreamOpen(Mp3StreamParserView *stream);
extern const char *mp3StreamGetPath(Mp3StreamParserView *stream);
extern void mp3StreamSetReadMode(Mp3StreamParserView *stream, int mode, int p1, int p2);
extern int mp3StreamGetDurationMs(Mp3StreamParserView *stream, int *outDurationMs, int *outErr);
extern int mp3StreamGetSampleRate(Mp3StreamParserView *stream, uint32_t *outSampleRate, int *outErr);
extern int mp3StreamGetSamplesPerFrame(Mp3StreamParserView *stream, uint16_t *outSamples, int *outErr);
extern int mp3StreamQueryTag(Mp3StreamParserView *stream, void *tagOut, void *tagOut2);

struct Mp3MediaInfoView : public MediaInfoHandle {
    uint8_t pathStorage[2052];
    Mp3StreamParserView *streamParser; /* +2064 */
    void *cacheIo;                     /* +2068 */
};

struct CacheIoPriorityIf {
    virtual ~CacheIoPriorityIf() {}
    virtual int isInitialized() const = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() const = 0;
    virtual int getFileSize() const = 0;
    virtual const char *getPath() const = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(int mode) = 0;
};

/* Pipeline / filter infrastructure (0x81206F38..0x8120825B) */
extern void parserFilterBaseInit(void *self, int filterKind, const char *threadName);
extern void parserFilterBaseDestroy(void *self);
extern int activeFilterIsReady(const void *activeBase);
extern void parserFilterSetOutputPipe(void *parser, void *pipe);
extern void decoderFilterSetInputPipe(void *decoder, void *pipe);
extern void decoderFilterSetOutputSink(void *decoder, void *audioSink);
extern void *mp3DecoderConstruct(void *mem, const char *threadName);
extern void *createBufRefPipe(int slotCount, void *producer, void *consumer);
extern void *createAudioOutputFilter(int grainSize, int slotCount, void *producer,
                                     int p1, int p2, int p3, int p4, int p5);
extern void *createDecoderPipeline(void *parser, void *pipe, void *decoder, void *audioSink);
extern bool decoderPipelineIsReady(const void *pipeline);
extern void *decoderPipelineGetParser(void *pipeline);
extern void *decoderPipelineGetDecoder(void *pipeline);
extern void *decoderPipelineGetAudio(void *pipeline);
extern void *decoderPipelineGetPipe(void *pipeline);

struct FilterDestroyable {
    virtual ~FilterDestroyable() {}
};

struct ActiveFilterAttach {
    virtual ~ActiveFilterAttach() {}
    virtual void setEventListener(void *listener) = 0; /* +0x08 */
};

struct FilterReadyCheck {
    virtual ~FilterReadyCheck() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
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

/* Cached last MP3 sample rate for fast trick-play skipping (0x81545008) */
static uint32_t g_lastMp3SampleRate = 0;

/* 0x811F7C60 */
int mapMp3ParserError(uint32_t errCode)
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
    case 0x80280016u:
    case 0x80280019u:
        return PARSER_ERR_UNSUPPORTED;
    case 0x80280017u:
    case 0x80280018u:
        return PARSER_ERR_CORRUPT;
    case 0x8028001Au:
        return PARSER_ERR_NO_SOURCE;
    case 0x8028001Bu:
        return PARSER_ERR_SYNC_LOST;
    default:
        return -1;
    }
}

/* ===== Mp3Parser (0x811F792C - 0x811F8C7B, size 124 bytes) ===== */

class Mp3Parser {
public:
    Mp3Parser(const char *threadName, int playMode);
    virtual ~Mp3Parser();

    bool isReady() const;
    int queryMetadata(void *metaOut);
    int findCachedMediaInfo(const char *path, Mp3MediaInfoView **outInfo);
    int setMediaInfo(Mp3MediaInfoView *mediaInfo);
    int prefetchMediaInfo(Mp3MediaInfoView *mediaInfo);
    void setPlaySpeed(int speed);
    int setStartPosition(int startMs);
    int setEndPosition(int endMs);
    int getDurationMs();
    int seekPosition(int positionMs);
    int resetPosition();
    int onStart();
    int onStop();
    int readFrame(ParserPacketOut *outPkt);

    uint8_t m_basePrefix[16];          /* +4..+19 */
    SyncMutexStorage m_mutex;          /* +20..+27 */
    uint32_t m_baseReserved;           /* +28 */
    int m_filterState;                 /* +32 */
    uint8_t m_baseTail[12];            /* +36..+47 */
    int m_playMode;                    /* +48 */
    int m_requestedSpeed;              /* +52 */
    int m_startPosMs;                  /* +56 */
    int m_endPosMs;                    /* +60 */
    int m_durationMs;                  /* +64 */
    int m_activeSpeed;                 /* +68 */
    float m_currentPosMs;              /* +72 */
    float m_frameDurationMs;           /* +76 */
    uint8_t m_seekPending;             /* +80 */
    uint8_t m_padSeek[3];              /* +81..+83 */
    uint32_t m_unusedStreamSlot;       /* +84 */
    CacheIoPriorityIf *m_activeCacheIo;/* +88 */
    CacheIoPriorityIf *m_nextCacheIo;  /* +92 */
    Mp3StreamParserView *m_activeStream;/* +96 */
    Mp3StreamParserView *m_nextStream;  /* +100 */
    Mp3MediaInfoView *m_activeMedia;   /* +104 */
    Mp3MediaInfoView *m_nextMedia;     /* +108 */
    uint8_t m_lastDiscontinuity;       /* +112 */
    uint8_t m_padDiscont[3];           /* +113..+115 */
    int m_trickFrameCounter;           /* +116 */
    uint8_t m_parserInitialized;       /* +120 */
    uint8_t m_cachedChannels;          /* +121 */
    uint8_t m_padTail[2];              /* +122..+123 */
};

/* 0x811F792C */
Mp3Parser::Mp3Parser(const char *threadName, int playMode)
    : m_playMode(playMode)
    , m_requestedSpeed(0)
    , m_startPosMs(-1)
    , m_endPosMs(-1)
    , m_durationMs(-1)
    , m_activeSpeed(0)
    , m_currentPosMs(-1.0f)
    , m_frameDurationMs(15.0f)
    , m_seekPending(0)
    , m_unusedStreamSlot(0)
    , m_activeCacheIo(NULL)
    , m_nextCacheIo(NULL)
    , m_activeStream(NULL)
    , m_nextStream(NULL)
    , m_activeMedia(NULL)
    , m_nextMedia(NULL)
    , m_lastDiscontinuity(1)
    , m_trickFrameCounter(0)
    , m_parserInitialized(1)
    , m_cachedChannels(0)
{
    parserFilterBaseInit(this, 1, threadName);
}

/* 0x811F7992 / 0x811F7A0A */
Mp3Parser::~Mp3Parser()
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
    parserFilterBaseDestroy(this);
}

/* 0x811F7A1A */
bool Mp3Parser::isReady() const
{
    if (m_parserInitialized == 0u) {
        return false;
    }
    return activeFilterIsReady((const uint8_t *)this + 4) != 0;
}

/* 0x811F7A34 */
int Mp3Parser::queryMetadata(void *metaOut)
{
    if (metaOut == NULL) {
        return 0;
    }
    mp3StreamQueryTag(m_activeStream, metaOut, metaOut);
    return -1;
}

/* 0x811F7A88 */
int Mp3Parser::findCachedMediaInfo(const char *path, Mp3MediaInfoView **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    syncMutexLock(&m_mutex);
    if (m_activeStream != NULL && sce_paf_strcmp(mp3StreamGetPath(m_activeStream), path) == 0) {
        syncMutexUnlock(&m_mutex);
        *outInfo = m_activeMedia;
        return 0;
    }
    if (m_nextStream != NULL && sce_paf_strcmp(mp3StreamGetPath(m_nextStream), path) == 0) {
        *outInfo = m_nextMedia;
        syncMutexUnlock(&m_mutex);
        return 0;
    }
    syncMutexUnlock(&m_mutex);
    *outInfo = NULL;
    return 0;
}

/* 0x811F7AF4 */
int Mp3Parser::setMediaInfo(Mp3MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_MP3) {
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

    const char *newPath = mp3StreamGetPath(mediaInfo->streamParser);
    if (m_activeStream != NULL) {
        if (sce_paf_strcmp(mp3StreamGetPath(m_activeStream), newPath) == 0) {
            if (m_activeStream == m_nextStream) {
                m_nextStream = NULL;
                m_nextCacheIo = NULL;
                m_nextMedia = NULL;
            }
            syncMutexUnlock(&m_mutex);
            delayWorkerThread(20000u);
            return 0;
        }
        if (m_activeMedia != NULL) {
            MediaInfoHandle *oldHandle = m_activeMedia;
            decoderManagerReleaseMediaInfo(&oldHandle, 1);
            m_activeMedia = NULL;
        }
        if (m_nextStream == m_activeStream) {
            m_nextStream = NULL;
            m_nextCacheIo = NULL;
            m_nextMedia = NULL;
        }
        m_activeStream = NULL;
        m_activeCacheIo = NULL;
    }

    m_currentPosMs = -1.0f;
    if (m_nextStream != NULL && sce_paf_strcmp(mp3StreamGetPath(m_nextStream), newPath) == 0) {
        m_activeStream = m_nextStream;
        m_activeMedia = m_nextMedia;
        m_activeCacheIo = m_nextCacheIo;
        if (m_activeCacheIo != NULL) {
            m_activeCacheIo->setPriorityMode(0);
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
        m_nextMedia = NULL;
        m_durationMs = -1;
        syncMutexUnlock(&m_mutex);
        return 0;
    }

    if (m_nextStream != NULL) {
        if (m_nextMedia != NULL) {
            MediaInfoHandle *nextHandle = m_nextMedia;
            decoderManagerReleaseMediaInfo(&nextHandle, 1);
            m_nextMedia = NULL;
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
    }

    if (m_activeMedia != NULL || m_nextMedia != NULL) {
        return -1;
    }

    mediaInfo->sharedFlag = 1u;
    m_activeMedia = mediaInfo;
    m_activeStream = mediaInfo->streamParser;
    m_activeCacheIo = (CacheIoPriorityIf *)mediaInfo->cacheIo;
    m_durationMs = -1;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811F7D18 */
int Mp3Parser::prefetchMediaInfo(Mp3MediaInfoView *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_MP3) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->streamParser == NULL || mediaInfo->cacheIo == NULL) {
        return -1;
    }

    syncMutexLock(&m_mutex);
    if (m_playMode == MODE_STREAM_PLAY) {
        syncMutexUnlock(&m_mutex);
        return 0;
    }

    const char *newPath = mp3StreamGetPath(mediaInfo->streamParser);
    if (m_nextStream != NULL) {
        if (sce_paf_strcmp(mp3StreamGetPath(m_nextStream), newPath) == 0) {
            syncMutexUnlock(&m_mutex);
            return 0;
        }
        if (sce_paf_strcmp(mp3StreamGetPath(m_nextStream), mp3StreamGetPath(m_activeStream)) == 0) {
            m_nextStream = m_activeStream;
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            syncMutexUnlock(&m_mutex);
            return 0;
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
        return -1;
    }

    mediaInfo->sharedFlag = 1u;
    m_nextMedia = mediaInfo;
    m_nextStream = mediaInfo->streamParser;
    m_nextCacheIo = (CacheIoPriorityIf *)mediaInfo->cacheIo;
    m_nextCacheIo->setPriorityMode(1);

    if (mp3StreamOpen(m_nextStream) < 0) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_OPEN_FAIL;
    }

    int durationOut = 0;
    int parserErr = -1;
    if (mp3StreamGetDurationMs(m_nextStream, &durationOut, &parserErr) != 0) {
        syncMutexUnlock(&m_mutex);
        return 0;
    }

    int mapped = mapMp3ParserError((uint32_t)parserErr);
    syncMutexUnlock(&m_mutex);
    return mapped;
}

/* 0x811F7EB6 */
void Mp3Parser::setPlaySpeed(int speed)
{
    m_requestedSpeed = speed;
    m_seekPending = 1u;
}

/* 0x811F7EC0 */
int Mp3Parser::setStartPosition(int startMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_startPosMs = startMs;
    m_currentPosMs = -1.0f;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811F7EF4 */
int Mp3Parser::setEndPosition(int endMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_endPosMs = endMs;
    m_currentPosMs = -1.0f;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811F7F28 */
int Mp3Parser::getDurationMs()
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

    int openRet = mp3StreamOpen(m_activeStream);
    if (openRet < 0) {
        syncMutexUnlock(&m_mutex);
        return openRet;
    }

    int parserErr = -1;
    if (mp3StreamGetDurationMs(m_activeStream, &m_durationMs, &parserErr) != 0) {
        int dur = m_durationMs;
        syncMutexUnlock(&m_mutex);
        return dur;
    }

    syncMutexUnlock(&m_mutex);
    if ((uint32_t)parserErr == 0x80280016u || (uint32_t)parserErr == 0x80280018u) {
        return 0;
    }
    return parserErr;
}

/* 0x811F7FDA */
int Mp3Parser::seekPosition(int positionMs)
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    if (positionMs >= 0) {
        if (m_currentPosMs != (float)positionMs) {
            m_currentPosMs = (float)positionMs;
        }
        m_seekPending = 1u;
    } else if (m_currentPosMs >= 0.0f) {
        m_seekPending = 1u;
        m_currentPosMs = -1.0f;
    }
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811F8052 */
int Mp3Parser::resetPosition()
{
    syncMutexLock(&m_mutex);
    if (m_filterState != 2) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_STATE;
    }
    m_currentPosMs = -1.0f;
    syncMutexUnlock(&m_mutex);
    return 0;
}

/* 0x811F8082 */
int Mp3Parser::onStart()
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (m_activeCacheIo != NULL) {
        m_activeCacheIo->setPriorityMode(0);
    }
    int openRet = mp3StreamOpen(m_activeStream);
    if (openRet < 0) {
        return mapMp3ParserError((uint32_t)openRet);
    }
    m_activeSpeed = 0;
    return 0;
}

/* 0x811F80B2 */
int Mp3Parser::onStop()
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (m_activeCacheIo != NULL) {
        m_activeCacheIo->setPriorityMode(1);
    }
    return 0;
}

/* 0x811F80CC */
int Mp3Parser::readFrame(ParserPacketOut *outPkt)
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (outPkt == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    bool fastSkipMode = (m_playMode == MODE_STREAM_PLAY
                         && (m_requestedSpeed > 20 || m_requestedSpeed < -20));

    syncMutexLock(&m_mutex);
    if (m_requestedSpeed == 0
        || (m_playMode != MODE_STREAM_PLAY && m_playMode != MODE_LOCAL_PLAY && m_playMode != MODE_DIRECT_PLAY)) {
        syncMutexUnlock(&m_mutex);
        return PARSER_ERR_BAD_PARAM;
    }

    int mode = m_playMode;
    if (m_requestedSpeed != m_activeSpeed) {
        if (m_requestedSpeed == 1) {
            mp3StreamSetReadMode(m_activeStream, 1, 0, 0);
        } else if (m_requestedSpeed < 0) {
            mp3StreamSetReadMode(m_activeStream, 4, 0, 0);
        } else {
            mp3StreamSetReadMode(m_activeStream, 2, 0, 0);
        }
        m_activeSpeed = m_requestedSpeed;
    }
    syncMutexUnlock(&m_mutex);

    Mp3FramePacket framePkt;
    framePkt.frameSize = outPkt->maxBufferSize;
    int parserErr = -1;
    int gotFrame = 0;

    if (m_currentPosMs >= 0.0f) {
        int speed = m_activeSpeed;
        if (speed <= 0) {
            if (m_startPosMs >= (int)m_currentPosMs) {
                outPkt->payloadSize = 0;
                outPkt->packetFlags = 2;
                return PARSER_ERR_EOS;
            }
        } else if (m_endPosMs >= 0 && m_endPosMs <= (int)m_currentPosMs) {
            outPkt->payloadSize = 0;
            outPkt->packetFlags = 2;
            return PARSER_ERR_EOS;
        }

        if (fastSkipMode) {
            int nextPosMs = (int)(m_currentPosMs + m_frameDurationMs * (float)speed);
            if (nextPosMs > 0 && nextPosMs < m_durationMs) {
                if (g_lastMp3SampleRate == 0u) {
                    g_lastMp3SampleRate = 44100u;
                }
                if (m_cachedChannels == 0u) {
                    m_cachedChannels = 2u;
                }
                outPkt->frameFlags = 0;
                outPkt->timestampMs = m_currentPosMs;
                outPkt->payloadSize = 0;
                outPkt->sampleRate = g_lastMp3SampleRate;
                outPkt->channelCount = m_cachedChannels;
                outPkt->frameDurationMs = m_frameDurationMs;
                m_currentPosMs = (float)nextPosMs;
                outPkt->packetFlags = 4;
                return 0;
            }
        }

        if (m_seekPending != 0u) {
            float seekTarget = (speed > 0) ? m_currentPosMs : 0.0f;
            if (speed < 0) {
                if (m_endPosMs >= 0) {
                    if (m_endPosMs < (int)m_currentPosMs) {
                        m_currentPosMs = (float)m_endPosMs;
                    }
                } else if ((uint32_t)m_durationMs < (uint32_t)m_currentPosMs) {
                    m_currentPosMs = (float)(uint32_t)m_durationMs - m_frameDurationMs;
                }
                if (mode == MODE_LOCAL_PLAY || mode == MODE_DIRECT_PLAY) {
                    seekTarget = m_currentPosMs - m_frameDurationMs * 2.0f;
                } else if (mode == MODE_STREAM_PLAY) {
                    seekTarget = m_currentPosMs - m_frameDurationMs * 20.0f;
                }
                if (seekTarget < 0.0f) {
                    seekTarget = 0.0f;
                }
            }
            gotFrame = m_activeStream->seekAndReadFrame((int)seekTarget, &framePkt, &parserErr);
            if (gotFrame != 0) {
                m_lastDiscontinuity = 1u;
                outPkt->discontinuity = 1u;
                m_trickFrameCounter = 1;
                m_frameDurationMs = ((float)framePkt.samplesPerFrame * 1000.0f) / (float)framePkt.sampleRate;
            }
        } else if (speed == 1) {
            gotFrame = m_activeStream->readNextFrame(&framePkt, &parserErr);
            if (gotFrame != 0) {
                m_currentPosMs += (float)(int)m_frameDurationMs;
                outPkt->discontinuity = 0u;
                m_lastDiscontinuity = 0u;
            }
        } else {
            bool needSeekStep = ((mode == MODE_LOCAL_PLAY || mode == MODE_DIRECT_PLAY) && m_trickFrameCounter > 2)
                             || (mode == MODE_STREAM_PLAY && m_trickFrameCounter > 20);
            if (needSeekStep) {
                int targetMs = (int)(m_currentPosMs + m_frameDurationMs * (float)speed);
                if (speed > 0 && m_endPosMs >= 0 && m_endPosMs <= targetMs) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                if (speed < 0 && m_startPosMs >= targetMs) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                gotFrame = m_activeStream->seekAndReadFrame(targetMs, &framePkt, &parserErr);
                if (gotFrame != 0) {
                    m_currentPosMs = (targetMs < 0) ? 0.0f : (float)targetMs;
                    m_lastDiscontinuity = 1u;
                    outPkt->discontinuity = 1u;
                    m_trickFrameCounter = 1;
                }
            } else {
                int targetMs = (int)(m_currentPosMs + m_frameDurationMs * (float)m_requestedSpeed);
                if (speed > 0 && m_endPosMs >= 0 && m_endPosMs <= targetMs) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                if (speed < 0 && m_startPosMs >= targetMs) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                gotFrame = m_activeStream->readNextFrame(&framePkt, &parserErr);
                if (gotFrame != 0) {
                    m_currentPosMs = (targetMs < 0) ? 0.0f : (float)targetMs;
                    outPkt->discontinuity = 0u;
                    m_lastDiscontinuity = 0u;
                    ++m_trickFrameCounter;
                }
            }
        }
    } else {
        if (mp3StreamGetDurationMs(m_activeStream, &m_durationMs, &parserErr) == 0) {
            return mapMp3ParserError((uint32_t)parserErr);
        }
        if (m_durationMs < 0) {
            return PARSER_ERR_BAD_DURATION;
        }

        if (m_activeSpeed > 0) {
            if (m_startPosMs >= 0) {
                gotFrame = m_activeStream->seekAndReadFrame(m_startPosMs, &framePkt, &parserErr);
                if (gotFrame != 0) {
                    m_lastDiscontinuity = 1u;
                    outPkt->discontinuity = 1u;
                    m_currentPosMs = (float)m_startPosMs;
                    m_trickFrameCounter = 1;
                    m_frameDurationMs = ((float)framePkt.samplesPerFrame * 1000.0f) / (float)framePkt.sampleRate;
                }
            } else {
                gotFrame = m_activeStream->readFirstFrame(&framePkt, &parserErr);
                if (gotFrame != 0) {
                    outPkt->discontinuity = 0u;
                    m_lastDiscontinuity = 0u;
                    m_currentPosMs = 0.0f;
                    m_trickFrameCounter = 1;
                    m_frameDurationMs = ((float)framePkt.samplesPerFrame * 1000.0f) / (float)framePkt.sampleRate;
                }
            }
        } else {
            uint32_t sampleRate = 0;
            uint16_t samplesPerFrame = 0;
            if (mp3StreamGetSampleRate(m_activeStream, &sampleRate, &parserErr) == 0) {
                return mapMp3ParserError((uint32_t)parserErr);
            }
            if (mp3StreamGetSamplesPerFrame(m_activeStream, &samplesPerFrame, &parserErr) == 0) {
                return mapMp3ParserError((uint32_t)parserErr);
            }
            float estFrameMs = ((float)samplesPerFrame * 1000.0f) / (float)sampleRate;
            float backSpan = (mode == MODE_STREAM_PLAY) ? (estFrameMs * 20.0f) : (estFrameMs * 2.0f);

            if (m_endPosMs < 0) {
                float seekPos = (float)(uint32_t)m_durationMs - backSpan;
                if (seekPos < 0.0f) {
                    seekPos = 0.0f;
                }
                gotFrame = m_activeStream->seekAndReadFrame((int)(uint32_t)seekPos, &framePkt, &parserErr);
                if (gotFrame != 0) {
                    m_lastDiscontinuity = 1u;
                    outPkt->discontinuity = 1u;
                    m_frameDurationMs = ((float)framePkt.samplesPerFrame * 1000.0f) / (float)framePkt.sampleRate;
                    m_currentPosMs = (float)(uint32_t)m_durationMs - m_frameDurationMs;
                    m_trickFrameCounter = (mode == MODE_STREAM_PLAY) ? 20 : 2;
                }
            } else {
                if ((float)m_endPosMs > (float)(uint32_t)m_durationMs * 1000.0f) {
                    outPkt->payloadSize = 0;
                    outPkt->packetFlags = 2;
                    return PARSER_ERR_EOS;
                }
                float seekPos = (float)m_endPosMs - backSpan;
                if (seekPos < 0.0f) {
                    seekPos = 0.0f;
                }
                gotFrame = m_activeStream->seekAndReadFrame((int)(uint32_t)seekPos, &framePkt, &parserErr);
                if (gotFrame != 0) {
                    m_lastDiscontinuity = 1u;
                    outPkt->discontinuity = 1u;
                    m_currentPosMs = (float)m_endPosMs;
                    m_frameDurationMs = ((float)framePkt.samplesPerFrame * 1000.0f) / (float)framePkt.sampleRate;
                    if (mode == MODE_STREAM_PLAY) {
                        m_trickFrameCounter = 20;
                    } else if (mode == MODE_LOCAL_PLAY) {
                        m_trickFrameCounter = 2;
                    }
                }
            }
        }
    }

    if (gotFrame == 0) {
        if ((uint32_t)parserErr == 0x8010A007u) {
            outPkt->payloadSize = 0;
            outPkt->packetFlags = 2;
            return PARSER_ERR_EOS;
        }
        return mapMp3ParserError((uint32_t)parserErr);
    }

    uint32_t activeSamples = framePkt.samplesPerFrame - framePkt.leadingSkip - framePkt.trailingSkip;
    g_lastMp3SampleRate = framePkt.sampleRate;
    m_seekPending = 0u;
    outPkt->frameData = framePkt.frameData;
    outPkt->payloadSize = framePkt.frameSize;
    outPkt->timestampMs = m_currentPosMs;
    outPkt->frameFlags = framePkt.frameFlags;
    outPkt->frameDurationMs = ((float)activeSamples * 1000.0f) / (float)framePkt.sampleRate;
    outPkt->sampleRate = framePkt.sampleRate;

    if (framePkt.channelMode <= 1u) {
        outPkt->channelConfig = (framePkt.channelMode > 0u) ? 1 : 0;
    } else if (framePkt.channelMode <= 3u) {
        outPkt->channelConfig = 2;
    } else {
        return -1;
    }

    outPkt->leadingSkip = framePkt.leadingSkip;
    outPkt->trailingSkip = framePkt.trailingSkip;

    if (framePkt.mpegVersion <= 2u) {
        outPkt->channelCount = 2u;
        m_cachedChannels = 2u;
    } else if (framePkt.mpegVersion == 3u) {
        outPkt->channelCount = 1u;
        m_cachedChannels = 1u;
    } else {
        return -1;
    }

    outPkt->packetFlags = (m_activeSpeed != 1) ? 1 : 0;
    return 0;
}

/* ===== Mp3Factory (0x811F7604 - 0x811F792B) ===== */

class Mp3Factory {
public:
    Mp3Factory() {}
    virtual ~Mp3Factory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf);
    void *createPipeline(int mode, int p1, int p2, int p3, int p4, int p5);
    int destroyPipeline(void *pipeline);
    void *createMediaInfo(const char *path, int ioPriority);
    int destroyMediaInfo(void **ioMediaInfo);
};

static Mp3Factory g_mp3Factory; /* 0x8156F3F0 */

/* 0x811F7604 */
Mp3Factory *getMp3Factory()
{
    return &g_mp3Factory;
}

/* 0x811F760E */
int Mp3Factory::getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf)
{
    *outParserBuf = 54272u;
    *outDecoderBuf = 78848u;
    *outAudioBuf = 0x4000u;
    return 0x4000;
}

/* 0x811F764A */
void *Mp3Factory::createPipeline(int mode, int p1, int p2, int p3, int p4, int p5)
{
    Mp3Parser *parser = NULL;
    void *parserMem = allocFromHeap(sizeof(Mp3Parser));
    if (parserMem != NULL) {
        parser = new (parserMem) Mp3Parser("SceShellMusicCoreMp3Parser", mode);
    }

    void *decoder = NULL;
    void *decMem = allocSecondaryHeap(168u);
    if (decMem != NULL) {
        decoder = mp3DecoderConstruct(decMem, "SceShellMusicCoreMp3Decoder");
    }

    if (mode != MODE_LOCAL_PLAY && mode != MODE_STREAM_PLAY && mode != MODE_DIRECT_PLAY) {
        return NULL;
    }

    int pipeSlots = (mode == MODE_STREAM_PLAY) ? 16 : 6;
    void *decoderConsumer = (decoder != NULL) ? ((uint8_t *)decoder + 4) : NULL;
    void *pipe = createBufRefPipe(pipeSlots, parser, decoderConsumer);
    void *audio = createAudioOutputFilter(4608, 16, decoder, p1, p2, p3, p4, p5);

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
        if (pipe != NULL) {
            delete (FilterDestroyable *)pipe;
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
        && parser->isReady()
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

/* 0x811F7860 */
int Mp3Factory::destroyPipeline(void *pipeline)
{
    if (pipeline == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    void *parser = decoderPipelineGetParser(pipeline);
    if (parser != NULL) {
        delete (FilterDestroyable *)parser;
    }
    void *decoder = decoderPipelineGetDecoder(pipeline);
    if (decoder != NULL) {
        ((FilterDestroyable *)decoder)->~FilterDestroyable();
        freeSecondaryHeap(decoder);
    }
    void *audio = decoderPipelineGetAudio(pipeline);
    if (audio != NULL) {
        delete (FilterDestroyable *)audio;
    }
    void *pipe = decoderPipelineGetPipe(pipeline);
    if (pipe != NULL) {
        delete (FilterDestroyable *)pipe;
    }
    delete (FilterDestroyable *)pipeline;
    return 0;
}

/* 0x811F78C2 */
void *Mp3Factory::createMediaInfo(const char *path, int ioPriority)
{
    void *mem = allocFromHeap(2072u);
    if (mem == NULL) {
        return NULL;
    }
    return mp3MediaInfoConstruct(mem, path, ioPriority);
}

/* 0x811F78EC */
int Mp3Factory::destroyMediaInfo(void **ioMediaInfo)
{
    if (*ioMediaInfo == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }
    delete (FilterDestroyable *)(*ioMediaInfo);
    *ioMediaInfo = NULL;
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
