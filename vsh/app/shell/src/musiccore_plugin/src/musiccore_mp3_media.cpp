/*
 * SceShell musiccore_plugin - Mp3MediaInfo, Mp3StreamParser, and MPEG-1/2/2.5
 * Layer-3 frame header / Xing / VBRI utilities.
 * Original address range (3.65-CEX): 0x811EA894 - 0x811EBA6F
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_mp3_readers.h"

extern "C" {
int sce_paf_memcmp(const void *lhs, const void *rhs, size_t len);
void *sce_paf_memcpy(void *dst, const void *src, size_t len);
}

namespace sce {
namespace MusicCore {

extern void *allocFromHeap(size_t size);
extern bool checkCodecServiceReady();
extern int mapMp3ParserError(int parserErr);

static const uint32_t FOURCC_MP3 = 0x2033504Du;

static const int MP3_ERR_UNSUPPORTED = -2144862186;  /* 0x80280016 */
static const int MP3_ERR_NO_FRAME = -2144862185;     /* 0x80280017 */
static const int MP3_ERR_EOF = -2144862184;          /* 0x80280018 */
static const int MP3_ERR_NOT_OPEN = -2146394107;     /* 0x8010A005 */
static const int MP3_ERR_READ_IO = -2146394108;      /* 0x8010A004 */
static const int MP3_ERR_OPEN_IO = -2146394109;      /* 0x8010A003 */
static const int MP3_ERR_BAD_SEEK = -2146390031;     /* 0x8010AFF1 */

/* MPEG Layer III bitrate & sample-rate tables (0x81512F24 - 0x81512FB8) */
static const int kMpeg1Layer3Bitrates[16] = {
    0, 32000, 40000, 48000, 56000, 64000, 80000, 96000,
    112000, 128000, 160000, 192000, 224000, 256000, 320000, 0
};

static const uint16_t kMpeg1SampleRates[4] = {
    44100, 48000, 32000, 0
};

static const int kMpeg2Layer3Bitrates[16] = {
    0, 8000, 16000, 24000, 32000, 40000, 48000, 56000,
    64000, 80000, 96000, 112000, 128000, 144000, 160000, 0
};

static const uint16_t kMpeg2SampleRates[4] = {
    22050, 24000, 16000, 0
};

static const uint16_t kMpeg25SampleRates[4] = {
    11025, 12000, 8000, 0
};

/* 0x811EBA14 */
uint8_t makeBitMask(uint8_t bitCount)
{
    return (uint8_t)((1u << bitCount) - 1u);
}

/* 0x811EBA20 */
uint32_t readBigEndian32(const uint8_t *bytes)
{
    uint32_t acc = 0;
    for (unsigned int i = 0; i < 4u; ++i) {
        acc = (acc << 8) | bytes[i];
    }
    return acc;
}

/* 0x811EBA3A */
uint32_t readBigEndian24(const uint8_t *bytes)
{
    uint32_t acc = 0;
    for (unsigned int i = 0; i < 3u; ++i) {
        acc = (acc << 8) | bytes[i];
    }
    return acc;
}

/* 0x811EBA54 */
uint16_t readBigEndian16(const uint8_t *bytes)
{
    uint32_t acc = 0;
    for (unsigned int i = 0; i < 2u; ++i) {
        acc = (acc << 8) | bytes[i];
    }
    return (uint16_t)acc;
}

/* 0x811EB74C */
int validateMpegLayer3Header(uint32_t rawHeader, int *outReason)
{
    if ((rawHeader & 0xFFE00000u) != 0xFFE00000u) {
        *outReason = 1;
        return 0;
    }
    uint32_t versionBits = (rawHeader >> 19) & 3u;
    uint32_t layerBits = (rawHeader >> 17) & 3u;
    if (versionBits == 1u || layerBits != 1u) {
        *outReason = 2;
        return 0;
    }
    uint32_t bitrateIdx = (rawHeader >> 12) & 0xFu;
    uint32_t sampleRateIdx = (rawHeader >> 10) & 3u;
    uint32_t emphasis = rawHeader & 3u;
    if (bitrateIdx == 15u || bitrateIdx == 0u || sampleRateIdx == 3u || emphasis == 2u) {
        *outReason = 1;
        return 0;
    }
    *outReason = 0;
    return 1;
}

