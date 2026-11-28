/*
 * SceShell musiccore_plugin - shared DbConnection, MusicRegCache, and DbManager declarations.
 */

#ifndef SHELL_MUSICCORE_DB_MANAGER_H
#define SHELL_MUSICCORE_DB_MANAGER_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <dbutil/connection.h>
#include <thread/thread.h>

#include "musiccore_heap.h"
#include "musiccore_queue.h"

namespace sce {
namespace MusicCore {

struct RegCacheSlot {
    int keyIndex;
    unsigned char dirty;
    unsigned char padDirty[3];
    uint64_t timestampUs;
    int value;
    int padValue;
};

struct MusicRegSnapshot {
    int headerFields[19];
    int repeatMode;
    int shuffleMode;
    int audioModeFields[3];
    int audioBalance;
    int eqType;
    int eqBandFields[3];
    int audioNormalizer;
};

struct MediaListSlot {
    virtual ~MediaListSlot() {}
    virtual unsigned int getSlotStride() { return 9008u; }

    unsigned char inUse;
    unsigned char padInUse[3];
    unsigned int payloadCap;
    void *payloadPtr;
    unsigned char payloadArena[8992];
};

struct DbCallbacks {
    int (*onResult)(const void *result, void *userData);
    int (*onNotify)(const void *notify, void *userData);
    void *userData;
};

struct DbMediaTrackInfo {
    void *vtable;
    int codecSubType;
    int contentKind;
    int padAlign;
    int64_t playlistId;
    int64_t trackId;
    int64_t trackItemId;
    char titlePath[2048];
    int codecFlags;
    int sampleRate;
    int codecType;
    int bitRate;
    int channels;
    int durationMs;
    int playAttemptCount;
    int fileSizeHigh;
    int resumePositionMs;
    int playCount;
    int sourceType;
    int slotSerial;
    void *ownerList;
    void *mediaInfo;
    unsigned char isValid;
};

struct DbResultEvent {
    uint64_t cmdHdr;
    int errCode;
    int padErr;
    uint32_t origParam;
    uint32_t origCmdHi;
    void *mediaHandle;
    void *workBuf;
    unsigned char dbChanged;
    unsigned char padChanged[7];
    DbMediaTrackInfo trackInfo;
    int seekStep;
    int trackIndex;
    int64_t savedPlaylistId;
    int64_t savedTrackItemId;
    int seekCmd;
    int repeatMode;
    unsigned char isNewMedia;
    unsigned char padNewMedia[3];
    int seekStepParam;
    int shuffleMode;
    int playMode;
    int audioNormalizer;
    int eqType;
    int eqBands[3];
    int audioBalance;
    int dbStatus;
};

struct DbOpenMediaReq;
struct DbCloseMediaReq;
struct DbSeekMediaReq;
struct DbQueryIdReq;
struct DbSimpleValueReq;

class DbConnection {
public:
    DbConnection();
    virtual ~DbConnection();

    int close();
    int open(const char *dbPath, unsigned char *outOpenedNew);
    int beginTransaction(int writeMode, dbutil::Connection **outConn);
    void endTransaction(int mode);

private:
    int m_padAlign;
    dbutil::Connection m_conn;
    paf::thread::Mutex m_mutex;
    unsigned char m_isOpen;
    unsigned char m_padTail[7];
};

class MusicRegCache {
public:
    MusicRegCache();
    virtual ~MusicRegCache();
    virtual void initialize() {}

    int loadAllSettings(MusicRegSnapshot *outSnap);
    int scheduleWrite(int keyIndex, int value);
    int flushPendingWrites();
    int getSuspendInterval(int *outInterval);
    int setImposeAudioBalance(int index, int value);

public:
    int m_padAlign;
    RegCacheSlot m_slots[5];
};

class DbManager {
public:
    DbManager(int capacity);
    virtual ~DbManager();

    int start(const DbCallbacks *cbs);
    int stop();
    int enqueueCmd(uint64_t cmdHdr, const void *inBuf, SceSize inSize);

    int requestOpenMedia(int mode, void *workBuf, int shuffleSeed, uint32_t origParam,
                         uint32_t origCmdHi, unsigned char isAsync, int playMode, void *mediaQueue);
    int requestCloseMedia(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi,
                          unsigned char isAsync);
    int requestSeekMedia(void *mediaHandle, int seekCmd, int64_t playlistId, int64_t trackItemId,
                         int targetIndex, int seekStep, unsigned char isNewMedia,
                         int seekStepOut, int shuffleSeed, uint32_t origParam,
                         uint32_t origCmdHi, unsigned char isAsync);
    int requestSyncDb(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    int requestRefreshDb(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    int requestQueryTracks(const uint64_t *trackIds, int trackCount, int trackOpt,
                           uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    int requestQueryDbId(char *outBuf, size_t outSize, uint32_t origParam,
                         uint32_t origCmdHi, unsigned char isAsync);
    int requestSetRepeatMode(int repeatMode, uint32_t origParam, uint32_t origCmdHi,
                             unsigned char isAsync);
    int requestSetShuffleMode(int shuffleMode, uint32_t origParam, uint32_t origCmdHi,
                              unsigned char isAsync);
    int requestSetNormalizer(int normalizerMode, uint32_t origParam, uint32_t origCmdHi,
                             unsigned char isAsync);
    int requestSetEqualizer(int eqType, int bandLow, int bandMid, int bandHigh,
                            uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    int requestSetAudioBalance(int balance, unsigned char alsoSaveBalance, uint32_t origParam,
                               uint32_t origCmdHi, unsigned char isAsync);
    int requestSetPowerSave(int mode, uint32_t origParam, uint32_t origCmdHi,
                            unsigned char isAsync);
    int requestLoadSettings(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);

    int handleSetAudioBalance(const DbSimpleValueReq *req);
    int handleQueryDbId(const DbQueryIdReq *req);
    int handleRefreshAllMedia(DbResultEvent *outEv);
    void refreshMediaTrackIds(void *mediaHandle);
    int openDbAndRefresh(const char *dbPath, unsigned char *outOpenedNew);
    int handleSeekMedia(const DbSeekMediaReq *req, DbResultEvent *outEv);
    int handleCloseMedia(const DbCloseMediaReq *req, DbResultEvent *outEv);
    int handleOpenMedia(const DbOpenMediaReq *req, DbResultEvent *outEv);
    int dispatchCmd(uint16_t cmdCode, const void *inBuf, DbResultEvent *outEv);
    int ensureDirectoryExists(const paf::string &path);
    int runThreadLoop();
    static int threadEntry(SceSize args, void *argp);
    MusicRegCache &regCache() { return m_regCache; }

private:
    MediaListSlot *findActiveSlotByIndex(int activeIdx);

    SceUID m_initSema;
    SceUID m_threadId;
    int m_initResult;
    CmdQueue *m_cmdQueue;
    DbCallbacks m_cbs;
    MediaListSlot *m_slots;
    int m_activeSlotCount;
    unsigned int m_capacity;
    const char *m_dbPath;
    DbConnection m_dbConn;
    MusicRegCache m_regCache;
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* SHELL_MUSICCORE_DB_MANAGER_H */
