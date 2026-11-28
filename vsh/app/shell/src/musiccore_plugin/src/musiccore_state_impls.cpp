/*
 * SceShell musiccore_plugin - PlaybackStateBase and concrete playback state
 * implementations (StateIdle, StateActivating, StateReady, StateStarting,
 * StatePlaying, StateStopping, StateDeactivating).
 * Original address range (3.65-CEX): 0x811FBEEC - 0x811FDB5B
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_audio_output.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_state_machine.h"
#include <stdlib.h>

extern "C" {
void *sce_paf_memset(void *dst, int val, SceSize size);
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
}

namespace sce {
namespace MusicCore {

static const int MC_ERR_FAILED = -1879113726;         /* 0x8FFF0002 */
static const int MC_ERR_BUSY = -1879113725;           /* 0x8FFF0003 */
static const int MC_ERR_INVALID_ARG = -1879113723;    /* 0x8FFF0005 */
static const int MC_ERR_NO_MEDIA = -1879113722;       /* 0x8FFF0006 */
static const int MC_ERR_INVALID_STATE = -1879113721;  /* 0x8FFF0007 */
static const int MC_ERR_LIST_EMPTY = -1879113720;     /* 0x8FFF0008 */
static const int MC_ERR_BAD_CODEC = -1879113719;      /* 0x8FFF0009 */
static const int MC_ERR_BAD_SOURCE = -1879113718;     /* 0x8FFF000A */
static const int MC_ERR_DB_REMOVED = -1879113715;     /* 0x8FFF000D */
static const int MC_ERR_END_FWD = -1879113713;        /* 0x8FFF000F */
static const int MC_ERR_END_REV = -1879113712;        /* 0x8FFF0010 */

static const int STATE_IDLE = 0;
static const int STATE_ACTIVATING = 1;
static const int STATE_READY = 2;
static const int STATE_STARTING = 3;
static const int STATE_PLAYING = 4;
static const int STATE_PAUSED = 5;
static const int STATE_STOPPING = 6;
static const int STATE_STOPPED = 7;
static const int STATE_DEACTIVATING = 8;

static const uint16_t CMD_DEACTIVATE = 0x1101u;       /* 4353 */
static const uint16_t CMD_PLAY = 0x1102u;             /* 4354 */
static const uint16_t CMD_STOP = 0x1104u;             /* 4356 */
static const uint16_t CMD_PAUSE = 0x1105u;            /* 4357 */
static const uint16_t CMD_SEEK = 0x1106u;             /* 4358 */
static const uint16_t CMD_SEEK_AND_PLAY = 0x1107u;    /* 4359 */
static const uint16_t CMD_SET_MEDIA = 0x110Bu;        /* 4363 */
static const uint16_t CMD_SET_SHUFFLE = 0x110Fu;      /* 4367 */
static const uint16_t CMD_SET_MEDIA_PLAY = 0x1111u;   /* 4369 */
static const uint16_t CMD_SUSPEND_DB = 0x1112u;       /* 4370 */
static const uint16_t EVT_TRACK_READY = 0x1206u;      /* 4614 */

static const uint16_t WAIT_NONE = 0x1100u;            /* 4352 */
static const uint16_t WAIT_START_DONE = 0x1200u;      /* 4608 */
static const uint16_t WAIT_STOP_DONE = 0x1201u;       /* 4609 */
static const uint16_t WAIT_ACTIVATE_DONE = 0x1204u;   /* 4612 */
static const uint16_t WAIT_DEACTIVATE_DONE = 0x1205u; /* 4613 */

struct DecoderStartParams {
    int64_t trackId;
    int64_t trackItemId;
    int trackOrderIndex;
    int startPosMs;
    int resumePosMs;
    int endPosMs;
    int durationMs;
    int sourceKind;
    int sampleRate;
    int codecFormat;
    int bitRate;
    int channels;
    int loopCount;
    int speedMode;
};

struct ActivateStateSnapshot {
    int stateId;
    int playState;
    int64_t playlistId;
    int64_t trackId;
    int64_t trackItemId;
    int trackOrderIndex;
    int trackIndex;
    int positionMs;
    int currentTrackNum;
    int totalTracks;
    int durationMs;
    int sampleRate;
    int bitRate;
    int channels;
    int codecType;
    void *mediaHandle;
    int repeatMode;
    int shuffleMode;
    int seekMode;
    int speedMode;
    int volumeMode;
    int audioBalance;
    int eqType;
    int eqBandLow;
    int eqBandMid;
    int eqBandHigh;
    int audioNormalizer;
    int sourceType;
    int lastErrorType;
    int hasPendingError;
    int extraErr;
    int targetState;
    int pendingFadeMs;
    int playParam;
};

struct AudioEqParams {
    int eqType;
    int bandLow;
    int bandMid;
    int bandHigh;
};

class MediaListAccess {
public:
    virtual ~MediaListAccess() {}
    virtual void initialize() = 0;
    virtual void finalize() = 0;
    virtual int openList() = 0;
    virtual int closeList() = 0;
    virtual int refreshList() = 0;
    virtual int seekToIndex(uint32_t target, uint8_t *outChanged) = 0;
    virtual uint32_t stepNextCandidate(uint32_t curIdx, int wrapMode, int dir,
                                       uint32_t *ioTried, int *ioErr,
                                       uint8_t *outReachedEnd, uint8_t *outWrapped) = 0;
    virtual int resolveCurrentTrackId(int64_t *outTrackId) = 0;
    virtual void setTrackPosition(int orderIdx, int64_t *ioTrackId) = 0;
    virtual int queryTrackMeta(void *metaOut) = 0;
    virtual int getListKind() = 0;
    virtual int isListReady() = 0;
    virtual int getStatus(MediaListStatus *outStatus) = 0;
    virtual void *getListHandle() = 0;
    virtual int hasTracks() = 0;
    virtual int getCurrentIndex() = 0;
    virtual int64_t getCurrentTrackKey() = 0;
    virtual void lockList(int shuffleSeed, int64_t trackKey, uint8_t *outLocked, int flag) = 0;
    virtual void unlockList(int flag) = 0;
    virtual void applyShuffleSeed() = 0;
    virtual int getTrackCount() = 0;
    virtual int getTrackOrderIndex(int orderIdx) = 0;
};

struct EmbeddedStateObject {
    void *vtablePtr;
    void *owner;
    int stateId;
};