/* 0x811EB7B6 */
int getMpegLayer3Bitrate(uint32_t rawHeader)
{
    if (((rawHeader >> 17) & 3u) != 1u) {
        return 0;
    }
    int versionCode = 4 - (int)((rawHeader >> 19) & 3u);
    uint32_t bitrateIdx = (rawHeader >> 12) & 0xFu;
    if (versionCode == 1) {
        return kMpeg1Layer3Bitrates[bitrateIdx];
    }
    if (versionCode == 2 || versionCode == 4) {
        return kMpeg2Layer3Bitrates[bitrateIdx];
    }
    return 0;
}

/* 0x811EB816 */
int getMpegLayer3SampleRate(uint32_t rawHeader)
{
    if (((rawHeader >> 17) & 3u) != 1u) {
        return 0;
    }
    int versionCode = 4 - (int)((rawHeader >> 19) & 3u);
    uint32_t rateIdx = (rawHeader >> 10) & 3u;
    if (versionCode == 1) {
        return kMpeg1SampleRates[rateIdx];
    }
    if (versionCode == 2) {
        return kMpeg2SampleRates[rateIdx];
    }
    if (versionCode == 4) {
        return kMpeg25SampleRates[rateIdx];
    }
    return 0;
}

/* 0x811EB876 */
int getMpegLayer3SamplesPerFrame(uint32_t rawHeader)
{
    if (((rawHeader >> 17) & 3u) != 1u) {
        return 0;
    }
    int versionCode = 4 - (int)((rawHeader >> 19) & 3u);
    if (versionCode == 1) {
        return 1152;
    }
    if (versionCode == 2 || versionCode == 4) {
        return 576;
    }
    return 0;
}

/* 0x811EB8B8 */
unsigned int getMpegLayer3FrameSize(uint32_t rawHeader)
{
    unsigned int padding = ((rawHeader & 0x200u) != 0u) ? 1u : 0u;
    unsigned int bitrate = (unsigned int)getMpegLayer3Bitrate(rawHeader);
    unsigned int samples = (unsigned int)getMpegLayer3SamplesPerFrame(rawHeader);
    unsigned int rate = (unsigned int)getMpegLayer3SampleRate(rawHeader);
    return padding + ((samples * bitrate) / rate) / 8u;
}

/* 0x811EB8F2 */
int hasSameMpegSampleRate(uint32_t lhsHeader, uint32_t rhsHeader)
{
    return (getMpegLayer3SampleRate(lhsHeader) == getMpegLayer3SampleRate(rhsHeader)) ? 1 : 0;
}

/* 0x811EB942 */
MpegFrameHeader::MpegFrameHeader() : rawBits(0), isValid(0) {}

/* 0x811EB94A */
int MpegFrameHeader::parse(uint32_t headerBits)
{
    int reason = 0xFFFF;
    if (validateMpegLayer3Header(headerBits, &reason) == 0) {
        return 0;
    }
    rawBits = headerBits;
    isValid = 1;
    return 1;
}

/* 0x811EB98C */
int MpegFrameHeader::getBitrate() const
{
    return getMpegLayer3Bitrate(rawBits);
}

/* 0x811EB996 */
int MpegFrameHeader::getSampleRate() const
{
    return getMpegLayer3SampleRate(rawBits);
}

/* 0x811EB9A0 */
int XingHeaderData::parse(const void *src)
{
    if (sce_paf_memcmp(src, "Xing", 4u) != 0 && sce_paf_memcmp(src, "Info", 4u) != 0) {
        return 0;
    }
    sce_paf_memcpy(rawBytes, src, sizeof(rawBytes));
    isValid = 1;
    return 1;
}

/* 0x811EB9E2 */
int VbriHeaderData::parse(const void *src)
{
    if (sce_paf_memcmp(src, "VBRI", 4u) != 0) {
        return 0;
    }
    sce_paf_memcpy(rawBytes, src, sizeof(rawBytes));
    isValid = 1;
    return 1;
}

/* 0x811EB910 */
uint32_t VbriHeaderData::getFrameCount() const
{
    return readBigEndian32(rawBytes + 14);
}

/* 0x811EB91A */
uint16_t VbriHeaderData::getTocEntryCount() const
{
    return readBigEndian16(rawBytes + 18);
}

/* 0x811EB924 */
uint16_t VbriHeaderData::getTocScaleFactor() const
{
    return readBigEndian16(rawBytes + 20);
}

