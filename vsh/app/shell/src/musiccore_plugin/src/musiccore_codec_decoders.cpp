/*
 * SceShell musiccore_plugin - AudioDecoderBase, Mp3Decoder, Mp4Decoder, At9Decoder,
 * AudioOutputFilter ("SceShellMusicCoreAIF"), and BufRefPipe ("SceShellMusicCoreBufRefPipe").
 * Original address range (3.65-CEX): 0x81207E74 - 0x81209B57
 */

#include <kernel.h>
#include <std/new>
#include <std/stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_codec_decoders.h"
#include "musiccore_heap.h"

extern "C" {
void *sce_paf_memset(void *dst, int val, SceSize size);
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
void *sce_paf_memmove(void *dst, const void *src, SceSize size);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelUnlockMutex(SceUID mutexId, int unlockCount);
void *ScePafStdc_3CF19248(SceSize size, void *heap);
int SceAudiocodec_E2AF3A54(void *ctrl, int codecType);
int SceAudiocodec_1F9A3722(void *ctrl, int codecType);
int SceAudiocodec_E827E04F(void *ctrl, int codecType);
}

namespace sce {
namespace MusicCore {

static const int CODEC_TYPE_MP3 = 0x1002;           /* 4098 */
static const int CODEC_TYPE_AAC = 0x1003;           /* 4099 */
static const int CODEC_TYPE_AT9 = 0x1004;           /* 4100 */

static const int DEC_ERR_NO_MEMORY = 4;
static const int DEC_ERR_TRIM_BOUNDS = 81;          /* 'Q' */
static const int DEC_ERR_CODEC_FAIL = 97;           /* 'a' */
static const int DEC_ERR_INVALID_ARG = 241;
static const int PIPE_ERR_FULL = 66;
static const int PIPE_ERR_EMPTY = 67;
static const int PIPE_ERR_OUT_OF_ORDER = 68;

static const int MC_ERR_INTERNAL = -1879113726;     /* 0x8FFF0002 */
static const int MC_ERR_NO_MEMORY = -1879113724;    /* 0x8FFF0004 */
static const int MC_ERR_AUDIO_DROPPED = -1879113710;/* 0x8FFF0012 */

static const uint32_t SILENCE_GRAIN_BYTES = 0x4000u;
static const int MAX_INFLIGHT_AUDIO_BUFS = 16;

extern int mapMp4ParserError(uint32_t errCode);
extern int mapAt9ParserError(uint32_t errCode);

static AudioOutputFilter *g_activeAudioOutputFilter = NULL; /* 0x81545060 */

/* ===== AudioDecoderBase (56 bytes, 0x81209AA8 - 0x81209B57) ===== */

/* 0x81209AA8 */
AudioDecoderBase::AudioDecoderBase(int workerKind, const char *threadName)
    : DecoderFilterBase(workerKind, threadName)
{
}

/* 0x81209ACC / 0x81209AF4 */
AudioDecoderBase::~AudioDecoderBase()
{
}

/* 0x81207F34 */
int AudioDecoderBase::onPipeConsumed(int frameFlags)
{
    int ret = onFrameConsumed(frameFlags);
    m_wakeEvent.signal(4u);
    return ret;
}

/* 0x81207F4E */
int AudioDecoderBase::onPipeUnderflow()
{
    m_wakeEvent.signal(4u);
    return 0;
}

/* 0x81207F5C */
int AudioDecoderBase::onFrameConsumed(int frameFlags)
{
    (void)frameFlags;
    return 0;
}

/* ===== Mp3Decoder (168 bytes, 0x81207E74 - 0x8120825B) ===== */

/* 0x81207E74 */
Mp3Decoder::Mp3Decoder(const char *threadName)
    : AudioDecoderBase(2, threadName)
    , m_ctrl(NULL)
    , m_workMemPool(NULL)
    , m_resetPending(0)
{
    uintptr_t aligned = ((uintptr_t)m_ctrlArena + 3u) & ~(uintptr_t)3u;
    m_ctrl = (AudiocodecCtrl *)aligned;
}

/* 0x81207EB0 / 0x81207EF0 */
Mp3Decoder::~Mp3Decoder()
{
    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    m_ctrl = NULL;
}

void Mp3Decoder::configureDecoder(const void *configData, uint32_t configSize)
{
    (void)configData;
    (void)configSize;
}

/* 0x81207F5E */
int Mp3Decoder::openDecoder()
{
    if (!isAudioCodecModuleLoaded())
        return -1;
    if (m_ctrl == NULL)
        return DEC_ERR_NO_MEMORY;
    if (SceAudiocodec_E2AF3A54(m_ctrl, CODEC_TYPE_MP3) < 0)
        return DEC_ERR_CODEC_FAIL;

    m_workMemPool = BufferPool::getInstance()->acquireTyped(2);
    if (m_workMemPool == NULL)
        return DEC_ERR_NO_MEMORY;

    m_resetPending = 0;
    m_ctrl->workMem = (void *)(((uintptr_t)m_workMemPool + 3u) & ~(uintptr_t)3u);
    return (SceAudiocodec_1F9A3722(m_ctrl, CODEC_TYPE_MP3) >= 0) ? 0 : -1;
}

/* 0x81207FC8 */
int Mp3Decoder::closeDecoder()
{
    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    return 0;
}

/* 0x81207FE2 */
int Mp3Decoder::decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf)
{
    if (inBuf == NULL || outBuf == NULL)
        return DEC_ERR_INVALID_ARG;

    if (inBuf->packetFlags == 4) {
        int silenceBytes = (int)(inBuf->frameDurationMs * 4.0f * (float)inBuf->sampleRate * 0.001f);
        if ((int)outBuf->maxBufferSize < silenceBytes)
            silenceBytes = (int)outBuf->maxBufferSize;
        outBuf->payloadSize = silenceBytes;
        sce_paf_memset(outBuf->frameData, 0, (SceSize)silenceBytes);
        outBuf->timestampMs = inBuf->timestampMs;
        outBuf->frameDurationMs = inBuf->frameDurationMs;
        outBuf->sampleRate = inBuf->sampleRate;
        outBuf->channelConfig = inBuf->channelConfig;
        outBuf->packetFlags = inBuf->packetFlags;
        outBuf->discontinuity = inBuf->discontinuity;
        outBuf->leadingSkip = 0;
        outBuf->trailingSkip = 0;
        outBuf->channelCount = inBuf->channelCount;
        return 0;
    }

    if (inBuf->frameData == NULL || outBuf->frameData == NULL)
        return DEC_ERR_INVALID_ARG;

    if (inBuf->packetFlags == 1 && inBuf->discontinuity != 0) {
        if (SceAudiocodec_1F9A3722(m_ctrl, CODEC_TYPE_MP3) < 0)
            return -1;
    }

    m_ctrl->config.mp3.inputBytes = inBuf->payloadSize;
    m_ctrl->inputBuf = inBuf->frameData;
    m_ctrl->outputBuf = outBuf->frameData;

    if (SceAudiocodec_E827E04F(m_ctrl, CODEC_TYPE_MP3) < 0) {
        if ((int)m_ctrl->size == -8) {
            int silenceBytes = 0;
            if (inBuf->discontinuity == 0)
                silenceBytes = (int)((inBuf->frameDurationMs * 4.0f) / (1000.0f / (float)inBuf->sampleRate));
            outBuf->payloadSize = silenceBytes;
            int clamped = ((int)outBuf->maxBufferSize < silenceBytes) ? (int)outBuf->maxBufferSize : silenceBytes;
            sce_paf_memset(outBuf->frameData, 0, (SceSize)clamped);
            outBuf->timestampMs = inBuf->timestampMs;
            outBuf->frameDurationMs = inBuf->frameDurationMs;
            outBuf->sampleRate = inBuf->sampleRate;
            outBuf->channelConfig = inBuf->channelConfig;
            outBuf->packetFlags = inBuf->packetFlags;
            outBuf->discontinuity = inBuf->discontinuity;
            outBuf->leadingSkip = 0;
            outBuf->trailingSkip = 0;
            outBuf->channelCount = inBuf->channelCount;
            return 0;
        }
        outBuf->payloadSize = 0;
        outBuf->timestampMs = inBuf->timestampMs;
        outBuf->frameDurationMs = inBuf->frameDurationMs;
        outBuf->sampleRate = inBuf->sampleRate;
        outBuf->channelConfig = 255;
        outBuf->packetFlags = 3;
        outBuf->leadingSkip = 0;
        outBuf->trailingSkip = 0;
        outBuf->channelCount = inBuf->channelCount;
        return DEC_ERR_CODEC_FAIL;
    }

    int outBytes = (inBuf->discontinuity == 0) ? m_ctrl->outputBytes : 0;
    outBuf->payloadSize = outBytes;
    outBuf->timestampMs = inBuf->timestampMs;
    outBuf->frameDurationMs = inBuf->frameDurationMs;
    outBuf->sampleRate = inBuf->sampleRate;
    outBuf->channelConfig = inBuf->channelConfig;
    outBuf->packetFlags = inBuf->packetFlags;
    outBuf->discontinuity = inBuf->discontinuity;
    outBuf->leadingSkip = inBuf->leadingSkip;
    outBuf->trailingSkip = inBuf->trailingSkip;
    outBuf->channelCount = inBuf->channelCount;

    if (outBytes > 0) {
        uint8_t *pcmBytes = (uint8_t *)outBuf->frameData;
        if (inBuf->leadingSkip != 0) {
            int trimLead = 4 * (int)(inBuf->leadingSkip & 0x3FFFu);
            if (trimLead > outBytes)
                return DEC_ERR_TRIM_BOUNDS;
            int remain = (outBytes > trimLead) ? (outBytes - trimLead) : 0;
            if ((int)outBuf->maxBufferSize < remain)
                remain = (int)outBuf->maxBufferSize;
            sce_paf_memmove(pcmBytes, pcmBytes + trimLead, (SceSize)remain);
            outBytes -= trimLead;
            outBuf->payloadSize = outBytes;
        }
        if (inBuf->trailingSkip != 0) {
            int trimTrail = 4 * (int)(inBuf->trailingSkip & 0x3FFFu);
            if (trimTrail > outBytes)
                return DEC_ERR_TRIM_BOUNDS;
            outBuf->payloadSize = outBytes - trimTrail;
        }
    }
    return 0;
}

/* ===== Mp4Decoder (208 bytes, 0x8120825C - 0x81208793) ===== */

/* 0x8120825C */
Mp4Decoder::Mp4Decoder(const char *threadName)
    : AudioDecoderBase(2, threadName)
    , m_ctrl(NULL)
    , m_workMemPool(NULL)
    , m_resetPending(0)
    , m_lastPacketFlags(255)
    , m_consecutiveErrors(0)
{
    uintptr_t aligned = ((uintptr_t)m_ctrlArena + 3u) & ~(uintptr_t)3u;
    m_ctrl = (AudiocodecCtrl *)aligned;
    sce_paf_memset(m_ctrl->config.raw, 0, sizeof(m_ctrl->config.raw));

    m_pendingCfg.outputSampleRate = 0;
    m_pendingCfg.inputSampleRate = 0;
    m_pendingCfg.parserErrorCode = 0xFFFFFFFFu;
    m_pendingCfg.isSbrEnabled = 0;

    m_activeCfg.outputSampleRate = 0;
    m_activeCfg.inputSampleRate = 0;
    m_activeCfg.parserErrorCode = 0xFFFFFFFFu;
    m_activeCfg.isSbrEnabled = 0;
}

/* 0x812082C8 / 0x81208308 */
Mp4Decoder::~Mp4Decoder()
{
    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    m_ctrl = NULL;
}

/* 0x8120834C */
void Mp4Decoder::configureDecoder(const void *configData, uint32_t configSize)
{
    if (configData == NULL || configSize != sizeof(Mp4DecoderConfig)) {
        m_pendingCfg.isSbrEnabled = 0;
        m_pendingCfg.outputSampleRate = 0;
        m_pendingCfg.inputSampleRate = 0;
        m_pendingCfg.parserErrorCode = 0xFFFFFFFFu;
        if (configData == NULL)
            return;
    }
    const Mp4DecoderConfig *cfg = (const Mp4DecoderConfig *)configData;
    m_pendingCfg = *cfg;
}

/* 0x81208382 */
int Mp4Decoder::openDecoder()
{
    if (!isAudioCodecModuleLoaded())
        return -1;

    uint32_t parserErr = m_pendingCfg.parserErrorCode;
    m_consecutiveErrors = 0;
    if (parserErr != 0) {
        switch (parserErr) {
        case 0x8010A001u: return 17;
        case 0x8010A003u: return 20;
        case 0x8010A004u: return 19;
        case 0x80280021u: return 81;
        case 0x80280022u:
        case 0x80280028u: return 95;
        case 0x80280029u: return 69;
        case 0x8028002Au: return 21;
        default:          return mapMp4ParserError(parserErr);
        }
    }

    if (m_lastPacketFlags == 2 &&
        m_activeCfg.inputSampleRate == m_pendingCfg.inputSampleRate &&
        m_activeCfg.isSbrEnabled == m_pendingCfg.isSbrEnabled) {
        return 0;
    }

    if (m_ctrl == NULL)
        return DEC_ERR_NO_MEMORY;
    if (SceAudiocodec_E2AF3A54(m_ctrl, CODEC_TYPE_AAC) < 0)
        return DEC_ERR_CODEC_FAIL;

    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    m_workMemPool = BufferPool::getInstance()->acquireTyped(2);
    if (m_workMemPool == NULL)
        return DEC_ERR_NO_MEMORY;

    m_resetPending = 0;
    m_ctrl->workMem = (void *)(((uintptr_t)m_workMemPool + 3u) & ~(uintptr_t)3u);
    sce_paf_memset(m_ctrl->config.raw, 0, sizeof(m_ctrl->config.raw));
    m_ctrl->config.aac.isAdts = 0;
    m_ctrl->config.aac.sampleRate = m_pendingCfg.inputSampleRate;
    m_ctrl->config.aac.isSbr = m_pendingCfg.isSbrEnabled;

    if (SceAudiocodec_1F9A3722(m_ctrl, CODEC_TYPE_AAC) < 0)
        return -1;

    m_activeCfg = m_pendingCfg;
    return 0;
}

int Mp4Decoder::closeDecoder()
{
    return 0;
}

/* 0x81208502 */
int Mp4Decoder::decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf)
{
    if (inBuf == NULL || outBuf == NULL)
        return DEC_ERR_INVALID_ARG;

    if (inBuf->packetFlags == 4) {
        int silenceBytes = (int)(inBuf->frameDurationMs * 4.0f * (float)inBuf->sampleRate * 0.001f);
        outBuf->payloadSize = silenceBytes;
        sce_paf_memset(outBuf->frameData, 0, (SceSize)silenceBytes);
        outBuf->timestampMs = inBuf->timestampMs;
        outBuf->frameDurationMs = inBuf->frameDurationMs;
        outBuf->sampleRate = (int)m_pendingCfg.outputSampleRate;
        outBuf->channelConfig = inBuf->channelConfig;
        outBuf->packetFlags = inBuf->packetFlags;
        outBuf->discontinuity = inBuf->discontinuity;
        outBuf->leadingSkip = 0;
        outBuf->trailingSkip = 0;
        outBuf->channelCount = inBuf->channelCount;
        return 0;
    }

    if (inBuf->frameData == NULL || outBuf->frameData == NULL)
        return DEC_ERR_INVALID_ARG;

    m_lastPacketFlags = inBuf->packetFlags;
    if (inBuf->packetFlags == 2) {
        outBuf->payloadSize = 0;
        outBuf->timestampMs = inBuf->timestampMs;
        outBuf->frameDurationMs = inBuf->frameDurationMs;
        outBuf->sampleRate = (int)m_pendingCfg.outputSampleRate;
        outBuf->channelConfig = inBuf->channelConfig;
        outBuf->packetFlags = inBuf->packetFlags;
        outBuf->channelCount = inBuf->channelCount;
        return 0;
    }

    m_ctrl->inputBuf = inBuf->frameData;
    m_ctrl->outputBuf = outBuf->frameData;

    if (SceAudiocodec_E827E04F(m_ctrl, CODEC_TYPE_AAC) < 0) {
        if (m_consecutiveErrors >= 10 || SceAudiocodec_1F9A3722(m_ctrl, CODEC_TYPE_AAC) != 0) {
            outBuf->payloadSize = 0;
            outBuf->timestampMs = inBuf->timestampMs;
            outBuf->frameDurationMs = inBuf->frameDurationMs;
            outBuf->sampleRate = (int)m_pendingCfg.outputSampleRate;
            outBuf->channelConfig = 255;
            outBuf->packetFlags = 3;
            outBuf->leadingSkip = 0;
            outBuf->trailingSkip = 0;
            outBuf->channelCount = inBuf->channelCount;
            return DEC_ERR_CODEC_FAIL;
        }
        outBuf->payloadSize = 0;
        outBuf->timestampMs = inBuf->timestampMs;
        outBuf->frameDurationMs = inBuf->frameDurationMs;
        outBuf->sampleRate = (int)m_pendingCfg.outputSampleRate;
        outBuf->channelConfig = 255;
        outBuf->packetFlags = inBuf->packetFlags;
        outBuf->leadingSkip = 0;
        outBuf->trailingSkip = 0;
        outBuf->channelCount = inBuf->channelCount;
        ++m_consecutiveErrors;
        return 0;
    }

    int outBytes = m_ctrl->outputBytes;
    outBuf->payloadSize = outBytes;
    uint32_t outSampleRate = m_pendingCfg.outputSampleRate;
    uint8_t channels = inBuf->channelCount;

    if (m_ctrl->config.aac.isSbr != 0) {
        if (outBytes == ((int)channels << 11)) {
            outSampleRate >>= 1;
            m_pendingCfg.outputSampleRate = outSampleRate;
            m_ctrl->config.aac.isSbr = 0;
            m_activeCfg.isSbrEnabled = 0;
            m_pendingCfg.isSbrEnabled = 0;
            m_activeCfg.outputSampleRate = outSampleRate;
        }
    } else if (outBytes == ((int)channels << 12)) {
        outSampleRate *= 2u;
        m_pendingCfg.outputSampleRate = outSampleRate;
        m_ctrl->config.aac.isSbr = 1;
        m_activeCfg.isSbrEnabled = 1;
        m_pendingCfg.isSbrEnabled = 1;
        m_activeCfg.outputSampleRate = outSampleRate;
    }

    outBuf->timestampMs = inBuf->timestampMs;
    outBuf->frameDurationMs = inBuf->frameDurationMs;
    outBuf->sampleRate = (int)outSampleRate;
    outBuf->channelConfig = inBuf->channelConfig;
    outBuf->packetFlags = inBuf->packetFlags;
    outBuf->discontinuity = inBuf->discontinuity;
    outBuf->leadingSkip = inBuf->leadingSkip;
    outBuf->trailingSkip = inBuf->trailingSkip;
    outBuf->channelCount = channels;

    if (outBytes > 0) {
        uint8_t *pcmBytes = (uint8_t *)outBuf->frameData;
        if (inBuf->leadingSkip != 0) {
            int trimLead = 4 * (int)(inBuf->leadingSkip & 0x3FFFu);
            if (trimLead > outBytes)
                return DEC_ERR_TRIM_BOUNDS;
            int remain = (outBytes > trimLead) ? (outBytes - trimLead) : 0;
            if ((int)outBuf->maxBufferSize < remain)
                remain = (int)outBuf->maxBufferSize;
            sce_paf_memmove(pcmBytes, pcmBytes + trimLead, (SceSize)remain);
            outBytes -= trimLead;
            outBuf->payloadSize = outBytes;
        }
        if (inBuf->trailingSkip != 0) {
            int trimTrail = 4 * (int)(inBuf->trailingSkip & 0x3FFFu);
            if (trimTrail > outBytes)
                return DEC_ERR_TRIM_BOUNDS;
            outBuf->payloadSize = outBytes - trimTrail;
        }
    }

    if (m_consecutiveErrors != 0)
        m_consecutiveErrors = 0;
    return 0;
}

