/*
 * SceShell musiccore_plugin - WavFactory and WavParser (RIFF/WAVE PCM pipeline
 * factory and 20ms PCM chunk reader with 8-bit/mono-to-16-bit-stereo expansion).
 * Original address range (3.65-CEX): 0x811FA21E - 0x811FAE27
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_codec_factories.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_decoder_pipeline.h"
#include "musiccore_media.h"
#include "musiccore_pipeline_types.h"

extern "C" {
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
int sce_paf_strcmp(const char *lhs, const char *rhs);
}

namespace sce {
namespace MusicCore {

static const uint32_t FOURCC_WAV = 0x20574156u; /* ' WAV' */

static const int PARSER_ERR_OK = 0;
static const int PARSER_ERR_NO_SOURCE = 4;
static const int PARSER_ERR_EOS = 5;
static const int PARSER_ERR_OPEN_FAIL = 17;
static const int PARSER_ERR_READ_IO = 19;
static const int PARSER_ERR_OPEN_IO = 20;
static const int PARSER_ERR_BAD_STATE = 33;
static const int PARSER_ERR_UNSUPPORTED = 81;
static const int PARSER_ERR_CORRUPT = 95;
static const int PARSER_ERR_BAD_PARAM = 241;

static const int IO_ERR_OPEN_FAIL = -2146394109;     /* 0x8010A003 */
static const int IO_ERR_READ_FAIL = -2146394108;     /* 0x8010A004 */
static const int WAVE_ERR_UNSUPPORTED = -2144862159; /* 0x80280031 */
static const int WAVE_ERR_CORRUPT = -2144862158;     /* 0x80280032 */


struct WavPcmPacketOut {
    void *pcmBuffer;        /* +0 */
    uint32_t maxBufferSize; /* +4 */
    uint32_t payloadSize;   /* +8 */
    float timestampMs;      /* +12 */
    float frameDurationMs;  /* +16 */
    uint32_t sampleRate;    /* +20 */
    uint8_t reservedMid[40];/* +24..+63 */
    uint8_t channelCount;   /* +64 */
};

static bool isSupportedWavSampleRate(uint32_t sampleRate)
{
    return sampleRate == 8000u
        || sampleRate == 11025u
        || sampleRate == 12000u
        || sampleRate == 16000u
        || sampleRate == 22050u
        || sampleRate == 24000u
        || sampleRate == 32000u
        || sampleRate == 44100u
        || sampleRate == 48000u;
}

static bool isSupportedWavPcmLayout(uint16_t channels, uint16_t blockAlign, uint16_t bitsPerSample)
{
    return (channels == 1u && blockAlign == 1u && bitsPerSample == 8u)
        || (channels == 1u && blockAlign == 2u && bitsPerSample == 16u)
        || (channels == 2u && blockAlign == 2u && bitsPerSample == 8u)
        || (channels == 2u && blockAlign == 4u && bitsPerSample == 16u);
}

static int mapWavParserError(int errCode)
{
    switch (errCode) {
    case IO_ERR_OPEN_FAIL:
        return PARSER_ERR_OPEN_IO;
    case IO_ERR_READ_FAIL:
        return PARSER_ERR_READ_IO;
    case WAVE_ERR_UNSUPPORTED:
        return PARSER_ERR_UNSUPPORTED;
    case WAVE_ERR_CORRUPT:
        return PARSER_ERR_CORRUPT;
    default:
        return -1;
    }
}

/* ===== WavParser (0x811FA3F4 - 0x811FAE27, size 112 bytes) ===== */

class WavParser : public AudioParserBase {
public:
    explicit WavParser(const char *threadName);
    virtual ~WavParser();

    virtual int onPipeConsumed(int frameFlags);
    virtual int onPipeUnderflow();
    virtual int onFrameConsumed(int frameFlags) { (void)frameFlags; return 0; }
    virtual int openStream();
    virtual int closeStream();
    virtual int produceFrame(MediaFrameBuffer *outBuf);

    virtual int findCachedMediaInfo(const char *path, MediaInfoHandle **outInfo);
    virtual int setMediaInfo(MediaInfoHandle *mediaInfo);
    virtual int prefetchMediaInfo(MediaInfoHandle *mediaInfo);
    virtual bool hasNextMedia() const { return m_nextMedia != NULL; }
    virtual void *getCodecConfig(uint32_t *outSize) { *outSize = 0; return NULL; }
    virtual void setPlaySpeed(int speed) { m_requestedSpeed = (speed == 1) ? 1 : (speed & ~1); }
    virtual int setStartPosition(int startMs);
    virtual int setEndPosition(int endMs);
    virtual int getDurationMs();
    virtual int seekPosition(int positionMs) { (void)positionMs; return 0; }
    virtual int resetPosition();
    virtual int getPlaySpeed() const { return m_requestedSpeed; }
    virtual int getStartPosition() const { return m_startPosMs; }
    virtual int getEndPosition() const { return m_endPosMs; }
    virtual bool isInitialized() const { return ActiveFilterBase::isInitialized(); }