/* 0x811EB92E */
uint16_t VbriHeaderData::getTocEntrySize() const
{
    return readBigEndian16(rawBytes + 22);
}

/* 0x811EB938 */
uint16_t VbriHeaderData::getTocFramesPerEntry() const
{
    return readBigEndian16(rawBytes + 24);
}

struct StreamIoInterface;
extern StreamIoInterface *createLocalCacheIo(const char *path, int chunkSize, int blockCount, int readAhead);

struct TrackMetadataResult {
    int channels;
    uint8_t hasChannels;
    uint8_t padChannels[3];
    int bitRate;
    uint8_t hasBitRate;
    uint8_t padBitRate[3];
    unsigned int durationMs;
    uint8_t hasDuration;
    uint8_t padDuration[3];
    int sampleRate;
    uint8_t hasSampleRate;
    uint8_t padSampleRate[3];
    int codecProfile;
    uint8_t hasCodecProfile;
    uint8_t padCodecProfile[3];
    int codecSubType;
    uint8_t hasCodecSubType;
    uint8_t padCodecSubType[3];
};

struct Mp3ParserInterface {
    virtual ~Mp3ParserInterface();
    virtual int readFirstFrame(Mp3FramePacket *outPacket, int *outErr) = 0;
    virtual int readNextFrame(Mp3FramePacket *outPacket, int *outErr) = 0;
    virtual int seekToTimeMs(unsigned int targetMs, Mp3FramePacket *outPacket, int *outErr) = 0;
};

class Mp3StreamParser : public Mp3ParserInterface, public Mp3FrameRecord {
public:
    CachedStreamIo *ioStream;
    int openStatus;
    Id3v1Reader id3v1Reader;
    Id3v2Reader id3v2Reader;
    Mp3FrameScanner frameScanner;
    Mp3FirstFrameFinder firstFrame;
    Mp3NextFrameLocator nextFrameIter;
    Mp3CbrSeeker cbrSeeker;
    XingTagReader xingReader;
    Mp3XingSeeker xingSeeker;
    VbriTagReader vbriReader;
    Mp3VbriSeeker vbriSeeker;
    Mp3SeekRouter compositeSeeker;
    Mp3FrameReader packetReader;
    int audioDataSize;

    explicit Mp3StreamParser(CachedStreamIo *io);
    virtual ~Mp3StreamParser();
    int openStream();
    int getStreamPosition();
    int setIoHint(int mode);
    int computeAudioDataSize(int *outErr);
    int getDurationMs(unsigned int *outDurationMs, int *outErr);
    int getSampleRate(int *outRate, int *outErr);
    int getSamplesPerFrame(uint16_t *outSamples, int *outErr);
    virtual int readFirstFrame(Mp3FramePacket *outPacket, int *outErr);
    virtual int readNextFrame(Mp3FramePacket *outPacket, int *outErr);
    virtual int seekToTimeMs(unsigned int targetMs, Mp3FramePacket *outPacket, int *outErr);
    int checkFrameReadable(const Mp3FramePacket *packet, int *outErr);
};

/* 0x811EAB1C */
Mp3StreamParser::Mp3StreamParser(CachedStreamIo *io)
    : ioStream(io),
      openStatus(-1),
      id3v1Reader(io),
      id3v2Reader(io),
      frameScanner(io),
      firstFrame(io, &id3v2Reader, &frameScanner),
      nextFrameIter(&frameScanner),
      cbrSeeker(&firstFrame, &frameScanner),
      xingReader(io, &firstFrame),
      xingSeeker(&firstFrame, &xingReader, &frameScanner),
      vbriReader(io, &firstFrame),
      vbriSeeker(&firstFrame, &vbriReader, &frameScanner),
      compositeSeeker(&cbrSeeker, &xingSeeker, &vbriSeeker),
      packetReader(io),
      audioDataSize(-1)
{
}

/* 0x811EABF2 / 0x811EAC98 / 0x811EB72E / 0x811EB73C */
Mp3StreamParser::~Mp3StreamParser()
{
    if (ioStream != NULL && ioStream->isOpen() == 1 && ioStream->close() >= 0) {
        openStatus = -1;
    }
}

/* 0x811EACA8 */
int Mp3StreamParser::openStream()
{
    if (ioStream == NULL) {
        return -1;
    }
    if (openStatus >= 0) {
        return openStatus;
    }
    int ret = ioStream->open();
    if (ret >= 0) {
        openStatus = ret;
    }
    return ret;
}

