/*
 * SceShell musiccore_plugin - WavMediaInfo, WavCacheIo, At9MediaInfo, and
 * At9CacheIo (RIFF/WAVE PCM and ATRAC9 metadata and chunk streaming).
 * Original address range (3.65-CEX): 0x811EDBD4 - 0x811EEC3F
 */

#include "musiccore_heap.h"
#include "musiccore_media.h"
#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
int sce_paf_memcmp(const void *lhs, const void *rhs, SceSize len);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize len);
}

namespace sce {
namespace MusicCore {

extern void *allocFromHeap(size_t size);

static const uint32_t FOURCC_WAV = 0x20574156u; /* ' WAV' */
static const uint32_t FOURCC_AT9 = 0x20415439u; /* ' AT9' */

static const int IO_ERR_OPEN_FAIL = -2146394109;    /* 0x8010A003 */
static const int IO_ERR_READ_FAIL = -2146394108;    /* 0x8010A004 */
static const int IO_ERR_NOT_OPEN = -2146394107;     /* 0x8010A005 */
static const int IO_ERR_BAD_RANGE = -2146390031;    /* 0x8010AFF1 */
static const int WAVE_ERR_UNSUPPORTED = -2144862159;/* 0x80280031 */
static const int WAVE_ERR_CORRUPT = -2144862158;    /* 0x80280032 */
static const int WAVE_ERR_NO_DATA = -2144862157;    /* 0x80280033 */

static const uint8_t CHUNK_MASK_FMT = 0x01u;
static const uint8_t CHUNK_MASK_DATA = 0x02u;
static const uint8_t CHUNK_MASK_INFO = 0x04u;
static const uint8_t CHUNK_MASK_SMPL = 0x05u;

static const uint8_t AT9_SUBFORMAT_GUID[16] = {
    0xD2u, 0x42u, 0xE1u, 0x47u, 0xBAu, 0x36u, 0x8Du, 0x4Du,
    0x88u, 0xFCu, 0x61u, 0x65u, 0x4Fu, 0x8Cu, 0x83u, 0x6Cu
};

static uint16_t readLe16(const uint8_t *src)
{
    return (uint16_t)(src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t readLe32(const uint8_t *src)
{
    return (uint32_t)src[0]
         | ((uint32_t)src[1] << 8)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 24);
}

static int mapWaveMetadataError(int errCode)
{
    switch (errCode) {
    case WAVE_ERR_CORRUPT:
        return 95;
    case WAVE_ERR_UNSUPPORTED:
        return 81;
    case IO_ERR_READ_FAIL:
        return 19;
    case IO_ERR_OPEN_FAIL:
        return 20;
    case 0:
        return 0;
    default:
        return -1;
    }
}

static void clearMetadataValidityFlags(TrackMetadataResult *meta)
{
    meta->hasChannels = 0;
    meta->hasBitRate = 0;
    meta->hasDuration = 0;
    meta->hasSampleRate = 0;
    meta->hasCodecProfile = 0;
    meta->hasCodecSubType = 0;
}

/* 0x811EDE90 */
WavCacheIo::WavCacheIo(const char *path, int chunkSize, int blockCount, int readAhead,
                       int extraChunk, int extraBlocks)
    : LocalCacheIo(path, chunkSize, blockCount, (uint8_t)readAhead, extraChunk, extraBlocks)
    , m_foundChunks(0)
    , m_fmtOffset(0)
    , m_fmtSize(0)
    , m_dataOffset(0)
    , m_dataSize(0)
    , m_infoOffset(0)
    , m_infoSize(0)
{
}

/* 0x811EDED0 / 0x811EDEEC */
WavCacheIo::~WavCacheIo()
{
}

/* 0x811EDF0C */
int WavCacheIo::setPath(const char *path)
{
    int ret = LocalCacheIo::setPath(path);
    if (ret >= 0) {
        m_foundChunks = 0;
        m_fmtOffset = 0;
        m_fmtSize = 0;
        m_dataOffset = 0;
        m_dataSize = 0;
        m_infoOffset = 0;
        m_infoSize = 0;
    }
    return ret;
}

/* 0x811EDF2E */
int WavCacheIo::scanChunks(uint8_t targetMask)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }

    if ((uint8_t)(m_foundChunks & targetMask) == targetMask) {
        return 0;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    int rawSize = getFileSize();
    if (rawSize < 0) {
        return -1;
    }

    uint32_t fileSize = (uint32_t)rawSize;
    if (fileSize <= 4u) {
        return WAVE_ERR_CORRUPT;
    }

    const char *riffTag = (const char *)mapRange(0u, 4u, 0, &mappedBytes, &ioErr, 0);
    if (riffTag == NULL) {
        return ioErr;
    }
    if (sce_paf_strncmp(riffTag, "RIFF", 4u) != 0) {
        return WAVE_ERR_CORRUPT;
    }
    if (fileSize <= 12u) {
        return WAVE_ERR_CORRUPT;
    }

    const char *waveTag = (const char *)mapRange(8u, 4u, 0, &mappedBytes, &ioErr, 0);
    if (waveTag == NULL) {
        return ioErr;
    }
    if (sce_paf_strncmp(waveTag, "WAVE", 4u) != 0) {
        return WAVE_ERR_CORRUPT;
    }

    uint32_t cursor = 12u;
    do {
        uint32_t chunkStart = cursor;
        if ((uint8_t)(m_foundChunks & targetMask) == targetMask || fileSize <= cursor + 4u) {
            break;
        }

        const char *chunkId = (const char *)mapRange(cursor, 4u, 0, &mappedBytes, &ioErr, 0);
        if (chunkId == NULL) {
            return ioErr;
        }

        uint32_t payloadOffset = cursor + 8u;
        if (fileSize <= payloadOffset) {
            return 0;
        }

        const uint8_t *sizeBytes = (const uint8_t *)mapRange(cursor + 4u, 4u, 0, &mappedBytes, &ioErr, 0);
        if (sizeBytes == NULL) {
            return ioErr;
        }

        uint32_t chunkSize = readLe32(sizeBytes);
        cursor = payloadOffset;
        if (chunkSize != 0u) {
            if (fileSize < payloadOffset + chunkSize) {
                chunkSize = fileSize - payloadOffset;
            }

            if (sce_paf_strncmp(chunkId, "fmt ", 4u) == 0) {
                m_fmtOffset = payloadOffset;
                m_fmtSize = chunkSize;
                m_foundChunks |= CHUNK_MASK_FMT;
            } else if (sce_paf_strncmp(chunkId, "data", 4u) == 0) {
                m_dataOffset = payloadOffset;
                m_dataSize = chunkSize;
                m_foundChunks |= CHUNK_MASK_DATA;
            } else if (sce_paf_strncmp(chunkId, "LIST", 4u) == 0) {
                if (fileSize <= chunkStart + 12u) {
                    return 0;
                }
                const char *listType = (const char *)mapRange(payloadOffset, 4u, 0, &mappedBytes, &ioErr, 0);
                if (listType == NULL) {
                    return ioErr;
                }
                if (sce_paf_strncmp(listType, "INFO", 4u) == 0) {
                    m_infoOffset = chunkStart + 12u;
                    m_infoSize = chunkSize - 4u;
                    m_foundChunks |= CHUNK_MASK_INFO;
                }
            }

            uint32_t advance = chunkSize;
            if ((chunkSize & 1u) != 0u) {
                advance = chunkSize + 1u;
            }
            cursor = payloadOffset + advance;
        }
    } while (cursor < fileSize);

    return 0;
}

/* 0x811EE18C */
int WavCacheIo::readFmtHeader(WavFmtHeader *outFmt)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }
    if (m_fmtSize < 16u) {
        return WAVE_ERR_CORRUPT;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    const uint8_t *raw = (const uint8_t *)mapRange(m_fmtOffset, 16u, 0, &mappedBytes, &ioErr, 0);
    if (raw == NULL) {
        return ioErr;
    }

    outFmt->formatTag = readLe16(raw + 0);
    outFmt->channels = readLe16(raw + 2);
    outFmt->sampleRate = readLe32(raw + 4);
    outFmt->avgBytesPerSec = readLe32(raw + 8);
    outFmt->blockAlign = readLe16(raw + 12);
    outFmt->bitsPerSample = readLe16(raw + 14);
    return 0;
}

/* 0x811EE25C */
int WavCacheIo::readDataRange(uint32_t offset, uint32_t size, void **outPtr, uint32_t *outSize)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }
    if (m_dataSize == 0u) {
        return WAVE_ERR_NO_DATA;
    }
    if (offset >= m_dataSize || size == 0u) {
        return IO_ERR_BAD_RANGE;
    }

    uint32_t readLen = size;
    if (m_dataSize < offset + readLen) {
        readLen = m_dataSize - offset;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    void *mapped = mapRange(m_dataOffset + offset, readLen, 0, &mappedBytes, &ioErr, 0);
    *outPtr = mapped;
    if (mapped == NULL) {
        return ioErr;
    }

    *outSize = readLen;
    return 0;
}

