/*
 * SceShell musiccore_plugin - Shared declarations for MediaInfoBase, MPEG-1/2/2.5
 * Layer-3 header utilities, ID3v1/ID3v2 tag readers, MP3 frame scanner,
 * VBRI/Xing/CBR seekers, and MP3 frame reader.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_MP3_READERS_H
#define VSH_APP_SHELL_MUSICCORE_MP3_READERS_H

#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

/* MPEG Layer-3 header and big-endian helpers (musiccore_mp3_media.cpp) */
uint8_t makeBitMask(uint8_t bitCount);
uint32_t readBigEndian32(const uint8_t *bytes);
uint32_t readBigEndian24(const uint8_t *bytes);
uint16_t readBigEndian16(const uint8_t *bytes);
int validateMpegLayer3Header(uint32_t rawHeader, int *outReason);
int getMpegLayer3Bitrate(uint32_t rawHeader);
int getMpegLayer3SampleRate(uint32_t rawHeader);
int getMpegLayer3SamplesPerFrame(uint32_t rawHeader);
unsigned int getMpegLayer3FrameSize(uint32_t rawHeader);
int hasSameMpegSampleRate(uint32_t lhsHeader, uint32_t rhsHeader);

struct MpegFrameHeader {
    uint32_t rawBits;
    uint8_t isValid;
    uint8_t padValid[3];

    MpegFrameHeader();
    int parse(uint32_t headerBits);
    int getBitrate() const;
    int getSampleRate() const;
};

struct XingHeaderData {
    uint8_t rawBytes[16];
    uint8_t isValid;
    uint8_t padValid[3];

    int parse(const void *src);
};

struct VbriHeaderData {
    uint8_t rawBytes[26];
    uint8_t isValid;
    uint8_t padValid;

    int parse(const void *src);
    uint32_t getFrameCount() const;
    uint16_t getTocEntryCount() const;
    uint16_t getTocScaleFactor() const;
    uint16_t getTocEntrySize() const;
    uint16_t getTocFramesPerEntry() const;
};

/* Cached stream I/O abstract interface matching LocalCacheIo vtable (0x815337A8) */
class CachedStreamIo {
public:
    virtual ~CachedStreamIo() {}
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
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, int *outErr,
                                 int *outMappedSize) = 0;
    virtual void *mapRangePinned(int offset, uint32_t size, int pinFlag,
                                 int *outPinToken, int *outErr, uint32_t *outMappedSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, int *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int prefetchRange(uint8_t flags, int offset, int size, int *outErr) = 0;
};

class MediaInfoBase {
public:
    MediaInfoBase(const char *path, uint32_t mediaFourcc);
    virtual ~MediaInfoBase();

    uint32_t m_mediaFourcc; /* +4 */
    uint8_t m_sharedFlag;   /* +8 */
    char m_path[2049];      /* +9..+2057 */
};

class Id3v1Reader {
public:
    explicit Id3v1Reader(CachedStreamIo *io);
    virtual ~Id3v1Reader();

    int probe(int *outErr);
    int *getTagStatePtr() { return &m_tagState; }

private:
    CachedStreamIo *m_io; /* +4 */
    int m_tagState;       /* +8: 0=absent, 1=present, 255=unprobed */
};

class Id3v2Header {
public:
    Id3v2Header();

    uint32_t getTagBodySize() const;
    static bool isHeaderValid(const uint8_t *bytes);
    static bool isFooterValid(const uint8_t *bytes);
    int parse(const uint8_t *bytes);

    uint8_t m_rawHeader[10]; /* +0..+9 */
    uint8_t m_isValid;       /* +10 */
    uint8_t m_padValid;      /* +11 */
};

class Id3v2Reader {
public:
    explicit Id3v2Reader(CachedStreamIo *io);
    virtual ~Id3v2Reader();

    int readTag(uint8_t fromStart, int *outErr);
    int hasTag() const;
    int probeTagPresence(uint8_t fromStart, int *outErr);

