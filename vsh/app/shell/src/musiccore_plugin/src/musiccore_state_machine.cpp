/*
 * SceShell musiccore_plugin - StateMachine and StateStopped (playback state machine & track seek).
 * Original address range (3.65-CEX): 0x811E4670 - 0x811E6D64
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_audio_output.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_media_list.h"
#include "musiccore_state_machine.h"
#include <stdlib.h>

#include <thread/thread.h>

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
void *sce_paf_memcpy(void *dst, const void *src, size_t size);
}

static const int MUSICCORE_ERR_FAILED = -1879113726;      /* 0x8FFF0002 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */
static const int MUSICCORE_ERR_NO_MEDIA = -1879113722;    /* 0x8FFF0006 */
static const int MUSICCORE_ERR_LIST_EMPTY = -1879113720;  /* 0x8FFF0008 */
static const int MUSICCORE_ERR_DB_REMOVED = -1879113715;  /* 0x8FFF000D */
static const int MUSICCORE_ERR_END_FWD = -1879113713;     /* 0x8FFF000F */
static const int MUSICCORE_ERR_END_REV = -1879113712;     /* 0x8FFF0010 */

namespace sce {
namespace MusicCore {

extern void computePlaybackRange(int seekMode, int speedMode, int step, int durationMs,
                                 int volumeMode, int *outPrev, int *outNext, int *outCur);

static unsigned char g_stateTransitionTable[64];

struct DecoderManagerHandle {
    void *vtablePtr;
    int ioPriority;
};

static void resetTrackInfo(TrackInfoData *info)
{
    sce_paf_memset(info, 0, sizeof(*info));
    info->contentKind = 2;
    info->playlistId = -1;
    info->trackId = -1;
    info->trackItemId = -1;
    info->codecFlags = -1;
    info->slotSerial = -1;
}

static void resetMediaListStatus(MediaListStatus *st)
{
    sce_paf_memset(st, 0, sizeof(*st));
    st->trackOrderIndex = -2;
    st->currentTrackId = -1;
    st->prevTrackIdx = -2;
    st->listMode = -1;
    st->listFlags = -1;
}

static void resetSeekParams(unsigned int *seekParams)
{
    seekParams[0] = 0xFFFFFFFEu;
    seekParams[1] = 1u;
    seekParams[2] = 0xFFFFFFFFu;
    seekParams[3] = 0xFFFFFFFFu;
    seekParams[4] = 0xFFFFFFFFu;
}

static int resolveWrapAndDir(unsigned int seekCmd, int repeatMode, int speedMode,
                             int listMode, int *outWrapMode, int *outDir)
{
    if ((seekCmd >> 16) == 0xFFFEu)
        seekCmd = 0xFFFFFFFDu;
    else if ((seekCmd >> 16) == 0xFFFDu)
        seekCmd = 0xFFFFFFFCu;

    if (seekCmd == 0xFFFFFFFDu) {
        *outWrapMode = (repeatMode == 1) ? 2 : 1;
        *outDir = 1;
        return 1;
    }
    if (seekCmd == 0xFFFFFFFCu) {
        *outWrapMode = (repeatMode == 1) ? 2 : 1;
        *outDir = -1;
        return *outWrapMode;
    }
    if (seekCmd == 0xFFFFFFFFu) {
        *outWrapMode = 0;
        *outDir = 0;
        return 0;
    }
    if (seekCmd == 0xFFFFFFFEu) {
        if (listMode != 1 && repeatMode == 2) {
            *outWrapMode = 0;
            *outDir = 0;
            return 0;
        }
        *outDir = (speedMode >= 0) ? 1 : -1;
        *outWrapMode = (repeatMode == 1) ? 2 : 1;
        return *outWrapMode;
    }
    if (*outWrapMode == 0) {
        *outWrapMode = (repeatMode == 1) ? 2 : 1;
        *outDir = (speedMode < 0) ? -1 : 1;
        return *outDir;
    }
    return *outWrapMode;
}

static unsigned int mapSeekCmdToListIndex(unsigned int seekCmd, int repeatMode,
                                          int speedMode, unsigned char isNewMedia)
{
    switch (seekCmd) {
    case 0xFFFFFFFFu:
        return 0xFFFFFFFEu;
    case 0xFFFFFFFEu:
        if (isNewMedia == 1 || repeatMode == 2)
            return 0xFFFFFFFFu;
        return (speedMode >= 0) ? 0xFFFFFFFDu : 0xFFFFFFFCu;
    case 0xFFFFFFFDu:
        return 0xFFFFFFFDu;
    case 0xFFFFFFFCu:
        return 0xFFFFFFFCu;
    case 0xFFFC0000u:
        return 0xFFFFFFFFu;
    default:
        if ((seekCmd >> 16) == 0xFFFEu || (seekCmd >> 16) == 0xFFFDu)
            return 0xFFFFFFFEu;
        return seekCmd;
    }
}

int dbSeekNormalTarget(unsigned int seekCmd, MediaListInterface *media,
                       unsigned char isNewMedia, int speedMode, int repeatMode,
                       int *ioShuffleSeed, int *outStep, void *decMgr,
                       unsigned char *outChanged, TrackInfoData *outTrackInfo,
                       int *outExtraErr, unsigned char allowReshuffle)
{
    if (media == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    MediaListStatus status;
    resetMediaListStatus(&status);
    TrackInfoData candidate;
    resetTrackInfo(&candidate);

    media->getStatus(&status);
    int initialOrderIdx = status.trackOrderIndex;
    int err = 0;
    unsigned int triedCount = 0;

    int stepDir = 0;
    int stepCount = 0;
    if ((seekCmd >> 16) == 0xFFFEu) {
        stepDir = 1;
        stepCount = (int)(seekCmd & 0xFFFFu);
    } else if ((seekCmd >> 16) == 0xFFFDu) {
        stepDir = -1;
        stepCount = (int)(seekCmd & 0xFFFFu);
    }

    int wrapMode = 0;
    int dir = 0;
    int lastGoodIdx = -2;
    unsigned int curCmd = seekCmd;

    while (true) {
        if (stepCount != 0) {
            if (stepDir == 1)
                curCmd = 0xFFFFFFFDu;
            else if (stepDir == -1)
                curCmd = 0xFFFFFFFCu;
        }

        unsigned char reachedEnd = 0;
        bool canRetryWrap = true;
        if (status.listMode != 1) {
            if (status.listMode == 0)
                canRetryWrap = false;
            else if (isNewMedia != 0 && status.currentTrackId != -1)
                canRetryWrap = false;
        }

        while (triedCount <= (unsigned int)status.playState) {
            unsigned int listIdx = mapSeekCmdToListIndex(curCmd, repeatMode, speedMode, isNewMedia);
            err = media->seekToIndex(listIdx, outChanged);
            media->getStatus(&status);

            if (err == 0) {
                err = reinterpret_cast<MediaItem *>(&candidate)->acquireFromList(status.trackOrderIndex, reinterpret_cast<MediaListBase *>(media));
                if (err == 0) {
                    int decArg = (decMgr != NULL) ? static_cast<const DecoderManagerHandle *>(decMgr)->ioPriority : 0;
                    err = reinterpret_cast<MediaItem *>(&candidate)->probe(decArg, static_cast<uint8_t>(1));
                    if (err != 0)
                        reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(1));
                }
            }

            if (err == 0) {
                if (reachedEnd == 1) {
                    err = (dir >= 0) ? MUSICCORE_ERR_END_FWD : MUSICCORE_ERR_END_REV;
                    reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(1));
                }
                break;
            }

            if (canRetryWrap) {
                resolveWrapAndDir(curCmd, repeatMode, speedMode, status.listMode, &wrapMode, &dir);
            } else {
                wrapMode = 0;
                dir = 0;
            }

            unsigned char wrapped = 0;
            curCmd = media->stepNextCandidate(listIdx, wrapMode, dir, &triedCount, &err,
                                              &reachedEnd, &wrapped);
            if (curCmd == 0xFFFFFFFEu)
                break;

            *outStep = -1;
            if (wrapped == 1 && *ioShuffleSeed != 0 && allowReshuffle == 1) {
                uint64_t t = sceKernelGetProcessTimeWide();
                *ioShuffleSeed = (t == 0) ? 23841636 : (int)t;
                media->applyShuffleSeed();
            }
        }

        if (stepCount == 0)
            break;

        if (err != 0) {
            if (lastGoodIdx != -2) {
                int restoreErr = reinterpret_cast<MediaItem *>(&candidate)->acquireFromList(lastGoodIdx, reinterpret_cast<MediaListBase *>(media));
                if (restoreErr != 0) {
                    *outExtraErr = err;
                    err = restoreErr;
                } else {
                    int decArg = (decMgr != NULL) ? static_cast<const DecoderManagerHandle *>(decMgr)->ioPriority : 0;
                    reinterpret_cast<MediaItem *>(&candidate)->probe(decArg, static_cast<uint8_t>(0));
                    int seekErr = media->seekToIndex((unsigned int)lastGoodIdx, outChanged);
                    *outExtraErr = err;
                    if (seekErr == 0) {
                        err = 0;
                    } else {
                        err = seekErr;
                        reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
                    }
                }
            }
            break;
        }

        if (stepCount == 1)
            break;

        lastGoodIdx = status.trackOrderIndex;
        --stepCount;
        reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
    }

