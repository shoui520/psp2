/*
 * SceShell musiccore_plugin - MediaInfoBase, ID3v1/ID3v2 tag readers,
 * MP3 first-frame finder, VBRI/Xing/CBR seekers, MP3 frame reader, and
 * frame sync scanner.
 * Original address range (3.65-CEX): 0x811FDB5C - 0x811FF92F
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
int sce_paf_memcmp(const void *lhs, const void *rhs, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
}

namespace sce {
namespace MusicCore {

static const int MP3_ERR_UNSUPPORTED = -2144862186;  /* 0x80280016 */
static const int MP3_ERR_CORRUPT = -2144862185;      /* 0x80280017 */
static const int MP3_ERR_NOT_FOUND = -2144862184;    /* 0x80280018 */
static const int MP3_ERR_BAD_OFFSET = -2144862183;   /* 0x80280019 */
static const int MP3_ERR_NO_MEMORY = -2144862182;    /* 0x8028001A */
static const int IO_ERR_EOF = -2146394105;           /* 0x8010A007 */
static const int IO_ERR_BAD_PARAM = -2146390031;     /* 0x8010AFF1 */

static const int TAG_STATE_NONE = 0;
static const int TAG_STATE_FOUND = 1;
static const int TAG_STATE_UNKNOWN = 255;

extern uint8_t *allocUint8Array(size_t count);
extern uint32_t *allocUint32ArrayCount(size_t count);

/* MP3 header bit helpers (implemented in musiccore_mp3_media.cpp) */
extern uint32_t bitMaskLow(int bits);
extern uint32_t readBigEndian32(const void *ptr);
extern uint32_t readBigEndian24(const void *ptr);
extern uint16_t readBigEndian16(const void *ptr);
extern int mp3HeaderValidate(uint32_t headerBits, int *outError);
extern int mp3HeaderSamplesPerFrame(uint32_t headerBits);
extern int mp3HeaderFrameSize(uint32_t headerBits);
extern int mp3HeaderMatchesRef(uint32_t candidateBits, uint32_t refBits);
extern uint32_t mp3HeaderBitRate(const void *headerHolder);
extern uint32_t mp3HeaderSampleRate(const void *headerHolder);

struct Mp3HeaderHolder {
    uint32_t headerBits;
    uint8_t isValid;
    uint8_t padValid[3];
};

extern void mp3HeaderHolderInit(Mp3HeaderHolder *holder);
extern int mp3HeaderHolderParse(Mp3HeaderHolder *holder, uint32_t headerBits);

/* VBRI & Xing header parsed structures (implemented in musiccore_mp3_media.cpp) */
struct VbriHeaderData {
    uint8_t rawBytes[26];
    uint8_t isValid;
    uint8_t padValid;
};

extern int vbriHeaderParse(VbriHeaderData *hdr, const void *src);
extern uint32_t vbriHeaderGetFrameCount(const VbriHeaderData *hdr);
extern uint16_t vbriHeaderGetTocEntries(const VbriHeaderData *hdr);
extern uint16_t vbriHeaderGetTocScale(const VbriHeaderData *hdr);
extern uint16_t vbriHeaderGetTocEntrySize(const VbriHeaderData *hdr);
extern uint16_t vbriHeaderGetTocFramesPerEntry(const VbriHeaderData *hdr);

struct XingHeaderData {
    uint8_t rawBytes[16];
    uint8_t isValid;
    uint8_t padValid[3];
};

extern int xingHeaderParse(XingHeaderData *hdr, const void *src);

/* CacheIo stream interface */
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
    virtual int setCacheHint(int hintMode) = 0;
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, int *outErr,
                                 int *outMappedSize) = 0;
    virtual void *mapRangePinned(int offset, uint32_t size, int pinFlag,
                                 int *outPinToken, int *outErr, uint32_t *outMappedSize) = 0;
    virtual int unpinRange(int pinToken, int *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
};

static uint32_t swapBigEndian32(uint32_t value)
{
    return ((value & 0x000000FFu) << 24)
         | ((value & 0x0000FF00u) << 8)
         | ((value & 0x00FF0000u) >> 8)
         | ((value & 0xFF000000u) >> 24);
}

/* ===== MediaInfoBase (0x811FDB5C - 0x811FDBBF) ===== */

class MediaInfoBase {
public:
    MediaInfoBase(const char *path, uint32_t mediaFourcc);
    virtual ~MediaInfoBase();

    uint32_t m_mediaFourcc; /* +4 */
    uint8_t m_sharedFlag;   /* +8 */
    char m_path[2049];      /* +9..+2057 */
};

/* 0x811FDB5C */
MediaInfoBase::MediaInfoBase(const char *path, uint32_t mediaFourcc)
    : m_mediaFourcc(mediaFourcc)
    , m_sharedFlag(0)
{
    if (path != NULL) {
        sce_paf_strncpy(m_path, path, 2049u);
        m_path[2048] = '\0';
    } else {
        m_path[0] = '\0';
    }
}

/* 0x811FDB94 / 0x811FDBA6 */
MediaInfoBase::~MediaInfoBase()
{
    m_path[0] = '\0';
}

/* ===== Id3v1Reader (0x811FDBC0 - 0x811FDCC3) ===== */

class Id3v1Reader {
public:
    explicit Id3v1Reader(CachedStreamIo *io);
    virtual ~Id3v1Reader();

    int probe(int *outErr);

private:
    CachedStreamIo *m_io; /* +4 */
    int m_tagState;       /* +8: 0=absent, 1=present, 255=unprobed */
};

/* 0x811FDBC0 */
Id3v1Reader::Id3v1Reader(CachedStreamIo *io)
    : m_io(io)
    , m_tagState(TAG_STATE_UNKNOWN)
{
}

/* 0x811FDBD8 / 0x811FDBE6 */
Id3v1Reader::~Id3v1Reader()
{
}

/* 0x811FDBFC */
int Id3v1Reader::probe(int *outErr)
{
    if (m_io == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_tagState != TAG_STATE_UNKNOWN) {
        *outErr = 0;
        return 0;
    }

    int fileSize = m_io->getFileSize();
    if (fileSize < 0) {
        *outErr = fileSize;
        return 0;
    }
    if (fileSize < 128) {
        *outErr = MP3_ERR_UNSUPPORTED;
        return 0;
    }

    int mapErr = -1;
    const void *tailBytes = m_io->mapRangeSimple(fileSize - 128, 3, 0, &mapErr, NULL);
    *outErr = mapErr;
    if (tailBytes == NULL) {
        return 0;
    }

    m_tagState = (sce_paf_memcmp(tailBytes, "TAG", 3u) == 0) ? TAG_STATE_FOUND : TAG_STATE_NONE;
    *outErr = 0;
    return 1;
}