    const int *getHeaderPresencePtr() const { return &m_headerPresent; }
    const int *getFooterPresencePtr() const { return &m_footerPresent; }
    const Id3v2Header *getHeaderPtr() const { return &m_header; }
    const int *getExtendedSizePtr() const { return &m_extendedSize; }
    const int *getTagOffsetPtr() const { return &m_tagOffset; }
    int getLocationKind() const { return m_locationKind; }

private:
    Id3v2Header m_header;  /* +4..+15 */
    int m_locationKind;    /* +16: 1=header, 2=footer, 255=none */
    int m_extendedSize;    /* +20 */
    uint8_t m_hasExtHdr;   /* +24 */
    uint8_t m_padExt[3];   /* +25..+27 */
    int m_tagOffset;       /* +28 */
    int m_headerPresent;   /* +32 */
    int m_footerPresent;   /* +36 */
    CachedStreamIo *m_io;  /* +40 */
};

class Mp3FrameScanner {
public:
    explicit Mp3FrameScanner(CachedStreamIo *io);

    int findSyncFrame(int startOffset, int maxOffset, const uint32_t *refHeaderBits,
                      int confirmDepth, int fileLimit, int *outOffset,
                      MpegFrameHeader *outHeader, uint32_t *outMatchedDepth, int *outErr);

private:
    CachedStreamIo *m_io; /* +0 */
};

class Mp3FrameRecord {
public:
    Mp3FrameRecord();
    virtual ~Mp3FrameRecord();

    int m_fileOffset;          /* +4 */
    MpegFrameHeader m_header;  /* +8..+15 */
};

class Mp3FirstFrameFinder {
public:
    Mp3FirstFrameFinder(CachedStreamIo *io, Id3v2Reader *id3v2, Mp3FrameScanner *scanner);
    virtual ~Mp3FirstFrameFinder();

    int locateFirstFrame(int fileLimit, int *outErr);

    int getProbeState() const { return m_probeState; }
    int getFirstFrameOffset() const { return m_firstFrameOffset; }
    int *getFirstFrameOffsetPtr() { return &m_firstFrameOffset; }
    const MpegFrameHeader *getFirstHeader() const { return &m_firstHeader; }
    MpegFrameHeader *getFirstHeaderMut() { return &m_firstHeader; }

private:
    int m_probeState;             /* +4 */
    int m_firstFrameOffset;       /* +8 */
    MpegFrameHeader m_firstHeader;/* +12..+19 */
    CachedStreamIo *m_io;         /* +20 */
    Id3v2Reader *m_id3v2;         /* +24 */
    Mp3FrameScanner *m_scanner;   /* +28 */
};

class VbriTagReader {
public:
    VbriTagReader(CachedStreamIo *io, Mp3FirstFrameFinder *firstFinder);
    virtual ~VbriTagReader();

    int readHeader(int fileLimit, int *outErr);
    int readToc(int fileLimit, int *outErr);

    const VbriHeaderData *getHeader() const { return &m_vbriHeader; }
    const uint32_t *getTocTable() const { return m_tocTable; }

private:
    int m_headerState;                  /* +4 */
    VbriHeaderData m_vbriHeader;        /* +8..+35 */
    int m_headerOffset;                 /* +36 */
    int m_tocState;                     /* +40 */
    uint32_t *m_tocTable;               /* +44 */
    CachedStreamIo *m_io;               /* +48 */
    Mp3FirstFrameFinder *m_firstFinder; /* +52 */
};

class XingTagReader {
public:
    XingTagReader(CachedStreamIo *io, Mp3FirstFrameFinder *firstFinder);
    virtual ~XingTagReader();

    int readHeader(int fileLimit, int *outErr);
    int readToc(int fileLimit, int *outErr);
    int readLameDelays(int fileLimit, int *outErr);
    uint16_t getEncoderDelay(int fileLimit);
    uint16_t getEncoderPadding(int fileLimit);