    int parseWaveHeader();

    int m_requestedSpeed;              /* +48 */
    int m_activeSpeed;                 /* +52 */
    int m_startPosMs;                  /* +56 */
    int m_endPosMs;                    /* +60 */
    int m_currentChunkIdx;             /* +64 */
    int m_formatTag;                   /* +68 */
    uint16_t m_channels;               /* +72 */
    uint16_t m_padChannels;            /* +74 */
    uint32_t m_sampleRate;             /* +76 */
    uint16_t m_bitsPerSample;          /* +80 */
    uint16_t m_padBits;                /* +82 */
    uint32_t m_chunkBytes;             /* +84 */
    uint32_t m_lastChunkIdx;           /* +88 */
    float m_bytesPerMs;                /* +92 */
    WavCacheIo *m_activeCacheIo;       /* +96 */
    WavCacheIo *m_nextCacheIo;         /* +100 */
    WavMediaInfo *m_activeMedia;       /* +104 */
    WavMediaInfo *m_nextMedia;         /* +108 */
};

/* 0x811FA3F4 */
WavParser::WavParser(const char *threadName)
    : AudioParserBase(1, threadName)
    , m_requestedSpeed(0)
    , m_activeSpeed(0)
    , m_startPosMs(-1)
    , m_endPosMs(-1)
    , m_currentChunkIdx(-1)
    , m_formatTag(-1)
    , m_channels(0)
    , m_padChannels(0)
    , m_sampleRate(0)
    , m_bitsPerSample(0)
    , m_padBits(0)
    , m_chunkBytes(0)
    , m_lastChunkIdx(0)
    , m_bytesPerMs(0.0f)
    , m_activeCacheIo(NULL)
    , m_nextCacheIo(NULL)
    , m_activeMedia(NULL)
    , m_nextMedia(NULL)
{
}

/* 0x811FA44A / 0x811FA4B0 */
WavParser::~WavParser()
{
    if (m_nextMedia != NULL) {
        MediaInfoHandle *nextHandle = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
        { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(nextHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); nextHandle = NULL; }
        m_nextMedia = NULL;
        m_nextCacheIo = NULL;
    }
    if (m_activeMedia != NULL) {
        MediaInfoHandle *curHandle = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
        { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(curHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); curHandle = NULL; }
        m_activeMedia = NULL;
        m_activeCacheIo = NULL;
    }
}

/* 0x811FADE0 */
int WavParser::onPipeConsumed(int frameFlags)
{
    int ret = onFrameConsumed(frameFlags);
    m_wakeEvent.signal(4u);
    return ret;
}

/* 0x811FADFA */
int WavParser::onPipeUnderflow()
{
    m_wakeEvent.signal(4u);
    return 0;
}

/* 0x811FA51C */
int WavParser::findCachedMediaInfo(const char *path, MediaInfoHandle **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    m_lock.lock();
    if (m_activeCacheIo != NULL && sce_paf_strcmp(m_activeCacheIo->getPath(), path) == 0) {
        m_lock.unlock();
        *outInfo = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
        return 0;
    }
    if (m_nextCacheIo != NULL && sce_paf_strcmp(m_nextCacheIo->getPath(), path) == 0) {
        *outInfo = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
        m_lock.unlock();
        return 0;
    }
    m_lock.unlock();
    *outInfo = NULL;
    return 0;
}