/* ===== At9Decoder (1196 bytes, 0x81208794 - 0x81208C33) ===== */

/* 0x81208794 */
At9Decoder::At9Decoder(const char *threadName)
    : AudioDecoderBase(2, threadName)
    , m_ctrl(NULL)
    , m_workMemPool(NULL)
    , m_parserErrorCode(0)
{
    uintptr_t aligned = ((uintptr_t)m_ctrlArena + 3u) & ~(uintptr_t)3u;
    m_ctrl = (AudiocodecCtrl *)aligned;
    sce_paf_memset(m_ctrl->config.raw, 0, sizeof(m_ctrl->config.raw));
    sce_paf_memset(m_at9ConfigBytes, 0, sizeof(m_at9ConfigBytes));
}

/* 0x812087E8 / 0x81208828 */
At9Decoder::~At9Decoder()
{
    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    m_ctrl = NULL;
}

/* 0x8120886C */
void At9Decoder::configureDecoder(const void *configData, uint32_t configSize)
{
    if (configData == NULL || configSize != sizeof(At9DecoderConfig)) {
        m_parserErrorCode = 0xFFFFFFFFu;
        sce_paf_memset(m_at9ConfigBytes, 0, sizeof(m_at9ConfigBytes));
        if (configData == NULL)
            return;
    }
    const At9DecoderConfig *cfg = (const At9DecoderConfig *)configData;
    m_parserErrorCode = cfg->parserErrorCode;
    uint32_t copyLen = (configSize > 4u) ? 4u : configSize;
    sce_paf_memcpy(m_at9ConfigBytes, cfg->configBytes, copyLen);
}

