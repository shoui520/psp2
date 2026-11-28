/*
 * SceShell musiccore_plugin - Mp3MediaInfo, Mp3StreamParser, and MPEG-1/2/2.5
 * Layer-3 frame header / Xing / VBRI utilities.
 * Original address range (3.65-CEX): 0x811EA894 - 0x811EBA6F
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

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

struct MpegFrameHeader {
    uint32_t rawBits;
    uint8_t isValid;
    uint8_t pad[3];

    MpegFrameHeader();
    int parse(uint32_t headerBits);
    int getBitrate() const;
    int getSampleRate() const;
};

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

struct XingHeaderData {
    uint8_t rawBytes[16];
    uint8_t isValid;

    int parse(const void *src);
};

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

struct VbriHeaderData {
    uint8_t rawBytes[26];
    uint8_t isValid;

    int parse(const void *src);
    uint32_t getFrameCount() const;
    uint16_t getTocEntryCount() const;
    uint16_t getTocScaleFactor() const;
    uint16_t getTocEntrySize() const;
    uint16_t getTocFramesPerEntry() const;
};

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

struct StreamIoInterface {
    virtual ~StreamIoInterface() {}
    virtual int isInitialized() = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() = 0;
    virtual int getSize() = 0;
    virtual int getPosition() = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(uint32_t priorityMode) = 0;
    virtual int applyOpenFilePriority(int priorityMode) = 0;
    virtual int flushCache() = 0;
    virtual int setIoHint(int hintMode, int hintParam) = 0;
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, int *outErr, int *outMapped) = 0;
    virtual void *mapRangePinned(int offset, uint32_t size, int pinFlag, int *outPinToken,
                                 int *outErr, uint32_t *outMapped) = 0;
    virtual int unpinRange(int pinToken, int *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, int *outErr) = 0;
};

extern StreamIoInterface *createLocalCacheIo(const char *path, int chunkSize, int blockCount, int readAhead);

struct Mp3FramePacket {
    uint32_t headerBits;
    int fileOffset;
    int frameSize;
    int sampleRate;
    uint16_t samplesPerFrame;
    uint16_t leadingSkipSamples;
    uint16_t trailingSkipSamples;
    uint8_t channelMode;
    uint8_t emphasis;
    int packetSize;
};

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

struct Mp3FrameCursor {
    virtual ~Mp3FrameCursor();
    int currentOffset;
    MpegFrameHeader currentHeader;
};

struct Mp3Id3v1Reader {
    void *vtable;
    StreamIoInterface *io;
    int status;
};

struct Mp3Id3v2Reader {
    uint8_t storage[44];
};

struct Mp3FrameScanner {
    StreamIoInterface *io;
};

struct Mp3FirstFrameInfo {
    uint8_t storage[32];
};

struct Mp3NextFrameIter {
    uint8_t storage[8];
};

struct Mp3CbrSeeker {
    uint8_t storage[12];
};

struct Mp3XingReader {
    uint8_t storage[56];
};

struct Mp3XingSeeker {
    uint8_t storage[16];
};

struct Mp3VbriReader {
    uint8_t storage[56];
};

struct Mp3VbriSeeker {
    uint8_t storage[16];
};

struct Mp3CompositeSeeker {
    uint8_t storage[12];
};

struct Mp3PacketReader {
    uint8_t storage[8];
};

extern void mp3ParserInterfaceInit(Mp3ParserInterface *iface);
extern void mp3ParserInterfaceDestroy(Mp3ParserInterface *iface);
extern void mp3FrameCursorInit(Mp3FrameCursor *cursor);
extern void mp3FrameCursorDestroy(Mp3FrameCursor *cursor);

extern void mp3Id3v1Init(Mp3Id3v1Reader *reader, StreamIoInterface *io);
extern void mp3Id3v1Destroy(Mp3Id3v1Reader *reader);
extern int mp3Id3v1Parse(Mp3Id3v1Reader *reader, int *outErr);
extern int *mp3Id3v1GetState(Mp3Id3v1Reader *reader);

extern void mp3Id3v2Init(Mp3Id3v2Reader *reader, StreamIoInterface *io);
extern void mp3Id3v2Destroy(Mp3Id3v2Reader *reader);
extern int mp3Id3v2Parse(Mp3Id3v2Reader *reader, int mode, int *outErr);
extern int *mp3Id3v2GetState(Mp3Id3v2Reader *reader);
extern void *mp3Id3v2GetTagHeader(Mp3Id3v2Reader *reader);
extern int mp3Id3v2GetPayloadSize(void *tagHeader);

extern void mp3FrameScannerInit(Mp3FrameScanner *scanner, StreamIoInterface *io);
extern int mp3FrameScannerFind(Mp3FrameScanner *scanner, int startOffset, int endOffset,
                               const MpegFrameHeader *refHeader, int minConsecutive, int audioSize,
                               int *outOffset, MpegFrameHeader *outHeader, int *outEndFlags, int *outErr);

extern void mp3FirstFrameInit(Mp3FirstFrameInfo *firstFrame, StreamIoInterface *io,
                              Mp3Id3v2Reader *id3v2, Mp3FrameScanner *scanner);
extern void mp3FirstFrameDestroy(Mp3FirstFrameInfo *firstFrame);
extern int mp3FirstFrameEnsure(Mp3FirstFrameInfo *firstFrame);
extern int *mp3FirstFrameGetOffset(Mp3FirstFrameInfo *firstFrame);
extern MpegFrameHeader *mp3FirstFrameGetHeader(Mp3FirstFrameInfo *firstFrame);

extern void mp3NextFrameInit(Mp3NextFrameIter *iter, Mp3FrameScanner *scanner);
extern void mp3NextFrameDestroy(Mp3NextFrameIter *iter);
extern int mp3NextFrameAdvance(Mp3NextFrameIter *iter, Mp3FrameCursor *cursor, int audioSize,
                               int *outOffset, MpegFrameHeader *outHeader, int *outEndFlags, int *outErr);

extern void mp3CbrSeekerInit(Mp3CbrSeeker *seeker, Mp3FirstFrameInfo *firstFrame, Mp3FrameScanner *scanner);
extern void mp3CbrSeekerDestroy(Mp3CbrSeeker *seeker);

extern void mp3XingInit(Mp3XingReader *reader, StreamIoInterface *io, Mp3FirstFrameInfo *firstFrame);
extern void mp3XingDestroy(Mp3XingReader *reader);
extern int mp3XingEnsure(Mp3XingReader *reader, int audioSize, int *outErr);
extern const uint8_t *mp3XingGetRawData(Mp3XingReader *reader);
extern int mp3XingGetEncoderDelay(Mp3XingReader *reader, int audioSize);
extern int mp3XingGetEncoderPadding(Mp3XingReader *reader, int audioSize);

extern void mp3XingSeekerInit(Mp3XingSeeker *seeker, Mp3FirstFrameInfo *firstFrame,
                              Mp3XingReader *xing, Mp3FrameScanner *scanner);
extern void mp3XingSeekerDestroy(Mp3XingSeeker *seeker);

extern void mp3VbriInit(Mp3VbriReader *reader, StreamIoInterface *io, Mp3FirstFrameInfo *firstFrame);
extern void mp3VbriDestroy(Mp3VbriReader *reader);
extern int mp3VbriEnsure(Mp3VbriReader *reader, int audioSize, int *outErr);
extern const VbriHeaderData *mp3VbriGetData(Mp3VbriReader *reader);

extern void mp3VbriSeekerInit(Mp3VbriSeeker *seeker, Mp3FirstFrameInfo *firstFrame,
                              Mp3VbriReader *vbri, Mp3FrameScanner *scanner);
extern void mp3VbriSeekerDestroy(Mp3VbriSeeker *seeker);

extern void mp3CompositeSeekerInit(Mp3CompositeSeeker *seeker, Mp3CbrSeeker *cbr,
                                   Mp3XingSeeker *xing, Mp3VbriSeeker *vbri);
extern int mp3CompositeSeek(Mp3CompositeSeeker *seeker, unsigned int targetMs, int audioSize,
                            int *outOffset, MpegFrameHeader *outHeader, uint8_t *outAtEnd, int *outErr);

extern void mp3PacketReaderInit(Mp3PacketReader *reader, StreamIoInterface *io);
extern void mp3PacketReaderDestroy(Mp3PacketReader *reader);
extern int mp3PacketReaderRead(Mp3PacketReader *reader, int offset, unsigned int frameSize,
                               int sampleRate, int samplesPerFrame, int emphasis,
                               int channelMode, Mp3FramePacket *outPacket, int *outErr);

class Mp3StreamParser : public Mp3ParserInterface, public Mp3FrameCursor {
public:
    StreamIoInterface *ioStream;
    int openStatus;
    Mp3Id3v1Reader id3v1Reader;
    Mp3Id3v2Reader id3v2Reader;
    Mp3FrameScanner frameScanner;
    Mp3FirstFrameInfo firstFrame;
    Mp3NextFrameIter nextFrameIter;
    Mp3CbrSeeker cbrSeeker;
    Mp3XingReader xingReader;
    Mp3XingSeeker xingSeeker;
    Mp3VbriReader vbriReader;
    Mp3VbriSeeker vbriSeeker;
    Mp3CompositeSeeker compositeSeeker;
    Mp3PacketReader packetReader;
    int audioDataSize;

    explicit Mp3StreamParser(StreamIoInterface *io);
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
Mp3StreamParser::Mp3StreamParser(StreamIoInterface *io)
    : ioStream(io),
      openStatus(-1),
      audioDataSize(-1)
{
    mp3ParserInterfaceInit(this);
    mp3FrameCursorInit(this);
    mp3Id3v1Init(&id3v1Reader, ioStream);
    mp3Id3v2Init(&id3v2Reader, ioStream);
    mp3FrameScannerInit(&frameScanner, ioStream);
    mp3FirstFrameInit(&firstFrame, ioStream, &id3v2Reader, &frameScanner);
    mp3NextFrameInit(&nextFrameIter, &frameScanner);
    mp3CbrSeekerInit(&cbrSeeker, &firstFrame, &frameScanner);
    mp3XingInit(&xingReader, ioStream, &firstFrame);
    mp3XingSeekerInit(&xingSeeker, &firstFrame, &xingReader, &frameScanner);
    mp3VbriInit(&vbriReader, ioStream, &firstFrame);
    mp3VbriSeekerInit(&vbriSeeker, &firstFrame, &vbriReader, &frameScanner);
    mp3CompositeSeekerInit(&compositeSeeker, &cbrSeeker, &xingSeeker, &vbriSeeker);
    mp3PacketReaderInit(&packetReader, ioStream);
}

/* 0x811EABF2 / 0x811EAC98 / 0x811EB72E / 0x811EB73C */
Mp3StreamParser::~Mp3StreamParser()
{
    if (ioStream != NULL && ioStream->isOpen() == 1 && ioStream->close() >= 0) {
        openStatus = -1;
    }
    mp3PacketReaderDestroy(&packetReader);
    mp3VbriSeekerDestroy(&vbriSeeker);
    mp3VbriDestroy(&vbriReader);
    mp3XingSeekerDestroy(&xingSeeker);
    mp3XingDestroy(&xingReader);
    mp3CbrSeekerDestroy(&cbrSeeker);
    mp3NextFrameDestroy(&nextFrameIter);
    mp3FirstFrameDestroy(&firstFrame);
    mp3Id3v2Destroy(&id3v2Reader);
    mp3Id3v1Destroy(&id3v1Reader);
    mp3FrameCursorDestroy(this);
    mp3ParserInterfaceDestroy(this);
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
    return ioStream->getPosition();
}

