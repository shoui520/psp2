/*
 * SceShell musiccore_plugin - Mp3Factory and Mp3Parser (MP3 decoder pipeline
 * factory and frame-by-frame / trick-play parser filter).
 * Original address range (3.65-CEX): 0x811F7604 - 0x811F8C7B
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_codec_decoders.h"
#include "musiccore_codec_factories.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_decoder_pipeline.h"
#include "musiccore_media.h"
#include "musiccore_pipeline_types.h"

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

class Mp3Parser : public AudioParserBase {
public:
    Mp3Parser(const char *threadName, int playMode);
    virtual ~Mp3Parser();

    virtual int onFrameConsumed(int frameFlags);
    virtual int openStream();
    virtual int closeStream();
    virtual int produceFrame(MediaFrameBuffer *outBuf);

    virtual int findCachedMediaInfo(const char *path, MediaInfoHandle **outInfo);
    virtual int setMediaInfo(MediaInfoHandle *mediaInfo);
    virtual int prefetchMediaInfo(MediaInfoHandle *mediaInfo);
    virtual bool hasNextMedia() const { return m_nextMedia != NULL; }
    virtual void *getCodecConfig(uint32_t *outSize) { *outSize = 0; return NULL; }
    virtual void setPlaySpeed(int speed);
    virtual int setStartPosition(int startMs);
    virtual int setEndPosition(int endMs);
    virtual int getDurationMs();
    virtual int seekPosition(int positionMs);
    virtual int resetPosition();
    virtual int getPlaySpeed() const { return m_requestedSpeed; }
    virtual int getStartPosition() const { return m_startPosMs; }
    virtual int getEndPosition() const { return m_endPosMs; }
    virtual bool isInitialized() const;

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
    CachedStreamIo *m_activeCacheIo;   /* +88 */
    CachedStreamIo *m_nextCacheIo;     /* +92 */
    Mp3StreamParser *m_activeStream;   /* +96 */
    Mp3StreamParser *m_nextStream;     /* +100 */
    Mp3MediaInfo *m_activeMedia;       /* +104 */
    Mp3MediaInfo *m_nextMedia;         /* +108 */
    uint8_t m_lastDiscontinuity;       /* +112 */
    uint8_t m_padDiscont[3];           /* +113..+115 */
    int m_trickFrameCounter;           /* +116 */
    uint8_t m_parserInitialized;       /* +120 */
    uint8_t m_cachedChannels;          /* +121 */
    uint8_t m_padTail[2];              /* +122..+123 */
};

/* 0x811F792C */
Mp3Parser::Mp3Parser(const char *threadName, int playMode)
    : AudioParserBase(1, threadName)
    , m_playMode(playMode)
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
        MediaInfoHandle *nextHandle = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
        { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(nextHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); nextHandle = NULL; }
        m_nextMedia = NULL;
        m_nextCacheIo = NULL;
        m_nextStream = NULL;
    }
    if (m_activeMedia != NULL) {
        MediaInfoHandle *curHandle = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
        { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(curHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); curHandle = NULL; }
        m_activeMedia = NULL;
        m_activeCacheIo = NULL;
        m_activeStream = NULL;
    }
}

/* 0x811F7A1A */
bool Mp3Parser::isInitialized() const
{
    if (m_parserInitialized == 0u) {
        return false;
    }
    return ActiveFilterBase::isInitialized();
}

/* 0x811F7A34 */
int Mp3Parser::onFrameConsumed(int frameFlags)
{
    void *metaOut = reinterpret_cast<void *>(frameFlags);
    if (metaOut == NULL) {
        return 0;
    }
    return -1;
}

/* 0x811F7A88 */
int Mp3Parser::findCachedMediaInfo(const char *path, MediaInfoHandle **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    m_lock.lock();
    if (m_activeStream != NULL
        && sce_paf_strcmp(reinterpret_cast<const char *>(m_activeStream->getStreamPosition()), path) == 0) {
        m_lock.unlock();
        *outInfo = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
        return 0;
    }
    if (m_nextStream != NULL
        && sce_paf_strcmp(reinterpret_cast<const char *>(m_nextStream->getStreamPosition()), path) == 0) {
        *outInfo = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
        m_lock.unlock();
        return 0;
    }
    m_lock.unlock();
    *outInfo = NULL;
    return 0;
}

