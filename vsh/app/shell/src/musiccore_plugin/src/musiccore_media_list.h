/*
 * SceShell musiccore_plugin - OrderListFactory, MediaItem, MediaListBase,
 * UriMediaList, and DbMediaList declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_MEDIA_LIST_H
#define VSH_APP_SHELL_MUSICCORE_MEDIA_LIST_H

#include <dbutil/connection.h>
#include <dbutil/record.h>
#include <dbutil/rowid_result.h>
#include <kernel.h>
#include <rtc.h>
#include <std/string>
#include <stddef.h>
#include <stdint.h>
#include <thread/thread.h>

#include "musiccore_state_machine.h"

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

struct MediaInfoHandle {
    void *vtablePtr;
    uint32_t mediaFourcc;
    uint8_t sharedFlag;
};

class DbConnectionProvider {
public:
    virtual ~DbConnectionProvider() {}
    virtual void initialize() = 0;
    virtual void finalize() = 0;
    virtual int acquireConnection(int writeMode, dbutil::Connection **outConn) = 0;
    virtual void releaseConnection(int writeMode) = 0;
};

class MusicRowidResult : public dbutil::RowidResult {
public:
    MusicRowidResult();
    virtual ~MusicRowidResult();
    int generateShuffledOrder(uint32_t seed, int64_t anchorRowid,
                              uint16_t *outIndices, uint32_t capacity);
private:
    uint32_t m_rngState;
};

struct DbColumnBinding {
    const char *columnName;
    int valueType;
    uint8_t selected;
    uint8_t padSelected[3];
    void *valuePtr;
    SceSize bufferCapacity;
};

class MediaEntryNode {
public:
    virtual ~MediaEntryNode();
    virtual size_t getAllocatedSize() const = 0;

    int m_entryKind;
    MediaEntryNode *m_prev;
    MediaEntryNode *m_next;
};

class MediaEntryPool {
public:
    MediaEntryPool(int minCapacity, unsigned int arenaBytes);
    virtual ~MediaEntryPool();

    void *reserveSlot(int neededBytes);
    void appendNode(MediaEntryNode *node);
    MediaEntryNode *getNodeAt(unsigned int index);
    unsigned int getNodeCount() const;

private:
    void *m_arenaStart;
    MediaEntryNode *m_head;
    MediaEntryNode *m_tail;
    unsigned int m_freeBytes;
    unsigned int m_nodeCount;
    int m_poolSerial;
};

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

    virtual void *acquireOrderList(void *ownerList, int *outPrevOwnerId, SceUInt *timeout);
    virtual void releaseOrderList(void *bufferPtr, uint8_t clearOwner);
    virtual int getMaxItems() const;

private:
    uint8_t *m_slotsBase;
    int m_maxItems;
    int m_slotCount;
    int m_elemBytes;
    SceUID m_slotSema;
    paf::thread::Mutex m_lock;
};

struct MetadataOutputRequest {
    char *albumOut;
    SceSize albumCapacity;
    char *titleOut;
    SceSize titleCapacity;
    char *artistOut;
    SceSize artistCapacity;
    paf::thread::Mutex *lock;
};

class MediaListBase;

class MediaItem {
public:
    MediaItem();
    virtual ~MediaItem();

    void reset();
    void copyFrom(const MediaItem &other);
    int probe(int ioPriority, uint8_t parseMeta);
    int acquireFromList(int navTarget, MediaListBase *listObj);
    int releaseToList(uint8_t commitChanges);
    int transferTo(MediaItem *dst, uint8_t commitDst);
    int commitToList();
    int readMetadata(MetadataOutputRequest *outMeta);

    int m_codecSubType;
    int m_status;
    int m_padStatus;
    int64_t m_rowId;
    int64_t m_musicId;
    int64_t m_listId;
    char m_contentPath[2048];
    int m_sourceType;
    int m_codecProfile;
    int m_channels;
    int m_bitRate;
    int m_sampleRate;
    int m_durationMs;
    int64_t m_fileSize;
    int m_resumePositionMs;
    int m_playCount;
    int m_loopMode;
    int m_slotSerial;
    MediaListBase *m_ownerList;
    MediaInfoHandle *m_mediaInfo;
    uint8_t m_mediaInfoShared;
    uint8_t m_padTail[7];
};

class MediaListBase {
public:
    MediaListBase(MediaEntryPool *entryPool, OrderListFactory *orderFactory);
    virtual ~MediaListBase();

    virtual int checkOrderListStatus(int *outPrevId, uint8_t *outAllocated);
    virtual int releaseOrderList(uint8_t clearOwner);
    virtual int getListId() const;
    virtual MediaEntryPool *getEntryPool();
    virtual int navigateTo(int navTarget, uint8_t *outValid);
    virtual int resolveErrorNavigation(int unused, int repeatMode, int direction,
                                       unsigned int *ioAttempts, int *ioErr,
                                       uint8_t *outWrappedAll, uint8_t *outWrappedOne);
    virtual int refreshAndKeepTrack(int64_t initialRowId, int64_t fallbackRowId,
                                    int64_t *outKeptRowId);
    virtual void setEntryTrackId(int index, int64_t trackId);
    virtual int checkChangedFlag(uint8_t *outChanged, uint8_t queryLive);
    virtual int checkValid();
    virtual int checkPlayableAt(int index, int *outStatus) = 0;
    virtual int getListStatus(MediaListStatus *outStatus);
    virtual int getTableCategory() const;
    virtual unsigned int getItemCount() const;
    virtual int getCurrentIndex() const;
    virtual int64_t getAnchorId() const;
    virtual int rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                 uint8_t *outChanged, uint8_t forceRebuild);
    virtual int unlockOrderList(uint8_t clearOwner);
    virtual int refreshOrderBuffer(int shuffleMode, int64_t anchorRowId);
    virtual int fetchItemMetadata(int64_t musicId, const char *path, MetadataOutputRequest *outMeta);
    virtual int getOrderedIndex(unsigned int orderPos) const;
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId) = 0;
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem) = 0;

    int resolveNavigationIndex(int navTarget, int currentIdx, uint8_t *outValid) const;
    void releaseItemSlot(const MediaItem *item);
    MediaItem *allocateItemSlot();
    MediaItem *findItemSlot(int slotSerial);

    MediaEntryPool *m_entryPool;
    void *m_auxVtable;
    unsigned int m_itemCount;
    int m_currentIndex;
    int m_padIndex;
    int64_t m_anchorId;
    int m_initialIndex;
    int m_shuffleMode;
    int m_sortMode;
    int m_tableCategory;
    MediaItem m_itemSlots[4];
    uint8_t m_isReady;
    uint8_t m_padReady[7];
    paf::thread::Mutex m_listLock;
    uint16_t *m_orderBuffer;
    unsigned int m_orderCapacity;
    OrderListFactory *m_orderFactory;
    int m_listId;
};

class UriMediaList : public MediaListBase {
public:
    UriMediaList(MediaEntryPool *entryPool, int shuffleMode, OrderListFactory *orderFactory);
    virtual ~UriMediaList();

    virtual void setEntryTrackId(int index, int64_t trackId);
    virtual int checkPlayableAt(int index, int *outStatus);
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId);
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem);

    int resolveEntryTrackId(int index, int64_t *ioTrackId);
};

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

    MetadataRecordCache();
    ~MetadataRecordCache();
    void dropRecord();
};

class DbMediaList : public MediaListBase {
public:
    DbMediaList(MediaEntryPool *entryPool, DbConnectionProvider *dbProvider,
                OrderListFactory *orderFactory);
    virtual ~DbMediaList();

    virtual int refreshAndKeepTrack(int64_t initialRowId, int64_t fallbackRowId,
                                    int64_t *outKeptRowId);
    virtual int checkChangedFlag(uint8_t *outChanged, uint8_t queryLive);
    virtual int checkPlayableAt(int index, int *outStatus);
    virtual int rebuildOrderList(int shuffleMode, int64_t anchorRowId,
                                 uint8_t *outChanged, uint8_t forceRebuild);
    virtual int unlockOrderList(uint8_t clearOwner);
    virtual int refreshOrderBuffer(int shuffleMode, int64_t anchorRowId);
    virtual int fetchItemMetadata(int64_t musicId, const char *path, MetadataOutputRequest *outMeta);
    virtual int getOrderedIndex(unsigned int orderPos) const;
    virtual int loadItemAt(int navTarget, MediaItem *outItem, int unused, int64_t expectedRowId);
    virtual int saveItemChanges(MediaItem *origItem, MediaItem *updatedItem);

    int executeInitialQuery(const char *sqlQuery);

private:
    paf::string formatQualifiedColumn(unsigned int tableCategory, int columnIndex) const;
    int selectColumnsByRowId(int64_t rowId, DbColumnBinding *columns, unsigned int columnCount);
    int updateColumnsByRowId(int64_t rowId, const DbColumnBinding *columns, unsigned int columnCount);
    int loadFullTrackRecord(int64_t rowId, DbTrackRecordData *outData);
    int findOrderPosForRowId(int64_t targetRowId, unsigned int rangeStart,
                             unsigned int rangeEnd, unsigned int *outOrderPos);
    int64_t resolveRowIdAtOrderPos(unsigned int orderPos, int64_t expectedRowId);

    MusicRowidResult m_mainRowids;
    dbutil::RowidResult m_auxRowids;
    DbConnectionProvider *m_dbProvider;
    uint8_t m_dbChangedFlag;
    uint8_t m_padChanged[3];
    MetadataRecordCache m_metaCache;
};

int isIdRejected(int64_t idValue, unsigned int kind);
void clearRejectedIdList();

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_MEDIA_LIST_H */