    if (err == 0) {
        int moveErr = reinterpret_cast<MediaItem *>(&candidate)->transferTo(reinterpret_cast<MediaItem *>(outTrackInfo), static_cast<uint8_t>(1));
        if (moveErr != 0) {
            reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(1));
            return moveErr;
        }
        return 0;
    }

    media->getStatus(&status);
    if (initialOrderIdx != status.trackOrderIndex)
        media->seekToIndex((unsigned int)initialOrderIdx, outChanged);
    *outChanged = 0;
    return err;
}

int dbSeekSpecialTarget(MediaListInterface *media, int64_t prevPlaylistId,
                        int repeatMode, int speedMode, int *ioShuffleSeed,
                        int *outStep, void *decMgr, unsigned char keepIfSame,
                        unsigned char checkEmpty, TrackInfoData *outTrackInfo,
                        unsigned char *outHasTrack)
{
    if (media == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    unsigned char changed = 0;
    void *listHandle = media->getListHandle();
    int64_t expectedId = prevPlaylistId;
    unsigned int initialCmd = 0xFFFC0000u;
    if (isIdRejected(prevPlaylistId, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(listHandle))) == 1) {
        expectedId = -1;
        initialCmd = 0xFFFFFFFDu;
    }

    TrackInfoData candidate;
    resetTrackInfo(&candidate);
    int extraErr = 0;

    int err = dbSeekNormalTarget(initialCmd, media, 0, speedMode, repeatMode,
                                 ioShuffleSeed, outStep, decMgr, &changed,
                                 &candidate, &extraErr, 0);
    int64_t resolvedId = -1;
    if (err != 0) {
        if (err == MUSICCORE_ERR_END_REV || err == MUSICCORE_ERR_END_FWD) {
            unsigned int retryCmd = (err == MUSICCORE_ERR_END_REV) ? 0xFFFFFFFDu : 0xFFFFFFFCu;
            err = dbSeekNormalTarget(retryCmd, media, 0, speedMode, repeatMode,
                                     ioShuffleSeed, outStep, decMgr, &changed,
                                     &candidate, &extraErr, 1);
            if (err == 0)
                resolvedId = candidate.playlistId;
        }
    } else {
        resolvedId = candidate.playlistId;
    }

    int64_t currentId = resolvedId;
    if (checkEmpty == 1) {
        int64_t queriedId = -1;
        int curErr = media->resolveCurrentTrackId(&queriedId);
        if (curErr == 0) {
            currentId = queriedId;
        } else if (curErr != MUSICCORE_ERR_LIST_EMPTY) {
            reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
            *outHasTrack = 0;
            return err;
        } else {
            if (media->hasTracks() != 0) {
                if (*ioShuffleSeed != 0) {
                    unsigned char locked = 0;
                    media->lockList(*ioShuffleSeed, -1, -1, &locked, 0);
                }
                err = dbSeekNormalTarget(0xFFFFFFFBu, media, 0, speedMode, repeatMode,
                                         ioShuffleSeed, outStep, decMgr, &changed,
                                         &candidate, &extraErr, 1);
                resolvedId = (err == 0) ? candidate.playlistId : -1;
            }
            currentId = resolvedId;
        }
    }

    if (expectedId != -1 && expectedId == currentId) {
        if (*outStep == -1)
            *outStep = -2;
        if (keepIfSame == 1) {
            if (resolvedId != -1) {
                err = reinterpret_cast<MediaItem *>(&candidate)->transferTo(reinterpret_cast<MediaItem *>(outTrackInfo), static_cast<uint8_t>(1));
                if (err != 0) {
                    reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
                    *outHasTrack = 0;
                } else {
                    *outHasTrack = 1;
                }
            }
        } else {
            reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
            *outHasTrack = 0;
            if (err == MUSICCORE_ERR_END_FWD || err == MUSICCORE_ERR_END_REV)
                return 0;
        }
    } else if (resolvedId != -1) {
        err = reinterpret_cast<MediaItem *>(&candidate)->transferTo(reinterpret_cast<MediaItem *>(outTrackInfo), static_cast<uint8_t>(1));
        if (err != 0) {
            reinterpret_cast<MediaItem *>(&candidate)->releaseToList(static_cast<uint8_t>(0));
            *outHasTrack = 0;
        } else {
            *outHasTrack = 1;
        }
    }

    return err;
}