/* 0x811EDBD4 */
WavMediaInfo::WavMediaInfo(const char *path)
    : MediaInfoBase(path, FOURCC_WAV)
    , m_cacheIo(NULL)
{
    void *mem = localAlloc(sizeof(WavCacheIo));
    if (mem != NULL) {
        m_cacheIo = new (mem) WavCacheIo(m_path, 0x10000, 8, 1, 0, 0);
    }
    if (m_cacheIo != NULL) {
        m_cacheIo->isInitialized();
    }
}

/* 0x811EDC38 / 0x811EDC68 */
WavMediaInfo::~WavMediaInfo()
{
    if (m_cacheIo != NULL) {
        delete m_cacheIo;
        m_cacheIo = NULL;
    }
}

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

/* 0x811EDC9C */
int WavMediaInfo::parseMetadata(TrackMetadataResult *outMeta)
{
    clearMetadataValidityFlags(outMeta);
    if (m_cacheIo == NULL) {
        return 33;
    }

    if (m_cacheIo->isOpen() != 1 && m_cacheIo->open() != 0) {
        return 17;
    }

    WavFmtHeader fmt;
    fmt.formatTag = 0;
    fmt.channels = 0;
    fmt.sampleRate = 0;
    fmt.avgBytesPerSec = 0;
    fmt.blockAlign = 0;
    fmt.bitsPerSample = 0;

    int err = m_cacheIo->scanChunks(CHUNK_MASK_FMT | CHUNK_MASK_DATA);
    if (err == 0) {
        err = m_cacheIo->readFmtHeader(&fmt);
        if (err == 0) {
            if (fmt.avgBytesPerSec == 0u) {
                err = WAVE_ERR_CORRUPT;
            } else {
                float bytesPerMs = (float)fmt.avgBytesPerSec * 0.001f;
                if (fmt.formatTag == 1u
                    && isSupportedWavSampleRate(fmt.sampleRate)
                    && isSupportedWavPcmLayout(fmt.channels, fmt.blockAlign, fmt.bitsPerSample)) {
                    outMeta->sampleRate = (int)fmt.sampleRate;
                    outMeta->codecProfile = 14;
                    outMeta->channels = (int)fmt.channels;
                    outMeta->bitRate = (int)(fmt.sampleRate * fmt.bitsPerSample * fmt.channels);
                    outMeta->durationMs = (unsigned int)(int)((float)m_cacheIo->getDataSize() / bytesPerMs);
                    outMeta->codecSubType = 10;
                    outMeta->hasChannels = 1;
                    outMeta->hasBitRate = 1;
                    outMeta->hasDuration = 1;
                    outMeta->hasSampleRate = 1;
                    outMeta->hasCodecProfile = 1;
                    outMeta->hasCodecSubType = 1;
                    err = 0;
                } else {
                    err = WAVE_ERR_UNSUPPORTED;
                }
            }
        }
    }

    int status = mapWaveMetadataError(err);
    if (status == 0) {
        return 0;
    }

    clearMetadataValidityFlags(outMeta);
    return status;
}