/* ===== Id3v2Header & Id3v2Reader (0x811FDCC4 - 0x811FE1B3) ===== */

static const char ID3V2_MAGIC[3] = { 'I', 'D', '3' };
static const char EA3_MAGIC[3] = { 'e', 'a', '3' };
static const char ID3V2_FOOTER_MAGIC[3] = { '3', 'D', 'I' };

static uint32_t decodeSynchsafeSize(const uint8_t *headerBytes)
{
    uint32_t total = 0;
    for (uint8_t idx = 0; idx < 4u; ++idx) {
        total = (total << 7) | (headerBytes[6u + idx] & bitMaskLow(7));
    }
    return total;
}

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

/* 0x811FE0F8 */
Id3v2Header::Id3v2Header()
    : m_isValid(0)
    , m_padValid(0)
{
}

/* 0x811FE018 */
uint32_t Id3v2Header::getTagBodySize() const
{
    return decodeSynchsafeSize(m_rawHeader);
}

/* 0x811FE042 */
bool Id3v2Header::isHeaderValid(const uint8_t *bytes)
{
    if (bytes == NULL) {
        return false;
    }
    if (sce_paf_memcmp(bytes, ID3V2_MAGIC, 3u) != 0
        && sce_paf_memcmp(bytes, EA3_MAGIC, 3u) != 0) {
        return false;
    }
    return decodeSynchsafeSize(bytes) != 0u;
}

/* 0x811FE0AA */
bool Id3v2Header::isFooterValid(const uint8_t *bytes)
{
    if (sce_paf_memcmp(bytes, ID3V2_FOOTER_MAGIC, 3u) != 0) {
        return false;
    }
    return decodeSynchsafeSize(bytes) != 0u;
}

/* 0x811FE0FE */
int Id3v2Header::parse(const uint8_t *bytes)
{
    bool valid = isHeaderValid(bytes);
    if (!valid && !isFooterValid(bytes)) {
        return 0;
    }
    sce_paf_memcpy(m_rawHeader, bytes, 10u);
    m_isValid = 1;
    return 1;
}

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

/* 0x811FDCC4 */
Id3v2Reader::Id3v2Reader(CachedStreamIo *io)
    : m_header()
    , m_locationKind(TAG_STATE_UNKNOWN)
    , m_extendedSize(0)
    , m_hasExtHdr(0)
    , m_tagOffset(-1)
    , m_headerPresent(TAG_STATE_UNKNOWN)
    , m_footerPresent(TAG_STATE_UNKNOWN)
    , m_io(io)
{
}

/* 0x811FDCF8 / 0x811FDD14 */
Id3v2Reader::~Id3v2Reader()
{
}

/* 0x811FDD34 */
int Id3v2Reader::readTag(uint8_t fromStart, int *outErr)
{
    if (m_io == NULL) {
        *outErr = -1;
        return 0;
    }

    int mapErr = -1;
    const uint8_t *mapped = NULL;
    if (fromStart != 0u) {
        if (m_headerPresent == TAG_STATE_FOUND) {
            *outErr = 0;
            return 1;
        }
        if (m_headerPresent == TAG_STATE_NONE) {
            *outErr = MP3_ERR_NOT_FOUND;
            return 0;
        }
        mapped = (const uint8_t *)m_io->mapRangeSimple(0, 10, 0, &mapErr, NULL);
    } else {
        if (m_footerPresent == TAG_STATE_FOUND) {
            *outErr = 0;
            return 1;
        }
        if (m_footerPresent == TAG_STATE_NONE) {
            *outErr = MP3_ERR_NOT_FOUND;
            return 0;
        }
        int fileSize = m_io->getFileSize();
        if (fileSize < 0) {
            mapErr = fileSize;
        } else {
            mapped = (const uint8_t *)m_io->mapRangeSimple(fileSize - 10, 10, 0, &mapErr, NULL);
        }
    }

    *outErr = mapErr;
    if (mapped == NULL) {
        if (mapErr == IO_ERR_EOF) {
            if (fromStart != 0u) {
                m_headerPresent = TAG_STATE_NONE;
            } else {
                m_footerPresent = TAG_STATE_NONE;
            }
            *outErr = MP3_ERR_UNSUPPORTED;
        }
        return 0;
    }

    if (m_header.parse(mapped) != 0) {
        if (fromStart != 0u) {
            m_headerPresent = TAG_STATE_FOUND;
            m_locationKind = 1;
            m_tagOffset = 0;
        } else {
            int fileSize = m_io->getFileSize();
            if (fileSize < 0) {
                *outErr = fileSize;
                return 0;
            }
            m_footerPresent = TAG_STATE_FOUND;
            m_locationKind = 2;
            m_tagOffset = fileSize - (int)m_header.getTagBodySize() - 20;
        }
        *outErr = 0;
        return 1;
    }

    if (fromStart != 0u) {
        const uint8_t *offsetMapped = (const uint8_t *)m_io->mapRangeSimple(128, 10, 0, &mapErr, NULL);
        *outErr = mapErr;
        if (offsetMapped != NULL) {
            if (m_header.parse(offsetMapped) != 0) {
                m_tagOffset = 128;
                m_headerPresent = TAG_STATE_FOUND;
                m_locationKind = 1;
                *outErr = 0;
                return 1;
            }
        } else if (mapErr != IO_ERR_EOF) {
            return 0;
        }
        m_headerPresent = TAG_STATE_NONE;
    } else {
        m_footerPresent = TAG_STATE_NONE;
    }

    *outErr = MP3_ERR_UNSUPPORTED;
    return 0;
}

/* 0x811FDED0 */
int Id3v2Reader::hasTag() const
{
    return (m_locationKind == 1 || m_locationKind == 2) ? 1 : 0;
}