StateMachine::StateMachine(void *owner)
    : m_owner(owner),
      m_stateStopped(this, g_stateTransitionTable),
      m_decoderMgr(NULL),
      m_sharedSync(NULL),
      m_audioOut(NULL),
      m_currentState(NULL),
      m_flagPendingActive(0),
      m_stopRequested(0),
      m_hasPendingTrack(0),
      m_dbRemovedError(0),
      m_speedDirChanged(0),
      m_seekFailed(0),
      m_pendingMedia(NULL),
      m_pendingSeekStep(-2),
      m_pendingVolumeMode((int)0x80000000u),
      m_padPendingAlign(0),
      m_padPendingTail(0),
      m_lastDbErr(0),
      m_extraSeekErr(0),
      m_targetState(7),
      m_pendingFadeMs(-1),
      m_playParam(-1),
      m_needDecoderRestart(0),
      m_activeMedia(NULL),
      m_currentPosMs(-1),
      m_prevTrackPos(-1),
      m_nextTrackPos(-1),
      m_padActiveTail(0),
      m_repeatMode(0),
      m_shuffleSeed(0),
      m_seekMode(-1),
      m_speedMode(-1),
      m_volumeMode(1),
      m_audioBalance(0x8000),
      m_eqType(0),
      m_eqBandLow(0),
      m_eqBandMid(0),
      m_eqBandHigh(0),
      m_audioNormalizer(0),
      m_hasPendingError(0),
      m_lastErrorType(0),
      m_extraErr(0),
      m_metaBufs(NULL),
      m_cachedPosMs(-1)
{
    int err = 0;
    StateMachineContext *ctx = reinterpret_cast<StateMachineContext *>(this);
    new (m_stateIdle) StateIdle(ctx, &err);
    new (m_stateInit) StateActivating(ctx, &err);
    new (m_stateReady) StateReady(ctx, &err);
    new (m_stateStarting) StateStarting(ctx, &err);
    new (m_statePlaying) StatePlaying(ctx, &err);
    new (m_statePaused) StateStopping(ctx, &err);
    new (m_stateStopping) StateDeactivating(ctx, &err);

    m_currentState = reinterpret_cast<PlaybackStateBase *>(m_stateIdle);
    resetTrackInfo(&m_pendingTrackInfo);
    resetTrackInfo(&m_activeTrackInfo);
    resetMediaListStatus(&m_mediaListStatus);
}

StateMachine::~StateMachine()
{
    if (m_hasPendingTrack == 1)
        reinterpret_cast<MediaItem *>(&m_pendingTrackInfo)->releaseToList(static_cast<uint8_t>(0));
    m_hasPendingTrack = 0;

    reinterpret_cast<StateDeactivating *>(m_stateStopping)->~StateDeactivating();
    reinterpret_cast<StateStopping *>(m_statePaused)->~StateStopping();
    reinterpret_cast<StatePlaying *>(m_statePlaying)->~StatePlaying();
    reinterpret_cast<StateStarting *>(m_stateStarting)->~StateStarting();
    reinterpret_cast<StateReady *>(m_stateReady)->~StateReady();
    reinterpret_cast<StateActivating *>(m_stateInit)->~StateActivating();
    reinterpret_cast<StateIdle *>(m_stateIdle)->~StateIdle();
}

int StateMachine::attach(void *audioOut, void *decoderMgr, void *sharedSync)
{
    m_decoderMgr = decoderMgr;
    m_audioOut = audioOut;
    m_sharedSync = sharedSync;
    return 0;
}

