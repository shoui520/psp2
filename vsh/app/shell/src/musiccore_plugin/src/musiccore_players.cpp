/*
 * SceShell musiccore_plugin - ExclusiveMusicPlayer, MusicAppPlayer,
 * MusicRowidResult (Fisher-Yates shuffle), and music0:/ <-> ux0:/music/ path
 * conversion helpers.
 * Original address range (3.65-CEX): 0x811F51FC - 0x811F60E3
 */

#include <dbutil/connection.h>
#include <dbutil/rowid_result.h>
#include <kernel.h>
#include <plugin.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>
#include <system/system.h>
#include <thread/thread.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
SceSize sce_paf_strlen(const char *str);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize len);
int sceKernelPowerTickForUser(int type);
}

namespace sce {
namespace MusicCore {

static const int POWER_TICK_DISABLE_AUTO_SUSPEND = 0x00000001;

static const int MC_ERR_INTERNAL = -1879113726;       /* 0x8FFF0002 */
static const int MC_ERR_NO_MEMORY = -1879113724;      /* 0x8FFF0004 */
static const int MC_ERR_INVALID_ARG = -1879113723;    /* 0x8FFF0005 */
static const int MC_ERR_INVALID_STATE = -1879113721;  /* 0x8FFF0007 */
static const int DBUTIL_ERR_INVALID_ARG = -2146422527;/* 0x80103101 */

static const int MUSIC_APP_PLAYER_PRIORITY = 128;
static const int EXCLUSIVE_PLAYER_PRIORITY = 0x7FFFFFFF;
static const uint16_t CMD_PLAY_MUSIC_APP = 0x3118u;   /* 12568 */
static const uint16_t CMD_CLEAR_PLAYLIST = 0x311Au;   /* 12570 */
static const uint16_t NOTIFY_PLAYBACK_STOP = 0x1201u; /* 4609 */

struct SoundOutExclusiveParam {
    int version;
    int configWords[5];
};

struct PlayerCommandPacket {
    uint32_t header;
    uint32_t sequenceId;
    int paramPrimary;
    int paramSecondary;
    void *payload;
};

struct ExclusiveOpenRequest {
    uint32_t headerWords[3];
    SoundOutExclusiveParam exclusiveParam;
};

struct MusicAppOpenRequest {
    uint32_t headerWords[2];
    int requestedPriority;
};

struct MusicAppPlayCommand {
    uint32_t headerWords[2];
    int dbParamPrimary;
    int dbParamSecondary;
    uint16_t reservedLow;
    uint16_t commandType;
    void *payload;
};

struct TrackNotifyPayload {
    uint32_t headerWord;
    uint16_t reservedLow;
    uint16_t notifyCode;
    uint32_t reservedWords[2];
    int eventKind;
    uint8_t midPadding[120];
    int playCountTrigger;
    uint8_t tailPadding[24];
    int dbSyncRequired;
};

struct MetadataQueryBuffer {
    char *titleOut;
    SceSize titleCapacity;
    char *artistOut;
    SceSize artistCapacity;
    char *albumOut;
    SceSize albumCapacity;
};

struct TrackMetadataBuffers {
    char *titlePtr;
    SceSize titleCapacity;
    char *artistPtr;
    SceSize artistCapacity;
    char *albumPtr;
    SceSize albumCapacity;
};

struct PowerManageInterface {
    uint8_t reservedSlots[44];
    void (*acquireAutoSuspendHold)();
    void (*releaseAutoSuspendHold)();
};

struct AutoSuspendSettingProvider {
    virtual ~AutoSuspendSettingProvider() {}
    virtual void initialize() = 0;
    virtual void finalize() = 0;
    virtual void reloadSettings() = 0;
    virtual int getAutoSuspendTimeoutSec(int *outSeconds) = 0;
};

struct DbManagerRuntime {
    uint8_t dbBaseStorage[280];
    AutoSuspendSettingProvider *settingProvider;
};

struct PlaybackDbCommandTarget {
    uint32_t reservedHeader;
    int handleId;
};

extern size_t getPlayerBasePoolSize(void *heap);
extern void playerBaseConstruct(void *self, int playerType, void *controller,
                                void *dbManager, int *outErr, unsigned int bufSize,
                                int poolOffset);
extern void playerBaseDestroy(void *self);
extern int playerBaseStopAndReset(void *self);
extern int playerBaseClearActiveList(void *self);
extern void playerBaseResetMetadataStrings(void *self, TrackMetadataBuffers *meta);
extern int playerBaseHandleSeek(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseHandleSkipNext(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseHandleSkipPrev(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseHandleSetRepeat(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseHandleSetShuffle(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseOnTrackEvent(void *self, const PlayerCommandPacket *cmd);
extern int playerBaseResumeOrStart(void *self);
extern int playerBasePause(void *self);
extern int playerBaseHandleNotify(void *self, const PlayerCommandPacket *cmd, int extra);

extern int dbManagerOpenSession(void *dbMgr);
extern int dbManagerReleaseList(void *dbMgr, void *mediaList, int paramPrimary,
                                int paramSecondary, int commitFlag);
extern int dbManagerNotifySeek(void *dbMgr, int handleId, int paramPrimary,
                               int paramSecondary, int commitFlag);
extern int dbManagerNotifyNext(void *dbMgr, int handleId, int paramPrimary,
                               int paramSecondary, int commitFlag);
extern int dbManagerNotifyShuffle(void *dbMgr, int handleId, int paramPrimary,
                                  int paramSecondary, int commitFlag);
extern int dbManagerNotifyRepeat(void *dbMgr);
extern int dbManagerNotifyPrev(void *dbMgr, int handleId, int commitFlag);
extern int dbManagerFlushRecord(void *dbMgr);

extern size_t orderListFactoryCalcSize(void *heap, int maxLists, int listSlots, int entrySize);
extern void *orderListFactoryConstruct(void *mem, void *heap, int maxLists, int listSlots,
                                       size_t allocSize, int entrySize);

/* PlayerBase layout (472 bytes, offsets matching BasePlayer in musiccore_player_base.cpp) */
class PlayerBaseView {
public:
    virtual ~PlayerBaseView() {}
    virtual void onConstructHook() = 0;
    virtual int attach(unsigned char channelId, const void *callbacks, void *dbMgr, void *audioOut) = 0;
    virtual int detach() = 0;
    virtual void setAudioFocus(int granted) = 0;
    virtual int getAudioFocus() = 0;
    virtual int getPriority() = 0;
    virtual int getSubPriority() = 0;
    virtual int open(const void *param, unsigned char *flags) = 0;
    virtual int close(unsigned char *flags) = 0;
    virtual int isOpen() = 0;
    virtual int isBusy() = 0;
    virtual void onDbSuspend(unsigned char *flags) = 0;
    virtual void onDbResume(unsigned char *flags) = 0;
    virtual void onPauseForDb(unsigned char *flags) = 0;
    virtual int getPrioritySummary() = 0;
    virtual int isPlaybackRunning() = 0;
    virtual int isActive() = 0;
    virtual void onPriorityHook() = 0;
    virtual int wantsAudio() = 0;
    virtual void onAudioHook() = 0;
    virtual int isLocked() = 0;
    virtual int getContentInfo(TrackMetadataBuffers *outMeta) = 0;
    virtual paf::system::Model *getModel() = 0;
    virtual void onIdlePlaylist() = 0;
    virtual int isAutoSuspendPending() = 0;

    uint32_t m_playerType;                  /* +4 */
    uint32_t m_callbackAndFactory[6];       /* +8..+31 */
    DbManagerRuntime *m_dbManager;          /* +32 */
    int m_pendingDbOps;                     /* +36 */
    int m_playbackState;                    /* +40 */
    void *m_audioOut;                       /* +44 */
    uint8_t m_isOpened;                     /* +48 */
    uint8_t m_playerStateFlags[7];          /* +49..+55 */
    int m_activePriority;                   /* +56 */
    int m_defaultPriority;                  /* +60 */
    int m_pendingUriCount;                  /* +64 */
    int m_padStateAlign;                    /* +68 */
    int m_currentListIndex;                 /* +72 */
    int m_currentListCount;                 /* +76 */
    int64_t m_currentRowId;                 /* +80 */
    int64_t m_currentMediaId;               /* +88 */
    int64_t m_currentListId;                /* +96 */
    int m_currentDurationMs;                /* +104 */
    int m_currentPositionMs;                /* +108 */
    int m_currentCodec;                     /* +112 */
    int m_currentSampleRate;                /* +116 */
    int m_currentChannels;                  /* +120 */
    int m_currentBitRate;                   /* +124 */
    uint32_t m_mediaStreamProps[4];         /* +128..+143 */
    void *m_activeMediaList;                /* +144 */
    uint32_t m_offlineListMeta[2];          /* +148..+155 */
    int m_repeatMode;                       /* +156 */
    int m_shuffleMode;                      /* +160 */
    int m_playDirection;                    /* +164 */
    uint32_t m_audioEffectSettings[7];      /* +168..+195 */
    int m_errorDomain;                      /* +196 */
    int m_errorDetail;                      /* +200 */
    int m_errorSubCode;                     /* +204 */
    uint32_t m_activeStateBlock[48];        /* +208..+399 */
    TrackMetadataBuffers m_metaBuffers;     /* +400..+423 */
    paf::thread::Mutex *m_metaMutex;        /* +424 */
    void *m_heap;                           /* +428 */
    void *m_orderListFactory;               /* +432 */
    uint32_t m_pendingSeekState[7];         /* +436..+463 */
    uint8_t m_systemSuspended;              /* +464 */
    uint8_t m_autoSuspendActive;            /* +465 */
    uint8_t m_padFlagTail[2];               /* +466..+467 */
    int m_completedPlayCount;               /* +468 */
};

class ExclusiveMusicPlayer : public PlayerBaseView {
public:
    static size_t calcRequiredSize(void *heap);

    ExclusiveMusicPlayer(void *controller, void *dbManager, int *outErr,
                         unsigned int bufSize, int poolOffset);
    virtual ~ExclusiveMusicPlayer();

    int openSession(const ExclusiveOpenRequest *req, uint8_t *outChangeFlags);

private:
    SoundOutExclusiveParam m_exclusiveParam; /* +472..+495 */
};

/* 0x811F51FC */
size_t ExclusiveMusicPlayer::calcRequiredSize(void *heap)
{
    return getPlayerBasePoolSize(heap) + 496u;
}

/* 0x811F5208 */
ExclusiveMusicPlayer::ExclusiveMusicPlayer(void *controller, void *dbManager, int *outErr,
                                           unsigned int bufSize, int poolOffset)
{
    playerBaseConstruct(this, 2, controller, dbManager, outErr, bufSize, poolOffset);
    m_exclusiveParam.version = 5;
    m_exclusiveParam.configWords[0] = 0;
    m_exclusiveParam.configWords[1] = 0;
    m_exclusiveParam.configWords[2] = 0;
    m_exclusiveParam.configWords[3] = 0;
    m_exclusiveParam.configWords[4] = 0;

    if (bufSize >= getPlayerBasePoolSize(dbManager) + 496u) {
        m_defaultPriority = EXCLUSIVE_PLAYER_PRIORITY;
        m_exclusiveParam.version = 5;
        m_exclusiveParam.configWords[0] = 0;
        m_exclusiveParam.configWords[1] = 0;
        m_exclusiveParam.configWords[2] = 0;
        m_exclusiveParam.configWords[3] = 0;
        m_exclusiveParam.configWords[4] = 0;
    } else {
        *outErr = MC_ERR_NO_MEMORY;
    }
}

/* 0x811F5296 / 0x811F52B2 */
ExclusiveMusicPlayer::~ExclusiveMusicPlayer()
{
    playerBaseDestroy(this);
}

/* 0x811F52D2 */
int ExclusiveMusicPlayer::openSession(const ExclusiveOpenRequest *req, uint8_t *outChangeFlags)
{
    if (req == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    m_activePriority = m_defaultPriority;
    m_exclusiveParam = req->exclusiveParam;

    int prevActive = isActive();
    m_isOpened = 1;
    int curActive = isActive();

    uint8_t changeFlags = *outChangeFlags;
    if (prevActive != curActive) {
        changeFlags |= 0x04u;
    }
    *outChangeFlags = (uint8_t)(changeFlags | 0x10u);
    return 0;
}

class MusicAppPlayer : public PlayerBaseView {
public:
    static size_t calcRequiredSize(void *heap);

    MusicAppPlayer(void *controller, void *dbManager, int *outErr,
                   unsigned int bufSize, int poolOffset);
    virtual ~MusicAppPlayer();

    int closeSession();
    bool isSupportedFormatFlags(uint32_t formatFlags, uint8_t extraFlag);
    void onReservedHook();
    int openSession(const MusicAppOpenRequest *req, uint8_t *outChangeFlags);
    int handlePlayCommand(const MusicAppPlayCommand *cmd, uint8_t *outChangeFlags);
    int handleClearPlaylist(const MusicAppPlayCommand *cmd);
    int handleSeekCommand(const PlayerCommandPacket *cmd);
    int handleSkipNextCommand(const PlayerCommandPacket *cmd);
    int handleSkipPrevCommand(const PlayerCommandPacket *cmd);
    int handleSetRepeatCommand(const PlayerCommandPacket *cmd);
    int handleSetShuffleCommand(const PlayerCommandPacket *cmd);
    int handleTrackEventNotify(const PlayerCommandPacket *cmd);
    int handleStartOrResume();
    int handlePause();
    int handlePlaybackNotify(const PlayerCommandPacket *cmd, int extra);
    int copyCurrentMetadata(MetadataQueryBuffer *outMeta);
    int tickPlaybackPowerHold();
    int tickAutoSuspendTimer();

    virtual int executePlayStart() = 0; /* +0xE4 (228) */

private:
    void clearAutoSuspendCountdown();

    SceUInt64 m_playPowerTickUs;      /* +472 */
    int m_autoSuspendTimeoutSec;      /* +480 */
    int m_padAutoSuspend;             /* +484 */
    SceUInt64 m_autoSuspendStartUs;   /* +488 */
    SceUInt64 m_autoSuspendLastTickUs;/* +496 */
    char m_titleStorage[2048];        /* +504 */
    char m_artistStorage[2048];       /* +2552 */
    char m_albumStorage[2048];        /* +4600 */
};

/* 0x811F5338 */
size_t MusicAppPlayer::calcRequiredSize(void *heap)
{
    return getPlayerBasePoolSize(heap) + 6648u;
}

/* 0x811F5346 */
MusicAppPlayer::MusicAppPlayer(void *controller, void *dbManager, int *outErr,
                               unsigned int bufSize, int poolOffset)
{
    playerBaseConstruct(this, 1, controller, dbManager, outErr, bufSize, poolOffset);
    m_playPowerTickUs = 0;
    m_autoSuspendTimeoutSec = 0;
    m_autoSuspendStartUs = 0;
    m_autoSuspendLastTickUs = 0;

    if (bufSize < getPlayerBasePoolSize(dbManager) + 6648u) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    m_defaultPriority = MUSIC_APP_PLAYER_PRIORITY;
    if (m_metaMutex != NULL) {
        m_metaMutex->Lock();
    }

    sce_paf_memset(m_titleStorage, 0, sizeof(m_titleStorage));
    m_metaBuffers.titlePtr = m_titleStorage;
    m_metaBuffers.titleCapacity = sizeof(m_titleStorage);

    sce_paf_memset(m_artistStorage, 0, sizeof(m_artistStorage));
    m_metaBuffers.artistPtr = m_artistStorage;
    m_metaBuffers.artistCapacity = sizeof(m_artistStorage);

    sce_paf_memset(m_albumStorage, 0, sizeof(m_albumStorage));
    m_metaBuffers.albumPtr = m_albumStorage;
    m_metaBuffers.albumCapacity = sizeof(m_albumStorage);

    if (m_metaMutex != NULL) {
        m_metaMutex->Unlock();
    }

    if (bufSize < getPlayerBasePoolSize(m_heap) + 6648u) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    unsigned int alignedOffset = (((unsigned int)(poolOffset - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedOffset > bufSize) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    size_t factorySize = orderListFactoryCalcSize(m_heap, 2, 2, 56);
    if (factorySize + alignedOffset > bufSize) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    void *factoryMem = (uint8_t *)this + alignedOffset;
    m_orderListFactory = orderListFactoryConstruct(factoryMem, m_heap, 2, 2, factorySize, 56);
}

/* 0x811F549E / 0x811F54BA */
MusicAppPlayer::~MusicAppPlayer()
{
    playerBaseDestroy(this);
}

/* 0x811F54DA */
int MusicAppPlayer::closeSession()
{
    int ret = playerBaseStopAndReset(this);
    if (m_metaMutex != NULL) {
        m_metaMutex->Lock();
    }
    sce_paf_memset(m_titleStorage, 0, sizeof(m_titleStorage));
    sce_paf_memset(m_artistStorage, 0, sizeof(m_artistStorage));
    sce_paf_memset(m_albumStorage, 0, sizeof(m_albumStorage));
    if (m_metaMutex != NULL) {
        m_metaMutex->Unlock();
    }
    return ret;
}

/* 0x811F5528 */
bool MusicAppPlayer::isSupportedFormatFlags(uint32_t formatFlags, uint8_t extraFlag)
{
    uint8_t byteLow = (uint8_t)(formatFlags & 0xFFu);
    uint8_t byteMid = (uint8_t)((formatFlags >> 8) & 0xFFu);
    uint8_t byteHigh = (uint8_t)((formatFlags >> 16) & 0xFFu);
    if (byteMid == 1u && byteHigh == 1u && byteLow == 1u) {
        return extraFlag == 1u;
    }
    return false;
}

/* 0x811F555A */
void MusicAppPlayer::onReservedHook()
{
}

/* 0x811F555C */
int MusicAppPlayer::openSession(const MusicAppOpenRequest *req, uint8_t *outChangeFlags)
{
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    if (req == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    int priority = req->requestedPriority;
    if (priority == -1) {
        priority = m_defaultPriority;
    }
    m_activePriority = priority;

    int ret = dbManagerOpenSession(m_dbManager);
    if (ret == 0) {
        int prevActive = isActive();
        m_isOpened = 1;
        int curActive = isActive();
        uint8_t changeFlags = *outChangeFlags;
        if (prevActive != curActive) {
            changeFlags |= 0x04u;
        }
        *outChangeFlags = (uint8_t)(changeFlags | 0x10u);
    }
    return ret;
}

/* 0x811F5602 */
int MusicAppPlayer::handlePlayCommand(const MusicAppPlayCommand *cmd, uint8_t *outChangeFlags)
{
    if (cmd->commandType != CMD_PLAY_MUSIC_APP || cmd->payload == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    int prevActive = isActive();
    int ret = executePlayStart();
    if (prevActive != isActive()) {
        *outChangeFlags |= 0x04u;
    }
    return ret;
}

/* 0x811F5684 */
int MusicAppPlayer::handleClearPlaylist(const MusicAppPlayCommand *cmd)
{
    if (cmd->commandType != CMD_CLEAR_PLAYLIST || cmd->payload == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_playbackState != 0) {
        return MC_ERR_INVALID_STATE;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }

    int ret = 0;
    if (m_activeMediaList != NULL) {
        playerBaseClearActiveList(this);
        void *prevList = m_activeMediaList;
        m_activeMediaList = NULL;
        ret = dbManagerReleaseList(m_dbManager, prevList, cmd->dbParamPrimary, cmd->dbParamSecondary, 1);

        m_currentListIndex = 0;
        m_currentListCount = 0;
        m_currentPositionMs = 0;
        m_currentRowId = -1;
        m_currentMediaId = -1;
        m_currentListId = -1;
        m_currentDurationMs = -2;
        m_currentCodec = -1;
        m_currentSampleRate = -1;
        m_currentChannels = -1;
        m_currentBitRate = 0;
        m_activeMediaList = NULL;
        m_repeatMode = -1;
        m_shuffleMode = -1;
        m_playDirection = 1;
        m_errorDomain = 0;
        m_errorDetail = 0;
        m_errorSubCode = 0;
        playerBaseResetMetadataStrings(this, &m_metaBuffers);
    }
    return ret;
}

/* 0x811F579A */
int MusicAppPlayer::handleSeekCommand(const PlayerCommandPacket *cmd)
{
    int ret = playerBaseHandleSeek(this, cmd);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target = (const PlaybackDbCommandTarget *)cmd->payload;
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    return dbManagerNotifySeek(m_dbManager, target->handleId, cmd->paramPrimary, cmd->paramSecondary, 1);
}

/* 0x811F57F8 */
int MusicAppPlayer::handleSkipNextCommand(const PlayerCommandPacket *cmd)
{
    int ret = playerBaseHandleSkipNext(this, cmd);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target = (const PlaybackDbCommandTarget *)cmd->payload;
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    return dbManagerNotifyNext(m_dbManager, target->handleId, cmd->paramPrimary, cmd->paramSecondary, 1);
}

/* 0x811F5856 */
int MusicAppPlayer::handleSkipPrevCommand(const PlayerCommandPacket *cmd)
{
    int ret = playerBaseHandleSkipPrev(this, cmd);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target = (const PlaybackDbCommandTarget *)cmd->payload;
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    return dbManagerNotifyPrev(m_dbManager, target->handleId, 1);
}

/* 0x811F5896 */
int MusicAppPlayer::handleSetRepeatCommand(const PlayerCommandPacket *cmd)
{
    int ret = playerBaseHandleSetRepeat(this, cmd);
    if (ret != 0) {
        return ret;
    }
    if (cmd->payload == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    return dbManagerNotifyRepeat(m_dbManager);
}

/* 0x811F590A */
int MusicAppPlayer::handleSetShuffleCommand(const PlayerCommandPacket *cmd)
{
    int ret = playerBaseHandleSetShuffle(this, cmd);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target = (const PlaybackDbCommandTarget *)cmd->payload;
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbManager == NULL) {
        return MC_ERR_INTERNAL;
    }
    return dbManagerNotifyShuffle(m_dbManager, target->handleId, cmd->paramPrimary, cmd->paramSecondary, 1);
}

void MusicAppPlayer::clearAutoSuspendCountdown()
{
    m_autoSuspendStartUs = 0;
    m_autoSuspendLastTickUs = 0;
    m_autoSuspendActive = 0;
}

/* 0x811F5968 */
int MusicAppPlayer::handleTrackEventNotify(const PlayerCommandPacket *cmd)
{
    const TrackNotifyPayload *payload = (const TrackNotifyPayload *)cmd->payload;
    if (payload == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    if (payload->eventKind == 3 && payload->playCountTrigger == 1) {
        m_completedPlayCount++;
    }

    int ret = playerBaseOnTrackEvent(this, cmd);
    if (ret == 0
        && payload->notifyCode == NOTIFY_PLAYBACK_STOP
        && isActive() == 0
        && m_playPowerTickUs != 0) {
        paf::Plugin *powerPlugin = paf::Plugin::Find("power_manage_plugin");
        if (powerPlugin != NULL) {
            PowerManageInterface *powerIf = (PowerManageInterface *)powerPlugin->GetInterface(1);
            if (powerIf != NULL && powerIf->releaseAutoSuspendHold != NULL) {
                powerIf->releaseAutoSuspendHold();
            }
        }

        m_playPowerTickUs = 0;
        paf::system::Model *model = getModel();
        if (model != NULL) {
            if (m_dbManager == NULL) {
                return MC_ERR_INTERNAL;
            }
            if (model->GetType() == 1) {
                m_autoSuspendTimeoutSec = 600;
                ret = 0;
            } else {
                ret = m_dbManager->settingProvider->getAutoSuspendTimeoutSec(&m_autoSuspendTimeoutSec);
                if (ret != 0) {
                    return ret;
                }
            }

            SceUInt64 nowUs = sceKernelGetProcessTimeWide();
            m_autoSuspendStartUs = nowUs;
            if (nowUs != 0) {
                m_autoSuspendLastTickUs = nowUs;
                m_autoSuspendActive = 1;
                sceKernelPowerTickForUser(POWER_TICK_DISABLE_AUTO_SUSPEND);
            }
        }
    }
    return ret;
}

/* 0x811F5A8A */
int MusicAppPlayer::handleStartOrResume()
{
    int ret = playerBaseResumeOrStart(this);
    if (ret == 0 && m_playbackState != 0 && m_playPowerTickUs == 0) {
        if (isAutoSuspendPending() != 0) {
            clearAutoSuspendCountdown();
        }
        SceUInt64 nowUs = sceKernelGetProcessTimeWide();
        m_playPowerTickUs = nowUs;
        if (nowUs != 0) {
            sceKernelPowerTickForUser(POWER_TICK_DISABLE_AUTO_SUSPEND);
        }
        paf::Plugin *powerPlugin = paf::Plugin::Find("power_manage_plugin");
        if (powerPlugin != NULL) {
            PowerManageInterface *powerIf = (PowerManageInterface *)powerPlugin->GetInterface(1);
            if (powerIf != NULL && powerIf->acquireAutoSuspendHold != NULL) {
                powerIf->acquireAutoSuspendHold();
            }
        }
    }
    return ret;
}

/* 0x811F5B16 */
int MusicAppPlayer::handlePause()
{
    return playerBasePause(this);
}

/* 0x811F5B1E */
int MusicAppPlayer::handlePlaybackNotify(const PlayerCommandPacket *cmd, int extra)
{
    const TrackNotifyPayload *payload = (const TrackNotifyPayload *)cmd->payload;
    if (payload != NULL && payload->dbSyncRequired != 0) {
        dbManagerFlushRecord(m_dbManager);
    }
    return playerBaseHandleNotify(this, cmd, extra);
}

/* 0x811F5B78 */
int MusicAppPlayer::copyCurrentMetadata(MetadataQueryBuffer *outMeta)
{
    int ret = 0;
    if (m_metaMutex != NULL) {
        m_metaMutex->Lock();
    }

    if (outMeta == NULL
        || outMeta->titleCapacity > 2048u
        || outMeta->artistCapacity > 2048u
        || outMeta->albumCapacity > 2048u) {
        ret = MC_ERR_INVALID_ARG;
    } else {
        if (outMeta->titleOut != NULL) {
            sce_paf_strncpy(outMeta->titleOut, m_metaBuffers.titlePtr, outMeta->titleCapacity);
            SceSize termIdx = (outMeta->titleCapacity != 0u) ? (outMeta->titleCapacity - 1u) : 0u;
            outMeta->titleOut[termIdx] = '\0';
        }
        if (outMeta->artistOut != NULL) {
            sce_paf_strncpy(outMeta->artistOut, m_metaBuffers.artistPtr, outMeta->artistCapacity);
            SceSize termIdx = (outMeta->artistCapacity != 0u) ? (outMeta->artistCapacity - 1u) : 0u;
            outMeta->artistOut[termIdx] = '\0';
        }
        if (outMeta->albumOut != NULL) {
            sce_paf_strncpy(outMeta->albumOut, m_metaBuffers.albumPtr, outMeta->albumCapacity);
            SceSize termIdx = (outMeta->albumCapacity != 0u) ? (outMeta->albumCapacity - 1u) : 0u;
            outMeta->albumOut[termIdx] = '\0';
        }
    }

    if (m_metaMutex != NULL) {
        m_metaMutex->Unlock();
    }
    return ret;
}

/* 0x811F5C32 */
int MusicAppPlayer::tickPlaybackPowerHold()
{
    if (m_playPowerTickUs == 0) {
        return 0;
    }

    SceUInt64 nowUs = sceKernelGetProcessTimeWide();
    if (nowUs != 0 && nowUs >= m_playPowerTickUs + 2000000ULL) {
        int ret = sceKernelPowerTickForUser(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        m_playPowerTickUs = nowUs;
        return ret;
    }
    return 0;
}

/* 0x811F5C90 */
int MusicAppPlayer::tickAutoSuspendTimer()
{
    paf::system::Model *model = getModel();
    if (isAutoSuspendPending() == 0) {
        return 0;
    }
    if (model == NULL) {
        clearAutoSuspendCountdown();
        return 0;
    }

    SceUInt64 nowUs = sceKernelGetProcessTimeWide();
    if (nowUs == 0) {
        return 0;
    }

    SceUInt64 startUs = m_autoSuspendStartUs;
    SceUInt64 timeoutUs = (SceUInt64)(uint32_t)(1000000 * m_autoSuspendTimeoutSec);
    bool expired = false;
    if (nowUs <= startUs) {
        if ((nowUs + 0xFFFFFFFFFFFFFFFFULL - startUs) >= timeoutUs) {
            expired = true;
        }
    } else if (nowUs >= startUs + timeoutUs) {
        expired = true;
    }

    if (expired) {
        clearAutoSuspendCountdown();
        return 0;
    }

    if (nowUs >= m_autoSuspendLastTickUs + 1000000ULL) {
        int ret = sceKernelPowerTickForUser(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        m_autoSuspendLastTickUs = nowUs;
        return ret;
    }
    return 1000000;
}

struct RowidResultLayout {
    void *vtablePtr;
    uint32_t headerWords[3];
    paf::thread::RMutex lock;
    uint32_t queryStateWords[3];
    uint32_t rowCount;
    uint32_t rowCapacity;
    int64_t *rowidArray;
    uint32_t tailWords[9];
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

/* 0x811F5DA8 */
MusicRowidResult::MusicRowidResult()
    : dbutil::RowidResult(), m_rngState(1u)
{
}

/* 0x811F5DC4 / 0x811F5DE0 */
MusicRowidResult::~MusicRowidResult()
{
}

/* 0x811F5E00 */
int MusicRowidResult::generateShuffledOrder(uint32_t seed, int64_t anchorRowid,
                                            uint16_t *outIndices, uint32_t capacity)
{
    RowidResultLayout *layout = (RowidResultLayout *)(void *)this;
    layout->lock.Lock();

    uint32_t count = layout->rowCount;
    if (outIndices == NULL || capacity < count) {
        layout->lock.Unlock();
        return DBUTIL_ERR_INVALID_ARG;
    }

    m_rngState = seed;
    if (count != 0u) {
        for (uint32_t idx = 0; idx < count; ++idx) {
            outIndices[idx] = (uint16_t)idx;
        }

        uint32_t rng = m_rngState;
        uint32_t pos = count - 1u;
        while (pos > 1u) {
            rng = 1103515245u * rng + 12345u;
            m_rngState = rng;
            uint32_t randSample = (rng >> 16) & 0x7FFFu;
            uint32_t swapIdx = randSample % (pos + 1u);
            uint16_t saved = outIndices[pos];
            outIndices[pos] = outIndices[swapIdx];
            outIndices[swapIdx] = saved;
            --pos;
        }

        if (anchorRowid != -1) {
            uint32_t totalRows = layout->rowCount;
            const int64_t *rowids = layout->rowidArray;
            for (uint32_t rowIdx = 0; rowIdx < totalRows; ++rowIdx) {
                if (rowids[rowIdx] == anchorRowid) {
                    for (uint32_t orderPos = 0; orderPos < totalRows; ++orderPos) {
                        if (outIndices[orderPos] == (uint16_t)rowIdx) {
                            uint16_t matched = outIndices[orderPos];
                            outIndices[orderPos] = outIndices[0];
                            outIndices[0] = matched;
                            break;
                        }
                    }
                }
            }
        }
    }

    layout->lock.Unlock();
    return 0;
}

/* 0x811F5F44 */
int convertMusicPath(const char *srcPath, char *dstBuf, SceSize dstCapacity, int direction)
{
    const char *targetPrefix = "music0:/";
    const char *sourcePrefix = "ux0:/music/";
    if (direction == 1) {
        sourcePrefix = "music0:/";
        targetPrefix = "ux0:/music/";
    }

    SceSize srcLen = sce_paf_strlen(srcPath);
    SceSize targetPrefixLen = sce_paf_strlen(targetPrefix);
    if (sce_paf_strncmp(targetPrefix, srcPath, targetPrefixLen) == 0) {
        sce_paf_memcpy(dstBuf, srcPath, srcLen);
        dstBuf[srcLen] = '\0';
        return 0;
    }

    SceSize sourcePrefixLen = sce_paf_strlen(sourcePrefix);
    if (sce_paf_strncmp(sourcePrefix, srcPath, sourcePrefixLen) != 0) {
        return -1;
    }

    if (srcLen - sourcePrefixLen + targetPrefixLen >= dstCapacity) {
        return -2;
    }

    /* Ping-pong 32-byte staging buffers allow safe in-place path rewriting. */
    uint8_t stagePrimary[32];
    uint8_t stageSecondary[32];
    sce_paf_memset(stagePrimary, 0, sizeof(stagePrimary));
    sce_paf_memset(stageSecondary, 0, sizeof(stageSecondary));

    SceSize chunkLen = 32u;
    if ((int)(srcLen - sourcePrefixLen) < 32) {
        chunkLen = srcLen - sourcePrefixLen;
    }
    sce_paf_memcpy(stagePrimary, srcPath + sourcePrefixLen, chunkLen);

    int readPos = (int)(sourcePrefixLen + chunkLen);
    sce_paf_memcpy(dstBuf, targetPrefix, targetPrefixLen);

    SceSize writeOffset = targetPrefixLen;
    uint8_t *activeStage = stagePrimary;
    uint8_t *nextStage = stageSecondary;

    while (readPos < (int)srcLen) {
        SceSize nextLen = 32u;
        if ((int)srcLen - readPos < 32) {
            nextLen = (SceSize)((int)srcLen - readPos);
        }
        chunkLen = nextLen;
        sce_paf_memcpy(nextStage, srcPath + readPos, chunkLen);
        readPos += (int)chunkLen;

        sce_paf_memcpy(dstBuf + writeOffset, activeStage, 32u);
        writeOffset += 32u;

        uint8_t *swapBuf = activeStage;
        activeStage = nextStage;
        nextStage = swapBuf;
    }

    sce_paf_memcpy(dstBuf + writeOffset, activeStage, chunkLen);
    dstBuf[writeOffset + chunkLen] = '\0';
    return 0;
}

/* 0x811F60D8 */
int convertToMusicDevicePath(const char *srcPath, char *dstBuf, SceSize dstCapacity)
{
    return convertMusicPath(srcPath, dstBuf, dstCapacity, 2);
}

} /* namespace MusicCore */
} /* namespace sce */