/* 0x811FDEE0 */
int Id3v2Reader::probeTagPresence(uint8_t fromStart, int *outErr)
{
    if (m_io == NULL) {
        *outErr = -1;
        return 0;
    }
    if (fromStart != 0u) {
        if (m_headerPresent != TAG_STATE_UNKNOWN) {
            *outErr = 0;
            return 1;
        }
    } else if (m_footerPresent != TAG_STATE_UNKNOWN) {
        *outErr = 0;
        return 1;
    }

    int mapErr = -1;
    if (fromStart != 0u) {
        const uint8_t *mapped = (const uint8_t *)m_io->mapRangeSimple(0, 10, 0, &mapErr, NULL);
        *outErr = mapErr;
        if (mapped == NULL) {
            if (mapErr == IO_ERR_EOF) {
                m_headerPresent = TAG_STATE_NONE;
                *outErr = 0;
                return 1;
            }
            return 0;
        }
        m_headerPresent = Id3v2Header::isHeaderValid(mapped) ? TAG_STATE_FOUND : TAG_STATE_NONE;
        *outErr = 0;
        return 1;
    }

    int fileSize = m_io->getFileSize();
    if (fileSize < 0) {
        *outErr = fileSize;
        return 0;
    }
    if (fileSize < 10) {
        *outErr = MP3_ERR_UNSUPPORTED;
        return 0;
    }
    const uint8_t *mapped = (const uint8_t *)m_io->mapRangeSimple(fileSize - 10, 10, 0, &mapErr, NULL);
    *outErr = mapErr;
    if (mapped == NULL) {
        return 0;
    }
    m_footerPresent = Id3v2Header::isFooterValid(mapped) ? TAG_STATE_FOUND : TAG_STATE_NONE;
    *outErr = 0;
    return 1;
}

/* ===== Mp3FrameScanner (0x811FEEC8 - 0x811FF13F) ===== */

class Mp3FrameScanner {
public:
    explicit Mp3FrameScanner(CachedStreamIo *io);

    int findSyncFrame(int startOffset, int maxOffset, const uint32_t *refHeaderBits,
                      int confirmDepth, int fileLimit, int *outOffset,
                      Mp3HeaderHolder *outHeader, uint32_t *outMatchedDepth, int *outErr);

private:
    CachedStreamIo *m_io; /* +0 */
};

/* 0x811FEEC8 */
Mp3FrameScanner::Mp3FrameScanner(CachedStreamIo *io)
    : m_io(io)
{
}

/* 0x811FEECC */
int Mp3FrameScanner::findSyncFrame(int startOffset, int maxOffset, const uint32_t *refHeaderBits,
                                   int confirmDepth, int fileLimit, int *outOffset,
                                   Mp3HeaderHolder *outHeader, uint32_t *outMatchedDepth,
                                   int *outErr)
{
    if (m_io == NULL) {
        *outErr = -1;
        return 0;
    }
    if ((startOffset | maxOffset) < 0) {
        *outErr = IO_ERR_BAD_PARAM;
        return 0;
    }

    int blockCount = 0;
    uint32_t chunkSize = 0;
    m_io->getCacheGeometry(&blockCount, &chunkSize);
    if (startOffset > maxOffset) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    int curOffset = startOffset;
    uint32_t mappedSize = 0;
    int pinToken = 0;
    int mapErr = -1;
    const uint8_t *pinnedBase = NULL;
    const uint8_t *cursor = NULL;
    uint32_t consumed = 0;
    int validateErr = 0xFFFF;
    bool found = false;
    uint32_t matchedHeaderBits = 0;

    while (curOffset <= maxOffset) {
        if (curOffset >= fileLimit) {
            *outMatchedDepth = 0;
            *outErr = IO_ERR_EOF;
            if (pinnedBase != NULL) {
                m_io->unpinRange(pinToken, &mapErr);
            }
            return 0;
        }

        if (consumed + 4u < mappedSize) {
            ++cursor;
        } else {
            if (pinnedBase != NULL) {
                m_io->unpinRange(pinToken, &mapErr);
                mappedSize = 0;
            }
            uint32_t requestSize = chunkSize;
            if (curOffset + (int)chunkSize > fileLimit) {
                requestSize = (uint32_t)(fileLimit - curOffset);
            }
            pinnedBase = (const uint8_t *)m_io->mapRangePinned(
                curOffset, requestSize, 1, &pinToken, &mapErr, &mappedSize);
            *outErr = mapErr;
            if (pinnedBase == NULL) {
                return 0;
            }
            if (mapErr == 0) {
                mappedSize = requestSize;
            }
            consumed = 0;
            cursor = pinnedBase;
        }

        if (cursor == NULL) {
            if (pinnedBase != NULL) {
                m_io->unpinRange(pinToken, &mapErr);
            }
            return 0;
        }

        ++consumed;
        uint32_t candidateBits = readBigEndian32(cursor);
        if (mp3HeaderValidate(candidateBits, &validateErr) != 0
            && (refHeaderBits == NULL || mp3HeaderMatchesRef(candidateBits, *refHeaderBits) != 0)) {
            int frameSize = mp3HeaderFrameSize(candidateBits);
            int nextOffset = 0;
            if (confirmDepth == 0) {
                matchedHeaderBits = candidateBits;
                found = true;
                break;
            }
            if (findSyncFrame(curOffset + frameSize, curOffset + frameSize, refHeaderBits,
                              confirmDepth - 1, fileLimit, &nextOffset, NULL,
                              outMatchedDepth, outErr) != 0) {
                ++(*outMatchedDepth);
                matchedHeaderBits = candidateBits;
                found = true;
                break;
            }
            if (*outErr == IO_ERR_EOF) {
                matchedHeaderBits = candidateBits;
                found = true;
                break;
            }
            if (*outErr != MP3_ERR_NOT_FOUND) {
                if (pinnedBase != NULL) {
                    m_io->unpinRange(pinToken, &mapErr);
                }
                return 0;
            }
        }
        ++curOffset;
    }

    if (pinnedBase != NULL) {
        m_io->unpinRange(pinToken, &mapErr);
    }

    if (!found) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    if (outHeader != NULL && mp3HeaderHolderParse(outHeader, matchedHeaderBits) == 0) {
        *outErr = -1;
        return 0;
    }

    *outOffset = curOffset;
    *outErr = 0;
    return 1;
}

/* ===== Mp3FrameRecord & Mp3FirstFrameFinder (0x811FE1B4 - 0x811FE4B3) ===== */

