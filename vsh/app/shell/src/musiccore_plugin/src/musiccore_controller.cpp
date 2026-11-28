/*
 * SceShell musiccore_plugin - MusicCoreController (SceShellMusicCoreController).
 * Original address range (3.65-CEX): 0x811D555C - 0x811DC4A0
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <thread/thread.h>

#include "musiccore_audio_output.h"
#include "musiccore_player_base.h"
#include "musiccore_db_manager.h"

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
void *sce_paf_memcpy(void *dst, const void *src, size_t size);
size_t sce_paf_strlen(const char *str);
int sceAppMgrGetCurrentBgmState2(void *outState);
int SceShell_7FE4E732(int eventType, void *cb, void *userData);
int SceShell_D4C7D2B7(int eventType, void *cb, void *userData);
}

static const int MUSICCORE_ERR_FAILED = -1879113726;        /* 0x8FFF0002 */
static const int MUSICCORE_ERR_TIMEOUT = -1879113725;       /* 0x8FFF0003 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;     /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723;   /* 0x8FFF0005 */
static const int MUSICCORE_ERR_BUSY = -1879113721;          /* 0x8FFF0007 */
static const int MUSICCORE_ERR_NOT_FOUND = -1879113716;     /* 0x8FFF000C */
static const int MUSICCORE_ERR_QUEUE_TIMEOUT = -1879113714; /* 0x8FFF000E */

namespace sce {
namespace MusicCore {

class BaseObject {
public:
    virtual ~BaseObject() {}
};

class PlayerPool {
public:
    void *getPlayer(unsigned char index);
};

class ObjFactory {
public:
    int createWorkBuf(void **outBuf);
    int getPlayerPool(char playerKind, PlayerPool **outPool);
};

struct SystemStateReport {
    unsigned char activeChannel;
    unsigned char activeKind;
    char overallStatus;
    unsigned char padStatus;
    int bgmPriority;
    char dbState;
    unsigned char dbBusy;
    unsigned char musicChannelId;
    unsigned char padChannel;
    PlayerPriorityInfo musicInfo;
    PlayerPriorityInfo playlistInfo;
    PlayerPriorityInfo streamInfo;
};

struct AppMgrBgmState {
    int bgmState;
    int activePriority;
    int bgmPriority;
    int reserved[5];
};

struct AppFocusInfo {
    int focusKind;
    int state;
};

struct AudioInitParams {
    int grain;
    int sampleRate;
    int channels;
};

struct DecoderCallbacks {
    int (*onEvent)(const void *ev, void *ctrl, int eventParam, int16_t flags);
    int (*onMessage)(const int *msg, void *ctrl, SceUInt *timeout);
    void *userData;
};

struct ExclusivePlayer : public BasePlayer {
    SoundOutExclusiveParam exclusiveParam;
};

struct IdleTimerTracker {
    void (*callback)(int channel, int value, int mode);
    char lastStatus;
    unsigned char padStatus[3];
    uint64_t pausedSinceUs;
};

struct DecoderEventHeader {
    int headerWord;
    uint16_t flagsWord;
    uint16_t subCode;
};

struct InternalMsgHeader {
    uint32_t param;
    uint8_t channelId;
    uint8_t flagsByte;
    uint16_t cmdCode;
    uint32_t extraWord;
};

extern int makeChannelId(char type, char slot);
extern int getChannelType(char channelId);
extern int getChannelSlot(unsigned char channelId);

extern void initDecoderManager(void *mgr);
extern void stopDecoderManager(void *mgr);
extern void termDecoderManager(void *mgr);
extern int startDecoderManager(void *mgr, const DecoderCallbacks *cbs, void *audioOut);
extern void notifyDecoderAudioEvent(void *mgr, int eventType, int eventVal);

extern int queryAppFocusState(AppFocusInfo *outInfo, int priority);
extern int getGlobalTrackTag();

extern void *workBufAllocRegion(void *workBuf, size_t size);
extern void workBufSetPayload(void *workBuf, void *payload);
extern void *constructUriData(void *mem, int uriPtr, int listIndex, int trackIndex, int shuffleFlag, int contentKind,
                              int totalCap, int headerCap, int playMode, int extraMeta);
extern void *constructUriString(void *mem, char *str, int flags, int cap, int headerSize);

static uint64_t makeCmdHeader(uint32_t param, uint32_t cmdHi)
{
    return ((uint64_t)cmdHi << 32) | (uint64_t)param;
}

class Controller : public BaseObject {
public:
    static unsigned int calcQueueBytes();

    Controller(ObjFactory *factory, int *outErr, unsigned int totalBytes);
    virtual ~Controller();

    int sendCommand(uint64_t cmdHeader, const void *inBuf, unsigned int inSize,
                    SceUInt *timeout, void *outResult);
    int getContentInfo(unsigned char channelId, void *infoBuf);
    int getStatus(unsigned char channelId, PlayerStatusSnapshot *outStatus, void *outPos, void *outPosValid,
                  int *outMode, void *workBuf, unsigned char *outFlag, int queryFlags);
    int getAudioState(unsigned char channelId);

    void initStatusSnapshot(PlayerStatusSnapshot *snap);
    BasePlayer *findPlayerByChannel(unsigned char channelId, unsigned char requireOpen);
    BasePlayer *findPlayerByKindSlot(char kind, unsigned char slot, char requireOpen);
    BasePlayer *findFreePlayer(char kind);
    BasePlayer *findBusyPlayer(char kind);
    BasePlayer *selectHighestPriorityPlayer(int minPriority);

    char computeBgmStatus(unsigned char channelId, char *outReason, int *outPriority);
    bool hasSystemStateChanged(const SystemStateReport *report);
    void updateIdleTrackerStatus(char newStatus, uint64_t nowUs);
    void checkIdleTrackerTimeout(uint64_t nowUs);
    void notifySystemState(unsigned char force);

    int handleOpenChannel(CmdNode *node, void *outResult, unsigned char *flags);
    int handleCloseChannel(CmdNode *node, unsigned char *flags);
    int handleDbOpen(CmdNode *node, unsigned char *flags);
    int handleDbQuery(CmdNode *node);
    int handleSelectMusicChannel(CmdNode *node, unsigned char *flags);
    int handleDbNotify(CmdNode *node, unsigned char *flags);
    int handleAudioFocusEvent(CmdNode *node, unsigned char *flags);
    int handleControllerCmd(CmdNode *node, void *outResult, unsigned char *flags);

    bool isAnyPlayerLocked();
    int updateAudioFocus();
    int syncActivePlayer(unsigned char *flags, uint32_t param, uint32_t cmdHi, unsigned char syncFlag);
    int pushToStoredQueue(CmdNode *node);
    int prepareUriWorkBuf(CmdNode *node);
    bool shouldDeferToStoredQueue(CmdNode *node);
    int onIdleTick();
    int runMainLoop();

    int storeSnapshot(const PlayerStatusSnapshot *snap);
    int hasTrackIdChanged(const PlayerStatusSnapshot *snap);
    int hasSnapshotChanged(const PlayerStatusSnapshot *snap);
    int mapErrorCode(int err);

    static int threadEntry(SceSize args, void *argp);
    static int onDecoderEvent(const void *ev, void *ctrlPtr, int eventParam, int16_t flags);
    static int onDecoderMessage(const int *msg, void *ctrlPtr, SceUInt *timeout);
    static int onAudioFocusCb(void *ctrlPtr, int priority, char granted);
    static int onAudioErrorCb(void *ctrlPtr, int errType, int errVal);
    static int onDbEvent(const void *ev, void *ctrlPtr);
    static int onDbMessage(const void *msg, void *ctrlPtr);
    static int onPlayerStatus(const PlayerStatusSnapshot *snap, void *ctrlPtr, unsigned char force);
    static int onPlayerExtra(const unsigned char *data, void *ctrlPtr);
    static int onPlayerMetadata(int type, const void *meta, PlayerStatusSnapshot *snap, void *ctrlPtr);
    static int onPlayerCap(const unsigned char *data, void *ctrlPtr);
    static int onShellSuspendEvent(int eventType, int eventParam, int eventExtra, void *ctrlPtr);
    static int onShellResumeEvent(int eventType, int eventParam, int eventExtra, void *ctrlPtr);

private:
    PlayerStatusSnapshot *getSnapshotSlot(unsigned char channelId);

public:
    ObjFactory *m_factory;
    SceUID m_threadId;
    SceUID m_initSema;
    int m_initResult;
    int m_padInit;
    paf::thread::Mutex m_ctrlMutex;
    paf::thread::Mutex m_ackMutex;
    paf::thread::Cond m_ackCond;
    CmdAckWaiter m_ackWaiter;
    CmdQueue *m_mainCmdQueue;
    int (*m_statusCb)(PlayerStatusSnapshot snap);
    unsigned int m_statusTimerInterval;
    int (*m_extraCb)(unsigned int data);
    unsigned int m_extraTimerInterval;
    int (*m_metadataCb)(int type);
    int (*m_capCb)(unsigned int data);
    void (*m_systemStateCb)(SystemStateReport report);
    int m_padCallbacks;

    unsigned char m_decoderMgrArena[4752];
    unsigned char m_dbMgrArena[400];

    int m_activeSlotReserved[2];
    BasePlayer *m_activePlayer;
    unsigned char m_deactivatingActive;
    unsigned char m_padDeactivating[3];
    PlayerPool *m_playlistPool;
    unsigned char m_playlistChannelId;
    unsigned char m_padPlaylistCh[3];
    PlayerPool *m_musicPool;
    unsigned char m_musicChannelId;
    unsigned char m_padMusicCh[3];
    PlayerPool *m_streamPool;
    unsigned char m_streamChannelId;
    unsigned char m_padStreamCh[3];

    unsigned char m_audioOutArena[224];
    int m_grantedPriority;
    int m_padPriority;

    PlayerStatusSnapshot m_playlistSnapshot;
    PlayerStatusSnapshot m_musicSnapshots[8];
    PlayerStatusSnapshot m_streamSnapshots[2];

