/*
 * SceShell musiccore_plugin - DbMediaList, UriMediaList, MediaListBase,
 * OrderListFactory, MediaEntryPool, MediaItem, and RejectedIdList.
 * Original address range (3.65-CEX): 0x811EEC40 - 0x811F360B
 */

#include <dbutil/connection.h>
#include <dbutil/record.h>
#include <dbutil/rowid_result.h>
#include <dbutil/statement.h>
#include <kernel.h>
#include <rtc.h>
#include <std/new>
#include <std/string>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread/thread.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
SceSize sce_paf_strlen(const char *str);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize len);
int sce_paf_strncasecmp(const char *lhs, const char *rhs, SceSize len);
int sce_paf_snprintf(char *dst, SceSize size, const char *fmt, ...);
int sceKernelDeleteSemaForUser(SceUID semaid);
int sceKernelSignalSemaForUser(SceUID semaid, int signalCount);
}

namespace sce {
namespace MusicCore {

static const int MC_ERR_INTERNAL = -1879113726;         /* 0x8FFF0002 */
static const int MC_ERR_NO_MEMORY = -1879113724;        /* 0x8FFF0004 */
static const int MC_ERR_INVALID_ARG = -1879113723;      /* 0x8FFF0005 */
static const int MC_ERR_NOT_READY = -1879113722;        /* 0x8FFF0006 */
static const int MC_ERR_NOT_FOUND = -1879113720;        /* 0x8FFF0008 */
static const int MC_ERR_UNSUPPORTED = -1879113719;      /* 0x8FFF0009 */
static const int MC_ERR_IO_ERROR = -1879113718;         /* 0x8FFF000A */
static const int MC_ERR_CORRUPT = -1879113717;          /* 0x8FFF000B */
static const int MC_ERR_EMPTY_LIST = -1879113715;       /* 0x8FFF000D */
static const int MC_ERR_END_OF_LIST = -1879113713;      /* 0x8FFF000F */
static const int MC_ERR_START_OF_LIST = -1879113712;    /* 0x8FFF0010 */
static const int DBUTIL_ERR_SQLITE_BUSY = -2146422267;  /* 0x80103205 */

static const int NAV_CURRENT = -1;
static const int NAV_INVALID = -2;
static const int NAV_NEXT = -3;
static const int NAV_PREV = -4;
static const int NAV_FIRST = -5;
static const int NAV_LAST = -6;

static const int REJECT_LIST_CAPACITY = 20;

extern int convertToMusicDevicePath(const char *srcPath, char *dstBuf, SceSize dstCapacity);

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

struct MediaInfoHandle {
    void *vtablePtr;
    uint32_t mediaFourcc;
    uint8_t sharedFlag;
};

extern void decoderManagerReleaseMediaInfo(MediaInfoHandle **ioHandle, int forceRelease);
extern int decoderManagerProbeTrack(const char *path, int codecHint, uint8_t parseMeta,
                                    int ioPriority, MediaInfoHandle **outMediaInfo,
                                    TrackMetadataResult *outMeta, int loopMode);

/* Database connection provider interface owned by DbManager */
class DbConnectionProvider {
public:
    virtual ~DbConnectionProvider() {}
    virtual void initialize() = 0;
    virtual void finalize() = 0;
    virtual int acquireConnection(int writeMode, dbutil::Connection **outConn) = 0;
    virtual void releaseConnection(int writeMode) = 0;
};

/* MusicRowidResult (implemented in musiccore_players.cpp) */
class MusicRowidResult : public dbutil::RowidResult {
public:
    MusicRowidResult();
    virtual ~MusicRowidResult();
    int generateShuffledOrder(uint32_t seed, int64_t anchorRowid,
                              uint16_t *outIndices, uint32_t capacity);
private:
    uint32_t m_rngState;
};

/* ===== RejectedIdList globals (0x8156F1E0, 0x8156F3C0, 0x8156F3C8) ===== */

struct RejectedIdTriplet {
    int64_t musicId;
    int64_t listId;
    int64_t itemId;
};

static int g_nextMediaItemSerial = 0;                         /* 0x8156F1D8 */
static RejectedIdTriplet g_rejectedIds[REJECT_LIST_CAPACITY]; /* 0x8156F1E0 */
static uint8_t g_rejectedIdsInitialized = 0;                  /* 0x8156F3C0 */
static paf::thread::Mutex g_rejectedListLock("SceShellMusicCoreRejListLock", 0); /* 0x8156F3C8 */
static int g_nextMediaListId = 0;                             /* 0x8156F3E8 */
static int g_nextEntryPoolId = 0;                             /* 0x8156F3EC */

/* 0x811F24FE */
int setRejectedIdList(const int64_t *ids, unsigned int count, unsigned int category)
{
    int ret = 0;
    g_rejectedListLock.Lock();

    if (ids == NULL) {
        for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
            g_rejectedIds[idx].musicId = -1;
            g_rejectedIds[idx].listId = -1;
            g_rejectedIds[idx].itemId = -1;
        }
    } else if (category == 0u) {
        for (unsigned int idx = 0; idx < (unsigned int)REJECT_LIST_CAPACITY; ++idx) {
            g_rejectedIds[idx].musicId = (idx < count) ? ids[idx] : -1;
        }
    } else if (category == 1u) {
        for (unsigned int idx = 0; idx < (unsigned int)REJECT_LIST_CAPACITY; ++idx) {
            g_rejectedIds[idx].listId = (idx < count) ? ids[idx] : -1;
        }
    } else if (category == 2u) {
        for (unsigned int idx = 0; idx < (unsigned int)REJECT_LIST_CAPACITY; ++idx) {
            g_rejectedIds[idx].itemId = (idx < count) ? ids[idx] : -1;
        }
    } else {
        ret = MC_ERR_INVALID_ARG;
    }

    g_rejectedListLock.Unlock();
    return ret;
}

/* 0x811F261E */
int isIdRejected(int64_t id, unsigned int category)
{
    if (id == -1) {
        return 0;
    }

    int found = 0;
    g_rejectedListLock.Lock();

    if (category == 0u) {
        for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
            if (g_rejectedIds[idx].musicId == id) {
                found = 1;
                break;
            }
        }
    } else if (category == 1u) {
        for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
            if (g_rejectedIds[idx].listId == id) {
                found = 1;
                break;
            }
        }
    } else if (category == 2u) {
        for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
            if (g_rejectedIds[idx].itemId == id) {
                found = 1;
                break;
            }
        }
    }

    g_rejectedListLock.Unlock();
    return found;
}

/* 0x811F26DE */
void clearRejectedIdList()
{
    g_rejectedListLock.Lock();
    for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
        g_rejectedIds[idx].musicId = -1;
        g_rejectedIds[idx].listId = -1;
        g_rejectedIds[idx].itemId = -1;
    }
    g_rejectedListLock.Unlock();
}

/* ===== Column descriptor tables (0x81513100, 0x81513240) ===== */

enum DbColumnType {
    DB_COL_TEXT = 0,
    DB_COL_INT64 = 1,
    DB_COL_INT32 = 2,
    DB_COL_DATETIME = 3
};

struct DbColumnBinding {
    const char *columnName;
    int valueType;
    uint8_t selected;
    uint8_t padSelected[3];
    void *valuePtr;
    SceSize bufferCapacity;
};

static const DbColumnBinding kMusicTableColumns[15] = {
    { "rowid",              DB_COL_INT64, 0, { 0, 0, 0 }, NULL, 0 },
    { "mrid",               DB_COL_INT64, 0, { 0, 0, 0 }, NULL, 0 },
    { "content_path",       DB_COL_TEXT,  0, { 0, 0, 0 }, NULL, 0 },
    { "codec_type",         DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "container_type",     DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "status",             DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "size",               DB_COL_INT64, 0, { 0, 0, 0 }, NULL, 0 },
    { "channel_type",       DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "bitrate",            DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "sampling_rate",      DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "duration",           DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "play_count",         DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "content_type",       DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "resume_position",    DB_COL_INT32, 0, { 0, 0, 0 }, NULL, 0 },
    { "last_play_datetime", DB_COL_TEXT,  0, { 0, 0, 0 }, NULL, 0 }
};

static const DbColumnBinding kContentItemTableColumns[2] = {
    { "list_id", DB_COL_INT64, 0, { 0, 0, 0 }, NULL, 0 },
    { "item_id", DB_COL_INT64, 0, { 0, 0, 0 }, NULL, 0 }
};

/* ===== MediaEntryNode / DbQueryEntryNode / UriEntryNode / MediaEntryPool ===== */

class MediaEntryNode {
public:
    virtual ~MediaEntryNode() {}
    virtual size_t getAllocatedSize() const = 0;

    int m_entryKind;            /* +4: 1=URI, 2=DB query */
    MediaEntryNode *m_prev;     /* +8 */
    MediaEntryNode *m_next;     /* +12 */
};

class DbQueryEntryNode : public MediaEntryNode {
public:
    DbQueryEntryNode(const char *sqlQuery, int tableCategory, int initialIndex,
                     int64_t anchorId, unsigned int allocSize, int headerOffset,
                     int extraOption, int sortMode);
    virtual ~DbQueryEntryNode() {}
    virtual size_t getAllocatedSize() const { return sizeof(DbQueryEntryNode); }

    char m_sqlQuery[2049];      /* +16..+2064 */
    uint8_t m_padQuery[3];      /* +2065..+2067 */
    int m_tableCategory;        /* +2068: 0=tbl_Music, 2=tbl_AVContentItem */
    int m_initialIndex;         /* +2072 */
    int m_padIndex;             /* +2076 */
    int64_t m_anchorId;         /* +2080 */
    int m_extraOption;          /* +2088 */
    int m_sortMode;             /* +2092 */
};