/* 0x811EE6DC */
At9CacheIo::At9CacheIo(const char *path, int chunkSize, int blockCount, int readAhead,
                       int extraChunk, int extraBlocks)
    : LocalCacheIo(path, chunkSize, blockCount, (uint8_t)readAhead, extraChunk, extraBlocks)
    , m_foundChunks(0)
    , m_fmtOffset(0)
    , m_fmtSize(0)
    , m_dataOffset(0)
    , m_dataSize(0)
    , m_infoOffset(0)
    , m_infoSize(0)
    , m_smplOffset(0)
    , m_smplSize(0)
{
}

/* 0x811EE720 / 0x811EE74C */
At9CacheIo::~At9CacheIo()
{
    if (isOpen() != 0) {
        close();
    }
}

/* 0x811EE77C */
int At9CacheIo::setPath(const char *path)
{
    int ret = LocalCacheIo::setPath(path);
    if (ret >= 0) {
        m_foundChunks = 0;
        m_fmtOffset = 0;
        m_fmtSize = 0;
        m_dataOffset = 0;
        m_dataSize = 0;
        m_infoOffset = 0;
        m_infoSize = 0;
        m_smplOffset = 0;
        m_smplSize = 0;
    }
    return ret;
}

/* 0x811EE7A2 */
int At9CacheIo::scanChunks(uint8_t targetMask)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }

    if ((uint8_t)(m_foundChunks & targetMask) == targetMask) {
        return 0;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    int rawSize = getFileSize();
    if (rawSize < 0) {
        return -1;
    }

    uint32_t fileSize = (uint32_t)rawSize;
    if (fileSize <= 4u) {
        return WAVE_ERR_CORRUPT;
    }

    const char *riffTag = (const char *)mapRange(0u, 4u, 0, &mappedBytes, &ioErr, 0);
    if (riffTag == NULL) {
        return ioErr;
    }
    if (sce_paf_strncmp(riffTag, "RIFF", 4u) != 0) {
        return WAVE_ERR_CORRUPT;
    }
    if (fileSize <= 12u) {
        return WAVE_ERR_CORRUPT;
    }

    const char *waveTag = (const char *)mapRange(8u, 4u, 0, &mappedBytes, &ioErr, 0);
    if (waveTag == NULL) {
        return ioErr;
    }
    if (sce_paf_strncmp(waveTag, "WAVE", 4u) != 0) {
        return WAVE_ERR_CORRUPT;
    }

    uint32_t cursor = 12u;
    do {
        uint32_t chunkStart = cursor;
        if ((uint8_t)(m_foundChunks & targetMask) == targetMask || fileSize <= cursor + 4u) {
            break;
        }

        const char *chunkId = (const char *)mapRange(cursor, 4u, 0, &mappedBytes, &ioErr, 0);
        if (chunkId == NULL) {
            return ioErr;
        }

        if (fileSize <= cursor + 8u) {
            return 0;
        }

        const uint8_t *sizeBytes = (const uint8_t *)mapRange(cursor + 4u, 4u, 0, &mappedBytes, &ioErr, 0);
        if (sizeBytes == NULL) {
            return ioErr;
        }

        cursor += 8u;
        uint32_t chunkSize = readLe32(sizeBytes);
        if (chunkSize != 0u) {
            if (fileSize < cursor + chunkSize) {
                chunkSize = fileSize - cursor;
            }

            if (sce_paf_strncmp(chunkId, "fmt ", 4u) == 0) {
                m_fmtOffset = cursor;
                m_fmtSize = chunkSize;
                m_foundChunks |= CHUNK_MASK_FMT;
            } else if (sce_paf_strncmp(chunkId, "data", 4u) == 0) {
                m_dataOffset = cursor;
                m_dataSize = chunkSize;
                m_foundChunks |= CHUNK_MASK_DATA;
            } else if (sce_paf_strncmp(chunkId, "LIST", 4u) == 0) {
                if (fileSize <= chunkStart + 12u) {
                    return 0;
                }
                const char *listType = (const char *)mapRange(cursor, 4u, 0, &mappedBytes, &ioErr, 0);
                if (listType == NULL) {
                    return ioErr;
                }
                if (sce_paf_strncmp(listType, "INFO", 4u) == 0) {
                    m_infoOffset = chunkStart + 12u;
                    m_infoSize = chunkSize - 4u;
                    m_foundChunks |= CHUNK_MASK_INFO;
                }
            } else if (sce_paf_strncmp(chunkId, "smpl", 4u) == 0) {
                m_smplOffset = cursor;
                m_smplSize = chunkSize;
                m_foundChunks |= CHUNK_MASK_SMPL;
            }

            uint32_t advance = chunkSize;
            if ((chunkSize & 1u) != 0u) {
                advance = chunkSize + 1u;
            }
            cursor += advance;
        }
    } while (cursor < fileSize);

    return 0;
}

