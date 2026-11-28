/*
 * SceShell musiccore_plugin - ISO BMFF / MP4 box & MPEG-4 descriptor readers
 * (esds, DecoderConfigDescriptor, AudioSpecificConfig, mp4a, dinf, mdhd, mdia,
 * minf, moov, mvhd, stbl, stco, stsc, stsd, stsz, stts, tkhd, trak, and
 * SampleTableNavigator).
 * Original address range (3.65-CEX): 0x811FF930 - 0x8120305B
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
int sce_paf_memcmp(const void *lhs, const void *rhs, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
}

namespace sce {
namespace MusicCore {

static const uint32_t MP4_ERR_UNSUPPORTED = 0x80280021u;   /* -2144862175 */
static const uint32_t MP4_ERR_CORRUPT = 0x80280022u;       /* -2144862174 */
static const uint32_t MP4_ERR_NOT_READY = 0x80280024u;     /* -2144862172 */
static const uint32_t MP4_ERR_BAD_PARENT = 0x80280025u;    /* -2144862171 */
static const uint32_t MP4_ERR_NOT_FOUND = 0x80280028u;     /* -2144862168 */
static const uint32_t IO_ERR_BAD_PARAM = 0x8010AFF1u;      /* -2146390031 */

/* AAC ISO/IEC 14496-3 sampling frequency index table (0x81513534) */
static const uint32_t g_aacSampleRateTable[16] = {
    96000u, 88200u, 64000u, 48000u,
    44100u, 32000u, 24000u, 22050u,
    16000u, 12000u, 11025u,  8000u,
     7350u,     0u,     0u,     0u
};