class Mp3FrameRecord {
public:
    Mp3FrameRecord();
    virtual ~Mp3FrameRecord();

    int m_fileOffset;          /* +4 */
    Mp3HeaderHolder m_header;  /* +8..+15 */
};

/* 0x811FE1B4 */
Mp3FrameRecord::Mp3FrameRecord()
    : m_fileOffset(-1)
{
    mp3HeaderHolderInit(&m_header);
}

/* 0x811FE1D8 */
Mp3FrameRecord::~Mp3FrameRecord()
{
}

class Mp3FirstFrameFinder {
public:
    Mp3FirstFrameFinder(CachedStreamIo *io, Id3v2Reader *id3v2, Mp3FrameScanner *scanner);
    virtual ~Mp3FirstFrameFinder();

    int locateFirstFrame(int fileLimit, int *outErr);

    int getProbeState() const { return m_probeState; }
    int getFirstFrameOffset() const { return m_firstFrameOffset; }
    const Mp3HeaderHolder *getFirstHeader() const { return &m_firstHeader; }

private:
    int m_probeState;            /* +4 */
    int m_firstFrameOffset;      /* +8 */
    Mp3HeaderHolder m_firstHeader;/* +12..+19 */
    CachedStreamIo *m_io;        /* +20 */
    Id3v2Reader *m_id3v2;        /* +24 */
    Mp3FrameScanner *m_scanner;  /* +28 */
};

/* 0x811FE1F8 */
Mp3FirstFrameFinder::Mp3FirstFrameFinder(CachedStreamIo *io, Id3v2Reader *id3v2,
                                         Mp3FrameScanner *scanner)
    : m_probeState(TAG_STATE_UNKNOWN)
    , m_firstFrameOffset(-1)
    , m_io(io)
    , m_id3v2(id3v2)
    , m_scanner(scanner)
{
    mp3HeaderHolderInit(&m_firstHeader);
}

/* 0x811FE234 / 0x811FE252 */
Mp3FirstFrameFinder::~Mp3FirstFrameFinder()
{
}

/* 0x811FE276 */
int Mp3FirstFrameFinder::locateFirstFrame(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_id3v2 == NULL || m_scanner == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_probeState == TAG_STATE_NONE) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }
    if (m_probeState == TAG_STATE_FOUND) {
        *outErr = 0;
        return 1;
    }

    if (m_id3v2->hasTag() == 0 && m_id3v2->readTag(1u, outErr) == 0) {
        if (*outErr != MP3_ERR_UNSUPPORTED && *outErr != MP3_ERR_NOT_FOUND) {
            return 0;
        }
    }

    int scanStart = 0;
    if (m_id3v2->getLocationKind() == 1) {
        scanStart = (int)m_id3v2->getHeaderPtr()->getTagBodySize() + 10;
    }

    int blockCount = 0;
    uint32_t chunkSize = 0;
    m_io->getCacheGeometry(&blockCount, &chunkSize);

    Id3v2Header nestedHeader;
    const uint8_t *windowPtr = NULL;
    int windowStart = -1;
    int windowSize = 0;
    int mapErr = -1;

    for (int probeIdx = 0; probeIdx < 128; ++probeIdx) {
        int probePos = scanStart + probeIdx;
        if (probePos < 0) {
            *outErr = MP3_ERR_CORRUPT;
            return 0;
        }

        const uint8_t *samplePtr = NULL;
        if (windowStart >= 0 && (uint32_t)(probePos + 10) <= (uint32_t)(windowStart + windowSize)) {
            samplePtr = windowPtr + (probePos - windowStart);
        } else {
            windowPtr = (const uint8_t *)m_io->mapRangeSimple(
                probePos, (int)chunkSize, 0, &mapErr, &windowSize);
            *outErr = mapErr;
            if (windowPtr == NULL) {
                if (mapErr == IO_ERR_EOF) {
                    *outErr = MP3_ERR_NOT_FOUND;
                }
                return 0;
            }
            windowStart = probePos;
            samplePtr = windowPtr;
            if (mapErr == 0) {
                windowSize = (int)chunkSize;
            }
        }

        if (nestedHeader.parse(samplePtr) == 1) {
            scanStart += (int)nestedHeader.getTagBodySize();
            probeIdx = 0;
        }
    }

    int foundOffset = -1;
    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(scanStart, scanStart + 14410, NULL, 0, fileLimit,
                                 &foundOffset, &m_firstHeader, &matchedDepth, outErr) == 0) {
        m_probeState = TAG_STATE_NONE;
        if (*outErr == IO_ERR_EOF || *outErr == MP3_ERR_NOT_FOUND) {
            *outErr = MP3_ERR_NOT_FOUND;
        }
        return 0;
    }

    m_probeState = TAG_STATE_FOUND;
    m_firstFrameOffset = foundOffset;
    *outErr = 0;
    return 1;
}

/* ===== VbriTagReader (0x811FF140 - 0x811FF54F) ===== */

class VbriTagReader {
public:
    VbriTagReader(CachedStreamIo *io, Mp3FirstFrameFinder *firstFinder);
    virtual ~VbriTagReader();

    int readHeader(int fileLimit, int *outErr);
    int readToc(int fileLimit, int *outErr);

    const VbriHeaderData *getHeader() const { return &m_vbriHeader; }
    const uint32_t *getTocTable() const { return m_tocTable; }

private:
    int m_headerState;                 /* +4 */
    VbriHeaderData m_vbriHeader;       /* +8..+35 */
    int m_headerOffset;                /* +36 */
    int m_tocState;                    /* +40 */
    uint32_t *m_tocTable;              /* +44 */
    CachedStreamIo *m_io;              /* +48 */
    Mp3FirstFrameFinder *m_firstFinder;/* +52 */
};

/* 0x811FF140 */
VbriTagReader::VbriTagReader(CachedStreamIo *io, Mp3FirstFrameFinder *firstFinder)
    : m_headerState(TAG_STATE_UNKNOWN)
    , m_headerOffset(-1)
    , m_tocState(TAG_STATE_UNKNOWN)
    , m_tocTable(NULL)
    , m_io(io)
    , m_firstFinder(firstFinder)
{
}

/* 0x811FF164 / 0x811FF184 */
VbriTagReader::~VbriTagReader()
{
    if (m_tocTable != NULL) {
        delete[] m_tocTable;
        m_tocTable = NULL;
    }
}