/* 0x811EACE2 */
int Mp3StreamParser::setIoHint(int mode)
{
    if (ioStream == NULL) {
        return 0;
    }
    switch (mode) {
    case 4:
        ioStream->setIoHint(3, 0);
        return 1;
    case 2:
        ioStream->setIoHint(2, 0);
        return 1;
    case 1:
        ioStream->setIoHint(1, 0);
        return 1;
    case 0:
        ioStream->setIoHint(0, 0);
        return 1;
    default:
        return 0;
    }
}

/* 0x811EAD36 */
int Mp3StreamParser::computeAudioDataSize(int *outErr)
{
    int overhead = 0;
    if (*mp3Id3v2GetState(&id3v2Reader) == 255 && mp3Id3v2Parse(&id3v2Reader, 0, outErr) == 0) {
        return 0;
    }
    if (*mp3Id3v2GetState(&id3v2Reader) == 1) {
        overhead = mp3Id3v2GetPayloadSize(mp3Id3v2GetTagHeader(&id3v2Reader)) + 10;
    }
    if (*mp3Id3v1GetState(&id3v1Reader) == 255 && mp3Id3v1Parse(&id3v1Reader, outErr) == 0) {
        return 0;
    }
    if (*mp3Id3v1GetState(&id3v1Reader) == 1) {
        overhead += 128;
    }
    int totalSize = ioStream->getSize();
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
    if (mp3FirstFrameEnsure(&firstFrame) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }

    MpegFrameHeader *hdr = mp3FirstFrameGetHeader(&firstFrame);
    unsigned int samplesPerFrame = (unsigned int)getMpegLayer3SamplesPerFrame(hdr->rawBits);
    unsigned int sampleRate = (unsigned int)hdr->getSampleRate();

    if (mp3XingEnsure(&xingReader, audioDataSize, outErr) != 0) {
        const uint8_t *xingRaw = mp3XingGetRawData(&xingReader);
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

    if (mp3VbriEnsure(&vbriReader, audioDataSize, outErr) != 0) {
        uint32_t totalFrames = mp3VbriGetData(&vbriReader)->getFrameCount();
        unsigned int ms = (unsigned int)((((float)totalFrames * (float)samplesPerFrame) / (float)sampleRate) * 1000.0f);
        *outDurationMs = ms;
        if (ms != 0u) {
            *outErr = 0;
            return 1;
        }
    }

    int payloadBytes = audioDataSize - *mp3FirstFrameGetOffset(&firstFrame);
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
    if (mp3FirstFrameEnsure(&firstFrame) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }
    *outRate = mp3FirstFrameGetHeader(&firstFrame)->getSampleRate();
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
    if (mp3FirstFrameEnsure(&firstFrame) == 0) {
        if (*outErr == MP3_ERR_EOF) {
            *outErr = MP3_ERR_NO_FRAME;
        }
        return 0;
    }
    *outSamples = (uint16_t)getMpegLayer3SamplesPerFrame(mp3FirstFrameGetHeader(&firstFrame)->rawBits);
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
        mp3FirstFrameEnsure(&firstFrame) == 0) {
        return 0;
    }

    int firstOff = *mp3FirstFrameGetOffset(&firstFrame);
    MpegFrameHeader *firstHdr = mp3FirstFrameGetHeader(&firstFrame);
    int foundOffset = -1;
    MpegFrameHeader foundHeader;
    int endFlags = 0;
    int leadingSkip = 0;

    if (mp3XingEnsure(&xingReader, audioDataSize, outErr) != 0) {
        int searchStart = firstOff + (int)getMpegLayer3FrameSize(firstHdr->rawBits);
        int searchEnd = searchStart + 1441;
        if (mp3FrameScannerFind(&frameScanner, searchStart, searchEnd, firstHdr, 3,
                                audioDataSize, &foundOffset, &foundHeader, &endFlags, outErr) == 0) {
            return 0;
        }
        leadingSkip = mp3XingGetEncoderDelay(&xingReader, audioDataSize);
        int spf = getMpegLayer3SamplesPerFrame(foundHeader.rawBits);
        if (leadingSkip > spf) {
            leadingSkip = spf;
        }
        if (leadingSkip == 0) {
            leadingSkip = spf >> 1;
        }
    } else if (mp3VbriEnsure(&vbriReader, audioDataSize, outErr) != 0) {
        int searchStart = firstOff + (int)getMpegLayer3FrameSize(firstHdr->rawBits);
        int searchEnd = searchStart + 1441;
        if (mp3FrameScannerFind(&frameScanner, searchStart, searchEnd, firstHdr, 3,
                                audioDataSize, &foundOffset, &foundHeader, &endFlags, outErr) == 0) {
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
    if (mp3PacketReaderRead(&packetReader, foundOffset, frameSize, sampleRate, spf,
                            (int)(foundHeader.rawBits & 3u),
                            (int)((foundHeader.rawBits >> 6) & 3u),
                            outPacket, outErr) == 0) {
        return 0;
    }

    currentOffset = foundOffset;
    currentHeader = foundHeader;
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
    int endKind = 0;

    if (mp3NextFrameAdvance(&nextFrameIter, this, audioDataSize,
                            &nextOffset, &nextHeader, &endKind, outErr) == 0) {
        return 0;
    }

    unsigned int frameSize = getMpegLayer3FrameSize(nextHeader.rawBits);
    int sampleRate = nextHeader.getSampleRate();
    int spf = getMpegLayer3SamplesPerFrame(nextHeader.rawBits);
    if (mp3PacketReaderRead(&packetReader, nextOffset, frameSize, sampleRate, spf,
                            (int)(nextHeader.rawBits & 3u),
                            (int)((nextHeader.rawBits >> 6) & 3u),
                            outPacket, outErr) == 0) {
        return 0;
    }

    if (mp3XingEnsure(&xingReader, audioDataSize, outErr) == 0) {
        outPacket->trailingSkipSamples = 0;
    } else if (endKind == 0) {
        int pad = mp3XingGetEncoderPadding(&xingReader, audioDataSize);
        outPacket->trailingSkipSamples = (uint16_t)((pad > spf) ? spf : pad);
    } else if (endKind == 1) {
        if (mp3XingEnsure(&xingReader, audioDataSize, outErr) != 0) {
            int pad = mp3XingGetEncoderPadding(&xingReader, audioDataSize);
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
    currentOffset = nextOffset;
    currentHeader = nextHeader;
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
    uint8_t atEnd = 0;

    if (mp3CompositeSeek(&compositeSeeker, targetMs, audioDataSize,
                         &seekOffset, &seekHeader, &atEnd, outErr) == 0) {
        return 0;
    }

    unsigned int frameSize = getMpegLayer3FrameSize(seekHeader.rawBits);
    int sampleRate = seekHeader.getSampleRate();
    int spf = getMpegLayer3SamplesPerFrame(seekHeader.rawBits);
    if (mp3PacketReaderRead(&packetReader, seekOffset, frameSize, sampleRate, spf,
                            (int)(seekHeader.rawBits & 3u),
                            (int)((seekHeader.rawBits >> 6) & 3u),
                            outPacket, outErr) == 0) {
        return 0;
    }

    if (atEnd != 0 && mp3XingEnsure(&xingReader, audioDataSize, outErr) != 0) {
        int pad = mp3XingGetEncoderPadding(&xingReader, audioDataSize);
        outPacket->trailingSkipSamples = (uint16_t)((pad > spf) ? spf : pad);
    } else {
        outPacket->trailingSkipSamples = 0;
    }

    outPacket->leadingSkipSamples = 0;
    currentOffset = seekOffset;
    currentHeader = seekHeader;
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

struct MediaInfoRecord {
    void *vtable;
    uint32_t formatFourCC;
    uint8_t isRetained;
    char filePath[2051];
};

extern void mediaInfoRecordInit(MediaInfoRecord *rec, const char *path, uint32_t fourCC);
extern void mediaInfoRecordDestroy(MediaInfoRecord *rec);

class Mp3MediaInfo {
public:
    uint32_t formatFourCC;
    uint8_t isRetained;
    char filePath[2051];
    int ioMode;
    Mp3StreamParser *parser;
    StreamIoInterface *ioStream;

    Mp3MediaInfo(const char *path, int mode);
    virtual ~Mp3MediaInfo();
    virtual int parseMetadata(TrackMetadataResult *outMeta);
};

/* 0x811EA894 */
Mp3MediaInfo::Mp3MediaInfo(const char *path, int mode)
    : ioMode(mode),
      parser(NULL),
      ioStream(NULL)
{
    mediaInfoRecordInit(reinterpret_cast<MediaInfoRecord *>(this), path, FOURCC_MP3);
    if (mode == 16) {
        ioStream = createLocalCacheIo(filePath, 0x4000, 32, 1);
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
    mediaInfoRecordDestroy(reinterpret_cast<MediaInfoRecord *>(this));
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
Mp3FrameCursor::~Mp3FrameCursor() {}

uint32_t bitMaskLow(int bits)
{
    return makeBitMask(static_cast<uint8_t>(bits));
}

uint32_t readBigEndian32(const void *ptr)
{
    return readBigEndian32(static_cast<const uint8_t *>(ptr));
}

uint32_t readBigEndian24(const void *ptr)
{
    return readBigEndian24(static_cast<const uint8_t *>(ptr));
}

uint16_t readBigEndian16(const void *ptr)
{
    return readBigEndian16(static_cast<const uint8_t *>(ptr));
}

int mp3HeaderValidate(uint32_t headerBits, int *outError)
{
    return validateMpegLayer3Header(headerBits, outError);
}

int mp3HeaderSamplesPerFrame(uint32_t headerBits)
{
    return getMpegLayer3SamplesPerFrame(headerBits);
}

int mp3HeaderFrameSize(uint32_t headerBits)
{
    return getMpegLayer3FrameSize(headerBits);
}

int mp3HeaderMatchesRef(uint32_t candidateBits, uint32_t refBits)
{
    return hasSameMpegSampleRate(candidateBits, refBits);
}

uint32_t mp3HeaderBitRate(const void *headerHolder)
{
    return (headerHolder != NULL)
        ? static_cast<uint32_t>(static_cast<const MpegFrameHeader *>(headerHolder)->getBitrate())
        : 0u;
}

uint32_t mp3HeaderSampleRate(const void *headerHolder)
{
    return (headerHolder != NULL)
        ? static_cast<uint32_t>(static_cast<const MpegFrameHeader *>(headerHolder)->getSampleRate())
        : 0u;
}

struct Mp3HeaderHolder {
    uint32_t headerBits;
    uint8_t isValid;
    uint8_t padValid[3];
};

void mp3HeaderHolderInit(Mp3HeaderHolder *holder)
{
    if (holder != NULL) {
        holder->headerBits = 0;
        holder->isValid = 0;
    }
}

int mp3HeaderHolderParse(Mp3HeaderHolder *holder, uint32_t headerBits)
{
    if (holder == NULL)
        return 0;
    int err = 0;
    int valid = validateMpegLayer3Header(headerBits, &err);
    holder->headerBits = headerBits;
    holder->isValid = static_cast<uint8_t>(valid != 0);
    return valid;
}

int vbriHeaderParse(VbriHeaderData *hdr, const void *src)
{
    return (hdr != NULL) ? hdr->parse(src) : 0;
}

uint32_t vbriHeaderGetFrameCount(const VbriHeaderData *hdr)
{
    return (hdr != NULL) ? readBigEndian32(hdr->rawBytes + 14) : 0u;
}

uint16_t vbriHeaderGetTocEntries(const VbriHeaderData *hdr)
{
    return (hdr != NULL) ? readBigEndian16(hdr->rawBytes + 18) : 0u;
}

uint16_t vbriHeaderGetTocScale(const VbriHeaderData *hdr)
{
    return (hdr != NULL) ? readBigEndian16(hdr->rawBytes + 20) : 0u;
}

uint16_t vbriHeaderGetTocEntrySize(const VbriHeaderData *hdr)
{
    return (hdr != NULL) ? readBigEndian16(hdr->rawBytes + 22) : 0u;
}

uint16_t vbriHeaderGetTocFramesPerEntry(const VbriHeaderData *hdr)
{
    return (hdr != NULL) ? readBigEndian16(hdr->rawBytes + 24) : 0u;
}

int xingHeaderParse(XingHeaderData *hdr, const void *src)
{
    return (hdr != NULL) ? hdr->parse(src) : 0;
}

} // namespace MusicCore
} // namespace sce