/* Expected ISO BMFF fixed constants for mvhd (0x81545010..0x8154505F) */
static const uint8_t g_mvhdExpectedRate[4] = { 0x00, 0x01, 0x00, 0x00 };
static const uint8_t g_mvhdExpectedVolume[2] = { 0x01, 0x00 };
static const uint8_t g_zeroBytes2[2] = { 0x00, 0x00 };
static const uint8_t g_zeroBytes4[4] = { 0x00, 0x00, 0x00, 0x00 };
static const uint8_t g_zeroBytes6[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t g_zeroBytes8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t g_zeroBytes12[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t g_zeroBytes24[24] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const uint8_t g_unityMatrix36[36] = {
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00
};
static const uint8_t g_tkhdAudioVolume[2] = { 0x01, 0x00 };
static const uint8_t g_tkhdWidth320[4] = { 0x01, 0x40, 0x00, 0x00 };
static const uint8_t g_tkhdHeight240[4] = { 0x00, 0xF0, 0x00, 0x00 };

extern uint32_t readBigEndian32(const void *ptr);
extern uint32_t readBigEndian24(const void *ptr);
extern uint16_t readBigEndian16(const void *ptr);

/* Cached multi-stream I/O interface used by MP4 box readers (+24 getFileSize, +68 mapStreamRange) */
class Mp4CachedIo {
public:
    virtual ~Mp4CachedIo() {}
    virtual int isInitialized() const = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() const = 0;
    virtual int getFileSize() const = 0;
    virtual const char *getPath() const = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(uint32_t mode) = 0;
    virtual int applyOpenFilePriority(int mode) = 0;
    virtual void flushCache() = 0;
    virtual int setCacheHint(int hintMode) = 0;
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, uint32_t *outErr,
                                 int *outMappedSize) = 0;
    virtual void *mapRangePinned(int offset, uint32_t size, int pinFlag,
                                 int *outPinToken, uint32_t *outErr, uint32_t *outMappedSize) = 0;
    virtual int unpinRange(int pinToken, uint32_t *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, uint32_t *outErr) = 0;
    virtual void *mapStreamRange(int streamIdx, uint32_t offset, uint32_t size,
                                 uint32_t *outErr) = 0;
};

/* 0x8120111A: big-endian bitstream extractor */
uint32_t readBitField(const uint8_t *buffer, uint32_t bitOffset, uint8_t bitCount)
{
    uint32_t endBit = bitOffset + bitCount;
    uint32_t endByte = endBit >> 3;
    uint32_t curByte = bitOffset >> 3;
    uint32_t result = 0;

    if (curByte <= endByte) {
        const uint8_t *ptr = buffer + curByte;
        do {
            uint8_t startBitInByte = (curByte == (bitOffset >> 3)) ? (uint8_t)(bitOffset & 7u) : 0u;
            uint8_t endBitInByte = (curByte == endByte) ? (uint8_t)(endBit & 7u) : 8u;
            uint8_t width = (uint8_t)(endBitInByte - startBitInByte);
            uint8_t byteVal = *ptr++;
            uint8_t mask = (uint8_t)((1u << width) - 1u);
            result = (result << width) | ((byteVal >> (8u - endBitInByte)) & mask);
            ++curByte;
        } while (curByte <= endByte);
    }
    return result;
}

/* ===== Base ISO BMFF Box & Reader (0x811FFBF8 - 0x811FFF5B) ===== */

class Mp4BoxBase {
public:
    explicit Mp4BoxBase(const char *fourcc);
    virtual ~Mp4BoxBase();

    virtual uint32_t getContentOffset() const;

    void resetParsed();
    bool isParsed() const;

    uint32_t m_boxSize;      /* +4 */
    char m_fourcc[4];        /* +8 */
    uint32_t m_fileOffset;   /* +12 */
    uint32_t m_headerSize;   /* +16 */
    int m_isParsed;          /* +20 */
};

/* 0x811FFBF8 */
Mp4BoxBase::Mp4BoxBase(const char *fourcc)
    : m_boxSize(0xFFFFFFFFu)
    , m_fileOffset(0)
    , m_headerSize(0)
    , m_isParsed(0)
{
    sce_paf_memcpy(m_fourcc, fourcc, 4u);
}

/* 0x811FFC28 / 0x811FFC36 */
Mp4BoxBase::~Mp4BoxBase()
{
}

uint32_t Mp4BoxBase::getContentOffset() const
{
    return m_headerSize;
}

/* 0x811FFC4C */
void Mp4BoxBase::resetParsed()
{
    m_isParsed = 0;
}

/* 0x811FFC52 */
bool Mp4BoxBase::isParsed() const
{
    return m_isParsed == 1;
}

class Mp4BoxReaderBase {
public:
    Mp4BoxReaderBase(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4BoxReaderBase();

    uint16_t countChildBoxes(const Mp4BoxBase *parentBox, const Mp4BoxBase *targetProto,
                             uint32_t *outErr);
    int locateChildBox(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                       Mp4BoxBase *outBox, uint32_t *outErr);

protected:
    Mp4CachedIo *m_io; /* +4 */
    int m_streamIdx;   /* +8 */
};

/* 0x811FFC5E */
Mp4BoxReaderBase::Mp4BoxReaderBase(Mp4CachedIo *io, int streamIdx)
    : m_io(io)
    , m_streamIdx(streamIdx)
{
}

/* 0x811FFC74 / 0x811FFC88 */
Mp4BoxReaderBase::~Mp4BoxReaderBase()
{
}

/* 0x811FFC96 */
uint16_t Mp4BoxReaderBase::countChildBoxes(const Mp4BoxBase *parentBox,
                                           const Mp4BoxBase *targetProto,
                                           uint32_t *outErr)
{
    if (m_io == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }

    int fileSize = m_io->getFileSize();
    if (fileSize < 0) {
        *outErr = (uint32_t)fileSize;
        return 0;
    }

    uint32_t scanStart = 0;
    uint32_t scanEnd = (uint32_t)fileSize;
    if (parentBox != NULL) {
        if (!parentBox->isParsed()) {
            *outErr = MP4_ERR_BAD_PARENT;
            return 0;
        }
        scanStart = parentBox->m_fileOffset + parentBox->getContentOffset();
        scanEnd = parentBox->m_fileOffset + parentBox->m_boxSize;
        if (scanEnd > (uint32_t)fileSize) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    uint16_t matchCount = 0;
    uint32_t curOffset = scanStart;
    while (curOffset < scanEnd) {
        const uint8_t *hdr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, curOffset, 8u, &mapErr);
        if (hdr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        uint32_t boxSize = readBigEndian32(hdr);
        if (boxSize == 1u || boxSize < 8u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        if (sce_paf_memcmp(hdr + 4, targetProto->m_fourcc, 4u) == 0) {
            ++matchCount;
        }
        curOffset += boxSize;
    }

    *outErr = 0;
    return matchCount;
}

/* 0x811FFDC2 */
int Mp4BoxReaderBase::locateChildBox(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                                     Mp4BoxBase *outBox, uint32_t *outErr)
{
    if (m_io == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }

    int fileSize = m_io->getFileSize();
    if (fileSize < 0) {
        *outErr = (uint32_t)fileSize;
        return 0;
    }
    if (occurrenceIndex == 0u) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }

    uint32_t scanStart = 0;
    uint32_t scanEnd = 0;
    if (parentBox != NULL) {
        if (!parentBox->isParsed()) {
            *outErr = MP4_ERR_BAD_PARENT;
            return 0;
        }
        scanStart = parentBox->m_fileOffset + parentBox->getContentOffset();
        scanEnd = parentBox->m_fileOffset + parentBox->m_boxSize;
        if (scanEnd > (uint32_t)fileSize) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
    } else {
        scanEnd = (uint32_t)m_io->getFileSize();
    }

    uint16_t seenCount = 1;
    uint32_t curOffset = scanStart;
    while (curOffset < scanEnd) {
        uint32_t mapErr = 0xFFFFFFFFu;
        const uint8_t *hdr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, curOffset, 8u, &mapErr);
        if (hdr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        uint32_t boxSize = readBigEndian32(hdr);
        if (boxSize == 1u || boxSize < 8u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        uint32_t nextOffset = curOffset + boxSize;
        if (sce_paf_memcmp(hdr + 4, outBox->m_fourcc, 4u) == 0) {
            if (seenCount == occurrenceIndex) {
                if (nextOffset > scanEnd) {
                    *outErr = MP4_ERR_CORRUPT;
                    return 0;
                }
                outBox->m_boxSize = boxSize;
                outBox->m_fileOffset = curOffset;
                outBox->m_headerSize = 8u;
                *outErr = 0;
                return 1;
            }
            ++seenCount;
        }
        curOffset = nextOffset;
    }

    *outErr = MP4_ERR_NOT_FOUND;
    return 0;
}

/* ===== FullBox Base & Reader (0x81200984 - 0x81200AC3) ===== */

class Mp4FullBox : public Mp4BoxBase {
public:
    Mp4FullBox(const char *fourcc, uint8_t version);
    virtual ~Mp4FullBox();

    virtual uint32_t getContentOffset() const;

    uint8_t m_version;         /* +24 */
    uint8_t m_flags[3];        /* +25..+27 */
    uint32_t m_fullHeaderSize; /* +28 */
};

/* 0x81200984 */
Mp4FullBox::Mp4FullBox(const char *fourcc, uint8_t version)
    : Mp4BoxBase(fourcc)
    , m_version(version)
    , m_fullHeaderSize(0)
{
    m_flags[0] = 0;
    m_flags[1] = 0;
    m_flags[2] = 0;
}

/* 0x812009A0 / 0x812009BC */
Mp4FullBox::~Mp4FullBox()
{
}

uint32_t Mp4FullBox::getContentOffset() const
{
    return m_fullHeaderSize;
}

class Mp4FullBoxReader : public Mp4BoxReaderBase {
public:
    Mp4FullBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4FullBoxReader();

    int readFullBoxHeader(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                          Mp4FullBox *outBox, uint32_t *outErr);
};

/* 0x812009DC */
Mp4FullBoxReader::Mp4FullBoxReader(Mp4CachedIo *io, int streamIdx)
    : Mp4BoxReaderBase(io, streamIdx)
{
}

/* 0x812009F4 / 0x81200A10 */
Mp4FullBoxReader::~Mp4FullBoxReader()
{
}

/* 0x81200A30 */
int Mp4FullBoxReader::readFullBoxHeader(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                                        Mp4FullBox *outBox, uint32_t *outErr)
{
    if (locateChildBox(parentBox, occurrenceIndex, outBox, outErr) == 0) {
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *vf = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_headerSize, 4u, &mapErr);
    if (vf == NULL) {
        *outErr = mapErr;
        return 0;
    }

    outBox->m_version = vf[0];
    sce_paf_memcpy(outBox->m_flags, vf + 1, 3u);
    outBox->m_fullHeaderSize = outBox->m_headerSize + 4u;
    *outErr = 0;
    return 1;
}

/* ===== MPEG-4 Descriptor Base & Reader (0x8120016C - 0x81200323) ===== */

class Mp4DescriptorBase {
public:
    explicit Mp4DescriptorBase(uint8_t tag);
    virtual ~Mp4DescriptorBase();

    uint8_t m_tag;           /* +4 */
    uint8_t m_padTag[3];     /* +5..+7 */
    uint32_t m_fileOffset;   /* +8 */
    uint32_t m_totalSize;    /* +12 */
    uint32_t m_headerSize;   /* +16 */
    int m_isParsed;          /* +20 */
};

/* 0x8120016C */
Mp4DescriptorBase::Mp4DescriptorBase(uint8_t tag)
    : m_tag(tag)
    , m_fileOffset(0)
    , m_totalSize(0)
    , m_headerSize(0)
    , m_isParsed(0)
{
}

/* 0x81200188 / 0x81200196 */
Mp4DescriptorBase::~Mp4DescriptorBase()
{
}

class Mp4DescriptorReaderBase {
public:
    Mp4DescriptorReaderBase(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DescriptorReaderBase();

    int readDescriptorHeader(uint32_t fileOffset, uint32_t availSize,
                             Mp4DescriptorBase *outDesc, uint32_t *outErr);

protected:
    Mp4CachedIo *m_io; /* +4 */
    int m_streamIdx;   /* +8 */
};

/* 0x812001AA */
Mp4DescriptorReaderBase::Mp4DescriptorReaderBase(Mp4CachedIo *io, int streamIdx)
    : m_io(io)
    , m_streamIdx(streamIdx)
{
}

/* 0x812001C0 / 0x812001D4 */
Mp4DescriptorReaderBase::~Mp4DescriptorReaderBase()
{
}

/* 0x812001E2 */
int Mp4DescriptorReaderBase::readDescriptorHeader(uint32_t fileOffset, uint32_t availSize,
                                                  Mp4DescriptorBase *outDesc, uint32_t *outErr)
{
    if (m_io == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }
    if (availSize < 2u) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *tagPtr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, fileOffset, 1u, &mapErr);
    if (tagPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }
    if (sce_paf_memcmp(tagPtr, &outDesc->m_tag, 1u) != 0) {
        *outErr = MP4_ERR_NOT_FOUND;
        return 0;
    }

    uint32_t payloadSize = 0;
    uint32_t curOffset = fileOffset + 1u;
    uint32_t hdrBytes = 1u;
    while (true) {
        if (hdrBytes > 4u || hdrBytes + 1u > availSize) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        const uint8_t *lenBytePtr = (const uint8_t *)m_io->mapStreamRange(
            m_streamIdx, curOffset, 1u, &mapErr);
        if (lenBytePtr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        uint8_t byteVal = *lenBytePtr;
        payloadSize = (payloadSize << 7) | (byteVal & 0x7Fu);
        ++curOffset;
        ++hdrBytes;
        if ((byteVal & 0x80u) == 0u) {
            break;
        }
    }

    if (availSize < payloadSize + hdrBytes) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outDesc->m_fileOffset = fileOffset;
    outDesc->m_totalSize = payloadSize + hdrBytes;
    outDesc->m_headerSize = hdrBytes;
    *outErr = 0;
    return 1;
}

/* ===== DecoderSpecificInfo & AudioSpecificConfig (0x812004D0 - 0x812005A3, 0x811FF930 - 0x811FFBF7) ===== */

class Mp4DecoderSpecificInfo : public Mp4DescriptorBase {
public:
    Mp4DecoderSpecificInfo();
    virtual ~Mp4DecoderSpecificInfo();

    uint32_t m_bodyOffset; /* +24 */
};

/* 0x812004D0 */
Mp4DecoderSpecificInfo::Mp4DecoderSpecificInfo()
    : Mp4DescriptorBase(5u)
    , m_bodyOffset(0)
{
}

/* 0x812004F0 / 0x8120050C */
Mp4DecoderSpecificInfo::~Mp4DecoderSpecificInfo()
{
}

class Mp4DecoderSpecificInfoReader : public Mp4DescriptorReaderBase {
public:
    Mp4DecoderSpecificInfoReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DecoderSpecificInfoReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4DecoderSpecificInfo *outInfo,
             uint32_t *outErr);
};

/* 0x8120052C */
Mp4DecoderSpecificInfoReader::Mp4DecoderSpecificInfoReader(Mp4CachedIo *io, int streamIdx)
    : Mp4DescriptorReaderBase(io, streamIdx)
{
}

/* 0x81200544 / 0x81200560 */
Mp4DecoderSpecificInfoReader::~Mp4DecoderSpecificInfoReader()
{
}

/* 0x81200580 */
int Mp4DecoderSpecificInfoReader::read(uint32_t fileOffset, uint32_t availSize,
                                       Mp4DecoderSpecificInfo *outInfo, uint32_t *outErr)
{
    if (readDescriptorHeader(fileOffset, availSize, outInfo, outErr) == 0) {
        return 0;
    }
    outInfo->m_bodyOffset = outInfo->m_headerSize;
    *outErr = 0;
    return 1;
}

class Mp4AudioSpecificConfig : public Mp4DecoderSpecificInfo {
public:
    Mp4AudioSpecificConfig();
    virtual ~Mp4AudioSpecificConfig();

    uint32_t getAudioObjectType() const;
    uint32_t getSamplingFrequency() const;
    uint8_t getChannelConfig() const;

    uint8_t m_rawHeaderBytes[4];   /* +28..+31 */
    uint32_t m_audioObjectType;    /* +32 */
    uint32_t m_samplingFrequency;  /* +36 */
    uint8_t m_channelConfig;       /* +40 */
    uint8_t m_padChannel[3];       /* +41..+43 */
    uint32_t m_extSampleRate;      /* +44 */
    uint8_t m_sbrPresentFlag;      /* +48 */
    uint8_t m_padSbr[3];           /* +49..+51 */
    uint32_t m_parsedEndOffset;    /* +52 */
};

/* 0x811FF930 */
Mp4AudioSpecificConfig::Mp4AudioSpecificConfig()
    : Mp4DecoderSpecificInfo()
    , m_audioObjectType(0)
    , m_samplingFrequency(0)
    , m_channelConfig(0)
    , m_extSampleRate(0)
    , m_sbrPresentFlag(0)
    , m_parsedEndOffset(0)
{
}

/* 0x811FF95C / 0x811FF978 */
Mp4AudioSpecificConfig::~Mp4AudioSpecificConfig()
{
}

uint32_t Mp4AudioSpecificConfig::getAudioObjectType() const
{
    return m_audioObjectType;
}

uint32_t Mp4AudioSpecificConfig::getSamplingFrequency() const
{
    return m_samplingFrequency;
}

uint8_t Mp4AudioSpecificConfig::getChannelConfig() const
{
    return m_channelConfig;
}

class Mp4AudioSpecificConfigReader : public Mp4DecoderSpecificInfoReader {
public:
    Mp4AudioSpecificConfigReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4AudioSpecificConfigReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4AudioSpecificConfig *outConfig,
             uint32_t *outErr);
};

/* 0x811FF9A8 */
Mp4AudioSpecificConfigReader::Mp4AudioSpecificConfigReader(Mp4CachedIo *io, int streamIdx)
    : Mp4DecoderSpecificInfoReader(io, streamIdx)
{
}

/* 0x811FF9C0 / 0x811FF9DC */
Mp4AudioSpecificConfigReader::~Mp4AudioSpecificConfigReader()
{
}

/* 0x811FF9FC */
int Mp4AudioSpecificConfigReader::read(uint32_t fileOffset, uint32_t availSize,
                                       Mp4AudioSpecificConfig *outConfig, uint32_t *outErr)
{
    if (Mp4DecoderSpecificInfoReader::read(fileOffset, availSize, outConfig, outErr) == 0) {
        return 0;
    }

    uint32_t bodyOff = outConfig->m_bodyOffset;
    uint32_t bodyLen = availSize - bodyOff;
    if (bodyLen < 2u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *ascBytes = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, fileOffset + bodyOff, bodyLen, &mapErr);
    if (ascBytes == NULL) {
        *outErr = mapErr;
        return 0;
    }

    sce_paf_memcpy(outConfig->m_rawHeaderBytes, ascBytes, 2u);
    outConfig->m_audioObjectType = (uint8_t)readBitField(ascBytes, 0u, 5u);

    uint8_t freqIndex = (uint8_t)readBitField(ascBytes, 5u, 4u);
    uint32_t chanBitPos = 9u;
    uint32_t afterChanBitPos = 13u;
    if (freqIndex == 15u) {
        outConfig->m_samplingFrequency = readBitField(ascBytes, 9u, 24u);
        chanBitPos = 33u;
        afterChanBitPos = 37u;
    } else {
        outConfig->m_samplingFrequency = g_aacSampleRateTable[freqIndex];
    }

    outConfig->m_channelConfig = (uint8_t)readBitField(ascBytes, chanBitPos, 4u);

    uint32_t cursorBit = afterChanBitPos;
    uint32_t extObjectType = 0;
    if (outConfig->m_audioObjectType == 5u) {
        outConfig->m_sbrPresentFlag = 1;
        extObjectType = 5u;
        uint8_t extFreqIdx = (uint8_t)readBitField(ascBytes, cursorBit, 4u);
        cursorBit += 4u;
        if (extFreqIdx == 15u) {
            extFreqIdx = (uint8_t)readBitField(ascBytes, cursorBit, 24u);
            cursorBit += 24u;
        }
        outConfig->m_extSampleRate = g_aacSampleRateTable[extFreqIdx];
        outConfig->m_audioObjectType = readBitField(ascBytes, cursorBit, 5u);
        cursorBit += 5u;
    }

    uint8_t dependsOnCoreCoder = (uint8_t)readBitField(ascBytes, cursorBit + 1u, 1u);
    uint32_t afterGaBit = (dependsOnCoreCoder != 0u) ? (cursorBit + 16u) : (cursorBit + 2u);
    int remainingBits = (int)(8u * bodyLen) - (int)(afterGaBit + 1u);

    if (extObjectType != 5u && remainingBits >= 16
        && (uint16_t)readBitField(ascBytes, afterGaBit + 1u, 11u) == 0x2B7u
        && (uint16_t)readBitField(ascBytes, afterGaBit + 12u, 5u) == 5u) {
        uint8_t sbrFlag = (uint8_t)readBitField(ascBytes, afterGaBit + 17u, 1u);
        outConfig->m_sbrPresentFlag = sbrFlag;
        if (sbrFlag == 1u) {
            uint8_t extFreqIdx = (uint8_t)readBitField(ascBytes, afterGaBit + 18u, 4u);
            if (extFreqIdx == 15u) {
                outConfig->m_extSampleRate = readBitField(ascBytes, afterGaBit + 22u, 24u);
            } else {
                outConfig->m_extSampleRate = g_aacSampleRateTable[extFreqIdx];
            }
        }
    }

    outConfig->m_isParsed = 1;
    outConfig->m_parsedEndOffset = outConfig->m_bodyOffset + 2u;
    *outErr = 0;
    return 1;
}

/* 0x81201188: validate AAC-LC AudioSpecificConfig compatibility */
bool isSupportedAacProfile(const Mp4AudioSpecificConfig *entryCountHolder,
                           const Mp4AudioSpecificConfig *asc)
{
    if (entryCountHolder->getAudioObjectType() != 1u || asc->getAudioObjectType() != 2u) {
        return false;
    }
    uint32_t sampleRate = asc->getSamplingFrequency();
    if (sampleRate != 8000u && sampleRate != 11025u && sampleRate != 12000u
        && sampleRate != 16000u && sampleRate != 22050u && sampleRate != 24000u
        && sampleRate != 32000u && sampleRate != 44100u && sampleRate != 48000u) {
        return false;
    }
    uint8_t channels = asc->getChannelConfig();
    return (channels == 1u || channels == 2u);
}

/* ===== DecoderConfigDescriptor & ES_Descriptor (0x81200324 - 0x812004CF, 0x812007AC - 0x81200983) ===== */

class Mp4DecoderConfigDescriptor : public Mp4DescriptorBase {
public:
    Mp4DecoderConfigDescriptor();
    virtual ~Mp4DecoderConfigDescriptor();

    uint8_t m_objectTypeIndication; /* +24 */
    uint8_t m_streamType;           /* +25 */
    uint8_t m_upstreamFlag;         /* +26 */
    uint8_t m_padFlags;             /* +27 */
    uint32_t m_bufferSizeDb;        /* +28 */
    uint32_t m_maxBitrate;          /* +32 */
    uint32_t m_avgBitrate;          /* +36 */
    Mp4DescriptorBase *m_decSpec;   /* +40 */
    uint32_t m_parsedEndOffset;     /* +44 */
};

/* 0x81200324 */
Mp4DecoderConfigDescriptor::Mp4DecoderConfigDescriptor()
    : Mp4DescriptorBase(4u)
    , m_objectTypeIndication(0xFFu)
    , m_streamType(0xFFu)
    , m_upstreamFlag(0)
    , m_padFlags(0)
    , m_bufferSizeDb(0)
    , m_maxBitrate(0)
    , m_avgBitrate(0)
    , m_decSpec(NULL)
    , m_parsedEndOffset(0)
{
}

/* 0x81200350 / 0x8120037C */
Mp4DecoderConfigDescriptor::~Mp4DecoderConfigDescriptor()
{
    if (m_decSpec != NULL) {
        delete m_decSpec;
        m_decSpec = NULL;
    }
}

class Mp4DecoderConfigReader : public Mp4DescriptorReaderBase {
public:
    Mp4DecoderConfigReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DecoderConfigReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4DecoderConfigDescriptor *outDesc,
             uint32_t *outErr);
};

/* 0x812003AC */
Mp4DecoderConfigReader::Mp4DecoderConfigReader(Mp4CachedIo *io, int streamIdx)
    : Mp4DescriptorReaderBase(io, streamIdx)
{
}

/* 0x812003C4 / 0x812003E0 */
Mp4DecoderConfigReader::~Mp4DecoderConfigReader()
{
}

/* 0x81200400 */
int Mp4DecoderConfigReader::read(uint32_t fileOffset, uint32_t availSize,
                                 Mp4DecoderConfigDescriptor *outDesc, uint32_t *outErr)
{
    if (readDescriptorHeader(fileOffset, availSize, outDesc, outErr) == 0) {
        return 0;
    }
    if (outDesc->m_totalSize < outDesc->m_headerSize + 13u) {
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *body = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, fileOffset + outDesc->m_headerSize, 13u, &mapErr);
    if (body == NULL) {
        *outErr = mapErr;
        return 0;
    }
    if ((body[1] & 1u) == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outDesc->m_objectTypeIndication = body[0];
    outDesc->m_streamType = (uint8_t)(readBigEndian32(body + 1) & 0x3Fu);
    outDesc->m_upstreamFlag = ((body[1] & 0x20u) != 0u) ? 1u : 0u;
    outDesc->m_bufferSizeDb = readBigEndian24(body + 2);
    outDesc->m_maxBitrate = readBigEndian32(body + 5);
    outDesc->m_avgBitrate = readBigEndian32(body + 9);
    outDesc->m_parsedEndOffset = outDesc->m_headerSize + 13u;
    outDesc->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

class Mp4EsDescriptor : public Mp4DescriptorBase {
public:
    Mp4EsDescriptor();
    virtual ~Mp4EsDescriptor();

    uint8_t m_esIdLow;             /* +24 */
    uint8_t m_streamDependFlag;    /* +25 */
    uint8_t m_urlFlag;             /* +26 */
    uint8_t m_padFlags;            /* +27 */
    uint8_t m_ocrStreamFlag;       /* +28 */
    uint8_t m_streamPriority;      /* +29 */
    uint8_t m_padTail[2];          /* +30..+31 */
    uint32_t m_parsedEndOffset;    /* +32 */
};

/* 0x812007AC */
Mp4EsDescriptor::Mp4EsDescriptor()
    : Mp4DescriptorBase(3u)
    , m_esIdLow(0)
    , m_streamDependFlag(0)
    , m_urlFlag(0)
    , m_padFlags(0)
    , m_ocrStreamFlag(0)
    , m_streamPriority(0)
    , m_parsedEndOffset(0)
{
}

/* 0x812007D4 / 0x812007F0 */
Mp4EsDescriptor::~Mp4EsDescriptor()
{
}

class Mp4EsDescriptorReader : public Mp4DescriptorReaderBase {
public:
    Mp4EsDescriptorReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4EsDescriptorReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4EsDescriptor *outDesc, uint32_t *outErr);
};

/* 0x81200810 */
Mp4EsDescriptorReader::Mp4EsDescriptorReader(Mp4CachedIo *io, int streamIdx)
    : Mp4DescriptorReaderBase(io, streamIdx)
{
}

/* 0x81200828 / 0x81200844 */
Mp4EsDescriptorReader::~Mp4EsDescriptorReader()
{
}

/* 0x81200864 */
int Mp4EsDescriptorReader::read(uint32_t fileOffset, uint32_t availSize,
                                Mp4EsDescriptor *outDesc, uint32_t *outErr)
{
    if (readDescriptorHeader(fileOffset, availSize, outDesc, outErr) == 0) {
        return 0;
    }
    if (availSize - outDesc->m_headerSize < 3u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *hdr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, fileOffset + outDesc->m_headerSize, 3u, &mapErr);
    if (hdr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    outDesc->m_esIdLow = (uint8_t)readBigEndian16(hdr);
    uint8_t flags = hdr[2];
    outDesc->m_streamDependFlag = flags & 0x80u;
    outDesc->m_urlFlag = flags & 0x40u;
    outDesc->m_ocrStreamFlag = flags & 0x20u;
    outDesc->m_streamPriority = flags & 0x1Fu;

    uint32_t cursor = outDesc->m_headerSize + 3u;
    if (outDesc->m_streamDependFlag != 0u) {
        cursor += 2u;
    }
    if (outDesc->m_urlFlag != 0u) {
        const uint8_t *urlLenPtr = (const uint8_t *)m_io->mapStreamRange(
            m_streamIdx, outDesc->m_fileOffset + cursor, 1u, &mapErr);
        if (urlLenPtr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        cursor += (uint32_t)(*urlLenPtr) + 1u;
    }
    if (outDesc->m_ocrStreamFlag != 0u) {
        cursor += 2u;
    }

    if (cursor > outDesc->m_totalSize) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outDesc->m_parsedEndOffset = cursor;
    outDesc->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* ===== SampleEntry, AudioSampleEntry (mp4a), Container Boxes ===== */

class Mp4SampleEntryBox : public Mp4BoxBase {
public:
    explicit Mp4SampleEntryBox(const char *fourcc);
    virtual ~Mp4SampleEntryBox();

    uint16_t m_dataReferenceIndex; /* +24 */
    uint16_t m_padRef;             /* +26 */
    uint32_t m_sampleEntryEnd;     /* +28 */
};

/* 0x81201200 */
Mp4SampleEntryBox::Mp4SampleEntryBox(const char *fourcc)
    : Mp4BoxBase(fourcc)
    , m_dataReferenceIndex(0)
    , m_padRef(0)
    , m_sampleEntryEnd(0)
{
}

/* 0x81201220 / 0x8120123C */
Mp4SampleEntryBox::~Mp4SampleEntryBox()
{
}

class Mp4SampleEntryReader : public Mp4BoxReaderBase {
public:
    Mp4SampleEntryReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4SampleEntryReader();

    int read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
             Mp4SampleEntryBox *outBox, uint32_t *outErr);
};

/* 0x8120125C */
Mp4SampleEntryReader::Mp4SampleEntryReader(Mp4CachedIo *io, int streamIdx)
    : Mp4BoxReaderBase(io, streamIdx)
{
}

/* 0x81201274 / 0x81201290 */
Mp4SampleEntryReader::~Mp4SampleEntryReader()
{
}

/* 0x812012B0 */
int Mp4SampleEntryReader::read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                               Mp4SampleEntryBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (locateChildBox(parentBox, 1u, outBox, outErr) == 0) {
        if (*outErr == IO_ERR_BAD_PARAM) {
            *outErr = 0xFFFFFFFFu;
        }
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_headerSize + 8u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *entryHdr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_headerSize, 8u, &mapErr);
    if (entryHdr == NULL) {
        *outErr = mapErr;
        return 0;
    }
    if (sce_paf_memcmp(entryHdr, g_zeroBytes6, 6u) != 0) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_dataReferenceIndex = readBigEndian16(entryHdr + 6);
    outBox->m_sampleEntryEnd = outBox->m_headerSize + 8u;
    *outErr = 0;
    return 1;
}

class Mp4AudioSampleEntryBox : public Mp4SampleEntryBox {
public:
    Mp4AudioSampleEntryBox();
    virtual ~Mp4AudioSampleEntryBox();

    virtual uint32_t getContentOffset() const;

    uint16_t m_sampleRateHi; /* +32 */
    uint16_t m_padRate;      /* +34 */
    uint32_t m_audioEntryEnd;/* +36 */
};

/* 0x811FFF5C */
Mp4AudioSampleEntryBox::Mp4AudioSampleEntryBox()
    : Mp4SampleEntryBox("mp4a")
    , m_sampleRateHi(0)
    , m_padRate(0)
    , m_audioEntryEnd(0)
{
}

/* 0x811FFF84 / 0x811FFFA0 */
Mp4AudioSampleEntryBox::~Mp4AudioSampleEntryBox()
{
}

uint32_t Mp4AudioSampleEntryBox::getContentOffset() const
{
    return m_sampleEntryEnd + 20u;
}

class Mp4AudioSampleEntryReader : public Mp4SampleEntryReader {
public:
    Mp4AudioSampleEntryReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4AudioSampleEntryReader();

    int read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
             Mp4AudioSampleEntryBox *outBox, uint32_t *outErr);
};

/* 0x811FFFC6 */
Mp4AudioSampleEntryReader::Mp4AudioSampleEntryReader(Mp4CachedIo *io, int streamIdx)
    : Mp4SampleEntryReader(io, streamIdx)
{
}

/* 0x811FFFDE / 0x811FFFFA */
Mp4AudioSampleEntryReader::~Mp4AudioSampleEntryReader()
{
}

/* 0x8120001A */
int Mp4AudioSampleEntryReader::read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                                    Mp4AudioSampleEntryBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (Mp4SampleEntryReader::read(parentBox, occurrenceIndex, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_sampleEntryEnd + 20u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *audioHdr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_sampleEntryEnd, 20u, &mapErr);
    if (audioHdr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    if (readBigEndian32(audioHdr) != 0u
        || readBigEndian32(audioHdr + 4) != 0u
        || readBigEndian16(audioHdr + 8) != 2u
        || readBigEndian16(audioHdr + 10) != 16u
        || readBigEndian32(audioHdr + 12) != 0u
        || readBigEndian16(audioHdr + 18) != 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_sampleRateHi = readBigEndian16(audioHdr + 16);
    outBox->m_audioEntryEnd = outBox->m_sampleEntryEnd + 20u;
    outBox->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* ===== Container Boxes: dinf, esds, mdia, minf, moov, stbl, trak ===== */

class Mp4ContainerBox : public Mp4BoxBase {
public:
    explicit Mp4ContainerBox(const char *fourcc);
    virtual ~Mp4ContainerBox();

    uint32_t m_childrenOffset; /* +24 */
};

Mp4ContainerBox::Mp4ContainerBox(const char *fourcc)
    : Mp4BoxBase(fourcc)
    , m_childrenOffset(0)
{
}

Mp4ContainerBox::~Mp4ContainerBox() {}

class Mp4ContainerReader : public Mp4BoxReaderBase {
public:
    Mp4ContainerReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4ContainerReader();

    int readChildContainer(const Mp4BoxBase *parentBox, Mp4ContainerBox *outBox, uint32_t *outErr);
    int readTopContainer(Mp4ContainerBox *outBox, uint32_t *outErr);
    uint16_t countTracks(const Mp4BoxBase *moovBox, uint32_t *outErr);
    int readTrackAt(const Mp4BoxBase *moovBox, uint16_t trackIndex,
                    Mp4ContainerBox *outTrak, uint32_t *outErr);
};

Mp4ContainerReader::Mp4ContainerReader(Mp4CachedIo *io, int streamIdx)
    : Mp4BoxReaderBase(io, streamIdx)
{
}

Mp4ContainerReader::~Mp4ContainerReader() {}

/* 0x81200654 (dinf/mdia/minf/stbl) */
int Mp4ContainerReader::readChildContainer(const Mp4BoxBase *parentBox, Mp4ContainerBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (locateChildBox(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    outBox->m_isParsed = 1;
    outBox->m_childrenOffset = outBox->m_headerSize;
    *outErr = 0;
    return 1;
}

/* 0x81200E88 (moov) */
int Mp4ContainerReader::readTopContainer(Mp4ContainerBox *outBox, uint32_t *outErr)
{
    if (locateChildBox(NULL, 1u, outBox, outErr) == 0) {
        return 0;
    }
    outBox->m_isParsed = 1;
    outBox->m_childrenOffset = outBox->m_headerSize;
    *outErr = 0;
    return 1;
}

/* 0x81202F88 (trak count) */
uint16_t Mp4ContainerReader::countTracks(const Mp4BoxBase *moovBox, uint32_t *outErr)
{
    if (!moovBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    Mp4ContainerBox trakProto("trak");
    return countChildBoxes(moovBox, &trakProto, outErr);
}

/* 0x8120300C (trak by index) */
int Mp4ContainerReader::readTrackAt(const Mp4BoxBase *moovBox, uint16_t trackIndex,
                                    Mp4ContainerBox *outTrak, uint32_t *outErr)
{
    if (!moovBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (locateChildBox(moovBox, trackIndex, outTrak, outErr) == 0) {
        return 0;
    }
    outTrak->m_isParsed = 1;
    outTrak->m_childrenOffset = outTrak->m_headerSize;
    *outErr = 0;
    return 1;
}

class Mp4EsdsBox : public Mp4FullBox {
public:
    Mp4EsdsBox();
    virtual ~Mp4EsdsBox();

    uint32_t m_descriptorOffset; /* +32 */
};

Mp4EsdsBox::Mp4EsdsBox()
    : Mp4FullBox("esds", 0u)
    , m_descriptorOffset(0)
{
}

Mp4EsdsBox::~Mp4EsdsBox() {}

class Mp4EsdsBoxReader : public Mp4FullBoxReader {
public:
    Mp4EsdsBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4EsdsBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4EsdsBox *outBox, uint32_t *outErr);
};

Mp4EsdsBoxReader::Mp4EsdsBoxReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4EsdsBoxReader::~Mp4EsdsBoxReader() {}

/* 0x81200750 */
int Mp4EsdsBoxReader::read(const Mp4BoxBase *parentBox, Mp4EsdsBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        if (*outErr == IO_ERR_BAD_PARAM) {
            *outErr = 0xFFFFFFFFu;
        }
        return 0;
    }
    outBox->m_isParsed = 1;
    outBox->m_descriptorOffset = outBox->m_fullHeaderSize;
    *outErr = 0;
    return 1;
}

/* ===== mdhd, mvhd, tkhd Boxes & Readers ===== */

class Mp4MdhdBox : public Mp4FullBox {
public:
    Mp4MdhdBox();
    virtual ~Mp4MdhdBox();

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_timescale;        /* +40 */
    uint32_t m_duration;         /* +44 */
    uint32_t m_parsedEndOffset;  /* +48 */
};

/* 0x81200AC4 */
Mp4MdhdBox::Mp4MdhdBox()
    : Mp4FullBox("mdhd", 0u)
    , m_creationTime(0)
    , m_modificationTime(0)
    , m_timescale(0)
    , m_duration(0)
    , m_parsedEndOffset(0)
{
}

/* 0x81200AFC / 0x81200B18 */
Mp4MdhdBox::~Mp4MdhdBox()
{
}

class Mp4MdhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4MdhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4MdhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4MdhdBox *outBox, uint32_t *outErr);
};

Mp4MdhdBoxReader::Mp4MdhdBoxReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4MdhdBoxReader::~Mp4MdhdBoxReader() {}

/* 0x81200B8C */
int Mp4MdhdBoxReader::read(const Mp4BoxBase *parentBox, Mp4MdhdBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_version != 0u) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 20u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *body = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 20u, &mapErr);
    if (body == NULL) {
        *outErr = mapErr;
        return 0;
    }

    outBox->m_creationTime = readBigEndian32(body);
    outBox->m_modificationTime = readBigEndian32(body + 4);
    outBox->m_timescale = readBigEndian32(body + 8);
    outBox->m_duration = readBigEndian32(body + 12);
    outBox->m_isParsed = 1;
    outBox->m_parsedEndOffset = outBox->m_fullHeaderSize + 20u;
    *outErr = 0;
    return 1;
}

class Mp4MvhdBox : public Mp4FullBox {
public:
    Mp4MvhdBox();
    virtual ~Mp4MvhdBox();

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_timescale;        /* +40 */
    uint32_t m_duration;         /* +44 */
    uint32_t m_parsedEndOffset;  /* +48 */
};

/* 0x81200EB4 */
Mp4MvhdBox::Mp4MvhdBox()
    : Mp4FullBox("mvhd", 0u)
    , m_creationTime(0xFFFFFFFFu)
    , m_modificationTime(0xFFFFFFFFu)
    , m_timescale(0xFFFFFFFFu)
    , m_duration(0xFFFFFFFFu)
    , m_parsedEndOffset(0xFFFFFFFFu)
{
}

/* 0x81200EEC / 0x81200F08 */
Mp4MvhdBox::~Mp4MvhdBox()
{
}

class Mp4MvhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4MvhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4MvhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4MvhdBox *outBox, uint32_t *outErr);
};

Mp4MvhdBoxReader::Mp4MvhdBoxReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4MvhdBoxReader::~Mp4MvhdBoxReader() {}

/* 0x81200F7C */
int Mp4MvhdBoxReader::read(const Mp4BoxBase *parentBox, Mp4MvhdBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_version != 0u) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 96u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *body = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 96u, &mapErr);
    if (body == NULL) {
        *outErr = mapErr;
        return 0;
    }

    outBox->m_creationTime = readBigEndian32(body);
    outBox->m_modificationTime = readBigEndian32(body + 4);
    outBox->m_timescale = readBigEndian32(body + 8);
    outBox->m_duration = readBigEndian32(body + 12);

    bool fieldsOk = true;
    if (sce_paf_memcmp(body + 16, g_mvhdExpectedRate, 4u) != 0) {
        fieldsOk = false;
    }
    if (sce_paf_memcmp(body + 20, g_mvhdExpectedVolume, 2u) != 0) {
        fieldsOk = false;
    }
    if (sce_paf_memcmp(body + 22, g_zeroBytes2, 2u) != 0) {
        fieldsOk = false;
    }
    if (sce_paf_memcmp(body + 24, g_zeroBytes8, 8u) != 0) {
        fieldsOk = false;
    }
    if (sce_paf_memcmp(body + 32, g_unityMatrix36, 36u) != 0) {
        fieldsOk = false;
    }
    if (sce_paf_memcmp(body + 68, g_zeroBytes24, 24u) != 0) {
        fieldsOk = false;
    }
    if (!fieldsOk) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_isParsed = 1;
    outBox->m_parsedEndOffset = outBox->m_fullHeaderSize + 96u;
    *outErr = 0;
    return 1;
}

