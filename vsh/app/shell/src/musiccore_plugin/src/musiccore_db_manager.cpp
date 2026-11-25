/*
 * SceShell musiccore_plugin - MusicPlayer, DbConnection, MusicRegCache, and DbManager.
 * Original address range (3.65-CEX): 0x811DFD10 - 0x811E20C0
 */

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/string>

#include <common/shared_ptr.h>
#include <dbutil/connection.h>
#include <file/localfile.h>
#include <thread/thread.h>

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
void *sce_paf_memcpy(void *dst, const void *src, size_t size);
char *sce_paf_strncpy(char *dst, const char *src, size_t maxLen);
size_t sce_paf_strlen(const char *str);
int sceKernelGetSystemTime(SceKernelSysClock *clock);
int sceKernelDeleteSemaForUser(SceUID semaId);
int sceKernelDeleteThreadForUser(SceUID threadId);
int sceKernelSignalSemaForUser(SceUID semaId, int signalCount);
int sceIoMkdir(const char *dir, int mode);
int sceRegMgrGetKeysForUser(const char *category, void *entries, int count);
int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *outVal);
int sceRegMgrSetKeyIntForUser(const char *category, const char *name, int val);
int sceRegMgrIsBlueScreenForUser(int errCode);
int SceShellUtil_BB54D049(int level, int errCode, int extra);
void SceLsdb_A6D8772C(void *lsdbClient);
int SceLsdb_E90B81CD(void *lsdbClient);
int SceLsdb_7AF9008C(void *lsdbClient, const paf::string *titleId, void *outInfoPtr, int flag);
int SceLsdb_DEF5C3F9(void *lsdbClient);
void SceLsdb_E2101B03(void *lsdbClient);
}

static const int MUSICCORE_ERR_FAILED = -1879113726;      /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;   /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */
static const int MUSICCORE_ERR_DB_REMOVED = -1879113715;  /* 0x8FFF000D */
static const int MUSICCORE_ERR_QUEUE_TIMEOUT = -1879113714; /* 0x8FFF000E */
static const int MUSICCORE_ERR_QUEUE_RETRY = -1879113725; /* 0x8FFF0003 */
static const int DBUTIL_ERR_BUSY = -2146422267;           /* 0x80103205 */

static const char *kMusicDbPath = "ux0:/mms/music/AVContent.db";
static const char *kMusicRegCategory = "/CONFIG/MUSIC/MUSIC_APP";
static const char *kPowerRegCategory = "/CONFIG/POWER_SAVING";

