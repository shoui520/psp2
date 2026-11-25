/*
 * SceShell musiccore_plugin - WavFactory and WavParser (RIFF/WAVE PCM pipeline
 * factory and 20ms PCM chunk reader with 8-bit/mono-to-16-bit-stereo expansion).
 * Original address range (3.65-CEX): 0x811FA21E - 0x811FAE27
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

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

extern void *allocFromHeap(size_t size);

extern void decoderManagerReleaseMediaInfo(MediaInfoHandle **ioHandle, int forceRelease);
extern void *wavMediaInfoConstruct(void *mem, const char *path);

struct WavFmtHeader {
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

class WavCacheIo {
public:
    virtual ~WavCacheIo() {}
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

    void *m_cacheHandle;
    uint8_t m_initialized;
    uint8_t m_padInit[3];
    char *m_filePath;
    void *m_fileObj;
    void *m_fileRefCount;
    int m_priorityMode;
    int m_padPriority;
    int64_t m_filePos;
    int m_chunkSize;
    int m_blockCount;
    uint8_t m_foundChunks;
    uint8_t m_padChunks[3];
    uint32_t m_fmtOffset;
    uint32_t m_fmtSize;
    uint32_t m_dataOffset;
    uint32_t dataSize; /* +64 */
};

extern int wavCacheScanChunks(WavCacheIo *io, uint8_t targetMask);
extern int wavCacheReadFmtHeader(WavCacheIo *io, WavFmtHeader *outFmt);
extern int wavCacheReadDataRange(WavCacheIo *io, uint32_t offset, uint32_t size,
                                 void **outPtr, uint32_t *outSize);

struct WavMediaInfo : public MediaInfoHandle {
    char mediaPath[2049]; /* +9..+2057 */
    uint8_t padPath[2];   /* +2058..+2059 */
    WavCacheIo *cacheIo;  /* +2060 */
};

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

class WavParser : public ParserFilterBase {
public:
    explicit WavParser(const char *threadName);
    virtual ~WavParser();

    virtual int onFrameConsumed(int frameFlags) { (void)frameFlags; return 0; }
    virtual int openStream() { return onStart(); }
    virtual int closeStream() { return onStop(); }
    virtual int produceFrame(MediaFrameBuffer *outBuf)
    {
        return readFrame(reinterpret_cast<WavPcmPacketOut *>(outBuf));
    }

    int findCachedMediaInfo(const char *path, WavMediaInfo **outInfo);
    int setMediaInfo(WavMediaInfo *mediaInfo);
    int prefetchMediaInfo(WavMediaInfo *mediaInfo);
    int setStartPosition(int startMs);
    int setEndPosition(int endMs);
    int resetPosition();
    int parseWaveHeader();
    int getDurationMs();
    int onStart();
    int onStop();
    int readFrame(WavPcmPacketOut *outPkt);

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
    : ParserFilterBase(1, threadName)
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
}

/* 0x811FA51C */
int WavParser::findCachedMediaInfo(const char *path, WavMediaInfo **outInfo)
{
    if (path == NULL) {
        *outInfo = NULL;
        return PARSER_ERR_BAD_PARAM;
    }

    syncMutexLock(&m_lock);
    if (m_activeCacheIo != NULL && sce_paf_strcmp(m_activeCacheIo->getPath(), path) == 0) {
        syncMutexUnlock(&m_lock);
        *outInfo = m_activeMedia;
        return 0;
    }
    if (m_nextCacheIo != NULL && sce_paf_strcmp(m_nextCacheIo->getPath(), path) == 0) {
        *outInfo = m_nextMedia;
        syncMutexUnlock(&m_lock);
        return 0;
    }
    syncMutexUnlock(&m_lock);
    *outInfo = NULL;
    return 0;
}

/* 0x811FA58C */
int WavParser::setMediaInfo(WavMediaInfo *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_WAV) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->cacheIo == NULL) {
        MediaInfoHandle *badHandle = mediaInfo;
        decoderManagerReleaseMediaInfo(&badHandle, 1);
        return -1;
    }

    syncMutexLock(&m_lock);
    if (m_runState != 2) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_BAD_STATE;
    }

    const char *newPath = mediaInfo->cacheIo->getPath();
    if (m_activeCacheIo != NULL) {
        if (sce_paf_strcmp(m_activeCacheIo->getPath(), newPath) == 0) {
            syncMutexUnlock(&m_lock);
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
        syncMutexUnlock(&m_lock);
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
    syncMutexUnlock(&m_lock);
    return 0;
}

/* 0x811FA6C4 */
int WavParser::prefetchMediaInfo(WavMediaInfo *mediaInfo)
{
    if (mediaInfo == NULL || mediaInfo->mediaFourcc != FOURCC_WAV) {
        return PARSER_ERR_BAD_PARAM;
    }
    if (mediaInfo->cacheIo == NULL) {
        return -1;
    }

    syncMutexLock(&m_lock);
    const char *newPath = mediaInfo->cacheIo->getPath();
    if (m_nextCacheIo != NULL) {
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), newPath) == 0) {
            syncMutexUnlock(&m_lock);
            return 0;
        }
        if (sce_paf_strcmp(m_nextCacheIo->getPath(), m_activeCacheIo->getPath()) == 0) {
            m_nextCacheIo = m_activeCacheIo;
            m_nextMedia = m_activeMedia;
            syncMutexUnlock(&m_lock);
            return 0;
        }
    }

    if (m_nextMedia != NULL) {
        return -1;
    }

    mediaInfo->sharedFlag = 1u;
    m_nextMedia = mediaInfo;
    m_nextCacheIo = mediaInfo->cacheIo;
    m_nextCacheIo->setPriorityMode(1);

    if (m_nextCacheIo->isOpen() != 1 && m_nextCacheIo->open() < 0) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_OPEN_FAIL;
    }

    int scanErr = wavCacheScanChunks(m_nextCacheIo, 3u);
    syncMutexUnlock(&m_lock);
    if (scanErr == 0) {
        return 0;
    }
    return mapWavParserError(scanErr);
}