/* 0x811EACD0 */
int Mp3StreamParser::getStreamPosition()
{
    if (ioStream == NULL) {
        return 0;
    }
    return reinterpret_cast<int>(ioStream->getPath());
}

/* 0x811EACE2 */
int Mp3StreamParser::setIoHint(int mode)
{
    if (ioStream == NULL) {
        return 0;
    }
    switch (mode) {
    case 4:
        ioStream->setCacheHint(3);
        return 1;
    case 2:
        ioStream->setCacheHint(2);
        return 1;
    case 1:
        ioStream->setCacheHint(1);
        return 1;
    case 0:
        ioStream->setCacheHint(0);
        return 1;
    default:
        return 0;
    }
}

/* 0x811EAD36 */
int Mp3StreamParser::computeAudioDataSize(int *outErr)
{
    int overhead = 0;
    if (*id3v2Reader.getHeaderPresencePtr() == 255 && id3v2Reader.readTag(0u, outErr) == 0) {
        return 0;
    }
    if (*id3v2Reader.getHeaderPresencePtr() == 1) {
        overhead = (int)id3v2Reader.getHeaderPtr()->getTagBodySize() + 10;
    }
    if (*id3v1Reader.getTagStatePtr() == 255 && id3v1Reader.probe(outErr) == 0) {
        return 0;
    }
    if (*id3v1Reader.getTagStatePtr() == 1) {
        overhead += 128;
    }
    int totalSize = ioStream->getFileSize();
    if (totalSize < 0) {
        *outErr = totalSize;
        return 0;
    }
    audioDataSize = totalSize - overhead;
    return 1;
}

/* 0x811EADCC */
int Mp3StreamParser::getDurationMs(unsigned int *outDurationMs, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if (audioDataSize < 0 && computeAudioDataSize(outErr) == 0) {
        return 0;
    }
    if (firstFrame.locateFirstFrame(-1, outErr) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }

    MpegFrameHeader *hdr = firstFrame.getFirstHeaderMut();
    unsigned int samplesPerFrame = (unsigned int)getMpegLayer3SamplesPerFrame(hdr->rawBits);
    unsigned int sampleRate = (unsigned int)hdr->getSampleRate();

    if (xingReader.readHeader(audioDataSize, outErr) != 0) {
        const uint8_t *xingRaw = xingReader.getHeader()->rawBytes;
        if ((xingRaw[7] & 2u) != 0u && (xingRaw[7] & 1u) != 0u) {
            uint32_t totalFrames = readBigEndian32(xingRaw + 8);
            unsigned int ms = (unsigned int)((((float)totalFrames * (float)samplesPerFrame) / (float)sampleRate) * 1000.0f);
            *outDurationMs = ms;
            if (ms != 0u) {
                *outErr = 0;
                return 1;
            }
        }
    }

    if (vbriReader.readHeader(audioDataSize, outErr) != 0) {
        uint32_t totalFrames = vbriReader.getHeader()->getFrameCount();
        unsigned int ms = (unsigned int)((((float)totalFrames * (float)samplesPerFrame) / (float)sampleRate) * 1000.0f);
        *outDurationMs = ms;
        if (ms != 0u) {
            *outErr = 0;
            return 1;
        }
    }

    int payloadBytes = audioDataSize - firstFrame.getFirstFrameOffset();
    unsigned int bitrate = (unsigned int)hdr->getBitrate();
    *outDurationMs = (unsigned int)((((float)payloadBytes * 8.0f) / (float)bitrate) * 1000.0f);
    *outErr = 0;
    return 1;
}

/* 0x811EAFB4 */
int Mp3StreamParser::getSampleRate(int *outRate, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if (audioDataSize < 0 && computeAudioDataSize(outErr) == 0) {
        return 0;
    }
    if (firstFrame.locateFirstFrame(-1, outErr) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }
    *outRate = firstFrame.getFirstHeaderMut()->getSampleRate();
    *outErr = 0;
    return 1;
}

/* 0x811EB034 */
int Mp3StreamParser::getSamplesPerFrame(uint16_t *outSamples, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if (audioDataSize < 0 && computeAudioDataSize(outErr) == 0) {
        return 0;
    }
    if (firstFrame.locateFirstFrame(-1, outErr) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }
    *outSamples = (uint16_t)getMpegLayer3SamplesPerFrame(firstFrame.getFirstHeaderMut()->rawBits);
    *outErr = 0;
    return 1;
}

