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

#include "musiccore_mp3_readers.h"

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

/* ===== MediaInfoBase (0x811FDB5C - 0x811FDBBF) ===== */

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

/* 0x811FE0F8 */
Id3v2Header::Id3v2Header()
    : m_isValid(0)
    , m_padValid(0)
{
}

/* 0x811FE018 */
uint32_t Id3v2Header::getTagBodySize() const
{
    uint32_t total = 0;
    for (uint8_t idx = 0; idx < 4u; ++idx) {
        total = (total << 7) | (m_rawHeader[6u + idx] & makeBitMask(7u));
    }
    return total;
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
    uint32_t total = 0;
    for (uint8_t idx = 0; idx < 4u; ++idx) {
        total = (total << 7) | (bytes[6u + idx] & makeBitMask(7u));
    }
    return total != 0u;
}

/* 0x811FE0AA */
bool Id3v2Header::isFooterValid(const uint8_t *bytes)
{
    if (sce_paf_memcmp(bytes, ID3V2_FOOTER_MAGIC, 3u) != 0) {
        return false;
    }
    uint32_t total = 0;
    for (uint8_t idx = 0; idx < 4u; ++idx) {
        total = (total << 7) | (bytes[6u + idx] & makeBitMask(7u));
    }
    return total != 0u;
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

/* 0x811FEEC8 */
Mp3FrameScanner::Mp3FrameScanner(CachedStreamIo *io)
    : m_io(io)
{
}

/* 0x811FEECC */
int Mp3FrameScanner::findSyncFrame(int startOffset, int maxOffset, const uint32_t *refHeaderBits,
                                   int confirmDepth, int fileLimit, int *outOffset,
                                   MpegFrameHeader *outHeader, uint32_t *outMatchedDepth,
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
                m_io->checkRangeReadable(pinToken, &mapErr);
            }
            return 0;
        }

        if (consumed + 4u < mappedSize) {
            ++cursor;
        } else {
            if (pinnedBase != NULL) {
                m_io->checkRangeReadable(pinToken, &mapErr);
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
                m_io->checkRangeReadable(pinToken, &mapErr);
            }
            return 0;
        }

        ++consumed;
        uint32_t candidateBits = readBigEndian32(cursor);
        if (validateMpegLayer3Header(candidateBits, &validateErr) != 0
            && (refHeaderBits == NULL || hasSameMpegSampleRate(candidateBits, *refHeaderBits) != 0)) {
            int frameSize = (int)getMpegLayer3FrameSize(candidateBits);
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
                    m_io->checkRangeReadable(pinToken, &mapErr);
                }
                return 0;
            }
        }
        ++curOffset;
    }

    if (pinnedBase != NULL) {
        m_io->checkRangeReadable(pinToken, &mapErr);
    }

    if (!found) {
        *outErr = MP3_ERR_NOT_FOUND;
        return 0;
    }

    if (outHeader != NULL && outHeader->parse(matchedHeaderBits) == 0) {
        *outErr = -1;
        return 0;
    }

    *outOffset = curOffset;
    *outErr = 0;
    return 1;
}

/* ===== Mp3FrameRecord & Mp3FirstFrameFinder (0x811FE1B4 - 0x811FE4B3) ===== */

/* 0x811FE1B4 */
Mp3FrameRecord::Mp3FrameRecord()
    : m_fileOffset(-1)
    , m_header()
{
}

/* 0x811FE1D8 */
Mp3FrameRecord::~Mp3FrameRecord()
{
}