/* 0x812088AC */
int At9Decoder::openDecoder()
{
    if (!isAudioCodecModuleLoaded())
        return -1;

    if (m_parserErrorCode != 0) {
        switch (m_parserErrorCode) {
        case 0x8010A001u: return 17;
        case 0x8010A003u: return 20;
        case 0x8010A004u: return 19;
        case 0x80280022u:
        case 0x80280028u: return 95;
        case 0x80280029u: return 69;
        case 0x8028002Au: return 21;
        case 0x80280031u: return 81;
        default:          return mapAt9ParserError(m_parserErrorCode);
        }
    }

    if (m_ctrl == NULL)
        return DEC_ERR_NO_MEMORY;
    if (SceAudiocodec_E2AF3A54(m_ctrl, CODEC_TYPE_AT9) < 0)
        return DEC_ERR_CODEC_FAIL;

    if (m_workMemPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_workMemPool);
        m_workMemPool = NULL;
    }
    m_workMemPool = BufferPool::getInstance()->acquireTyped(2);
    if (m_workMemPool == NULL)
        return DEC_ERR_NO_MEMORY;

    m_ctrl->workMem = (void *)(((uintptr_t)m_workMemPool + 3u) & ~(uintptr_t)3u);
    sce_paf_memset(m_ctrl->config.raw, 0, sizeof(m_ctrl->config.raw));
    sce_paf_memcpy(m_ctrl->config.at9.configData, m_at9ConfigBytes, sizeof(m_at9ConfigBytes));
    return (SceAudiocodec_1F9A3722(m_ctrl, CODEC_TYPE_AT9) >= 0) ? 0 : -1;
}