class Mp4TkhdBox : public Mp4FullBox {
public:
    Mp4TkhdBox();
    virtual ~Mp4TkhdBox();

    int isAudioTrackVolume() const;

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_trackId;          /* +40 */
    uint32_t m_duration;         /* +44 */
    uint8_t m_volumeBytes[2];    /* +48..+49 */
    uint8_t m_dimensionBytes[10];/* +50..+59 */
    uint32_t m_parsedEndOffset;  /* +60 */
};

/* 0x81202B60 */
Mp4TkhdBox::Mp4TkhdBox()
    : Mp4FullBox("tkhd", 0u)
    , m_creationTime(0)
    , m_modificationTime(0)
    , m_trackId(0)
    , m_duration(0)
    , m_parsedEndOffset(0)
{
}

/* 0x81202B98 / 0x81202BB4 */
Mp4TkhdBox::~Mp4TkhdBox()
{
}

/* 0x81202BD4 */
int Mp4TkhdBox::isAudioTrackVolume() const
{
    if (sce_paf_memcmp(m_volumeBytes, g_tkhdAudioVolume, 2u) == 0) {
        return 1;
    }
    return 0;
}

class Mp4TkhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4TkhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4TkhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4TkhdBox *outBox, uint32_t *outErr);
};

Mp4TkhdBoxReader::Mp4TkhdBoxReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4TkhdBoxReader::~Mp4TkhdBoxReader() {}