int StateMachine::buildSnapshot(StateSnapshotOut *outSnap)
{
    outSnap->stateId = (m_currentState != NULL) ? m_currentState->m_stateId : 0;
    outSnap->playState = m_mediaListStatus.playState;
    outSnap->mediaHandle = m_activeMedia;
    outSnap->playlistId = m_activeTrackInfo.playlistId;
    outSnap->trackId = m_activeTrackInfo.trackId;
    outSnap->trackItemId = m_activeTrackInfo.trackItemId;
    outSnap->trackOrderIndex = m_mediaListStatus.trackOrderIndex;
    outSnap->trackIndex = (m_activeMedia != NULL) ? m_activeMedia->getTrackOrderIndex() : -2;

    outSnap->durationMs = m_activeTrackInfo.durationMs;
    outSnap->sampleRate = m_activeTrackInfo.sampleRate;
    outSnap->bitRate = m_activeTrackInfo.bitRate;
    outSnap->channels = m_activeTrackInfo.channels;
    outSnap->codecType = m_activeTrackInfo.codecFormat;
    outSnap->sourceType = m_activeTrackInfo.loopCount;

    outSnap->positionMs = (m_currentState != NULL) ? m_currentState->getCurrentPosMs() : m_currentPosMs;
    outSnap->currentTrackNum = m_prevTrackPos;
    outSnap->totalTracks = m_nextTrackPos;

    outSnap->repeatMode = m_repeatMode;
    outSnap->shuffleMode = m_shuffleSeed;
    outSnap->seekMode = m_seekMode;
    outSnap->speedMode = m_speedMode;
    outSnap->volumeMode = m_volumeMode;
    outSnap->audioBalance = m_audioBalance;
    outSnap->eqType = m_eqType;
    outSnap->eqBandLow = m_eqBandLow;
    outSnap->eqBandMid = m_eqBandMid;
    outSnap->eqBandHigh = m_eqBandHigh;
    outSnap->audioNormalizer = m_audioNormalizer;
    outSnap->lastErrorType = m_lastErrorType;
    outSnap->hasPendingError = m_hasPendingError;
    outSnap->extraErr = m_extraErr;

    if (m_extraErr != 0) {
        m_extraErr = 0;
        return 0;
    }
    return 0;
}

int StateMachine::dispatchCmd(uint16_t cmdCode, const StateDispatchReq *req,
                              StateSnapshotOut *outSnap, int *outTargetTail,
                              unsigned char *outFlags)
{
    if (m_currentState == NULL)
        return MUSICCORE_ERR_FAILED;

    int ret = MUSICCORE_ERR_INVALID_ARG;
    if (cmdCode >= 0x1200u && cmdCode <= 0x1206u) {
        switch (cmdCode) {
        case 0x1200u: ret = m_currentState->onTrackInfo(); break;
        case 0x1201u: ret = m_currentState->onAudioError(req->errorCode); break;
        case 0x1202u: ret = m_currentState->onTrackReady(); break;
        case 0x1204u: ret = m_currentState->onBufferReady(); break;
        case 0x1205u: ret = m_currentState->onStreamEnd(); break;
        case 0x1206u: ret = m_currentState->onAudioPortEvent(req->portFlag); break;
        default: ret = MUSICCORE_ERR_INVALID_ARG; break;
        }
    } else if (cmdCode >= 0x1100u && cmdCode <= 0x1112u) {
        switch (cmdCode) {
        case 0x1100u:
            ret = 0;
            break;
        case 0x1101u:
            if (m_owner != NULL)
                m_metaBufs = (TrackMetadataBuffers *)req->metaBufs;
            ret = m_currentState->onActivate(
                reinterpret_cast<const ActivateStateSnapshot *>(req), req->fadeMs);
            if (ret != 0)
                m_metaBufs = NULL;
            break;
        case 0x1102u:
            ret = m_currentState->onDeactivate(req->fadeMs, req->syncFlag);
            if (ret == 0 && m_owner != NULL)
                m_metaBufs = NULL;
            break;
        case 0x1103u:
            ret = m_currentState->onCancel();
            break;
        case 0x1104u:
            ret = m_currentState->onSetMedia(
                reinterpret_cast<MediaListAccess *>(req->newMediaHandle),
                req->shuffleValue, req->syncFlag);
            if (ret != 0) {
                void *oldMedia = req->newMediaHandle;
                notifyMediaChanged(0, NULL, 0, NULL, &oldMedia, 1);
            }
            break;
        case 0x1105u:
            ret = m_currentState->onPlay(req->playParamVal);
            break;
        case 0x1106u:
            ret = m_currentState->onPause(req->resetPos, req->playParamVal);
            break;
        case 0x1107u:
            ret = m_currentState->onStop();
            break;
        case 0x1108u:
            ret = m_currentState->onSetRepeat(req->repeatValue);
            break;
        case 0x1109u:
            ret = m_currentState->onSetShuffle(req->shuffleValue);
            break;
        case 0x110Bu:
            ret = m_currentState->onSeek(req->seekParams);
            break;
        case 0x110Cu:
            ret = m_currentState->onSetVolume(req->volumeLevel, req->volumeRamp);
            break;
        case 0x110Du:
            ret = m_currentState->onSetEqualizer(req->eqParams[0], req->eqParams[1], req->eqParams[2]);
            break;
        case 0x110Eu:
            ret = m_currentState->onSetAudioNormalizer(req->normalizerMode);
            break;
        case 0x110Fu:
            ret = m_currentState->onSuspendForDb(req->suspendFlag);
            *outFlags = req->suspendFlag;
            break;
        case 0x1111u:
            ret = m_currentState->onSeekAndPlay(req->seekParams, req->playParamVal);
            break;
        case 0x1112u:
            ret = m_currentState->onSetMediaPlaying(
                reinterpret_cast<MediaListAccess *>(req->newMediaHandle), req->shuffleValue);
            if (ret != 0) {
                void *oldMedia = req->newMediaHandle;
                notifyMediaChanged(0, NULL, 0, NULL, &oldMedia, 1);
            }
            break;
        case 0x110Au:
        case 0x1110u:
            ret = MUSICCORE_ERR_INVALID_ARG;
            break;
        default:
            abort();
        }
    }

    buildSnapshot(outSnap);
    outTargetTail[0] = m_targetState;
    outTargetTail[1] = m_pendingFadeMs;
    outTargetTail[2] = m_playParam;
    return ret;
}

int StateMachine::shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode)
{
    if (m_currentState != NULL)
        return m_currentState->shouldDeferCmd(cmdCode, outWaitCode);
    return 1;
}