/* 0x811FA7FA */
int WavParser::setStartPosition(int startMs)
{
    syncMutexLock(&m_lock);
    if (m_runState != 2) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_BAD_STATE;
    }
    m_startPosMs = startMs;
    m_currentChunkIdx = -1;
    syncMutexUnlock(&m_lock);
    return 0;
}

/* 0x811FA82C */
int WavParser::setEndPosition(int endMs)
{
    syncMutexLock(&m_lock);
    if (m_runState != 2) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_BAD_STATE;
    }
    m_endPosMs = endMs;
    m_currentChunkIdx = -1;
    syncMutexUnlock(&m_lock);
    return 0;
}

/* 0x811FA85E */
int WavParser::resetPosition()
{
    syncMutexLock(&m_lock);
    if (m_runState != 2) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_BAD_STATE;
    }
    m_currentChunkIdx = -1;
    syncMutexUnlock(&m_lock);
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

    int err = wavCacheScanChunks(m_activeCacheIo, 3u);
    if (err != 0) {
        return err;
    }
    err = wavCacheReadFmtHeader(m_activeCacheIo, &fmt);
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
    m_lastChunkIdx = (m_activeCacheIo->dataSize - 1u) / m_chunkBytes;
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
    syncMutexLock(&m_lock);
    if (m_activeCacheIo == NULL) {
        return 0;
    }

    if (m_formatTag < 0) {
        if (m_activeCacheIo->isOpen() != 1 && m_activeCacheIo->open() < 0) {
            syncMutexUnlock(&m_lock);
            return 0;
        }
        if (parseWaveHeader() != 0) {
            syncMutexUnlock(&m_lock);
            return 0;
        }
    }

    int durationMs = (int)((float)m_activeCacheIo->dataSize / m_bytesPerMs);
    syncMutexUnlock(&m_lock);
    return durationMs;
}

/* 0x811FAA76 */
int WavParser::onStart()
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
int WavParser::onStop()
{
    if (m_activeCacheIo == NULL) {
        return PARSER_ERR_NO_SOURCE;
    }
    m_activeCacheIo->setPriorityMode(1);
    return 0;
}

/* 0x811FAAC2 */
int WavParser::readFrame(WavPcmPacketOut *outPkt)
{
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
    if (m_activeCacheIo->dataSize == 0u) {
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

    syncMutexLock(&m_lock);
    if (m_requestedSpeed == 0) {
        syncMutexUnlock(&m_lock);
        return PARSER_ERR_BAD_PARAM;
    }
    if (m_requestedSpeed != m_activeSpeed) {
        if (m_requestedSpeed == 1) {
            m_activeCacheIo->setCacheHint(1, 0);
        } else if (m_requestedSpeed < 0) {
            m_activeCacheIo->setCacheHint(3, 0);
        } else {
            m_activeCacheIo->setCacheHint(2, 0);
        }
        m_activeSpeed = m_requestedSpeed;
    }
    syncMutexUnlock(&m_lock);

    uint32_t byteOffset = m_chunkBytes * (uint32_t)m_currentChunkIdx;
    void *rawData = NULL;
    uint32_t readBytes = 0;
    int readErr = wavCacheReadDataRange(m_activeCacheIo, byteOffset, m_chunkBytes, &rawData, &readBytes);
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

class WavFactory {
public:
    WavFactory() {}
    virtual ~WavFactory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf);
    PipelineEventListener *createPipeline(AudioOpenCallback openCb, AudioCloseCallback closeCb,
                                          AudioWriteCallback writeCb, AudioFlushCallback flushCb,
                                          void *userContext);
    int destroyPipeline(PipelineEventListener *pipeline);
    void *createMediaInfo(const char *path);
};

static WavFactory g_wavFactory; /* 0x8156F3F8 */

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
    void *parserMem = allocFromHeap(sizeof(WavParser));
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

    if (audio->isInitialized() && decoderPipelineIsReady(pipeline)) {
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
    ParserFilterBase *parser = decoderPipelineGetParser(pipeline);
    delete parser;
    AudioOutputFilter *audio = decoderPipelineGetAudio(pipeline);
    delete audio;
    delete pipeline;
    return 0;
}

/* 0x811FA3A4 */
void *WavFactory::createMediaInfo(const char *path)
{
    void *mem = allocFromHeap(2064u);
    if (mem == NULL) {
        return NULL;
    }
    return wavMediaInfoConstruct(mem, path);
}

} /* namespace MusicCore */
} /* namespace sce */