/* 0x81202C5A */
int Mp4TkhdBoxReader::read(const Mp4BoxBase *parentBox, Mp4TkhdBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        if (*outErr == IO_ERR_BAD_PARAM) {
            *outErr = 0xFFFFFFFFu;
        }
        return 0;
    }
    if (outBox->m_version != 0u) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 80u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *body = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 80u, &mapErr);
    if (body == NULL) {
        *outErr = mapErr;
        return 0;
    }

    outBox->m_creationTime = readBigEndian32(body);
    outBox->m_modificationTime = readBigEndian32(body + 4);
    outBox->m_trackId = readBigEndian32(body + 8);
    outBox->m_duration = readBigEndian32(body + 16);
    (void)sce_paf_memcmp(g_zeroBytes12, body + 20, 12u);

    if (sce_paf_memcmp(g_tkhdAudioVolume, body + 32, 2u) == 0) {
        sce_paf_memcpy(outBox->m_volumeBytes, g_tkhdAudioVolume, 2u);
    } else if (sce_paf_memcmp(g_zeroBytes2, body + 32, 2u) == 0) {
        sce_paf_memcpy(outBox->m_volumeBytes, g_zeroBytes2, 2u);
    } else {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    if (sce_paf_memcmp(g_zeroBytes2, body + 34, 2u) != 0
        || sce_paf_memcmp(g_unityMatrix36, body + 36, 36u) != 0) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    if (sce_paf_memcmp(g_tkhdWidth320, body + 72, 4u) == 0) {
        sce_paf_memcpy(outBox->m_dimensionBytes, g_tkhdWidth320, 4u);
    } else if (sce_paf_memcmp(g_zeroBytes4, body + 72, 4u) == 0) {
        sce_paf_memcpy(outBox->m_dimensionBytes, g_zeroBytes4, 4u);
    }

    if (sce_paf_memcmp(g_tkhdHeight240, body + 76, 4u) == 0) {
        sce_paf_memcpy(outBox->m_dimensionBytes, g_tkhdHeight240, 4u);
    } else if (sce_paf_memcmp(g_zeroBytes4, body + 76, 4u) == 0) {
        sce_paf_memcpy(outBox->m_dimensionBytes, g_zeroBytes4, 4u);
    }

    outBox->m_isParsed = 1;
    outBox->m_parsedEndOffset = outBox->m_fullHeaderSize + 80u;
    *outErr = 0;
    return 1;
}

/* ===== stco, stsc, stsd, stsz, stts Boxes & Readers ===== */

class Mp4TableBox : public Mp4FullBox {
public:
    explicit Mp4TableBox(const char *fourcc);
    virtual ~Mp4TableBox();

    uint32_t m_entryCount;  /* +32 */
    uint32_t m_tableOffset; /* +36 */
};

Mp4TableBox::Mp4TableBox(const char *fourcc)
    : Mp4FullBox(fourcc, 0u)
    , m_entryCount(0)
    , m_tableOffset(0)
{
}

Mp4TableBox::~Mp4TableBox() {}