int StateMachine::getCurrentPosMs()
{
    if (m_currentState != NULL) {
        int pos = m_currentState->getCurrentPosMs();
        m_cachedPosMs = pos;
        return pos;
    }
    return (m_cachedPosMs != -1) ? m_cachedPosMs : 0;
}

int64_t StateMachine::getCurrentTrackId()
{
    return m_activeTrackInfo.trackId;
}

int StateMachine::transitionTo(int targetStateId)
{
    if (m_currentState != NULL) {
        int exitErr = m_currentState->onExit();
        if (exitErr != 0)
            return exitErr;
    }

    PlaybackStateBase *nextState = NULL;
    switch (targetStateId) {
    case 0: nextState = (PlaybackStateBase *)m_stateIdle; break;
    case 1: nextState = (PlaybackStateBase *)m_stateInit; break;
    case 2: nextState = (PlaybackStateBase *)m_stateReady; break;
    case 3: nextState = (PlaybackStateBase *)m_stateStarting; break;
    case 4: nextState = (PlaybackStateBase *)m_statePlaying; break;
    case 6: nextState = (PlaybackStateBase *)m_statePaused; break;
    case 7: nextState = (PlaybackStateBase *)&m_stateStopped; break;
    case 8: nextState = (PlaybackStateBase *)m_stateStopping; break;
    default: return MUSICCORE_ERR_INVALID_ARG;
    }

    int enterErr = nextState->onEnter();
    if (enterErr != 0) {
        switch (targetStateId) {
        case 0:
        case 1:
            m_currentState = (PlaybackStateBase *)m_stateIdle;
            break;
        case 2:
        case 6:
        case 8:
            break;
        case 3:
        case 7:
            m_currentState = (PlaybackStateBase *)&m_stateStopped;
            break;
        case 4:
        case 5:
            m_currentState = (PlaybackStateBase *)m_statePaused;
            break;
        default:
            return enterErr;
        }
        return enterErr;
    }

    m_currentState = nextState;
    return 0;
}

int StateMachine::populateMetadataSafe(const TrackInfoData *trackInfo, TrackMetadataBuffers *metaBufs)
{
    int ret = const_cast<MediaItem *>(reinterpret_cast<const MediaItem *>(trackInfo))->readMetadata(reinterpret_cast<MetadataOutputRequest *>(metaBufs));
    if (ret != 0 && metaBufs != NULL) {
        if (metaBufs->mutex != NULL)
            metaBufs->mutex->Lock();
        if (metaBufs->titleBuf != NULL && metaBufs->titleLen != 0)
            metaBufs->titleBuf[0] = '\0';
        if (metaBufs->artistBuf != NULL && metaBufs->artistLen != 0)
            metaBufs->artistBuf[0] = '\0';
        if (metaBufs->albumBuf != NULL && metaBufs->albumLen != 0)
            metaBufs->albumBuf[0] = '\0';
        if (metaBufs->mutex != NULL)
            metaBufs->mutex->Unlock();
    }
    return ret;
}

int StateMachine::seekActiveMedia(const unsigned int *seekParams, MediaListInterface *media,
                                  unsigned char isNewMedia)
{
    unsigned int seekCmd = seekParams[0];
    int volModeParam = (int)seekParams[1];
    unsigned int seekModeParam = seekParams[2];
    unsigned int speedModeParam = seekParams[3];
    int stepParam = (int)seekParams[4];

    if (media == NULL && (seekCmd != 0xFFFFFFFFu || stepParam != -2))
        return MUSICCORE_ERR_NO_MEDIA;

    int ret = 0;
    if (media != NULL) {
        int shuffleSeed = m_shuffleSeed;
        int prevOrderIdx = m_mediaListStatus.trackOrderIndex;

        if ((seekCmd >> 16) == 0xFFFCu) {
            bool hadTrack = (reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->releaseToList(static_cast<uint8_t>(1)) == 0);
            unsigned char hasTrack = 0;
            media->setTrackPosition(prevOrderIdx, &m_activeTrackInfo.trackId);
            ret = dbSeekSpecialTarget(media, m_activeTrackInfo.playlistId, m_repeatMode,
                                      m_volumeMode, &shuffleSeed, &stepParam, m_decoderMgr, 0,
                                      ((seekCmd & 0xFFFFu) != 0) ? 1 : 0, &m_activeTrackInfo,
                                      &hasTrack);
            if (ret != 0) {
                if (prevOrderIdx != -2 &&
                    reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->acquireFromList(prevOrderIdx, reinterpret_cast<MediaListBase *>(media)) == 0) {
                    int decArg = (m_decoderMgr != NULL) ? static_cast<const DecoderManagerHandle *>(m_decoderMgr)->ioPriority : 0;
                    reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->probe(decArg, static_cast<uint8_t>(0));
                } else {
                    if (hadTrack)
                        m_lastErrorType = 0;
                }
            } else {
                ret = media->getStatus(&m_mediaListStatus);
                TrackInfoData metaCopy = m_activeTrackInfo;
                populateMetadataSafe(&metaCopy, m_metaBufs);
                if (ret == 0 && hadTrack)
                    m_lastErrorType = 0;
                if (ret != 0)
                    return ret;
            }
        } else if (seekCmd != 0xFFFFFFFFu || prevOrderIdx == -2) {
            bool hadTrack = (reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->releaseToList(static_cast<uint8_t>(1)) == 0);
            unsigned char changed = 0;
            int extraErr = 0;
            ret = dbSeekNormalTarget(seekCmd, media, isNewMedia, m_volumeMode, m_repeatMode,
                                     &shuffleSeed, &stepParam, m_decoderMgr, &changed,
                                     &m_activeTrackInfo, &extraErr, 1);
            if (ret != 0) {
                if (prevOrderIdx != -2 &&
                    reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->acquireFromList(prevOrderIdx, reinterpret_cast<MediaListBase *>(m_activeMedia)) == 0) {
                    int decArg = (m_decoderMgr != NULL) ? static_cast<const DecoderManagerHandle *>(m_decoderMgr)->ioPriority : 0;
                    reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->probe(decArg, static_cast<uint8_t>(0));
                } else if (hadTrack) {
                    m_lastErrorType = 0;
                }
                return ret;
            }
            media->getStatus(&m_mediaListStatus);
            TrackInfoData metaCopy = m_activeTrackInfo;
            populateMetadataSafe(&metaCopy, m_metaBufs);
            if (hadTrack)
                m_lastErrorType = 0;
        }
    }

    if (volModeParam == (int)0x80000000u) {
        if (m_volumeMode == (int)0x80000000u)
            m_volumeMode = 1;
    } else {
        m_volumeMode = volModeParam;
    }

    if (seekModeParam != 0xFFFFFFFEu)
        m_seekMode = (int)seekModeParam;
    if (speedModeParam != 0xFFFFFFFEu)
        m_speedMode = (int)speedModeParam;

    if (media != NULL) {
        if (m_currentState != NULL)
            m_currentPosMs = m_currentState->getCurrentPosMs();
        computePlaybackRange(m_seekMode, m_speedMode, stepParam, m_activeTrackInfo.durationMs,
                             m_volumeMode, &m_prevTrackPos, &m_nextTrackPos, &m_currentPosMs);
        m_cachedPosMs = m_currentPosMs;
    }
    return ret;
}