/* 0x811F1C6A */
DbQueryEntryNode::DbQueryEntryNode(const char *sqlQuery, int tableCategory, int initialIndex,
                                   int64_t anchorId, unsigned int allocSize, int headerOffset,
                                   int extraOption, int sortMode)
{
    m_entryKind = 2;
    m_prev = NULL;
    m_next = NULL;
    m_tableCategory = tableCategory;
    m_initialIndex = initialIndex;
    m_anchorId = anchorId;
    m_extraOption = extraOption;
    m_sortMode = sortMode;

    unsigned int alignedOffset = (((unsigned int)(headerOffset - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedOffset <= allocSize && sqlQuery != NULL) {
        sce_paf_strncpy(m_sqlQuery, sqlQuery, 2048u);
        m_sqlQuery[2048] = '\0';
    }
}

class UriEntryNode : public MediaEntryNode {
public:
    UriEntryNode(const char *uriPath, int loopMode, unsigned int allocSize, int headerOffset);
    virtual ~UriEntryNode() {}
    virtual size_t getAllocatedSize() const { return m_allocSize; }

    int m_codecSubType;         /* +16 */
    int m_status;               /* +20 */
    char *m_uriPath;            /* +24 */
    int m_padUri;               /* +28 */
    int64_t m_trackId;          /* +32 */
    int m_sourceType;           /* +40 */
    int m_codecProfile;         /* +44 */
    int m_channels;             /* +48 */
    int m_bitRate;              /* +52 */
    int m_sampleRate;           /* +56 */
    int m_durationMs;           /* +60 */
    uint8_t m_hasVerifiedStatus;/* +64 */
    uint8_t m_padStatus[3];     /* +65..+67 */
    int m_resumePositionMs;     /* +68 */
    int m_playCount;            /* +72 */
    int m_loopMode;             /* +76 */

private:
    size_t m_allocSize;
};

/* 0x811F2148 */
UriEntryNode::UriEntryNode(const char *uriPath, int loopMode, unsigned int allocSize, int headerOffset)
{
    m_entryKind = 1;
    m_prev = NULL;
    m_next = NULL;
    m_codecSubType = 0;
    m_status = 2;
    m_uriPath = NULL;
    m_trackId = -1;
    m_sourceType = -1;
    m_codecProfile = 0;
    m_channels = 0;
    m_bitRate = 0;
    m_sampleRate = 0;
    m_durationMs = 0;
    m_hasVerifiedStatus = 0;
    m_resumePositionMs = 0;
    m_playCount = 0;
    m_loopMode = loopMode;
    m_allocSize = allocSize;

    unsigned int alignedOffset = (((unsigned int)(headerOffset - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedOffset > allocSize || uriPath == NULL) {
        return;
    }

    SceSize uriLen = sce_paf_strlen(uriPath);
    if (allocSize < (uriLen & 0xFFFFFFF8u) + alignedOffset + 8u) {
        return;
    }

    char *selfBytes = reinterpret_cast<char *>(this);
    char *dst = selfBytes + alignedOffset;
    m_uriPath = dst;
    sce_paf_strncpy(dst, uriPath, uriLen);
    m_uriPath[uriLen] = '\0';
    m_trackId = -2;
}

class MediaEntryPool {
public:
    MediaEntryPool(int minCapacity, unsigned int arenaBytes);
    virtual ~MediaEntryPool();

    void *reserveSlot(int neededBytes);
    void appendNode(MediaEntryNode *node);
    MediaEntryNode *getNodeAt(unsigned int index);
    unsigned int getNodeCount() const { return m_nodeCount; }

private:
    void *m_arenaStart;         /* +4 */
    MediaEntryNode *m_head;     /* +8 */
    MediaEntryNode *m_tail;     /* +12 */
    unsigned int m_freeBytes;   /* +16 */
    unsigned int m_nodeCount;   /* +20 */
    int m_poolSerial;           /* +24 */
};

/* 0x811F3468 */
MediaEntryPool::MediaEntryPool(int minCapacity, unsigned int arenaBytes)
    : m_arenaStart(NULL)
    , m_head(NULL)
    , m_tail(NULL)
    , m_freeBytes(0)
    , m_nodeCount(0)
    , m_poolSerial(g_nextEntryPoolId++)
{
    unsigned int minRequired = (((unsigned int)(minCapacity - 1)) & 0xFFFFFFF8u) + 40u;
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t alignedSelf = ((selfAddr - 1u) & ~(uintptr_t)7u) + 8u;
    if (arenaBytes >= minRequired && alignedSelf == selfAddr) {
        uint8_t *selfBytes = reinterpret_cast<uint8_t *>(this);
        m_arenaStart = selfBytes + 32u;
        m_freeBytes = arenaBytes + 32u;
    }
}

/* 0x811F34BA / 0x811F3504 */
MediaEntryPool::~MediaEntryPool()
{
    MediaEntryNode *node = m_head;
    while (node != NULL) {
        MediaEntryNode *next = node->m_next;
        --m_nodeCount;
        m_freeBytes += (unsigned int)node->getAllocatedSize();
        node->~MediaEntryNode();
        node = next;
    }
    m_arenaStart = NULL;
    m_head = NULL;
    m_tail = NULL;
}

/* 0x811F3548 */
void *MediaEntryPool::reserveSlot(int neededBytes)
{
    if (m_arenaStart == NULL) {
        return NULL;
    }
    unsigned int alignedNeeded = (((unsigned int)(neededBytes - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedNeeded > m_freeBytes) {
        return NULL;
    }
    if (m_head == NULL) {
        return m_arenaStart;
    }
    size_t tailSize = m_tail->getAllocatedSize();
    unsigned int alignedTail = (((unsigned int)(tailSize - 1u)) & 0xFFFFFFF8u) + 8u;
    uint8_t *tailBytes = reinterpret_cast<uint8_t *>(m_tail);
    return tailBytes + alignedTail;
}

/* 0x811F3582 */
void MediaEntryPool::appendNode(MediaEntryNode *node)
{
    if (node == NULL || m_arenaStart == NULL) {
        return;
    }
    if (m_head != NULL) {
        node->m_next = m_tail;
        m_tail->m_prev = node;
    } else {
        m_head = node;
    }
    m_tail = node;
    ++m_nodeCount;
    m_freeBytes -= (unsigned int)node->getAllocatedSize();
}

/* 0x811F35B4 */
MediaEntryNode *MediaEntryPool::getNodeAt(unsigned int index)
{
    if (m_head == NULL || index >= m_nodeCount) {
        return NULL;
    }
    MediaEntryNode *cursor = m_head;
    for (unsigned int idx = 0; idx < index && cursor != NULL; ++idx) {
        cursor = cursor->m_next;
    }
    return cursor;
}

/* ===== Playback range resolver (0x811F2C18) ===== */

static const uint32_t RANGE_DEFAULT = 0xFFFFFFFFu;
static const uint32_t RANGE_KEEP = 0xFFFFFFFEu;
static const uint32_t RANGE_END = 0xFFFFFFFDu;
static const uint32_t RANGE_START = 0xFFFFFFFCu;

/* 0x811F2C18 */
unsigned int resolvePlaybackRange(unsigned int reqStart, unsigned int reqEnd,
                                  unsigned int reqPos, unsigned int durationMs,
                                  int playSpeed, unsigned int *ioStart,
                                  unsigned int *ioEnd, unsigned int *ioPos)
{
    switch (reqStart) {
    case RANGE_KEEP:
        if (*ioStart == RANGE_DEFAULT || *ioStart == RANGE_KEEP || *ioStart == RANGE_START) {
            *ioStart = 0u;
        } else if (*ioStart == RANGE_END) {
            *ioStart = durationMs;
        }
        break;
    case RANGE_DEFAULT:
        *ioStart = RANGE_DEFAULT;
        break;
    case RANGE_START:
        *ioStart = 0u;
        break;
    case RANGE_END:
        *ioStart = durationMs;
        break;
    default:
        *ioStart = (reqStart > durationMs) ? durationMs : reqStart;
        break;
    }

    unsigned int endVal = reqEnd;
    switch (reqEnd) {
    case RANGE_KEEP:
        endVal = *ioEnd;
        if (endVal == RANGE_START) {
            endVal = 0u;
            *ioEnd = 0u;
        } else if (endVal == RANGE_DEFAULT || endVal == RANGE_KEEP || endVal == RANGE_END) {
            endVal = durationMs;
            *ioEnd = durationMs;
        }
        break;
    case RANGE_DEFAULT:
        endVal = RANGE_DEFAULT;
        *ioEnd = RANGE_DEFAULT;
        break;
    case RANGE_START:
        endVal = 0u;
        *ioEnd = 0u;
        break;
    case RANGE_END:
        endVal = durationMs;
        *ioEnd = durationMs;
        break;
    default:
        if (reqEnd > durationMs) {
            endVal = durationMs;
        }
        *ioEnd = endVal;
        break;
    }

    if (*ioStart != RANGE_DEFAULT && endVal != RANGE_DEFAULT && *ioStart > endVal) {
        *ioEnd = *ioStart;
        endVal = *ioStart;
    }

    unsigned int posVal = 0u;
    if (reqPos == RANGE_KEEP) {
        posVal = *ioPos;
        if (posVal == RANGE_START) {
            *ioPos = 0u;
            posVal = 0u;
        } else if (posVal >= RANGE_KEEP) {
            posVal = (playSpeed >= 0) ? *ioStart : endVal;
            *ioPos = posVal;
        } else if (posVal == RANGE_END) {
            posVal = durationMs;
            *ioPos = durationMs;
        }
    } else {
        switch (reqPos) {
        case RANGE_DEFAULT:
            if (playSpeed >= 0) {
                posVal = (*ioStart == RANGE_DEFAULT) ? 0u : *ioStart;
            } else {
                posVal = (endVal == RANGE_DEFAULT) ? durationMs : endVal;
            }
            *ioPos = posVal;
            break;
        case RANGE_START:
            posVal = 0u;
            *ioPos = 0u;
            break;
        case RANGE_END:
            posVal = durationMs;
            *ioPos = durationMs;
            break;
        default:
            posVal = reqPos;
            *ioPos = reqPos;
            break;
        }
    }

    if (posVal == RANGE_DEFAULT) {
        return posVal;
    }

    if (playSpeed >= 0) {
        if (*ioStart != RANGE_DEFAULT && posVal < *ioStart) {
            posVal = *ioStart;
            *ioPos = posVal;
        }
    } else {
        if (*ioEnd != RANGE_DEFAULT && posVal > *ioEnd) {
            posVal = *ioEnd;
            *ioPos = posVal;
        }
    }

    if (reqPos != RANGE_KEEP) {
        posVal += 100u;
        if (posVal > durationMs) {
            posVal = (durationMs <= 100u) ? 0u : (durationMs - 100u);
            *ioPos = posVal;
        }
    }
    return posVal;
}

/* ===== OrderListFactory (0x811F29E2 - 0x811F2C17) ===== */

struct OrderListSlotHeader {
    uint8_t inUse;
    uint8_t padSlot[7];
    int ownerListId;
    int poolPadding;
};

class OrderListFactory {
public:
    static unsigned int calcTotalSize(int maxItems, int slotCount, int elemBytes, int headerSize);

    OrderListFactory(int maxItems, int slotCount, int elemBytes,
                     unsigned int arenaSize, int headerSize);
    virtual ~OrderListFactory();

    void *acquireOrderList(void *ownerList, int *outPrevOwnerId, SceUInt *timeout);
    void releaseOrderList(void *bufferPtr, uint8_t clearOwner);

private:
    uint8_t *m_slotsBase;          /* +4 */
    int m_maxItems;                /* +8 */
    int m_slotCount;               /* +12 */
    int m_elemBytes;               /* +16 */
    SceUID m_slotSema;             /* +20 */
    paf::thread::Mutex m_lock;     /* +24 */
};

/* 0x811F29E2 */
unsigned int OrderListFactory::calcTotalSize(int maxItems, int slotCount,
                                             int elemBytes, int headerSize)
{
    unsigned int alignedHeader = (((unsigned int)(headerSize - 1)) & 0xFFFFFFF8u) + 8u;
    unsigned int payloadBytes = ((unsigned int)(elemBytes * maxItems + 15)) & 0xFFFFFFF8u;
    return alignedHeader + (unsigned int)slotCount * (payloadBytes + 8u);
}

/* 0x811F29FE */
OrderListFactory::OrderListFactory(int maxItems, int slotCount, int elemBytes,
                                   unsigned int arenaSize, int headerSize)
    : m_slotsBase(NULL)
    , m_maxItems(maxItems)
    , m_slotCount(slotCount)
    , m_elemBytes(elemBytes)
    , m_slotSema(-1)
    , m_lock("SceShellMusicCoreOrderListFactory", 0)
{
    SceUID sema = sceKernelCreateSema("SceShellMusicCoreOrderListFactory", 0x2000u,
                                      slotCount, slotCount, NULL);
    if (sema <= 0) {
        return;
    }
    m_slotSema = sema;

    if (arenaSize < calcTotalSize(m_maxItems, m_slotCount, m_elemBytes, 56)) {
        return;
    }
    unsigned int alignedHeader = (((unsigned int)(headerSize - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedHeader > arenaSize) {
        return;
    }
    unsigned int payloadBytes = ((unsigned int)(m_elemBytes * m_maxItems + 15)) & 0xFFFFFFF8u;
    unsigned int stride = payloadBytes + 8u;
    unsigned int totalSlotsBytes = (((unsigned int)(stride * m_slotCount - 1u)) & 0xFFFFFFF8u) + 8u;
    if (alignedHeader + totalSlotsBytes > arenaSize) {
        return;
    }

    uint8_t *selfBytes = reinterpret_cast<uint8_t *>(this);
    m_slotsBase = selfBytes + alignedHeader;
    uint8_t *cursor = m_slotsBase;
    for (int idx = 0; idx < m_slotCount; ++idx) {
        OrderListSlotHeader *hdr = (OrderListSlotHeader *)cursor;
        hdr->inUse = 0;
        hdr->ownerListId = -1;
        cursor += stride;
    }
}

/* 0x811F2ABE / 0x811F2AEC */
OrderListFactory::~OrderListFactory()
{
    if (m_slotSema != -1) {
        sceKernelDeleteSemaForUser(m_slotSema);
        m_slotSema = -1;
    }
}

class MediaListBase;

struct MetadataOutputRequest {
    char *albumOut;
    SceSize albumCapacity;
    char *titleOut;
    SceSize titleCapacity;
    char *artistOut;
    SceSize artistCapacity;
    paf::thread::Mutex *lock;
};

/* ===== MediaItem (0x811F2DFA - 0x811F3467, size 2152 bytes) ===== */

class MediaItem {
public:
    MediaItem();
    virtual ~MediaItem() {}

    void reset();
    void copyFrom(const MediaItem &other);
    int probe(int ioPriority, uint8_t parseMeta);
    int acquireFromList(int navTarget, MediaListBase *listObj);
    int releaseToList(uint8_t commitChanges);
    int transferTo(MediaItem *dst, uint8_t commitDst);
    int commitToList();
    int readMetadata(MetadataOutputRequest *outMeta);

    int m_codecSubType;                 /* +4 */
    int m_status;                       /* +8 */
    int m_padStatus;                    /* +12 */
    int64_t m_rowId;                    /* +16 */
    int64_t m_musicId;                  /* +24 */
    int64_t m_listId;                   /* +32 */
    char m_contentPath[2048];           /* +40..+2087 */
    int m_sourceType;                   /* +2088 */
    int m_codecProfile;                 /* +2092 */
    int m_channels;                     /* +2096 */
    int m_bitRate;                      /* +2100 */
    int m_sampleRate;                   /* +2104 */
    int m_durationMs;                   /* +2108 */
    int64_t m_fileSize;                 /* +2112 */
    int m_resumePositionMs;             /* +2120 */
    int m_playCount;                    /* +2124 */
    int m_loopMode;                     /* +2128 */
    int m_slotSerial;                   /* +2132 */
    MediaListBase *m_ownerList;         /* +2136 */
    MediaInfoHandle *m_mediaInfo;       /* +2140 */
    uint8_t m_mediaInfoShared;          /* +2144 */
    uint8_t m_padTail[7];               /* +2145..+2151 */
};

MediaItem::MediaItem()
{
    reset();
}

void MediaItem::reset()
{
    m_codecSubType = 0;
    m_status = 2;
    m_rowId = -1;
    m_musicId = -1;
    m_listId = -1;
    sce_paf_memset(m_contentPath, 0, sizeof(m_contentPath));
    m_sourceType = -1;
    m_codecProfile = 0;
    m_channels = 0;
    m_bitRate = 0;
    m_sampleRate = 0;
    m_durationMs = 0;
    m_fileSize = 0;
    m_resumePositionMs = 0;
    m_playCount = 0;
    m_loopMode = 0;
    m_slotSerial = -1;
    m_ownerList = NULL;
    m_mediaInfo = NULL;
    m_mediaInfoShared = 0;
}

void MediaItem::copyFrom(const MediaItem &other)
{
    m_codecSubType = other.m_codecSubType;
    m_status = other.m_status;
    m_rowId = other.m_rowId;
    m_musicId = other.m_musicId;
    m_listId = other.m_listId;
    sce_paf_memcpy(m_contentPath, other.m_contentPath, sizeof(m_contentPath));
    m_sourceType = other.m_sourceType;
    m_codecProfile = other.m_codecProfile;
    m_channels = other.m_channels;
    m_bitRate = other.m_bitRate;
    m_sampleRate = other.m_sampleRate;
    m_durationMs = other.m_durationMs;
    m_fileSize = other.m_fileSize;
    m_resumePositionMs = other.m_resumePositionMs;
    m_playCount = other.m_playCount;
    m_loopMode = other.m_loopMode;
    m_slotSerial = other.m_slotSerial;
    m_ownerList = other.m_ownerList;
    m_mediaInfo = other.m_mediaInfo;
    m_mediaInfoShared = other.m_mediaInfoShared;
}

static int mapCodecSubTypeToProbeHint(int codecSubType)
{
    switch (codecSubType) {
    case 10:
        return 0; /* WAV */
    case 7:
        return 1; /* MP3 */
    case 4:
        return 4; /* MP4 / AAC */
    case 13:
        return 6; /* ATRAC9 */
    default:
        return 7; /* Probe all */
    }
}

static bool isSupportedCodecSubType(int codecSubType)
{
    return codecSubType == 4 || codecSubType == 7 || codecSubType == 10 || codecSubType == 13;
}

/* 0x811F2DFA */
int MediaItem::probe(int ioPriority, uint8_t parseMeta)
{
    if (m_mediaInfo != NULL) {
        if (parseMeta != 1u || m_mediaInfo->sharedFlag != 0u) {
            return 0;
        }
        decoderManagerReleaseMediaInfo(&m_mediaInfo, 1);
        if (m_mediaInfo == NULL) {
            m_mediaInfoShared = 0;
        }
    }

    MediaInfoHandle *probedInfo = NULL;
    TrackMetadataResult meta;
    meta.hasChannels = 0;
    meta.hasBitRate = 0;
    meta.hasDuration = 0;
    meta.hasSampleRate = 0;
    meta.hasCodecProfile = 0;
    meta.hasCodecSubType = 0;

    int probeHint = mapCodecSubTypeToProbeHint(m_codecSubType);
    int ret = decoderManagerProbeTrack(m_contentPath, probeHint, parseMeta, ioPriority,
                                       &probedInfo, &meta, m_loopMode);

    if (parseMeta == 1u) {
        if (meta.hasChannels == 1u && m_channels != meta.channels) {
            m_channels = meta.channels;
        }
        if (meta.hasBitRate == 1u && m_bitRate != meta.bitRate) {
            m_bitRate = meta.bitRate;
        }
        if (meta.hasDuration == 1u && m_durationMs != (int)meta.durationMs) {
            m_durationMs = (int)meta.durationMs;
        }
        if (meta.hasSampleRate == 1u && m_sampleRate != meta.sampleRate) {
            m_sampleRate = meta.sampleRate;
        }
        if (meta.hasCodecProfile == 1u && m_codecProfile != meta.codecProfile) {
            m_codecProfile = meta.codecProfile;
        }
        if (meta.hasCodecSubType == 1u && m_codecSubType != meta.codecSubType) {
            m_codecSubType = meta.codecSubType;
        }
    }

    if (ret == 0) {
        if (!isSupportedCodecSubType(m_codecSubType)) {
            ret = MC_ERR_UNSUPPORTED;
        }
        if (probedInfo == NULL) {
            ret = MC_ERR_INTERNAL;
        } else if (ret == 0) {
            if (m_mediaInfo == NULL) {
                m_mediaInfo = probedInfo;
                m_mediaInfoShared = probedInfo->sharedFlag;
            }
            if (parseMeta == 1u) {
                m_status = 2;
            }
            return 0;
        }
    }

    if (probedInfo != NULL && probedInfo->sharedFlag == 0u) {
        decoderManagerReleaseMediaInfo(&probedInfo, 1);
    }

    if (ret == MC_ERR_IO_ERROR || ret == MC_ERR_NOT_FOUND || ret == MC_ERR_CORRUPT) {
        m_status = 1;
    } else if (ret == MC_ERR_UNSUPPORTED && m_status == 2) {
        m_status = 7;
    }
    return ret;
}

static int classifyContentSourceType(const char *path)
{
    if (sce_paf_strncmp(path, "http:", 5u) == 0) {
        return 2;
    }
    return 16;
}

/* ===== MediaListBase (0x811F21EC - 0x811F29E1) ===== */

class MediaListBase {
public:
    MediaListBase(MediaEntryPool *entryPool, OrderListFactory *orderFactory);
    virtual ~MediaListBase();

    virtual int releaseOrderList(uint8_t clearOwner);
    virtual void onOrderListReserved(int *outPrevId, int timeoutParam);
    virtual int getListId() const { return m_listId; }
    virtual int checkValid();
    virtual int checkPlayableAt(int index, int *outStatus) = 0;
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId) = 0;
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem) = 0;
    virtual int rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                 uint8_t *outChanged, uint8_t forceRebuild);
    virtual int refreshOrderBuffer(int shuffleMode, int64_t anchorRowId);
    virtual int fetchItemMetadata(int64_t musicId, const char *path, MetadataOutputRequest *outMeta);
    virtual int checkChangedFlag(uint8_t *outChanged, uint8_t queryLive);

    int resolveNavigationIndex(int navTarget, int currentIdx, uint8_t *outValid) const;
    int navigateTo(int navTarget, uint8_t *outValid);
    int resolveErrorNavigation(int unused, int repeatMode, int direction,
                               unsigned int *ioAttempts, int *ioErr,
                               uint8_t *outWrappedAll, uint8_t *outWrappedOne);
    void releaseItemSlot(const MediaItem *item);
    MediaItem *allocateItemSlot();
    MediaItem *findItemSlot(int slotSerial);

    MediaEntryPool *m_entryPool;        /* +4 */
    void *m_auxVtable;                  /* +8 */
    unsigned int m_itemCount;           /* +12 */
    int m_currentIndex;                 /* +16 */
    int m_padIndex;                     /* +20 */
    int64_t m_anchorId;                 /* +24 */
    int m_initialIndex;                 /* +32 */
    int m_shuffleMode;                  /* +36 */
    int m_sortMode;                     /* +40 */
    int m_tableCategory;                /* +44 */
    MediaItem m_itemSlots[4];           /* +48..+8655 */
    uint8_t m_isReady;                  /* +8656 */
    uint8_t m_padReady[7];              /* +8657..+8663 */
    paf::thread::Mutex m_listLock;      /* +8664 */
    uint16_t *m_orderBuffer;            /* +8696 */
    unsigned int m_orderCapacity;       /* +8700 */
    OrderListFactory *m_orderFactory;   /* +8704 */
    int m_listId;                       /* +8708 */
};

/* 0x811F27A2 / 0x811F293C */
MediaListBase::MediaListBase(MediaEntryPool *entryPool, OrderListFactory *orderFactory)
    : m_entryPool(entryPool)
    , m_auxVtable(NULL)
    , m_itemCount(0)
    , m_currentIndex(NAV_INVALID)
    , m_padIndex(0)
    , m_anchorId(-1)
    , m_initialIndex(NAV_INVALID)
    , m_shuffleMode(0)
    , m_sortMode(-1)
    , m_tableCategory(-1)
    , m_isReady(0)
    , m_listLock("SceShellMusicCoreDbListLock", 0)
    , m_orderBuffer(NULL)
    , m_orderCapacity(0)
    , m_orderFactory(orderFactory)
    , m_listId(g_nextMediaListId)
{
    g_nextMediaListId = (g_nextMediaListId == 0x7FFFFFFF) ? 1 : (g_nextMediaListId + 1);

    if (g_rejectedIdsInitialized == 0u) {
        g_rejectedIdsInitialized = 1u;
        for (int idx = 0; idx < REJECT_LIST_CAPACITY; ++idx) {
            g_rejectedIds[idx].musicId = -1;
            g_rejectedIds[idx].listId = -1;
            g_rejectedIds[idx].itemId = -1;
        }
    }
}

MediaListBase::~MediaListBase()
{
}

int MediaListBase::releaseOrderList(uint8_t clearOwner)
{
    if (m_orderFactory != NULL && m_orderBuffer != NULL) {
        m_orderFactory->releaseOrderList(m_orderBuffer, clearOwner);
        m_orderBuffer = NULL;
        m_orderCapacity = 0;
    }
    return 0;
}

void MediaListBase::onOrderListReserved(int *outPrevId, int timeoutParam)
{
    (void)timeoutParam;
    if (outPrevId != NULL) {
        *outPrevId = -1;
    }
}

/* 0x811F2732 */
int MediaListBase::checkValid()
{
    int ret = 0;
    m_listLock.Lock();
    if (m_isReady == 0u) {
        ret = MC_ERR_NOT_READY;
    }
    m_listLock.Unlock();
    return ret;
}

int MediaListBase::rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                    uint8_t *outChanged, uint8_t forceRebuild)
{
    (void)shuffleMode;
    (void)anchorRowId;
    (void)outChanged;
    (void)forceRebuild;
    return 0;
}

int MediaListBase::refreshOrderBuffer(int shuffleMode, int64_t anchorRowId)
{
    (void)shuffleMode;
    (void)anchorRowId;
    return 0;
}

/* 0x811F275C */
int MediaListBase::fetchItemMetadata(int64_t musicId, const char *path,
                                     MetadataOutputRequest *outMeta)
{
    (void)musicId;
    (void)path;
    if (outMeta != NULL) {
        if (outMeta->lock != NULL) {
            outMeta->lock->Lock();
        }
        if (outMeta->albumOut != NULL && outMeta->albumCapacity != 0u) {
            outMeta->albumOut[0] = '\0';
        }
        if (outMeta->titleOut != NULL && outMeta->titleCapacity != 0u) {
            outMeta->titleOut[0] = '\0';
        }
        if (outMeta->artistOut != NULL && outMeta->artistCapacity != 0u) {
            outMeta->artistOut[0] = '\0';
        }
        if (outMeta->lock != NULL) {
            outMeta->lock->Unlock();
        }
    }
    return 0;
}

/* 0x811F272C */
int MediaListBase::checkChangedFlag(uint8_t *outChanged, uint8_t queryLive)
{
    (void)queryLive;
    *outChanged = 0;
    return 0;
}

/* 0x811F21EC */
int MediaListBase::resolveNavigationIndex(int navTarget, int currentIdx, uint8_t *outValid) const
{
    *outValid = 0;
    int resolved = NAV_INVALID;

    switch (navTarget) {
    case NAV_LAST:
        if (m_itemCount != 0u) {
            *outValid = 1;
            resolved = (int)(m_itemCount - 1u);
        }
        break;
    case NAV_FIRST:
        *outValid = 1;
        resolved = 0;
        break;
    case NAV_PREV:
        if (currentIdx != NAV_INVALID && currentIdx != NAV_CURRENT
            && currentIdx != NAV_PREV && currentIdx != NAV_NEXT && currentIdx != NAV_FIRST) {
            if (currentIdx == NAV_LAST) {
                if (m_itemCount > 1u) {
                    *outValid = 1;
                    resolved = (int)(m_itemCount - 2u);
                }
            } else if (currentIdx != 0) {
                *outValid = 1;
                resolved = currentIdx - 1;
            }
        }
        break;
    case NAV_NEXT:
        if (currentIdx != NAV_INVALID && currentIdx != NAV_CURRENT
            && currentIdx != NAV_PREV && currentIdx != NAV_NEXT) {
            if (currentIdx == NAV_FIRST) {
                if (m_itemCount > 1u) {
                    *outValid = 1;
                    resolved = 1;
                }
            } else if (currentIdx != NAV_LAST && (unsigned int)(currentIdx + 1) < m_itemCount) {
                *outValid = 1;
                resolved = currentIdx + 1;
            }
        }
        break;
    case NAV_INVALID:
        if (currentIdx == NAV_FIRST) {
            resolved = 0;
        } else if (currentIdx == NAV_LAST) {
            resolved = NAV_LAST;
        }
        break;
    case NAV_CURRENT:
        if (currentIdx == NAV_INVALID || currentIdx == NAV_FIRST) {
            *outValid = 1;
            resolved = 0;
        } else if (currentIdx == NAV_LAST) {
            if (m_itemCount != 0u) {
                *outValid = 1;
                resolved = (int)(m_itemCount - 1u);
            }
        } else if (currentIdx != NAV_CURRENT && currentIdx != NAV_PREV && currentIdx != NAV_NEXT) {
            *outValid = 1;
            resolved = currentIdx;
        }
        break;
    default:
        *outValid = 1;
        resolved = navTarget;
        break;
    }
    return resolved;
}

/* 0x811F2328 */
int MediaListBase::navigateTo(int navTarget, uint8_t *outValid)
{
    m_listLock.Lock();
    if (m_isReady == 0u) {
        m_listLock.Unlock();
        *outValid = 0;
        return MC_ERR_NOT_READY;
    }

    int targetIdx = resolveNavigationIndex(navTarget, m_currentIndex, outValid);
    int ret = 0;
    if (targetIdx == NAV_INVALID) {
        if (navTarget == NAV_NEXT) {
            ret = MC_ERR_END_OF_LIST;
        } else if (navTarget == NAV_PREV) {
            ret = MC_ERR_START_OF_LIST;
        } else {
            ret = MC_ERR_NOT_FOUND;
        }
    } else if ((unsigned int)targetIdx >= m_itemCount) {
        m_listLock.Unlock();
        return MC_ERR_INTERNAL;
    } else {
        m_currentIndex = targetIdx;
        int statusVal = 2;
        ret = checkPlayableAt(targetIdx, &statusVal);
    }

    m_listLock.Unlock();
    return ret;
}

/* 0x811F241A */
int MediaListBase::resolveErrorNavigation(int unused, int repeatMode, int direction,
                                          unsigned int *ioAttempts, int *ioErr,
                                          uint8_t *outWrappedAll, uint8_t *outWrappedOne)
{
    (void)unused;
    int err = *ioErr;
    int nextNav = NAV_INVALID;

    if (err == 0) {
        ++(*ioAttempts);
        return nextNav;
    }

    switch (err) {
    case MC_ERR_NOT_FOUND:
    case MC_ERR_UNSUPPORTED:
    case MC_ERR_IO_ERROR:
    case MC_ERR_CORRUPT:
        if (repeatMode != 0) {
            unsigned int attempts = *ioAttempts + 1u;
            *ioAttempts = attempts;
            if (attempts == m_itemCount) {
                *ioErr = MC_ERR_EMPTY_LIST;
            } else if (attempts < m_itemCount) {
                nextNav = (direction >= 0) ? NAV_NEXT : NAV_PREV;
                *ioErr = 0;
            }
        }
        break;

    case MC_ERR_END_OF_LIST:
    case MC_ERR_START_OF_LIST:
        if (repeatMode != 0) {
            if (repeatMode == 1) {
                *outWrappedAll = 1;
            }
            if (repeatMode == 2) {
                *outWrappedOne = 1;
            }
            if (*ioAttempts == m_itemCount) {
                *ioErr = MC_ERR_EMPTY_LIST;
            } else if (*ioAttempts >= m_itemCount) {
                *ioErr = MC_ERR_INTERNAL;
            } else {
                nextNav = (direction >= 0) ? NAV_FIRST : NAV_LAST;
                *ioErr = 0;
            }
        }
        break;

    default:
        break;
    }
    return nextNav;
}

/* 0x811F294C */
void MediaListBase::releaseItemSlot(const MediaItem *item)
{
    for (int idx = 0; idx < 4; ++idx) {
        if (m_itemSlots[idx].m_slotSerial == item->m_slotSerial) {
            m_itemSlots[idx].reset();
            return;
        }
    }
}

MediaItem *MediaListBase::allocateItemSlot()
{
    for (int idx = 0; idx < 4; ++idx) {
        if (m_itemSlots[idx].m_slotSerial == -1) {
            int serial = g_nextMediaItemSerial;
            m_itemSlots[idx].m_slotSerial = serial;
            g_nextMediaItemSerial = (serial == 0x7FFFFFFF) ? 0 : (serial + 1);
            return &m_itemSlots[idx];
        }
    }
    return NULL;
}

MediaItem *MediaListBase::findItemSlot(int slotSerial)
{
    for (int idx = 0; idx < 4; ++idx) {
        if (m_itemSlots[idx].m_slotSerial == slotSerial) {
            return &m_itemSlots[idx];
        }
    }
    return NULL;
}

/* 0x811F2B24 */
void *OrderListFactory::acquireOrderList(void *ownerList, int *outPrevOwnerId, SceUInt *timeout)
{
    if (sceKernelWaitSema(m_slotSema, 1, timeout) != 0) {
        return NULL;
    }

    m_lock.Lock();
    int targetOwnerId = 0;
    if (ownerList != NULL) {
        targetOwnerId = ((MediaListBase *)ownerList)->getListId();
    }

    unsigned int payloadBytes = ((unsigned int)(m_elemBytes * m_maxItems + 15)) & 0xFFFFFFF8u;
    unsigned int stride = payloadBytes + 8u;

    OrderListSlotHeader *chosenHdr = NULL;
    uint8_t *cursor = m_slotsBase;
    for (int idx = 0; idx < m_slotCount; ++idx) {
        OrderListSlotHeader *hdr = (OrderListSlotHeader *)cursor;
        if (hdr->inUse == 0u) {
            chosenHdr = hdr;
            if (hdr->ownerListId == targetOwnerId) {
                break;
            }
        }
        cursor += stride;
    }

    void *resultBuf = NULL;
    if (chosenHdr != NULL) {
        *outPrevOwnerId = chosenHdr->ownerListId;
        chosenHdr->ownerListId = targetOwnerId;
        chosenHdr->inUse = 1u;
        uint8_t *chosenBytes = reinterpret_cast<uint8_t *>(chosenHdr);
        resultBuf = chosenBytes + sizeof(*chosenHdr);
    }

    m_lock.Unlock();
    return resultBuf;
}

/* 0x811F2BC4 */
void OrderListFactory::releaseOrderList(void *bufferPtr, uint8_t clearOwner)
{
    if (bufferPtr == NULL) {
        return;
    }

    OrderListSlotHeader *hdr = (OrderListSlotHeader *)((uint8_t *)bufferPtr - 16);
    bool wasInUse = false;

    m_lock.Lock();
    if (hdr->inUse == 1u) {
        hdr->inUse = 0u;
        wasInUse = true;
    }
    if (clearOwner == 1u) {
        hdr->ownerListId = -1;
    }
    m_lock.Unlock();

    if (wasInUse) {
        sceKernelSignalSemaForUser(m_slotSema, 1);
    }
}

/* 0x811F3018 */
int MediaItem::acquireFromList(int navTarget, MediaListBase *listObj)
{
    if (listObj == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    MediaItem *slot = listObj->allocateItemSlot();
    if (slot == NULL) {
        return MC_ERR_NO_MEMORY;
    }

    int ret = listObj->loadItemAt(navTarget, slot, 0, -1);
    if (ret != 0) {
        listObj->releaseItemSlot(slot);
        return ret;
    }

    copyFrom(*slot);
    return 0;
}

/* 0x811F3170 */
int MediaItem::releaseToList(uint8_t commitChanges)
{
    if (m_ownerList == NULL) {
        return MC_ERR_INTERNAL;
    }

    MediaItem *slot = m_ownerList->findItemSlot(m_slotSerial);
    if (slot == NULL) {
        return MC_ERR_INTERNAL;
    }

    if (commitChanges == 1u) {
        m_ownerList->saveItemChanges(slot, this);
    }

    if (m_mediaInfo != NULL && m_mediaInfoShared == 0u) {
        decoderManagerReleaseMediaInfo(&m_mediaInfo, 1);
    }

    m_ownerList->releaseItemSlot(this);
    reset();
    return 0;
}

/* 0x811F3258 */
int MediaItem::transferTo(MediaItem *dst, uint8_t commitDst)
{
    if (m_ownerList == NULL) {
        return MC_ERR_INTERNAL;
    }
    if (m_ownerList->findItemSlot(m_slotSerial) == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    if (dst->m_ownerList != NULL) {
        dst->releaseToList(commitDst);
    }

    dst->copyFrom(*this);
    reset();
    return 0;
}

/* 0x811F33EC */
int MediaItem::commitToList()
{
    if (m_ownerList == NULL) {
        return MC_ERR_INTERNAL;
    }
    MediaItem *slot = m_ownerList->findItemSlot(m_slotSerial);
    if (slot == NULL) {
        return MC_ERR_INTERNAL;
    }

    int ret = m_ownerList->saveItemChanges(slot, this);
    return (ret != 0) ? MC_ERR_INTERNAL : 0;
}

/* 0x811F343A */
int MediaItem::readMetadata(MetadataOutputRequest *outMeta)
{
    if (m_ownerList == NULL) {
        return MC_ERR_NOT_READY;
    }
    return m_ownerList->fetchItemMetadata(m_musicId, m_contentPath, outMeta);
}

/* ===== UriMediaList (0x811F1CE0 - 0x811F2147) ===== */

class UriMediaList : public MediaListBase {
public:
    UriMediaList(MediaEntryPool *entryPool, int shuffleMode, OrderListFactory *orderFactory);
    virtual ~UriMediaList() {}

    virtual int checkPlayableAt(int index, int *outStatus);
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId);
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem);

    int resolveEntryTrackId(int index, int64_t *ioTrackId);
    void setEntryTrackId(int index, int64_t trackId);
};

/* 0x811F1CE0 */
UriMediaList::UriMediaList(MediaEntryPool *entryPool, int shuffleMode, OrderListFactory *orderFactory)
    : MediaListBase(entryPool, orderFactory)
{
    if (m_entryPool == NULL) {
        return;
    }

    MediaEntryNode *node = m_entryPool->getNodeAt(0u);
    while (node != NULL) {
        if (node->m_entryKind != 1) {
            return;
        }
        node = node->m_next;
    }

    m_itemCount = m_entryPool->getNodeCount();
    m_shuffleMode = shuffleMode;

    uint8_t navValid = 0;
    m_currentIndex = resolveNavigationIndex(NAV_FIRST, m_currentIndex, &navValid);

    UriEntryNode *uriNode = (UriEntryNode *)m_entryPool->getNodeAt(0u);
    for (unsigned int idx = 0; idx < m_itemCount && uriNode != NULL; ++idx) {
        uriNode->m_status = 2;
        uriNode->m_hasVerifiedStatus = 0;
        uriNode->m_resumePositionMs = 0;
        uriNode->m_playCount = 0;
        uriNode = (UriEntryNode *)uriNode->m_next;
    }

    m_listLock.Lock();
    m_isReady = 1;
    m_listLock.Unlock();
}

/* 0x811F1DAC */
int UriMediaList::checkPlayableAt(int index, int *outStatus)
{
    if (outStatus == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    *outStatus = 2;

    UriEntryNode *entry = (UriEntryNode *)m_entryPool->getNodeAt((unsigned int)index);
    if (entry == NULL) {
        return MC_ERR_NOT_FOUND;
    }
    if (isIdRejected(entry->m_trackId, 0u) == 1) {
        return MC_ERR_NOT_FOUND;
    }

    *outStatus = (entry->m_hasVerifiedStatus == 1u) ? entry->m_status : 2;
    return 0;
}

/* 0x811F1DF8 */
int UriMediaList::resolveEntryTrackId(int index, int64_t *ioTrackId)
{
    if (m_entryPool == NULL) {
        return 0;
    }
    UriEntryNode *entry = (UriEntryNode *)m_entryPool->getNodeAt((unsigned int)index);
    if (entry == NULL) {
        return 0;
    }
    if ((uint64_t)*ioTrackId >= 0xFFFFFFFFFFFFFFFEULL && entry->m_trackId != -1) {
        *ioTrackId = entry->m_trackId;
    }
    return -1;
}

/* 0x811F1E48 */
void UriMediaList::setEntryTrackId(int index, int64_t trackId)
{
    if (m_entryPool == NULL) {
        return;
    }
    UriEntryNode *entry = (UriEntryNode *)m_entryPool->getNodeAt((unsigned int)index);
    if (entry != NULL) {
        entry->m_trackId = trackId;
    }
}

/* 0x811F1E60 */
int UriMediaList::loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId)
{
    (void)unused;
    (void)expectedRowId;
    if (m_itemCount == 0u) {
        return MC_ERR_EMPTY_LIST;
    }

    uint8_t navValid = 0;
    int resolvedIdx = resolveNavigationIndex(navTarget, m_currentIndex, &navValid);
    if (resolvedIdx == NAV_INVALID) {
        if (navTarget == NAV_NEXT) {
            return MC_ERR_END_OF_LIST;
        }
        if (navTarget == NAV_PREV) {
            return MC_ERR_START_OF_LIST;
        }
        return MC_ERR_NOT_FOUND;
    }
    if ((unsigned int)resolvedIdx >= m_itemCount) {
        return MC_ERR_INTERNAL;
    }

    UriEntryNode *entry = (UriEntryNode *)m_entryPool->getNodeAt((unsigned int)resolvedIdx);
    if (entry == NULL) {
        return MC_ERR_NOT_FOUND;
    }
    if (isIdRejected(entry->m_trackId, 0u) == 1) {
        return MC_ERR_NOT_FOUND;
    }

    outItem->m_codecSubType = entry->m_codecSubType;
    outItem->m_status = entry->m_status;
    sce_paf_strncpy(outItem->m_contentPath, entry->m_uriPath, 2047u);
    outItem->m_contentPath[2047] = '\0';
    outItem->m_musicId = entry->m_trackId;
    outItem->m_rowId = (int64_t)resolvedIdx;
    if (outItem->m_mediaInfo == NULL) {
        outItem->m_mediaInfo = NULL;
        outItem->m_mediaInfoShared = 0;
    }

    outItem->m_sourceType = classifyContentSourceType(outItem->m_contentPath);
    outItem->m_ownerList = this;
    outItem->m_codecProfile = entry->m_codecProfile;
    outItem->m_channels = entry->m_channels;
    outItem->m_bitRate = entry->m_bitRate;
    outItem->m_sampleRate = entry->m_sampleRate;
    outItem->m_durationMs = entry->m_durationMs;
    outItem->m_resumePositionMs = entry->m_resumePositionMs;
    outItem->m_playCount = entry->m_playCount;
    outItem->m_loopMode = entry->m_loopMode;
    return 0;
}

/* 0x811F2072 */
int UriMediaList::saveItemChanges(MediaItem *origItem, MediaItem *updatedItem)
{
    UriEntryNode *entry = (UriEntryNode *)m_entryPool->getNodeAt((unsigned int)origItem->m_rowId);
    if (entry == NULL) {
        return MC_ERR_NOT_FOUND;
    }

    if (origItem->m_codecSubType != updatedItem->m_codecSubType) {
        entry->m_codecSubType = updatedItem->m_codecSubType;
    }
    if (origItem->m_status != updatedItem->m_status) {
        entry->m_status = updatedItem->m_status;
    }
    if (origItem->m_sourceType != updatedItem->m_sourceType) {
        entry->m_sourceType = updatedItem->m_sourceType;
    }
    if (origItem->m_codecProfile != updatedItem->m_codecProfile) {
        entry->m_codecProfile = updatedItem->m_codecProfile;
    }
    if (origItem->m_sampleRate != updatedItem->m_sampleRate) {
        entry->m_sampleRate = updatedItem->m_sampleRate;
    }
    if (origItem->m_channels != updatedItem->m_channels) {
        entry->m_channels = updatedItem->m_channels;
    }
    if (origItem->m_bitRate != updatedItem->m_bitRate) {
        entry->m_bitRate = updatedItem->m_bitRate;
    }
    if (origItem->m_durationMs != updatedItem->m_durationMs) {
        entry->m_durationMs = updatedItem->m_durationMs;
    }
    if (origItem->m_playCount != updatedItem->m_playCount) {
        int delta = updatedItem->m_playCount - origItem->m_playCount;
        int64_t nextCount = (int64_t)(uint32_t)entry->m_playCount + (int64_t)delta;
        entry->m_status = (nextCount >= 0) ? (int)nextCount : 0;
    }
    return 0;
}

/* ===== DbMediaList helpers & class (0x811EEC40 - 0x811F1C69) ===== */

struct DbTrackRecordData {
    void *vtablePtr;
    char contentPath[2049];
    uint8_t padPath[3];
    int64_t musicId;
    int64_t listId;
    int64_t rowId;
    int codecProfile;
    int codecSubType;
    int status;
    int padMeta;
    int64_t fileSize;
    int channels;
    int bitRate;
    int sampleRate;
    int durationMs;
    int playCount;
    int contentType;
    int resumePositionMs;
};

struct MetadataRecordCache {
    int64_t cachedMusicId;
    dbutil::Record *record;
    int padRecord;
    paf::thread::Mutex lock;
    SceRtcTick cachedCtime;
    SceRtcTick cachedMtime;

    MetadataRecordCache()
        : cachedMusicId(-1)
        , record(NULL)
        , padRecord(0)
        , lock("SceShellMusicCoreRcdExclsv", 0)
    {
        cachedCtime.tick = 0;
        cachedMtime.tick = 0;
    }

    ~MetadataRecordCache()
    {
        if (record != NULL) {
            record->Release();
            record = NULL;
        }
    }

    void dropRecord()
    {
        if (cachedMusicId != -1) {
            if (record != NULL) {
                record->Release();
                record = NULL;
            }
            cachedMusicId = -1;
        }
    }
};

/* 0x811EF1E8 */
static int readRecordTextColumn(dbutil::Record *record, int columnIdx, char *dstBuf, SceSize dstCap)
{
    if (dstBuf == NULL || dstCap == 0u) {
        return -1;
    }
    dbutil::Value val;
    int ret = record->GetValue(columnIdx, &val);
    if (ret != 0) {
        return ret;
    }
    if (val.type != SQLITE_TEXT) {
        return -1;
    }

    SceSize copyLen = (SceSize)val.size;
    if (dstCap <= 1u) {
        copyLen = 0u;
    } else {
        if (dstCap <= copyLen) {
            copyLen = dstCap - 1u;
        }
        sce_paf_strncpy(dstBuf, val.data.text, copyLen);
    }
    dstBuf[copyLen] = '\0';
    dstBuf[dstCap - 1u] = '\0';
    return 0;
}

/* 0x811EFF36 */
int64_t queryMusicIdByContentPath(const char *contentPath, dbutil::Connection *conn)
{
    if (contentPath == NULL || conn == NULL || sce_paf_strlen(contentPath) == 0u) {
        return -1;
    }

    paf::string sql("SELECT mrid FROM tbl_Music WHERE content_path='");
    sql += contentPath;
    sql += "'";

    dbutil::RowidResult result;
    if (conn->Execute(sql, 0, NULL, &result, NULL, NULL) != 0) {
        return -1;
    }
    if (result.m_rowids.empty()) {
        return -1;
    }

    dbutil::RefPtr<dbutil::Record> rec;
    paf::string colName("mrid");
    if (result.OpenRecord(0, colName, &rec) != 0 || rec.Get() == NULL) {
        return -1;
    }

    dbutil::Value val;
    if (rec->GetValue(0, &val) == 0 && val.type == SQLITE_INTEGER) {
        return val.data.integer;
    }
    return -1;
}

/* 0x811F034C */
static int ensureMetadataRecordLoaded(MetadataRecordCache *cache, int64_t musicId,
                                      const char *filePath, DbConnectionProvider *provider,
                                      dbutil::RowidResult *auxRowids)
{
    cache->lock.Lock();

    if (cache->cachedMusicId != -1 && musicId == -1) {
        return 0;
    }

    if (cache->cachedMusicId != -1 && cache->cachedMusicId == musicId) {
        SceIoStat statBuf;
        if (filePath != NULL && sceIoGetstat(filePath, &statBuf) == 0) {
            SceRtcTick curCtime;
            SceRtcTick curMtime;
            sceRtcGetTick(&statBuf.st_ctime, &curCtime);
            sceRtcGetTick(&statBuf.st_mtime, &curMtime);
            if (curCtime.tick == cache->cachedCtime.tick
                && curMtime.tick == cache->cachedMtime.tick) {
                return 0;
            }
        }
        cache->dropRecord();
    } else {
        if (musicId == -1) {
            cache->lock.Unlock();
            return MC_ERR_NOT_FOUND;
        }
        cache->dropRecord();
    }

    if (provider == NULL || auxRowids == NULL) {
        cache->lock.Unlock();
        return MC_ERR_INVALID_ARG;
    }

    dbutil::Connection *conn = NULL;
    int connErr = provider->acquireConnection(0, &conn);
    if (connErr != 0) {
        cache->lock.Unlock();
        return connErr;
    }

    cache->lock.Unlock();

    char idBuf[24];
    sce_paf_memset(idBuf, 0, sizeof(idBuf));
    sce_paf_snprintf(idBuf, 20u, "%lld", (long long)musicId);

    paf::string sql("SELECT rowid FROM ");
    sql += "tbl_Music";
    sql += " WHERE mrid=";
    sql += idBuf;

    int ret = 0;
    int waitedMs = 0;
    int execErr = DBUTIL_ERR_SQLITE_BUSY;
    while (execErr == DBUTIL_ERR_SQLITE_BUSY && waitedMs < 10000) {
        execErr = conn->Execute(sql, 0, NULL, auxRowids, NULL, NULL);
        if (execErr == 0) {
            break;
        }
        if (execErr != DBUTIL_ERR_SQLITE_BUSY) {
            ret = MC_ERR_NOT_FOUND;
            break;
        }
        waitedMs += 50;
        sceKernelDelayThread(50000);
    }

    paf::string columns("album_name, title, artist");
    dbutil::RefPtr<dbutil::Record> openedRec;
    if (auxRowids->OpenRecord(0, columns, &openedRec) != 0) {
        ret = MC_ERR_NOT_FOUND;
    }

    provider->releaseConnection(0);

    cache->lock.Lock();
    cache->cachedMusicId = musicId;
    if (openedRec.Get() != NULL) {
        openedRec->AddRef();
        cache->record = openedRec.Get();
    }

    SceIoStat statAfter;
    sceIoGetstat(filePath, &statAfter);
    if (ret == 0) {
        sceRtcGetTick(&statAfter.st_ctime, &cache->cachedCtime);
        sceRtcGetTick(&statAfter.st_mtime, &cache->cachedMtime);
    }
    return ret;
}

class DbMediaList : public MediaListBase {
public:
    DbMediaList(MediaEntryPool *entryPool, DbConnectionProvider *dbProvider,
                OrderListFactory *orderFactory);
    virtual ~DbMediaList();

    virtual int releaseOrderList(uint8_t clearOwner);
    virtual int checkPlayableAt(int index, int *outStatus);
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId);
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem);
    virtual int rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                 uint8_t *outChanged, uint8_t forceRebuild);
    virtual int refreshOrderBuffer(int shuffleMode, int64_t anchorRowId);
    virtual int fetchItemMetadata(int64_t musicId, const char *path, MetadataOutputRequest *outMeta);
    virtual int checkChangedFlag(uint8_t *outChanged, uint8_t queryLive);

    int executeInitialQuery(const char *sqlQuery);
    int refreshAndKeepTrack(int64_t primaryRowId, int64_t fallbackRowId, int64_t *outKeptRowId);
    int getOrderedIndex(unsigned int orderPos) const;

private:
    paf::string formatQualifiedColumn(unsigned int tableCategory, int columnIndex) const;
    int selectColumnsByRowId(int64_t rowId, DbColumnBinding *columns, unsigned int columnCount);
    int updateColumnsByRowId(int64_t rowId, const DbColumnBinding *columns, unsigned int columnCount);
    int loadFullTrackRecord(int64_t rowId, DbTrackRecordData *outData);
    int findOrderPosForRowId(int64_t targetRowId, unsigned int rangeStart,
                             unsigned int rangeEnd, unsigned int *outOrderPos);
    int64_t resolveRowIdAtOrderPos(unsigned int orderPos, int64_t expectedRowId);

    MusicRowidResult m_mainRowids;          /* +8712 */
    dbutil::RowidResult m_auxRowids;        /* +8816 */
    DbConnectionProvider *m_dbProvider;     /* +8920 */
    uint8_t m_dbChangedFlag;                /* +8924 */
    uint8_t m_padChanged[3];                /* +8925..+8927 */
    MetadataRecordCache m_metaCache;        /* +8928 */
};

/* 0x811EEC40 */
int DbMediaList::executeInitialQuery(const char *sqlQuery)
{
    if (sqlQuery == NULL || sce_paf_strncasecmp(sqlQuery, "SELECT", 6u) != 0) {
        return MC_ERR_EMPTY_LIST;
    }

    m_listLock.Lock();
    dbutil::Connection *conn = NULL;
    int ret = m_dbProvider->acquireConnection(0, &conn);
    if (ret != 0) {
        m_listLock.Unlock();
        return ret;
    }

    m_dbChangedFlag = 0;
    paf::string queryStr(sqlQuery);
    int execErr = conn->Execute(queryStr, 0, NULL, &m_mainRowids, NULL, NULL);
    if (execErr != 0 || m_mainRowids.m_rowids.empty()) {
        ret = MC_ERR_EMPTY_LIST;
    } else {
        m_isReady = 1;
    }

    m_dbProvider->releaseConnection(0);
    m_listLock.Unlock();
    return ret;
}

/* 0x811EED5A */
DbMediaList::DbMediaList(MediaEntryPool *entryPool, DbConnectionProvider *dbProvider,
                         OrderListFactory *orderFactory)
    : MediaListBase(entryPool, orderFactory)
    , m_mainRowids()
    , m_auxRowids()
    , m_dbProvider(dbProvider)
    , m_dbChangedFlag(0)
    , m_metaCache()
{
    if (m_dbProvider == NULL || m_entryPool == NULL) {
        return;
    }

    MediaEntryNode *node = m_entryPool->getNodeAt(0u);
    if (node == NULL) {
        return;
    }
    while (node != NULL) {
        if (node->m_entryKind != 2) {
            return;
        }
        node = node->m_next;
    }

    DbQueryEntryNode *queryNode = (DbQueryEntryNode *)m_entryPool->getNodeAt(0u);
    if (queryNode == NULL || executeInitialQuery(queryNode->m_sqlQuery) != 0) {
        return;
    }

    m_listLock.Lock();
    if (m_isReady == 1u) {
        m_anchorId = queryNode->m_anchorId;
        m_initialIndex = queryNode->m_initialIndex;
        m_itemCount = (unsigned int)m_mainRowids.m_rowids.size();
        m_currentIndex = queryNode->m_initialIndex;
        m_sortMode = queryNode->m_sortMode;
        m_tableCategory = queryNode->m_tableCategory;

        uint8_t navValid = 0;
        m_currentIndex = resolveNavigationIndex(m_initialIndex, m_initialIndex, &navValid);
    }
    m_listLock.Unlock();
}

/* 0x811EEEAE */
int DbMediaList::releaseOrderList(uint8_t clearOwner)
{
    MediaListBase::releaseOrderList(clearOwner);
    return 0;
}

/* 0x811EEEBC / 0x811EEF26 */
DbMediaList::~DbMediaList()
{
    releaseOrderList(1u);
}

/* 0x811EEF3C */
paf::string DbMediaList::formatQualifiedColumn(unsigned int tableCategory, int columnIndex) const
{
    paf::string qualified("");
    if (tableCategory == 0u) {
        qualified += "tbl_Music";
        qualified += ".";
        qualified += kMusicTableColumns[columnIndex].columnName;
    } else if (tableCategory == 2u) {
        qualified += "tbl_AVContentItem";
        qualified += ".";
        qualified += kContentItemTableColumns[columnIndex].columnName;
    }
    return qualified;
}

/* 0x811EF260 */
int DbMediaList::selectColumnsByRowId(int64_t rowId, DbColumnBinding *columns, unsigned int columnCount)
{
    if (columns == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    bool hasSelection = false;
    for (unsigned int idx = 0; idx < columnCount; ++idx) {
        if (columns[idx].selected == 1u) {
            hasSelection = true;
            break;
        }
    }
    if (!hasSelection) {
        return MC_ERR_INVALID_ARG;
    }

    DbQueryEntryNode *queryNode = (DbQueryEntryNode *)m_entryPool->getNodeAt(0u);
    if (queryNode == NULL) {
        return MC_ERR_INTERNAL;
    }

    unsigned int tableCategory = (unsigned int)queryNode->m_tableCategory;
    if (isIdRejected(rowId, tableCategory) == 1) {
        return MC_ERR_NOT_FOUND;
    }

    int64_t musicId = -1;
    if (tableCategory == 0u) {
        musicId = rowId;
    } else if (tableCategory == 2u) {
        paf::string listCol = formatQualifiedColumn(tableCategory, 0);
        dbutil::RefPtr<dbutil::Record> listRec;
        if (m_mainRowids.OpenRecord(rowId, listCol, &listRec) != 0) {
            return MC_ERR_NOT_FOUND;
        }
        int64_t listId = -1;
        dbutil::Value listVal;
        if (listRec.Get() != NULL && listRec->GetValue(0, &listVal) == 0
            && listVal.type == SQLITE_INTEGER) {
            listId = listVal.data.integer;
        }
        if (isIdRejected(listId, 1u) == 1) {
            return MC_ERR_EMPTY_LIST;
        }

        paf::string itemCol = formatQualifiedColumn(tableCategory, 1);
        dbutil::RefPtr<dbutil::Record> itemRec;
        if (m_mainRowids.OpenRecord(rowId, itemCol, &itemRec) != 0) {
            return MC_ERR_NOT_FOUND;
        }
        dbutil::Value itemVal;
        if (itemRec.Get() != NULL && itemRec->GetValue(0, &itemVal) == 0
            && itemVal.type == SQLITE_INTEGER) {
            musicId = itemVal.data.integer;
        }
    } else {
        return MC_ERR_EMPTY_LIST;
    }

    dbutil::Connection *conn = NULL;
    int ret = m_dbProvider->acquireConnection(0, &conn);
    if (ret != 0) {
        return ret;
    }

    char idBuf[24];
    sce_paf_memset(idBuf, 0, sizeof(idBuf));
    sce_paf_snprintf(idBuf, 20u, "%llu", (unsigned long long)musicId);

    paf::string sql("SELECT rowid FROM ");
    sql += "tbl_Music";
    sql += " WHERE mrid=";
    sql += idBuf;

    int waitedMs = 0;
    int execErr = DBUTIL_ERR_SQLITE_BUSY;
    while (execErr == DBUTIL_ERR_SQLITE_BUSY && waitedMs < 10000) {
        execErr = conn->Execute(sql, 0, NULL, &m_auxRowids, NULL, NULL);
        if (execErr == 0) {
            break;
        }
        if (execErr != DBUTIL_ERR_SQLITE_BUSY) {
            ret = MC_ERR_NOT_FOUND;
            break;
        }
        waitedMs += 50;
        sceKernelDelayThread(50000);
    }

    if (waitedMs >= 10000) {
        m_dbProvider->releaseConnection(0);
        return MC_ERR_INTERNAL;
    }
    if (ret != 0) {
        m_dbProvider->releaseConnection(0);
        return ret;
    }

    paf::string colList("");
    bool firstCol = true;
    for (unsigned int idx = 0; idx < columnCount; ++idx) {
        if (columns[idx].selected == 1u) {
            if (!firstCol) {
                colList += ", ";
            }
            firstCol = false;
            colList += columns[idx].columnName;
        }
    }

    dbutil::RefPtr<dbutil::Record> rec;
    if (m_auxRowids.OpenRecord(0, colList, &rec) != 0) {
        m_dbProvider->releaseConnection(0);
        return MC_ERR_NOT_FOUND;
    }

    int resultCol = 0;
    for (unsigned int idx = 0; idx < columnCount; ++idx) {
        if (columns[idx].selected != 1u) {
            continue;
        }
        if (columns[idx].valueType == DB_COL_TEXT) {
            readRecordTextColumn(rec.Get(), resultCol, (char *)columns[idx].valuePtr,
                                 columns[idx].bufferCapacity);
        } else if (columns[idx].valueType == DB_COL_INT64) {
            dbutil::Value val;
            if (rec->GetValue(resultCol, &val) == 0 && val.type == SQLITE_INTEGER) {
                *(int64_t *)columns[idx].valuePtr = val.data.integer;
            }
        } else if (columns[idx].valueType == DB_COL_INT32) {
            dbutil::Value val;
            if (rec->GetValue(resultCol, &val) == 0 && val.type == SQLITE_INTEGER) {
                *(int *)columns[idx].valuePtr = (int)val.data.integer;
            }
        }
        ++resultCol;
    }

    m_dbProvider->releaseConnection(0);
    return 0;
}

/* 0x811EFA26 */
int DbMediaList::loadFullTrackRecord(int64_t rowId, DbTrackRecordData *outData)
{
    if (m_dbProvider == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    DbColumnBinding bindings[15];
    sce_paf_memcpy(bindings, kMusicTableColumns, sizeof(bindings));

    for (int idx = 0; idx < 15; ++idx) {
        if (idx != 0 && idx != 14) {
            bindings[idx].selected = 1;
        }
    }

    bindings[1].valuePtr = &outData->musicId;
    bindings[2].valuePtr = outData->contentPath;
    bindings[2].bufferCapacity = 2049u;
    bindings[3].valuePtr = &outData->codecProfile;
    bindings[4].valuePtr = &outData->codecSubType;
    bindings[5].valuePtr = &outData->status;
    bindings[6].valuePtr = &outData->fileSize;
    bindings[7].valuePtr = &outData->channels;
    bindings[8].valuePtr = &outData->bitRate;
    bindings[9].valuePtr = &outData->sampleRate;
    bindings[10].valuePtr = &outData->durationMs;
    bindings[11].valuePtr = &outData->playCount;
    bindings[12].valuePtr = &outData->contentType;
    bindings[13].valuePtr = &outData->resumePositionMs;

    int ret = selectColumnsByRowId(rowId, bindings, 15u);
    if (ret != 0) {
        return ret;
    }

    if (convertToMusicDevicePath(outData->contentPath, outData->contentPath, 2049u) != 0) {
        return MC_ERR_INTERNAL;
    }

    int64_t listId = -1;
    DbQueryEntryNode *queryNode = (DbQueryEntryNode *)m_entryPool->getNodeAt(0u);
    if (queryNode == NULL) {
        return MC_ERR_INTERNAL;
    }

    if (queryNode->m_tableCategory == 2) {
        paf::string listCol = formatQualifiedColumn(2u, 0);
        dbutil::RefPtr<dbutil::Record> listRec;
        if (m_mainRowids.OpenRecord(rowId, listCol, &listRec) != 0) {
            ret = MC_ERR_NOT_FOUND;
        } else if (listRec.Get() != NULL) {
            dbutil::Value val;
            if (listRec->GetValue(0, &val) == 0 && val.type == SQLITE_INTEGER) {
                listId = val.data.integer;
            }
        }
    }

    outData->rowId = rowId;
    outData->listId = listId;
    return ret;
}

/* 0x811EFC0C */
int DbMediaList::findOrderPosForRowId(int64_t targetRowId, unsigned int rangeStart,
                                      unsigned int rangeEnd, unsigned int *outOrderPos)
{
    unsigned int clampedEnd = (rangeEnd > m_orderCapacity) ? m_orderCapacity : rangeEnd;
    int found = 0;

    for (unsigned int rawIdx = rangeStart; rawIdx <= clampedEnd; ++rawIdx) {
        int64_t rowId = m_mainRowids.GetRowid((int)rawIdx);
        if (rowId < 0) {
            return 0;
        }
        if (rowId == targetRowId) {
            if (m_orderBuffer[rawIdx] == (uint16_t)rawIdx) {
                *outOrderPos = rawIdx;
                return 1;
            }
            for (unsigned int orderIdx = rangeStart; orderIdx <= clampedEnd; ++orderIdx) {
                if (m_orderBuffer[orderIdx] == (uint16_t)rawIdx) {
                    *outOrderPos = orderIdx;
                    found = 1;
                    break;
                }
            }
        }
    }
    return found;
}

/* 0x811EFCD6 */
int64_t DbMediaList::resolveRowIdAtOrderPos(unsigned int orderPos, int64_t expectedRowId)
{
    if (m_orderBuffer == NULL || m_itemCount == 0u || orderPos > m_orderCapacity) {
        return -1;
    }
    if ((int)orderPos >= NAV_LAST && (int)orderPos <= NAV_CURRENT) {
        return -1;
    }

    int64_t rowId = m_mainRowids.GetRowid((int)m_orderBuffer[orderPos]);
    if (rowId < 0) {
        rowId = -1;
    }

    if (expectedRowId != -1) {
        int matched = (rowId != -1 && rowId == expectedRowId) ? 1 : 0;
        if (rowId != -1 && matched == 0) {
            unsigned int winStart = (orderPos >= 15u) ? (orderPos - 15u) : 0u;
            unsigned int winEnd = (orderPos + 16u > m_itemCount) ? (m_itemCount - 1u) : (orderPos + 15u);
            unsigned int foundPos = 0;
            matched = findOrderPosForRowId(expectedRowId, winStart, winEnd, &foundPos);
            if (matched == 1) {
                m_currentIndex = (int)foundPos;
            }
        }
        if (matched == 0) {
            unsigned int foundPos = 0;
            matched = findOrderPosForRowId(expectedRowId, 0u, m_itemCount - 1u, &foundPos);
            if (matched == 1) {
                m_currentIndex = (int)foundPos;
            }
        }
        if (matched == 0) {
            rowId = -1;
        }
    }
    return rowId;
}

/* 0x811EFE48 */
int DbMediaList::checkPlayableAt(int index, int *outStatus)
{
    if (outStatus == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    *outStatus = 2;

    int64_t rowId = resolveRowIdAtOrderPos((unsigned int)index, -1);
    if (rowId == -1) {
        return MC_ERR_NOT_FOUND;
    }

    DbTrackRecordData recData;
    sce_paf_memset(&recData, 0, sizeof(recData));
    recData.musicId = -1;
    recData.listId = -1;
    recData.rowId = -1;
    recData.status = 2;

    int ret = loadFullTrackRecord(rowId, &recData);
    if (ret == 0) {
        *outStatus = recData.status;
    }
    return ret;
}

/* 0x811F01BC */
int DbMediaList::rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                  uint8_t *outChanged, uint8_t forceRebuild)
{
    (void)outChanged;
    int prevOwnerId = 0;
    onOrderListReserved(&prevOwnerId, 0);
    if (m_orderBuffer == NULL) {
        return MC_ERR_NO_MEMORY;
    }

    int ret = 0;
    if (prevOwnerId != getListId() || forceRebuild != 0u || m_shuffleMode != shuffleMode) {
        ret = refreshOrderBuffer(shuffleMode, anchorRowId);
    }

    if (ret == 0) {
        unsigned int count = (unsigned int)m_mainRowids.m_rowids.size();
        m_itemCount = count;
        m_shuffleMode = shuffleMode;
        if (count == 0u) {
            m_currentIndex = NAV_INVALID;
        } else if (anchorRowId == -1) {
            if (m_currentIndex == NAV_INVALID) {
                m_currentIndex = 0;
            }
        } else {
            unsigned int foundPos = (unsigned int)NAV_INVALID;
            if (findOrderPosForRowId(anchorRowId, 0u, count - 1u, &foundPos) == 1) {
                m_currentIndex = (int)foundPos;
            } else {
                m_currentIndex = NAV_INVALID;
            }
        }
    }
    return ret;
}

/* 0x811F02CA */
int DbMediaList::refreshOrderBuffer(int shuffleMode, int64_t anchorRowId)
{
    if (shuffleMode != 0) {
        if (m_mainRowids.generateShuffledOrder((uint32_t)shuffleMode, anchorRowId,
                                               m_orderBuffer, m_orderCapacity) != 0) {
            return MC_ERR_INTERNAL;
        }
        return 0;
    }

    unsigned int count = (unsigned int)m_mainRowids.m_rowids.size();
    if (count > m_orderCapacity) {
        return MC_ERR_INTERNAL;
    }
    for (unsigned int idx = 0; idx < count; ++idx) {
        m_orderBuffer[idx] = (uint16_t)idx;
    }
    return 0;
}

/* 0x811F0A26 */
int DbMediaList::fetchItemMetadata(int64_t musicId, const char *path,
                                   MetadataOutputRequest *outMeta)
{
    int ret = ensureMetadataRecordLoaded(&m_metaCache, musicId, path, m_dbProvider, &m_auxRowids);
    if (ret != 0) {
        return ret;
    }

    if (outMeta != NULL) {
        if (outMeta->lock != NULL) {
            outMeta->lock->Lock();
        }

        if (outMeta->albumOut != NULL && outMeta->albumCapacity != 0u) {
            int colRet = -1;
            if (m_metaCache.cachedMusicId != -1 && m_metaCache.record != NULL) {
                dbutil::RefPtr<dbutil::Record> recRef(m_metaCache.record);
                colRet = readRecordTextColumn(recRef.Get(), 0, outMeta->albumOut, outMeta->albumCapacity);
            }
            if (colRet != 0) {
                outMeta->albumOut[0] = '\0';
            }
            outMeta->albumOut[outMeta->albumCapacity - 1u] = '\0';
        }

        if (outMeta->titleOut != NULL && outMeta->titleCapacity != 0u) {
            int colRet = -1;
            if (m_metaCache.cachedMusicId != -1 && m_metaCache.record != NULL) {
                dbutil::RefPtr<dbutil::Record> recRef(m_metaCache.record);
                colRet = readRecordTextColumn(recRef.Get(), 1, outMeta->titleOut, outMeta->titleCapacity);
            }
            if (colRet != 0) {
                outMeta->titleOut[0] = '\0';
            }
            outMeta->titleOut[outMeta->titleCapacity - 1u] = '\0';
        }

        if (outMeta->artistOut != NULL && outMeta->artistCapacity != 0u) {
            int colRet = -1;
            if (m_metaCache.cachedMusicId != -1 && m_metaCache.record != NULL) {
                dbutil::RefPtr<dbutil::Record> recRef(m_metaCache.record);
                colRet = readRecordTextColumn(recRef.Get(), 2, outMeta->artistOut, outMeta->artistCapacity);
            }
            if (colRet != 0) {
                outMeta->artistOut[0] = '\0';
            }
            outMeta->artistOut[outMeta->artistCapacity - 1u] = '\0';
        }

        if (outMeta->lock != NULL) {
            outMeta->lock->Unlock();
        }
    }

    m_metaCache.lock.Unlock();
    return 0;
}

/* 0x811F0C02 */
int DbMediaList::getOrderedIndex(unsigned int orderPos) const
{
    if (m_orderBuffer != NULL && orderPos < m_orderCapacity) {
        return (int)m_orderBuffer[orderPos];
    }
    return NAV_INVALID;
}

/* 0x811F0C20 */
int DbMediaList::updateColumnsByRowId(int64_t rowId, const DbColumnBinding *columns,
                                      unsigned int columnCount)
{
    if (columns == NULL) {
        return 0;
    }

    bool hasUpdate = false;
    for (unsigned int idx = 0; idx < columnCount; ++idx) {
        if (columns[idx].selected == 1u) {
            hasUpdate = true;
            break;
        }
    }
    if (!hasUpdate) {
        return 0;
    }

    DbQueryEntryNode *queryNode = (DbQueryEntryNode *)m_entryPool->getNodeAt(0u);
    if (queryNode == NULL) {
        return 0;
    }

    int64_t musicId = 0;
    unsigned int tableCategory = (unsigned int)queryNode->m_tableCategory;
    if (tableCategory == 0u) {
        musicId = rowId;
    } else if (tableCategory == 2u) {
        paf::string itemCol = formatQualifiedColumn(tableCategory, 1);
        dbutil::RefPtr<dbutil::Record> rec;
        if (m_mainRowids.OpenRecord(rowId, itemCol, &rec) != 0) {
            return 0;
        }
        dbutil::Value val;
        if (rec.Get() != NULL && rec->GetValue(0, &val) == 0 && val.type == SQLITE_INTEGER) {
            musicId = val.data.integer;
        }
    } else {
        return 0;
    }

    char idBuf[24];
    sce_paf_memset(idBuf, 0, sizeof(idBuf));
    sce_paf_snprintf(idBuf, 20u, "%llu", (unsigned long long)musicId);

    paf::string sql("UPDATE tbl_Music SET");
    bool firstField = true;
    char numBuf[24];
    numBuf[20] = '\0';

    for (unsigned int idx = 0; idx < columnCount; ++idx) {
        if (columns[idx].selected != 1u) {
            continue;
        }
        if (!firstField) {
            sql += ",";
        }
        firstField = false;
        sql += " ";
        sql += columns[idx].columnName;
        sql += "=";

        if (columns[idx].valueType == DB_COL_INT64) {
            int64_t int64Value = *(const int64_t *)columns[idx].valuePtr;
            sce_paf_snprintf(numBuf, 20u, "%lld", (long long)int64Value);
            sql += numBuf;
        } else if (columns[idx].valueType == DB_COL_INT32) {
            int int32Value = *(const int *)columns[idx].valuePtr;
            sce_paf_snprintf(numBuf, 20u, "%d", int32Value);
            sql += numBuf;
        } else if (columns[idx].valueType == DB_COL_TEXT || columns[idx].valueType == DB_COL_DATETIME) {
            sql += "'";
            sql += (const char *)columns[idx].valuePtr;
            sql += "'";
        } else {
            return 0;
        }
    }

    sql += " WHERE mrid=";
    sql += idBuf;

    dbutil::Connection *conn = NULL;
    if (m_dbProvider->acquireConnection(1, &conn) != 0) {
        return 0;
    }
    conn->Execute(sql, 0, NULL, NULL, NULL, NULL);
    m_dbProvider->releaseConnection(1);
    return 0;
}

/* 0x811F1428 */
int DbMediaList::loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId)
{
    (void)unused;
    m_listLock.Lock();

    if (m_isReady == 0u) {
        m_listLock.Unlock();
        return MC_ERR_NOT_READY;
    }
    if (m_itemCount == 0u) {
        m_listLock.Unlock();
        return MC_ERR_EMPTY_LIST;
    }

    uint8_t navValid = 0;
    int resolvedIdx = resolveNavigationIndex(navTarget, m_currentIndex, &navValid);
    if (resolvedIdx == NAV_INVALID) {
        m_listLock.Unlock();
        if (navTarget == NAV_NEXT) {
            return MC_ERR_END_OF_LIST;
        }
        if (navTarget == NAV_PREV) {
            return MC_ERR_START_OF_LIST;
        }
        return MC_ERR_NOT_FOUND;
    }
    if ((unsigned int)resolvedIdx >= m_itemCount) {
        m_listLock.Unlock();
        return MC_ERR_INTERNAL;
    }

    int64_t rowId = resolveRowIdAtOrderPos((unsigned int)resolvedIdx, expectedRowId);
    if (rowId == -1) {
        m_listLock.Unlock();
        return MC_ERR_NOT_FOUND;
    }

    DbTrackRecordData recData;
    sce_paf_memset(&recData, 0, sizeof(recData));
    recData.musicId = -1;
    recData.listId = -1;
    recData.rowId = -1;
    recData.status = 2;

    int ret = loadFullTrackRecord(rowId, &recData);
    m_listLock.Unlock();
    if (ret != 0) {
        return ret;
    }

    outItem->m_rowId = recData.rowId;
    outItem->m_musicId = recData.musicId;
    outItem->m_listId = recData.listId;
    if (outItem->m_mediaInfo == NULL) {
        outItem->m_mediaInfo = NULL;
        outItem->m_mediaInfoShared = 0;
    }

    sce_paf_strncpy(outItem->m_contentPath, recData.contentPath, 2047u);
    outItem->m_contentPath[2047] = '\0';
    outItem->m_codecSubType = recData.codecSubType;
    outItem->m_status = recData.status;
    outItem->m_fileSize = recData.fileSize;
    outItem->m_codecProfile = recData.codecProfile;
    outItem->m_channels = recData.channels;
    outItem->m_bitRate = recData.bitRate;
    outItem->m_sampleRate = recData.sampleRate;
    outItem->m_durationMs = recData.durationMs;
    outItem->m_resumePositionMs = 0;
    outItem->m_playCount = recData.playCount;
    outItem->m_sourceType = classifyContentSourceType(outItem->m_contentPath);
    outItem->m_ownerList = this;
    return 0;
}

/* 0x811F1728 */
int DbMediaList::saveItemChanges(MediaItem *origItem, MediaItem *updatedItem)
{
    DbColumnBinding bindings[15];
    sce_paf_memcpy(bindings, kMusicTableColumns, sizeof(bindings));

    m_listLock.Lock();
    if (m_isReady == 0u) {
        m_listLock.Unlock();
        return MC_ERR_NOT_READY;
    }

    if (origItem->m_codecProfile != updatedItem->m_codecProfile) {
        origItem->m_codecProfile = updatedItem->m_codecProfile;
        bindings[3].selected = 1;
        bindings[3].valuePtr = &origItem->m_codecProfile;
    }
    if (origItem->m_codecSubType != updatedItem->m_codecSubType) {
        origItem->m_codecSubType = updatedItem->m_codecSubType;
        bindings[4].selected = 1;
        bindings[4].valuePtr = &origItem->m_codecSubType;
    }
    if (origItem->m_status != updatedItem->m_status) {
        origItem->m_status = updatedItem->m_status;
        bindings[5].selected = 1;
        bindings[5].valuePtr = &origItem->m_status;
    }
    if (origItem->m_fileSize != updatedItem->m_fileSize) {
        origItem->m_fileSize = updatedItem->m_fileSize;
        bindings[6].selected = 1;
        bindings[6].valuePtr = &origItem->m_fileSize;
    }
    if (origItem->m_channels != updatedItem->m_channels) {
        origItem->m_channels = updatedItem->m_channels;
        bindings[7].selected = 1;
        bindings[7].valuePtr = &origItem->m_channels;
    }
    if (origItem->m_bitRate != updatedItem->m_bitRate) {
        origItem->m_bitRate = updatedItem->m_bitRate;
        bindings[8].selected = 1;
        bindings[8].valuePtr = &origItem->m_bitRate;
    }
    if (origItem->m_sampleRate != updatedItem->m_sampleRate) {
        origItem->m_sampleRate = updatedItem->m_sampleRate;
        bindings[9].selected = 1;
        bindings[9].valuePtr = &origItem->m_sampleRate;
    }
    if (origItem->m_durationMs != updatedItem->m_durationMs) {
        origItem->m_durationMs = updatedItem->m_durationMs;
        bindings[10].selected = 1;
        bindings[10].valuePtr = &origItem->m_durationMs;
    }

    int ret = 0;
    paf::string timestampStr("");
    int dbPlayCount = 0;

    if (origItem->m_playCount != updatedItem->m_playCount) {
        DbColumnBinding playCountQuery = bindings[11];
        playCountQuery.selected = 1;
        playCountQuery.valuePtr = &dbPlayCount;

        if (selectColumnsByRowId(origItem->m_rowId, &playCountQuery, 1u) == 0) {
            int delta = updatedItem->m_playCount - origItem->m_playCount;
            int64_t nextCount = (int64_t)(uint32_t)dbPlayCount + (int64_t)delta;
            dbPlayCount = (nextCount >= 0) ? (int)nextCount : 0;

            if (delta != 0) {
                origItem->m_playCount = updatedItem->m_playCount;
                bindings[11].selected = 1;
                bindings[11].valuePtr = &dbPlayCount;

                if (m_dbProvider != NULL) {
                    dbutil::Connection *conn = NULL;
                    ret = m_dbProvider->acquireConnection(0, &conn);
                    if (ret == 0) {
                        dbutil::RefPtr<dbutil::Statement> stmt;
                        paf::string timeSql("select strftime(\"%Y-%m-%dT%H:%M:%f\",\"now\");");
                        int prepErr = conn->Prepare(timeSql, &stmt);
                        m_dbProvider->releaseConnection(0);

                        if (prepErr == 0 && stmt.Get() != NULL) {
                            stmt->Reset();
                            stmt->Step(1);
                            dbutil::Value timeVal;
                            if (stmt->GetValue(0, &timeVal) == 0 && timeVal.size <= 1024) {
                                if (timeVal.data.text != NULL) {
                                    timestampStr = timeVal.data.text;
                                }
                                bindings[14].selected = 1;
                                bindings[14].valuePtr = (void *)timestampStr.c_str();
                                bindings[14].bufferCapacity = timestampStr.size();
                            }
                        }
                    }
                }
            }
        }
    }

    updateColumnsByRowId(origItem->m_rowId, bindings, 15u);
    m_listLock.Unlock();
    return ret;
}

/* 0x811F1A94 */
int DbMediaList::refreshAndKeepTrack(int64_t primaryRowId, int64_t fallbackRowId,
                                     int64_t *outKeptRowId)
{
    m_listLock.Lock();
    if (m_isReady != 1u) {
        m_listLock.Unlock();
        return MC_ERR_NOT_READY;
    }

    m_dbChangedFlag = 0;
    if (m_mainRowids.Refresh(NULL, NULL) != 0) {
        m_listLock.Unlock();
        return MC_ERR_NOT_READY;
    }

    int savedShuffleMode = m_shuffleMode;
    uint8_t listChanged = 1;
    int ret = rebuildOrderList(0, -1, &listChanged, 1u);
    if (ret != 0) {
        m_listLock.Unlock();
        return ret;
    }

    int64_t candidates[2];
    candidates[0] = primaryRowId;
    candidates[1] = fallbackRowId;

    int keepErr = MC_ERR_NOT_FOUND;
    for (int idx = 0; idx < 2; ++idx) {
        unsigned int count = (unsigned int)m_mainRowids.m_rowids.size();
        m_itemCount = count;
        if (count == 0u) {
            keepErr = MC_ERR_EMPTY_LIST;
            m_currentIndex = NAV_INVALID;
            break;
        }

        unsigned int foundPos = (unsigned int)NAV_INVALID;
        if (findOrderPosForRowId(candidates[idx], 0u, count - 1u, &foundPos) == 1) {
            *outKeptRowId = candidates[idx];
            m_currentIndex = (int)foundPos;
            keepErr = 0;
            break;
        }
        m_currentIndex = NAV_INVALID;
    }

    if (savedShuffleMode != 0 && keepErr == 0) {
        if (rebuildOrderList(savedShuffleMode, *outKeptRowId, &listChanged, 0u) != 0) {
            m_listLock.Unlock();
            return MC_ERR_INTERNAL;
        }
    }

    m_listLock.Unlock();
    return keepErr;
}

/* 0x811F1C00 */
int DbMediaList::checkChangedFlag(uint8_t *outChanged, uint8_t queryLive)
{
    m_listLock.Lock();
    if (m_isReady == 0u) {
        m_listLock.Unlock();
        return MC_ERR_NOT_READY;
    }

    if (queryLive == 1u) {
        m_mainRowids.m_lock.Lock();
        uint8_t changed = m_mainRowids.m_changed ? 1u : 0u;
        m_mainRowids.m_lock.Unlock();
        m_dbChangedFlag = changed;
        *outChanged = changed;
    } else {
        *outChanged = m_dbChangedFlag;
    }

    m_listLock.Unlock();
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