class Mp4StcoReader : public Mp4FullBoxReader {
public:
    Mp4StcoReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StcoReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);
    uint32_t getChunkOffset(const Mp4TableBox *stco, uint32_t chunkIndex, uint32_t *outErr);
};

Mp4StcoReader::Mp4StcoReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4StcoReader::~Mp4StcoReader() {}

/* 0x81201DF4 */
int Mp4StcoReader::read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_fullHeaderSize + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *cntPtr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 4u, &mapErr);
    if (cntPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    uint32_t count = readBigEndian32(cntPtr);
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 4u * count + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_entryCount = count;
    outBox->m_tableOffset = outBox->m_fullHeaderSize + 4u;
    outBox->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* 0x81201ED2 */
uint32_t Mp4StcoReader::getChunkOffset(const Mp4TableBox *stco, uint32_t chunkIndex, uint32_t *outErr)
{
    if (!stco->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }
    if (chunkIndex == 0u || chunkIndex > stco->m_entryCount) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    uint32_t off = stco->m_fileOffset + stco->m_tableOffset + 4u * (chunkIndex - 1u);
    const uint8_t *entryPtr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
    if (entryPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }
    *outErr = 0;
    return readBigEndian32(entryPtr);
}

class Mp4StscReader : public Mp4FullBoxReader {
public:
    Mp4StscReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StscReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);

    inline uint32_t readEntryField(const Mp4TableBox *stsc, uint32_t entryIndex,
                                   uint32_t fieldOffset, uint32_t *outErr)
    {
        if (!stsc->isParsed()) {
            *outErr = MP4_ERR_NOT_READY;
            return 0;
        }
        if (entryIndex == 0u || entryIndex > stsc->m_entryCount) {
            *outErr = IO_ERR_BAD_PARAM;
            return 0;
        }

        uint32_t mapErr = 0xFFFFFFFFu;
        uint32_t off = stsc->m_fileOffset + stsc->m_tableOffset + 12u * (entryIndex - 1u) + fieldOffset;
        const uint8_t *ptr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
        if (ptr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        *outErr = 0;
        return readBigEndian32(ptr);
    }

    uint32_t getFirstChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
    uint32_t getSamplesPerChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
    uint32_t getSampleDescIndex(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
};

Mp4StscReader::Mp4StscReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4StscReader::~Mp4StscReader() {}

/* 0x81202038 */
int Mp4StscReader::read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        if (*outErr == IO_ERR_BAD_PARAM) {
            *outErr = 0xFFFFFFFFu;
        }
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_fullHeaderSize + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *cntPtr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 4u, &mapErr);
    if (cntPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    uint32_t count = readBigEndian32(cntPtr);
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 12u * count + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_entryCount = count;
    outBox->m_tableOffset = outBox->m_fullHeaderSize + 4u;
    outBox->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* 0x8120212E */
uint32_t Mp4StscReader::getFirstChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err)
{
    return readEntryField(stsc, idx, 0u, err);
}

/* 0x812021D8 */
uint32_t Mp4StscReader::getSamplesPerChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err)
{
    return readEntryField(stsc, idx, 4u, err);
}

/* 0x81202280 */
uint32_t Mp4StscReader::getSampleDescIndex(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err)
{
    return readEntryField(stsc, idx, 8u, err);
}

class Mp4StsdReader : public Mp4FullBoxReader {
public:
    Mp4StsdReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StsdReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);
    int readSampleEntry(const Mp4TableBox *stsd, const char *handlerType, uint32_t entryIndex,
                        size_t entryStructSize, Mp4AudioSampleEntryBox *outEntry, uint32_t *outErr);

private:
    Mp4AudioSampleEntryReader m_audioEntryReader; /* +12 */
};

Mp4StsdReader::Mp4StsdReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
    , m_audioEntryReader(io, streamIdx)
{
}

Mp4StsdReader::~Mp4StsdReader() {}

/* 0x81202408 */
int Mp4StsdReader::read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_fullHeaderSize + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *cntPtr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 4u, &mapErr);
    if (cntPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    uint32_t count = readBigEndian32(cntPtr);
    outBox->m_entryCount = count;
    if (count != 1u) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }

    const uint8_t *codecTag = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize + 8u, 4u, &mapErr);
    if (codecTag == NULL) {
        *outErr = mapErr;
        return 0;
    }

    if (sce_paf_memcmp(codecTag, "mp4a", 4u) == 0) {
        outBox->m_isParsed = 1;
        outBox->m_tableOffset = outBox->m_fullHeaderSize + 4u;
        *outErr = 0;
        return 1;
    }
    if (sce_paf_memcmp(codecTag, "samr", 4u) == 0 || sce_paf_memcmp(codecTag, "drms", 4u) == 0) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }

    *outErr = MP4_ERR_CORRUPT;
    return 0;
}

/* 0x81202550 */
int Mp4StsdReader::readSampleEntry(const Mp4TableBox *stsd, const char *handlerType, uint32_t entryIndex,
                                   size_t entryStructSize, Mp4AudioSampleEntryBox *outEntry, uint32_t *outErr)
{
    if (entryIndex == 0u || entryIndex > stsd->m_entryCount) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }
    if (sce_paf_memcmp(handlerType, "soun", 4u) != 0 || entryStructSize != 40u) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }
    if (m_audioEntryReader.read(stsd, (uint16_t)entryIndex, outEntry, outErr) == 0) {
        return 0;
    }
    *outErr = 0;
    return 1;
}

class Mp4StszBox : public Mp4FullBox {
public:
    Mp4StszBox();
    virtual ~Mp4StszBox();

    uint32_t m_fixedSampleSize; /* +32 */
    uint32_t m_sampleCount;     /* +36 */
    uint32_t m_tableOffset;     /* +40 */
};

Mp4StszBox::Mp4StszBox()
    : Mp4FullBox("stsz", 0u)
    , m_fixedSampleSize(0)
    , m_sampleCount(0)
    , m_tableOffset(0)
{
}

Mp4StszBox::~Mp4StszBox() {}

class Mp4StszReader : public Mp4FullBoxReader {
public:
    Mp4StszReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StszReader();

    int read(const Mp4BoxBase *parentBox, Mp4StszBox *outBox, uint32_t *outErr);
    uint32_t getSampleSize(const Mp4StszBox *stsz, uint32_t sampleIndex, uint32_t *outErr);
};

Mp4StszReader::Mp4StszReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4StszReader::~Mp4StszReader() {}