/* 0x811FF1A8 */
int VbriTagReader::readHeader(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_firstFinder == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_headerState == TAG_STATE_FOUND) {
        *outErr = 0;
        return 1;
    }
    if (m_headerState == TAG_STATE_NONE) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    if (m_firstFinder->locateFirstFrame(fileLimit, outErr) == 0) {
        return 0;
    }

    int curOffset = m_firstFinder->getFirstFrameOffset() + 4;
    int blockCount = 0;
    uint32_t chunkSize = 0;
    m_io->getCacheGeometry(&blockCount, &chunkSize);

    int windowStart = -1;
    int windowSize = 0;
    const uint8_t *cursor = NULL;

    for (uint32_t step = 0; step < 0x5A1u; ++step) {
        if (curOffset < 0) {
            *outErr = MP3_ERR_CORRUPT;
            return 0;
        }
        int mapErr = -1;
        if (windowStart >= 0 && (uint32_t)(curOffset + 26) <= (uint32_t)(windowStart + windowSize)) {
            ++cursor;
        } else {
            cursor = (const uint8_t *)m_io->mapRangeSimple(
                curOffset, (int)chunkSize, 0, &mapErr, &windowSize);
            *outErr = mapErr;
            if (cursor == NULL) {
                break;
            }
            windowStart = curOffset;
            if (mapErr == 0) {
                windowSize = (int)chunkSize;
            }
        }
        if (vbriHeaderParse(&m_vbriHeader, cursor) != 0) {
            m_headerOffset = curOffset;
            m_headerState = TAG_STATE_FOUND;
            *outErr = 0;
            return 1;
        }
        ++curOffset;
    }

    m_headerState = TAG_STATE_NONE;
    if (*outErr == IO_ERR_EOF) {
        *outErr = MP3_ERR_NOT_FOUND;
    }
    return 0;
}

/* 0x811FF2F8 */
int VbriTagReader::readToc(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_firstFinder == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_tocState == TAG_STATE_FOUND) {
        *outErr = 0;
        return 1;
    }
    if (m_tocState == TAG_STATE_NONE) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }
    if (readHeader(fileLimit, outErr) == 0) {
        return 0;
    }

    uint16_t entrySize = vbriHeaderGetTocEntrySize(&m_vbriHeader);
    if (entrySize > 4u) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    uint16_t entryCount = vbriHeaderGetTocEntries(&m_vbriHeader);
    m_tocTable = allocUint32ArrayCount((size_t)entryCount + 1u);
    if (m_tocTable == NULL) {
        *outErr = MP3_ERR_NO_MEMORY;
        return 0;
    }

    int curOffset = m_headerOffset + 26;
    int blockCount = 0;
    uint32_t chunkSize = 0;
    m_io->getCacheGeometry(&blockCount, &chunkSize);

    int windowStart = -1;
    int windowSize = 0;
    const uint8_t *cursor = NULL;
    uint16_t scale = vbriHeaderGetTocScale(&m_vbriHeader);

    for (uint32_t idx = 0; idx <= (uint32_t)entryCount; ++idx) {
        if (curOffset < 0) {
            *outErr = MP3_ERR_CORRUPT;
            return 0;
        }

        int mapErr = -1;
        if (windowStart >= 0 && (uint32_t)(curOffset + entrySize) <= (uint32_t)(windowStart + windowSize)) {
            cursor += entrySize;
        } else {
            cursor = (const uint8_t *)m_io->mapRangeSimple(curOffset, (int)chunkSize, 0, &mapErr, NULL);
            *outErr = mapErr;
            if (cursor == NULL) {
                delete[] m_tocTable;
                m_tocTable = NULL;
                return 0;
            }
            windowStart = curOffset;
            if (mapErr == 0) {
                windowSize = (int)chunkSize;
            }
        }

        uint32_t rawValue = 0;
        if (entrySize == 1u) {
            rawValue = cursor[0];
        } else if (entrySize == 2u) {
            rawValue = readBigEndian16(cursor);
        } else if (entrySize == 3u) {
            rawValue = readBigEndian24(cursor);
        } else if (entrySize == 4u) {
            rawValue = readBigEndian32(cursor);
        } else {
            delete[] m_tocTable;
            m_tocTable = NULL;
            *outErr = -1;
            return 0;
        }

        int scaledValue = (int)(rawValue * scale);
        m_tocTable[idx] = (uint32_t)scaledValue;
        if (scaledValue < 0) {
            delete[] m_tocTable;
            m_tocTable = NULL;
            *outErr = MP3_ERR_CORRUPT;
            return 0;
        }
        curOffset += entrySize;
    }

    m_tocState = TAG_STATE_FOUND;
    *outErr = 0;
    return 1;
}

/* ===== XingTagReader (0x811FF550 - 0x811FF92F) ===== */

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
    int m_headerState;                 /* +4 */
    XingHeaderData m_xingHeader;       /* +8..+27 */
    int m_headerOffset;                /* +28 */
    int m_tocState;                    /* +32 */
    uint8_t *m_tocBytes;               /* +36 */
    CachedStreamIo *m_io;              /* +40 */
    Mp3FirstFrameFinder *m_firstFinder;/* +44 */
    int m_encoderDelay;                /* +48 */
    int m_encoderPadding;              /* +52 */
};

/* 0x811FF550 */
XingTagReader::XingTagReader(CachedStreamIo *io, Mp3FirstFrameFinder *firstFinder)
    : m_headerState(TAG_STATE_UNKNOWN)
    , m_headerOffset(-1)
    , m_tocState(TAG_STATE_UNKNOWN)
    , m_tocBytes(NULL)
    , m_io(io)
    , m_firstFinder(firstFinder)
    , m_encoderDelay(-1)
    , m_encoderPadding(-1)
{
}

/* 0x811FF578 / 0x811FF598 */
XingTagReader::~XingTagReader()
{
    if (m_tocBytes != NULL) {
        delete[] m_tocBytes;
        m_tocBytes = NULL;
    }
}