int At9Decoder::closeDecoder()
{
    return 0;
}

/* 0x812089DC */
int At9Decoder::decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf)
{
    if (inBuf == NULL || outBuf == NULL || inBuf->frameData == NULL || outBuf->frameData == NULL)
        return DEC_ERR_INVALID_ARG;

    int16_t *channelPtrs[2] = { NULL, NULL };
    uint8_t channels = inBuf->channelCount;
    for (uint8_t ch = 0; ch < channels && ch < 2; ++ch)
        channelPtrs[ch] = m_planarPcm[ch];

    m_ctrl->outputBuf = channelPtrs;
    m_ctrl->inputBuf = inBuf->frameData;

    uint8_t *inBytes = (uint8_t *)inBuf->frameData;
    uint8_t *outBytesBase = (uint8_t *)outBuf->frameData;
    uint8_t *outLimit = outBytesBase + outBuf->maxBufferSize;
    int inputConsumed = 0;
    int outputOffset = 0;

    do {
        if (SceAudiocodec_E827E04F(m_ctrl, CODEC_TYPE_AT9) < 0) {
            outBuf->payloadSize = 0;
            outBuf->timestampMs = inBuf->timestampMs;
            outBuf->frameDurationMs = inBuf->frameDurationMs;
            outBuf->sampleRate = inBuf->sampleRate;
            outBuf->channelCount = inBuf->channelCount;
            outBuf->trimStartSample = inBuf->trimStartSample;
            outBuf->trimEndSample = inBuf->trimEndSample;
            return DEC_ERR_CODEC_FAIL;
        }

        int16_t *dstSample = (int16_t *)(outBytesBase + outputOffset);
        uint8_t chCount = inBuf->channelCount;
        for (int sampleIdx = 0; sampleIdx < 256; ++sampleIdx) {
            if ((uint8_t *)(dstSample + chCount) > outLimit)
                break;
            for (uint8_t ch = 0; ch < chCount; ++ch) {
                *dstSample++ = channelPtrs[ch][sampleIdx];
            }
        }

        inputConsumed += m_ctrl->inputUsedBytes;
        outputOffset += m_ctrl->outputBytes;
        if (inputConsumed < inBuf->payloadSize)
            m_ctrl->inputBuf = inBytes + inputConsumed;
    } while (inputConsumed < inBuf->payloadSize);

    uint32_t frameStride = 2u * (uint32_t)(inBuf->channelCount & 0x7Fu);
    uint32_t startBytes = inBuf->trimStartSample * frameStride;
    uint32_t endBytes = inBuf->trimEndSample * frameStride;
    int trimmedBytes = 0;
    if (endBytes > startBytes + frameStride)
        trimmedBytes = (int)(endBytes - startBytes + frameStride);
    if ((int)outBuf->maxBufferSize < trimmedBytes)
        trimmedBytes = (int)outBuf->maxBufferSize;

    if (startBytes != 0)
        sce_paf_memmove(outBytesBase, outBytesBase + startBytes, (SceSize)trimmedBytes);

    outBuf->payloadSize = trimmedBytes;
    outBuf->timestampMs = inBuf->timestampMs;
    if (outBuf->trimStartSample != 0) {
        outBuf->timestampMs = inBuf->timestampMs +
            ((float)inBuf->trimStartSample / (float)inBuf->sampleRate) * 1000.0f;
    }
    outBuf->frameDurationMs = inBuf->frameDurationMs;
    outBuf->sampleRate = inBuf->sampleRate;
    outBuf->channelCount = inBuf->channelCount;
    outBuf->trimStartSample = inBuf->trimStartSample;
    outBuf->trimEndSample = inBuf->trimEndSample;
    return 0;
}