/* 0x811EB0B6 */
int Mp3StreamParser::readFirstFrame(Mp3FramePacket *outPacket, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if ((audioDataSize < 0 && computeAudioDataSize(outErr) == 0) ||
        firstFrame.locateFirstFrame(-1, outErr) == 0) {
        return 0;
    }

    int firstOff = firstFrame.getFirstFrameOffset();
    MpegFrameHeader *firstHdr = firstFrame.getFirstHeaderMut();
    int foundOffset = -1;
    MpegFrameHeader foundHeader;
    uint32_t endFlags = 0;
    int leadingSkip = 0;

    if (xingReader.readHeader(audioDataSize, outErr) != 0) {
        int searchStart = firstOff + (int)getMpegLayer3FrameSize(firstHdr->rawBits);
        int searchEnd = searchStart + 1441;
        if (frameScanner.findSyncFrame(searchStart, searchEnd, &firstHdr->rawBits, 3,
                                       audioDataSize, &foundOffset, &foundHeader,
                                       &endFlags, outErr) == 0) {
            return 0;
        }
        leadingSkip = (int)xingReader.getEncoderDelay(audioDataSize);
        int spf = getMpegLayer3SamplesPerFrame(foundHeader.rawBits);
        if (leadingSkip > spf) {
            leadingSkip = spf;
        }
        if (leadingSkip == 0) {
            leadingSkip = spf >> 1;
        }
    } else if (vbriReader.readHeader(audioDataSize, outErr) != 0) {
        int searchStart = firstOff + (int)getMpegLayer3FrameSize(firstHdr->rawBits);
        int searchEnd = searchStart + 1441;
        if (frameScanner.findSyncFrame(searchStart, searchEnd, &firstHdr->rawBits, 3,
                                       audioDataSize, &foundOffset, &foundHeader,
                                       &endFlags, outErr) == 0) {
            return 0;
        }
        leadingSkip = getMpegLayer3SamplesPerFrame(foundHeader.rawBits) >> 1;
    } else {
        foundOffset = firstOff;
        foundHeader = *firstHdr;
        leadingSkip = getMpegLayer3SamplesPerFrame(foundHeader.rawBits) >> 1;
    }

    unsigned int frameSize = getMpegLayer3FrameSize(foundHeader.rawBits);
    int sampleRate = foundHeader.getSampleRate();
    int spf = getMpegLayer3SamplesPerFrame(foundHeader.rawBits);
    if (packetReader.readFrameData(foundOffset, frameSize, (uint16_t)sampleRate, (uint16_t)spf,
                                   (uint8_t)(foundHeader.rawBits & 3u),
                                   (uint8_t)((foundHeader.rawBits >> 6) & 3u),
                                   outPacket, outErr) == 0) {
        return 0;
    }

    m_fileOffset = foundOffset;
    m_header = foundHeader;
    outPacket->leadingSkipSamples = (uint16_t)leadingSkip;
    outPacket->trailingSkipSamples = 0;
    return 1;
}

/* 0x811EB38E */
int Mp3StreamParser::readNextFrame(Mp3FramePacket *outPacket, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if (audioDataSize < 0 && computeAudioDataSize(outErr) == 0) {
        return 0;
    }

    int nextOffset = -1;
    MpegFrameHeader nextHeader;
    uint32_t endKind = 0;

    if (nextFrameIter.locateNextFrame(this, audioDataSize,
                                      &nextOffset, &nextHeader, &endKind, outErr) == 0) {
        return 0;
    }

    unsigned int frameSize = getMpegLayer3FrameSize(nextHeader.rawBits);
    int sampleRate = nextHeader.getSampleRate();
    int spf = getMpegLayer3SamplesPerFrame(nextHeader.rawBits);
    if (packetReader.readFrameData(nextOffset, frameSize, (uint16_t)sampleRate, (uint16_t)spf,
                                   (uint8_t)(nextHeader.rawBits & 3u),
                                   (uint8_t)((nextHeader.rawBits >> 6) & 3u),
                                   outPacket, outErr) == 0) {
        return 0;
    }

    if (xingReader.readHeader(audioDataSize, outErr) == 0) {
        outPacket->trailingSkipSamples = 0;
    } else if (endKind == 0u) {
        int pad = (int)xingReader.getEncoderPadding(audioDataSize);
        outPacket->trailingSkipSamples = (uint16_t)((pad > spf) ? spf : pad);
    } else if (endKind == 1u) {
        if (xingReader.readHeader(audioDataSize, outErr) != 0) {
            int pad = (int)xingReader.getEncoderPadding(audioDataSize);
            if (pad <= spf) {
                outPacket->trailingSkipSamples = (uint16_t)pad;
            } else {
                int rem = pad - spf;
                outPacket->trailingSkipSamples = (uint16_t)((rem > spf) ? spf : rem);
            }
        }
    } else {
        outPacket->trailingSkipSamples = 0;
    }

    outPacket->leadingSkipSamples = 0;
    m_fileOffset = nextOffset;
    m_header = nextHeader;
    return 1;
}