/* 0x811FE1F8 */
Mp3FirstFrameFinder::Mp3FirstFrameFinder(CachedStreamIo *io, Id3v2Reader *id3v2,
                                         Mp3FrameScanner *scanner)
    : m_probeState(TAG_STATE_UNKNOWN)
    , m_firstFrameOffset(-1)
    , m_firstHeader()
    , m_io(io)
    , m_id3v2(id3v2)
    , m_scanner(scanner)
{
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
        if (m_vbriHeader.parse(cursor) != 0) {
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

    uint16_t entrySize = m_vbriHeader.getTocEntrySize();
    if (entrySize > 4u) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    uint16_t entryCount = m_vbriHeader.getTocEntryCount();
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
    uint16_t scale = m_vbriHeader.getTocScaleFactor();

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
        if (m_xingHeader.parse(cursor) != 0) {
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
                           MpegFrameHeader *outHeader, int *outErr)
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

    const MpegFrameHeader *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t bitRate = (uint32_t)firstHdr->getBitrate();
    int byteOffset = m_firstFinder->getFirstFrameOffset()
                   + (int)((((float)targetMs * (float)bitRate) * 0.125f) / 1000.0f);
    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(byteOffset, byteOffset + 2881, &firstHdr->rawBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

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
                            MpegFrameHeader *outHeader, int *outErr)
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
    uint16_t framesPerEntry = vbriHdr->getTocFramesPerEntry();
    if (framesPerEntry == 0u) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }

    const MpegFrameHeader *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t totalFrames = vbriHdr->getFrameCount();
    uint32_t samplesPerFrame = (uint32_t)getMpegLayer3SamplesPerFrame(firstHdr->rawBits);
    uint32_t sampleRate = (uint32_t)firstHdr->getSampleRate();
    float totalDurationMs = ((float)(totalFrames * samplesPerFrame) / (float)sampleRate) * 1000.0f;
    if (totalDurationMs == 0.0f) {
        *outErr = MP3_ERR_CORRUPT;
        return 0;
    }
    if (targetMs > (uint32_t)totalDurationMs) {
        *outErr = IO_ERR_EOF;
        return 0;
    }

    uint32_t tocCount = (uint32_t)vbriHdr->getTocEntryCount() + 1u;
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
    if (m_scanner->findSyncFrame(bytePos, bytePos + 1441, &firstHdr->rawBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

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
                            MpegFrameHeader *outHeader, int *outErr)
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

    uint32_t totalFrames = readBigEndian32(xingHdr->rawBytes + 8);

    const MpegFrameHeader *firstHdr = m_firstFinder->getFirstHeader();
    uint32_t samplesPerFrame = (uint32_t)getMpegLayer3SamplesPerFrame(firstHdr->rawBits);
    uint32_t sampleRate = (uint32_t)firstHdr->getSampleRate();
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

    int totalBytes = (int)readBigEndian32(xingHdr->rawBytes + 12);
    if (totalBytes < 0) {
        *outErr = totalBytes;
        return 0;
    }

    float tocPos = (float)tocBytes[pctIndex] + interpOffset;
    int bytePos = m_firstFinder->getFirstFrameOffset()
                + (int)((tocPos * (float)totalBytes) * 0.00390625f);
    uint32_t matchedDepth = 0;
    if (m_scanner->findSyncFrame(bytePos, bytePos + 1441, &firstHdr->rawBits,
                                 3, fileLimit, outOffset, outHeader, &matchedDepth, outErr) != 0) {
        *outErr = 0;
        return 1;
    }
    *outOffset = -1;
    return 0;
}

/* 0x811FE4B4 */
Mp3SeekRouter::Mp3SeekRouter(Mp3CbrSeeker *cbr, Mp3XingSeeker *xing, Mp3VbriSeeker *vbri)
    : m_cbrSeeker(cbr)
    , m_xingSeeker(xing)
    , m_vbriSeeker(vbri)
{
}

/* 0x811FE4BC */
int Mp3SeekRouter::seekToMs(uint32_t targetMs, int fileLimit, int *outOffset,
                            MpegFrameHeader *outHeader, bool *outAtEnd, int *outErr)
{
    if (m_cbrSeeker == NULL || m_xingSeeker == NULL || m_vbriSeeker == NULL) {
        *outErr = -1;
        return 0;
    }

    if (m_xingSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
        *outAtEnd = (fileLimit <= *outOffset + (int)getMpegLayer3FrameSize(outHeader->rawBits));
        *outErr = 0;
        return 1;
    }
    if (*outErr != IO_ERR_EOF) {
        if (m_vbriSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
            *outAtEnd = (fileLimit <= *outOffset + (int)getMpegLayer3FrameSize(outHeader->rawBits));
            *outErr = 0;
            return 1;
        }
        if (*outErr != IO_ERR_EOF
            && m_cbrSeeker->seekToMs(targetMs, fileLimit, outOffset, outHeader, outErr) != 0) {
            *outAtEnd = (fileLimit <= *outOffset + (int)getMpegLayer3FrameSize(outHeader->rawBits));
            *outErr = 0;
            return 1;
        }
    }
    return 0;
}

/* ===== Mp3FrameReader & Mp3NextFrameLocator (0x811FECD0 - 0x811FEEC7) ===== */

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
                                  uint8_t mpegVersion, Mp3FramePacket *outFrame, int *outErr)
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
                                         int *outOffset, MpegFrameHeader *outHeader,
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

    int nextSearchStart = curRecord->m_fileOffset + (int)getMpegLayer3FrameSize(curRecord->m_header.rawBits);
    int foundOffset = -1;
    if (m_scanner->findSyncFrame(nextSearchStart, nextSearchStart + 2881,
                                 &curRecord->m_header.rawBits, 2, fileLimit,
                                 &foundOffset, outHeader, outMatchedDepth, outErr) == 0) {
        return 0;
    }

    *outOffset = foundOffset;
    *outErr = 0;
    return 1;
}

} /* namespace MusicCore */
} /* namespace sce */