int StateMachine::seekPendingMedia(const unsigned int *seekParams, MediaListInterface *media,
                                   unsigned char *outHasPending, unsigned char isNewMedia)
{
    unsigned int seekCmd = seekParams[0];
    int volModeParam = (int)seekParams[1];
    unsigned int seekModeParam = seekParams[2];
    unsigned int speedModeParam = seekParams[3];
    int stepParam = (int)seekParams[4];

    *outHasPending = 0;
    if (media == NULL && (seekCmd != 0xFFFFFFFFu || stepParam != -2))
        return MUSICCORE_ERR_NO_MEDIA;

    int ret = 0;
    if (media != NULL) {
        if ((seekCmd >> 16) == 0xFFFCu) {
            unsigned char hadPending = 0;
            if (m_hasPendingTrack == 1) {
                reinterpret_cast<MediaItem *>(&m_pendingTrackInfo)->releaseToList(static_cast<uint8_t>(1));
                m_hasPendingTrack = 0;
                hadPending = 1;
            }

            unsigned char hasTrack = 0;
            media->setTrackPosition(m_mediaListStatus.trackOrderIndex, &m_activeTrackInfo.trackId);
            unsigned char checkEmpty = ((seekCmd & 0xFFFFu) != 0) ? 1 : 0;
            ret = dbSeekSpecialTarget(media, m_activeTrackInfo.playlistId, m_repeatMode,
                                      m_volumeMode, &m_shuffleSeed, &stepParam, m_decoderMgr,
                                      hadPending, checkEmpty, &m_pendingTrackInfo, &hasTrack);
            if (ret != 0) {
                if (ret == MUSICCORE_ERR_DB_REMOVED) {
                    m_hasPendingTrack = 0;
                    m_pendingSeekStep = -2;
                    *outHasPending = 1;
                    return ret;
                }
            } else if (hasTrack == 1) {
                m_hasPendingTrack = 1;
                *outHasPending = (hadPending == 1) ? 0 : 1;
            } else {
                *outHasPending = 0;
                m_hasPendingTrack = 0;
                if (hadPending != 1)
                    media->getStatus(&m_mediaListStatus);
            }
        } else if (seekCmd != 0xFFFFFFFFu || m_mediaListStatus.trackOrderIndex == -2) {
            unsigned char hadPending = 0;
            if (m_hasPendingTrack == 1) {
                reinterpret_cast<MediaItem *>(&m_pendingTrackInfo)->releaseToList(static_cast<uint8_t>(1));
                m_hasPendingTrack = 0;
                hadPending = 1;
            }

            unsigned char changed = 0;
            ret = dbSeekNormalTarget(seekCmd, media, isNewMedia, m_volumeMode, m_repeatMode,
                                     &m_shuffleSeed, &stepParam, m_decoderMgr, &changed,
                                     &m_pendingTrackInfo, &m_extraSeekErr, 1);
            if (ret != 0) {
                if (hadPending == 1) {
                    if (reinterpret_cast<MediaItem *>(&m_pendingTrackInfo)->acquireFromList(m_mediaListStatus.trackOrderIndex, reinterpret_cast<MediaListBase *>(m_activeMedia)) != 0) {
                        m_hasPendingTrack = 0;
                        *outHasPending = 0;
                    } else {
                        int decArg = (m_decoderMgr != NULL) ? static_cast<const DecoderManagerHandle *>(m_decoderMgr)->ioPriority : 0;
                        reinterpret_cast<MediaItem *>(&m_pendingTrackInfo)->probe(decArg, static_cast<uint8_t>(0));
                        m_hasPendingTrack = 1;
                        *outHasPending = 1;
                    }
                }
                return ret;
            }
            media->getStatus(&m_mediaListStatus);
            m_hasPendingTrack = changed;
            *outHasPending = 1;
        }
    }

    int newVolMode = m_volumeMode;
    if (volModeParam == (int)0x80000000u) {
        if (m_volumeMode == (int)0x80000000u)
            newVolMode = 1;
        else if (m_pendingVolumeMode != (int)0x80000000u)
            newVolMode = m_pendingVolumeMode;
    } else {
        newVolMode = volModeParam;
    }

    if (m_volumeMode == (int)0x80000000u || newVolMode * m_volumeMode < 0) {
        *outHasPending = 1;
        m_speedDirChanged = 1;
    } else {
        m_volumeMode = newVolMode;
    }
    m_pendingVolumeMode = newVolMode;

    if (seekModeParam != 0xFFFFFFFEu)
        m_seekMode = (int)seekModeParam;
    if (speedModeParam != 0xFFFFFFFEu)
        m_speedMode = (int)speedModeParam;

    if (media != NULL && stepParam != -2) {
        m_pendingSeekStep = stepParam;
        *outHasPending = 1;
    } else if (seekModeParam != 0xFFFFFFFEu || speedModeParam != 0xFFFFFFFEu) {
        *outHasPending = 1;
    }
    return ret;
}

