/*
 * SceShell musiccore_plugin - Controller and WorkBufferObject declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_CONTROLLER_H
#define VSH_APP_SHELL_MUSICCORE_CONTROLLER_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <thread/thread.h>

#include "musiccore_player_base.h"
#include "musiccore_queue.h"

namespace sce {
namespace MusicCore {

class BaseObject {
public:
    virtual ~BaseObject() {}
};

class PlayerPool;
class ObjFactory;

class WorkBufferObject : public BaseObject {
public:
    WorkBufferObject(unsigned int bufSize) : m_bufSize(bufSize), m_payload(NULL) {}
    virtual ~WorkBufferObject() {}
    void *allocRegion() { return reinterpret_cast<char *>(this) + sizeof(WorkBufferObject); }
    void setPayload(void *payload) { m_payload = payload; }
    void *getRegion()
    {
        return (m_payload != NULL) ? m_payload
                                   : (reinterpret_cast<char *>(this) + sizeof(WorkBufferObject));
    }
    unsigned int m_bufSize;
    void *m_payload;
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

struct IdleTimerTracker {
    void (*callback)(int channel, int value, int mode);
    char lastStatus;
    unsigned char padStatus[3];
    uint64_t pausedSinceUs;
};

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

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_CONTROLLER_H */