/* 0x811EEA3C */
int At9CacheIo::readFmtHeader(At9FmtHeader *outFmt)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }
    if (m_fmtSize < 52u) {
        return WAVE_ERR_CORRUPT;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    const uint8_t *raw = (const uint8_t *)mapRange(m_fmtOffset, 52u, 0, &mappedBytes, &ioErr, 0);
    if (raw == NULL) {
        return ioErr;
    }

    outFmt->formatTag = readLe16(raw + 0);
    outFmt->channels = readLe16(raw + 2);
    outFmt->sampleRate = readLe32(raw + 4);
    outFmt->avgBytesPerSec = readLe32(raw + 8);
    outFmt->blockAlign = readLe16(raw + 12);
    outFmt->cbSize = readLe16(raw + 16);
    outFmt->samplesPerBlock = readLe16(raw + 18);
    outFmt->channelMask = readLe32(raw + 20);
    sce_paf_memcpy(outFmt->subFormatGuid, raw + 24, 16u);
    outFmt->versionInfo = readLe32(raw + 40);
    sce_paf_memcpy(outFmt->configData, raw + 44, 4u);
    return 0;
}

/* 0x811EEB64 */
int At9CacheIo::readSmplLoop(At9SmplLoopInfo *outLoop)
{
    if (isOpen() == 0) {
        return IO_ERR_NOT_OPEN;
    }
    if (m_smplSize == 0u) {
        return 0;
    }
    if (m_smplSize < 60u) {
        return WAVE_ERR_CORRUPT;
    }

    int mappedBytes = 0;
    int ioErr = 0;
    const uint8_t *raw = (const uint8_t *)mapRange(m_smplOffset, 60u, 0, &mappedBytes, &ioErr, 0);
    if (raw == NULL) {
        return ioErr;
    }

    uint32_t rawLoopStart = readLe32(raw + 44);
    uint32_t rawLoopEnd = readLe32(raw + 48);

    outLoop->loopStart = (rawLoopStart != 256u) ? (rawLoopStart - 257u) : 0u;
    outLoop->loopEnd = (rawLoopEnd != 256u) ? (rawLoopEnd - 257u) : 0u;
    return 0;
}

/* 0x811EE30C */
At9MediaInfo::At9MediaInfo(const char *path, int loopMode)
    : MediaInfoBase(path, FOURCC_AT9)
    , m_loopMode(loopMode)
    , m_cacheIo(NULL)
{
    void *mem = localAlloc(sizeof(At9CacheIo));
    if (mem != NULL) {
        m_cacheIo = new (mem) At9CacheIo(m_path, 0x4000, 32, 1, 0, 0);
    }
    if (m_cacheIo != NULL) {
        m_cacheIo->isInitialized();
    }
}

/* 0x811EE378 / 0x811EE3A8 */
At9MediaInfo::~At9MediaInfo()
{
    if (m_cacheIo != NULL) {
        delete m_cacheIo;
        m_cacheIo = NULL;
    }
}

