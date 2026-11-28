/*
 * SceShell musiccore_plugin - StateMachine and playback state declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_STATE_MACHINE_H
#define VSH_APP_SHELL_MUSICCORE_STATE_MACHINE_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>

namespace sce {
namespace MusicCore {

struct StateMachineContext;
struct StateMachine;
class MediaListAccess;
struct ActivateStateSnapshot;

struct TrackMetadataBuffers {
    char *titleBuf;
    size_t titleLen;
    char *artistBuf;
    size_t artistLen;
    char *albumBuf;
    size_t albumLen;
    paf::thread::Mutex *mutex;
};

struct TrackInfoData {
    union {
        void *vtable;
        void *vtablePtr;
    };
    union {
        int flags;
        int codecType;
    };
    int contentKind;
    int padAlign;
    int64_t playlistId;
    int64_t trackId;
    int64_t trackItemId;
    union {
        char titlePath[2048];
        char mediaPath[2048];
    };
    union {
        int codecFlags;
        int sourceKind;
    };
    int sampleRate;
    union {
        int codecTypeField;
        int codecFormat;
    };
    int bitRate;
    int channels;
    int durationMs;
    int playAttemptCount;
    int fileSizeHigh;
    int resumePositionMs;
    int playCount;
    int loopCount;
    int slotSerial;
    void *ownerList;
    void *mediaInfo;
    union {
        uint8_t isValid;
        uint8_t mediaInfoShared;
    };
    uint8_t padTail[3];
};

struct MediaListStatus {
    union {
        void *vtable;
        void *vtablePtr;
    };
    int playState;
    int trackOrderIndex;
    int padAlign;
    int64_t currentTrackId;
    int prevTrackIdx;
    int nextTrackIdx;
    int listMode;
    int listFlags;
};

struct StateSnapshotOut {
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
};

struct StateDispatchReq {
    unsigned char mediaPrefix[148];
    int padMedia;
    int fadeMs;
    void *newMediaHandle;
    unsigned char resetPos;
    unsigned char padReset[3];
    unsigned int seekParams[5];
    int repeatValue;
    int shuffleValue;
    int padShuffle;
    int errorCode;
    int volumeLevel;
    int volumeRamp;
    int eqParams[4];
    int normalizerMode;
    int playParamVal;
    uint32_t origParam;
    uint32_t origCmdHi;
    void *metaBufs;
    unsigned char suspendFlag;
    unsigned char syncFlag;
    unsigned char portFlag;
    unsigned char padTail;
};

struct MediaListInterface {
    virtual ~MediaListInterface() {}
    virtual void initialize() {}
    virtual void finalize() {}
    virtual int openList() { return 0; }
    virtual int closeList() { return 0; }
    virtual int refreshList() { return 0; }
    virtual int seekToIndex(unsigned int target, unsigned char *outChanged) = 0;
    virtual unsigned int stepNextCandidate(unsigned int curIdx, int wrapMode, int dir,
                                           unsigned int *ioTried, int *ioErr,
                                           unsigned char *outReachedEnd,
                                           unsigned char *outWrapped) = 0;
    virtual int resolveCurrentTrackId(int64_t *outTrackId) = 0;
    virtual void setTrackPosition(int orderIdx, int64_t *ioTrackId) = 0;
    virtual int queryTrackMeta(void *metaOut) { (void)metaOut; return 0; }
    virtual int getListKind() { return 0; }
    virtual int isListReady() { return 0; }
    virtual int getStatus(MediaListStatus *outStatus) = 0;
    virtual void *getListHandle() = 0;
    virtual int hasTracks() = 0;
    virtual int getCurrentIndex() = 0;
    virtual int64_t getCurrentTrackKey() { return -1; }
    virtual void lockList(int shuffleSeed, int anchorLow, int anchorHigh,
                          unsigned char *outLocked, int flag) = 0;
    virtual void unlockList(int flag) = 0;
    virtual void applyShuffleSeed() = 0;
    virtual int getTrackCount() { return 0; }
    virtual int getTrackOrderIndex() = 0;
};

class PlaybackStateBase {
public:
    PlaybackStateBase(StateMachineContext *owner, int stateId, int *outErr);
    virtual ~PlaybackStateBase();

    virtual int onEnter();
    virtual int onExit();
    virtual int onActivate(const ActivateStateSnapshot *snap, int fadeMs);
    virtual int onDeactivate(int fadeMs, uint8_t syncFlag);
    virtual int onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag);
    virtual int onPlay(int playParam);
    virtual int onPause(uint8_t resetPos, int fadeParam);
    virtual int onStop();
    virtual int onSeek(const uint32_t *seekParams);
    virtual int onSetRepeat(int repeatVal);
    virtual int onSetShuffle(int shuffleVal);
    virtual int onSetVolume(int vol, int ramp);
    virtual int onSetEqualizer(int eqType, int bandLow, int bandMid);
    virtual int onSetAudioNormalizer(int mode);
    virtual int onSuspendForDb(uint8_t suspendFlag);
    virtual int onCancel();
    virtual int onSeekAndPlay(const uint32_t *seekParams, int playParam);
    virtual int onSetMediaPlaying(MediaListAccess *media, int shuffleVal);
    virtual int onTrackInfo();
    virtual int onAudioError(int errCode);
    virtual int onTrackReady();
    virtual int onBufferReady();
    virtual int onStreamEnd();
    virtual int onAudioPortEvent(uint8_t portFlag);
    virtual int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
    virtual int getCurrentPosMs();
    virtual void setCachedPosMs(int posMs);

public:
    StateMachineContext *m_owner;
    int m_stateId;
};

class StateStarting : public PlaybackStateBase {
public:
    StateStarting(StateMachineContext *owner, int *outErr);
    virtual ~StateStarting();

    virtual int onEnter();
    virtual int onPlay(int playParam);
    virtual int onPause(uint8_t resetPos, int fadeParam);
    virtual int onStop();
    virtual int onSetShuffle(int shuffleVal);
    virtual int onSetEqualizer(int eqType, int bandLow, int bandMid);
    virtual int onTrackInfo();
    virtual int onTrackReady();
    virtual int onAudioPortEvent(uint8_t portFlag);
    virtual int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
};

class StateStopping : public PlaybackStateBase {
public:
    StateStopping(StateMachineContext *owner, int *outErr);
    virtual ~StateStopping();

    virtual int onEnter();
    virtual int onDeactivate(int fadeMs, uint8_t syncFlag);
    virtual int onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag);
    virtual int onStop();
    virtual int onSeek(const uint32_t *seekParams);
    virtual int onSetRepeat(int repeatVal);
    virtual int onSuspendForDb(uint8_t suspendFlag);
    virtual int onAudioError(int errCode);
    virtual int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
};

class StateDeactivating : public PlaybackStateBase {
public:
    StateDeactivating(StateMachineContext *owner, int *outErr);
    virtual ~StateDeactivating();

    virtual int onEnter();
    virtual int onBufferReady();
    virtual int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
};

class StateActivating : public PlaybackStateBase {
public:
    StateActivating(StateMachineContext *owner, int *outErr);
    virtual ~StateActivating();

    virtual int onEnter();
    virtual int onSetShuffle(int shuffleVal);
    virtual int onStreamEnd();
    virtual int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
};

class StateReady : public PlaybackStateBase {
public:
    StateReady(StateMachineContext *owner, int *outErr);
    virtual ~StateReady();

    virtual int onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag);
    virtual int onPlay(int playParam);
    virtual int onPause(uint8_t resetPos, int fadeParam);
    virtual int onSeekAndPlay(const uint32_t *seekParams, int playParam);
    virtual int onSetMediaPlaying(MediaListAccess *media, int shuffleVal);
};

class StateIdle : public PlaybackStateBase {
public:
    StateIdle(StateMachineContext *owner, int *outErr);
    virtual ~StateIdle();

    virtual int onEnter();
    virtual int onActivate(const ActivateStateSnapshot *snap, int fadeMs);
};

class StatePlaying : public PlaybackStateBase {
public:
    StatePlaying(StateMachineContext *owner, int *outErr);
    virtual ~StatePlaying();

    virtual int onDeactivate(int fadeMs, uint8_t syncFlag);
    virtual int onSetMedia(MediaListAccess *media, int shuffleVal, uint8_t pauseFlag);
    virtual int onPause(uint8_t resetPos, int fadeParam);
    virtual int onSeek(const uint32_t *seekParams);
    virtual int onCancel();
    virtual int onSuspendForDb(uint8_t suspendFlag);
    virtual int onSeekAndPlay(const uint32_t *seekParams, int playParam);
    virtual int onSetMediaPlaying(MediaListAccess *media, int shuffleVal);
    virtual int onAudioError(int errCode);
    virtual int onAudioPortEvent(uint8_t portFlag);
    virtual int getCurrentPosMs();
    virtual void setCachedPosMs(int posMs);
};

class StateStopped {
public:
    StateStopped(StateMachine *owner, void *sharedTable);
    virtual ~StateStopped();

    int onDeactivate();
    int onSetMedia(MediaListInterface *media, int shuffleSeed, unsigned char pauseFlag);
    int onPlay(int playParam);
    int onPause(unsigned char resetPos);
    int onSeek(const unsigned int *seekParams);
    void onSetRepeat() {}
    int onSetShuffle(int shuffleSeed);
    int onSetVolume(int volume, int ramp);
    int onSetAudioNormalizer(int alcMode);
    int onSuspendForDb(unsigned char flag);
    int onSeekAndPlay(const unsigned int *seekParams, int playParam);
    int onSetMediaPlaying(MediaListInterface *media, int shuffleSeed);

private:
    StateMachine *m_owner;
    int m_stateId;
};

struct StateMachine {
    StateMachine(void *owner);
    virtual ~StateMachine();

    virtual int notifyTrackInfo();
    virtual int notifyAudioError();
    virtual int notifyBufferReady();
    virtual int notifyStreamEnd();
    virtual int notifyMediaChanged(unsigned char hasNewMedia, void *newMedia,
                                   unsigned char hasUriFlag, const void *stateSnap,
                                   void **ioOldMedia, unsigned char decrementUri);

    int attach(void *audioOut, void *decoderMgr, void *sharedSync);
    int buildSnapshot(StateSnapshotOut *outSnap);
    int dispatchCmd(uint16_t cmdCode, const StateDispatchReq *req, StateSnapshotOut *outSnap,
                    int *outTargetTail, unsigned char *outFlags);
    int shouldDeferCmd(uint16_t cmdCode, uint16_t *outWaitCode);
    int getCurrentPosMs();
    int64_t getCurrentTrackId();
    int transitionTo(int targetStateId);
    int populateMetadataSafe(const TrackInfoData *trackInfo, TrackMetadataBuffers *metaBufs);

    int seekActiveMedia(const unsigned int *seekParams, MediaListInterface *media,
                        unsigned char isNewMedia);
    int seekPendingMedia(const unsigned int *seekParams, MediaListInterface *media,
                         unsigned char *outHasPending, unsigned char isNewMedia);
    int swapActiveMedia(MediaListInterface *newMedia, unsigned char hasUriFlag,
                        unsigned char seekInitial, unsigned char hasNewMedia);
    int setPendingMedia(MediaListInterface *newMedia);

public:
    void *m_owner;
    unsigned char m_stateIdle[12];
    unsigned char m_stateInit[12];
    unsigned char m_stateReady[12];
    unsigned char m_stateStarting[12];
    unsigned char m_statePlaying[12];
    unsigned char m_statePaused[12];
    StateStopped m_stateStopped;
    unsigned char m_stateStopping[12];
    void *m_decoderMgr;
    void *m_sharedSync;
    void *m_audioOut;
    PlaybackStateBase *m_currentState;
    unsigned char m_flagPendingActive;
    unsigned char m_stopRequested;
    unsigned char m_hasPendingTrack;
    unsigned char m_dbRemovedError;
    unsigned char m_speedDirChanged;
    unsigned char m_seekFailed;
    unsigned char m_padStatus[2];
    void *m_pendingMedia;
    int m_pendingSeekStep;
    int m_pendingVolumeMode;
    int m_padPendingAlign;
    TrackInfoData m_pendingTrackInfo;
    int m_padPendingTail;
    int m_lastDbErr;
    int m_extraSeekErr;
    int m_targetState;
    int m_pendingFadeMs;
    int m_playParam;
    unsigned char m_needDecoderRestart;
    unsigned char m_padRestart[3];
    MediaListInterface *m_activeMedia;
    int m_currentPosMs;
    int m_prevTrackPos;
    int m_nextTrackPos;
    TrackInfoData m_activeTrackInfo;
    int m_padActiveTail;
    MediaListStatus m_mediaListStatus;
    int m_repeatMode;
    int m_shuffleSeed;
    int m_seekMode;
    int m_speedMode;
    int m_volumeMode;
    int m_audioBalance;
    int m_eqType;
    int m_eqBandLow;
    int m_eqBandMid;
    int m_eqBandHigh;
    int m_audioNormalizer;
    int m_hasPendingError;
    int m_lastErrorType;
    int m_extraErr;
    TrackMetadataBuffers *m_metaBufs;
    int m_cachedPosMs;
};

int dbSeekNormalTarget(unsigned int seekCmd, MediaListInterface *media,
                       unsigned char isNewMedia, int speedMode, int repeatMode,
                       int *ioShuffleSeed, int *outStep, void *decMgr,
                       unsigned char *outChanged, TrackInfoData *outTrackInfo,
                       int *outExtraErr, unsigned char allowReshuffle);
int dbSeekSpecialTarget(MediaListInterface *media, int64_t prevPlaylistId,
                        int repeatMode, int speedMode, int *ioShuffleSeed,
                        int *outStep, void *decMgr, unsigned char keepIfSame,
                        unsigned char checkEmpty, TrackInfoData *outTrackInfo,
                        unsigned char *outHasTrack);

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_STATE_MACHINE_H */