/* ===== AudioOutputFilter ("SceShellMusicCoreAIF", 0x81208C34 - 0x8120960F) ===== */

/* 0x81209544 */
static int onAudioBufferConsumedStatic(void *pcmBuffer)
{
    if (g_activeAudioOutputFilter != NULL)
        return g_activeAudioOutputFilter->handleBufferConsumed(pcmBuffer);
    return 0;
}

/* 0x81208C34 */
AudioOutputFilter::AudioOutputFilter(uint32_t grainBytes, int slotCount, PipeProducerControl *producer,
                                     AudioOpenCallback openCb, AudioCloseCallback closeCb,
                                     AudioWriteCallback writeCb, AudioFlushCallback flushCb,
                                     void *userContext)
    : m_eventListener(NULL)
    , m_grainBytes(grainBytes)
    , m_slotCount(slotCount)
    , m_pcmPool(NULL)
    , m_slots(NULL)
    , m_silenceBuf(NULL)
    , m_readIndex(-1)
    , m_writeIndex(-1)
    , m_inflightAudioCount(0)
    , m_inflightSilenceCount(0)
    , m_producer(producer)
    , m_unusedConsumer(NULL)
    , m_posListener(NULL)
    , m_playPositionMs(-1)
    , m_mutexId(-1)
    , m_needsPrimeSilence(0)
    , m_openCb(openCb)
    , m_closeCb(closeCb)
    , m_writeCb(writeCb)
    , m_flushCb(flushCb)
    , m_userContext(userContext)
{
    SceKernelMutexOptParam optParam;
    sce_paf_memset(&optParam, 0, sizeof(optParam));
    optParam.size = sizeof(optParam);
    optParam.ceilingPriority = 0x100000E1;
    SceUID mutexId = sceKernelCreateMutex("SceShellMusicCoreAIF", 0, 0, &optParam);
    if (mutexId < 0)
        return;
    m_mutexId = mutexId;

    m_pcmPool = BufferPool::getInstance()->acquireTyped(3);
    m_silenceBuf = BufferPool::getInstance()->acquireTyped(4);
    m_slots = (MediaFrameBuffer *)BufferPool::getInstance()->acquireTyped(5);

    if (m_pcmPool == NULL || m_slots == NULL || m_silenceBuf == NULL) {
        if (m_pcmPool != NULL) {
            BufferPool::getInstance()->releaseTyped(m_pcmPool);
            m_pcmPool = NULL;
        }
        if (m_slots != NULL) {
            BufferPool::getInstance()->releaseTyped(m_slots);
            m_slots = NULL;
        }
        if (m_silenceBuf != NULL) {
            BufferPool::getInstance()->releaseTyped(m_silenceBuf);
            m_silenceBuf = NULL;
        }
        return;
    }

    uintptr_t poolBase = (uintptr_t)m_pcmPool;
    uintptr_t offset = 0;
    for (int idx = 0; idx < slotCount; ++idx) {
        MediaFrameBuffer *slot = &m_slots[idx];
        slot->frameData = (void *)((poolBase + offset + 1u) & ~(uintptr_t)1u);
        slot->maxBufferSize = grainBytes;
        slot->payloadSize = -1;
        slot->timestampMs = -1.0f;
        slot->frameDurationMs = -1.0f;
        slot->sampleRate = -1;
        slot->packetFlags = 255;
        offset += grainBytes + 2u;
    }

    sce_paf_memset(m_silenceBuf, 0, SILENCE_GRAIN_BYTES);
    m_readIndex = -1;
    m_writeIndex = 0;
}