/* 0x811FA58C */
int WavParser::setMediaInfo(MediaInfoHandle *handle)
{
    if (handle == NULL || handle->mediaFourcc != FOURCC_WAV) {
        return PARSER_ERR_BAD_PARAM;
    }
    WavMediaInfo *mediaInfo = reinterpret_cast<WavMediaInfo *>(handle);
    if (mediaInfo->m_cacheIo == NULL) {
        MediaInfoHandle *badHandle = reinterpret_cast<MediaInfoHandle *>(mediaInfo);
        { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(badHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); badHandle = NULL; }
        return -1;
    }

    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }

    const char *newPath = mediaInfo->m_cacheIo->getPath();
    if (m_activeCacheIo != NULL) {
        if (sce_paf_strcmp(m_activeCacheIo->getPath(), newPath) == 0) {
            m_lock.unlock();
            delayWorkerThread(20000u);
            return 0;
        }
        if (m_activeMedia == m_nextMedia) {
            m_nextMedia = NULL;
            m_nextCacheIo = NULL;
        }
        if (m_activeMedia != NULL) {
            MediaInfoHandle *oldHandle = reinterpret_cast<MediaInfoHandle *>(m_activeMedia);
            { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(oldHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); oldHandle = NULL; }
            m_activeMedia = NULL;
        }
        m_activeCacheIo = NULL;
    }

    m_currentChunkIdx = -1;
    m_formatTag = -1;

    if (m_nextCacheIo != NULL && sce_paf_strcmp(m_nextCacheIo->getPath(), newPath) == 0) {
        m_activeCacheIo = m_nextCacheIo;
        m_activeMedia = m_nextMedia;
        if (m_activeCacheIo != NULL) {
            m_activeCacheIo->setPriorityMode(0);
        }
        m_nextCacheIo = NULL;
        m_nextMedia = NULL;
        m_lock.unlock();
        return 0;
    }

    if (m_nextCacheIo != NULL) {
        if (m_nextMedia != NULL) {
            MediaInfoHandle *nextHandle = reinterpret_cast<MediaInfoHandle *>(m_nextMedia);
            { MediaInfoBase *relMedia = reinterpret_cast<MediaInfoBase *>(nextHandle); DecoderManager::releaseMediaInfo(&relMedia, 1u); nextHandle = NULL; }
            m_nextMedia = NULL;
        }
        m_nextCacheIo = NULL;
    }

    mediaInfo->m_sharedFlag = 1u;
    m_activeMedia = mediaInfo;
    m_activeCacheIo = mediaInfo->m_cacheIo;
    m_lock.unlock();
    return 0;
}

/* 0x811FA6C4 */
int WavParser::prefetchMediaInfo(MediaInfoHandle *handle)
{
    if (handle == NULL || handle->mediaFourcc != FOURCC_WAV) {
        return PARSER_ERR_BAD_PARAM;
    }
    WavMediaInfo *mediaInfo = reinterpret_cast<WavMediaInfo *>(handle);
    if (mediaInfo->m_cacheIo == NULL) {
        return -1;
    }

    m_lock.lock();
    const char *newPath = mediaInfo->m_cacheIo->getPath();
    if (m_nextCacheIo != NULL) {
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), newPath) == 0) {
            m_lock.unlock();
            return 0;
        }
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), m_activeCacheIo->getPath()) == 0) {
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            m_lock.unlock();
            return 0;
        }
    }

    if (m_nextMedia != NULL) {
        return -1;
    }

    mediaInfo->m_sharedFlag = 1u;
    m_nextMedia = mediaInfo;
    m_nextCacheIo = mediaInfo->m_cacheIo;
    m_nextCacheIo->setPriorityMode(1);

    if (m_nextCacheIo->isOpen() != 1 && m_nextCacheIo->open() < 0) {
        m_lock.unlock();
        return PARSER_ERR_OPEN_FAIL;
    }

    int scanErr = m_nextCacheIo->scanChunks(3u);
    m_lock.unlock();
    if (scanErr == 0) {
        return 0;
    }
    return mapWavParserError(scanErr);
}

/* 0x811FA7FA */
int WavParser::setStartPosition(int startMs)
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_startPosMs = startMs;
    m_currentChunkIdx = -1;
    m_lock.unlock();
    return 0;
}

/* 0x811FA82C */
int WavParser::setEndPosition(int endMs)
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_endPosMs = endMs;
    m_currentChunkIdx = -1;
    m_lock.unlock();
    return 0;
}

/* 0x811FA85E */
int WavParser::resetPosition()
{
    m_lock.lock();
    if (m_runState != 2) {
        m_lock.unlock();
        return PARSER_ERR_BAD_STATE;
    }
    m_currentChunkIdx = -1;
    m_lock.unlock();
    return 0;
}