/* 0x812026B4 */
int Mp4StszReader::read(const Mp4BoxBase *parentBox, Mp4StszBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_fullHeaderSize + 8u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *hdr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 8u, &mapErr);
    if (hdr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    uint32_t fixedSize = readBigEndian32(hdr);
    uint32_t sampleCount = readBigEndian32(hdr + 4);
    if (fixedSize != 0u) {
        if (outBox->m_boxSize != outBox->m_fullHeaderSize + 8u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
    } else if (outBox->m_boxSize != outBox->m_fullHeaderSize + 4u * sampleCount + 8u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_fixedSampleSize = fixedSize;
    outBox->m_sampleCount = sampleCount;
    outBox->m_tableOffset = outBox->m_fullHeaderSize + 8u;
    outBox->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* 0x812027C0 */
uint32_t Mp4StszReader::getSampleSize(const Mp4StszBox *stsz, uint32_t sampleIndex, uint32_t *outErr)
{
    if (!stsz->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }
    if (sampleIndex == 0u || sampleIndex > stsz->m_sampleCount) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }
    if (stsz->m_fixedSampleSize != 0u) {
        *outErr = 0;
        return stsz->m_fixedSampleSize;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    uint32_t off = stsz->m_fileOffset + stsz->m_tableOffset + 4u * (sampleIndex - 1u);
    const uint8_t *ptr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
    if (ptr == NULL) {
        *outErr = mapErr;
        return 0;
    }
    *outErr = 0;
    return readBigEndian32(ptr);
}

class Mp4SttsReader : public Mp4FullBoxReader {
public:
    Mp4SttsReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4SttsReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);

    inline uint32_t readEntryField(const Mp4TableBox *stts, uint32_t entryIndex,
                                   uint32_t fieldOffset, uint32_t *outErr)
    {
        if (!stts->isParsed()) {
            *outErr = MP4_ERR_NOT_READY;
            return 0;
        }
        if (entryIndex == 0u || entryIndex > stts->m_entryCount) {
            *outErr = IO_ERR_BAD_PARAM;
            return 0;
        }

        uint32_t mapErr = 0xFFFFFFFFu;
        uint32_t off = stts->m_fileOffset + stts->m_tableOffset + 8u * (entryIndex - 1u) + fieldOffset;
        const uint8_t *ptr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
        if (ptr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        *outErr = 0;
        return readBigEndian32(ptr);
    }

    uint32_t getSampleCount(const Mp4TableBox *stts, uint32_t idx, uint32_t *err);
    uint32_t getSampleDelta(const Mp4TableBox *stts, uint32_t idx, uint32_t *err);
};

Mp4SttsReader::Mp4SttsReader(Mp4CachedIo *io, int streamIdx)
    : Mp4FullBoxReader(io, streamIdx)
{
}

Mp4SttsReader::~Mp4SttsReader() {}

/* 0x81202934 */
int Mp4SttsReader::read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr)
{
    if (!parentBox->isParsed()) {
        *outErr = MP4_ERR_BAD_PARENT;
        return 0;
    }
    if (readFullBoxHeader(parentBox, 1u, outBox, outErr) == 0) {
        return 0;
    }
    if (outBox->m_boxSize < outBox->m_fullHeaderSize + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t mapErr = 0xFFFFFFFFu;
    const uint8_t *cntPtr = (const uint8_t *)m_io->mapStreamRange(
        m_streamIdx, outBox->m_fileOffset + outBox->m_fullHeaderSize, 4u, &mapErr);
    if (cntPtr == NULL) {
        *outErr = mapErr;
        return 0;
    }

    uint32_t count = readBigEndian32(cntPtr);
    if (outBox->m_boxSize != outBox->m_fullHeaderSize + 8u * count + 4u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    outBox->m_entryCount = count;
    outBox->m_tableOffset = outBox->m_fullHeaderSize + 4u;
    outBox->m_isParsed = 1;
    *outErr = 0;
    return 1;
}

/* 0x81202A12 */
uint32_t Mp4SttsReader::getSampleCount(const Mp4TableBox *stts, uint32_t idx, uint32_t *err)
{
    return readEntryField(stts, idx, 0u, err);
}

/* 0x81202AB8 */
uint32_t Mp4SttsReader::getSampleDelta(const Mp4TableBox *stts, uint32_t idx, uint32_t *err)
{
    return readEntryField(stts, idx, 4u, err);
}

/* ===== Mp4SampleTableNavigator (0x81201454 - 0x81201D33) ===== */

class Mp4SampleTableNavigator {
public:
    Mp4SampleTableNavigator(Mp4SttsReader *sttsReader, Mp4StszReader *stszReader,
                            Mp4StscReader *stscReader, Mp4StcoReader *stcoReader);
    virtual ~Mp4SampleTableNavigator();

    void attachStts(Mp4TableBox *stts);
    void attachStsz(Mp4StszBox *stsz);
    void attachStsc(Mp4TableBox *stsc);
    void attachStco(Mp4TableBox *stco);

    int lookupChunkOffset(uint32_t chunkIndex, uint32_t *outOffset, uint32_t *outErr);
    int sumSampleSizes(uint32_t firstSample, uint32_t lastSample, uint32_t *outTotalBytes,
                       uint32_t *outErr);
    int lookupSampleChunk(uint32_t sampleIndex, uint32_t *outChunkIndex,
                          uint32_t *outFirstSampleInChunk, uint32_t *outSamplesPerChunk,
                          uint32_t *outSampleDescIndex, uint32_t *outErr);
    int lookupSampleLocation(uint32_t sampleIndex, uint32_t *outFileOffset,
                             uint32_t *outSampleSize, uint32_t *outSampleDescIndex,
                             uint32_t *outErr);
    int lookupSampleByTimestamp(uint32_t targetTimeTicks, uint32_t *outSampleIndex,
                                uint32_t *outErr);

private:
    Mp4SttsReader *m_sttsReader;      /* +4 */
    Mp4TableBox *m_sttsBox;           /* +8 */
    uint32_t m_sttsEntryIdx;          /* +12 */
    uint32_t m_sttsFirstSample;       /* +16 */
    uint32_t m_sttsStartTime;         /* +20 */
    uint32_t m_sttsSampleCount;       /* +24 */
    uint32_t m_sttsSampleDelta;       /* +28 */

    Mp4StszReader *m_stszReader;      /* +32 */
    Mp4StszBox *m_stszBox;            /* +36 */

    Mp4StscReader *m_stscReader;      /* +40 */
    Mp4TableBox *m_stscBox;           /* +44 */
    uint32_t m_stscEntryIdx;          /* +48 */
    uint32_t m_stscFirstChunk;        /* +52 */
    uint32_t m_stscSamplesPerChunk;   /* +56 */
    uint32_t m_stscSampleDescIdx;     /* +60 */
    uint32_t m_stscFirstSample;       /* +64 */
    uint32_t m_stscNextFirstChunk;    /* +68 */

    uint32_t m_cachedSizeSum;         /* +72 */
    uint32_t m_cachedSumFirst;        /* +76 */
    uint32_t m_cachedSumLast;         /* +80 */

    Mp4StcoReader *m_stcoReader;      /* +84 */
    Mp4TableBox *m_stcoBox;           /* +88 */
    uint32_t m_cachedChunkIdx;        /* +92 */
    uint32_t m_cachedChunkOffset;     /* +96 */
};

/* 0x81201454 */
Mp4SampleTableNavigator::Mp4SampleTableNavigator(Mp4SttsReader *sttsReader,
                                                 Mp4StszReader *stszReader,
                                                 Mp4StscReader *stscReader,
                                                 Mp4StcoReader *stcoReader)
    : m_sttsReader(sttsReader)
    , m_sttsBox(NULL)
    , m_sttsEntryIdx(0)
    , m_sttsFirstSample(0)
    , m_sttsStartTime(0)
    , m_sttsSampleCount(0)
    , m_sttsSampleDelta(0)
    , m_stszReader(stszReader)
    , m_stszBox(NULL)
    , m_stscReader(stscReader)
    , m_stscBox(NULL)
    , m_stscEntryIdx(0)
    , m_stscFirstChunk(0)
    , m_stscSamplesPerChunk(0)
    , m_stscSampleDescIdx(0)
    , m_stscFirstSample(0)
    , m_stscNextFirstChunk(0)
    , m_cachedSizeSum(0)
    , m_cachedSumFirst(0)
    , m_cachedSumLast(0)
    , m_stcoReader(stcoReader)
    , m_stcoBox(NULL)
    , m_cachedChunkIdx(0)
    , m_cachedChunkOffset(0)
{
}

/* 0x8120149C / 0x812014B0 */
Mp4SampleTableNavigator::~Mp4SampleTableNavigator()
{
}

/* 0x81201B46 */
void Mp4SampleTableNavigator::attachStts(Mp4TableBox *stts)
{
    m_sttsBox = stts;
    m_sttsEntryIdx = 0;
    m_sttsFirstSample = 0;
    m_sttsStartTime = 0;
    m_sttsSampleCount = 0;
    m_sttsSampleDelta = 0;
}

/* 0x81201D14 */
void Mp4SampleTableNavigator::attachStsz(Mp4StszBox *stsz)
{
    m_stszBox = stsz;
    m_cachedSizeSum = 0;
    m_cachedSumFirst = 0;
    m_cachedSumLast = 0;
}

/* 0x81201D20 */
void Mp4SampleTableNavigator::attachStsc(Mp4TableBox *stsc)
{
    m_stscBox = stsc;
    m_stscEntryIdx = 0;
    m_stscFirstChunk = 0;
    m_stscSamplesPerChunk = 0;
    m_stscSampleDescIdx = 0;
    m_stscFirstSample = 0;
    m_stscNextFirstChunk = 0;
}

void Mp4SampleTableNavigator::attachStco(Mp4TableBox *stco)
{
    m_stcoBox = stco;
    m_cachedChunkIdx = 0;
    m_cachedChunkOffset = 0;
}

/* 0x812014BE */
int Mp4SampleTableNavigator::lookupChunkOffset(uint32_t chunkIndex, uint32_t *outOffset,
                                               uint32_t *outErr)
{
    if (m_stcoReader == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }
    if (m_stcoBox == NULL || !m_stcoBox->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }
    if (chunkIndex == 0u || chunkIndex > m_stcoBox->m_entryCount) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }
    if (m_cachedChunkIdx == chunkIndex) {
        *outOffset = m_cachedChunkOffset;
        *outErr = 0;
        return 1;
    }

    *outOffset = m_stcoReader->getChunkOffset(m_stcoBox, chunkIndex, outErr);
    if (*outErr != 0u) {
        return 0;
    }
    m_cachedChunkIdx = chunkIndex;
    m_cachedChunkOffset = *outOffset;
    return 1;
}

/* 0x8120154C */
int Mp4SampleTableNavigator::sumSampleSizes(uint32_t firstSample, uint32_t lastSample,
                                            uint32_t *outTotalBytes, uint32_t *outErr)
{
    if (m_stszReader == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }
    if (m_stszBox == NULL || !m_stszBox->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }
    uint32_t totalSamples = m_stszBox->m_sampleCount;
    if (totalSamples == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }
    if (lastSample > totalSamples || firstSample > totalSamples
        || lastSample == 0u || firstSample == 0u || firstSample > lastSample) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }

    uint32_t sumBytes = 0;
    if (m_cachedSumFirst == firstSample && (m_cachedSumFirst + m_cachedSumLast) < 2u * lastSample) {
        sumBytes = m_cachedSizeSum;
        if (lastSample < m_cachedSumLast) {
            for (uint32_t cur = m_cachedSumLast; cur > lastSample; --cur) {
                uint32_t sz = m_stszReader->getSampleSize(m_stszBox, cur, outErr);
                if (*outErr != 0u) {
                    return 0;
                }
                if (sz == 0u) {
                    *outErr = MP4_ERR_CORRUPT;
                    return 0;
                }
                sumBytes -= sz;
            }
        } else if (lastSample > m_cachedSumLast) {
            for (uint32_t cur = m_cachedSumLast + 1u; cur <= lastSample; ++cur) {
                uint32_t sz = m_stszReader->getSampleSize(m_stszBox, cur, outErr);
                if (*outErr != 0u) {
                    return 0;
                }
                if (sz == 0u) {
                    *outErr = MP4_ERR_CORRUPT;
                    return 0;
                }
                sumBytes += sz;
            }
        }
    } else {
        for (uint32_t cur = firstSample; cur <= lastSample; ++cur) {
            uint32_t sz = m_stszReader->getSampleSize(m_stszBox, cur, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (sz == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            sumBytes += sz;
        }
    }

    if (firstSample != lastSample) {
        m_cachedSizeSum = sumBytes;
        m_cachedSumFirst = firstSample;
        m_cachedSumLast = lastSample;
    }
    *outTotalBytes = sumBytes;
    *outErr = 0;
    return 1;
}

/* 0x812016A0 */
int Mp4SampleTableNavigator::lookupSampleChunk(uint32_t sampleIndex, uint32_t *outChunkIndex,
                                               uint32_t *outFirstSampleInChunk,
                                               uint32_t *outSamplesPerChunk,
                                               uint32_t *outSampleDescIndex,
                                               uint32_t *outErr)
{
    if (m_stscReader == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }
    if (m_stscBox == NULL || !m_stscBox->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }

    uint32_t entryCount = m_stscBox->m_entryCount;
    if (entryCount == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t entryIdx = m_stscEntryIdx;
    uint32_t firstChunk = m_stscFirstChunk;
    uint32_t samplesPerChunk = m_stscSamplesPerChunk;
    uint32_t sampleDescIdx = m_stscSampleDescIdx;
    uint32_t firstSample = m_stscFirstSample;
    uint32_t nextFirstChunk = m_stscNextFirstChunk;

    if (entryIdx == 0u) {
        entryIdx = 1u;
        firstChunk = m_stscReader->getFirstChunk(m_stscBox, 1u, outErr);
        if (*outErr != 0u) {
            return 0;
        }
        if (firstChunk == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        samplesPerChunk = m_stscReader->getSamplesPerChunk(m_stscBox, 1u, outErr);
        if (*outErr != 0u) {
            return 0;
        }
        if (samplesPerChunk == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        sampleDescIdx = m_stscReader->getSampleDescIndex(m_stscBox, 1u, outErr);
        if (*outErr != 0u) {
            return 0;
        }
        if (sampleDescIdx == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        firstSample = 1u;
        if (entryCount == 1u) {
            nextFirstChunk = 0u;
        } else {
            nextFirstChunk = m_stscReader->getFirstChunk(m_stscBox, 2u, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (nextFirstChunk == 0u || firstChunk >= nextFirstChunk) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
        }
    }

    bool inRange = (sampleIndex >= firstSample)
                && (nextFirstChunk == 0u
                    || sampleIndex < firstSample + samplesPerChunk * (nextFirstChunk - firstChunk));

    if (sampleIndex < firstSample) {
        bool found = false;
        while (entryIdx > 1u) {
            --entryIdx;
            nextFirstChunk = firstChunk;
            firstChunk = m_stscReader->getFirstChunk(m_stscBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (nextFirstChunk == 0u || firstChunk >= nextFirstChunk) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            samplesPerChunk = m_stscReader->getSamplesPerChunk(m_stscBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (samplesPerChunk == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            sampleDescIdx = m_stscReader->getSampleDescIndex(m_stscBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (sampleDescIdx == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            firstSample -= samplesPerChunk * (nextFirstChunk - firstChunk);
            if (firstSample <= sampleIndex
                && sampleIndex < firstSample + samplesPerChunk * (nextFirstChunk - firstChunk)) {
                found = true;
                break;
            }
        }
        if (!found) {
            *outErr = MP4_ERR_NOT_FOUND;
            return 0;
        }
    } else if (!inRange) {
        bool found = false;
        while (entryIdx < entryCount) {
            ++entryIdx;
            firstSample = (nextFirstChunk != 0u)
                        ? (firstSample + samplesPerChunk * (nextFirstChunk - firstChunk))
                        : 0u;
            firstChunk = nextFirstChunk;
            samplesPerChunk = m_stscReader->getSamplesPerChunk(m_stscBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (samplesPerChunk == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            sampleDescIdx = m_stscReader->getSampleDescIndex(m_stscBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (sampleDescIdx == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            if (entryIdx + 1u <= entryCount) {
                nextFirstChunk = m_stscReader->getFirstChunk(m_stscBox, entryIdx + 1u, outErr);
                if (*outErr != 0u) {
                    return 0;
                }
                if (nextFirstChunk == 0u || firstChunk >= nextFirstChunk) {
                    *outErr = MP4_ERR_CORRUPT;
                    return 0;
                }
            } else {
                nextFirstChunk = 0u;
            }
            if (firstSample <= sampleIndex
                && (nextFirstChunk == 0u
                    || sampleIndex < firstSample + samplesPerChunk * (nextFirstChunk - firstChunk))) {
                found = true;
                break;
            }
        }
        if (!found) {
            *outErr = MP4_ERR_NOT_FOUND;
            return 0;
        }
    }

    uint32_t chunkDelta = (sampleIndex - firstSample) / samplesPerChunk;
    *outChunkIndex = firstChunk + chunkDelta;
    *outFirstSampleInChunk = firstSample + chunkDelta * samplesPerChunk;
    *outSamplesPerChunk = samplesPerChunk;
    *outSampleDescIndex = sampleDescIdx;
    *outErr = 0;

    m_stscEntryIdx = entryIdx;
    m_stscFirstChunk = firstChunk;
    m_stscSamplesPerChunk = samplesPerChunk;
    m_stscSampleDescIdx = sampleDescIdx;
    m_stscFirstSample = firstSample;
    m_stscNextFirstChunk = nextFirstChunk;
    return 1;
}

/* 0x81201A8E */
int Mp4SampleTableNavigator::lookupSampleLocation(uint32_t sampleIndex, uint32_t *outFileOffset,
                                                  uint32_t *outSampleSize,
                                                  uint32_t *outSampleDescIndex,
                                                  uint32_t *outErr)
{
    uint32_t chunkIdx = 0;
    uint32_t firstSampleInChunk = 0;
    uint32_t samplesPerChunk = 0;
    if (lookupSampleChunk(sampleIndex, &chunkIdx, &firstSampleInChunk,
                          &samplesPerChunk, outSampleDescIndex, outErr) == 0) {
        return 0;
    }

    uint32_t offsetWithinChunk = 0;
    if (firstSampleInChunk != sampleIndex
        && sumSampleSizes(firstSampleInChunk, sampleIndex - 1u, &offsetWithinChunk, outErr) == 0) {
        return 0;
    }
    if (sumSampleSizes(sampleIndex, sampleIndex, outSampleSize, outErr) == 0) {
        return 0;
    }

    uint32_t chunkFileOffset = 0;
    if (lookupChunkOffset(chunkIdx, &chunkFileOffset, outErr) == 0) {
        return 0;
    }

    *outFileOffset = chunkFileOffset + offsetWithinChunk;
    *outErr = 0;
    return 1;
}

/* 0x81201B56 */
int Mp4SampleTableNavigator::lookupSampleByTimestamp(uint32_t targetTimeTicks,
                                                     uint32_t *outSampleIndex,
                                                     uint32_t *outErr)
{
    if (m_sttsReader == NULL) {
        *outErr = 0xFFFFFFFFu;
        return 0;
    }
    if (m_sttsBox == NULL || !m_sttsBox->isParsed()) {
        *outErr = MP4_ERR_NOT_READY;
        return 0;
    }

    uint32_t entryCount = m_sttsBox->m_entryCount;
    if (entryCount == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    uint32_t entryIdx = m_sttsEntryIdx;
    uint32_t firstSample = m_sttsFirstSample;
    uint32_t startTime = m_sttsStartTime;
    uint32_t sampleCount = m_sttsSampleCount;
    uint32_t sampleDelta = m_sttsSampleDelta;

    if (entryIdx == 0u) {
        entryIdx = 1u;
        firstSample = 1u;
        sampleCount = m_sttsReader->getSampleCount(m_sttsBox, 1u, outErr);
        if (*outErr != 0u) {
            return 0;
        }
        if (sampleCount == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        sampleDelta = m_sttsReader->getSampleDelta(m_sttsBox, 1u, outErr);
        if (*outErr != 0u) {
            return 0;
        }
        if (sampleDelta == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        startTime = 0u;
    }

    bool found = (targetTimeTicks >= startTime
                  && targetTimeTicks < startTime + sampleCount * sampleDelta);
    if (!found) {
        uint32_t nextStart = startTime + sampleCount * sampleDelta;
        while (entryIdx < entryCount) {
            ++entryIdx;
            firstSample += sampleCount;
            startTime = nextStart;
            sampleCount = m_sttsReader->getSampleCount(m_sttsBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (sampleCount == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            sampleDelta = m_sttsReader->getSampleDelta(m_sttsBox, entryIdx, outErr);
            if (*outErr != 0u) {
                return 0;
            }
            if (sampleDelta == 0u) {
                *outErr = MP4_ERR_CORRUPT;
                return 0;
            }
            nextStart = startTime + sampleCount * sampleDelta;
            if (targetTimeTicks >= startTime || targetTimeTicks < nextStart) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        *outErr = MP4_ERR_NOT_FOUND;
        return 0;
    }

    m_sttsEntryIdx = entryIdx;
    m_sttsFirstSample = firstSample;
    m_sttsStartTime = startTime;
    m_sttsSampleCount = sampleCount;
    m_sttsSampleDelta = sampleDelta;
    *outSampleIndex = firstSample + (targetTimeTicks - startTime) / sampleDelta;
    *outErr = 0;
    return 1;
}

/* Bridges for musiccore_mp4_media.cpp */
struct StreamIoInterface;
struct Mp4BoxNode;
struct Mp4HdlrBox;
struct Mp4Mp4aBox;
struct Mp4EsdsHeaderBox;
struct Mp4DecoderConfigBox;
struct Mp4AudioSpecConfig;
struct Mp4StblBox;
struct Mp4StsdBox;
struct Mp4SttsBox;
struct Mp4StscBox;
struct Mp4StcoBox;
struct Mp4BoxReader;
struct Mp4StsdBoxReader;
struct Mp4SampleTable;

void mp4BoxReset(void *box)
{
    if (box != NULL)
        static_cast<Mp4BoxBase *>(box)->resetParsed();
}

unsigned int mp4BoxGetPayloadSize(const void *box)
{
    const Mp4BoxBase *b = static_cast<const Mp4BoxBase *>(box);
    return (b != NULL && b->m_boxSize >= b->m_headerSize) ? (b->m_boxSize - b->m_headerSize) : 0u;
}

unsigned int mp4BoxGetPayloadOffset(const void *box)
{
    const Mp4BoxBase *b = static_cast<const Mp4BoxBase *>(box);
    return (b != NULL) ? (b->m_fileOffset + b->m_headerSize) : 0u;
}

unsigned int mp4BoxGetHeaderSize(const void *box)
{
    const Mp4BoxBase *b = static_cast<const Mp4BoxBase *>(box);
    return (b != NULL) ? b->m_headerSize : 0u;
}

unsigned int mp4BoxGetEntryCount(const void *box)
{
    const Mp4TableBox *b = static_cast<const Mp4TableBox *>(box);
    return (b != NULL) ? b->m_entryCount : 0u;
}

unsigned int mp4BoxGetInnerOffset(const void *box)
{
    const Mp4BoxBase *b = static_cast<const Mp4BoxBase *>(box);
    return (b != NULL) ? b->getContentOffset() : 0u;
}

int mp4ChildBoxRead(Mp4BoxReader *reader, Mp4BoxNode *parent, void *outChild, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4ContainerReader *>(reader)->readChildContainer(
              reinterpret_cast<const Mp4BoxBase *>(parent),
              static_cast<Mp4ContainerBox *>(outChild), outErr)
        : 0;
}

void mp4MoovBoxInit(Mp4BoxNode *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("moov"); }
void mp4MoovBoxDestroy(Mp4BoxNode *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4MoovReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4MoovReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }
int mp4MoovReaderRead(Mp4BoxReader *reader, Mp4BoxNode *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4ContainerReader *>(reader)->readTopContainer(
              reinterpret_cast<Mp4ContainerBox *>(outBox), outErr)
        : 0;
}

void mp4MvhdBoxInit(Mp4MvhdBox *box) { new (box) Mp4MvhdBox(); }
void mp4MvhdBoxDestroy(Mp4MvhdBox *box) { if (box != NULL) box->~Mp4MvhdBox(); }
void mp4MvhdReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4MvhdBoxReader *>(reader)) Mp4MvhdBoxReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4MvhdReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4MvhdBoxReader *>(reader)->~Mp4MvhdBoxReader(); }
int mp4MvhdReaderRead(Mp4BoxReader *reader, Mp4BoxNode *moov, Mp4MvhdBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4MvhdBoxReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(moov), outBox, outErr)
        : 0;
}

void mp4TrakBoxInit(Mp4BoxNode *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("trak"); }
void mp4TrakBoxDestroy(Mp4BoxNode *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4TrakReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4TrakReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }
int mp4TrakReaderCount(Mp4BoxReader *reader, Mp4BoxNode *moov, unsigned int *outErr)
{
    return (reader != NULL)
        ? static_cast<int>(reinterpret_cast<Mp4ContainerReader *>(reader)->countTracks(
              reinterpret_cast<const Mp4BoxBase *>(moov), outErr))
        : 0;
}
int mp4TrakReaderGet(Mp4BoxReader *reader, Mp4BoxNode *moov, int index, Mp4BoxNode *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4ContainerReader *>(reader)->readTrackAt(
              reinterpret_cast<const Mp4BoxBase *>(moov), static_cast<uint16_t>(index + 1),
              reinterpret_cast<Mp4ContainerBox *>(outBox), outErr)
        : 0;
}

void mp4HdlrBoxInit(Mp4HdlrBox *box) { new (reinterpret_cast<Mp4TkhdBox *>(box)) Mp4TkhdBox(); }
void mp4HdlrBoxDestroy(Mp4HdlrBox *box) { if (box != NULL) reinterpret_cast<Mp4TkhdBox *>(box)->~Mp4TkhdBox(); }
int mp4HdlrIsSoundTrack(const Mp4HdlrBox *box) { return (box != NULL) ? reinterpret_cast<const Mp4TkhdBox *>(box)->isAudioTrackVolume() : 0; }
void mp4HdlrReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4TkhdBoxReader *>(reader)) Mp4TkhdBoxReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4HdlrReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4TkhdBoxReader *>(reader)->~Mp4TkhdBoxReader(); }
int mp4HdlrReaderRead(Mp4BoxReader *reader, Mp4BoxNode *trak, Mp4HdlrBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4TkhdBoxReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(trak),
              reinterpret_cast<Mp4TkhdBox *>(outBox), outErr)
        : 0;
}

void mp4MdiaBoxInit(Mp4BoxNode *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("mdia"); }
void mp4MdiaBoxDestroy(Mp4BoxNode *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4MdiaReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4MdiaReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }

void mp4MdhdBoxInit(Mp4MdhdBox *box) { new (box) Mp4MdhdBox(); }
void mp4MdhdBoxDestroy(Mp4MdhdBox *box) { if (box != NULL) box->~Mp4MdhdBox(); }
void mp4MdhdReaderInit(Mp4BoxReader *reader) { new (reinterpret_cast<Mp4MdhdBoxReader *>(reader)) Mp4MdhdBoxReader(NULL, 0); }
void mp4MdhdReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4MdhdBoxReader *>(reader)->~Mp4MdhdBoxReader(); }
int mp4MdhdReaderRead(Mp4BoxReader *reader, Mp4BoxNode *mdia, Mp4MdhdBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4MdhdBoxReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(mdia), outBox, outErr)
        : 0;
}
unsigned int mp4MdhdGetTimeScale(const Mp4MdhdBox *box) { return (box != NULL) ? box->m_timescale : 0u; }
unsigned int mp4MdhdGetDuration(const Mp4MdhdBox *box) { return (box != NULL) ? box->m_duration : 0u; }

void mp4MinfBoxInit(Mp4BoxNode *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("minf"); }
void mp4MinfBoxDestroy(Mp4BoxNode *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4MinfReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4MinfReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }

void mp4SmhdBoxInit(Mp4BoxNode *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("smhd"); }
void mp4SmhdBoxDestroy(Mp4BoxNode *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4SmhdReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4SmhdReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }

void mp4StblBoxInit(Mp4StblBox *box) { new (reinterpret_cast<Mp4ContainerBox *>(box)) Mp4ContainerBox("stbl"); }
void mp4StblBoxDestroy(Mp4StblBox *box) { if (box != NULL) reinterpret_cast<Mp4ContainerBox *>(box)->~Mp4ContainerBox(); }
void mp4StblReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4ContainerReader *>(reader)) Mp4ContainerReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4StblReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4ContainerReader *>(reader)->~Mp4ContainerReader(); }

void mp4StsdBoxInit(Mp4StsdBox *box) { new (reinterpret_cast<Mp4TableBox *>(box)) Mp4TableBox("stsd"); }
void mp4StsdBoxDestroy(Mp4StsdBox *box) { if (box != NULL) reinterpret_cast<Mp4TableBox *>(box)->~Mp4TableBox(); }
void mp4StsdReaderInit(Mp4StsdBoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4StsdReader *>(reader)) Mp4StsdReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4StsdReaderDestroy(Mp4StsdBoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4StsdReader *>(reader)->~Mp4StsdReader(); }
int mp4StsdReaderRead(Mp4StsdBoxReader *reader, Mp4StblBox *stbl, Mp4StsdBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4StsdReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(stbl),
              reinterpret_cast<Mp4TableBox *>(outBox), outErr)
        : 0;
}
int mp4StsdReaderFindEntry(Mp4StsdBoxReader *reader, Mp4StsdBox *stsd, const char *handler,
                           int index, int entrySize, Mp4SampleEntryBox *outEntry, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4StsdReader *>(reader)->readSampleEntry(
              reinterpret_cast<const Mp4TableBox *>(stsd), handler,
              static_cast<uint32_t>(index), static_cast<size_t>(entrySize),
              reinterpret_cast<Mp4AudioSampleEntryBox *>(outEntry), outErr)
        : 0;
}

void mp4SampleEntryInit(Mp4SampleEntryBox *box) { new (reinterpret_cast<Mp4AudioSampleEntryBox *>(box)) Mp4AudioSampleEntryBox(); }
void mp4SampleEntryDestroy(Mp4SampleEntryBox *box) { if (box != NULL) box->~Mp4SampleEntryBox(); }

void mp4Mp4aBoxInit(Mp4Mp4aBox *box) { new (reinterpret_cast<Mp4EsdsBox *>(box)) Mp4EsdsBox(); }
void mp4Mp4aBoxDestroy(Mp4Mp4aBox *box) { if (box != NULL) reinterpret_cast<Mp4EsdsBox *>(box)->~Mp4EsdsBox(); }
void mp4Mp4aReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4EsdsBoxReader *>(reader)) Mp4EsdsBoxReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4Mp4aReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4EsdsBoxReader *>(reader)->~Mp4EsdsBoxReader(); }
int mp4Mp4aReaderRead(Mp4BoxReader *reader, Mp4SampleEntryBox *entry, Mp4Mp4aBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4EsdsBoxReader *>(reader)->read(
              entry, reinterpret_cast<Mp4EsdsBox *>(outBox), outErr)
        : 0;
}

void mp4EsdsBoxInit(Mp4EsdsHeaderBox *box) { new (reinterpret_cast<Mp4EsDescriptor *>(box)) Mp4EsDescriptor(); }
void mp4EsdsBoxDestroy(Mp4EsdsHeaderBox *box) { if (box != NULL) reinterpret_cast<Mp4EsDescriptor *>(box)->~Mp4EsDescriptor(); }
void mp4EsdsReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4EsDescriptorReader *>(reader)) Mp4EsDescriptorReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4EsdsReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4EsDescriptorReader *>(reader)->~Mp4EsDescriptorReader(); }
int mp4EsdsReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                      Mp4EsdsHeaderBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4EsDescriptorReader *>(reader)->read(
              offset, size, reinterpret_cast<Mp4EsDescriptor *>(outBox), outErr)
        : 0;
}

void mp4DecoderConfigInit(Mp4DecoderConfigBox *box) { new (reinterpret_cast<Mp4DecoderConfigDescriptor *>(box)) Mp4DecoderConfigDescriptor(); }
void mp4DecoderConfigDestroy(Mp4DecoderConfigBox *box) { if (box != NULL) reinterpret_cast<Mp4DecoderConfigDescriptor *>(box)->~Mp4DecoderConfigDescriptor(); }
void mp4DecoderConfigReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4DecoderConfigReader *>(reader)) Mp4DecoderConfigReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4DecoderConfigReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4DecoderConfigReader *>(reader)->~Mp4DecoderConfigReader(); }
int mp4DecoderConfigReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                               Mp4DecoderConfigBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4DecoderConfigReader *>(reader)->read(
              offset, size, reinterpret_cast<Mp4DecoderConfigDescriptor *>(outBox), outErr)
        : 0;
}
int mp4DecoderConfigGetObjectType(const Mp4DecoderConfigBox *box)
{
    return (box != NULL) ? reinterpret_cast<const Mp4DecoderConfigDescriptor *>(box)->m_objectTypeIndication : 0;
}

void mp4AudioSpecConfigInit(Mp4AudioSpecConfig *cfg) { new (reinterpret_cast<Mp4AudioSpecificConfig *>(cfg)) Mp4AudioSpecificConfig(); }
void mp4AudioSpecConfigDestroy(Mp4AudioSpecConfig *cfg) { if (cfg != NULL) reinterpret_cast<Mp4AudioSpecificConfig *>(cfg)->~Mp4AudioSpecificConfig(); }
void mp4AudioSpecConfigReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4AudioSpecificConfigReader *>(reader)) Mp4AudioSpecificConfigReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4AudioSpecConfigReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4AudioSpecificConfigReader *>(reader)->~Mp4AudioSpecificConfigReader(); }
int mp4AudioSpecConfigReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                                 Mp4AudioSpecConfig *outCfg, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4AudioSpecificConfigReader *>(reader)->read(
              offset, size, reinterpret_cast<Mp4AudioSpecificConfig *>(outCfg), outErr)
        : 0;
}
int mp4AudioSpecConfigHasSbr(const Mp4AudioSpecConfig *cfg)
{
    return (cfg != NULL) ? reinterpret_cast<const Mp4AudioSpecificConfig *>(cfg)->m_sbrPresentFlag : 0;
}
int mp4AudioSpecConfigGetExtSampleRate(const Mp4AudioSpecConfig *cfg)
{
    return (cfg != NULL) ? static_cast<int>(reinterpret_cast<const Mp4AudioSpecificConfig *>(cfg)->m_extSampleRate) : 0;
}
int mp4AudioSpecConfigGetBaseSampleRate(const Mp4AudioSpecConfig *cfg)
{
    return (cfg != NULL) ? static_cast<int>(reinterpret_cast<const Mp4AudioSpecificConfig *>(cfg)->getSamplingFrequency()) : 0;
}
uint8_t mp4AudioSpecConfigGetChannels(const Mp4AudioSpecConfig *cfg)
{
    return (cfg != NULL) ? reinterpret_cast<const Mp4AudioSpecificConfig *>(cfg)->getChannelConfig() : 0u;
}
int mp4ValidateAudioTrack(const Mp4MdhdBox *mdhd, const Mp4StsdBox *stsd,
                          const Mp4DecoderConfigBox *decCfg, const Mp4AudioSpecConfig *asc)
{
    (void)mdhd;
    (void)decCfg;
    return isSupportedAacProfile(reinterpret_cast<const Mp4AudioSpecificConfig *>(stsd),
                                 reinterpret_cast<const Mp4AudioSpecificConfig *>(asc))
        ? 1
        : 0;
}

void mp4SttsBoxInit(Mp4SttsBox *box) { new (reinterpret_cast<Mp4TableBox *>(box)) Mp4TableBox("stts"); }
void mp4SttsBoxDestroy(Mp4SttsBox *box) { if (box != NULL) reinterpret_cast<Mp4TableBox *>(box)->~Mp4TableBox(); }
void mp4SttsReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4SttsReader *>(reader)) Mp4SttsReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4SttsReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4SttsReader *>(reader)->~Mp4SttsReader(); }
int mp4SttsReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4SttsBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4SttsReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(stbl),
              reinterpret_cast<Mp4TableBox *>(outBox), outErr)
        : 0;
}