int StateMachine::swapActiveMedia(MediaListInterface *newMedia, unsigned char hasUriFlag,
                                  unsigned char seekInitial, unsigned char hasNewMedia)
{
    int savedPosMs = -1;
    int savedPrevPos = -1;
    int savedNextPos = -1;
    int savedSeekMode = -1;
    int savedSpeedMode = -1;
    int savedVolMode = 1;
    int savedHasPendingErr = 0;
    int savedExtraErr = 0;

    if (m_activeMedia != NULL) {
        if (reinterpret_cast<MediaItem *>(&m_activeTrackInfo)->releaseToList(static_cast<uint8_t>(1)) == 0)
            m_lastErrorType = 0;
        savedPosMs = m_currentPosMs;
        savedPrevPos = m_prevTrackPos;
        savedNextPos = m_nextTrackPos;
        savedSeekMode = m_seekMode;
        savedSpeedMode = m_speedMode;
        savedVolMode = m_volumeMode;
        savedHasPendingErr = m_hasPendingError;
        savedExtraErr = m_extraErr;

        m_currentPosMs = -1;
        m_prevTrackPos = -1;
        m_nextTrackPos = -1;
        m_seekMode = -1;
        m_speedMode = -1;
        m_volumeMode = 1;
        m_hasPendingError = 0;
        m_extraErr = 0;
    }

    int ret = 0;
    if (newMedia == NULL) {
        resetMediaListStatus(&m_mediaListStatus);
    } else {
        MediaListStatus tmpStatus;
        resetMediaListStatus(&tmpStatus);
        newMedia->getStatus(&tmpStatus);

        if (seekInitial == 1 && m_shuffleSeed != 0) {
            uint64_t t = sceKernelGetProcessTimeWide();
            m_shuffleSeed = (t == 0) ? 23841636 : (int)t;
        }

        unsigned char locked = 0;
        newMedia->lockList(m_shuffleSeed, -1, -1, &locked, 0);

        if (seekInitial == 1) {
            unsigned int initSeek[5];
            resetSeekParams(initSeek);
            ret = seekActiveMedia(initSeek, newMedia, 1);
            if (ret != 0) {
                if (locked == 1)
                    newMedia->unlockList(0);
                m_currentPosMs = savedPosMs;
                m_prevTrackPos = savedPrevPos;
                m_nextTrackPos = savedNextPos;
                m_seekMode = savedSeekMode;
                m_speedMode = savedSpeedMode;
                m_volumeMode = savedVolMode;
                m_hasPendingError = savedHasPendingErr;
                m_extraErr = savedExtraErr;
                return ret;
            }
        }
    }

    void *oldMedia = m_activeMedia;
    if (m_activeMedia != NULL)
        m_activeMedia->unlockList(0);
    m_activeMedia = newMedia;

    StateSnapshotOut snap;
    sce_paf_memset(&snap, 0, sizeof(snap));
    buildSnapshot(&snap);
    notifyMediaChanged(hasNewMedia, newMedia, hasUriFlag, &snap, &oldMedia, 0);
    return ret;
}

int StateMachine::setPendingMedia(MediaListInterface *newMedia)
{
    if (newMedia != NULL) {
        if (m_shuffleSeed != 0) {
            uint64_t t = sceKernelGetProcessTimeWide();
            m_shuffleSeed = (t == 0) ? 23841636 : (int)t;
        }
        MediaListStatus tmpStatus;
        resetMediaListStatus(&tmpStatus);
        newMedia->getStatus(&tmpStatus);

        unsigned char locked = 0;
        newMedia->lockList(m_shuffleSeed, (int)tmpStatus.currentTrackId,
                           (int)(tmpStatus.currentTrackId >> 32), &locked, 1);

        unsigned char hasPending = 0;
        unsigned int initSeek[5];
        resetSeekParams(initSeek);
        int err = seekPendingMedia(initSeek, newMedia, &hasPending, 1);
        if (locked == 1)
            newMedia->unlockList(0);
        if (err != 0)
            return err;
    }

    notifyMediaChanged(0, NULL, 0, NULL, &m_pendingMedia, 1);
    m_pendingMedia = newMedia;
    return 0;
}

int StateMachine::notifyTrackInfo()
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    typedef int (*Fn)(void *);
    return (*(Fn **)m_owner)[0](m_owner);
}

int StateMachine::notifyAudioError()
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    typedef int (*Fn)(void *);
    return (*(Fn **)m_owner)[1](m_owner);
}

int StateMachine::notifyBufferReady()
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    typedef int (*Fn)(void *);
    return (*(Fn **)m_owner)[4](m_owner);
}

int StateMachine::notifyStreamEnd()
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    typedef int (*Fn)(void *);
    return (*(Fn **)m_owner)[5](m_owner);
}

int StateMachine::notifyMediaChanged(unsigned char hasNewMedia, void *newMedia,
                                     unsigned char hasUriFlag, const void *stateSnap,
                                     void **ioOldMedia, unsigned char decrementUri)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    if (*ioOldMedia == NULL && hasNewMedia != 1)
        return 0;
    typedef int (*Fn)(void *, unsigned char, void *, unsigned char, const void *, void **, unsigned char);
    return (*(Fn **)m_owner)[6](m_owner, hasNewMedia, newMedia, hasUriFlag,
                                stateSnap, ioOldMedia, decrementUri);
}

/* =========================================================================
 * StateStopped (0x811E6938 - 0x811E6D64)
 * ========================================================================= */

StateStopped::StateStopped(StateMachine *owner, void *sharedTable)
    : m_owner(owner),
      m_stateId(7)
{
    (void)sharedTable;
    int err = 0;
    new (this) PlaybackStateBase(reinterpret_cast<StateMachineContext *>(owner), 7, &err);
}