/* 0x81208DFC / 0x81208E5E */
AudioOutputFilter::~AudioOutputFilter()
{
    if (m_pcmPool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_pcmPool);
        m_pcmPool = NULL;
    }
    if (m_slots != NULL) {
        BufferPool::getInstance()->releaseTyped(m_slots);
        m_slots = NULL;
    }
    if (m_silenceBuf != NULL) {
        BufferPool::getInstance()->releaseTyped(m_silenceBuf);
        m_silenceBuf = NULL;
    }
    if (m_mutexId != -1) {
        sceKernelDeleteMutex(m_mutexId);
        m_mutexId = -1;
    }
}

void AudioOutputFilter::setEventListener(PipelineEventListener *listener)
{
    m_eventListener = listener;
}

/* 0x81208EC6 */
bool AudioOutputFilter::isInitialized() const
{
    return m_mutexId != -1 && m_pcmPool != NULL && m_slots != NULL;
}

/* 0x81208EDE */
int AudioOutputFilter::start()
{
    g_activeAudioOutputFilter = this;
    m_needsPrimeSilence = 1;
    if (m_openCb == NULL) {
        sce_paf_printf_dummy();
        return -1;
    }
    int ret = m_openCb(m_userContext);
    if (ret == 0)
        return 0;
    return (ret == MC_ERR_NO_MEMORY) ? DEC_ERR_NO_MEMORY : -1;
}

/* 0x81208F1E */
int AudioOutputFilter::stop()
{
    if (m_closeCb == NULL) {
        sce_paf_printf_dummy();
        return -1;
    }
    if (m_closeCb(m_userContext) != 0)
        return -1;
    m_needsPrimeSilence = 0;
    g_activeAudioOutputFilter = NULL;
    return 0;
}

/* 0x81208F50 */
void AudioOutputFilter::setPositionListener(PositionChangeListener *listener)
{
    m_posListener = listener;
}

int AudioOutputFilter::getPlayPositionMs() const
{
    return m_playPositionMs;
}

/* 0x81208F54 */
void AudioOutputFilter::setPlayPositionMs(int posMs)
{
    m_playPositionMs = posMs;
}

/* 0x81208F58 */
void AudioOutputFilter::reset()
{
    sceKernelLockMutex(m_mutexId, 1, NULL);
    m_readIndex = -1;
    m_writeIndex = 0;
    m_playPositionMs = -1;
    sceKernelUnlockMutex(m_mutexId, 1);

    if (m_flushCb == NULL) {
        sce_paf_printf_dummy();
        return;
    }
    m_flushCb(m_userContext);

    sceKernelLockMutex(m_mutexId, 1, NULL);
    m_inflightAudioCount = 0;
    m_inflightSilenceCount = 0;
    sceKernelUnlockMutex(m_mutexId, 1);
}

/* 0x81208FA4 */
bool AudioOutputFilter::hasQueuedFrame() const
{
    return m_readIndex >= 0;
}

bool AudioOutputFilter::hasReadableFrame() const
{
    return m_readIndex >= 0;
}

/* 0x81208FAE */
int AudioOutputFilter::acquireWriteSlot(MediaFrameBuffer **outBuf)
{
    if (m_pcmPool == NULL || m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL) {
        *outBuf = NULL;
        return -1;
    }

    sceKernelLockMutex(m_mutexId, 1, NULL);
    if (m_writeIndex < 0 || (m_inflightSilenceCount + m_inflightAudioCount) >= MAX_INFLIGHT_AUDIO_BUFS) {
        sceKernelUnlockMutex(m_mutexId, 1);
        *outBuf = NULL;
        return PIPE_ERR_FULL;
    }
    if (m_writeIndex >= m_slotCount) {
        sceKernelUnlockMutex(m_mutexId, 1);
        *outBuf = NULL;
        return -1;
    }

    *outBuf = &m_slots[m_writeIndex];
    sceKernelUnlockMutex(m_mutexId, 1);
    return 0;
}