struct StateMachineContext {
    virtual ~StateMachineContext() {}
    virtual int onActivatedHook() = 0;
    virtual int onTrackInfo() = 0;
    virtual int onAudioError() = 0;
    virtual int onBufferReady() = 0;
    virtual int onDeactivatedHook(void *ownerToken) = 0;
    virtual int onMediaChanged(uint8_t hasNewMedia, void *newMedia, uint8_t hasUriFlag,
                               const void *stateSnap, void **ioOldMedia, uint8_t decUri) = 0;

    void *m_owner;
    EmbeddedStateObject m_states[8];
    void *m_decoderMgr;                    /* +104 */
    DecoderStartParams *m_decoderStartCfg; /* +108 */
    void *m_audioOut;                      /* +112 */
    void *m_currentState;                  /* +116 */
    uint8_t m_deactivateRequested;         /* +120 */
    uint8_t m_stopRequested;               /* +121 */
    uint8_t m_hasPendingTrack;             /* +122 */
    uint8_t m_dbRemovedError;              /* +123 */
    uint8_t m_speedDirChanged;             /* +124 */
    uint8_t m_seekFailed;                  /* +125 */
    uint8_t m_padFlags[2];                 /* +126..+127 */
    MediaListAccess *m_pendingMedia;       /* +128 */
    int m_pendingSeekStep;                 /* +132 */
    int m_pendingVolumeMode;               /* +136 */
    int m_padPendingAlign;                 /* +140 */
    TrackInfoData m_pendingTrackInfo;      /* +144 */
    int m_padPendingTail;                  /* +2292 */
    int m_lastDbErr;                       /* +2296 */
    int m_extraSeekErr;                    /* +2300 */
    int m_targetState;                     /* +2304 */
    int m_pendingFadeMs;                   /* +2308 */
    int m_playParam;                       /* +2312 */
    uint8_t m_needDecoderRestart;          /* +2316 */
    uint8_t m_padRestart[3];               /* +2317..+2319 */
    MediaListAccess *m_activeMedia;        /* +2320 */
    int m_currentPosMs;                    /* +2324 */
    int m_prevTrackPos;                    /* +2328 */
    int m_nextTrackPos;                    /* +2332 */
    TrackInfoData m_activeTrackInfo;       /* +2336 */
    int m_padActiveTail;                   /* +4484 */
    MediaListStatus m_mediaListStatus;     /* +4488 */
    int m_repeatMode;                      /* +4528 */
    int m_shuffleSeed;                     /* +4532 */
    int m_seekMode;                        /* +4536 */
    int m_speedMode;                       /* +4540 */
    int m_volumeMode;                      /* +4544 */
    int m_audioBalance;                    /* +4548 */
    int m_eqType;                          /* +4552 */
    int m_eqBandLow;                       /* +4556 */
    int m_eqBandMid;                       /* +4560 */
    int m_eqBandHigh;                      /* +4564 */
    int m_audioNormalizer;                 /* +4568 */
    int m_hasPendingError;                 /* +4572 */
    int m_lastErrorType;                   /* +4576 */
    int m_extraErr;                        /* +4580 */
    void *m_metaBufs;                      /* +4584 */
    int m_cachedPosMs;                     /* +4588 */
    int m_activeStartCount;                /* +4592 */
    void *m_activateToken;                 /* +4596 */
};

extern void clearRejectedIdList();

extern void computePlaybackRange(int seekMode, int speedMode, int step, int durationMs,
                                 int volumeMode, int *outPrev, int *outNext, int *outCur);
extern int trackInfoReleaseResources(void *info, int closeFlag);
extern int trackInfoMoveTo(void *src, void *dst, int moveFlag);
extern void trackInfoResetCounters(void *info);

/* ===== PlaybackStateBase (0x811FCF14 - 0x811FCFBB) ===== */

/* 0x811FCF14 */
PlaybackStateBase::PlaybackStateBase(StateMachineContext *owner, int stateId, int *outErr)
    : m_owner(owner)
    , m_stateId(stateId)
{
    *outErr = (owner == NULL) ? MC_ERR_INVALID_ARG : 0;
}

/* 0x811FCF38 / 0x811FCF46 */
PlaybackStateBase::~PlaybackStateBase()
{
}