/* 0x811F7AF4 */
int Mp3Parser::setMediaInfo(MediaInfoHandle *handle)
{
    if (handle == NULL || handle->mediaFourcc != FOURCC_MP3) {
        return PARSER_ERR_BAD_PARAM;
    }
    Mp3MediaInfo *mediaInfo = reinterpret_cast<Mp3MediaInfo *>(handle);
    if (mediaInfo->parser == NULL || mediaInfo->ioStream == NULL) {
        return -1;
    }

    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }

    const char *newPath = reinterpret_cast<const char *>(mediaInfo->parser->getStreamPosition());
    if (m_activeStream != NULL) {
        if (sce_paf_strcmp(reinterpret_cast<const char *>(m_activeStream->getStreamPosition()), newPath) == 0) {
            if (m_activeStream == m_nextStream) {
                m_nextStream = NULL;
                m_nextCacheIo = NULL;
                m_nextMedia = NULL;
            }
            m_lock.unlock();
            delayWorkerThread(20000u);
            return 0;
        }
        if (m_activeMedia != NULL) {
            MediaInfoHandle *oldHandle = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
            { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(oldHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); oldHandle = NULL; }
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
    if (m_nextStream != NULL
        && sce_paf_strcmp(reinterpret_cast<const char *>(m_nextStream->getStreamPosition()), newPath) == 0) {
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
        m_lock.unlock();
        return 0;
    }

    if (m_nextStream != NULL) {
        if (m_nextMedia != NULL) {
            MediaInfoHandle *nextHandle = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
            { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(nextHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); nextHandle = NULL; }
            m_nextMedia = NULL;
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
    }

    if (m_activeMedia != NULL || m_nextMedia != NULL) {
        return -1;
    }

    mediaInfo->m_sharedFlag = 1u;
    m_activeMedia = mediaInfo;
    m_activeStream = mediaInfo->parser;
    m_activeCacheIo = mediaInfo->ioStream;
    m_durationMs = -1;
    m_lock.unlock();
    return 0;
}

/* 0x811F7D18 */
int Mp3Parser::prefetchMediaInfo(MediaInfoHandle *handle)
{
    if (handle == NULL || handle->mediaFourcc != FOURCC_MP3) {
        return PARSER_ERR_BAD_PARAM;
    }
    Mp3MediaInfo *mediaInfo = reinterpret_cast<Mp3MediaInfo *>(handle);
    if (mediaInfo->parser == NULL || mediaInfo->ioStream == NULL) {
        return -1;
    }

    m_lock.lock();
    if (m_playMode == MODE_STREAM_PLAY) {
        m_lock.unlock();
        return 0;
    }

    const char *newPath = reinterpret_cast<const char *>(mediaInfo->parser->getStreamPosition());
    if (m_nextStream != NULL) {
        if (sce_paf_strcmp(reinterpret_cast<const char *>(m_nextStream->getStreamPosition()), newPath) == 0) {
            m_lock.unlock();
            return 0;
        }
        if (sce_paf_strcmp(reinterpret_cast<const char *>(m_nextStream->getStreamPosition()),
                           reinterpret_cast<const char *>(m_activeStream->getStreamPosition())) == 0) {
            m_nextStream = m_activeStream;
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            m_lock.unlock();
            return 0;
        }
        if (m_activeStream == m_nextStream) {
            m_nextMedia = NULL;
        } else if (m_nextMedia != NULL) {
            MediaInfoHandle *oldNext = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
            { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(oldNext); DecoderManager::releaseMediaInfo(&relMedia, 1u); oldNext = NULL; }
            m_nextMedia = NULL;
        }
        m_nextStream = NULL;
        m_nextCacheIo = NULL;
    }

    if (m_nextMedia != NULL) {
        return -1;
    }

    mediaInfo->m_sharedFlag = 1u;
    m_nextMedia = mediaInfo;
    m_nextStream = mediaInfo->parser;
    m_nextCacheIo = mediaInfo->ioStream;
    m_nextCacheIo->setPriorityMode(1);

    if (m_nextStream->openStream() < 0) {
        m_lock.unlock();
        return PARSER_ERR_OPEN_FAIL;
    }

    unsigned int durationOut = 0;
    int parserErr = -1;
    if (m_nextStream->getDurationMs(&durationOut, &parserErr) != 0) {
        m_lock.unlock();
        return 0;
    }

    int mapped = mapMp3ParserError((uint32_t)parserErr);
    m_lock.unlock();
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
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_startPosMs = startMs;
    m_currentPosMs = -1.0f;
    m_lock.unlock();
    return 0;
}

/* 0x811F7EF4 */
int Mp3Parser::setEndPosition(int endMs)
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_endPosMs = endMs;
    m_currentPosMs = -1.0f;
    m_lock.unlock();
    return 0;
}

/* 0x811F7F28 */
int Mp3Parser::getDurationMs()
{
    m_lock.lock();
    if (m_durationMs >= 0) {
        int cached = m_durationMs;
        m_lock.unlock();
        return cached;
    }
    if (m_activeStream == NULL) {
        m_lock.unlock();
        return -1;
    }

    int openRet = m_activeStream->openStream();
    if (openRet < 0) {
        m_lock.unlock();
        return openRet;
    }

    int parserErr = -1;
    if (m_activeStream->getDurationMs(reinterpret_cast<unsigned int *>(&m_durationMs), &parserErr) != 0) {
        int dur = m_durationMs;
        m_lock.unlock();
        return dur;
    }

    m_lock.unlock();
    if ((uint32_t)parserErr == 0x80280016u || (uint32_t)parserErr == 0x80280018u) {
        return 0;
    }
    return parserErr;
}

/* 0x811F7FDA */
int Mp3Parser::seekPosition(int positionMs)
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
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
    m_lock.unlock();
    return 0;
}