namespace sce {
namespace MusicCore {

extern void *localAlloc(SceSize size);
extern void localFree(void *ptr);
extern void enterWorkerThread();
extern void leaveWorkerThread(int unused);
extern SceSize cmdQueueCalcSize(SceSize inCap, SceSize outCap, int count);
extern void *cmdQueueCreate(void *mem, const char *name, SceSize inCap, SceSize outCap,
                            int count, int *outErr, SceSize totalSize);
extern int cmdQueueEnqueue(void *queue, uint64_t cmdHdr, const void *inBuf, SceSize inSize,
                           int waitFlag);
extern int cmdQueueDequeue(void *queue, void **outNode, SceUInt *timeout);
extern int cmdQueueRelease(void *queue, void *node);

extern unsigned int basePlayerCalcExtraSize(int capacity);
extern void basePlayerConstruct(void *self, char kind, void *factory, int capacity,
                                int *outErr, unsigned int totalBytes, unsigned int headerBytes);
extern void basePlayerDestruct(void *self);

extern int reportDbFileError(int sqliteErr, const char *dbPath, unsigned int flags);
extern int checkTrackListValid(const void *req, int trackCount, int trackOpt);
extern void invalidateAllMediaCaches();
extern void *mediaListGetEntry(void *listObj, int index);
extern int64_t queryTrackIdByPath(const char *path);
extern void mediaListUpdateTrackId(void *media, int index, int64_t trackId);
extern int dbSeekSpecialTarget(void *media, int64_t playlistId, int targetIndex, int seekStep,
                               int *ioShuffleSeed, int *outFlag, int isSpecial,
                               void *outTrackInfo, unsigned char *outHasTrack);
extern int dbSeekNormalTarget(int seekCmd, void *media, unsigned char isNew, int seekStep,
                              int targetIndex, int *ioShuffleSeed, int *outStep, int unusedFlag,
                              char *outFlag, void *outTrackInfo, int *outExtra, int updateFlag);
extern int mediaListFillTrackInfo(void *trackInfo, int trackIdx, void *media);
extern void trackInfoReleaseResources(void *trackInfo, int flag);
extern void *createUriMediaList(void *slotMem);
extern void *createDbMediaList(void *slotMem, void *workBuf, void *dbConn, int shuffleSeed,
                               void *mediaQueue, unsigned int slotSize, unsigned int maxSize);
extern int setPowerSaveMode(int mode, int val, int flags);

/* =========================================================================
 * MusicPlayer (0x811DFD10 - 0x811DFDF0)
 * ========================================================================= */

class MusicPlayer {
public:
    static unsigned int calcTotalSize(int capacity);
    MusicPlayer(void *factory, int capacity, int *outErr, unsigned int totalBytes,
                unsigned int headerBytes);
    virtual ~MusicPlayer();
    virtual void onSessionConstructed() {}
    virtual bool canHandleCommand(int channel, uint32_t cmdPacked, int param, int extra, int flags);

private:
    unsigned char m_playerBaseMembers[472];
};

unsigned int MusicPlayer::calcTotalSize(int capacity)
{
    return basePlayerCalcExtraSize(capacity) + 472;
}

MusicPlayer::MusicPlayer(void *factory, int capacity, int *outErr,
                         unsigned int totalBytes, unsigned int headerBytes)
{
    basePlayerConstruct(this, 0, factory, capacity, outErr, totalBytes, headerBytes);
    if (totalBytes >= basePlayerCalcExtraSize(capacity) + 472) {
        int *baseWords = (int *)this;
        baseWords[15] = -1;
    } else {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
    }
}

MusicPlayer::~MusicPlayer()
{
    basePlayerDestruct(this);
}

bool MusicPlayer::canHandleCommand(int channel, uint32_t cmdPacked, int param, int extra, int flags)
{
    typedef int (*CheckFn)(MusicPlayer *, int, uint32_t, int, int, int);
    CheckFn *vtable = *(CheckFn **)this;
    if (vtable[17](this, channel, cmdPacked, param, extra, flags) == 1)
        return ((cmdPacked >> 24) & 0xFFu) == 1u;
    return false;
}

/* =========================================================================
 * DbConnection (0x811DFDF0 - 0x811E0204)
 * ========================================================================= */

static void onDbConnectionError(void *userArg, int sqliteErr)
{
    (void)userArg;
    if (sqliteErr != 5)
        reportDbFileError(sqliteErr, kMusicDbPath, 1u);
}

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

DbConnection::DbConnection()
    : m_padAlign(0),
      m_conn(),
      m_mutex("SceShellMusicCoreDbExclsv", 0),
      m_isOpen(0)
{
}

DbConnection::~DbConnection()
{
    m_mutex.Lock();
    if (m_isOpen == 1) {
        m_conn.Close();
        m_isOpen = 0;
    }
    m_mutex.Unlock();
}

int DbConnection::close()
{
    int ret = 0;
    m_mutex.Lock();
    if (m_isOpen == 1) {
        if (m_conn.Close() != 0)
            ret = MUSICCORE_ERR_FAILED;
        m_isOpen = 0;
    }
    m_mutex.Unlock();
    return ret;
}

int DbConnection::open(const char *dbPath, unsigned char *outOpenedNew)
{
    int ret = 0;
    m_mutex.Lock();

    if (m_isOpen == 1) {
        paf::string pathStr(dbPath != NULL ? dbPath : "");
        paf::string dbName = m_conn.GetDatabaseName(pathStr);
        if (!dbName.empty())
            *outOpenedNew = 0;
        else
            ret = MUSICCORE_ERR_FAILED;
    } else {
        paf::string pathStr(dbPath != NULL ? dbPath : "");
        int openErr = m_conn.Open(pathStr, 6);
        if (openErr != 0) {
            if (openErr != 1 && (openErr & 0xFFFFFF00) == (int)0x80103200) {
                int sqliteCode = openErr & 0xFF;
                if (sqliteCode != 5)
                    reportDbFileError(sqliteCode, kMusicDbPath, 1u);
                ret = MUSICCORE_ERR_FAILED;
            }
        } else {
            dbutil::Connection::ErrorHandler handler;
            handler.callback = onDbConnectionError;
            handler.arg = NULL;
            m_isOpen = 1;
            *outOpenedNew = 1;
            m_conn.SetErrorHandler(&handler);
        }
    }

    m_mutex.Unlock();
    return ret;
}

int DbConnection::beginTransaction(int writeMode, dbutil::Connection **outConn)
{
    int mode = (writeMode != 0) ? 1 : 0;
    m_mutex.Lock();

    if (m_isOpen != 1) {
        *outConn = NULL;
        m_mutex.Unlock();
        return MUSICCORE_ERR_FAILED;
    }

    unsigned int elapsedMs = 0;
    int dbErr = DBUTIL_ERR_BUSY;
    while (dbErr == DBUTIL_ERR_BUSY && elapsedMs < 10000u) {
        dbErr = m_conn.Begin(mode);
        if (dbErr != DBUTIL_ERR_BUSY)
            break;
        elapsedMs += 50u;
        m_mutex.Unlock();
        sceKernelDelayThread(50000);
        m_mutex.Lock();
    }

    if (dbErr != 0) {
        *outConn = NULL;
        m_mutex.Unlock();
        return MUSICCORE_ERR_FAILED;
    }

    *outConn = &m_conn;
    return 0;
}

void DbConnection::endTransaction(int mode)
{
    if (m_isOpen == 1) {
        unsigned int elapsedMs = 0;
        int dbErr = DBUTIL_ERR_BUSY;
        while (dbErr == DBUTIL_ERR_BUSY && elapsedMs < 10000u) {
            if (mode == 1) {
                dbErr = m_conn.Commit();
            } else if (mode == 2) {
                dbErr = m_conn.Rollback();
            } else {
                paf::string endSql("END;");
                dbErr = m_conn.Execute(endSql, 0, NULL, NULL, NULL, NULL);
            }
            elapsedMs += 50u;
            if (dbErr == DBUTIL_ERR_BUSY)
                sceKernelDelayThread(50000);
        }

        if (dbErr != 0) {
            paf::string endSql("END;");
            m_conn.Execute(endSql, 0, NULL, NULL, NULL, NULL);
        }
    }
    m_mutex.Unlock();
}

/* =========================================================================
 * MusicRegCache (0x811E0204 - 0x811E050A)
 * ========================================================================= */

struct RegKeyEntry {
    int size;
    const char *name;
    uint16_t type;
    uint16_t valueSize;
    int value;
};

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

static void reportRegistryError(int regErr)
{
    if (sceRegMgrIsBlueScreenForUser(regErr) == 1)
        SceShellUtil_BB54D049(4, regErr, 0);
    else
        SceShellUtil_BB54D049(5, regErr, 0);
}

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

MusicRegCache::MusicRegCache()
    : m_padAlign(0)
{
    for (int i = 0; i < 5; ++i) {
        m_slots[i].keyIndex = 255;
        m_slots[i].dirty = 0;
        m_slots[i].timestampUs = 0;
        m_slots[i].value = 0;
    }
}

MusicRegCache::~MusicRegCache()
{
}

int MusicRegCache::loadAllSettings(MusicRegSnapshot *outSnap)
{
    RegKeyEntry entries[5];
    for (int i = 0; i < 5; ++i) {
        entries[i].size = sizeof(RegKeyEntry);
        entries[i].type = 0;
        entries[i].valueSize = 4;
        entries[i].value = 0;
    }
    entries[0].name = "repeat_mode";
    entries[1].name = "shuffle_mode";
    entries[2].name = "audio_normalizer";
    entries[3].name = "audio_equalize_type";
    entries[4].name = "audio_balance";

    int err = sceRegMgrGetKeysForUser(kMusicRegCategory, entries, 5);
    if (err != 0) {
        reportRegistryError(err);
        return MUSICCORE_ERR_FAILED;
    }

    outSnap->repeatMode = entries[0].value;
    outSnap->shuffleMode = entries[1].value;
    outSnap->audioNormalizer = entries[2].value;
    outSnap->eqType = entries[3].value;
    outSnap->audioBalance = entries[4].value;
    return 0;
}

int MusicRegCache::scheduleWrite(int keyIndex, int value)
{
    if (keyIndex >= 5)
        return MUSICCORE_ERR_INVALID_ARG;

    SceKernelSysClock now;
    sceKernelGetSystemTime(&now);
    m_slots[keyIndex].timestampUs = now.quad;
    m_slots[keyIndex].keyIndex = keyIndex;
    m_slots[keyIndex].value = value;
    m_slots[keyIndex].dirty = 1;
    return 0;
}

int MusicRegCache::flushPendingWrites()
{
    static const char *const kKeyNames[5] = {
        "repeat_mode",
        "shuffle_mode",
        "audio_normalizer",
        "audio_equalize_type",
        "audio_balance"
    };

    for (unsigned int i = 0; i < 5; ++i) {
        if (m_slots[i].dirty != 1)
            continue;

        SceKernelSysClock now;
        sceKernelGetSystemTime(&now);
        if ((now.quad - m_slots[i].timestampUs) < 3000000ULL)
            continue;

        int err = sceRegMgrSetKeyIntForUser(kMusicRegCategory, kKeyNames[i], m_slots[i].value);
        if (err != 0) {
            reportRegistryError(err);
            return MUSICCORE_ERR_FAILED;
        }

        m_slots[i].dirty = 0;
        m_slots[i].keyIndex = 255;
        m_slots[i].timestampUs = 0;
        m_slots[i].value = 0;
    }
    return 0;
}

int MusicRegCache::getSuspendInterval(int *outInterval)
{
    int interval = 0;
    int err = sceRegMgrGetKeyIntForUser(kPowerRegCategory, "suspend_interval", &interval);
    if (err != 0) {
        reportRegistryError(err);
        return MUSICCORE_ERR_FAILED;
    }
    *outInterval = interval;
    return 0;
}

int MusicRegCache::setImposeAudioBalance(int index, int value)
{
    if (index != 0)
        return MUSICCORE_ERR_INVALID_ARG;

    int err = sceRegMgrSetKeyIntForUser(kMusicRegCategory, "impose_audio_balance", value);
    if (err != 0) {
        reportRegistryError(err);
        return MUSICCORE_ERR_FAILED;
    }
    return 0;
}

/* =========================================================================
 * DbManager (0x811E050A - 0x811E20A8)
 * ========================================================================= */

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
    void (*onResult)(const void *result, void *userData);
    void (*onNotify)(const void *notify, void *userData);
    void *userData;
};

struct DbCmdQueueNode {
    void *vtable;
    int padAlign;
    uint64_t cmdHdr;
    void *inBuf;
    unsigned int inSize;
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

struct DbOpenMediaReq {
    int mode;
    void *workBuf;
    int shuffleSeed;
    void *mediaQueue;
    int playMode;
    int padBody[37];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbCloseMediaReq {
    void *mediaHandle;
    int padBody[41];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbSeekMediaReq {
    int seekCmd;
    int padCmd;
    int64_t playlistId;
    int64_t trackItemId;
    int targetIndex;
    int seekStep;
    unsigned char isNewMedia;
    unsigned char padNewMedia[3];
    void *mediaHandle;
    int seekStepOut;
    int shuffleSeed;
    int padBody[30];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbQueryTracksReq {
    uint64_t trackIds[20];
    int trackCount;
    int trackOpt;
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbQueryIdReq {
    char *outBuf;
    size_t outSize;
    int padBody[40];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbSimpleValueReq {
    int value;
    unsigned char alsoSaveBalance;
    unsigned char padBody[163];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct DbSetEqReq {
    int eqType;
    int bandLow;
    int bandMid;
    int bandHigh;
    int padBody[38];
    uint32_t origParam;
    uint32_t origCmdHi;
};

struct LsdbAppEntry {
    int appCategory;
    int appType;
    int appStatus;
    paf::string titleId;
    paf::string appPath;
    int appAttr;
    paf::string iconPath;
    int iconAttr;
    paf::string liveAreaPath;
    int extraAttrs[4];

    LsdbAppEntry() : appCategory(0), appType(0), appStatus(0), appAttr(0), iconAttr(0)
    {
        extraAttrs[0] = 0;
        extraAttrs[1] = 0;
        extraAttrs[2] = 0;
        extraAttrs[3] = 0;
    }
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

private:
    MediaListSlot *findActiveSlotByIndex(int activeIdx);

    SceUID m_initSema;
    SceUID m_threadId;
    int m_initResult;
    void *m_cmdQueue;
    DbCallbacks m_cbs;
    MediaListSlot *m_slots;
    int m_activeSlotCount;
    unsigned int m_capacity;
    const char *m_dbPath;
    DbConnection m_dbConn;
    MusicRegCache m_regCache;
};

static uint64_t makeDbCmdHeader(uint16_t cmdCode, uint32_t origCmdHi, unsigned char isAsync)
{
    uint32_t hi = ((uint32_t)cmdCode << 16) | (origCmdHi & 0x7FFFu) |
                  ((uint32_t)(isAsync & 1u) << 15);
    return (uint64_t)hi << 32;
}

DbManager::DbManager(int capacity)
    : m_initSema(-1),
      m_threadId(-1),
      m_initResult(MUSICCORE_ERR_FAILED),
      m_cmdQueue(NULL),
      m_slots(NULL),
      m_activeSlotCount(0),
      m_capacity((unsigned int)capacity),
      m_dbPath(kMusicDbPath),
      m_dbConn(),
      m_regCache()
{
    m_cbs.onResult = NULL;
    m_cbs.onNotify = NULL;
    m_cbs.userData = NULL;

    SceUID sema = sceKernelCreateSema("SceShellMusicCoreDbMgrWait", 0, 0, 1, NULL);
    if (sema >= 0)
        m_initSema = sema;
}

DbManager::~DbManager()
{
    stop();
    if (m_initSema >= 0)
        sceKernelDeleteSemaForUser(m_initSema);
}

int DbManager::enqueueCmd(uint64_t cmdHdr, const void *inBuf, SceSize inSize)
{
    if (m_threadId == -1 || m_cmdQueue == NULL)
        return MUSICCORE_ERR_FAILED;
    if (inSize != 0 && inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    int ret = cmdQueueEnqueue(m_cmdQueue, cmdHdr, inBuf, inSize, 0);
    return (ret == 0) ? MUSICCORE_ERR_FAILED : ret;
}

int DbManager::stop()
{
    int ret = 0;
    if (m_threadId != -1) {
        unsigned char stopPayload[176];
        sce_paf_memset(stopPayload, 0, sizeof(stopPayload));
        uint64_t stopHdr = (uint64_t)0x81008000u << 32;
        ret = enqueueCmd(stopHdr, stopPayload, sizeof(stopPayload));

        int waitErr = sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
        if (waitErr < 0)
            ret = waitErr;
        else
            sceKernelDeleteThreadForUser(m_threadId);
        m_threadId = -1;
    }

    if (m_cmdQueue != NULL) {
        typedef void (*DtorFn)(void *);
        DtorFn *vtable = *(DtorFn **)m_cmdQueue;
        vtable[1](m_cmdQueue);
        localFree(m_cmdQueue);
        m_cmdQueue = NULL;
    }

    if (m_slots != NULL) {
        localFree(m_slots);
        m_slots = NULL;
    }

    m_cbs.onResult = NULL;
    m_cbs.onNotify = NULL;
    m_cbs.userData = NULL;
    return ret;
}

int DbManager::start(const DbCallbacks *cbs)
{
    if (cbs == NULL || cbs->onResult == NULL || cbs->onNotify == NULL || cbs->userData == NULL)
        return MUSICCORE_ERR_FAILED;

    m_cbs = *cbs;
    int err = 0;

    void *slotsMem = localAlloc(sizeof(MediaListSlot) * m_capacity);
    if (slotsMem != NULL) {
        m_slots = (MediaListSlot *)slotsMem;
        unsigned char *cursor = (unsigned char *)slotsMem;
        for (unsigned int i = 0; i < m_capacity; ++i) {
            MediaListSlot *slot = new (cursor) MediaListSlot();
            slot->inUse = 0;
            slot->payloadCap = 8992u;
            slot->payloadPtr = slot->payloadArena;
            cursor += slot->getSlotStride();
        }
    }

    SceSize queueBytes = cmdQueueCalcSize(176u, 0u, 16);
    void *queueMem = localAlloc(queueBytes);
    if (queueMem == NULL) {
        err = MUSICCORE_ERR_NO_MEMORY;
    } else {
        void *queue = cmdQueueCreate(queueMem, "SceShellMusicCoreDbThr", 176u, 0u, 16,
                                     &err, queueBytes);
        if (queue == NULL) {
            err = MUSICCORE_ERR_NO_MEMORY;
        } else if (err == 0) {
            m_cmdQueue = queue;
            SceUID tid = sceKernelCreateThread("SceShellMusicCoreDbManager",
                                               threadEntry, 0x10000101, 0x5000u, 0, 0, NULL);
            if (tid < 0) {
                err = tid;
            } else {
                DbManager *selfArg = this;
                int startErr = sceKernelStartThread(tid, sizeof(selfArg), &selfArg);
                if (startErr != 0) {
                    err = startErr;
                } else {
                    int waitErr = sceKernelWaitSema(m_initSema, 1, NULL);
                    if (waitErr < 0) {
                        err = waitErr;
                    } else {
                        err = m_initResult;
                        if (err == 0)
                            m_threadId = tid;
                    }
                }
            }
        }
    }

    if (err != 0 && m_slots != NULL) {
        localFree(m_slots);
        m_slots = NULL;
    }
    return err;
}

int DbManager::requestOpenMedia(int mode, void *workBuf, int shuffleSeed, uint32_t origParam,
                                uint32_t origCmdHi, unsigned char isAsync, int playMode,
                                void *mediaQueue)
{
    DbOpenMediaReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.mode = mode;
    req.workBuf = workBuf;
    req.shuffleSeed = shuffleSeed;
    req.mediaQueue = mediaQueue;
    req.playMode = playMode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2100u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestCloseMedia(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi,
                                 unsigned char isAsync)
{
    DbCloseMediaReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.mediaHandle = mediaHandle;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2101u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSeekMedia(void *mediaHandle, int seekCmd, int64_t playlistId,
                                int64_t trackItemId, int targetIndex, int seekStep,
                                unsigned char isNewMedia, int seekStepOut, int shuffleSeed,
                                uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbSeekMediaReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.seekCmd = seekCmd;
    req.playlistId = playlistId;
    req.trackItemId = trackItemId;
    req.targetIndex = targetIndex;
    req.seekStep = seekStep;
    req.isNewMedia = isNewMedia;
    req.mediaHandle = mediaHandle;
    req.seekStepOut = seekStepOut;
    req.shuffleSeed = shuffleSeed;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2102u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSyncDb(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbCloseMediaReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2103u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestRefreshDb(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbCloseMediaReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2104u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestQueryTracks(const uint64_t *trackIds, int trackCount, int trackOpt,
                                  uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbQueryTracksReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    for (int i = 0; i < trackCount && i < 20; ++i)
        req.trackIds[i] = trackIds[i];
    req.trackCount = trackCount;
    req.trackOpt = trackOpt;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2105u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestQueryDbId(char *outBuf, size_t outSize, uint32_t origParam,
                                uint32_t origCmdHi, unsigned char isAsync)
{
    DbQueryIdReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.outBuf = outBuf;
    req.outSize = outSize;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x2180u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetRepeatMode(int repeatMode, uint32_t origParam, uint32_t origCmdHi,
                                    unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.value = repeatMode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A1u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetShuffleMode(int shuffleMode, uint32_t origParam, uint32_t origCmdHi,
                                     unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.value = shuffleMode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A2u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetNormalizer(int normalizerMode, uint32_t origParam, uint32_t origCmdHi,
                                    unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.value = normalizerMode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A3u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetEqualizer(int eqType, int bandLow, int bandMid, int bandHigh,
                                   uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbSetEqReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.eqType = eqType;
    req.bandLow = bandLow;
    req.bandMid = bandMid;
    req.bandHigh = bandHigh;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A4u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetAudioBalance(int balance, unsigned char alsoSaveBalance,
                                      uint32_t origParam, uint32_t origCmdHi,
                                      unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.value = balance;
    req.alsoSaveBalance = alsoSaveBalance;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A5u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestSetPowerSave(int mode, uint32_t origParam, uint32_t origCmdHi,
                                   unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.value = mode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21B0u, origCmdHi, isAsync), &req, sizeof(req));
}

int DbManager::requestLoadSettings(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    DbSimpleValueReq req;
    sce_paf_memset(&req, 0, sizeof(req));
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeDbCmdHeader(0x21A0u, origCmdHi, isAsync), &req, sizeof(req));
}

MediaListSlot *DbManager::findActiveSlotByIndex(int activeIdx)
{
    unsigned char *cursor = (unsigned char *)m_slots;
    int seen = 0;
    for (unsigned int i = 0; i < m_capacity; ++i) {
        MediaListSlot *slot = (MediaListSlot *)cursor;
        if (slot->inUse == 1) {
            if (seen == activeIdx)
                return slot;
            ++seen;
        }
        cursor += slot->getSlotStride();
    }
    return NULL;
}

int DbManager::handleSetAudioBalance(const DbSimpleValueReq *req)
{
    int balance = req->value;
    if (req->alsoSaveBalance == 1)
        m_regCache.scheduleWrite(4, balance);
    return m_regCache.setImposeAudioBalance(0, balance);
}

int DbManager::handleQueryDbId(const DbQueryIdReq *req)
{
    if (req->outBuf == NULL || req->outSize == 0)
        return MUSICCORE_ERR_INVALID_ARG;

    char lsdbClient[4];
    SceLsdb_A6D8772C(lsdbClient);
    paf::common::SharedPtr<LsdbAppEntry> entry(new LsdbAppEntry());

    if (SceLsdb_E90B81CD(lsdbClient) != 0) {
        SceLsdb_E2101B03(lsdbClient);
        return MUSICCORE_ERR_FAILED;
    }

    paf::string titleId("NPXS10009");
    int queryErr = SceLsdb_7AF9008C(lsdbClient, &titleId, &entry, 1);
    if (queryErr != 0) {
        SceLsdb_DEF5C3F9(lsdbClient);
        SceLsdb_E2101B03(lsdbClient);
        return MUSICCORE_ERR_FAILED;
    }

    if (entry->appPath.size() + 1 > req->outSize) {
        SceLsdb_DEF5C3F9(lsdbClient);
        SceLsdb_E2101B03(lsdbClient);
        return MUSICCORE_ERR_FAILED;
    }

    sce_paf_strncpy(req->outBuf, entry->appPath.c_str(), req->outSize);
    req->outBuf[req->outSize - 1] = '\0';
    SceLsdb_DEF5C3F9(lsdbClient);
    SceLsdb_E2101B03(lsdbClient);
    return 0;
}

int DbManager::handleRefreshAllMedia(DbResultEvent *outEv)
{
    int ret = 0;
    unsigned char changed = 0;
    for (int idx = 0; ; ++idx) {
        MediaListSlot *slot = findActiveSlotByIndex(idx);
        if (slot == NULL)
            break;
        typedef int (*RefreshFn)(void *, unsigned char *, int);
        RefreshFn *vtable = *(RefreshFn **)slot->payloadPtr;
        ret = vtable[10](slot->payloadPtr, &changed, 1);
        if (ret == 0 && changed == 1)
            outEv->dbChanged = 1;
    }
    invalidateAllMediaCaches();
    return ret;
}

void DbManager::refreshMediaTrackIds(void *mediaHandle)
{
    if (mediaHandle == NULL)
        return;

    typedef void *(*GetListFn)(void *);
    GetListFn *vtable = *(GetListFn **)mediaHandle;
    void *listObj = vtable[5](mediaHandle);
    if (listObj == NULL)
        return;

    struct MediaListItem {
        int headerWord;
        int mode;
        int itemMeta[4];
        const char *path;
        int padAlign;
        uint64_t trackId;
    };

    MediaListItem *entry = (MediaListItem *)mediaListGetEntry(listObj, 0);
    if (entry == NULL || entry->mode != 1)
        return;

    dbutil::Connection *conn = NULL;
    if (m_dbConn.beginTransaction(0, &conn) != 0)
        return;

    int idx = 0;
    while (entry != NULL) {
        if (entry->trackId < 0xFFFFFFFFFFFFFFFEULL)
            break;
        if (entry->path != NULL) {
            int64_t resolvedId = queryTrackIdByPath(entry->path);
            mediaListUpdateTrackId(mediaHandle, idx, resolvedId);
        }
        ++idx;
        entry = (MediaListItem *)mediaListGetEntry(listObj, idx);
    }
    m_dbConn.endTransaction(0);
}

int DbManager::openDbAndRefresh(const char *dbPath, unsigned char *outOpenedNew)
{
    int ret = m_dbConn.open(dbPath, outOpenedNew);
    if (ret == 0) {
        for (int idx = 0; ; ++idx) {
            MediaListSlot *slot = findActiveSlotByIndex(idx);
            if (slot == NULL)
                break;
            refreshMediaTrackIds(slot->payloadPtr);
        }
    }
    return ret;
}

int DbManager::handleSeekMedia(const DbSeekMediaReq *req, DbResultEvent *outEv)
{
    void *media = req->mediaHandle;
    outEv->mediaHandle = media;
    outEv->savedPlaylistId = req->playlistId;
    outEv->savedTrackItemId = req->trackItemId;
    outEv->seekCmd = req->seekCmd;
    outEv->repeatMode = req->targetIndex;
    outEv->seekStepParam = req->seekStep;
    outEv->shuffleMode = req->shuffleSeed;
    outEv->isNewMedia = req->isNewMedia;
    outEv->seekStep = req->seekStepOut;
    outEv->dbStatus = 0;

    if (media == NULL)
        return 0;

    int shuffleSeed = req->shuffleSeed;
    unsigned char locked = 0;
    typedef void (*LockFn)(void *, unsigned char *, int);
    LockFn *mediaVtable = *(LockFn **)media;
    mediaVtable[18](media, &locked, 0);

    DbMediaTrackInfo trackInfo;
    sce_paf_memset(&trackInfo, 0, sizeof(trackInfo));
    trackInfo.contentKind = 2;
    trackInfo.playlistId = -1;
    trackInfo.trackId = -1;
    trackInfo.trackItemId = -1;
    trackInfo.codecFlags = -1;
    trackInfo.sourceType = -1;

    unsigned char hasTrack = 0;
    int ret = 0;
    int seekCmd = req->seekCmd;
    if (((unsigned int)seekCmd >> 16) == 0xFFFCu) {
        int isSpecial = ((seekCmd & 0xFFFF) != 0) ? 1 : 0;
        ret = dbSeekSpecialTarget(media, req->trackItemId, req->targetIndex, req->seekStep,
                                  &shuffleSeed, NULL, isSpecial, &trackInfo, &hasTrack);
        if (ret != 0) {
            outEv->dbStatus = ret;
            if (ret == MUSICCORE_ERR_DB_REMOVED)
                ret = 0;
            else
                hasTrack = 0;
        } else {
            hasTrack = 1;
        }
    } else if (seekCmd == -1) {
        hasTrack = 0;
        ret = 0;
    } else {
        char normalFlag = 0;
        int extraVal = 0;
        ret = dbSeekNormalTarget(seekCmd, media, req->isNewMedia, req->seekStep,
                                 req->targetIndex, &shuffleSeed, &outEv->seekStep, 0,
                                 &normalFlag, &trackInfo, &extraVal, 1);
        if (ret != 0) {
            outEv->dbStatus = ret;
            hasTrack = 0;
        } else {
            hasTrack = 1;
        }
    }

    outEv->shuffleMode = shuffleSeed;

    if (hasTrack == 1) {
        typedef void (*PopulateTitleFn)(void *, char *, int);
        PopulateTitleFn *vtable = *(PopulateTitleFn **)media;
        vtable[21](media, trackInfo.titlePath, 0);
    }
    if (hasTrack == 0) {
        typedef int (*GetIdxFn)(void *);
        GetIdxFn *vtable = *(GetIdxFn **)media;
        int idx = vtable[16](media);
        hasTrack = (mediaListFillTrackInfo(&trackInfo, idx, media) == 0) ? 1 : 0;
    }

    if (hasTrack == 1) {
        outEv->trackInfo = trackInfo;
        typedef int (*GetIdxFn)(void *);
        typedef int (*GetOrderFn)(void *, int);
        GetIdxFn *idxVtbl = *(GetIdxFn **)media;
        GetOrderFn *ordVtbl = *(GetOrderFn **)media;
        int curIdx = idxVtbl[16](media);
        outEv->trackIndex = ordVtbl[22](media, curIdx);
        trackInfoReleaseResources(&trackInfo, 0);
        outEv->trackInfo.mediaInfo = NULL;
    }

    if (locked == 1) {
        typedef void (*UnlockFn)(void *, int);
        UnlockFn *unlockVtbl = *(UnlockFn **)media;
        unlockVtbl[19](media, 0);
    }
    return ret;
}

int DbManager::handleCloseMedia(const DbCloseMediaReq *req, DbResultEvent *outEv)
{
    void *targetMedia = req->mediaHandle;
    if (targetMedia == NULL || m_capacity == 0)
        return 0;

    unsigned char *cursor = (unsigned char *)m_slots;
    for (unsigned int i = 0; i < m_capacity; ++i) {
        MediaListSlot *slot = (MediaListSlot *)cursor;
        if (slot->payloadPtr == targetMedia) {
            if (slot->inUse == 1) {
                typedef void *(*GetWorkBufFn)(void *);
                typedef void (*DestroyFn)(void *);
                GetWorkBufFn *getVtbl = *(GetWorkBufFn **)targetMedia;
                DestroyFn *dtorVtbl = *(DestroyFn **)targetMedia;
                outEv->workBuf = getVtbl[5](targetMedia);
                dtorVtbl[0](targetMedia);
                slot->inUse = 0;
                if (m_activeSlotCount != 0)
                    ++m_activeSlotCount;
            }
            return 0;
        }
        cursor += slot->getSlotStride();
    }
    return 0;
}

int DbManager::handleOpenMedia(const DbOpenMediaReq *req, DbResultEvent *outEv)
{
    outEv->shuffleMode = req->shuffleSeed;
    outEv->playMode = req->playMode;

    MediaListSlot *freeSlot = NULL;
    unsigned char *cursor = (unsigned char *)m_slots;
    for (unsigned int i = 0; i < m_capacity; ++i) {
        MediaListSlot *slot = (MediaListSlot *)cursor;
        if (slot->inUse == 0) {
            freeSlot = slot;
            break;
        }
        cursor += slot->getSlotStride();
    }

    if (freeSlot == NULL)
        return MUSICCORE_ERR_NO_MEMORY;
    if (req->workBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int ret = 0;
    void *media = NULL;

    if (req->mode == 1) {
        if (freeSlot->payloadPtr != NULL)
            media = createUriMediaList(freeSlot->payloadPtr);
        if (media != NULL)
            refreshMediaTrackIds(media);
    } else if (req->mode == 2) {
        if (req->mediaQueue == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        unsigned char openedNew = 0;
        ret = openDbAndRefresh(m_dbPath, &openedNew);
        if (ret == 0) {
            if (openedNew == 1)
                outEv->dbChanged = 1;
            if (freeSlot->payloadPtr != NULL) {
                media = createDbMediaList(freeSlot->payloadPtr, req->workBuf, &m_dbConn,
                                          req->shuffleSeed, req->mediaQueue,
                                          freeSlot->payloadCap, 8992u);
            }
        }
    } else if (req->mode == 3) {
        ret = MUSICCORE_ERR_NO_MEMORY;
    } else {
        ret = MUSICCORE_ERR_INVALID_ARG;
    }

    if (ret == 0) {
        if (media != NULL) {
            typedef int (*InitFn)(void *);
            InitFn *vtable = *(InitFn **)media;
            if (vtable[11](media) != 0) {
                typedef void (*DestroyFn)(void *);
                DestroyFn *dtorVtbl = *(DestroyFn **)media;
                dtorVtbl[0](media);
                media = NULL;
                ret = MUSICCORE_ERR_FAILED;
            } else {
                freeSlot->inUse = 1;
                ++m_activeSlotCount;
            }
        } else {
            ret = MUSICCORE_ERR_NO_MEMORY;
        }
    }

    outEv->mediaHandle = media;
    if (ret != 0)
        outEv->workBuf = req->workBuf;
    return ret;
}

int DbManager::dispatchCmd(uint16_t cmdCode, const void *inBuf, DbResultEvent *outEv)
{
    switch (cmdCode) {
    case 0x2100:
        return handleOpenMedia((const DbOpenMediaReq *)inBuf, outEv);
    case 0x2101:
        return handleCloseMedia((const DbCloseMediaReq *)inBuf, outEv);
    case 0x2102:
        return handleSeekMedia((const DbSeekMediaReq *)inBuf, outEv);
    case 0x2103: {
        unsigned char openedNew = 0;
        int ret = openDbAndRefresh(m_dbPath, &openedNew);
        if (ret == 0)
            outEv->dbChanged = 0;
        return ret;
    }
    case 0x2104:
        return handleRefreshAllMedia(outEv);
    case 0x2105: {
        const DbQueryTracksReq *req = (const DbQueryTracksReq *)inBuf;
        int ret = checkTrackListValid(req, req->trackCount, req->trackOpt);
        outEv->dbChanged = (ret == 0) ? 1 : 0;
        return ret;
    }
    case 0x2180:
        return handleQueryDbId((const DbQueryIdReq *)inBuf);
    case 0x21A0: {
        MusicRegSnapshot regSnap;
        sce_paf_memset(&regSnap, 0, sizeof(regSnap));
        regSnap.audioBalance = 0x8000;
        int ret = m_regCache.loadAllSettings(&regSnap);
        if (ret == 0) {
            outEv->repeatMode = regSnap.repeatMode;
            outEv->shuffleMode = regSnap.shuffleMode;
            outEv->audioNormalizer = regSnap.audioNormalizer;
            outEv->eqType = regSnap.eqType;
            outEv->audioBalance = regSnap.audioBalance;
        }
        return ret;
    }
    case 0x21A1:
        return m_regCache.scheduleWrite(0, ((const DbSimpleValueReq *)inBuf)->value);
    case 0x21A2:
        return m_regCache.scheduleWrite(1, ((const DbSimpleValueReq *)inBuf)->value);
    case 0x21A3:
        return m_regCache.scheduleWrite(2, ((const DbSimpleValueReq *)inBuf)->value);
    case 0x21A4:
        return m_regCache.scheduleWrite(3, ((const DbSimpleValueReq *)inBuf)->value);
    case 0x21A5:
        return handleSetAudioBalance((const DbSimpleValueReq *)inBuf);
    case 0x21B0:
        setPowerSaveMode(8, ((const DbSimpleValueReq *)inBuf)->value, 0);
        return 0;
    default:
        return 0;
    }
}

int DbManager::ensureDirectoryExists(const paf::string &path)
{
    int ret = 0;
    paf::string::size_type pos = path.rfind("/", path.size(), 1);
    if (pos != paf::string::npos) {
        paf::string parent = path.substr(0, pos);
        if (!paf::LocalFile::Exists(parent.c_str()))
            ret = ensureDirectoryExists(parent);
        int mkErr = sceIoMkdir(path.c_str(), 6);
        if (mkErr != SCE_ERROR_ERRNO_EEXIST && mkErr != 0)
            ret = MUSICCORE_ERR_FAILED;
    }
    return ret;
}

int DbManager::runThreadLoop()
{
    m_initResult = 0;
    int sigErr = sceKernelSignalSemaForUser(m_initSema, 1);
    if (sigErr < 0)
        return sigErr;

    enterWorkerThread();

    if (paf::LocalFile::Exists("ux0:")) {
        paf::string musicDir("ux0:/music/");
        ensureDirectoryExists(musicDir);
    }

    bool running = true;
    while (running) {
        void *nodePtr = NULL;
        SceUInt timeoutUs = 200000u;
        SceUInt *timeoutPtr = (m_activeSlotCount != 0) ? &timeoutUs : NULL;
        int deqErr = cmdQueueDequeue(m_cmdQueue, &nodePtr, timeoutPtr);
        if (deqErr != 0) {
            if (deqErr == MUSICCORE_ERR_QUEUE_RETRY)
                continue;
            if (deqErr == MUSICCORE_ERR_QUEUE_TIMEOUT) {
                m_regCache.flushPendingWrites();
                continue;
            }
            break;
        }

        if (nodePtr == NULL)
            continue;

        DbCmdQueueNode *node = (DbCmdQueueNode *)nodePtr;
        if (node->inSize > 176u) {
            cmdQueueRelease(m_cmdQueue, nodePtr);
            continue;
        }

        uint64_t cmdHdr = node->cmdHdr;
        unsigned char reqBuf[176];
        sce_paf_memset(reqBuf, 0, sizeof(reqBuf));
        sce_paf_memcpy(reqBuf, node->inBuf, sizeof(reqBuf));
        cmdQueueRelease(m_cmdQueue, nodePtr);

        if ((int64_t)cmdHdr < 0) {
            uint16_t stopCode = (uint16_t)(cmdHdr >> 48);
            if (stopCode == 0x8100u)
                running = false;
            continue;
        }

        DbResultEvent ev;
        sce_paf_memset(&ev, 0, sizeof(ev));
        ev.trackInfo.contentKind = 2;
        ev.trackInfo.playlistId = -1;
        ev.trackInfo.trackId = -1;
        ev.trackInfo.trackItemId = -1;
        ev.trackInfo.codecFlags = -1;
        ev.trackInfo.sourceType = -1;

        uint16_t cmdCode = (uint16_t)(cmdHdr >> 48);
        int err = dispatchCmd(cmdCode, reqBuf, &ev);
        const DbCloseMediaReq *tailView = (const DbCloseMediaReq *)reqBuf;
        ev.cmdHdr = cmdHdr;
        ev.errCode = err;
        ev.origParam = tailView->origParam;
        ev.origCmdHi = tailView->origCmdHi;
        m_cbs.onResult(&ev, m_cbs.userData);
    }

    m_dbConn.close();
    leaveWorkerThread(0);
    m_initResult = MUSICCORE_ERR_FAILED;
    return 0;
}

int DbManager::threadEntry(SceSize args, void *argp)
{
    (void)args;
    DbManager *self = *(DbManager **)argp;
    int ret = (self != NULL) ? self->runThreadLoop() : 0;
    sceKernelExitThread(1);
    return ret;
}

} /* namespace MusicCore */
} /* namespace sce */