int PlaybackStateBase::onEnter() { return 0; }
int PlaybackStateBase::onExit() { return 0; }
int PlaybackStateBase::onActivate(const ActivateStateSnapshot *snap, int fadeMs)
{
    (void)snap;
    (void)fadeMs;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onDeactivate(int fadeMs, uint8_t syncFlag)
{
    (void)fadeMs;
    (void)syncFlag;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag)
{
    (void)shuffleVal;
    (void)pauseFlag;
    return (media != NULL) ? MC_ERR_INVALID_STATE : MC_ERR_INVALID_ARG;
}
int PlaybackStateBase::onPlay(int playParam)
{
    (void)playParam;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onPause(uint8_t resetPos, int fadeParam)
{
    (void)resetPos;
    (void)fadeParam;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onStop() { return MC_ERR_INVALID_STATE; }
int PlaybackStateBase::onSeek(const uint32_t *seekParams)
{
    (void)seekParams;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetRepeat(int repeatVal)
{
    (void)repeatVal;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetShuffle(int shuffleVal)
{
    (void)shuffleVal;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetVolume(int vol, int ramp)
{
    (void)vol;
    (void)ramp;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetEqualizer(int eqType, int bandLow, int bandMid)
{
    (void)eqType;
    (void)bandLow;
    (void)bandMid;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetAudioNormalizer(int mode)
{
    (void)mode;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSuspendForDb(uint8_t suspendFlag)
{
    (void)suspendFlag;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onCancel() { return MC_ERR_INVALID_STATE; }
int PlaybackStateBase::onSeekAndPlay(const uint32_t *seekParams, int playParam)
{
    (void)seekParams;
    (void)playParam;
    return MC_ERR_INVALID_STATE;
}
int PlaybackStateBase::onSetMediaPlaying(MediaListAccess *media, int shuffleVal)
{
    (void)media;
    (void)shuffleVal;
    return MC_ERR_INVALID_STATE;
}
/* 0x811FCF8E */
int PlaybackStateBase::onTrackInfo()
{
    return (m_owner != NULL) ? 0 : MC_ERR_FAILED;
}
int PlaybackStateBase::onAudioError(int errCode)
{
    (void)errCode;
    return 0;
}
int PlaybackStateBase::onTrackReady() { return 0; }
int PlaybackStateBase::onBufferReady() { return 0; }
int PlaybackStateBase::onStreamEnd() { return 0; }
int PlaybackStateBase::onAudioPortEvent(uint8_t portFlag)
{
    (void)portFlag;
    return 0;
}
/* 0x811FCFA0 */
int PlaybackStateBase::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    (void)cmdCode;
    *outWaitCode = WAIT_NONE;
    return 0;
}
/* 0x811FCFAA */
int PlaybackStateBase::getCurrentPosMs()
{
    return (m_owner != NULL) ? m_owner->m_currentPosMs : 0;
}
void PlaybackStateBase::setCachedPosMs(int posMs)
{
    (void)posMs;
}

/* ===== StateStarting (stateId = 3, 0x811FBEEC - 0x811FC30F) ===== */

/* 0x811FBEEC */
StateStarting::StateStarting(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_STARTING, outErr)
{
}

/* 0x811FBF08 / 0x811FBF24 */
StateStarting::~StateStarting()
{
}

/* 0x811FBF44 */
int StateStarting::onEnter()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (m_owner->m_mediaListStatus.trackOrderIndex == -2) {
        return MC_ERR_LIST_EMPTY;
    }

    if (m_owner->m_needDecoderRestart == 1u) {
        m_owner->m_needDecoderRestart = 0;
        int closeRet = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->closeCodec();
        if (closeRet != 0) {
            return closeRet;
        }

        int ioMode = 0;
        if (m_owner->m_activeTrackInfo.sourceKind == 2) {
            ioMode = 2;
        } else if (m_owner->m_activeTrackInfo.sourceKind == 16) {
            ioMode = 3;
        } else {
            return MC_ERR_BAD_SOURCE;
        }

        int pipelineKind = -1;
        switch (m_owner->m_activeTrackInfo.codecType) {
        case 4:
            pipelineKind = 4;
            break;
        case 7:
            pipelineKind = 1;
            break;
        case 10:
            pipelineKind = 0;
            break;
        case 13:
            pipelineKind = 6;
            break;
        default:
            return MC_ERR_BAD_CODEC;
        }

        int openRet = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->openCodec(pipelineKind, ioMode, m_owner->m_activeTrackInfo.loopCount);
        if (openRet != 0) {
            return openRet;
        }
    }

    uint8_t rampPending = 0;
    static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
        0xFFFFFFFFu, m_owner->m_playParam, &rampPending);
    m_owner->m_playParam = -1;

    int startPos = getCurrentPosMs();
    int endPos = m_owner->m_nextTrackPos;
    if (m_owner->m_volumeMode < 0) {
        startPos = m_owner->m_prevTrackPos;
        endPos = getCurrentPosMs();
    }

    if (m_owner->m_decoderStartCfg != NULL) {
        DecoderStartParams *cfg = m_owner->m_decoderStartCfg;
        cfg->trackId = m_owner->m_activeTrackInfo.trackId;
        cfg->trackItemId = m_owner->m_activeTrackInfo.trackItemId;
        cfg->trackOrderIndex = m_owner->m_activeMedia->getTrackOrderIndex(
            m_owner->m_mediaListStatus.trackOrderIndex);
        cfg->startPosMs = startPos;
        cfg->resumePosMs = startPos;
        cfg->endPosMs = endPos;
        cfg->durationMs = m_owner->m_activeTrackInfo.durationMs;
        cfg->sourceKind = m_owner->m_activeTrackInfo.sourceKind;
        cfg->sampleRate = m_owner->m_activeTrackInfo.sampleRate;
        cfg->codecFormat = m_owner->m_activeTrackInfo.codecFormat;
        cfg->bitRate = m_owner->m_activeTrackInfo.bitRate;
        cfg->channels = m_owner->m_activeTrackInfo.channels;
        cfg->loopCount = m_owner->m_activeTrackInfo.loopCount;
        cfg->speedMode = m_owner->m_volumeMode;
    }

    if (m_owner->m_activeTrackInfo.mediaInfo != NULL) {
        m_owner->m_activeTrackInfo.mediaInfoShared = 1;
    }

    int startRet = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->startMedia(static_cast<MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo), startPos, endPos, m_owner->m_volumeMode, m_owner->m_decoderStartCfg);
    if (startRet != 0) {
        return startRet;
    }

    if (m_owner->m_hasPendingError == 0) {
        ++m_owner->m_activeTrackInfo.playAttemptCount;
        trackInfoResetCounters(&m_owner->m_activeTrackInfo);
        ++m_owner->m_activeStartCount;
    }
    ++m_owner->m_hasPendingError;
    return 0;
}

/* 0x811FC126 */
int StateStarting::onPlay(int playParam)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_targetState = STATE_PLAYING;
    if (playParam != -1) {
        m_owner->m_playParam = playParam;
    }
    uint8_t rampPending = 0;
    static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
        0xFFFFFFFFu, m_owner->m_playParam, &rampPending);
    m_owner->m_playParam = -1;
    return 0;
}

/* 0x811FC188 */
int StateStarting::onPause(uint8_t resetPos, int fadeParam)
{
    (void)resetPos;
    (void)fadeParam;
    return MC_ERR_BUSY;
}

/* 0x811FC190 */
int StateStarting::onStop()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_targetState = STATE_PAUSED;
    return 0;
}

/* 0x811FC1A8 */
int StateStarting::onSetShuffle(int shuffleVal)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    if (m_owner->m_activeMedia != NULL) {
        uint8_t locked = 0;
        m_owner->m_activeMedia->lockList(m_owner->m_shuffleSeed,
                                         m_owner->m_activeTrackInfo.trackId, &locked, 0);
        m_owner->m_activeMedia->getStatus(&m_owner->m_mediaListStatus);
    }
    return 0;
}

/* 0x811FC224 */
int StateStarting::onSetEqualizer(int eqType, int bandLow, int bandMid)
{
    (void)bandLow;
    (void)bandMid;
    if (m_owner == NULL || m_owner->m_audioOut == NULL) {
        return MC_ERR_FAILED;
    }
    int ret = static_cast<AudioOutput *>(m_owner->m_audioOut)->setEffectType(eqType);
    if (ret == 0) {
        m_owner->m_eqType = eqType;
    }
    return ret;
}

/* 0x811FC266 */
int StateStarting::onTrackInfo()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(m_owner->m_targetState);
}

/* 0x811FC294 */
int StateStarting::onTrackReady()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPED);
}

/* 0x811FC2AA */
int StateStarting::onAudioPortEvent(uint8_t portFlag)
{
    if (portFlag == 1u && m_owner->m_targetState == STATE_STOPPED) {
        return MC_ERR_BUSY;
    }
    return 0;
}