    SystemStateReport m_lastSystemReport;
    char m_dbState;
    unsigned char m_padDbState[3];
    int m_dbBusyCount;
    unsigned char m_dbSuspendPending;
    unsigned char m_padDbSuspend[3];
    CmdQueue *m_storedCmdQueue;
    unsigned char m_pendingChannelId;
    unsigned char m_drainStoredQueue;
    unsigned char m_padStoredQueue[6];
    IdleTimerTracker m_idleTracker;
    unsigned char m_shellSuspended;
    unsigned char m_padTail[7];
};

void Controller::initStatusSnapshot(PlayerStatusSnapshot *snap)
{
    snap->bgmReason = -1;
    snap->bgmStatus = -1;
    snap->channelId = 0xFF;
    snap->playerKind = 15;
    snap->bgmPriority = 0;
    snap->seqNum = 0;
    snap->flags = -1;
    snap->cmdParam = 0;
    snap->errCode = 0;
    snap->playState = 0;
    snap->contentType = 0;
    snap->trackId = -1;
    snap->trackItemId = -1;
    snap->playlistId = -1;
    snap->trackIndex = 0;
    snap->totalTracks = -1;
    snap->currentTrackNum = -1;
    snap->durationMs = 0;
    snap->sampleRate = -1;
    snap->bitRate = -1;
    snap->channels = -1;
    snap->codecType = -1;
    snap->sourceType = 0;
    snap->repeatMode = 0;
    snap->shuffleMode = 0;
    snap->volumeMode = 1;
    snap->audioBalance = 0x8000;
    snap->eqType = 0;
    snap->eqBandLow = 0;
    snap->eqBandMid = 0;
    snap->eqBandHigh = 0;
    snap->audioNormalizer = 0;
    snap->extraErr = 0;
}

unsigned int Controller::calcQueueBytes()
{
    unsigned int mainQ = CmdQueue::calcRequiredSize(2272, 8, 1);
    unsigned int storedQ = CmdQueue::calcRequiredSize(2272, 8, 16);
    return mainQ + storedQ;
}

static void initDefaultPriorityInfo(PlayerPriorityInfo *info)
{
    info->bgmStatus = -1;
    info->bgmReason = -1;
    info->activePlayback = 0;
    info->isPlayable = 1;
    info->isOpen = 0;
    info->isEnabled = 1;
    info->isDbAllowed = 1;
    info->isBusy = 0;
    info->priority = -1;
    info->subPriority = -1;
    info->wantsPlay = 0;
    info->hasMedia = 0;
    info->hasPending = 0;
}

Controller::Controller(ObjFactory *factory, int *outErr, unsigned int totalBytes)
    : m_factory(factory),
      m_threadId(-1),
      m_initSema(-1),
      m_initResult(MUSICCORE_ERR_FAILED),
      m_ctrlMutex("SceShellMusicCoreCtrlThr", 0),
      m_ackMutex("SceShellMusicCoreCtrlAckWait", 0),
      m_ackCond(m_ackMutex, "SceShellMusicCoreCtrlAckWait", 0),
      m_mainCmdQueue(NULL),
      m_statusCb(NULL),
      m_statusTimerInterval(100000),
      m_extraCb(NULL),
      m_extraTimerInterval(0),
      m_metadataCb(NULL),
      m_capCb(NULL),
      m_systemStateCb(NULL),
      m_activePlayer(NULL),
      m_deactivatingActive(0),
      m_playlistPool(NULL),
      m_playlistChannelId(0xFF),
      m_musicPool(NULL),
      m_musicChannelId(0xFF),
      m_streamPool(NULL),
      m_streamChannelId(0xFF),
      m_grantedPriority(-1),
      m_dbState(4),
      m_dbBusyCount(0),
      m_dbSuspendPending(0),
      m_storedCmdQueue(NULL),
      m_pendingChannelId(0xFF),
      m_drainStoredQueue(0),
      m_shellSuspended(0)
{
    m_ackWaiter.cond = &m_ackCond;
    m_ackWaiter.mutex = &m_ackMutex;
    m_ackWaiter.signalCount = 0;

    initDecoderManager(m_decoderMgrArena);
    new (m_dbMgrArena) DbManager(36);
    new (m_audioOutArena) AudioOutput();

    initDefaultPriorityInfo(&m_lastSystemReport.musicInfo);
    initDefaultPriorityInfo(&m_lastSystemReport.playlistInfo);
    initDefaultPriorityInfo(&m_lastSystemReport.streamInfo);
    m_idleTracker.callback = NULL;
    m_idleTracker.lastStatus = -1;
    m_idleTracker.pausedSinceUs = 0;

    *outErr = MUSICCORE_ERR_FAILED;
    if (m_factory == NULL) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    SceUID sema = sceKernelCreateSema("SceShellMusicCoreCtrlWait", 0, 0, 1, NULL);
    if (sema < 0)
        return;
    m_initSema = sema;

    unsigned int headerOffset = 7288;
    if (headerOffset > totalBytes) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    unsigned int mainQSize = CmdQueue::calcRequiredSize(2272, 8, 1);
    if (headerOffset + mainQSize > totalBytes) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    char *mainQMem = (char *)this + headerOffset;
    CmdQueue *mainQ = new (mainQMem) CmdQueue("SceShellMusicCoreCtrl", 2272, 8, 1, outErr, mainQSize);
    if (mainQ == NULL) {
        *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    if (*outErr != 0) {
        mainQ->~CmdQueue();
        return;
    }
    m_mainCmdQueue = mainQ;

    unsigned int storedQSize = CmdQueue::calcRequiredSize(2272, 8, 16);
    if (headerOffset + mainQSize + storedQSize > totalBytes) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    char *storedQMem = mainQMem + mainQSize;
    CmdQueue *storedQ = new (storedQMem) CmdQueue("SceShellMusicCoreCtrlStored", 2272, 8, 16, outErr, storedQSize);
    if (storedQ == NULL) {
        *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    if (*outErr != 0) {
        storedQ->~CmdQueue();
        return;
    }
    m_storedCmdQueue = storedQ;

    int ret = m_factory->getPlayerPool(0, &m_musicPool);
    *outErr = ret;
    if (ret != 0 || m_musicPool == NULL) {
        m_musicPool = NULL;
        if (ret == 0)
            *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    for (int i = 0; i < 8; ++i)
        initStatusSnapshot(&m_musicSnapshots[i]);

    ret = m_factory->getPlayerPool(1, &m_playlistPool);
    *outErr = ret;
    if (ret != 0 || m_playlistPool == NULL) {
        m_playlistPool = NULL;
        if (ret == 0)
            *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    initStatusSnapshot(&m_playlistSnapshot);

    ret = m_factory->getPlayerPool(2, &m_streamPool);
    *outErr = ret;
    if (ret != 0 || m_streamPool == NULL) {
        m_streamPool = NULL;
        if (ret == 0)
            *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    initStatusSnapshot(&m_streamSnapshots[0]);
    initStatusSnapshot(&m_streamSnapshots[1]);

    SceUID thid = sceKernelCreateThread(
        "SceShellMusicCoreController",
        threadEntry,
        0x100000E3,
        0x4000,
        0,
        0,
        NULL);
    if (thid < 0) {
        *outErr = thid;
        return;
    }

    Controller *selfPtr = this;
    int startRet = sceKernelStartThread(thid, sizeof(selfPtr), &selfPtr);
    if (startRet != 0) {
        *outErr = startRet;
        return;
    }

    if (*outErr == 0) {
        int waitRet = sceKernelWaitSema(m_initSema, 1, NULL);
        if (waitRet < 0) {
            *outErr = waitRet;
            return;
        }
        *outErr = m_initResult;
    }
    if (m_initResult == 0)
        m_threadId = thid;
}

int Controller::sendCommand(uint64_t cmdHeader, const void *inBuf, unsigned int inSize,
                            SceUInt *timeout, void *outResult)
{
    if (m_threadId == -1 || m_mainCmdQueue == NULL)
        return MUSICCORE_ERR_FAILED;
    if (inSize != 0 && inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    CmdNode *node = NULL;
    int ret = m_mainCmdQueue->allocNode(&node, timeout);
    if (ret == 0) {
        if (node == NULL) {
            ret = MUSICCORE_ERR_FAILED;
        } else if (inSize > node->inCap) {
            ret = MUSICCORE_ERR_NO_MEMORY;
        } else {
            node->param = (uint32_t)(cmdHeader & 0xFFFFFFFFu);
            uint32_t hi = (uint32_t)(cmdHeader >> 32);
            node->targetChannel = (uint8_t)(hi & 0xFFu);
            node->flagsByte = (uint8_t)((hi >> 8) & 0xFFu);
            node->cmdCode = (uint16_t)(hi >> 16);
            node->inSize = inSize;
            sce_paf_memset(node->inBuf, 0, node->inCap);
            sce_paf_memcpy(node->inBuf, inBuf, node->inSize);
            node->outSize = 8;
            sce_paf_memset(node->outBuf, 0, node->outCap);

            if (timeout != NULL)
                ret = m_mainCmdQueue->enqueue(node, NULL, timeout);
            else
                ret = m_mainCmdQueue->enqueue(node, &m_ackWaiter, NULL);

            if (ret == 0) {
                int *outWords = (int *)node->outBuf;
                ret = outWords[0];
                if (outResult != NULL) {
                    int *resWords = (int *)outResult;
                    resWords[0] = ret;
                    ((unsigned char *)outResult)[4] = ((unsigned char *)node->outBuf)[4];
                }
            }
        }
    }

    if (node != NULL && timeout == NULL)
        m_mainCmdQueue->freeNode(node, NULL);
    return ret;
}

Controller::~Controller()
{
    if (m_threadId != -1) {
        stopDecoderManager(m_decoderMgrArena);
        reinterpret_cast<AudioOutput *>(m_audioOutArena)->stopOutput();
        reinterpret_cast<DbManager *>(m_dbMgrArena)->stop();

        unsigned char exitPayload = 0;
        sendCommand(makeCmdHeader(0, 0x81008000u), &exitPayload, 1, NULL, NULL);
        if (sceKernelWaitThreadEnd(m_threadId, NULL, NULL) >= 0)
            sceKernelDeleteThread(m_threadId);
        m_threadId = -1;
    }

    if (m_mainCmdQueue != NULL) {
        m_mainCmdQueue->~CmdQueue();
        m_mainCmdQueue = NULL;
    }
    if (m_storedCmdQueue != NULL) {
        m_storedCmdQueue->~CmdQueue();
        m_storedCmdQueue = NULL;
    }

    m_grantedPriority = -1;
    m_capCb = NULL;
    m_systemStateCb = NULL;
    m_extraCb = NULL;
    m_extraTimerInterval = 0;
    m_statusCb = NULL;
    m_statusTimerInterval = 100000;

    initStatusSnapshot(&m_playlistSnapshot);
    for (int i = 0; i < 8; ++i)
        initStatusSnapshot(&m_musicSnapshots[i]);
    initStatusSnapshot(&m_streamSnapshots[0]);
    initStatusSnapshot(&m_streamSnapshots[1]);

    sceKernelDeleteSema(m_initSema);
    m_idleTracker.callback = NULL;
    m_idleTracker.lastStatus = -1;
    m_idleTracker.pausedSinceUs = 0;

    reinterpret_cast<AudioOutput *>(m_audioOutArena)->finalize();
    reinterpret_cast<AudioOutput *>(m_audioOutArena)->~AudioOutput();
    reinterpret_cast<DbManager *>(m_dbMgrArena)->~DbManager();
    termDecoderManager(m_decoderMgrArena);
}

BasePlayer *Controller::findPlayerByChannel(unsigned char channelId, unsigned char requireOpen)
{
    unsigned char effectiveId = channelId;
    if (channelId == 0xEF) {
        effectiveId = (m_activePlayer != NULL) ? m_activePlayer->m_channelId : 0xFF;
    }

    int kind = getChannelType((char)effectiveId);
    unsigned char slot = (unsigned char)getChannelSlot(effectiveId);

    PlayerPool *pool = NULL;
    if (kind == 0)
        pool = m_musicPool;
    else if (kind == 1)
        pool = m_playlistPool;
    else if (kind == 2)
        pool = m_streamPool;
    else
        return NULL;

    if (pool == NULL)
        return NULL;

    BasePlayer *player = (BasePlayer *)pool->getPlayer(slot);
    if (player != NULL && requireOpen == 1 && player->isOpen() == 0)
        return NULL;
    return player;
}

int Controller::getContentInfo(unsigned char channelId, void *infoBuf)
{
    BasePlayer *player = findPlayerByChannel(channelId, 1);
    if (player == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    return player->getContentInfo(static_cast<MetadataBuffers *>(infoBuf));
}

char Controller::computeBgmStatus(unsigned char channelId, char *outReason, int *outPriority)
{
    BasePlayer *musicPlayer = findPlayerByChannel(m_musicChannelId, 1);
    BasePlayer *playlistPlayer = findPlayerByChannel(m_playlistChannelId, 1);
    BasePlayer *streamPlayer = findPlayerByChannel(m_streamChannelId, 1);

    int kind = getChannelType((char)channelId);
    *outReason = 0;

    AppMgrBgmState bgmState;
    int bgmRet = sceAppMgrGetCurrentBgmState2(&bgmState);
    *outPriority = (bgmRet == 0) ? bgmState.activePriority : 0;

    BasePlayer *target = NULL;
    BasePlayer *others[2] = { NULL, NULL };

    if (kind == 2) {
        if (channelId != m_streamChannelId)
            return 0;
        target = streamPlayer;
        others[0] = playlistPlayer;
        others[1] = musicPlayer;
    } else if (kind == 1) {
        if (channelId != m_playlistChannelId)
            return 0;
        target = playlistPlayer;
        others[0] = streamPlayer;
        others[1] = musicPlayer;
    } else if (kind == 0) {
        if (channelId != m_musicChannelId)
            return 0;
        target = musicPlayer;
        others[0] = streamPlayer;
        others[1] = playlistPlayer;
    } else {
        return -1;
    }

    if (target == NULL)
        return -1;
    if (m_dbBusyCount != 0 && kind != 2)
        return 0;

    PlayerPriorityInfo targetInfo = target->getPriorityInfo();

    for (int i = 0; i < 2; ++i) {
        BasePlayer *other = others[i];
        if (other == NULL)
            continue;
        PlayerPriorityInfo otherInfo = other->getPriorityInfo();
        if (otherInfo.isOpen == 1 && otherInfo.hasMedia == 1 && otherInfo.wantsPlay == 1 &&
            targetInfo.priority <= otherInfo.priority) {
            if (targetInfo.priority < otherInfo.priority) {
                if (other->m_playerKind == 2)
                    *outReason = 1;
                return 0;
            }
            if (targetInfo.subPriority == -1 || otherInfo.subPriority == -1 ||
                targetInfo.subPriority == otherInfo.subPriority) {
                if (target->m_playerKind == 0)
                    return 0;
                if (target->m_playerKind == 1) {
                    if (other->m_playerKind == 2) {
                        *outReason = 1;
                        return 0;
                    }
                } else if (target->m_playerKind < 0 || target->m_playerKind > 2) {
                    return -1;
                }
            } else if (targetInfo.subPriority < otherInfo.subPriority) {
                return 0;
            }
        }
    }

    if (bgmRet != 0)
        return -1;

    int maxBgmPrio = (bgmState.activePriority > bgmState.bgmPriority)
                         ? bgmState.activePriority
                         : bgmState.bgmPriority;
    if (maxBgmPrio == -1)
        maxBgmPrio = 0xFFFF;

    if (targetInfo.priority < maxBgmPrio) {
        *outReason = 2;
        return 0;
    }

    AppFocusInfo focusInfo;
    if (queryAppFocusState(&focusInfo, targetInfo.priority) != 0)
        return -1;
    if (focusInfo.state == 2 && target->m_playerKind != 2)
        return 0;

    if (targetInfo.isOpen != 1)
        return 0;
    if (targetInfo.hasMedia != 1)
        return 1;
    if (targetInfo.wantsPlay != 1)
        return 2;
    return 3;
}

int Controller::getStatus(unsigned char channelId, PlayerStatusSnapshot *outStatus, void *outPos, void *outPosValid,
                          int *outMode, void *workBuf, unsigned char *outFlag, int queryFlags)
{
    m_ctrlMutex.Lock();
    BasePlayer *player = findPlayerByChannel(channelId, 1);
    if (player == NULL) {
        m_ctrlMutex.Unlock();
        return MUSICCORE_ERR_INVALID_ARG;
    }

    int ret = player->getStatus(outStatus, static_cast<int *>(outPos),
                                static_cast<unsigned char *>(outPosValid));
    if (ret != 0) {
        m_ctrlMutex.Unlock();
        return ret;
    }

    char reason = -1;
    int bgmPrio = 0;
    outStatus->bgmStatus = computeBgmStatus(outStatus->channelId, &reason, &bgmPrio);
    outStatus->bgmReason = reason;
    outStatus->bgmPriority = bgmPrio;
    outStatus->playableFlag = (unsigned char)player->isPlayable();

    if (queryFlags < 0 || queryFlags > 1) {
        *outFlag = 0;
        *outMode = 4;
        m_ctrlMutex.Unlock();
        return 0;
    }

    int extraRet = player->getExtraStatus(outMode, static_cast<char>(queryFlags),
                                          static_cast<char *>(workBuf), outFlag);
    if (extraRet != 0) {
        *outMode = 4;
        m_ctrlMutex.Unlock();
        return extraRet;
    }

    m_ctrlMutex.Unlock();
    return 0;
}

int Controller::getAudioState(unsigned char channelId)
{
    (void)channelId;
    return 0;
}

BasePlayer *Controller::findFreePlayer(char kind)
{
    PlayerPool *pool = NULL;
    int count = 0;
    if (kind == 0) {
        pool = m_musicPool;
        count = 8;
    } else if (kind == 1) {
        pool = m_playlistPool;
        count = 1;
    } else if (kind == 2) {
        pool = m_streamPool;
        count = 2;
    } else {
        return NULL;
    }

    if (pool == NULL)
        return NULL;
    for (int i = 0; i < count; ++i) {
        BasePlayer *p = (BasePlayer *)pool->getPlayer((unsigned char)i);
        if (p != NULL && p->isOpen() == 0)
            return p;
    }
    return NULL;
}

BasePlayer *Controller::findBusyPlayer(char kind)
{
    PlayerPool *pool = NULL;
    int count = 0;
    if (kind == 0) {
        pool = m_musicPool;
        count = 8;
    } else if (kind == 1) {
        pool = m_playlistPool;
        count = 1;
    } else if (kind == 2) {
        pool = m_streamPool;
        count = 2;
    } else {
        return NULL;
    }

    if (pool == NULL)
        return NULL;
    for (int i = 0; i < count; ++i) {
        BasePlayer *p = (BasePlayer *)pool->getPlayer((unsigned char)i);
        if (p != NULL && p->isBusy() == 1)
            return p;
    }
    return NULL;
}

int Controller::handleOpenChannel(CmdNode *node, void *outResult, unsigned char *flags)
{
    if (node->cmdCode != 0x810B || node->inBuf == NULL || node->outBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    char kind = (char)node->targetChannel;
    if (kind == 2 && findBusyPlayer(2) != NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    BasePlayer *player = findFreePlayer(kind);
    if (player == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    int ret = player->open(node->inBuf, flags);
    if (ret == 0) {
        unsigned char chId = player->m_channelId;
        ((unsigned char *)outResult)[4] = chId;
        if (player->m_playerKind == 1)
            m_playlistChannelId = chId;
        else if (player->m_playerKind == 2)
            m_streamChannelId = chId;
    }
    return ret;
}

int Controller::handleCloseChannel(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x810C || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    unsigned char chId = node->targetChannel;
    BasePlayer *player = findPlayerByChannel(chId, 1);
    int ret = 0;
    if (player != NULL) {
        ret = player->close(flags);
    } else {
        ret = (findPlayerByChannel(chId, 0) != NULL) ? 0 : MUSICCORE_ERR_INVALID_ARG;
    }

    if (ret == 0) {
        if (chId == m_playlistChannelId)
            m_playlistChannelId = 0xFF;
        if (chId == m_musicChannelId)
            m_musicChannelId = 0xFF;
        if (chId == m_streamChannelId)
            m_streamChannelId = 0xFF;
    }
    return ret;
}

static bool priorityInfoDiffers(const PlayerPriorityInfo &a, const PlayerPriorityInfo &b)
{
    return a.bgmStatus != b.bgmStatus ||
           a.bgmReason != b.bgmReason ||
           a.activePlayback != b.activePlayback ||
           a.isPlayable != b.isPlayable ||
           a.isOpen != b.isOpen ||
           a.isEnabled != b.isEnabled ||
           a.isDbAllowed != b.isDbAllowed ||
           a.isBusy != b.isBusy ||
           a.priority != b.priority ||
           a.subPriority != b.subPriority ||
           a.wantsPlay != b.wantsPlay ||
           a.hasMedia != b.hasMedia ||
           a.hasPending != b.hasPending;
}

bool Controller::hasSystemStateChanged(const SystemStateReport *report)
{
    if (m_lastSystemReport.activeKind != report->activeKind ||
        m_lastSystemReport.overallStatus != report->overallStatus ||
        m_lastSystemReport.bgmPriority != report->bgmPriority ||
        m_lastSystemReport.dbState != report->dbState) {
        return true;
    }
    if (priorityInfoDiffers(m_lastSystemReport.musicInfo, report->musicInfo) ||
        priorityInfoDiffers(m_lastSystemReport.playlistInfo, report->playlistInfo) ||
        priorityInfoDiffers(m_lastSystemReport.streamInfo, report->streamInfo)) {
        return true;
    }
    return false;
}

void Controller::updateIdleTrackerStatus(char newStatus, uint64_t nowUs)
{
    char prev = m_idleTracker.lastStatus;
    if (newStatus != -1 && prev == newStatus)
        return;

    if (prev == 3 && newStatus == 0) {
        m_idleTracker.pausedSinceUs = (nowUs != 0) ? nowUs : 1;
    } else if (prev >= 0 && prev <= 2 && newStatus == 3) {
        if (m_idleTracker.callback != NULL)
            m_idleTracker.callback(0, getGlobalTrackTag(), 0);
        m_idleTracker.pausedSinceUs = 0;
    }
    m_idleTracker.lastStatus = newStatus;
}

static void clearPriorityInfo(PlayerPriorityInfo *info)
{
    info->bgmStatus = 0;
    info->bgmReason = -1;
    info->activePlayback = 0;
    info->isPlayable = 0;
    info->isOpen = 0;
    info->isEnabled = 0;
    info->isDbAllowed = 0;
    info->isBusy = 0;
    info->priority = -1;
    info->subPriority = -1;
    info->wantsPlay = 0;
    info->hasMedia = 0;
    info->hasPending = 0;
}

void Controller::notifySystemState(unsigned char force)
{
    SystemStateReport report;
    sce_paf_memset(&report, 0, sizeof(report));
    initDefaultPriorityInfo(&report.musicInfo);
    initDefaultPriorityInfo(&report.playlistInfo);
    initDefaultPriorityInfo(&report.streamInfo);

    int bgmPriority = 0;
    if (m_activePlayer != NULL) {
        report.activeChannel = m_activePlayer->m_channelId;
        report.activeKind = (unsigned char)m_activePlayer->m_playerKind;
    } else {
        report.activeChannel = 0xFF;
        report.activeKind = 0x0F;
    }

    BasePlayer *streamPlayer = findPlayerByChannel(m_streamChannelId, 1);
    if (streamPlayer != NULL) {
        char reason = -1;
        report.streamInfo = streamPlayer->getPriorityInfo();
        report.streamInfo.bgmStatus = computeBgmStatus(m_streamChannelId, &reason, &bgmPriority);
        report.streamInfo.bgmReason = reason;
        report.streamInfo.isPlayable = (unsigned char)streamPlayer->isPlayable();
    } else {
        clearPriorityInfo(&report.streamInfo);
    }

    BasePlayer *playlistPlayer = findPlayerByChannel(m_playlistChannelId, 1);
    if (playlistPlayer != NULL) {
        char reason = -1;
        report.playlistInfo = playlistPlayer->getPriorityInfo();
        report.playlistInfo.bgmStatus = computeBgmStatus(m_playlistChannelId, &reason, &bgmPriority);
        report.playlistInfo.bgmReason = reason;
        report.playlistInfo.isPlayable = (unsigned char)playlistPlayer->isPlayable();

        AppFocusInfo focusInfo;
        if (queryAppFocusState(&focusInfo, 0) != 0 || focusInfo.state != 1)
            updateIdleTrackerStatus(report.playlistInfo.bgmStatus, sceKernelGetProcessTimeWide());
    } else {
        clearPriorityInfo(&report.playlistInfo);
    }

    BasePlayer *musicPlayer = findPlayerByChannel(m_musicChannelId, 1);
    if (musicPlayer != NULL) {
        char reason = -1;
        report.musicChannelId = m_musicChannelId;
        report.musicInfo = musicPlayer->getPriorityInfo();
        report.musicInfo.bgmStatus = computeBgmStatus(m_musicChannelId, &reason, &bgmPriority);
        report.musicInfo.bgmReason = reason;
        report.musicInfo.isPlayable = (unsigned char)musicPlayer->isPlayable();
    } else {
        report.musicChannelId = 0xFF;
        clearPriorityInfo(&report.musicInfo);
    }

    char stStream = report.streamInfo.bgmStatus;
    char stPlaylist = report.playlistInfo.bgmStatus;
    char stMusic = report.musicInfo.bgmStatus;
    report.overallStatus = 1;

    if (stStream == 3 || stPlaylist == 3 || (m_musicChannelId != 0xFF && stMusic == 3)) {
        report.overallStatus = 3;
    } else if (stStream == 0 && stPlaylist == 0 && (m_musicChannelId == 0xFF || stMusic == 0)) {
        report.overallStatus = 0;
    } else if (stStream == -1 && stPlaylist == -1 && (m_musicChannelId == 0xFF || stMusic == -1)) {
        report.overallStatus = -1;
    }

    report.bgmPriority = bgmPriority;
    report.dbState = m_dbState;
    report.dbBusy = (m_dbBusyCount != 0) ? 1 : 0;

    if (force == 1 || hasSystemStateChanged(&report)) {
        if (m_systemStateCb != NULL) {
            m_lastSystemReport = report;
            m_systemStateCb(report);
        }
    }
}

BasePlayer *Controller::findPlayerByKindSlot(char kind, unsigned char slot, char requireOpen)
{
    PlayerPool *pool = NULL;
    if (kind == 0)
        pool = m_musicPool;
    else if (kind == 1)
        pool = m_playlistPool;
    else if (kind == 2)
        pool = m_streamPool;
    else
        return NULL;

    if (pool == NULL)
        return NULL;
    BasePlayer *player = (BasePlayer *)pool->getPlayer(slot);
    if (player != NULL && requireOpen == 1 && player->isOpen() == 0)
        return NULL;
    return player;
}

int Controller::handleDbOpen(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x8107 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const unsigned char *payload = (const unsigned char *)node->inBuf;
    uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                     ((uint32_t)node->cmdCode << 16);

    if (m_dbState == 4) {
        m_dbState = 0;
        int ret = reinterpret_cast<DbManager *>(m_dbMgrArena)->requestOpenMedia(
            0, NULL, 0, node->param, cmdHi, 0, 0, NULL);
        if (ret != 0) {
            m_dbState = 4;
            return ret;
        }
        if (payload[4] == 1) {
            m_dbSuspendPending = 1;
            ++m_dbBusyCount;
            for (int i = 0; i < 8; ++i) {
                BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 1);
                if (p != NULL)
                    p->onPauseForDb(flags);
            }
            BasePlayer *pl = findPlayerByKindSlot(1, 0, 1);
            if (pl != NULL)
                pl->onPauseForDb(flags);
            if (m_activePlayer == NULL)
                m_dbState = 1;
        }
        return 0;
    }

    if (payload[4] == 1) {
        m_dbSuspendPending = 1;
        ++m_dbBusyCount;
    }
    return 0;
}

int Controller::handleDbQuery(CmdNode *node)
{
    if (node->cmdCode != 0x8109 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const int *payload = (const int *)node->inBuf;
    char prevState = m_dbState;
    if (prevState == 4 || prevState == 2)
        return MUSICCORE_ERR_BUSY;
    if (payload[1] == 0 || payload[2] == 0 || payload[3] == -1)
        return MUSICCORE_ERR_INVALID_ARG;

    m_dbState = 2;
    uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                     ((uint32_t)node->cmdCode << 16);
    int ret = reinterpret_cast<DbManager *>(m_dbMgrArena)->requestQueryTracks(
        reinterpret_cast<const uint64_t *>(payload[1]), payload[2], payload[3],
        node->param, cmdHi, 0);
    if (ret != 0)
        m_dbState = prevState;
    return ret;
}

int Controller::handleSelectMusicChannel(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x810A || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    BasePlayer *curMusic = findPlayerByChannel(m_musicChannelId, 1);
    unsigned char targetCh = node->targetChannel;

    if (targetCh == 0xFF) {
        if (m_musicChannelId != 0xFF) {
            int wasWanting = 0;
            if (curMusic != NULL) {
                wasWanting = curMusic->wantsAudio();
                curMusic->setAudioFocus(0);
            }
            m_musicChannelId = 0xFF;
            if (wasWanting != 0)
                *flags |= 4u;
        }
        return 0;
    }

    BasePlayer *nextMusic = findPlayerByChannel(targetCh, 1);
    if (nextMusic == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    PlayerPriorityInfo info = nextMusic->getPriorityInfo();
    if (info.isOpen != 1)
        return MUSICCORE_ERR_BUSY;

    if (targetCh != m_musicChannelId) {
        int wasWanting = 0;
        if (curMusic != NULL) {
            wasWanting = curMusic->wantsAudio();
            if (nextMusic->getPriority() == curMusic->getPriority())
                nextMusic->setAudioFocus(curMusic->getAudioFocus());
            curMusic->setAudioFocus(0);
        }
        m_musicChannelId = targetCh;
        if (nextMusic->wantsAudio() != wasWanting)
            *flags |= 4u;
    }
    return 0;
}

int Controller::handleDbNotify(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x8400 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    struct DbNotifyPayload {
        uint8_t header[6];
        uint16_t subCode;
        uint8_t body[24];
        uint8_t changedFlag;
    };
    const DbNotifyPayload *payload = (const DbNotifyPayload *)node->inBuf;
    uint16_t subCode = payload->subCode;

    if (subCode == 0x2103)
        return 0;
    if (subCode == 0x2104) {
        *flags |= 0xA0u;
        return 0;
    }
    if (subCode == 0x2105) {
        if (payload->changedFlag == 1)
            *flags |= 0x20u;
        *flags &= ~0x80u;
        return 0;
    }
    return MUSICCORE_ERR_INVALID_ARG;
}

BasePlayer *Controller::selectHighestPriorityPlayer(int minPriority)
{
    BasePlayer *playlistPlayer = findPlayerByChannel(m_playlistChannelId, 1);
    int playlistActive = 0, playlistPrio = -1, playlistSub = -1;
    if (playlistPlayer != NULL) {
        playlistActive = playlistPlayer->isActive();
        playlistPrio = playlistPlayer->getPriority();
        playlistSub = playlistPlayer->getSubPriority();
    }

    BasePlayer *musicPlayer = findPlayerByChannel(m_musicChannelId, 1);
    int musicActive = 0, musicPrio = -1, musicSub = -1;
    if (musicPlayer != NULL) {
        musicActive = musicPlayer->isActive();
        musicPrio = musicPlayer->getPriority();
        musicSub = musicPlayer->getSubPriority();
    }

    BasePlayer *streamPlayer = findPlayerByChannel(m_streamChannelId, 1);
    int streamActive = 0;
    if (streamPlayer != NULL) {
        streamActive = streamPlayer->isActive();
        (void)streamPlayer->getPriority();
    }

    BasePlayer *candidates[3] = { NULL, NULL, NULL };
    int count = 0;
    if (streamActive == 1)
        candidates[count++] = streamPlayer;

    bool playlistFirst = (playlistPrio > musicPrio) ||
                         (playlistPrio == musicPrio && playlistSub >= musicSub);
    if (playlistFirst) {
        if (playlistActive == 1)
            candidates[count++] = playlistPlayer;
        if (musicActive == 1)
            candidates[count++] = musicPlayer;
    } else {
        if (musicActive == 1)
            candidates[count++] = musicPlayer;
        if (playlistActive == 1)
            candidates[count++] = playlistPlayer;
    }

    if (minPriority == -1)
        return candidates[0];

    for (int i = 0; i < 3; ++i) {
        if (candidates[i] == NULL)
            break;
        if (candidates[i]->getPriority() >= minPriority)
            return candidates[i];
    }
    return NULL;
}

int Controller::handleAudioFocusEvent(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x8800 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    struct FocusEventPayload {
        void *vtable;
        int priority;
        unsigned char granted;
    };
    const FocusEventPayload *ev = (const FocusEventPayload *)node->inBuf;

    if (ev->granted != 1) {
        m_grantedPriority = -1;
        if (m_activePlayer != NULL) {
            int prevWant = m_activePlayer->wantsAudio();
            m_activePlayer->setAudioFocus(0);
            if (m_activePlayer->wantsAudio() != prevWant)
                *flags |= 4u;
        }
        if (ev->priority == -1)
            return 0;

        BasePlayer *cand = selectHighestPriorityPlayer(ev->priority);
        if (cand == NULL)
            return 0;
        if (m_activePlayer != cand) {
            int prevWant = cand->wantsAudio();
            cand->setAudioFocus(0);
            if (cand->wantsAudio() != prevWant)
                *flags |= 4u;
        }
        int prio = cand->getPriority();
        if (prio == 0x7FFFFFFF)
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setExclusiveBgmPriority(
                0x7FFFFFFF, &((ExclusivePlayer *)cand)->exclusiveParam, 0);
        else
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(prio, 0);
        return 0;
    }

    m_grantedPriority = ev->priority;
    BasePlayer *cand = selectHighestPriorityPlayer(ev->priority);
    if (cand != NULL) {
        int prio = cand->getPriority();
        if (prio == ev->priority) {
            if (m_activePlayer != NULL && m_activePlayer != cand) {
                int prevWant = m_activePlayer->wantsAudio();
                m_activePlayer->setAudioFocus(0);
                if (m_activePlayer->wantsAudio() != prevWant)
                    *flags |= 4u;
            }
            int prevWant = cand->wantsAudio();
            cand->setAudioFocus(1);
            if (cand->wantsAudio() != prevWant)
                *flags |= 4u;
            return 0;
        }
        if (prio == 0x7FFFFFFF)
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setExclusiveBgmPriority(
                0x7FFFFFFF, &((ExclusivePlayer *)cand)->exclusiveParam, 0);
        else
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(prio, 0);
        return 0;
    }

    if (m_shellSuspended == 1 && ev->priority == 128) {
        BasePlayer *pl = findPlayerByChannel(m_playlistChannelId, 1);
        if (pl != NULL) {
            int prevWant = pl->wantsAudio();
            pl->setAudioFocus(1);
            if (pl->wantsAudio() != prevWant)
                *flags |= 4u;
        }
        return 0;
    }

    BasePlayer *fallback = selectHighestPriorityPlayer(-1);
    if (fallback != NULL) {
        int prio = fallback->getPriority();
        if (prio == 0x7FFFFFFF)
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setExclusiveBgmPriority(
                0x7FFFFFFF, &((ExclusivePlayer *)fallback)->exclusiveParam, 0);
        else
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(prio, 0);
    } else {
        reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(-1, 0);
    }
    return 0;
}

bool Controller::isAnyPlayerLocked()
{
    BasePlayer *pl = findPlayerByKindSlot(1, 0, 1);
    if (pl != NULL && pl->isLocked() == 1)
        return true;

    for (int i = 0; i < 8; ++i) {
        BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 1);
        if (p != NULL && p->isLocked() == 1)
            return true;
    }
    for (int i = 0; i < 2; ++i) {
        BasePlayer *p = findPlayerByKindSlot(2, (unsigned char)i, 1);
        if (p != NULL && p->isLocked() == 1)
            return true;
    }
    return false;
}

int Controller::updateAudioFocus()
{
    BasePlayer *cand = selectHighestPriorityPlayer(-1);
    if (cand != NULL) {
        int prio = cand->getPriority();
        int effPrio = prio;
        if (m_shellSuspended == 1 && prio != 0x7FFFFFFF)
            effPrio = 128;

        if (m_grantedPriority != -1 && m_grantedPriority >= effPrio) {
            cand->setAudioFocus(1);
            if (m_activePlayer != NULL && m_activePlayer != cand)
                m_activePlayer->setAudioFocus(0);
        }

        if (cand->getPriority() == 0x7FFFFFFF)
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setExclusiveBgmPriority(
                effPrio, &((ExclusivePlayer *)cand)->exclusiveParam, 0);
        else
            reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(effPrio, 0);
    } else if (m_shellSuspended == 1) {
        reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(128, 0);
    } else {
        reinterpret_cast<AudioOutput *>(m_audioOutArena)->setBgmPriority(-1, 0);
    }
    return 0;
}

int Controller::syncActivePlayer(unsigned char *flags, uint32_t param, uint32_t cmdHi, unsigned char syncFlag)
{
    BasePlayer *playlistPlayer = findPlayerByChannel(m_playlistChannelId, 1);
    if (playlistPlayer != NULL)
        (void)playlistPlayer->wantsAudio();
    BasePlayer *musicPlayer = findPlayerByChannel(m_musicChannelId, 1);
    if (musicPlayer != NULL)
        (void)musicPlayer->wantsAudio();
    BasePlayer *streamPlayer = findPlayerByChannel(m_streamChannelId, 1);
    if (streamPlayer != NULL)
        (void)streamPlayer->wantsAudio();

    unsigned char activeId = (m_activePlayer != NULL) ? m_activePlayer->m_channelId : 0xFF;
    if (m_deactivatingActive == 1)
        return 0;

    BasePlayer *desired = NULL;
    BasePlayer *cStream = findPlayerByChannel(m_streamChannelId, 1);
    BasePlayer *cPlaylist = findPlayerByChannel(m_playlistChannelId, 1);
    BasePlayer *cMusic = findPlayerByChannel(m_musicChannelId, 1);

    if (cStream != NULL && cStream->wantsAudio() == 1)
        desired = cStream;
    else if (cPlaylist != NULL && cPlaylist->wantsAudio() == 1)
        desired = cPlaylist;
    else if (cMusic != NULL && cMusic->wantsAudio() == 1)
        desired = cMusic;

    unsigned char desiredId = 0xFF;
    if (desired != NULL) {
        desiredId = desired->m_channelId;
        if (activeId == desiredId || desiredId == 0xEF)
            return 0;
    }

    if (m_activePlayer != NULL) {
        int fadeMs = 0;
        if (m_activePlayer->m_playerKind == 0) {
            BasePlayer *top = selectHighestPriorityPlayer(-1);
            fadeMs = (top != NULL && top->wantsAudio() == 0) ? 0 : 400;
        }
        int ret = m_activePlayer->deactivateDecoder(fadeMs, param, cmdHi, syncFlag);
        if (ret == 0) {
            *flags &= ~0x10u;
            m_deactivatingActive = 1;
            m_pendingChannelId = m_activePlayer->m_channelId;
            m_drainStoredQueue = 0;
        }
        return ret;
    }

    BasePlayer *nextActive = findPlayerByChannel(desiredId, 1);
    m_activePlayer = nextActive;
    if (nextActive != NULL) {
        int fadeMs = (nextActive->m_playerKind == 0) ? 100 : 0;
        int ret = nextActive->activateDecoder(fadeMs,
                                              reinterpret_cast<DecoderSession *>(m_decoderMgrArena));
        if (ret == 0) {
            *flags &= ~0x10u;
            m_pendingChannelId = m_activePlayer->m_channelId;
            m_drainStoredQueue = 0;
        }
        return ret;
    }
    return 0;
}

int Controller::handleControllerCmd(CmdNode *node, void *outResult, unsigned char *flags)
{
    uint16_t cmd = node->cmdCode;
    if (cmd == 0x8400)
        return handleDbNotify(node, flags);
    if (cmd == 0x8800)
        return handleAudioFocusEvent(node, flags);
    if (cmd == 0x8801) {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        const int *args = (const int *)node->inBuf;
        notifyDecoderAudioEvent(m_decoderMgrArena, args[1], args[2]);
        return 0;
    }

    if (cmd < 0x8101 || cmd > 0x810D)
        return MUSICCORE_ERR_INVALID_ARG;

    switch (cmd) {
    case 0x8101: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetSysStateCbPayload {
            int headerWord;
            void (*cb)(SystemStateReport);
        };
        m_systemStateCb = ((const SetSysStateCbPayload *)node->inBuf)->cb;
        return 0;
    }
    case 0x8102:
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        notifySystemState(1);
        return 0;
    case 0x8103: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetStatusCbPayload {
            int headerWord;
            int (*cb)(PlayerStatusSnapshot);
        };
        m_statusCb = ((const SetStatusCbPayload *)node->inBuf)->cb;
        return 0;
    }
    case 0x8104: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetCapCbPayload {
            int headerWord;
            int (*cb)(unsigned int);
        };
        m_capCb = ((const SetCapCbPayload *)node->inBuf)->cb;
        return 0;
    }
    case 0x8105: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetExtraCbPayload {
            int headerWord;
            int (*cb)(unsigned int);
            unsigned int interval;
        };
        const SetExtraCbPayload *p = (const SetExtraCbPayload *)node->inBuf;
        m_extraCb = p->cb;
        m_extraTimerInterval = p->interval;
        return 0;
    }
    case 0x8106: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetMetaCbPayload {
            int headerWord;
            int (*cb)(int);
        };
        m_metadataCb = ((const SetMetaCbPayload *)node->inBuf)->cb;
        return 0;
    }
    case 0x8107:
        return handleDbOpen(node, flags);
    case 0x8108:
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        if (m_dbState == 2)
            return MUSICCORE_ERR_BUSY;
        if (m_dbState == 4)
            return 0;
        return reinterpret_cast<DbManager *>(m_dbMgrArena)->requestCloseMedia(NULL, 0, 0, 0);
    case 0x8109:
        return handleDbQuery(node);
    case 0x810A:
        return handleSelectMusicChannel(node, flags);
    case 0x810B:
        return handleOpenChannel(node, outResult, flags);
    case 0x810C:
        return handleCloseChannel(node, flags);
    case 0x810D: {
        if (node->inBuf == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        struct SetIdleCbPayload {
            int headerWord;
            void (*cb)(int, int, int);
        };
        m_idleTracker.callback = ((const SetIdleCbPayload *)node->inBuf)->cb;
        m_idleTracker.lastStatus = -1;
        m_idleTracker.pausedSinceUs = 0;
        return 0;
    }
    default:
        abort();
    }
}

int Controller::pushToStoredQueue(CmdNode *srcNode)
{
    if (m_threadId == -1 || m_storedCmdQueue == NULL)
        return MUSICCORE_ERR_FAILED;
    if (srcNode == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    CmdNode *dstNode = NULL;
    SceUInt zeroTimeout = 0;
    int ret = m_storedCmdQueue->allocNode(&dstNode, &zeroTimeout);
    if (ret != 0)
        return ret;
    if (dstNode == NULL)
        return MUSICCORE_ERR_FAILED;
    if (srcNode->inCap > dstNode->inCap)
        return MUSICCORE_ERR_NO_MEMORY;

    dstNode->param = srcNode->param;
    dstNode->targetChannel = srcNode->targetChannel;
    dstNode->flagsByte = srcNode->flagsByte;
    dstNode->cmdCode = srcNode->cmdCode;
    dstNode->inSize = srcNode->inSize;
    sce_paf_memset(dstNode->inBuf, 0, dstNode->inCap);
    sce_paf_memcpy(dstNode->inBuf, srcNode->inBuf, dstNode->inSize);
    dstNode->outSize = 8;
    sce_paf_memset(dstNode->outBuf, 0, dstNode->outCap);
    return m_storedCmdQueue->enqueue(dstNode, NULL, &zeroTimeout);
}

int Controller::prepareUriWorkBuf(CmdNode *node)
{
    if (node == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_factory == NULL)
        return MUSICCORE_ERR_FAILED;

    if (node->cmdCode == 0x3111) {
        int *payload = (int *)node->inBuf;
        if (payload == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        void *workBuf = NULL;
        int ret = m_factory->createWorkBuf(&workBuf);
        if (workBuf == NULL)
            return MUSICCORE_ERR_FAILED;
        void *region = workBufAllocRegion(workBuf, 2096);
        if (region == NULL)
            return MUSICCORE_ERR_NO_MEMORY;
        void *obj = constructUriData(region, payload[2], payload[3], payload[4],
                                     payload[6], payload[7], 2096, 2096, payload[9], payload[10]);
        if (obj == NULL)
            return MUSICCORE_ERR_FAILED;
        workBufSetPayload(workBuf, obj);
        payload[1] = (int)(uintptr_t)workBuf;

        BasePlayer *p = findPlayerByChannel(node->targetChannel, 1);
        if (p != NULL)
            p->m_pendingUriCount = (p->m_pendingUriCount == -1) ? 1 : (p->m_pendingUriCount + 1);
        return ret;
    }

    if (node->cmdCode == 0x3112) {
        struct SetUriStrPayload {
            int headerWord;
            void *workBuf;
            int uriReserved;
            char *uri;
            int flags;
        };
        SetUriStrPayload *payload = (SetUriStrPayload *)node->inBuf;
        if (payload == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
        void *workBuf = NULL;
        int ret = m_factory->createWorkBuf(&workBuf);
        if (workBuf == NULL)
            return MUSICCORE_ERR_FAILED;
        size_t allocSz = (sce_paf_strlen(payload->uri) & ~7u) + 88;
        void *region = workBufAllocRegion(workBuf, allocSz);
        if (region == NULL)
            return MUSICCORE_ERR_NO_MEMORY;
        void *obj = constructUriString(region, payload->uri, payload->flags, (int)allocSz, 80);
        if (obj == NULL)
            return MUSICCORE_ERR_FAILED;
        workBufSetPayload(workBuf, obj);
        payload->workBuf = workBuf;

        BasePlayer *p = findPlayerByChannel(node->targetChannel, 1);
        if (p != NULL)
            p->m_pendingUriCount = (p->m_pendingUriCount == -1) ? 1 : (p->m_pendingUriCount + 1);
        return ret;
    }

    return 0;
}

bool Controller::shouldDeferToStoredQueue(CmdNode *node)
{
    if (node == NULL || m_pendingChannelId == 0xFF)
        return false;

    if ((node->cmdCode & 0x0400) != 0)
        return node->targetChannel != m_pendingChannelId;
    if ((node->cmdCode & 0x0200) == 0 && (node->cmdCode & 0x0800) == 0 &&
        (node->cmdCode & 0x8000) == 0) {
        return node->targetChannel == m_pendingChannelId;
    }
    return false;
}

void Controller::checkIdleTrackerTimeout(uint64_t nowUs)
{
    if (m_idleTracker.pausedSinceUs == 0)
        return;
    if (m_idleTracker.lastStatus < 0 || m_idleTracker.lastStatus > 2)
        return;
    if (nowUs > m_idleTracker.pausedSinceUs &&
        (nowUs - m_idleTracker.pausedSinceUs) >= 1000000ULL) {
        if (m_idleTracker.callback != NULL)
            m_idleTracker.callback(0, getGlobalTrackTag(), 1);
        m_idleTracker.pausedSinceUs = 0;
    }
}

int Controller::onIdleTick()
{
    BasePlayer *active = findPlayerByChannel(0xEF, 1);
    BasePlayer *stream = findPlayerByChannel(m_streamChannelId, 1);
    BasePlayer *music = findPlayerByChannel(m_musicChannelId, 1);
    BasePlayer *playlist = findPlayerByChannel(m_playlistChannelId, 1);

    if (stream != NULL)
        stream->tickStatus();
    if (music != NULL)
        music->tickStatus();
    if (playlist != NULL) {
        playlist->tickStatus();
        checkIdleTrackerTimeout(sceKernelGetProcessTimeWide());
    }
    if (active != NULL) {
        int ret = active->flushTimeCallback();
        if (ret != 0)
            return ret;
        active->onIdleActive();
    }
    if (playlist != NULL)
        playlist->onIdlePlaylist();

    notifySystemState(0);
    return 0;
}

int Controller::runMainLoop()
{
    int grain = 2048;
    const int sampleRate = 44100;
    AudioInitParams audioParams;
    audioParams.sampleRate = sampleRate;
    audioParams.channels = 1;

    if ((int64_t)grain * 1000000LL / sampleRate > 60000LL) {
        grain = (int)(((int64_t)sampleRate * 60000LL / 1000000LL) & ~63LL);
        audioParams.grain = grain;
        if (grain > 1024)
            audioParams.grain = grain & ~1023;
    } else {
        audioParams.grain = grain & ~1023;
    }

    int initErr = reinterpret_cast<AudioOutput *>(m_audioOutArena)->initialize(
        reinterpret_cast<const AudioOutputInitParam *>(&audioParams), 0);
    if (initErr == 0)
        initErr = reinterpret_cast<AudioOutput *>(m_audioOutArena)->setCallbacks(
            reinterpret_cast<AudioOutput::BgmAdoptCallback>(onAudioFocusCb),
            reinterpret_cast<AudioOutput::FadeCompleteCallback>(onAudioErrorCb), this);
    if (initErr == 0) {
        DecoderCallbacks decCbs;
        decCbs.onEvent = onDecoderEvent;
        decCbs.onMessage = onDecoderMessage;
        decCbs.userData = this;
        initErr = startDecoderManager(m_decoderMgrArena, &decCbs, m_audioOutArena);
    }
    if (initErr == 0) {
        DbCallbacks dbCbs;
        dbCbs.onResult = onDbEvent;
        dbCbs.onNotify = onDbMessage;
        dbCbs.userData = this;
        initErr = reinterpret_cast<DbManager *>(m_dbMgrArena)->start(&dbCbs);
    }
    if (initErr == 0) {
        PlayerCallbacks plCbs;
        plCbs.onStatus = onPlayerStatus;
        plCbs.onExtra = onPlayerExtra;
        plCbs.onMetadata = onPlayerMetadata;
        plCbs.onCap = onPlayerCap;
        plCbs.userData = this;

        for (int i = 0; i < 8 && initErr == 0; ++i) {
            BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 0);
            if (p != NULL)
                initErr = p->attach((unsigned char)makeChannelId(0, (char)i),
                                    &plCbs, m_dbMgrArena, m_audioOutArena);
        }
        if (initErr == 0) {
            BasePlayer *p = findPlayerByKindSlot(1, 0, 0);
            if (p != NULL)
                initErr = p->attach((unsigned char)makeChannelId(1, 0),
                                    &plCbs, m_dbMgrArena, m_audioOutArena);
        }
        for (int i = 0; i < 2 && initErr == 0; ++i) {
            BasePlayer *p = findPlayerByKindSlot(2, (unsigned char)i, 0);
            if (p != NULL)
                initErr = p->attach((unsigned char)makeChannelId(2, (char)i),
                                    &plCbs, m_dbMgrArena, m_audioOutArena);
        }
    }
    if (initErr == 0) {
        SceShell_7FE4E732(2, (void *)onShellSuspendEvent, this);
        initErr = SceShell_7FE4E732(3, (void *)onShellResumeEvent, this);
    }

    if (initErr != 0) {
        SceShell_D4C7D2B7(2, (void *)onShellSuspendEvent, NULL);
        SceShell_D4C7D2B7(3, (void *)onShellResumeEvent, NULL);
        stopDecoderManager(m_decoderMgrArena);
        reinterpret_cast<AudioOutput *>(m_audioOutArena)->stopOutput();
        reinterpret_cast<DbManager *>(m_dbMgrArena)->stop();
        for (int i = 0; i < 8; ++i) {
            BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 0);
            if (p != NULL)
                p->detach();
        }
        BasePlayer *pl = findPlayerByKindSlot(1, 0, 0);
        if (pl != NULL)
            pl->detach();
        for (int i = 0; i < 2; ++i) {
            BasePlayer *p = findPlayerByKindSlot(2, (unsigned char)i, 0);
            if (p != NULL)
                p->detach();
        }
    }

    m_initResult = initErr;
    int sigRet = sceKernelSignalSema(m_initSema, 1);
    if (sigRet < 0)
        return sigRet;
    if (initErr != 0)
        return initErr;

    bool shouldExit = false;
    unsigned char ctrlFlags[2] = { 0, 0 };

    while (!shouldExit) {
        unsigned int timeoutVal = 0;
        if (m_extraCb != NULL && m_extraTimerInterval != 0)
            timeoutVal = m_extraTimerInterval;
        if (m_statusCb != NULL && m_statusTimerInterval != 0) {
            if (timeoutVal == 0 || timeoutVal > m_statusTimerInterval)
                timeoutVal = m_statusTimerInterval;
        }

        CmdNode *node = NULL;
        bool fromStoredQueue = false;

        if (m_drainStoredQueue == 1) {
            SceUInt storedTimeout = 1000;
            int dqRet = MUSICCORE_ERR_FAILED;
            if (m_storedCmdQueue != NULL) {
                dqRet = m_storedCmdQueue->dequeue(&node, &storedTimeout);
                if (dqRet == MUSICCORE_ERR_QUEUE_TIMEOUT)
                    dqRet = m_storedCmdQueue->peekAt(&node, 0);
            }
            if (dqRet == 0) {
                fromStoredQueue = (node != NULL);
            } else if (dqRet == MUSICCORE_ERR_NOT_FOUND) {
                m_drainStoredQueue = 0;
                m_pendingChannelId = 0xFF;
            }
        } else {
            SceUInt *pTimeout = (timeoutVal != 0) ? &timeoutVal : NULL;
            int dqRet = m_mainCmdQueue->dequeue(&node, pTimeout);
            if (dqRet != 0) {
                if (dqRet == MUSICCORE_ERR_QUEUE_TIMEOUT) {
                    onIdleTick();
                    continue;
                }
                break;
            }
        }

        if (node == NULL)
            continue;

        if (!fromStoredQueue) {
            bool defer = shouldDeferToStoredQueue(node);
            int prepRet = prepareUriWorkBuf(node);
            if (prepRet != 0) {
                notifySystemState(0);
                *(int *)node->outBuf = prepRet;
                m_mainCmdQueue->complete(node);
                continue;
            }
            if (defer) {
                int storeRet = pushToStoredQueue(node);
                notifySystemState(0);
                *(int *)node->outBuf = storeRet;
                m_mainCmdQueue->complete(node);
                continue;
            }
        }

        m_ctrlMutex.Lock();
        ctrlFlags[0] &= 0x03u;
        ctrlFlags[1] &= ~0x01u;

        uint32_t savedParam = node->param;
        uint32_t savedCmdHi = (uint32_t)node->targetChannel |
                              ((uint32_t)node->flagsByte << 8) |
                              ((uint32_t)node->cmdCode << 16);

        BasePlayer *targetPlayer = NULL;
        int cmdRet = 0;

        if ((node->cmdCode & 0x8000) != 0) {
            if (node->cmdCode == 0x8100)
                shouldExit = true;
            else
                cmdRet = handleControllerCmd(node, node->outBuf, ctrlFlags);
        } else {
            targetPlayer = findPlayerByChannel(node->targetChannel, 0);
            if (targetPlayer != NULL)
                cmdRet = targetPlayer->dispatchCmd(node, ctrlFlags);
            else
                cmdRet = MUSICCORE_ERR_FAILED;
        }

        if (node->cmdCode == 0x3200) {
            uint16_t subCode = ((const DecoderEventHeader *)node->inBuf)->subCode;
            if (subCode == 0x1205) {
                m_drainStoredQueue = 1;
                if (m_activePlayer != NULL && m_activePlayer->m_channelId == node->targetChannel) {
                    m_activePlayer = NULL;
                    m_deactivatingActive = 0;
                }
            } else if (subCode == 0x1204) {
                m_drainStoredQueue = 1;
            }
        }

        unsigned char syncFlag = ((node->flagsByte & 0x80) != 0) ? 1 : 0;
        syncActivePlayer(ctrlFlags, savedParam, savedCmdHi, syncFlag);
        updateAudioFocus();
        notifySystemState(0);

        *(int *)node->outBuf = cmdRet;
        if (fromStoredQueue)
            m_storedCmdQueue->complete(node);
        else
            m_mainCmdQueue->complete(node);

        if ((ctrlFlags[0] & 0x10) != 0 && targetPlayer != NULL) {
            if (cmdRet == 0)
                targetPlayer->notifyStatus(0, savedParam, savedCmdHi, 0, ctrlFlags);
            targetPlayer->flushTimeCallback();
        }

        if ((ctrlFlags[0] & 0x20) != 0) {
            int suspendVal = ((ctrlFlags[0] & 0x80) != 0) ? 1 : 0;
            for (int i = 0; i < 8; ++i) {
                BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 1);
                if (p != NULL)
                    p->setSuspended(static_cast<char>(suspendVal));
            }
            BasePlayer *pl = findPlayerByKindSlot(1, 0, 1);
            if (pl != NULL)
                pl->setSuspended(static_cast<char>(suspendVal));
            for (int i = 0; i < 2; ++i) {
                BasePlayer *p = findPlayerByKindSlot(2, (unsigned char)i, 1);
                if (p != NULL)
                    p->setSuspended(static_cast<char>(suspendVal));
            }
            if (!isAnyPlayerLocked())
                ctrlFlags[0] |= 0x40u;
        }

        if ((ctrlFlags[1] & 0x01) != 0 && m_dbSuspendPending == 1 && m_dbState == 0)
            m_dbState = 1;

        if ((ctrlFlags[0] & 0x40) != 0 && !isAnyPlayerLocked()) {
            if ((ctrlFlags[0] & 0x80) != 0) {
                if (m_dbSuspendPending == 1) {
                    if (m_dbBusyCount != 0)
                        --m_dbBusyCount;
                    if (m_dbBusyCount == 0) {
                        m_dbSuspendPending = 0;
                        m_dbState = 4;
                        for (int i = 0; i < 8; ++i) {
                            BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 1);
                            if (p != NULL)
                                p->onDbResume(ctrlFlags);
                        }
                        BasePlayer *pl = findPlayerByKindSlot(1, 0, 1);
                        if (pl != NULL)
                            pl->onDbResume(ctrlFlags);
                    }
                } else {
                    m_dbState = 4;
                }
            } else if (m_dbState == 2) {
                m_dbState = 3;
            }
        }

        m_ctrlMutex.Unlock();
        notifySystemState(0);
    }

    SceShell_D4C7D2B7(2, (void *)onShellSuspendEvent, NULL);
    SceShell_D4C7D2B7(3, (void *)onShellResumeEvent, NULL);
    for (int i = 0; i < 8; ++i) {
        BasePlayer *p = findPlayerByKindSlot(0, (unsigned char)i, 0);
        if (p != NULL)
            p->detach();
    }
    BasePlayer *pl = findPlayerByKindSlot(1, 0, 0);
    if (pl != NULL)
        pl->detach();
    for (int i = 0; i < 2; ++i) {
        BasePlayer *p = findPlayerByKindSlot(2, (unsigned char)i, 0);
        if (p != NULL)
            p->detach();
    }
    m_initResult = MUSICCORE_ERR_FAILED;
    return 0;
}

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

int Controller::threadEntry(SceSize args, void *argp)
{
    (void)args;
    if (argp != NULL) {
        Controller *ctrl = *(Controller **)argp;
        if (ctrl != NULL)
            ctrl->runMainLoop();
    }
    SceThreadmgrCoredumpTime_sceKernelExitThread(1);
    return 0;
}

int Controller::onDecoderEvent(const void *ev, void *ctrlPtr, int eventParam, int16_t flags)
{
    (void)eventParam;
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    unsigned char buf[184];
    sce_paf_memset(buf, 0, sizeof(buf));
    sce_paf_memcpy(buf, ev, 177);

    unsigned char chId = ((const unsigned char *)ev)[172];
    uint32_t hi = (0x3200u << 16) | (((uint32_t)flags | 0x8000u) & 0xFF00u) | chId;
    return ctrl->sendCommand(makeCmdHeader(0, hi), buf, sizeof(buf), NULL, NULL);
}

int Controller::onDecoderMessage(const int *msg, void *ctrlPtr, SceUInt *timeout)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    const InternalMsgHeader *hdr = (const InternalMsgHeader *)msg;
    unsigned char chId = (ctrl->m_activePlayer != NULL) ? ctrl->m_activePlayer->m_channelId : 0xFF;
    uint16_t cmdCode = hdr->cmdCode;
    uint32_t hi = ((uint32_t)cmdCode << 16) | 0x8000u | chId;

    unsigned char buf[176];
    sce_paf_memset(buf, 0, sizeof(buf));
    sce_paf_memcpy(buf, msg, 172);
    return ctrl->sendCommand(makeCmdHeader(hdr->param, hi), buf, sizeof(buf), timeout, NULL);
}

int Controller::onAudioFocusCb(void *ctrlPtr, int priority, char granted)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    struct FocusMsg {
        void *vtable;
        int priority;
        char granted;
    } msg;
    msg.vtable = NULL;
    msg.priority = priority;
    msg.granted = granted;

    Controller *ctrl = (Controller *)ctrlPtr;
    return ctrl->sendCommand(makeCmdHeader(0, 0x880080FFu), &msg, sizeof(msg), NULL, NULL);
}

int Controller::onAudioErrorCb(void *ctrlPtr, int errType, int errVal)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    struct ErrMsg {
        void *vtable;
        int errType;
        int errVal;
    } msg;
    msg.vtable = NULL;
    msg.errType = errType;
    msg.errVal = errVal;

    Controller *ctrl = (Controller *)ctrlPtr;
    return ctrl->sendCommand(makeCmdHeader(0, 0x880180FFu), &msg, sizeof(msg), NULL, NULL);
}

int Controller::onDbEvent(const void *ev, void *ctrlPtr)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    unsigned char chId = ((const unsigned char *)ev)[20];
    uint16_t cmdCode = (chId == 0xFF) ? 0x8400u : 0x3400u;
    uint32_t hi = ((uint32_t)cmdCode << 16) | 0x8000u | chId;

    unsigned char buf[2272];
    sce_paf_memset(buf, 0, sizeof(buf));
    sce_paf_memcpy(buf, ev, sizeof(buf));
    return ctrl->sendCommand(makeCmdHeader(0, hi), buf, sizeof(buf), NULL, NULL);
}

int Controller::onDbMessage(const void *msg, void *ctrlPtr)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    const InternalMsgHeader *hdr = (const InternalMsgHeader *)msg;
    const uint32_t *words = (const uint32_t *)msg;
    unsigned char chId = hdr->channelId;
    uint16_t cmdCode = hdr->cmdCode;
    uint32_t hi = ((uint32_t)cmdCode << 16) | 0x8000u | chId;

    uint32_t buf[4];
    buf[0] = words[0];
    buf[1] = words[1];
    buf[2] = words[2];
    buf[3] = 0;
    return ctrl->sendCommand(makeCmdHeader(buf[0], hi), buf, sizeof(buf), NULL, NULL);
}

PlayerStatusSnapshot *Controller::getSnapshotSlot(unsigned char channelId)
{
    int slot = getChannelSlot(channelId);
    int kind = getChannelType((char)channelId);
    if (kind == 0 && slot < 8)
        return &m_musicSnapshots[slot];
    if (kind == 1 && slot == 0)
        return &m_playlistSnapshot;
    if (kind == 2 && slot < 2)
        return &m_streamSnapshots[slot];
    return NULL;
}

int Controller::storeSnapshot(const PlayerStatusSnapshot *snap)
{
    PlayerStatusSnapshot *slot = getSnapshotSlot(snap->channelId);
    if (slot == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    *slot = *snap;
    slot->errCode = 0;
    slot->extraErr = 0;
    return 0;
}

int Controller::hasTrackIdChanged(const PlayerStatusSnapshot *snap)
{
    PlayerStatusSnapshot *slot = getSnapshotSlot(snap->channelId);
    if (slot != NULL && snap->trackId != slot->trackId)
        return 1;
    return 0;
}

int Controller::hasSnapshotChanged(const PlayerStatusSnapshot *snap)
{
    PlayerStatusSnapshot *slot = getSnapshotSlot(snap->channelId);
    if (slot == NULL)
        return 0;

    if (snap->bgmStatus != slot->bgmStatus ||
        snap->bgmReason != slot->bgmReason ||
        snap->bgmPriority != slot->bgmPriority ||
        snap->playableFlag != slot->playableFlag ||
        snap->seqNum != slot->seqNum ||
        snap->errCode != slot->errCode ||
        snap->playState != slot->playState ||
        snap->contentType != slot->contentType ||
        snap->trackId != slot->trackId ||
        snap->trackItemId != slot->trackItemId ||
        snap->playlistId != slot->playlistId ||
        snap->trackIndex != slot->trackIndex ||
        snap->totalTracks != slot->totalTracks ||
        snap->currentTrackNum != slot->currentTrackNum ||
        snap->durationMs != slot->durationMs ||
        snap->sampleRate != slot->sampleRate ||
        snap->bitRate != slot->bitRate ||
        snap->channels != slot->channels ||
        snap->codecType != slot->codecType ||
        snap->sourceType != slot->sourceType ||
        snap->repeatMode != slot->repeatMode ||
        snap->shuffleMode != slot->shuffleMode ||
        snap->volumeMode != slot->volumeMode ||
        snap->audioBalance != slot->audioBalance ||
        snap->eqType != slot->eqType ||
        snap->eqBandLow != slot->eqBandLow ||
        snap->eqBandMid != slot->eqBandMid ||
        snap->eqBandHigh != slot->eqBandHigh ||
        snap->audioNormalizer != slot->audioNormalizer ||
        snap->extraErr != slot->extraErr) {
        return 1;
    }
    return 0;
}

int Controller::mapErrorCode(int err)
{
    if (err == 0)
        return 0;
    switch ((unsigned int)err) {
    case 0x8FFF0006u:
    case 0x8FFF000Du:
        return (int)0x80100D08u;
    case 0x8FFF0008u:
    case 0x8FFF000Au:
        return (int)0x80100D02u;
    case 0x8FFF0009u:
        return (int)0x80100D03u;
    case 0x8FFF000Bu:
        return (int)0x80100D04u;
    case 0x8FFF000Fu:
        return (int)0x80100D07u;
    case 0x8FFF0010u:
        return (int)0x80100D06u;
    default:
        return (int)0x80100D00u;
    }
}

int Controller::onPlayerStatus(const PlayerStatusSnapshot *snapIn, void *ctrlPtr, unsigned char force)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    PlayerStatusSnapshot snap = *snapIn;

    char reason = -1;
    int bgmPrio = 0;
    snap.bgmStatus = ctrl->computeBgmStatus(snap.channelId, &reason, &bgmPrio);
    snap.bgmReason = reason;
    snap.bgmPriority = bgmPrio;
    snap.errCode = ctrl->mapErrorCode(snap.errCode);
    snap.extraErr = ctrl->mapErrorCode(snap.extraErr);

    BasePlayer *player = ctrl->findPlayerByChannel(snap.channelId, 1);
    snap.playableFlag = (player != NULL) ? (unsigned char)player->isPlayable() : 1;

    if (ctrl->m_statusCb == NULL) {
        if (player != NULL && player->m_pendingUriCount == 0)
            player->m_pendingUriCount = -1;
        return 0;
    }

    int changed = (force == 1) ? 1 : ctrl->hasSnapshotChanged(&snap);
    int trackChanged = ctrl->hasTrackIdChanged(&snap);
    int ret = ctrl->storeSnapshot(&snap);
    if (ret != 0)
        changed = 0;

    int savedExtraErr = snap.extraErr;
    snap.extraErr = 0;

    if (changed == 1) {
        bool sendSecond = true;
        if (snap.cmdParam != 0) {
            PlayerStatusSnapshot first = snap;
            if (first.errCode == 0 && savedExtraErr != 0)
                first.errCode = savedExtraErr;
            ret = ctrl->m_statusCb(first);
            if (snap.errCode != 0 || savedExtraErr != 0) {
                snap.cmdParam = 0;
            } else {
                sendSecond = false;
            }
        }
        if (sendSecond) {
            if (savedExtraErr != 0)
                snap.errCode = savedExtraErr;
            ret = ctrl->m_statusCb(snap);
        }
        if (trackChanged == 1 && player != NULL)
            ret = player->flushTimeCallback();
        if (player != NULL && player->m_pendingUriCount == 0)
            player->m_pendingUriCount = -1;
    }
    return ret;
}

int Controller::onPlayerExtra(const unsigned char *data, void *ctrlPtr)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    if (ctrl->m_extraCb != NULL) {
        unsigned int packed = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
        return ctrl->m_extraCb(packed);
    }
    return 0;
}

int Controller::onPlayerMetadata(int type, const void *meta, PlayerStatusSnapshot *snap, void *ctrlPtr)
{
    (void)meta;
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    char reason = -1;
    int bgmPrio = 0;
    snap->bgmStatus = ctrl->computeBgmStatus(snap->channelId, &reason, &bgmPrio);
    snap->bgmReason = reason;
    snap->bgmPriority = bgmPrio;

    BasePlayer *player = ctrl->findPlayerByChannel(snap->channelId, 1);
    if (player != NULL)
        snap->playableFlag = (unsigned char)player->isPlayable();

    if (snap->extraErr != 0) {
        snap->errCode = snap->extraErr;
        snap->extraErr = 0;
    }
    snap->errCode = ctrl->mapErrorCode(snap->errCode);

    if (ctrl->m_metadataCb != NULL)
        return ctrl->m_metadataCb(type);
    return 0;
}

int Controller::onPlayerCap(const unsigned char *data, void *ctrlPtr)
{
    if (ctrlPtr == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    Controller *ctrl = (Controller *)ctrlPtr;
    if (ctrl->m_capCb != NULL) {
        unsigned int packed = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
        return ctrl->m_capCb(packed);
    }
    return 0;
}

int Controller::onShellSuspendEvent(int eventType, int eventParam, int eventExtra, void *ctrlPtr)
{
    (void)eventParam;
    (void)eventExtra;
    if (eventType != 2 || ctrlPtr == NULL)
        return eventType;

    Controller *ctrl = (Controller *)ctrlPtr;
    BasePlayer *pl = ctrl->findPlayerByKindSlot(1, 0, 1);
    if (pl != NULL)
        pl->setSystemSuspended(1);

    ctrl->m_shellSuspended = 1;
    BasePlayer *active = ctrl->findPlayerByChannel(0xEF, 1);
    if (active == NULL || active->m_playerKind != 2)
        return reinterpret_cast<AudioOutput *>(ctrl->m_audioOutArena)->setBgmPriority(128, 0);
    return active->m_playerKind;
}

int Controller::onShellResumeEvent(int eventType, int eventParam, int eventExtra, void *ctrlPtr)
{
    (void)eventParam;
    (void)eventExtra;
    if (eventType != 3 || ctrlPtr == NULL)
        return eventType;

    Controller *ctrl = (Controller *)ctrlPtr;
    BasePlayer *pl = ctrl->findPlayerByKindSlot(1, 0, 1);
    if (pl != NULL)
        pl->setSystemSuspended(0);

    ctrl->m_shellSuspended = 0;
    BasePlayer *active = ctrl->findPlayerByChannel(0xEF, 1);
    if (active == NULL || (active->m_playerKind != 2 && active->m_playerKind != 1)) {
        BasePlayer *playlist = ctrl->findPlayerByChannel(ctrl->m_playlistChannelId, 1);
        if (playlist != NULL)
            playlist->setAudioFocus(0);
        return reinterpret_cast<AudioOutput *>(ctrl->m_audioOutArena)->setBgmPriority(-1, 0);
    }
    return active->m_playerKind;
}

int controllerSendCommand(void *ctrl, uint64_t cmdHeader, const void *inBuf,
                          unsigned int inSize, SceUInt *timeout, void *outResult)
{
    return static_cast<Controller *>(ctrl)->sendCommand(cmdHeader, inBuf, inSize, timeout, outResult);
}

int controllerGetContentInfo(void *ctrl, unsigned char channelId, void *infoBuf)
{
    return static_cast<Controller *>(ctrl)->getContentInfo(channelId, infoBuf);
}

int controllerGetStatus(void *ctrl, unsigned char channelId, void *statusBuf,
                        void *outPos, void *outPosValid, int *outMode,
                        void *workBuf, unsigned char *outFlag, int queryFlags)
{
    return static_cast<Controller *>(ctrl)->getStatus(
        channelId, static_cast<PlayerStatusSnapshot *>(statusBuf), outPos, outPosValid,
        outMode, workBuf, outFlag, queryFlags);
}

int controllerGetState(void *ctrl, unsigned char channelId, void *outState)
{
    int state = static_cast<Controller *>(ctrl)->getAudioState(channelId);
    if (outState != NULL)
        *static_cast<int *>(outState) = state;
    return 0;
}

unsigned int calcControllerSize()
{
    return sizeof(Controller) + Controller::calcQueueBytes();
}

BaseObject *constructController(void *mem, void *factory, int *outErr, unsigned int slotSize)
{
    return new (mem) Controller(static_cast<ObjFactory *>(factory), outErr, slotSize);
}

class WorkBufferObject : public BaseObject {
public:
    WorkBufferObject(unsigned int bufSize) : m_bufSize(bufSize), m_payload(NULL) {}
    virtual ~WorkBufferObject() {}
    unsigned int m_bufSize;
    void *m_payload;
};

BaseObject *constructWorkBuffer(void *mem, unsigned int bufSize, unsigned int slotSize)
{
    (void)slotSize;
    return new (mem) WorkBufferObject(bufSize);
}

void *workBufAllocRegion(void *workBuf, size_t size)
{
    (void)size;
    if (workBuf == NULL)
        return NULL;
    return static_cast<char *>(workBuf) + sizeof(WorkBufferObject);
}

void workBufSetPayload(void *workBuf, void *payload)
{
    if (workBuf != NULL)
        static_cast<WorkBufferObject *>(workBuf)->m_payload = payload;
}

void *workBufGetRegion(void *workBuf, int index)
{
    (void)index;
    if (workBuf == NULL)
        return NULL;
    WorkBufferObject *obj = static_cast<WorkBufferObject *>(workBuf);
    return (obj->m_payload != NULL) ? obj->m_payload
                                    : (static_cast<char *>(workBuf) + sizeof(WorkBufferObject));
}

void *constructUriData(void *mem, int uriPtr, int listIndex, int trackIndex, int shuffleFlag,
                       int contentKind, int repeatFlag, int modeFlag, int extraFlag, int cap)
{
    (void)uriPtr; (void)listIndex; (void)trackIndex; (void)shuffleFlag;
    (void)contentKind; (void)repeatFlag; (void)modeFlag; (void)extraFlag; (void)cap;
    return mem;
}

void *constructUriString(void *mem, char *str, int flags, int cap, int headerSize)
{
    (void)flags; (void)headerSize;
    if (mem != NULL && str != NULL && cap > 0)
        sce_paf_memcpy(mem, str, static_cast<size_t>(cap));
    return mem;
}

int queryAppFocusState(AppFocusInfo *outInfo, int priority)
{
    (void)priority;
    if (outInfo != NULL)
        sce_paf_memset(outInfo, 0, sizeof(*outInfo));
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