/* 0x811F8052 */
int Mp3Parser::resetPosition()
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_currentPosMs = -1.0f;
    m_lock.unlock();
    return 0;
}

/* 0x811F8082 */
int Mp3Parser::openStream()
{
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (m_activeCacheIo != NULL) {
        m_activeCacheIo->setPriorityMode(0);
    }
    int openRet = m_activeStream->openStream();
    if (openRet < 0) {
        return mapMp3ParserError((uint32_t)openRet);
    }
    m_activeSpeed = 0;
    return 0;
}

/* 0x811F80B2 */
int Mp3Parser::closeStream()
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
int Mp3Parser::produceFrame(MediaFrameBuffer *outBuf)
{
    ParserPacketOut *outPkt = reinterpret_cast<ParserPacketOut *>(outBuf);
    if (m_activeStream == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (outPkt == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    bool fastSkipMode = (m_playMode == MODE_STREAM_PLAY
                         && (m_requestedSpeed > 20 || m_requestedSpeed < -20));

    m_lock.lock();
    if (m_requestedSpeed == 0
        || (m_playMode != MODE_STREAM_PLAY && m_playMode != MODE_LOCAL_PLAY && m_playMode != MODE_DIRECT_PLAY)) {
        m_lock.unlock();
        return PARSER_ERR_BAD_PARAM;
    }

    int mode = m_playMode;
    if (m_requestedSpeed != m_activeSpeed) {
        if (m_requestedSpeed == 1) {
            m_activeStream->setIoHint(1);
        } else if (m_requestedSpeed < 0) {
            m_activeStream->setIoHint(4);
        } else {
            m_activeStream->setIoHint(2);
        }
        m_activeSpeed = m_requestedSpeed;
    }
    m_lock.unlock();

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
            gotFrame = m_activeStream->seekToTimeMs(static_cast<unsigned int>((int)seekTarget),
                                                    &framePkt, &parserErr);
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
                gotFrame = m_activeStream->seekToTimeMs(static_cast<unsigned int>(targetMs),
                                                        &framePkt, &parserErr);
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
        if (m_activeStream->getDurationMs(reinterpret_cast<unsigned int *>(&m_durationMs), &parserErr) == 0) {
            return mapMp3ParserError((uint32_t)parserErr);
        }
        if (m_durationMs < 0) {
            return PARSER_ERR_BAD_DURATION;
        }

        if (m_activeSpeed > 0) {
            if (m_startPosMs >= 0) {
                gotFrame = m_activeStream->seekToTimeMs(static_cast<unsigned int>(m_startPosMs),
                                                        &framePkt, &parserErr);
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
            int sampleRate = 0;
            uint16_t samplesPerFrame = 0;
            if (m_activeStream->getSampleRate(&sampleRate, &parserErr) == 0) {
                return mapMp3ParserError((uint32_t)parserErr);
            }
            if (m_activeStream->getSamplesPerFrame(&samplesPerFrame, &parserErr) == 0) {
                return mapMp3ParserError((uint32_t)parserErr);
            }
            float estFrameMs = ((float)samplesPerFrame * 1000.0f) / (float)(uint32_t)sampleRate;
            float backSpan = (mode == MODE_STREAM_PLAY) ? (estFrameMs * 20.0f) : (estFrameMs * 2.0f);

            if (m_endPosMs < 0) {
                float seekPos = (float)(uint32_t)m_durationMs - backSpan;
                if (seekPos < 0.0f) {
                    seekPos = 0.0f;
                }
                gotFrame = m_activeStream->seekToTimeMs((uint32_t)seekPos, &framePkt, &parserErr);
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
                gotFrame = m_activeStream->seekToTimeMs((uint32_t)seekPos, &framePkt, &parserErr);
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

    uint32_t activeSamples = framePkt.samplesPerFrame - framePkt.leadingSkipSamples
                             - framePkt.trailingSkipSamples;
    g_lastMp3SampleRate = framePkt.sampleRate;
    m_seekPending = 0u;
    outPkt->frameData = framePkt.frameData;
    outPkt->payloadSize = framePkt.frameSize;
    outPkt->timestampMs = m_currentPosMs;
    outPkt->frameFlags = framePkt.packetSize;
    outPkt->frameDurationMs = ((float)activeSamples * 1000.0f) / (float)framePkt.sampleRate;
    outPkt->sampleRate = framePkt.sampleRate;

    if (framePkt.channelMode <= 1u) {
        outPkt->channelConfig = (framePkt.channelMode > 0u) ? 1 : 0;
    } else if (framePkt.channelMode <= 3u) {
        outPkt->channelConfig = 2;
    } else {
        return -1;
    }

    outPkt->leadingSkip = framePkt.leadingSkipSamples;
    outPkt->trailingSkip = framePkt.trailingSkipSamples;

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
PipelineEventListener *Mp3Factory::createPipeline(int mode, AudioOpenCallback openCb,
                                                  AudioCloseCallback closeCb,
                                                  AudioWriteCallback writeCb,
                                                  AudioFlushCallback flushCb, void *userContext)
{
    Mp3Parser *parser = NULL;
    void *parserMem = localAlloc(sizeof(Mp3Parser));
    if (parserMem != NULL) {
        parser = new (parserMem) Mp3Parser("SceShellMusicCoreMp3Parser", mode);
    }

    AudioDecoderBase *decoder = NULL;
    void *decMem = localAlloc(sizeof(Mp3Decoder));
    if (decMem != NULL) {
        decoder = new (decMem) Mp3Decoder("SceShellMusicCoreMp3Decoder");
    }

    if (mode != MODE_LOCAL_PLAY && mode != MODE_STREAM_PLAY && mode != MODE_DIRECT_PLAY) {
        return NULL;
    }

    int pipeSlots = (mode == MODE_STREAM_PLAY) ? 16 : 6;
    PipeConsumerControl *decoderConsumer = static_cast<PipeConsumerControl *>(decoder);
    PipeInterface *pipe = createBufRefPipe(pipeSlots, parser, decoderConsumer);
    AudioOutputFilter *audio = createAudioOutputFilter(4608u, 16, decoder, openCb, closeCb,
                                                       writeCb, flushCb, userContext);

    if (parser != NULL) {
        parser->setOutputPipe(pipe);
    }
    if (decoder != NULL) {
        decoder->setInputPipe(pipe);
        decoder->setOutputPipe(static_cast<PipeInterface *>(audio));
    }

    PipelineEventListener *pipeline = createDecoderPipeline(parser, pipe, decoder, audio);
    if (pipeline == NULL || parser == NULL || decoder == NULL || audio == NULL || pipe == NULL) {
        delete pipeline;
        delete parser;
        delete pipe;
        if (decoder != NULL) {
            decoder->~AudioDecoderBase();
            localFree(decoder);
        }
        delete audio;
        return NULL;
    }

    if (reinterpret_cast<DecoderPipeline *>(pipeline)->isReady()
        && parser->isInitialized()
        && audio->isInitialized()
        && pipe->isInitialized()) {
        static_cast<ActiveFilterBase *>(parser)->setEventListener(pipeline);
        static_cast<ActiveFilterBase *>(decoder)->setEventListener(pipeline);
        audio->setEventListener(pipeline);
        return pipeline;
    }

    delete pipeline;
    delete parser;
    decoder->~AudioDecoderBase();
    localFree(decoder);
    delete audio;
    delete pipe;
    return NULL;
}

/* 0x811F7860 */
int Mp3Factory::destroyPipeline(PipelineEventListener *pipeline)
{
    if (pipeline == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    DecoderPipeline *dp = reinterpret_cast<DecoderPipeline *>(pipeline);
    ParserFilterBase *parser = reinterpret_cast<ParserFilterBase *>(dp->getParser());
    delete parser;
    DecoderFilterBase *decoder = reinterpret_cast<DecoderFilterBase *>(dp->getDecoder());
    if (decoder != NULL) {
        decoder->~DecoderFilterBase();
        localFree(decoder);
    }
    AudioOutputFilter *audio = reinterpret_cast<AudioOutputFilter *>(dp->getAudioSink());
    delete audio;
    PipeInterface *pipe = reinterpret_cast<PipeInterface *>(dp->getPipe());
    delete pipe;
    delete pipeline;
    return 0;
}

/* 0x811F78C2 */
void *Mp3Factory::createMediaInfo(const char *path, int ioPriority)
{
    void *mem = localAlloc(sizeof(Mp3MediaInfo));
    if (mem == NULL) {
        return NULL;
    }
    return new (mem) Mp3MediaInfo(path, ioPriority);
}

/* 0x811F78EC */
int Mp3Factory::destroyMediaInfo(void **ioMediaInfo)
{
    if (*ioMediaInfo == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }
    delete static_cast<PipelineEventListener *>(*ioMediaInfo);
    *ioMediaInfo = NULL;
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