/* 0x811FC2C8 */
int StateStarting::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    if (cmdCode == EVT_TRACK_READY) {
        *outWaitCode = WAIT_START_DONE;
        return 1;
    }
    switch (cmdCode) {
    case CMD_PLAY:
    case CMD_STOP:
    case CMD_SEEK:
    case CMD_SET_MEDIA:
    case CMD_SET_SHUFFLE:
    case CMD_SET_MEDIA_PLAY:
    case CMD_SUSPEND_DB:
        *outWaitCode = WAIT_START_DONE;
        return 1;
    default:
        return 0;
    }
}

/* ===== StateStopping (stateId = 6, 0x811FC310 - 0x811FCA7F) ===== */

/* 0x811FC310 */
StateStopping::StateStopping(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_STOPPING, outErr)
{
}

/* 0x811FC32C / 0x811FC348 */
StateStopping::~StateStopping()
{
}

/* 0x811FC368 */
int StateStopping::onEnter()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    return static_cast<DecoderManager *>(m_owner->m_decoderMgr)->stopMedia();
}

/* 0x811FC37E */
int StateStopping::onDeactivate(int fadeMs, uint8_t syncFlag)
{
    (void)fadeMs;
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_deactivateRequested = 1;
    if (syncFlag == 1u) {
        m_owner->m_stopRequested = 1;
    }
    return 0;
}

/* 0x811FC3A0 */
int StateStopping::onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (media == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->setPendingMedia(reinterpret_cast<MediaListInterface *>(media));
    if (ret == 0 && pauseFlag == 1u) {
        m_owner->m_targetState = STATE_STOPPED;
        m_owner->m_stopRequested = 1;
    }
    return ret;
}

/* 0x811FC3DE */
int StateStopping::onStop()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_targetState = STATE_STOPPED;
    m_owner->m_playParam = -1;
    m_owner->m_stopRequested = 1;
    return 0;
}

/* 0x811FC402 */
int StateStopping::onSeek(const uint32_t *seekParams)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }

    uint32_t localParams[5];
    sce_paf_memcpy(localParams, seekParams, sizeof(localParams));
    if (localParams[1] != 0x80000000u && (int)localParams[1] == m_owner->m_pendingVolumeMode) {
        localParams[1] = 0x80000000u;
    }

    MediaListAccess *targetMedia = m_owner->m_pendingMedia;
    if (targetMedia == NULL) {
        targetMedia = m_owner->m_activeMedia;
    }

    uint8_t hasPending = 0;
    uint8_t listLocked = 0;
    if (targetMedia == m_owner->m_pendingMedia) {
        targetMedia->lockList(m_owner->m_shuffleSeed, m_owner->m_activeTrackInfo.trackId,
                              &listLocked, 0);
    }

    int ret = reinterpret_cast<StateMachine *>(m_owner)->seekPendingMedia(localParams, reinterpret_cast<MediaListInterface *>(targetMedia), &hasPending, 0);
    if (listLocked == 1u) {
        targetMedia->unlockList(0);
    }
    return ret;
}

/* 0x811FC4BC */
int StateStopping::onSetRepeat(int repeatVal)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_repeatMode = repeatVal;
    return 0;
}

/* 0x811FC4D4 */
int StateStopping::onSuspendForDb(uint8_t suspendFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    MediaListAccess *targetMedia = m_owner->m_pendingMedia;
    if (targetMedia == NULL) {
        targetMedia = m_owner->m_activeMedia;
    }
    if (targetMedia == NULL) {
        return MC_ERR_NO_MEDIA;
    }

    uint8_t listLocked = 0;
    if (targetMedia == m_owner->m_pendingMedia) {
        targetMedia->lockList(m_owner->m_shuffleSeed, m_owner->m_activeTrackInfo.trackId,
                              &listLocked, 0);
    }

    uint32_t seekParams[5];
    seekParams[0] = (suspendFlag != 0u) ? 0xFFFC0001u : 0xFFFC0000u;
    seekParams[1] = 0x80000000u;
    seekParams[2] = 0xFFFFFFFEu;
    seekParams[3] = 0xFFFFFFFEu;
    seekParams[4] = 0xFFFFFFFFu;

    uint8_t hasPending = 0;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->seekPendingMedia(seekParams, reinterpret_cast<MediaListInterface *>(targetMedia), &hasPending, 0);
    if (listLocked == 1u) {
        targetMedia->unlockList(0);
    }
    if (ret == MC_ERR_DB_REMOVED) {
        m_owner->m_lastDbErr = MC_ERR_DB_REMOVED;
        m_owner->m_dbRemovedError = 1;
        return 0;
    }
    if (ret != 0) {
        m_owner->m_seekFailed = 1;
    }
    return ret;
}

