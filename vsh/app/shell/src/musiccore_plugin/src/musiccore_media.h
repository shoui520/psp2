/*
 * SceShell musiccore_plugin - Shared media info and stream parser declarations
 * for MP3, MP4/AAC, WAV, and ATRAC9 media handlers and their codec factories.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_MEDIA_H
#define VSH_APP_SHELL_MUSICCORE_MEDIA_H

#include <stddef.h>
#include <stdint.h>

#include "musiccore_local_cache_io.h"
#include "musiccore_mp3_readers.h"
#include "musiccore_mp4_boxes.h"

namespace sce {
namespace MusicCore {

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
    virtual int mapRange(int offset, unsigned int size, int flags, int *outMapped,
                         int *outErr, int extra) = 0;
    virtual int unpinRange(int pinToken, int *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, int *outErr) = 0;
};

extern StreamIoInterface *createLocalCacheIo(const char *path, int chunkSize, int blockCount, int readAhead);
extern StreamIoInterface *createLocalCacheIoEx(const char *path, int chunkSize, int blockCount,
                                               int readAhead, int extraChunk, int extraBlocks);

/* ===== MP3 media & stream parser (musiccore_mp3_media.cpp) ===== */

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

class Mp3MediaInfo : public MediaInfoBase {
public:
    int ioMode;
    Mp3StreamParser *parser;
    CachedStreamIo *ioStream;

    Mp3MediaInfo(const char *path, int mode);
    virtual ~Mp3MediaInfo();
    virtual int parseMetadata(TrackMetadataResult *outMeta);
};

/* ===== MP4/AAC media & stream parser (musiccore_mp4_media.cpp) ===== */

struct Mp4FramePacket {
    void *frameData;          /* +0 */
    unsigned int frameSize;   /* +4 */
    int sampleRate;           /* +8 */
    int samplesPerFrame;      /* +12 */
    uint8_t flags;            /* +16 */
    uint8_t channelCount;     /* +17 */
    uint8_t padChan[2];       /* +18..+19 */
    int frameFlags;           /* +20 */
    uint16_t leadingSkip;     /* +24 */
    uint16_t trailingSkip;    /* +26 */
};

class Mp4StreamParser {
public:
    StreamIoInterface *ioStream;
    int openStatus;
    unsigned int currentSampleIndex;
    Mp4MdhdBoxReader mdhdReader;
    Mp4ContainerReader stblReader;
    Mp4SttsReader sttsReader;
    Mp4StszReader stszReader;
    Mp4StscReader stscReader;
    Mp4StcoReader stcoReader;
    Mp4MdhdBox mdhdBox;
    Mp4ContainerBox stblBox;
    Mp4TableBox sttsBox;
    Mp4StszBox stszBox;
    Mp4TableBox stscBox;
    Mp4TableBox stcoBox;
    Mp4DecoderConfigDescriptor decoderConfigBox;
    Mp4AudioSpecificConfig audioSpecConfig;
    Mp4SampleTableNavigator sampleTable;

    explicit Mp4StreamParser(StreamIoInterface *io);
    virtual ~Mp4StreamParser();
    int closeStream();
    int openStream(unsigned int *outErr);
    int getStreamPosition();
    int setIoHint(int mode);
    int resolveEffectiveSampleRate() const;
    int loadSampleAtIndex(unsigned int sampleIdx, int effectiveRate,
                          Mp4FramePacket *outPacket, int *outErr);
    int getDurationMs(unsigned int *outDurationMs, int *outErr);
    int getBaseSampleRate(int *outRate, int *outErr);
    int getSbrSampleRate(int *outRate, int *outErr);
    int getHasSbr(uint8_t *outHasSbr, int *outErr);
    int getSamplesPerFrame(int16_t *outSamples, int *outErr);
    int readFirstSample(Mp4FramePacket *outPacket, int *outErr);
    int readNextSample(int skipCount, Mp4FramePacket *outPacket, int *outErr);
    int readPrevSample(int skipCount, Mp4FramePacket *outPacket, int *outErr);
    int seekToTimeMs(unsigned int targetMs, Mp4FramePacket *outPacket,
                     unsigned int *outSampleIdx, int *outErr);
    int checkSampleReadable(const Mp4FramePacket *packet, int *outErr);
};

class Mp4MediaInfo : public MediaInfoBase {
public:
    int ioMode;
    Mp4StreamParser *parser;
    StreamIoInterface *ioStream;

    Mp4MediaInfo(const char *path, int mode);
    virtual ~Mp4MediaInfo();
    virtual int parseMetadata(TrackMetadataResult *outMeta);
};

/* ===== WAV & ATRAC9 media & cache I/O (musiccore_wav_at9_media.cpp) ===== */

struct WavFmtHeader {
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct At9FmtHeader {
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint16_t cbSize;
    uint16_t samplesPerBlock;
    uint32_t channelMask;
    uint8_t subFormatGuid[16];
    uint32_t versionInfo;
    uint8_t configData[4];
};

struct At9SmplLoopInfo {
    uint32_t loopStart;
    uint32_t loopEnd;
};

class WavCacheIo : public LocalCacheIo {
public:
    WavCacheIo(const char *path, int chunkSize, int blockCount, int readAhead,
               int extraChunk, int extraBlocks);
    virtual ~WavCacheIo();

    virtual int setPath(const char *path);

    int scanChunks(uint8_t targetMask);
    int readFmtHeader(WavFmtHeader *outFmt);
    int readDataRange(uint32_t offset, uint32_t size, void **outPtr, uint32_t *outSize);

    uint32_t getDataSize() const { return m_dataSize; }

    uint8_t m_foundChunks;
    uint8_t m_padFlags[3];
    uint32_t m_fmtOffset;
    uint32_t m_fmtSize;
    uint32_t m_dataOffset;
    uint32_t m_dataSize;
    uint32_t m_infoOffset;
    uint32_t m_infoSize;
};

class WavMediaInfo : public MediaInfoBase {
public:
    explicit WavMediaInfo(const char *path);
    virtual ~WavMediaInfo();

    virtual int parseMetadata(TrackMetadataResult *outMeta);

    WavCacheIo *m_cacheIo;
};

class At9CacheIo : public LocalCacheIo {
public:
    At9CacheIo(const char *path, int chunkSize, int blockCount, int readAhead,
               int extraChunk, int extraBlocks);
    virtual ~At9CacheIo();

    virtual int setPath(const char *path);

    int scanChunks(uint8_t targetMask);
    int readFmtHeader(At9FmtHeader *outFmt);
    int readSmplLoop(At9SmplLoopInfo *outLoop);

    uint32_t getDataSize() const { return m_dataSize; }

    uint8_t m_foundChunks;
    uint8_t m_padFlags[3];
    uint32_t m_fmtOffset;
    uint32_t m_fmtSize;
    uint32_t m_dataOffset;
    uint32_t m_dataSize;
    uint32_t m_infoOffset;
    uint32_t m_infoSize;
    uint32_t m_smplOffset;
    uint32_t m_smplSize;
};

class At9MediaInfo : public MediaInfoBase {
public:
    At9MediaInfo(const char *path, int loopMode);
    virtual ~At9MediaInfo();

    virtual int parseMetadata(TrackMetadataResult *outMeta, int loopCount);

    int m_loopMode;
    At9CacheIo *m_cacheIo;
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_MEDIA_H */