    const XingHeaderData *getHeader() const { return &m_xingHeader; }
    const uint8_t *getTocBytes() const { return m_tocBytes; }

private:
    int m_headerState;                  /* +4 */
    XingHeaderData m_xingHeader;        /* +8..+27 */
    int m_headerOffset;                 /* +28 */
    int m_tocState;                     /* +32 */
    uint8_t *m_tocBytes;                /* +36 */
    CachedStreamIo *m_io;               /* +40 */
    Mp3FirstFrameFinder *m_firstFinder; /* +44 */
    int m_encoderDelay;                 /* +48 */
    int m_encoderPadding;               /* +52 */
};

class Mp3CbrSeeker {
public:
    Mp3CbrSeeker(Mp3FirstFrameFinder *firstFinder, Mp3FrameScanner *scanner);
    virtual ~Mp3CbrSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 MpegFrameHeader *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    Mp3FrameScanner *m_scanner;         /* +8 */
};

class Mp3VbriSeeker {
public:
    Mp3VbriSeeker(Mp3FirstFrameFinder *firstFinder, VbriTagReader *vbriReader,
                  Mp3FrameScanner *scanner);
    virtual ~Mp3VbriSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 MpegFrameHeader *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    VbriTagReader *m_vbriReader;        /* +8 */
    Mp3FrameScanner *m_scanner;         /* +12 */
};

class Mp3XingSeeker {
public:
    Mp3XingSeeker(Mp3FirstFrameFinder *firstFinder, XingTagReader *xingReader,
                  Mp3FrameScanner *scanner);
    virtual ~Mp3XingSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 MpegFrameHeader *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    XingTagReader *m_xingReader;        /* +8 */
    Mp3FrameScanner *m_scanner;         /* +12 */
};

class Mp3SeekRouter {
public:
    Mp3SeekRouter(Mp3CbrSeeker *cbr, Mp3XingSeeker *xing, Mp3VbriSeeker *vbri);

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 MpegFrameHeader *outHeader, bool *outAtEnd, int *outErr);

private:
    Mp3CbrSeeker *m_cbrSeeker;   /* +0 */
    Mp3XingSeeker *m_xingSeeker; /* +4 */
    Mp3VbriSeeker *m_vbriSeeker; /* +8 */
};

struct Mp3FramePacket {
    void *frameData;              /* +0 */
    uint32_t frameSize;           /* +4 */
    uint32_t sampleRate;          /* +8 */
    uint32_t samplesPerFrame;     /* +12 */
    uint8_t channelMode;          /* +16 */
    uint8_t padChan;              /* +17 */
    uint16_t leadingSkipSamples;  /* +18 */
    uint16_t trailingSkipSamples; /* +20 */
    uint8_t mpegVersion;          /* +22 */
    uint8_t padVer;               /* +23 */
    int packetSize;               /* +24 */
};

class Mp3FrameReader {
public:
    explicit Mp3FrameReader(CachedStreamIo *io);
    virtual ~Mp3FrameReader();

    int readFrameData(int fileOffset, uint32_t frameSize, uint16_t sampleRate,
                      uint16_t samplesPerFrame, uint8_t channelMode,
                      uint8_t mpegVersion, Mp3FramePacket *outFrame, int *outErr);

private:
    CachedStreamIo *m_io; /* +4 */
};

class Mp3NextFrameLocator {
public:
    explicit Mp3NextFrameLocator(Mp3FrameScanner *scanner);
    virtual ~Mp3NextFrameLocator();

    int locateNextFrame(const Mp3FrameRecord *curRecord, int fileLimit, int *outOffset,
                        MpegFrameHeader *outHeader, uint32_t *outMatchedDepth, int *outErr);

private:
    Mp3FrameScanner *m_scanner; /* +4 */
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_MP3_READERS_H */