/* 0x811FC5CC */
int StateStopping::onAudioError(int errCode)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }

    int nextState = STATE_STOPPING;
    m_owner->m_hasPendingError = 0;

    if (m_owner->m_pendingMedia != NULL) {
        int swapRet = reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(m_owner->m_pendingMedia), 1, 0, 1);
        if (swapRet != 0) {
            void *oldMedia = m_owner->m_pendingMedia;
            m_owner->onMediaChanged(0, NULL, 0, NULL, &oldMedia, 1);
            m_owner->m_pendingMedia = NULL;
        }
        nextState = STATE_STARTING;
    }

    uint8_t hadPendingTrack = m_owner->m_hasPendingTrack;
    bool commitFailed = false;
    if (hadPendingTrack == 1u) {
        m_owner->m_hasPendingTrack = 0;
        m_owner->m_activeMedia->getStatus(&m_owner->m_mediaListStatus);
        if (trackInfoMoveTo(&m_owner->m_pendingTrackInfo, &m_owner->m_activeTrackInfo, 1) != 0) {
            trackInfoReleaseResources(&m_owner->m_pendingTrackInfo, 1);
            m_owner->m_hasPendingError = 2;
            nextState = STATE_STOPPED;
            commitFailed = true;
        } else {
            m_owner->m_lastErrorType = 0;
            TrackInfoData metaCopy = m_owner->m_activeTrackInfo;
            reinterpret_cast<StateMachine *>(m_owner)->populateMetadataSafe(&metaCopy, reinterpret_cast<TrackMetadataBuffers *>(m_owner->m_metaBufs));
            nextState = STATE_STARTING;
        }
    }

    if (!commitFailed) {
        if (m_owner->m_speedDirChanged == 1u) {
            nextState = STATE_STARTING;
            m_owner->m_volumeMode = m_owner->m_pendingVolumeMode;
        }

        if (hadPendingTrack == 1u || m_owner->m_pendingSeekStep != -2) {
            m_owner->m_currentPosMs = getCurrentPosMs();
            computePlaybackRange(m_owner->m_seekMode, m_owner->m_speedMode,
                                 m_owner->m_pendingSeekStep, m_owner->m_activeTrackInfo.durationMs,
                                 m_owner->m_volumeMode, &m_owner->m_prevTrackPos,
                                 &m_owner->m_nextTrackPos, &m_owner->m_currentPosMs);
            nextState = STATE_STARTING;
            m_owner->m_cachedPosMs = m_owner->m_currentPosMs;
        }

        if (m_owner->m_dbRemovedError == 1u && m_owner->m_lastDbErr == MC_ERR_DB_REMOVED
            && m_owner->m_pendingMedia == NULL) {
            reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(NULL), 0, 0, 0);
        }

        if (m_owner->m_deactivateRequested == 1u) {
            if (m_owner->m_stopRequested == 1u || errCode != 0) {
                m_owner->m_targetState = STATE_STOPPED;
            }
            m_owner->m_hasPendingError = 5;
            nextState = STATE_DEACTIVATING;
        } else if (m_owner->m_stopRequested == 1u) {
            m_owner->m_hasPendingError = 3;
            nextState = STATE_STOPPED;
        } else if (m_owner->m_dbRemovedError == 1u) {
            int dbErr = m_owner->m_lastDbErr;
            if (dbErr == MC_ERR_DB_REMOVED && m_owner->m_pendingMedia == NULL) {
                m_owner->m_extraErr = dbErr;
                nextState = STATE_READY;
                m_owner->m_hasPendingError = 6;
            } else {
                if (dbErr != 0) {
                    m_owner->m_extraErr = dbErr;
                }
                nextState = STATE_STOPPED;
            }
        } else if (m_owner->m_seekFailed == 1u) {
            m_owner->m_extraErr = m_owner->m_lastDbErr;
            nextState = STATE_STOPPED;
            m_owner->m_hasPendingError = 2;
        } else if (errCode != 0) {
            m_owner->m_extraErr = errCode;
            nextState = STATE_STOPPED;
            m_owner->m_hasPendingError = 2;
        } else if (nextState == STATE_STOPPING) {
            nextState = STATE_STOPPED;
            m_owner->m_hasPendingError = 6;
        } else {
            int sameFmt = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->checkMediaCompat(static_cast<const MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo));
            m_owner->m_needDecoderRestart = (sameFmt != 1) ? 1u : 0u;
        }
    }

    int extraSeekErr = (m_owner->m_extraSeekErr != 0) ? m_owner->m_extraSeekErr : 0;
    if (m_owner->m_hasPendingTrack == 1u) {
        trackInfoReleaseResources(&m_owner->m_pendingTrackInfo, 0);
    }

    m_owner->m_deactivateRequested = 0;
    m_owner->m_stopRequested = 0;
    m_owner->m_hasPendingTrack = 0;
    m_owner->m_pendingMedia = NULL;
    m_owner->m_dbRemovedError = 0;
    m_owner->m_speedDirChanged = 0;
    m_owner->m_seekFailed = 0;
    m_owner->m_pendingSeekStep = -2;
    m_owner->m_pendingVolumeMode = (int)0x80000000u;
    m_owner->m_lastDbErr = 0;
    m_owner->m_extraSeekErr = 0;

    int transRet = reinterpret_cast<StateMachine *>(m_owner)->transitionTo(nextState);
    return (transRet == 0) ? extraSeekErr : transRet;
}

/* 0x811FCA42 */
int StateStopping::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    if (cmdCode == CMD_SEEK || cmdCode == CMD_SEEK_AND_PLAY
        || cmdCode == CMD_SET_MEDIA_PLAY || cmdCode == CMD_SUSPEND_DB) {
        *outWaitCode = WAIT_STOP_DONE;
        return 1;
    }
    return 0;
}

/* ===== StateDeactivating (stateId = 8, 0x811FCA80 - 0x811FCB4F) ===== */

/* 0x811FCA80 */
StateDeactivating::StateDeactivating(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_DEACTIVATING, outErr)
{
}

/* 0x811FCA9C / 0x811FCAB8 */
StateDeactivating::~StateDeactivating()
{
}

/* 0x811FCAD8 */
int StateDeactivating::onEnter()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (m_owner->m_audioOut != NULL) {
        int ret = static_cast<AudioOutput *>(m_owner->m_audioOut)->closePort();
        if (ret != 0) {
            return ret;
        }
    }
    int ret = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->closeCodec();
    if (ret == 0) {
        ret = m_owner->onDeactivatedHook(m_owner->m_activateToken);
        if (ret == 0) {
            m_owner->m_activateToken = NULL;
        }
    }
    return ret;
}

/* 0x811FCB20 */
int StateDeactivating::onBufferReady()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_IDLE);
}

/* 0x811FCB36 */
int StateDeactivating::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    if (cmdCode == CMD_DEACTIVATE) {
        *outWaitCode = WAIT_DEACTIVATE_DONE;
        return 1;
    }
    return 0;
}

/* ===== StateActivating (stateId = 1, 0x811FCB50 - 0x811FCD47) ===== */

/* 0x811FCB50 */
StateActivating::StateActivating(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_ACTIVATING, outErr)
{
}

/* 0x811FCB6C / 0x811FCB88 */
StateActivating::~StateActivating()
{
}

/* 0x811FCBA8 */
int StateActivating::onEnter()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (m_owner->m_audioOut != NULL) {
        AudioEqParams eq;
        eq.eqType = m_owner->m_eqType;
        eq.bandLow = m_owner->m_eqBandLow;
        eq.bandMid = m_owner->m_eqBandMid;
        eq.bandHigh = m_owner->m_eqBandHigh;
        int ret = static_cast<AudioOutput *>(m_owner->m_audioOut)->openPort(
            static_cast<unsigned int>(m_owner->m_audioBalance),
            reinterpret_cast<const AudioEqParam *>(&eq),
            static_cast<unsigned int>(m_owner->m_audioNormalizer));
        if (ret != 0) {
            return ret;
        }
        uint8_t fadePending = 0;
        static_cast<AudioOutput *>(m_owner->m_audioOut)->setScaleFade(
            -1, m_owner->m_pendingFadeMs, &fadePending);
        m_owner->m_pendingFadeMs = -1;
    }
    m_owner->m_activateToken = NULL;
    return m_owner->onActivatedHook();
}

/* 0x811FCC3C */
int StateActivating::onSetShuffle(int shuffleVal)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    return 0;
}