/* 0x81209034 */
int AudioOutputFilter::commitWriteSlot(MediaFrameBuffer *buf)
{
    if (buf == NULL)
        return DEC_ERR_INVALID_ARG;
    if (m_pcmPool == NULL || m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_writeIndex >= m_slotCount)
        return -1;
    if (m_writeCb == NULL) {
        sce_paf_printf_dummy();
        return -1;
    }

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return DEC_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return DEC_ERR_INVALID_ARG;

    sceKernelLockMutex(m_mutexId, 1, NULL);
    if (slotIdx != m_writeIndex) {
        sceKernelUnlockMutex(m_mutexId, 1);
        return PIPE_ERR_OUT_OF_ORDER;
    }

    bool shouldPrimeSilence = false;
    int nextWrite = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextWrite == m_readIndex) {
        m_writeIndex = -1;
    } else {
        m_writeIndex = nextWrite;
        if (m_readIndex < 0) {
            m_readIndex = slotIdx;
            shouldPrimeSilence = (buf->payloadSize <= 0x2000);
        }
    }
    sceKernelUnlockMutex(m_mutexId, 1);

    int sampleRate = buf->sampleRate;
    int payloadBytes = buf->payloadSize;
    int channels = (int)buf->channelCount;
    void *pcmBuf = buf->frameData;

    if (shouldPrimeSilence) {
        m_needsPrimeSilence = 1;
        int primeTs = (m_playPositionMs >= 0) ? m_playPositionMs : (int)buf->timestampMs;
        int primeRet = m_writeCb(m_userContext, primeTs, channels, sampleRate,
                                 (int)SILENCE_GRAIN_BYTES, m_silenceBuf, onAudioBufferConsumedStatic);
        if (primeRet == 0) {
            sceKernelLockMutex(m_mutexId, 1, NULL);
            ++m_inflightSilenceCount;
            sceKernelUnlockMutex(m_mutexId, 1);
        } else if (primeRet != MC_ERR_AUDIO_DROPPED) {
            return -1;
        }
    } else {
        m_needsPrimeSilence = 0;
    }

    int writeTs = (buf->timestampMs >= 0.0f) ? (int)buf->timestampMs : m_playPositionMs;
    int writeRet = m_writeCb(m_userContext, writeTs, channels, sampleRate,
                             payloadBytes, pcmBuf, onAudioBufferConsumedStatic);
    if (writeRet == 0) {
        sceKernelLockMutex(m_mutexId, 1, NULL);
        ++m_inflightAudioCount;
        sceKernelUnlockMutex(m_mutexId, 1);
    } else if (writeRet != MC_ERR_AUDIO_DROPPED) {
        return -1;
    }

    if (m_posListener != NULL) {
        if (m_playPositionMs < 0 || (m_playPositionMs / 1000) != ((int)buf->timestampMs / 1000)) {
            m_posListener->onPositionSecondChanged((int)buf->timestampMs);
        }
    }
    m_playPositionMs = (int)buf->timestampMs;
    return 0;
}

/* 0x81209328 */
int AudioOutputFilter::acquireReadSlot(MediaFrameBuffer **outBuf)
{
    if (m_pcmPool == NULL || m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL)
        return -1;

    sceKernelLockMutex(m_mutexId, 1, NULL);
    if (m_readIndex < 0) {
        sceKernelUnlockMutex(m_mutexId, 1);
        *outBuf = NULL;
        return PIPE_ERR_EMPTY;
    }
    if (m_readIndex >= m_slotCount) {
        sceKernelUnlockMutex(m_mutexId, 1);
        *outBuf = NULL;
        m_producer->onPipeUnderflow();
        return -1;
    }

    *outBuf = &m_slots[m_readIndex];
    sceKernelUnlockMutex(m_mutexId, 1);
    return 0;
}

/* 0x8120939E */
int AudioOutputFilter::releaseReadSlot(MediaFrameBuffer *buf)
{
    if (m_pcmPool == NULL || m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_readIndex >= m_slotCount)
        return -1;

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return DEC_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return DEC_ERR_INVALID_ARG;

    sceKernelLockMutex(m_mutexId, 1, NULL);
    if (slotIdx != m_readIndex) {
        sceKernelUnlockMutex(m_mutexId, 1);
        return PIPE_ERR_OUT_OF_ORDER;
    }

    int nextRead = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextRead == m_writeIndex) {
        m_needsPrimeSilence = 1;
        m_readIndex = -1;
    } else {
        m_readIndex = nextRead;
        if (m_writeIndex < 0)
            m_writeIndex = slotIdx;
    }

    --m_inflightAudioCount;
    if (m_readIndex < 0 && m_eventListener != NULL)
        m_eventListener->onFilterEvent(3, static_cast<FilterBase *>(this), 0);

    int frameFlags = buf->frameFlags;
    sceKernelUnlockMutex(m_mutexId, 1);

    m_producer->onPipeConsumed(frameFlags);
    return 0;
}

/* 0x812094B0 */
int AudioOutputFilter::handleBufferConsumed(void *pcmBuffer)
{
    sceKernelLockMutex(m_mutexId, 1, NULL);
    if (m_silenceBuf == pcmBuffer) {
        --m_inflightSilenceCount;
        sceKernelUnlockMutex(m_mutexId, 1);
        m_producer->onPipeConsumed(0);
        return 0;
    }
    if (m_readIndex < 0) {
        sceKernelUnlockMutex(m_mutexId, 1);
        return 0;
    }

    int readIdx = m_readIndex;
    if (m_slots[readIdx].frameData != pcmBuffer) {
        sceKernelUnlockMutex(m_mutexId, 1);
        return MC_ERR_INTERNAL;
    }

    sceKernelUnlockMutex(m_mutexId, 1);
    return (releaseReadSlot(&m_slots[readIdx]) != 0) ? MC_ERR_INTERNAL : 0;
}