/* 0x811FA88C */
int WavParser::parseWaveHeader()
{
    WavFmtHeader fmt;
    fmt.formatTag = 0;
    fmt.channels = 0;
    fmt.sampleRate = 0;
    fmt.avgBytesPerSec = 0;
    fmt.blockAlign = 0;
    fmt.bitsPerSample = 0;

    int err = m_activeCacheIo->scanChunks(3u);
    if (err != 0) {
        return err;
    }
    err = m_activeCacheIo->readFmtHeader(&fmt);
    if (err != 0) {
        return err;
    }
    if (fmt.avgBytesPerSec == 0u || fmt.formatTag != 1u) {
        return WAVE_ERR_UNSUPPORTED;
    }
    if (!isSupportedWavSampleRate(fmt.sampleRate)
        || !isSupportedWavPcmLayout(fmt.channels, fmt.blockAlign, fmt.bitsPerSample)) {
        return WAVE_ERR_UNSUPPORTED;
    }

    m_chunkBytes = (uint32_t)((int)((float)fmt.sampleRate * 0.02f)) * fmt.blockAlign;
    m_lastChunkIdx = (m_activeCacheIo->m_dataSize - 1u) / m_chunkBytes;
    m_formatTag = 1;
    m_channels = fmt.channels;
    m_sampleRate = fmt.sampleRate;
    m_bitsPerSample = fmt.bitsPerSample;
    m_bytesPerMs = (float)fmt.avgBytesPerSec * 0.001f;
    return 0;
}

/* 0x811FA9FA */
int WavParser::getDurationMs()
{
    m_lock.lock();
    if (m_activeCacheIo == NULL) {
        return 0;
    }

    if (m_formatTag < 0) {
        if (m_activeCacheIo->isOpen() != 1 && m_activeCacheIo->open() < 0) {
            m_lock.unlock();
            return 0;
        }
        if (parseWaveHeader() != 0) {
            m_lock.unlock();
            return 0;
        }
    }

    int durationMs = (int)((float)m_activeCacheIo->m_dataSize / m_bytesPerMs);
    m_lock.unlock();
    return durationMs;
}

/* 0x811FAA76 */
int WavParser::openStream()
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

/* 0x811FAAAC */
int WavParser::closeStream()
{
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    m_activeCacheIo->setPriorityMode(1);
    return 0;
}

/* 0x811FAAC2 */
int WavParser::produceFrame(MediaFrameBuffer *outBuf)
{
    WavPcmPacketOut *outPkt = reinterpret_cast<WavPcmPacketOut *>(outBuf);
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    if (outPkt == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }

    if (m_formatTag < 0) {
        int err = parseWaveHeader();
        if (err != 0) {
            return mapWavParserError(err);
        }
    }
    if (m_activeCacheIo->m_dataSize == 0u) {
        return PARSER_ERR_EOS;
    }

    int startChunk = 0;
    if (m_startPosMs >= 0) {
        startChunk = (int)(m_bytesPerMs * (float)m_startPosMs) / (int)m_chunkBytes;
        if ((int)m_lastChunkIdx < startChunk) {
            startChunk = (int)m_lastChunkIdx;
        }
    }

    int endChunk = (int)m_lastChunkIdx;
    if (m_endPosMs >= 0) {
        endChunk = (int)(m_bytesPerMs * (float)m_endPosMs) / (int)m_chunkBytes;
        if ((int)m_lastChunkIdx < endChunk) {
            endChunk = (int)m_lastChunkIdx;
        }
    }

    if (m_currentChunkIdx < 0) {
        m_currentChunkIdx = (m_requestedSpeed <= 0) ? endChunk : startChunk;
    } else {
        m_currentChunkIdx += m_requestedSpeed;
        if (m_currentChunkIdx < startChunk || m_currentChunkIdx > endChunk) {
            return PARSER_ERR_EOS;
        }
    }

    m_lock.lock();
    if (m_requestedSpeed == 0) {
        m_lock.unlock();
        return PARSER_ERR_BAD_PARAM;
    }
    if (m_requestedSpeed != m_activeSpeed) {
        if (m_requestedSpeed == 1) {
            m_activeCacheIo->setCacheHint(1);
        } else if (m_requestedSpeed < 0) {
            m_activeCacheIo->setCacheHint(3);
        } else {
            m_activeCacheIo->setCacheHint(2);
        }
        m_activeSpeed = m_requestedSpeed;
    }
    m_lock.unlock();

    uint32_t byteOffset = m_chunkBytes * (uint32_t)m_currentChunkIdx;
    void *rawData = NULL;
    uint32_t readBytes = 0;
    int readErr = m_activeCacheIo->readDataRange(byteOffset, m_chunkBytes, &rawData, &readBytes);
    if (readErr != 0) {
        if (readErr == IO_ERR_READ_FAIL) {
            return PARSER_ERR_READ_IO;
        }
        if (readErr == IO_ERR_OPEN_FAIL) {
            return PARSER_ERR_OPEN_IO;
        }
        return -1;
    }

    if (m_formatTag == 1) {
        if (m_channels == 2u) {
            if (m_bitsPerSample == 16u) {
                readBytes &= ~3u;
                sce_paf_memcpy(outPkt->pcmBuffer, rawData, readBytes);
                outPkt->payloadSize = readBytes;
            } else if (m_bitsPerSample == 8u) {
                readBytes &= ~1u;
                const uint8_t *src = (const uint8_t *)rawData;
                int16_t *dst = (int16_t *)outPkt->pcmBuffer;
                for (uint32_t idx = 0; idx < readBytes; ++idx) {
                    dst[idx] = (int16_t)(((int)src[idx] - 128) << 8);
                }
                outPkt->payloadSize = readBytes * 2u;
            }
        } else if (m_channels == 1u) {
            if (m_bitsPerSample == 16u) {
                uint32_t sampleCount = readBytes >> 1;
                const int16_t *src = (const int16_t *)rawData;
                int16_t *dst = (int16_t *)outPkt->pcmBuffer;
                for (uint32_t idx = 0; idx < sampleCount; ++idx) {
                    int16_t sample = src[idx];
                    dst[idx * 2u + 0u] = sample;
                    dst[idx * 2u + 1u] = sample;
                }
                outPkt->payloadSize = sampleCount * 4u;
            } else if (m_bitsPerSample == 8u) {
                const uint8_t *src = (const uint8_t *)rawData;
                int16_t *dst = (int16_t *)outPkt->pcmBuffer;
                for (uint32_t idx = 0; idx < readBytes; ++idx) {
                    int16_t sample = (int16_t)(((int)src[idx] - 128) << 8);
                    dst[idx * 2u + 0u] = sample;
                    dst[idx * 2u + 1u] = sample;
                }
                outPkt->payloadSize = readBytes * 4u;
            }
        }
    }

    outPkt->sampleRate = m_sampleRate;
    outPkt->timestampMs = (float)byteOffset / m_bytesPerMs;
    if (m_channels == 1u || m_channels == 2u) {
        outPkt->channelCount = 2u;
        return 0;
    }
    return -1;
}

