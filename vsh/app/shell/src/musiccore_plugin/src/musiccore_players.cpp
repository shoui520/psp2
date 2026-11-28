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

#include "musiccore_player_base.h"
#include "musiccore_db_manager.h"

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
SceSize sce_paf_strlen(const char *str);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize len);
int sceKernelPowerTick(int type);
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

struct ExclusiveOpenRequest {
    uint32_t headerWords[3];
    SoundOutExclusiveParam exclusiveParam;
};

struct MusicAppOpenRequest {
    uint32_t headerWords[2];
    int requestedPriority;
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

struct PowerManagePluginInterface {
    void (*func[11])();
    void (*acquireAutoSuspendHold)();
    void (*releaseAutoSuspendHold)();
};

struct PlaybackDbCommandTarget {
    uint32_t reservedHeader;
    int handleId;
};

struct EqualizerDbCommand {
    uint32_t reservedHeader;
    int eqType;
    int bandLow;
    int bandMid;
    int bandHigh;
};

extern size_t orderListFactoryCalcSize(void *heap, int maxLists, int listSlots, int entrySize);
extern void *orderListFactoryConstruct(void *mem, void *heap, int maxLists, int listSlots,
                                       size_t allocSize, int entrySize);

static uint32_t packCmdNodeHi(const CmdNode *node)
{
    return static_cast<uint32_t>(node->targetChannel) |
           (static_cast<uint32_t>(node->flagsByte) << 8) |
           (static_cast<uint32_t>(node->cmdCode) << 16);
}

class ExclusiveMusicPlayer : public BasePlayer {
public:
    static size_t calcRequiredSize(int capacity);

    ExclusiveMusicPlayer(void *controller, int capacity, int *outErr,
                         unsigned int bufSize, int poolOffset);
    virtual ~ExclusiveMusicPlayer();