void mp4StszBoxInit(Mp4StszBox *box) { new (box) Mp4StszBox(); }
void mp4StszBoxDestroy(Mp4StszBox *box) { if (box != NULL) box->~Mp4StszBox(); }
void mp4StszReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4StszReader *>(reader)) Mp4StszReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4StszReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4StszReader *>(reader)->~Mp4StszReader(); }
int mp4StszReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StszBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4StszReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(stbl), outBox, outErr)
        : 0;
}
unsigned int mp4StszGetSampleCount(const Mp4StszBox *box) { return (box != NULL) ? box->m_sampleCount : 0u; }

void mp4StscBoxInit(Mp4StscBox *box) { new (reinterpret_cast<Mp4TableBox *>(box)) Mp4TableBox("stsc"); }
void mp4StscBoxDestroy(Mp4StscBox *box) { if (box != NULL) reinterpret_cast<Mp4TableBox *>(box)->~Mp4TableBox(); }
void mp4StscReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4StscReader *>(reader)) Mp4StscReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4StscReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4StscReader *>(reader)->~Mp4StscReader(); }
int mp4StscReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StscBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4StscReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(stbl),
              reinterpret_cast<Mp4TableBox *>(outBox), outErr)
        : 0;
}

void mp4StcoBoxInit(Mp4StcoBox *box) { new (reinterpret_cast<Mp4TableBox *>(box)) Mp4TableBox("stco"); }
void mp4StcoBoxDestroy(Mp4StcoBox *box) { if (box != NULL) reinterpret_cast<Mp4TableBox *>(box)->~Mp4TableBox(); }
void mp4StcoReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode) { new (reinterpret_cast<Mp4StcoReader *>(reader)) Mp4StcoReader(reinterpret_cast<Mp4CachedIo *>(io), mode); }
void mp4StcoReaderDestroy(Mp4BoxReader *reader) { if (reader != NULL) reinterpret_cast<Mp4StcoReader *>(reader)->~Mp4StcoReader(); }
int mp4StcoReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StcoBox *outBox, unsigned int *outErr)
{
    return (reader != NULL)
        ? reinterpret_cast<Mp4StcoReader *>(reader)->read(
              reinterpret_cast<const Mp4BoxBase *>(stbl),
              reinterpret_cast<Mp4TableBox *>(outBox), outErr)
        : 0;
}