/* 0x811FCC52 */
int StateActivating::onStreamEnd()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    int nextState = STATE_READY;
    if (m_owner->m_activeMedia != NULL) {
        uint8_t locked = 0;
        m_owner->m_activeMedia->lockList(m_owner->m_shuffleSeed,
                                         m_owner->m_activeTrackInfo.trackId, &locked, 0);
        m_owner->m_activeMedia->getStatus(&m_owner->m_mediaListStatus);
        if (m_owner->m_targetState == STATE_STOPPED) {
            nextState = STATE_STOPPED;
        } else if (m_owner->m_targetState == STATE_PLAYING || m_owner->m_targetState == STATE_PAUSED) {
            m_owner->m_needDecoderRestart = 1;
            nextState = STATE_STARTING;
        } else {
            return MC_ERR_FAILED;
        }
    }
    reinterpret_cast<StateMachine *>(m_owner)->transitionTo(nextState);
    return 0;
}

/* 0x811FCD16 */
int StateActivating::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    switch (cmdCode) {
    case CMD_PLAY:
    case CMD_STOP:
    case CMD_PAUSE:
    case CMD_SEEK:
    case CMD_SEEK_AND_PLAY:
    case CMD_SET_MEDIA:
    case CMD_SET_SHUFFLE:
    case CMD_SET_MEDIA_PLAY:
    case CMD_SUSPEND_DB:
        *outWaitCode = WAIT_ACTIVATE_DONE;
        return 1;
    default:
        return 0;
    }
}

/* ===== StateReady (stateId = 2, 0x811FCD48 - 0x811FCF13) ===== */

/* 0x811FCD48 */
StateReady::StateReady(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_READY, outErr)
{
}

/* 0x811FCD64 / 0x811FCD80 */
StateReady::~StateReady()
{
}

/* 0x811FCDA2 */
int StateReady::onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (media == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    reinterpret_cast<StateMachine *>(m_owner)->setPendingMedia(reinterpret_cast<MediaListInterface *>(media));
    int ret = reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(media), 1, 1, 1);
    if (ret != 0) {
        return ret;
    }

    int nextState = STATE_STOPPED;
    if (pauseFlag == 1u) {
        m_owner->m_targetState = STATE_STOPPED;
        m_owner->m_needDecoderRestart = 1;
        nextState = STATE_STOPPED;
    } else if (m_owner->m_targetState == STATE_STOPPED) {
        m_owner->m_needDecoderRestart = 1;
        nextState = STATE_STOPPED;
    } else if (m_owner->m_targetState == STATE_PLAYING || m_owner->m_targetState == STATE_PAUSED) {
        m_owner->m_needDecoderRestart = 1;
        nextState = STATE_STARTING;
    } else {
        return MC_ERR_FAILED;
    }

    reinterpret_cast<StateMachine *>(m_owner)->transitionTo(nextState);
    return 0;
}

/* 0x811FCE3A */
int StateReady::onPlay(int playParam)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_playParam = playParam;
    m_owner->m_targetState = STATE_PLAYING;
    return 0;
}

/* 0x811FCE56 */
int StateReady::onPause(uint8_t resetPos, int fadeParam)
{
    (void)fadeParam;
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_targetState = STATE_STOPPED;
    m_owner->m_playParam = -1;
    if (resetPos == 1u) {
        m_owner->m_currentPosMs = 0;
    }
    return 0;
}

/* 0x811FCE82 */
int StateReady::onSeekAndPlay(const uint32_t *seekParams, int playParam)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    int ret = reinterpret_cast<StateMachine *>(m_owner)->seekActiveMedia(seekParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), 0);
    if (ret == 0) {
        m_owner->m_targetState = STATE_PLAYING;
        m_owner->m_playParam = playParam;
    }
    return ret;
}

/* 0x811FCEB2 */
int StateReady::onSetMediaPlaying(MediaListAccess *media, int shuffleVal)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (media == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    reinterpret_cast<StateMachine *>(m_owner)->setPendingMedia(reinterpret_cast<MediaListInterface *>(media));
    int ret = reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(media), 1, 1, 1);
    if (ret == 0) {
        m_owner->m_targetState = STATE_PLAYING;
        m_owner->m_playParam = 0;
        m_owner->m_needDecoderRestart = 1;
        reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STARTING);
    }
    return ret;
}

/* ===== StateIdle (stateId = 0, 0x811FCFBC - 0x811FD517) ===== */

/* 0x811FCFBC */
StateIdle::StateIdle(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_IDLE, outErr)
{
}

/* 0x811FCFD8 / 0x811FCFF4 */
StateIdle::~StateIdle()
{
}

/* 0x811FD014 */
int StateIdle::onEnter()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }

    m_owner->m_deactivateRequested = 0;
    m_owner->m_stopRequested = 0;
    m_owner->m_hasPendingTrack = 0;
    m_owner->m_dbRemovedError = 0;
    m_owner->m_speedDirChanged = 0;
    m_owner->m_seekFailed = 0;
    m_owner->m_pendingMedia = NULL;
    m_owner->m_pendingSeekStep = -2;
    m_owner->m_pendingVolumeMode = (int)0x80000000u;

    sce_paf_memset(&m_owner->m_pendingTrackInfo, 0, sizeof(TrackInfoData));
    m_owner->m_pendingTrackInfo.contentKind = 2;
    m_owner->m_pendingTrackInfo.playlistId = -1;
    m_owner->m_pendingTrackInfo.trackId = -1;
    m_owner->m_pendingTrackInfo.trackItemId = -1;
    m_owner->m_pendingTrackInfo.sourceKind = -1;
    m_owner->m_pendingTrackInfo.slotSerial = -1;

    m_owner->m_lastDbErr = 0;
    m_owner->m_extraSeekErr = 0;
    m_owner->m_targetState = STATE_STOPPED;
    m_owner->m_pendingFadeMs = -1;
    m_owner->m_playParam = -1;
    m_owner->m_needDecoderRestart = 1;

    trackInfoReleaseResources(&m_owner->m_activeTrackInfo, 0);
    m_owner->m_activeMedia = NULL;
    m_owner->m_currentPosMs = -1;
    m_owner->m_prevTrackPos = -1;
    m_owner->m_nextTrackPos = -1;

    m_owner->m_mediaListStatus.playState = 0;
    m_owner->m_mediaListStatus.trackOrderIndex = -2;
    m_owner->m_mediaListStatus.currentTrackId = -1;
    m_owner->m_mediaListStatus.prevTrackIdx = -2;
    m_owner->m_mediaListStatus.nextTrackIdx = 0;
    m_owner->m_mediaListStatus.listMode = -1;
    m_owner->m_mediaListStatus.listFlags = -1;

    m_owner->m_repeatMode = 0;
    m_owner->m_shuffleSeed = 0;
    m_owner->m_seekMode = -1;
    m_owner->m_speedMode = -1;
    m_owner->m_volumeMode = 1;
    m_owner->m_audioBalance = 0x8000;
    m_owner->m_eqType = 0;
    m_owner->m_eqBandLow = 0;
    m_owner->m_eqBandMid = 0;
    m_owner->m_eqBandHigh = 0;
    m_owner->m_audioNormalizer = 0;
    m_owner->m_hasPendingError = 0;
    m_owner->m_lastErrorType = 0;
    m_owner->m_extraErr = 0;
    return 0;
}