/* 0x811EB548 */
int Mp3StreamParser::seekToTimeMs(unsigned int targetMs, Mp3FramePacket *outPacket, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP3_ERR_NOT_OPEN;
        return 0;
    }
    if (targetMs > 0x7FFFFFFFu) {
        *outErr = MP3_ERR_BAD_SEEK;
        return 0;
    }
    if (audioDataSize < 0 && computeAudioDataSize(outErr) == 0) {
        return 0;
    }

    int seekOffset = -1;
    MpegFrameHeader seekHeader;
    bool atEnd = false;

    if (compositeSeeker.seekToMs(targetMs, audioDataSize,
                                 &seekOffset, &seekHeader, &atEnd, outErr) == 0) {
        return 0;
    }

    unsigned int frameSize = getMpegLayer3FrameSize(seekHeader.rawBits);
    int sampleRate = seekHeader.getSampleRate();
    int spf = getMpegLayer3SamplesPerFrame(seekHeader.rawBits);
    if (packetReader.readFrameData(seekOffset, frameSize, (uint16_t)sampleRate, (uint16_t)spf,
                                   (uint8_t)(seekHeader.rawBits & 3u),
                                   (uint8_t)((seekHeader.rawBits >> 6) & 3u),
                                   outPacket, outErr) == 0) {
        return 0;
    }

    if (atEnd && xingReader.readHeader(audioDataSize, outErr) != 0) {
        int pad = (int)xingReader.getEncoderPadding(audioDataSize);
        outPacket->trailingSkipSamples = (uint16_t)((pad > spf) ? spf : pad);
    } else {
        outPacket->trailingSkipSamples = 0;
    }

    outPacket->leadingSkipSamples = 0;
    m_fileOffset = seekOffset;
    m_header = seekHeader;
    return 1;
}