void mp4SampleTableInit(Mp4SampleTable *tbl, Mp4BoxReader *sttsRdr, Mp4BoxReader *stszRdr,
                        Mp4BoxReader *stscRdr, Mp4BoxReader *stcoRdr)
{
    new (reinterpret_cast<Mp4SampleTableNavigator *>(tbl)) Mp4SampleTableNavigator(
        reinterpret_cast<Mp4SttsReader *>(sttsRdr),
        reinterpret_cast<Mp4StszReader *>(stszRdr),
        reinterpret_cast<Mp4StscReader *>(stscRdr),
        reinterpret_cast<Mp4StcoReader *>(stcoRdr));
}
void mp4SampleTableDestroy(Mp4SampleTable *tbl) { if (tbl != NULL) reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->~Mp4SampleTableNavigator(); }
void mp4SampleTableSetStts(Mp4SampleTable *tbl, Mp4SttsBox *box) { if (tbl != NULL) reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->attachStts(reinterpret_cast<Mp4TableBox *>(box)); }
void mp4SampleTableSetStsz(Mp4SampleTable *tbl, Mp4StszBox *box) { if (tbl != NULL) reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->attachStsz(box); }
void mp4SampleTableSetStsc(Mp4SampleTable *tbl, Mp4StscBox *box) { if (tbl != NULL) reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->attachStsc(reinterpret_cast<Mp4TableBox *>(box)); }
void mp4SampleTableSetStco(Mp4SampleTable *tbl, Mp4StcoBox *box) { if (tbl != NULL) reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->attachStco(reinterpret_cast<Mp4TableBox *>(box)); }
int mp4SampleTableGetSample(Mp4SampleTable *tbl, unsigned int sampleIdx,
                            int *outOffset, unsigned int *outSize, int *outTime, int *outErr)
{
    uint32_t fileOff = 0;
    uint32_t sampleSz = 0;
    uint32_t descIdx = 0;
    uint32_t err = 0;
    int ret = (tbl != NULL)
        ? reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->lookupSampleLocation(
              sampleIdx, &fileOff, &sampleSz, &descIdx, &err)
        : 0;
    if (outOffset != NULL) *outOffset = static_cast<int>(fileOff);
    if (outSize != NULL) *outSize = sampleSz;
    if (outTime != NULL) *outTime = static_cast<int>(descIdx);
    if (outErr != NULL) *outErr = static_cast<int>(err);
    return ret;
}
int mp4SampleTableFindByTime(Mp4SampleTable *tbl, unsigned int targetTicks,
                             unsigned int *outSampleIdx, int *outErr)
{
    uint32_t sampleIdx = 0;
    uint32_t err = 0;
    int ret = (tbl != NULL)
        ? reinterpret_cast<Mp4SampleTableNavigator *>(tbl)->lookupSampleByTimestamp(
              targetTicks, &sampleIdx, &err)
        : 0;
    if (outSampleIdx != NULL) *outSampleIdx = sampleIdx;
    if (outErr != NULL) *outErr = static_cast<int>(err);
    return ret;
}

} /* namespace MusicCore */
} /* namespace sce */