static bool lookupAt9BitrateKbps(uint16_t frameBytes, int *outKbps)
{
    switch (frameBytes) {
    case 40:
        *outKbps = 60;
        return true;
    case 48:
        *outKbps = 72;
        return true;
    case 56:
        *outKbps = 84;
        return true;
    case 64:
        *outKbps = 96;
        return true;
    case 80:
        *outKbps = 120;
        return true;
    case 96:
        *outKbps = 144;
        return true;
    case 112:
        *outKbps = 168;
        return true;
    case 128:
        *outKbps = 192;
        return true;
    default:
        return false;
    }
}

/* 0x811EE3DC */
int At9MediaInfo::parseMetadata(TrackMetadataResult *outMeta, int loopCount)
{
    clearMetadataValidityFlags(outMeta);
    if (m_cacheIo == NULL) {
        return 33;
    }

    if (m_cacheIo->isOpen() != 1 && m_cacheIo->open() != 0) {
        return 17;
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

    int err = m_cacheIo->scanChunks(CHUNK_MASK_FMT | CHUNK_MASK_DATA);
    if (err == 0) {
        err = m_cacheIo->readFmtHeader(&fmt);
        if (err == 0) {
            if (fmt.avgBytesPerSec == 0u) {
                err = WAVE_ERR_CORRUPT;
            } else {
                float bytesPerMs = (float)fmt.avgBytesPerSec * 0.001f;
                if (fmt.formatTag == 0xFFFEu
                    && sce_paf_memcmp(AT9_SUBFORMAT_GUID, fmt.subFormatGuid, 16u) == 0
                    && fmt.sampleRate == 48000u
                    && (fmt.channels == 1u || fmt.channels == 2u)
                    && fmt.bitsPerSample == 16u) {
                    outMeta->sampleRate = (int)fmt.sampleRate;
                    outMeta->hasSampleRate = 1;
                    outMeta->codecProfile = 29;
                    outMeta->hasCodecProfile = 1;

                    At9SmplLoopInfo loopInfo;
                    loopInfo.loopStart = 0;
                    loopInfo.loopEnd = 0;
                    err = m_cacheIo->readSmplLoop(&loopInfo);
                    if (err == 0) {
                        int baseDurationMs = (int)((float)m_cacheIo->getDataSize() / bytesPerMs);
                        outMeta->durationMs = (unsigned int)baseDurationMs;
                        outMeta->hasDuration = 1;

                        if (loopInfo.loopStart != 0u && loopInfo.loopEnd != 0u) {
                            uint32_t loopSamples = loopInfo.loopEnd - loopInfo.loopStart;
                            uint32_t loopDurationMs = (loopSamples * 1000u) / fmt.sampleRate;
                            outMeta->durationMs = (unsigned int)(baseDurationMs + (int)loopDurationMs);
                        }
                        if (loopCount == -1) {
                            outMeta->durationMs = 0xFFFFFFFEu;
                        }

                        outMeta->channels = (int)fmt.channels;
                        outMeta->hasChannels = 1;

                        uint16_t frameBytes = fmt.blockAlign;
                        if (fmt.samplesPerBlock == 1024u) {
                            frameBytes = (uint16_t)(frameBytes / 4u);
                        }

                        int bitrateKbps = 0;
                        if (lookupAt9BitrateKbps(frameBytes, &bitrateKbps)) {
                            outMeta->bitRate = bitrateKbps;
                            outMeta->hasBitRate = 1;
                        } else {
                            outMeta->hasBitRate = 0;
                        }

                        outMeta->codecSubType = 13;
                        outMeta->hasCodecSubType = 1;
                    }
                } else {
                    err = WAVE_ERR_UNSUPPORTED;
                }
            }
        }
    }

    int status = mapWaveMetadataError(err);
    if (status == 0) {
        return 0;
    }

    clearMetadataValidityFlags(outMeta);
    return status;
}

} /* namespace MusicCore */
} /* namespace sce */