StateStopped::~StateStopped()
{
    reinterpret_cast<PlaybackStateBase *>(this)->~PlaybackStateBase();
}

int StateStopped::onDeactivate()
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    return m_owner->transitionTo(8);
}

int StateStopped::onSetMedia(MediaListInterface *media, int shuffleSeed, unsigned char pauseFlag)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    if (media == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    m_owner->m_shuffleSeed = shuffleSeed;
    m_owner->setPendingMedia(NULL);
    int ret = m_owner->swapActiveMedia(media, 1, 1, 1);
    if (ret == 0 && pauseFlag == 1)
        m_owner->m_targetState = 7;
    return ret;
}

int StateStopped::onPlay(int playParam)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;

    m_owner->m_targetState = 4;
    m_owner->m_playParam = playParam;
    if (m_owner->m_decoderMgr != NULL) {
        int sameFmt = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->checkMediaCompat(
            static_cast<const MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo));
        m_owner->m_needDecoderRestart = (sameFmt != 1) ? 1 : 0;
    } else {
        m_owner->m_needDecoderRestart = 1;
    }
    return m_owner->transitionTo(3);
}

int StateStopped::onPause(unsigned char resetPos)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;

    m_owner->m_targetState = 7;
    if (resetPos == 1)
        m_owner->m_currentPosMs = 0;
    m_owner->m_playParam = -1;
    return 0;
}

int StateStopped::onSeek(const unsigned int *seekParams)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    return m_owner->seekActiveMedia(seekParams, m_owner->m_activeMedia, 0);
}

int StateStopped::onSetShuffle(int shuffleSeed)
{
    m_owner->m_shuffleSeed = shuffleSeed;
    if (m_owner->m_activeMedia != NULL) {
        m_owner->m_activeMedia->applyShuffleSeed();
        m_owner->m_activeMedia->getStatus(&m_owner->m_mediaListStatus);
    }
    return 0;
}

int StateStopped::onSetVolume(int volume, int ramp)
{
    if (m_owner == NULL || m_owner->m_audioOut == NULL)
        return MUSICCORE_ERR_FAILED;
    unsigned char applied = 0;
    int ret = static_cast<AudioOutput *>(m_owner->m_audioOut)->setVolumeOrDbFade(
        static_cast<unsigned int>(volume), ramp, &applied);
    if (ret == 0)
        m_owner->m_audioBalance = volume;
    return ret;
}

int StateStopped::onSetAudioNormalizer(int alcMode)
{
    if (m_owner == NULL || m_owner->m_audioOut == NULL)
        return MUSICCORE_ERR_FAILED;
    int ret = static_cast<AudioOutput *>(m_owner->m_audioOut)->setAlcMode(alcMode);
    if (ret == 0)
        m_owner->m_audioNormalizer = alcMode;
    return ret;
}

int StateStopped::onSuspendForDb(unsigned char flag)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    if (m_owner->m_activeMedia == NULL)
        return MUSICCORE_ERR_NO_MEDIA;

    unsigned int seekParams[5];
    seekParams[0] = (flag != 0 ? 1u : 0u) | 0xFFFC0000u;
    seekParams[1] = 0x80000000u;
    seekParams[2] = 0xFFFFFFFEu;
    seekParams[3] = 0xFFFFFFFEu;
    seekParams[4] = 0xFFFFFFFFu;

    int ret = m_owner->seekActiveMedia(seekParams, m_owner->m_activeMedia, 0);
    switch (ret) {
    case MUSICCORE_ERR_DB_REMOVED:
        m_owner->swapActiveMedia(NULL, 0, 0, 0);
        m_owner->m_hasPendingError = 6;
        m_owner->m_extraErr = MUSICCORE_ERR_DB_REMOVED;
        return m_owner->transitionTo(2);
    case MUSICCORE_ERR_END_FWD:
    case MUSICCORE_ERR_END_REV: {
        unsigned int retryParams[5];
        resetSeekParams(retryParams);
        retryParams[0] = 0xFFFFFFFCu;
        m_owner->seekActiveMedia(retryParams, m_owner->m_activeMedia, 0);
        m_owner->m_extraErr = m_owner->m_lastDbErr;
        return 0;
    }
    case 0:
        return 0;
    default:
        m_owner->m_extraErr = ret;
        m_owner->m_hasPendingError = 2;
        return ret;
    }
}

int StateStopped::onSeekAndPlay(const unsigned int *seekParams, int playParam)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;

    int ret = m_owner->seekActiveMedia(seekParams, m_owner->m_activeMedia, 0);
    if (ret != 0)
        return ret;

    m_owner->m_playParam = playParam;
    m_owner->m_targetState = 4;
    if (m_owner->m_decoderMgr != NULL) {
        int sameFmt = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->checkMediaCompat(
            static_cast<const MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo));
        m_owner->m_needDecoderRestart = (sameFmt != 1) ? 1 : 0;
    } else {
        m_owner->m_needDecoderRestart = 1;
    }
    return m_owner->transitionTo(3);
}

int StateStopped::onSetMediaPlaying(MediaListInterface *media, int shuffleSeed)
{
    if (m_owner == NULL)
        return MUSICCORE_ERR_FAILED;
    if (media == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    m_owner->m_shuffleSeed = shuffleSeed;
    m_owner->setPendingMedia(NULL);
    int ret = m_owner->swapActiveMedia(media, 1, 1, 1);
    if (ret != 0)
        return ret;

    m_owner->m_targetState = 4;
    m_owner->m_playParam = 0;
    if (m_owner->m_decoderMgr != NULL) {
        int sameFmt = static_cast<DecoderManager *>(m_owner->m_decoderMgr)->checkMediaCompat(
            static_cast<const MediaInfoBase *>(m_owner->m_activeTrackInfo.mediaInfo));
        m_owner->m_needDecoderRestart = (sameFmt != 1) ? 1 : 0;
    } else {
        m_owner->m_needDecoderRestart = 1;
    }
    return m_owner->transitionTo(3);
}

} /* namespace MusicCore */
} /* namespace sce */