    virtual unsigned int calcAllocSize()
    {
        return BasePlayer::calcBaseExtraSize(m_capacity) + 496u;
    }
    virtual int open(const void *param, unsigned char *flags);
    virtual int isActiveForInfo(PlayerPriorityInfo info);
    virtual int wantsAudioForInfo(PlayerPriorityInfo info);

private:
    SoundOutExclusiveParam m_exclusiveParam; /* +472..+495 */
};

/* 0x811F51FC */
size_t ExclusiveMusicPlayer::calcRequiredSize(int capacity)
{
    return BasePlayer::calcBaseExtraSize(capacity) + 496u;
}

/* 0x811F5208 */
ExclusiveMusicPlayer::ExclusiveMusicPlayer(void *controller, int capacity, int *outErr,
                                           unsigned int bufSize, int poolOffset)
    : BasePlayer(2, controller, capacity, outErr, bufSize, static_cast<unsigned int>(poolOffset))
{
    m_exclusiveParam.mode = 5;
    m_exclusiveParam.reserved = 0;
    m_exclusiveParam.flags = 0;
    m_exclusiveParam.param = 0;
    m_exclusiveParam.extraParam = 0;
    m_exclusiveParam.auxParam = 0;

    if (bufSize >= BasePlayer::calcBaseExtraSize(capacity) + 496u) {
        m_subPriority = EXCLUSIVE_PLAYER_PRIORITY;
        m_exclusiveParam.mode = 5;
        m_exclusiveParam.reserved = 0;
        m_exclusiveParam.flags = 0;
        m_exclusiveParam.param = 0;
        m_exclusiveParam.extraParam = 0;
        m_exclusiveParam.auxParam = 0;
    } else {
        *outErr = MC_ERR_NO_MEMORY;
    }
}

/* 0x811F5296 / 0x811F52B2 */
ExclusiveMusicPlayer::~ExclusiveMusicPlayer()
{
}

/* 0x811F52D2 */
int ExclusiveMusicPlayer::open(const void *param, unsigned char *flags)
{
    const ExclusiveOpenRequest *req = static_cast<const ExclusiveOpenRequest *>(param);
    if (req == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    m_priority = m_subPriority;
    m_exclusiveParam = req->exclusiveParam;

    int prevWant = wantsAudio();
    m_isOpen = 1;
    int curWant = wantsAudio();

    unsigned char changeFlags = *flags;
    if (prevWant != curWant) {
        changeFlags |= 0x04u;
    }
    *flags = static_cast<unsigned char>(changeFlags | 0x10u);
    return 0;
}

int ExclusiveMusicPlayer::isActiveForInfo(PlayerPriorityInfo info)
{
    if (info.isEnabled == 1 && info.isDbAllowed == 1 && info.isOpen == 1)
        return (info.wantsPlay == 1) ? 1 : 0;
    return 0;
}

int ExclusiveMusicPlayer::wantsAudioForInfo(PlayerPriorityInfo info)
{
    if (isActiveForInfo(info) == 1)
        return (info.isBusy == 1) ? 1 : 0;
    return 0;
}

class MusicAppPlayer : public BasePlayer {
public:
    static size_t calcRequiredSize(int capacity);

    MusicAppPlayer(void *controller, int capacity, int *outErr,
                   unsigned int bufSize, int poolOffset);
    virtual ~MusicAppPlayer();

    virtual unsigned int calcAllocSize()
    {
        return BasePlayer::calcBaseExtraSize(m_capacity) + 6648u;
    }
    virtual int detach();
    virtual int open(const void *param, unsigned char *flags);
    virtual int isActiveForInfo(PlayerPriorityInfo info);
    virtual int wantsAudioForInfo(PlayerPriorityInfo info);
    virtual int getContentInfo(MetadataBuffers *outMeta);
    virtual void onIdleActive();
    virtual void onIdlePlaylist();
    virtual int cmdSetRepeat(CmdNode *node, unsigned char *flags);
    virtual int cmdSetShuffle(CmdNode *node, unsigned char *flags);
    virtual int cmdClearPlaylist(CmdNode *node, unsigned char *flags);
    virtual int cmdSetAudioBalance(CmdNode *node, unsigned char *flags);
    virtual int cmdSetEqualizer(CmdNode *node, unsigned char *flags);
    virtual int cmdSetAudioNormalizer(CmdNode *node, unsigned char *flags);
    virtual int cmdNotifyStatus(CmdNode *node, unsigned char *flags);
    virtual int onDecoderEvent(CmdNode *node, unsigned char *flags);
    virtual int onDecoderTrackInfo(CmdNode *node);
    virtual int onDecoderAudioError(CmdNode *node, unsigned char *flags);
    virtual int onDecoderStreamEnd(CmdNode *node, unsigned char *flags);

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
size_t MusicAppPlayer::calcRequiredSize(int capacity)
{
    return BasePlayer::calcBaseExtraSize(capacity) + 6648u;
}

/* 0x811F5346 */
MusicAppPlayer::MusicAppPlayer(void *controller, int capacity, int *outErr,
                               unsigned int bufSize, int poolOffset)
    : BasePlayer(1, controller, capacity, outErr, bufSize, static_cast<unsigned int>(poolOffset)),
      m_playPowerTickUs(0),
      m_autoSuspendTimeoutSec(0),
      m_padAutoSuspend(0),
      m_autoSuspendStartUs(0),
      m_autoSuspendLastTickUs(0)
{
    if (bufSize < BasePlayer::calcBaseExtraSize(capacity) + 6648u) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    m_subPriority = MUSIC_APP_PLAYER_PRIORITY;
    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Lock();
    }

    sce_paf_memset(m_titleStorage, 0, sizeof(m_titleStorage));
    m_metaBufs.titleBuf = m_titleStorage;
    m_metaBufs.titleLen = sizeof(m_titleStorage);

    sce_paf_memset(m_artistStorage, 0, sizeof(m_artistStorage));
    m_metaBufs.artistBuf = m_artistStorage;
    m_metaBufs.artistLen = sizeof(m_artistStorage);

    sce_paf_memset(m_albumStorage, 0, sizeof(m_albumStorage));
    m_metaBufs.albumBuf = m_albumStorage;
    m_metaBufs.albumLen = sizeof(m_albumStorage);

    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Unlock();
    }

    if (bufSize < BasePlayer::calcBaseExtraSize(m_capacity) + 6648u) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    unsigned int alignedOffset = (((unsigned int)(poolOffset - 1)) & 0xFFFFFFF8u) + 8u;
    if (alignedOffset > bufSize) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    void *capacityArg = reinterpret_cast<void *>(static_cast<uintptr_t>(m_capacity));
    size_t factorySize = orderListFactoryCalcSize(capacityArg, 2, 2, 56);
    if (factorySize + alignedOffset > bufSize) {
        *outErr = MC_ERR_NO_MEMORY;
        return;
    }

    uint8_t *selfBytes = reinterpret_cast<uint8_t *>(this);
    void *factoryMem = selfBytes + alignedOffset;
    m_mediaQueue = orderListFactoryConstruct(factoryMem, capacityArg, 2, 2, factorySize, 56);
}

/* 0x811F549E / 0x811F54BA */
MusicAppPlayer::~MusicAppPlayer()
{
}

/* 0x811F54DA */
int MusicAppPlayer::detach()
{
    int ret = BasePlayer::detach();
    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Lock();
    }
    sce_paf_memset(m_titleStorage, 0, sizeof(m_titleStorage));
    sce_paf_memset(m_artistStorage, 0, sizeof(m_artistStorage));
    sce_paf_memset(m_albumStorage, 0, sizeof(m_albumStorage));
    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Unlock();
    }
    return ret;
}