/* 0x811FF5BC */
int XingTagReader::readHeader(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_firstFinder == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_headerState == TAG_STATE_FOUND) {
        *outErr = 0;
        return 1;
    }
    if (m_headerState == TAG_STATE_NONE) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }
    if (m_firstFinder->locateFirstFrame(fileLimit, outErr) == 0) {
        return 0;
    }

    int curOffset = m_firstFinder->getFirstFrameOffset() + 4;
    int blockCount = 0;
    uint32_t chunkSize = 0;
    m_io->getCacheGeometry(&blockCount, &chunkSize);

    int windowStart = -1;
    int windowSize = 0;
    const uint8_t *cursor = NULL;

    for (uint8_t step = 0; step < 255u; ++step) {
        if (curOffset < 0) {
            *outErr = MP3_ERR_CORRUPT;
            return 0;
        }
        int mapErr = -1;
        if (windowStart >= 0 && (uint32_t)(curOffset + 16) <= (uint32_t)(windowStart + windowSize)) {
            ++cursor;
        } else {
            cursor = (const uint8_t *)m_io->mapRangeSimple(
                curOffset, (int)chunkSize, 0, &mapErr, &windowSize);
            *outErr = mapErr;
            if (cursor == NULL) {
                break;
            }
            windowStart = curOffset;
            if (mapErr == 0) {
                windowSize = (int)chunkSize;
            }
        }
        if (xingHeaderParse(&m_xingHeader, cursor) != 0) {
            m_headerOffset = curOffset;
            m_headerState = TAG_STATE_FOUND;
            *outErr = 0;
            return 1;
        }
        ++curOffset;
    }

    m_headerState = TAG_STATE_NONE;
    if (*outErr == IO_ERR_EOF) {
        *outErr = MP3_ERR_NOT_FOUND;
    }
    return 0;
}

/* 0x811FF706 */
int XingTagReader::readToc(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_firstFinder == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_tocState == TAG_STATE_FOUND) {
        *outErr = 0;
        return 1;
    }
    if (m_tocState == TAG_STATE_NONE) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }
    if (readHeader(fileLimit, outErr) == 0) {
        return 0;
    }
    if ((m_xingHeader.rawBytes[7] & 0x04u) == 0u) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }
    if (m_headerOffset + 16 < 0) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    int mapErr = -1;
    const void *tocSrc = m_io->mapRangeSimple(m_headerOffset + 16, 100, 0, &mapErr, NULL);
    *outErr = mapErr;
    if (tocSrc == NULL) {
        return 0;
    }

    m_tocBytes = allocUint8Array(100u);
    if (m_tocBytes == NULL) {
        *outErr = MP3_ERR_NO_MEMORY;
        return 0;
    }
    sce_paf_memcpy(m_tocBytes, tocSrc, 100u);
    m_tocState = TAG_STATE_FOUND;
    *outErr = 0;
    return 1;
}