/* 0x811FD306 */
int StateIdle::onActivate(const ActivateStateSnapshot *snap, int fadeMs)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }

    if (snap != NULL) {
        m_owner->m_repeatMode = snap->repeatMode;
        m_owner->m_shuffleSeed = snap->shuffleMode;
        m_owner->m_audioBalance = snap->audioBalance;
        m_owner->m_eqType = snap->eqType;
        m_owner->m_eqBandLow = snap->eqBandLow;
        m_owner->m_eqBandMid = snap->eqBandMid;
        m_owner->m_eqBandHigh = snap->eqBandHigh;
        m_owner->m_audioNormalizer = snap->audioNormalizer;
        m_owner->m_lastErrorType = snap->lastErrorType;
        m_owner->m_hasPendingError = snap->hasPendingError;
        m_owner->m_extraErr = snap->extraErr;
        m_owner->m_seekMode = snap->seekMode;
        m_owner->m_speedMode = snap->speedMode;
        m_owner->m_targetState = snap->targetState;
        m_owner->m_pendingFadeMs = snap->pendingFadeMs;
        m_owner->m_playParam = snap->playParam;

        uint32_t seekParams[5];
        seekParams[0] = (uint32_t)snap->trackOrderIndex;
        seekParams[1] = (uint32_t)snap->volumeMode;
        seekParams[2] = (uint32_t)m_owner->m_seekMode;
        seekParams[3] = (uint32_t)m_owner->m_speedMode;
        seekParams[4] = (uint32_t)snap->positionMs;

        MediaListAccess *media = (MediaListAccess *)snap->mediaHandle;
        if (media != NULL) {
            uint8_t listLocked = 0;
            int64_t trackKey = media->getCurrentTrackKey();
            media->lockList(snap->shuffleMode, trackKey, &listLocked, 0);

            int seekRet = reinterpret_cast<StateMachine *>(m_owner)->seekActiveMedia(seekParams, reinterpret_cast<MediaListInterface *>(media), 0);
            if (listLocked == 1u) {
                media->unlockList(0);
            }

            if (seekRet == 0) {
                seekRet = reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(media), 0, 0, 0);
                if (seekRet != 0) {
                    void *oldMedia = media;
                    m_owner->onMediaChanged(0, NULL, 0, NULL, &oldMedia, 0);
                }
            } else {
                int swapRet = reinterpret_cast<StateMachine *>(m_owner)->swapActiveMedia(reinterpret_cast<MediaListInterface *>(media), 0, 0, 0);
                if (swapRet != 0) {
                    void *oldMedia = media;
                    m_owner->onMediaChanged(0, NULL, 0, NULL, &oldMedia, 0);
                } else {
                    m_owner->m_hasPendingError = 2;
                    m_owner->m_extraErr = seekRet;
                    m_owner->m_targetState = STATE_STOPPED;
                }
            }
        }
    }

    if (fadeMs != -1) {
        m_owner->m_pendingFadeMs = fadeMs;
    }
    return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_ACTIVATING);
}

/* ===== StatePlaying (stateId = 4, 0x811FD518 - 0x811FDB5B) ===== */

/* 0x811FD518 */
StatePlaying::StatePlaying(StateMachineContext *owner, int *outErr)
    : PlaybackStateBase(owner, STATE_PLAYING, outErr)
{
}

/* 0x811FD534 / 0x811FD550 */
StatePlaying::~StatePlaying()
{
}

/* 0x811FD570 */
int StatePlaying::onDeactivate(int fadeMs, uint8_t syncFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_deactivateRequested = 1;
    if (syncFlag == 1u) {
        m_owner->m_stopRequested = 1;
    }
    if (fadeMs != -1) {
        m_owner->m_pendingFadeMs = fadeMs;
    }
    uint8_t fadePending = 0;
    static_cast<AudioOutput *>(m_owner->m_audioOut)->setScaleFade(
        -2, m_owner->m_pendingFadeMs, &fadePending);
    int ret = 0;
    if (fadePending != 1u) {
        ret = reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    m_owner->m_pendingFadeMs = -1;
    return ret;
}

/* 0x811FD5F6 */
int StatePlaying::onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (media == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->setPendingMedia(reinterpret_cast<MediaListInterface *>(media));
    if (ret == 0) {
        if (pauseFlag == 1u) {
            m_owner->m_targetState = STATE_STOPPED;
            m_owner->m_stopRequested = 1;
        }
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    return ret;
}

/* 0x811FD63C */
int StatePlaying::onPause(uint8_t resetPos, int fadeParam)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_stopRequested = 1;
    if (resetPos == 1u) {
        m_owner->m_pendingSeekStep = 0;
    }
    m_owner->m_targetState = STATE_STOPPED;
    if (fadeParam != -1) {
        m_owner->m_playParam = fadeParam;
    }
    uint8_t rampPending = 0;
    static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
        0xFFFFFFFEu, m_owner->m_playParam, &rampPending);
    int ret = 0;
    if (rampPending != 1u) {
        ret = reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    m_owner->m_playParam = -1;
    return ret;
}

/* 0x811FD6C6 */
int StatePlaying::onSeek(const uint32_t *seekParams)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    uint32_t localParams[5];
    sce_paf_memcpy(localParams, seekParams, sizeof(localParams));
    if (localParams[1] != 0x80000000u && (int)localParams[1] == m_owner->m_volumeMode) {
        localParams[1] = 0x80000000u;
    }

    uint8_t needStop = 0;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->seekPendingMedia(localParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), &needStop, 0);
    if (ret != 0) {
        return ret;
    }
    if (needStop == 1u) {
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    if (localParams[1] != 0x80000000u) {
        ret = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->seekMedia((m_owner->m_volumeMode < 0) ? 1 : 0, 0);
        if (ret != 0) {
            return ret;
        }
    }

    m_owner->m_currentPosMs = getCurrentPosMs();
    computePlaybackRange(m_owner->m_seekMode, m_owner->m_speedMode,
                         m_owner->m_pendingSeekStep, m_owner->m_activeTrackInfo.durationMs,
                         m_owner->m_volumeMode, &m_owner->m_prevTrackPos,
                         &m_owner->m_nextTrackPos, &m_owner->m_currentPosMs);
    m_owner->m_pendingSeekStep = -2;
    m_owner->m_cachedPosMs = m_owner->m_currentPosMs;
    return 0;
}