/* 0x811F5528 */
int MusicAppPlayer::isActiveForInfo(PlayerPriorityInfo info)
{
    if (info.isEnabled == 1 && info.isDbAllowed == 1 && info.isOpen == 1)
        return (info.wantsPlay == 1) ? 1 : 0;
    return 0;
}

int MusicAppPlayer::wantsAudioForInfo(PlayerPriorityInfo info)
{
    if (isActiveForInfo(info) == 1)
        return (info.isBusy == 1) ? 1 : 0;
    return 0;
}

/* 0x811F555C */
int MusicAppPlayer::open(const void *param, unsigned char *flags)
{
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    const MusicAppOpenRequest *req = static_cast<const MusicAppOpenRequest *>(param);
    if (req == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    int priority = req->requestedPriority;
    if (priority == -1) {
        priority = m_subPriority;
    }
    m_priority = priority;

    int ret = static_cast<DbManager *>(m_dbMgr)->requestLoadSettings(0, m_channelId, 0);
    if (ret == 0) {
        int prevWant = wantsAudio();
        m_isOpen = 1;
        int curWant = wantsAudio();
        unsigned char changeFlags = *flags;
        if (prevWant != curWant) {
            changeFlags |= 0x04u;
        }
        *flags = static_cast<unsigned char>(changeFlags | 0x10u);
    }
    return ret;
}

/* 0x811F5602 */
int MusicAppPlayer::cmdNotifyStatus(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != CMD_PLAY_MUSIC_APP || node->inBuf == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    int prevWant = wantsAudio();
    int ret = cmdResetSpeed(node->param, packCmdNodeHi(node), flags);
    if (prevWant != wantsAudio()) {
        *flags |= 0x04u;
    }
    return ret;
}

/* 0x811F5684 */
int MusicAppPlayer::cmdClearPlaylist(CmdNode *node, unsigned char *flags)
{
    (void)flags;
    if (node->cmdCode != CMD_CLEAR_PLAYLIST || node->inBuf == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_decoder != NULL) {
        return MC_ERR_INVALID_STATE;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }

    int ret = 0;
    if (m_offlineState.mediaHandle != NULL) {
        clearPendingUriIfMatches(static_cast<MediaItemHandle *>(m_offlineState.mediaHandle));
        void *prevList = m_offlineState.mediaHandle;
        m_offlineState.mediaHandle = NULL;
        ret = static_cast<DbManager *>(m_dbMgr)->requestCloseMedia(
            prevList, node->param, packCmdNodeHi(node), 1);

        m_offlineState.stateId = 0;
        m_offlineState.playState = 0;
        m_offlineState.trackIndex = 0;
        m_offlineState.playlistId = -1;
        m_offlineState.trackId = -1;
        m_offlineState.trackItemId = -1;
        m_offlineState.trackOrderIndex = -2;
        m_offlineState.positionMs = -1;
        m_offlineState.currentTrackNum = -1;
        m_offlineState.totalTracks = -1;
        m_offlineState.durationMs = 0;
        m_offlineState.mediaHandle = NULL;
        m_offlineState.seekMode = -1;
        m_offlineState.speedMode = -1;
        m_offlineState.volumeMode = 1;
        m_offlineState.lastErrorType = 0;
        m_offlineState.hasPendingError = 0;
        m_offlineState.extraErr = 0;
        BasePlayer::getContentInfo(&m_metaBufs);
    }
    return ret;
}

/* 0x811F579A */
int MusicAppPlayer::cmdSetRepeat(CmdNode *node, unsigned char *flags)
{
    int ret = BasePlayer::cmdSetRepeat(node, flags);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target =
        static_cast<const PlaybackDbCommandTarget *>(node->inBuf);
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    return static_cast<DbManager *>(m_dbMgr)->requestSetRepeatMode(
        target->handleId, node->param, packCmdNodeHi(node), 1);
}

/* 0x811F57F8 */
int MusicAppPlayer::cmdSetShuffle(CmdNode *node, unsigned char *flags)
{
    int ret = BasePlayer::cmdSetShuffle(node, flags);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target =
        static_cast<const PlaybackDbCommandTarget *>(node->inBuf);
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    return static_cast<DbManager *>(m_dbMgr)->requestSetShuffleMode(
        target->handleId, node->param, packCmdNodeHi(node), 1);
}

/* 0x811F5856 */
int MusicAppPlayer::cmdSetAudioBalance(CmdNode *node, unsigned char *flags)
{
    int ret = BasePlayer::cmdSetAudioBalance(node, flags);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target =
        static_cast<const PlaybackDbCommandTarget *>(node->inBuf);
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    return static_cast<DbManager *>(m_dbMgr)->requestSetAudioBalance(
        target->handleId, 1, node->param, packCmdNodeHi(node), 1);
}

/* 0x811F5896 */
int MusicAppPlayer::cmdSetEqualizer(CmdNode *node, unsigned char *flags)
{
    int ret = BasePlayer::cmdSetEqualizer(node, flags);
    if (ret != 0) {
        return ret;
    }
    const EqualizerDbCommand *eqCmd =
        static_cast<const EqualizerDbCommand *>(node->inBuf);
    if (eqCmd == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    return static_cast<DbManager *>(m_dbMgr)->requestSetEqualizer(
        eqCmd->eqType, eqCmd->bandLow, eqCmd->bandMid, eqCmd->bandHigh,
        node->param, packCmdNodeHi(node), 1);
}

/* 0x811F590A */
int MusicAppPlayer::cmdSetAudioNormalizer(CmdNode *node, unsigned char *flags)
{
    int ret = BasePlayer::cmdSetAudioNormalizer(node, flags);
    if (ret != 0) {
        return ret;
    }
    const PlaybackDbCommandTarget *target =
        static_cast<const PlaybackDbCommandTarget *>(node->inBuf);
    if (target == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (m_dbMgr == NULL) {
        return MC_ERR_INTERNAL;
    }
    return static_cast<DbManager *>(m_dbMgr)->requestSetNormalizer(
        target->handleId, node->param, packCmdNodeHi(node), 1);
}

void MusicAppPlayer::clearAutoSuspendCountdown()
{
    m_autoSuspendStartUs = 0;
    m_autoSuspendLastTickUs = 0;
    m_playable = 0;
}

/* 0x811F5968 */
int MusicAppPlayer::onDecoderEvent(CmdNode *node, unsigned char *flags)
{
    const TrackNotifyPayload *payload = static_cast<const TrackNotifyPayload *>(node->inBuf);
    if (payload == NULL) {
        return MC_ERR_INVALID_ARG;
    }

    if (payload->eventKind == 3 && payload->playCountTrigger == 1) {
        m_seqNum++;
    }

    int ret = BasePlayer::onDecoderEvent(node, flags);
    if (ret == 0
        && payload->notifyCode == NOTIFY_PLAYBACK_STOP
        && wantsAudio() == 0
        && m_playPowerTickUs != 0) {
        paf::Plugin *powerPlugin = paf::Plugin::Find("power_manage_plugin");
        if (powerPlugin != NULL) {
            PowerManagePluginInterface *powerIf =
                static_cast<PowerManagePluginInterface *>(powerPlugin->GetInterface(1));
            if (powerIf != NULL && powerIf->releaseAutoSuspendHold != NULL) {
                powerIf->releaseAutoSuspendHold();
            }
        }

        m_playPowerTickUs = 0;
        if (getSystemSuspended() != 0) {
            DbManager *dbMgr = static_cast<DbManager *>(m_dbMgr);
            if (dbMgr == NULL) {
                return MC_ERR_INTERNAL;
            }
            if (paf::system::Model::GetType() == 1) {
                m_autoSuspendTimeoutSec = 600;
                ret = 0;
            } else {
                ret = dbMgr->regCache().getSuspendInterval(&m_autoSuspendTimeoutSec);
                if (ret != 0) {
                    return ret;
                }
            }

            SceUInt64 nowUs = sceKernelGetProcessTimeWide();
            m_autoSuspendStartUs = nowUs;
            if (nowUs != 0) {
                m_autoSuspendLastTickUs = nowUs;
                m_playable = 1;
                sceKernelPowerTick(POWER_TICK_DISABLE_AUTO_SUSPEND);
            }
        }
    }
    return ret;
}

/* 0x811F5A8A */
int MusicAppPlayer::onDecoderTrackInfo(CmdNode *node)
{
    int ret = BasePlayer::onDecoderTrackInfo(node);
    if (ret == 0 && m_decoder != NULL && m_playPowerTickUs == 0) {
        if (isPlayable() != 0) {
            clearAutoSuspendCountdown();
        }
        SceUInt64 nowUs = sceKernelGetProcessTimeWide();
        m_playPowerTickUs = nowUs;
        if (nowUs != 0) {
            sceKernelPowerTick(POWER_TICK_DISABLE_AUTO_SUSPEND);
        }
        paf::Plugin *powerPlugin = paf::Plugin::Find("power_manage_plugin");
        if (powerPlugin != NULL) {
            PowerManagePluginInterface *powerIf =
                static_cast<PowerManagePluginInterface *>(powerPlugin->GetInterface(1));
            if (powerIf != NULL && powerIf->acquireAutoSuspendHold != NULL) {
                powerIf->acquireAutoSuspendHold();
            }
        }
    }
    return ret;
}

/* 0x811F5B16 */
int MusicAppPlayer::onDecoderAudioError(CmdNode *node, unsigned char *flags)
{
    return BasePlayer::onDecoderAudioError(node, flags);
}

/* 0x811F5B1E */
int MusicAppPlayer::onDecoderStreamEnd(CmdNode *node, unsigned char *flags)
{
    const TrackNotifyPayload *payload = static_cast<const TrackNotifyPayload *>(node->inBuf);
    if (payload != NULL && payload->dbSyncRequired != 0) {
        static_cast<DbManager *>(m_dbMgr)->requestSetPowerSave(
            payload->dbSyncRequired, node->param, packCmdNodeHi(node), 1);
    }
    return BasePlayer::onDecoderStreamEnd(node, flags);
}

/* 0x811F5B78 */
int MusicAppPlayer::getContentInfo(MetadataBuffers *outMeta)
{
    int ret = 0;
    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Lock();
    }

    if (outMeta == NULL
        || outMeta->titleLen > 2048u
        || outMeta->artistLen > 2048u
        || outMeta->albumLen > 2048u) {
        ret = MC_ERR_INVALID_ARG;
    } else {
        if (outMeta->titleBuf != NULL) {
            sce_paf_strncpy(outMeta->titleBuf, m_metaBufs.titleBuf, outMeta->titleLen);
            SceSize termIdx = (outMeta->titleLen != 0u) ? (outMeta->titleLen - 1u) : 0u;
            outMeta->titleBuf[termIdx] = '\0';
        }
        if (outMeta->artistBuf != NULL) {
            sce_paf_strncpy(outMeta->artistBuf, m_metaBufs.artistBuf, outMeta->artistLen);
            SceSize termIdx = (outMeta->artistLen != 0u) ? (outMeta->artistLen - 1u) : 0u;
            outMeta->artistBuf[termIdx] = '\0';
        }
        if (outMeta->albumBuf != NULL) {
            sce_paf_strncpy(outMeta->albumBuf, m_metaBufs.albumBuf, outMeta->albumLen);
            SceSize termIdx = (outMeta->albumLen != 0u) ? (outMeta->albumLen - 1u) : 0u;
            outMeta->albumBuf[termIdx] = '\0';
        }
    }

    if (m_metaMutexPtr != NULL) {
        m_metaMutexPtr->Unlock();
    }
    return ret;
}

/* 0x811F5C32 */
void MusicAppPlayer::onIdleActive()
{
    if (m_playPowerTickUs == 0) {
        return;
    }

    SceUInt64 nowUs = sceKernelGetProcessTimeWide();
    if (nowUs != 0 && nowUs >= m_playPowerTickUs + 2000000ULL) {
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        m_playPowerTickUs = nowUs;
    }
}

/* 0x811F5C90 */
void MusicAppPlayer::onIdlePlaylist()
{
    int sysSuspended = getSystemSuspended();
    if (isPlayable() == 0) {
        return;
    }
    if (sysSuspended == 0) {
        clearAutoSuspendCountdown();
        return;
    }

    SceUInt64 nowUs = sceKernelGetProcessTimeWide();
    if (nowUs == 0) {
        return;
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
        return;
    }

    if (nowUs >= m_autoSuspendLastTickUs + 1000000ULL) {
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        m_autoSuspendLastTickUs = nowUs;
    }
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
    uint8_t currChunkBuf[32];
    uint8_t nextChunkBuf[32];
    sce_paf_memset(currChunkBuf, 0, sizeof(currChunkBuf));
    sce_paf_memset(nextChunkBuf, 0, sizeof(nextChunkBuf));

    SceSize chunkLen = 32u;
    if ((int)(srcLen - sourcePrefixLen) < 32) {
        chunkLen = srcLen - sourcePrefixLen;
    }
    sce_paf_memcpy(currChunkBuf, srcPath + sourcePrefixLen, chunkLen);

    int readPos = (int)(sourcePrefixLen + chunkLen);
    sce_paf_memcpy(dstBuf, targetPrefix, targetPrefixLen);

    SceSize writeOffset = targetPrefixLen;
    uint8_t *activeStage = currChunkBuf;
    uint8_t *nextStage = nextChunkBuf;

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

struct BaseObject;

unsigned int calcPlaylistPlayerSize(int maxItems)
{
    return static_cast<unsigned int>(MusicAppPlayer::calcRequiredSize(maxItems));
}

unsigned int calcStreamPlayerSize(int maxStreams)
{
    return static_cast<unsigned int>(ExclusiveMusicPlayer::calcRequiredSize(maxStreams));
}

BaseObject *constructPlaylistPlayer(void *mem, void *factory, int maxItems,
                                    int *outErr, unsigned int slotSize, unsigned int hdrSize)
{
    return reinterpret_cast<BaseObject *>(
        new (mem) MusicAppPlayer(factory, maxItems, outErr, slotSize, hdrSize));
}

BaseObject *constructStreamPlayer(void *mem, void *factory, int maxStreams,
                                  int *outErr, unsigned int slotSize, unsigned int hdrSize)
{
    return reinterpret_cast<BaseObject *>(
        new (mem) ExclusiveMusicPlayer(factory, maxStreams, outErr, slotSize, hdrSize));
}

} /* namespace MusicCore */
} /* namespace sce */