/* ===== BufRefPipe ("SceShellMusicCoreBufRefPipe", 44 bytes, 0x81209610 - 0x81209AA7) ===== */

/* 0x81209610 */
BufRefPipe::BufRefPipe(int slotCount, PipeProducerControl *producer, PipeConsumerControl *consumer)
    : m_slotCount(slotCount)
    , m_slots(NULL)
    , m_readIndex(-1)
    , m_writeIndex(-1)
    , m_producer(producer)
    , m_consumer(consumer)
    , m_posListener(NULL)
    , m_playPositionMs(-1)
    , m_lock("SceShellMusicCoreBufRefPipe")
{
    m_slots = (MediaFrameBuffer *)ScePafStdc_3CF19248(sizeof(MediaFrameBuffer) * (SceSize)slotCount,
                                                      getLocalHeap());
    if (m_slots == NULL)
        return;

    for (int idx = 0; idx < slotCount; ++idx) {
        MediaFrameBuffer *slot = &m_slots[idx];
        slot->frameData = NULL;
        slot->maxBufferSize = 0;
        slot->payloadSize = -1;
        slot->timestampMs = -1.0f;
        slot->frameDurationMs = -1.0f;
        slot->sampleRate = -1;
        slot->packetFlags = 255;
        slot->discontinuity = 0;
    }

    m_readIndex = -1;
    m_writeIndex = 0;
}

/* 0x812096AA / 0x812096D4 */
BufRefPipe::~BufRefPipe()
{
    if (m_slots != NULL) {
        delete[] m_slots;
        m_slots = NULL;
    }
}

/* 0x81209708 */
bool BufRefPipe::isInitialized() const
{
    return m_lock.isValid() && m_slots != NULL;
}

/* 0x81209728 */
void BufRefPipe::reset()
{
    m_lock.lock();
    if (m_readIndex >= 0 && m_slots != NULL && m_producer != NULL) {
        int cursor = m_readIndex;
        for (;;) {
            m_producer->onPipeConsumed(m_slots[cursor].frameFlags);
            int nextIdx = (cursor + 1) % m_slotCount;
            if (nextIdx == m_readIndex || nextIdx == m_writeIndex)
                break;
            cursor = nextIdx;
        }
    }
    m_readIndex = -1;
    m_writeIndex = 0;
    m_playPositionMs = -1;
    m_lock.unlock();
}

/* 0x812097A0 */
int BufRefPipe::acquireWriteSlot(MediaFrameBuffer **outBuf)
{
    if (m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL) {
        *outBuf = NULL;
        return -1;
    }

    m_lock.lock();
    if (m_writeIndex < 0) {
        m_lock.unlock();
        *outBuf = NULL;
        m_consumer->onPipeFull();
        return PIPE_ERR_FULL;
    }
    if (m_writeIndex >= m_slotCount) {
        m_lock.unlock();
        *outBuf = NULL;
        return -1;
    }

    *outBuf = &m_slots[m_writeIndex];
    m_lock.unlock();
    return 0;
}

/* 0x81209814 */
int BufRefPipe::commitWriteSlot(MediaFrameBuffer *buf)
{
    if (buf == NULL)
        return DEC_ERR_INVALID_ARG;
    if (m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL || m_writeIndex >= m_slotCount)
        return -1;

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return DEC_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return DEC_ERR_INVALID_ARG;

    m_lock.lock();
    if (slotIdx != m_writeIndex) {
        m_lock.unlock();
        return PIPE_ERR_OUT_OF_ORDER;
    }

    int nextWrite = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextWrite == m_readIndex) {
        m_writeIndex = -1;
    } else {
        m_writeIndex = nextWrite;
        if (m_readIndex < 0)
            m_readIndex = slotIdx;
    }
    m_lock.unlock();

    m_consumer->onPipeProduced();
    if (m_posListener != NULL) {
        if (m_playPositionMs < 0 ||
            (float)(m_playPositionMs / 1000) != (buf->timestampMs / 1000.0f)) {
            m_posListener->onPositionSecondChanged((int)buf->timestampMs);
        }
    }
    m_playPositionMs = (int)buf->timestampMs;
    return 0;
}

/* 0x81209960 */
int BufRefPipe::acquireReadSlot(MediaFrameBuffer **outBuf)
{
    if (m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL) {
        *outBuf = NULL;
        return -1;
    }

    m_lock.lock();
    if (m_readIndex < 0) {
        m_lock.unlock();
        *outBuf = NULL;
        return PIPE_ERR_EMPTY;
    }
    if (m_readIndex >= m_slotCount) {
        m_lock.unlock();
        *outBuf = NULL;
        m_producer->onPipeUnderflow();
        return -1;
    }

    *outBuf = &m_slots[m_readIndex];
    m_lock.unlock();
    return 0;
}

/* 0x812099D0 */
int BufRefPipe::releaseReadSlot(MediaFrameBuffer *buf)
{
    if (m_slots == NULL)
        return DEC_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL || m_readIndex >= m_slotCount)
        return -1;

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return DEC_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return DEC_ERR_INVALID_ARG;

    m_lock.lock();
    if (slotIdx != m_readIndex) {
        m_lock.unlock();
        return PIPE_ERR_OUT_OF_ORDER;
    }

    int nextRead = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextRead == m_writeIndex) {
        m_readIndex = -1;
    } else {
        m_readIndex = nextRead;
        if (m_writeIndex < 0)
            m_writeIndex = slotIdx;
    }
    int frameFlags = buf->frameFlags;
    m_lock.unlock();

    m_producer->onPipeConsumed(frameFlags);
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