/* 0x811FD7C0 */
int StatePlaying::onCancel()
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    return MC_ERR_INVALID_STATE;
}

/* 0x811FD7D4 */
int StatePlaying::onSuspendForDb(uint8_t suspendFlag)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (m_owner->m_activeMedia == NULL) {
        return MC_ERR_NO_MEDIA;
    }

    uint32_t seekParams[5];
    seekParams[0] = (suspendFlag != 0u) ? 0xFFFC0001u : 0xFFFC0000u;
    seekParams[1] = 0x80000000u;
    seekParams[2] = 0xFFFFFFFEu;
    seekParams[3] = 0xFFFFFFFEu;
    seekParams[4] = 0xFFFFFFFFu;

    uint8_t needStop = 0;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->seekPendingMedia(seekParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), &needStop, 0);
    if (ret == MC_ERR_DB_REMOVED) {
        m_owner->m_dbRemovedError = 1;
        m_owner->m_lastDbErr = MC_ERR_DB_REMOVED;
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    if (ret != 0) {
        m_owner->m_seekFailed = 1;
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    if (needStop == 1u) {
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    return 0;
}

/* 0x811FD88E */
int StatePlaying::onSeekAndPlay(const uint32_t *seekParams, int playParam)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    int ret = onSeek(seekParams);
    if (ret == 0) {
        m_owner->m_targetState = STATE_PLAYING;
        if (playParam != -1) {
            m_owner->m_playParam = playParam;
        }
        uint8_t rampPending = 0;
        static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
        0xFFFFFFFFu, m_owner->m_playParam, &rampPending);
        m_owner->m_playParam = -1;
    }
    return ret;
}

/* 0x811FD904 */
int StatePlaying::onSetMediaPlaying(MediaListAccess *media, int shuffleVal)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    if (media == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    m_owner->m_shuffleSeed = shuffleVal;
    int ret = reinterpret_cast<StateMachine *>(m_owner)->setPendingMedia(reinterpret_cast<MediaListInterface *>(media));
    if (ret == 0) {
        m_owner->m_targetState = STATE_PLAYING;
        m_owner->m_playParam = 0;
        uint8_t rampPending = 0;
        static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
            0xFFFFFFFEu, 0, &rampPending);
        if (rampPending != 1u) {
            return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
        }
    }
    return ret;
}

/* 0x811FD980 */
int StatePlaying::onAudioError(int errCode)
{
    if (m_owner == NULL) {
        return MC_ERR_FAILED;
    }
    m_owner->m_hasPendingError = 0;

    if (errCode != 0) {
        int contentKind = m_owner->m_activeTrackInfo.contentKind;
        if (errCode == MC_ERR_LIST_EMPTY || errCode == MC_ERR_BAD_SOURCE || errCode == -1879113717) {
            contentKind = 1;
        } else if (errCode == MC_ERR_BAD_CODEC) {
            contentKind = 7;
        }
        m_owner->m_extraErr = errCode;
        if (contentKind == 2) {
            m_owner->m_hasPendingError = 2;
            m_owner->m_targetState = STATE_STOPPED;
            return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPED);
        }
        m_owner->m_activeTrackInfo.contentKind = contentKind;
    }

    uint32_t seekParams[5];
    seekParams[0] = 0xFFFFFFFEu;
    seekParams[1] = 0x80000000u;
    seekParams[2] = (uint32_t)m_owner->m_seekMode;
    seekParams[3] = (uint32_t)m_owner->m_speedMode;
    seekParams[4] = 0xFFFFFFFFu;

    int seekRet = reinterpret_cast<StateMachine *>(m_owner)->seekActiveMedia(seekParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), 0);
    int nextState = STATE_STOPPED;
    if (seekRet == 0) {
        int sameFmt = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->checkMediaCompat(static_cast<const MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo));
        m_owner->m_needDecoderRestart = (sameFmt != 1) ? 1u : 0u;
        nextState = STATE_STARTING;
    } else if (seekRet == MC_ERR_END_FWD) {
        m_owner->m_hasPendingError = 6;
        clearRejectedIdList();
        seekParams[0] = 0xFFFFFFFBu;
        reinterpret_cast<StateMachine *>(m_owner)->seekActiveMedia(seekParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), 0);
        m_owner->m_targetState = STATE_STOPPED;
        seekRet = 0;
        nextState = STATE_STOPPED;
    } else if (seekRet == MC_ERR_END_REV) {
        m_owner->m_hasPendingError = 7;
        clearRejectedIdList();
        seekParams[0] = 0xFFFFFFFBu;
        reinterpret_cast<StateMachine *>(m_owner)->seekActiveMedia(seekParams, reinterpret_cast<MediaListInterface *>(m_owner->m_activeMedia), 0);
        m_owner->m_targetState = STATE_STOPPED;
        seekRet = 0;
        nextState = STATE_STOPPED;
    } else {
        m_owner->m_hasPendingError = 2;
        m_owner->m_targetState = STATE_STOPPED;
        nextState = STATE_STOPPED;
    }

    int transRet = reinterpret_cast<StateMachine *>(m_owner)->transitionTo(nextState);
    return (transRet != 0) ? transRet : seekRet;
}

/* 0x811FDB10 */
int StatePlaying::onAudioPortEvent(uint8_t portFlag)
{
    if (portFlag == 1u
        && (m_owner->m_targetState == STATE_STOPPED || m_owner->m_deactivateRequested == 1u)) {
        return reinterpret_cast<StateMachine *>(m_owner)->transitionTo(STATE_STOPPING);
    }
    return 0;
}

/* 0x811FDB3A */
int StatePlaying::getCurrentPosMs()
{
    if (m_owner == NULL) {
        return 0;
    }
    m_owner->m_currentPosMs = m_owner->m_cachedPosMs;
    return m_owner->m_cachedPosMs;
}

/* 0x811FDB50 */
void StatePlaying::setCachedPosMs(int posMs)
{
    if (m_owner != NULL) {
        m_owner->m_cachedPosMs = posMs;
    }
}

} /* namespace MusicCore */
} /* namespace sce */