/* 0x811FF7DE */
int XingTagReader::readLameDelays(int fileLimit, int *outErr)
{
    if (m_io == NULL || m_firstFinder == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_encoderDelay >= 0 || m_encoderPadding >= 0) {
        *outErr = 0;
        return 1;
    }
    if (readHeader(fileLimit, outErr) == 0) {
        return 0;
    }
    if (m_headerOffset + 141 < 0) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    int mapErr = -1;
    const uint8_t *delayBytes = (const uint8_t *)m_io->mapRangeSimple(
        m_headerOffset + 141, 3, 0, &mapErr, NULL);
    *outErr = mapErr;
    if (delayBytes == NULL) {
        m_encoderDelay = 0;
        m_encoderPadding = 0;
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    m_encoderDelay = ((int)delayBytes[0] << 4) | (delayBytes[1] >> 4);
    m_encoderPadding = (((int)delayBytes[1] & 0x0F) << 8) | delayBytes[2];
    *outErr = 0;
    return 1;
}

/* 0x811FF8A0 */
uint16_t XingTagReader::getEncoderDelay(int fileLimit)
{
    if (m_encoderDelay >= 0) {
        return (uint16_t)m_encoderDelay;
    }
    int err = -1;
    if (readLameDelays(fileLimit, &err) != 0) {
        return (uint16_t)m_encoderDelay;
    }
    return 0;
}

/* 0x811FF8E8 */
uint16_t XingTagReader::getEncoderPadding(int fileLimit)
{
    if (m_encoderPadding >= 0) {
        return (uint16_t)m_encoderPadding;
    }
    int err = -1;
    if (readLameDelays(fileLimit, &err) != 0) {
        return (uint16_t)m_encoderPadding;
    }
    return 0;
}

/* ===== Mp3CbrSeeker / Mp3VbriSeeker / Mp3XingSeeker / Mp3SeekRouter (0x811FE4B4 - 0x811FECCF) ===== */

class Mp3CbrSeeker {
public:
    Mp3CbrSeeker(Mp3FirstFrameFinder *firstFinder, Mp3FrameScanner *scanner);
    virtual ~Mp3CbrSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 Mp3HeaderHolder *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    Mp3FrameScanner *m_scanner;         /* +8 */
};

/* 0x811FE5B4 */
Mp3CbrSeeker::Mp3CbrSeeker(Mp3FirstFrameFinder *firstFinder, Mp3FrameScanner *scanner)
    : m_firstFinder(firstFinder)
    , m_scanner(scanner)
{
}

/* 0x811FE5C8 / 0x811FE5D6 */
Mp3CbrSeeker::~Mp3CbrSeeker()
{
}

/* 0x811FE5EC */
int Mp3CbrSeeker::seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                           Mp3HeaderHolder *outHeader, int *outErr)
{
    if (m_firstFinder == NULL || m_scanner == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_firstFinder->getProbeState() == TAG_STATE_UNKNOWN
        && m_firstFinder->locateFirstFrame(fileLimit, outErr) == 0) {
        return 0;
    }
    if (m_firstFinder->getProbeState() != TAG_STATE_FOUND) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    const Mp3HeaderHolder *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t bitRate = mp3HeaderBitRate(firstHdr);
    int byteOffset = m_firstFinder->getFirstFrameOffset()
                   + (int)((((float)targetMs * (float)bitRate) * 0.125f) / 1000.0f);
    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(byteOffset, byteOffset + 2881, &firstHdr->headerBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

class Mp3VbriSeeker {
public:
    Mp3VbriSeeker(Mp3FirstFrameFinder *firstFinder, VbriTagReader *vbriReader,
                  Mp3FrameScanner *scanner);
    virtual ~Mp3VbriSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 Mp3HeaderHolder *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    VbriTagReader *m_vbriReader;        /* +8 */
    Mp3FrameScanner *m_scanner;         /* +12 */
};

/* 0x811FE724 */
Mp3VbriSeeker::Mp3VbriSeeker(Mp3FirstFrameFinder *firstFinder, VbriTagReader *vbriReader,
                             Mp3FrameScanner *scanner)
    : m_firstFinder(firstFinder)
    , m_vbriReader(vbriReader)
    , m_scanner(scanner)
{
}

/* 0x811FE73C / 0x811FE74A */
Mp3VbriSeeker::~Mp3VbriSeeker()
{
}

/* 0x811FE760 */
int Mp3VbriSeeker::seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                            Mp3HeaderHolder *outHeader, int *outErr)
{
    if (m_firstFinder == NULL || m_vbriReader == NULL || m_scanner == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_vbriReader->readToc(fileLimit, outErr) == 0) {
        if (*outErr == IO_ERR_EOF) {
            *outErr = MP3_ERR_NOT_FOUND;
        }
        return 0;
    }

    const VbriHeaderData *vbriHdr = m_vbriReader->getHeader();
    uint16_t framesPerEntry = vbriHeaderGetTocFramesPerEntry(vbriHdr);
    if (framesPerEntry == 0u) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    const Mp3HeaderHolder *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t totalFrames = vbriHeaderGetFrameCount(vbriHdr);
    uint32_t samplesPerFrame = (uint32_t)mp3HeaderSamplesPerFrame(firstHdr->headerBits);
    uint32_t sampleRate = mp3HeaderSampleRate(firstHdr);
    float totalDurationMs = ((float)(totalFrames * samplesPerFrame) / (float)sampleRate) * 1000.0f;
    if (totalDurationMs == 0.0f) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }
    if (targetMs > (uint32_t)totalDurationMs) {
        *outErr = IO_ERR_EOF;
        return 0;
    }

    uint32_t tocCount = (uint32_t)vbriHeaderGetTocEntries(vbriHdr) + 1u;
    float msPerEntry = totalDurationMs / (float)tocCount;
    const uint32_t *tocTable = m_vbriReader->getTocTable();

    float accumMs = 0.0f;
    float targetFloat = (float)targetMs;
    uint32_t byteAccum = 0;
    int entryIdx = 0;
    while (entryIdx < (int)tocCount) {
        accumMs += msPerEntry;
        byteAccum += tocTable[entryIdx];
        if (accumMs >= targetFloat) {
            break;
        }
        ++entryIdx;
    }

    float frac = ((accumMs - targetFloat) / msPerEntry) + (1.0f / ((float)framesPerEntry * 2.0f));
    float frameUnits = frac * (float)framesPerEntry;
    uint32_t entryBytes = (entryIdx <= 0) ? tocTable[entryIdx] : tocTable[entryIdx - 1];
    int bytePos = (int)byteAccum - (int)(((float)entryBytes * frameUnits) / (float)framesPerEntry);

    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(bytePos, bytePos + 1441, &firstHdr->headerBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

class Mp3XingSeeker {
public:
    Mp3XingSeeker(Mp3FirstFrameFinder *firstFinder, XingTagReader *xingReader,
                  Mp3FrameScanner *scanner);
    virtual ~Mp3XingSeeker();

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 Mp3HeaderHolder *outHeader, int *outErr);

private:
    Mp3FirstFrameFinder *m_firstFinder; /* +4 */
    XingTagReader *m_xingReader;        /* +8 */
    Mp3FrameScanner *m_scanner;         /* +12 */
};

/* 0x811FE9EC */
Mp3XingSeeker::Mp3XingSeeker(Mp3FirstFrameFinder *firstFinder, XingTagReader *xingReader,
                             Mp3FrameScanner *scanner)
    : m_firstFinder(firstFinder)
    , m_xingReader(xingReader)
    , m_scanner(scanner)
{
}

/* 0x811FEA04 / 0x811FEA12 */
Mp3XingSeeker::~Mp3XingSeeker()
{
}

/* 0x811FEA28 */
int Mp3XingSeeker::seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                            Mp3HeaderHolder *outHeader, int *outErr)
{
    if (m_firstFinder == NULL || m_xingReader == NULL || m_scanner == NULL) {
        *outErr = -1;
        return 0;
    }
    if (m_xingReader->readToc(fileLimit, outErr) == 0) {
        if (*outErr == IO_ERR_EOF) {
            *outErr = MP3_ERR_NOT_FOUND;
        }
        return 0;
    }

    const XingHeaderData *xingHdr = m_xingReader->getHeader();
    uint8_t flagsByte = xingHdr->rawBytes[7];
    if ((flagsByte & 0x07u) != 0x07u) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    uint32_t rawFrames = 0;
    sce_paf_memcpy(&rawFrames, xingHdr->rawBytes + 8, 4u);
    uint32_t totalFrames = swapBigEndian32(rawFrames);

    const Mp3HeaderHolder *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t samplesPerFrame = (uint32_t)mp3HeaderSamplesPerFrame(firstHdr->headerBits);
    uint32_t sampleRate = mp3HeaderSampleRate(firstHdr);
    float totalDurationMs = (((float)totalFrames * (float)samplesPerFrame) / (float)sampleRate) * 1000.0f;
    if (totalDurationMs == 0.0f) {
        *outErr = MP3_ERR_UNSUPPORTED;
        return 0;
    }
    if (targetMs > (uint32_t)totalDurationMs) {
        *outErr = IO_ERR_EOF;
        return 0;
    }

    uint8_t pctIndex = (uint8_t)(uint32_t)(((float)targetMs / totalDurationMs) * 100.0f);
    float interpOffset = 0.0f;
    float remainderMs = (float)targetMs - ((totalDurationMs * (float)pctIndex) / 100.0f);
    const uint8_t *tocBytes = m_xingReader->getTocBytes();
    if (pctIndex < 99u) {
        float delta = (float)((int)tocBytes[pctIndex + 1u] - (int)tocBytes[pctIndex]);
        interpOffset = remainderMs * (delta / (totalDurationMs / 100.0f));
    }

    uint32_t rawBytes = 0;
    sce_paf_memcpy(&rawBytes, xingHdr->rawBytes + 12, 4u);
    int totalBytes = (int)swapBigEndian32(rawBytes);
    if (totalBytes < 0) {
        *outErr = totalBytes;
        return 0;
    }

    float tocPos = (float)tocBytes[pctIndex] + interpOffset;
    int bytePos = m_firstFinder->getFirstFrameOffset()
                + (int)((tocPos * (float)totalBytes) * 0.00390625f);
    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(bytePos, bytePos + 1441, &firstHdr->headerBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

class Mp3SeekRouter {
public:
    Mp3SeekRouter(Mp3CbrSeeker *cbr, Mp3XingSeeker *xing, Mp3VbriSeeker *vbri);

    int seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                 Mp3HeaderHolder *outHeader, bool *outAtEnd, int *outErr);

private:
    Mp3CbrSeeker *m_cbrSeeker;   /* +0 */
    Mp3XingSeeker *m_xingSeeker; /* +4 */
    Mp3VbriSeeker *m_vbriSeeker; /* +8 */
};

/* 0x811FE4B4 */
Mp3SeekRouter::Mp3SeekRouter(Mp3CbrSeeker *cbr, Mp3XingSeeker *xing, Mp3VbriSeeker *vbri)
    : m_cbrSeeker(cbr)
    , m_xingSeeker(xing)
    , m_vbriSeeker(vbri)
{
}

/* 0x811FE4BC */
int Mp3SeekRouter::seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                            Mp3HeaderHolder *outHeader, bool *outAtEnd, int *outErr)
{
    if (m_cbrSeeker == NULL || m_xingSeeker == NULL || m_vbriSeeker == NULL) {
        *outErr = -1;
        return 0;
    }

    if (m_xingSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
        *outAtEnd = (fileLimit <= *outOffset + mp3HeaderFrameSize(outHeader->headerBits));
        *outErr = 0;
        return 1;
    }
    if (*outErr != IO_ERR_EOF) {
        if (m_vbriSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
            *outAtEnd = (fileLimit <= *outOffset + mp3HeaderFrameSize(outHeader->headerBits));
            *outErr = 0;
            return 1;
        }
        if (*outErr != IO_ERR_EOF
            && m_cbrSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
            *outAtEnd = (fileLimit <= *outOffset + mp3HeaderFrameSize(outHeader->headerBits));
            *outErr = 0;
            return 1;
        }
    }
    return 0;
}