/* ===== WavFactory (0x811FA248 - 0x811FA3F3) ===== */

WavFactory g_wavFactory; /* 0x8156F3F8 */

/* 0x811FA248 */
int WavFactory::getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf)
{
    *outParserBuf = 54272u;
    *outDecoderBuf = 0u;
    *outAudioBuf = 0x4000u;
    return 0x4000;
}

/* 0x811FA282 */
PipelineEventListener *WavFactory::createPipeline(AudioOpenCallback openCb,
                                                  AudioCloseCallback closeCb,
                                                  AudioWriteCallback writeCb,
                                                  AudioFlushCallback flushCb, void *userContext)
{
    void *parserMem = localAlloc(sizeof(WavParser));
    WavParser *parser = NULL;
    if (parserMem != NULL) {
        parser = new (parserMem) WavParser("SceShellMusicCoreWavParser");
    }
    if (parser == NULL) {
        return NULL;
    }

    AudioOutputFilter *audio = createAudioOutputFilter(4096u, 16, parser, openCb, closeCb,
                                                       writeCb, flushCb, userContext);
    if (audio == NULL) {
        delete parser;
        return NULL;
    }

    parser->setOutputPipe(static_cast<PipeInterface *>(audio));
    PipelineEventListener *pipeline = createDecoderPipeline(parser, NULL, NULL, audio);
    if (pipeline == NULL) {
        delete audio;
        delete parser;
        return NULL;
    }

    static_cast<ActiveFilterBase *>(parser)->setEventListener(pipeline);
    audio->setEventListener(pipeline);

    if (audio->isInitialized() && reinterpret_cast<DecoderPipeline *>(pipeline)->isReady()) {
        return pipeline;
    }

    delete pipeline;
    delete audio;
    delete parser;
    return NULL;
}

/* 0x811FA36A */
int WavFactory::destroyPipeline(PipelineEventListener *pipeline)
{
    if (pipeline == NULL) {
        return PARSER_ERR_BAD_PARAM;
    }
    DecoderPipeline *dp = reinterpret_cast<DecoderPipeline *>(pipeline);
    ParserFilterBase *parser = reinterpret_cast<ParserFilterBase *>(dp->getParser());
    delete parser;
    AudioOutputFilter *audio = reinterpret_cast<AudioOutputFilter *>(dp->getAudioSink());
    delete audio;
    delete pipeline;
    return 0;
}

/* 0x811FA3A4 */
void *WavFactory::createMediaInfo(const char *path)
{
    void *mem = localAlloc(sizeof(WavMediaInfo));
    if (mem == NULL) {
        return NULL;
    }
    return new (mem) WavMediaInfo(path);
}

} /* namespace MusicCore */
} /* namespace sce */