/* 0x811EB6CC */
int Mp3StreamParser::checkFrameReadable(const Mp3FramePacket *packet, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    int ioErr = -1;
    if (ioStream->checkRangeReadable(packet->packetSize, &ioErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outErr = ioErr;
    return 0;
}

class Mp3MediaInfo : public MediaInfoBase {
public:
    int ioMode;
    Mp3StreamParser *parser;
    CachedStreamIo *ioStream;

    Mp3MediaInfo(const char *path, int mode);
    virtual ~Mp3MediaInfo();
    virtual int parseMetadata(TrackMetadataResult *outMeta);
};

/* 0x811EA894 */
Mp3MediaInfo::Mp3MediaInfo(const char *path, int mode)
    : MediaInfoBase(path, FOURCC_MP3),
      ioMode(mode),
      parser(NULL),
      ioStream(NULL)
{
    if (mode == 16) {
        ioStream = reinterpret_cast<CachedStreamIo *>(createLocalCacheIo(m_path, 0x4000, 32, 1));
        if (ioStream != NULL) {
            void *mem = allocFromHeap(sizeof(Mp3StreamParser));
            if (mem != NULL) {
                parser = new (mem) Mp3StreamParser(ioStream);
            }
            if (parser == NULL) {
                delete ioStream;
                ioStream = NULL;
            }
        }
    }
}

/* 0x811EA924 / 0x811EA962 */
Mp3MediaInfo::~Mp3MediaInfo()
{
    if (parser != NULL) {
        delete parser;
        parser = NULL;
    }
    if (ioStream != NULL) {
        delete ioStream;
        ioStream = NULL;
    }
}

/* 0x811EA9A6 */
int Mp3MediaInfo::parseMetadata(TrackMetadataResult *outMeta)
{
    outMeta->hasCodecSubType = 0;
    outMeta->hasCodecProfile = 0;
    outMeta->hasSampleRate = 0;
    outMeta->hasDuration = 0;
    outMeta->hasBitRate = 0;
    outMeta->hasChannels = 0;

    if (parser == NULL || !checkCodecServiceReady()) {
        return 33;
    }
    if (parser->openStream() != 0) {
        return 17;
    }

    Mp3FramePacket firstPacket;
    int parserErr = 0;
    if (parser->readFirstFrame(&firstPacket, &parserErr) == 1) {
        if (firstPacket.channelMode <= 2u) {
            outMeta->channels = 2;
            outMeta->hasChannels = 1;
        } else if (firstPacket.channelMode <= 3u) {
            outMeta->channels = 1;
            outMeta->hasChannels = 1;
        } else {
            parserErr = -1;
        }

        if (parserErr == 0 && parser->checkFrameReadable(&firstPacket, &parserErr) == 1) {
            int durOk = parser->getDurationMs(&outMeta->durationMs, &parserErr);
            outMeta->hasDuration = (uint8_t)durOk;
            if (durOk == 1) {
                int rateOk = parser->getSampleRate(&outMeta->sampleRate, &parserErr);
                outMeta->hasSampleRate = (uint8_t)rateOk;
                if (rateOk == 1) {
                    outMeta->codecProfile = 12;
                    outMeta->hasCodecProfile = 1;
                    outMeta->codecSubType = 7;
                    outMeta->hasCodecSubType = 1;
                    parserErr = 0;
                }
            }
        }
    }

    int result;
    switch (parserErr) {
    case 0:
        return 0;
    case -1:
        result = -1;
        break;
    case MP3_ERR_UNSUPPORTED:
        result = 81;
        break;
    case MP3_ERR_READ_IO:
        result = 19;
        break;
    case MP3_ERR_OPEN_IO:
        result = 20;
        break;
    default:
        result = mapMp3ParserError(parserErr);
        if (result == 0) {
            return 0;
        }
        break;
    }

    outMeta->hasChannels = 0;
    outMeta->hasBitRate = 0;
    outMeta->hasDuration = 0;
    outMeta->hasSampleRate = 0;
    outMeta->hasCodecProfile = 0;
    outMeta->hasCodecSubType = 0;
    return result;
}

void *mp3MediaInfoConstruct(void *mem, const char *path, int ioPriority)
{
    return new (mem) Mp3MediaInfo(path, ioPriority);
}

int mp3StreamOpen(Mp3StreamParser *stream)
{
    return (stream != NULL) ? stream->openStream() : -1;
}

const char *mp3StreamGetPath(Mp3StreamParser *stream)
{
    return (stream != NULL && stream->ioStream != NULL)
        ? reinterpret_cast<const char *>(stream->getStreamPosition())
        : NULL;
}

int mp3StreamSetReadDirection(Mp3StreamParser *stream, int direction)
{
    return (stream != NULL) ? stream->setIoHint(direction) : -1;
}

int mp3StreamGetDurationMs(Mp3StreamParser *stream, int *outDurationMs, int *outErr)
{
    unsigned int duration = 0;
    int ret = (stream != NULL) ? stream->getDurationMs(&duration, outErr) : -1;
    if (outDurationMs != NULL)
        *outDurationMs = static_cast<int>(duration);
    return ret;
}

int mp3StreamGetSampleRate(Mp3StreamParser *stream, uint32_t *outSampleRate, int *outErr)
{
    int rate = 0;
    int ret = (stream != NULL) ? stream->getSampleRate(&rate, outErr) : -1;
    if (outSampleRate != NULL)
        *outSampleRate = static_cast<uint32_t>(rate);
    return ret;
}

int mp3StreamGetSamplesPerFrame(Mp3StreamParser *stream, uint16_t *outSamples, int *outErr)
{
    return (stream != NULL) ? stream->getSamplesPerFrame(outSamples, outErr) : -1;
}

int mp3StreamQueryTag(Mp3StreamParser *stream, void *tagOut, void *extraOut)
{
    (void)stream;
    (void)tagOut;
    (void)extraOut;
    return 0;
}

Mp3ParserInterface::~Mp3ParserInterface() {}

} // namespace MusicCore
} // namespace sce