/* ===== Mp3FrameReader & Mp3NextFrameLocator (0x811FECD0 - 0x811FEEC7) ===== */

struct Mp3MappedFrame {
    void *frameData;          /* +0 */
    uint32_t frameSize;       /* +4 */
    uint32_t sampleRate;      /* +8 */
    uint32_t samplesPerFrame; /* +12 */
    uint8_t channelMode;      /* +16 */
    uint8_t padChan[5];       /* +17..+21 */
    uint8_t mpegVersion;      /* +22 */
};

class Mp3FrameReader {
public:
    explicit Mp3FrameReader(CachedStreamIo *io);
    virtual ~Mp3FrameReader();

    int readFrameData(int fileOffset, uint32_t frameSize, uint16_t sampleRate,
                      uint16_t samplesPerFrame, uint8_t channelMode,
                      uint8_t mpegVersion, Mp3MappedFrame *outFrame, int *outErr);

private:
    CachedStreamIo *m_io; /* +4 */
};

/* 0x811FECD0 */
Mp3FrameReader::Mp3FrameReader(CachedStreamIo *io)
    : m_io(io)
{
}

/* 0x811FECE4 / 0x811FECF2 */
Mp3FrameReader::~Mp3FrameReader()
{
}

/* 0x811FED08 */
int Mp3FrameReader::readFrameData(int fileOffset, uint32_t frameSize, uint16_t sampleRate,
                                  uint16_t samplesPerFrame, uint8_t channelMode,
                                  uint8_t mpegVersion, Mp3MappedFrame *outFrame, int *outErr)
{
    if (m_io == NULL) {
        *outErr = -1;
        return 0;
    }
    if (fileOffset < 0) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    int mapErr = -1;
    int pinToken = 0;
    uint32_t mappedSize = 0;
    void *dataPtr = m_io->mapRangePinned(fileOffset, frameSize, 0, &pinToken, &mapErr, &mappedSize);
    *outErr = mapErr;
    if (dataPtr == NULL) {
        return 0;
    }

    outFrame->frameData = dataPtr;
    outFrame->frameSize = frameSize;
    outFrame->sampleRate = sampleRate;
    outFrame->samplesPerFrame = samplesPerFrame;
    outFrame->channelMode = channelMode;
    outFrame->mpegVersion = mpegVersion;
    *outErr = 0;
    return 1;
}

class Mp3NextFrameLocator {
public:
    explicit Mp3NextFrameLocator(Mp3FrameScanner *scanner);
    virtual ~Mp3NextFrameLocator();

    int locateNextFrame(const Mp3FrameRecord *curRecord, int fileLimit, int *outOffset,
                        Mp3HeaderHolder *outHeader, uint32_t *outMatchedDepth, int *outErr);

private:
    Mp3FrameScanner *m_scanner; /* +4 */
};

/* 0x811FEDD4 */
Mp3NextFrameLocator::Mp3NextFrameLocator(Mp3FrameScanner *scanner)
    : m_scanner(scanner)
{
}

/* 0x811FEDE8 / 0x811FEDF6 */
Mp3NextFrameLocator::~Mp3NextFrameLocator()
{
}

/* 0x811FEE0C */
int Mp3NextFrameLocator::locateNextFrame(const Mp3FrameRecord *curRecord, int fileLimit,
                                         int *outOffset, Mp3HeaderHolder *outHeader,
                                         uint32_t *outMatchedDepth, int *outErr)
{
    if (m_scanner == NULL) {
        *outErr = -1;
        return 0;
    }
    if (curRecord->m_fileOffset < 0) {
        *outErr = MP3_ERR_BAD_OFFSET;
        return 0;
    }

    int nextSearchStart = curRecord->m_fileOffset + mp3HeaderFrameSize(curRecord->m_header.headerBits);
    int foundOffset = -1;
    if (m_scanner->findSyncFrame(nextSearchStart, nextSearchStart + 2881,
                                 &curRecord->m_header.headerBits, 2, fileLimit,
                                 &foundOffset, outHeader, outMatchedDepth, outErr) == 0) {
        return 0;
    }

    *outOffset = foundOffset;
    *outErr = 0;
    return 1;
}

} /* namespace MusicCore */
} /* namespace sce */
