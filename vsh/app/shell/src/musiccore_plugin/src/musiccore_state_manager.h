/*
 * SceShell musiccore_plugin - StateManager class and command payload declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_STATE_MANAGER_H
#define VSH_APP_SHELL_MUSICCORE_STATE_MANAGER_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_queue.h"

namespace sce {
namespace MusicCore {

struct StateMediaSnapshot {
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

struct StateCmdPayload {
    StateMediaSnapshot media;
    int padMedia;
    int fadeMs;
    void *newMediaHandle;
    int resetPos;
    int seekTarget;
    int seekVolumeMode;
    int seekMode;
    int seekSpeedMode;
    int seekStep;
    int repeatValue;
    int shuffleValue;
    int padShuffle;
    int errorCode;
    int volumeLevel;
    int volumeRamp;
    int eqType;
    int eqBandLow;
    int eqBandMid;
    int eqBandHigh;
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

struct StateResultEvent {
    uint64_t cmdHdr;
    int errCode;
    int padErr;
    StateMediaSnapshot media;
    uint32_t origParam;
    uint32_t origCmdHi;
    unsigned char extraFlags[8];
};

struct StateQueueNode {
    void *vtable;
    int padAlign;
    uint64_t cmdHdr;
    void *inBuf;
    unsigned int inSize;
};

struct StateManagerCallbacks {
    void (*onResult)(const StateResultEvent *ev, void *userData);
    int (*onMessage)(const void *msg, void *userData, int timeout);
    void *userData;
};

struct DecoderTrackInfoMsg {
    int headerLow;
    int headerHigh;
    int64_t playlistId;
    int64_t trackId;
    int trackIndex;
    int positionMs;
    int currentTrackNum;
    int totalTracks;
    int durationMs;
    int sampleRate;
    int bitRate;
    int channels;
    int codecType;
    int sourceType;
    int volumeMode;
};

struct StateNotifyMsg {
    uint32_t param;
    uint32_t cmdHi;
    int errorCode;
    int notifyKind;
    int notifySubCode;
    int padAlign;
    StateMediaSnapshot media;
    int padMedia;
    int extraVal;
    int padTail;
};

struct StateMediaChangedMsg {
    uint32_t param;
    uint32_t cmdHi;
    int padHeader;
    unsigned char hasNewMedia;
    unsigned char padHasNew[3];
    void *newMedia;
    unsigned char hasUriFlag;
    unsigned char padHasUri[3];
    StateMediaSnapshot media;
    void *oldMedia;
    unsigned char decrementUri;
    unsigned char padDecrement[3];
};

class StateMachineOwner {
public:
    virtual int onTrackReadyCallback() = 0;
    virtual int onAudioErrorCallback(int errCode) = 0;
    virtual int forwardNotifyMetadata() = 0;
    virtual int onBufferReadyCallback() = 0;
    virtual int onTrackInfoCallback(const DecoderTrackInfoMsg *info) = 0;
    virtual int onStreamEndCallback(int val) = 0;
    virtual int onMediaChangedCallback(unsigned char hasNewMedia, void *newMedia,
                                       unsigned char hasUriFlag, const StateMediaSnapshot *state,
                                       void **ioOldMedia, unsigned char decrementUri) = 0;
};

class StateManager : public StateMachineOwner {
public:
    StateManager();

    virtual int onTrackReadyCallback();
    virtual int onAudioErrorCallback(int errCode);
    virtual int forwardNotifyMetadata();
    virtual int onBufferReadyCallback();
    virtual int onTrackInfoCallback(const DecoderTrackInfoMsg *info);
    virtual int onStreamEndCallback(int val);
    virtual int onMediaChangedCallback(unsigned char hasNewMedia, void *newMedia,
                                       unsigned char hasUriFlag, const StateMediaSnapshot *state,
                                       void **ioOldMedia, unsigned char decrementUri);

    virtual ~StateManager();

    virtual int start(const StateManagerCallbacks *cbs, const int *audioCfg);
    virtual int stop();

    virtual int activate(const StateMediaSnapshot *initState, void *metaBufs, int fadeMs,
                         uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int deactivate(int fadeMs, uint32_t origParam, uint32_t origCmdHi,
                           unsigned char isAsync, unsigned char syncFlag);
    virtual int seek(const int *seekParams, uint32_t origParam, uint32_t origCmdHi,
                     unsigned char isAsync);
    virtual int play(int playParam, uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int pause(unsigned char resetPos, int fadeParam, uint32_t origParam, uint32_t origCmdHi,
                      unsigned char isAsync);
    virtual int stopPlayback(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int setRepeat(int repeatVal, uint32_t origParam, uint32_t origCmdHi,
                          unsigned char isAsync);
    virtual int setShuffle(int shuffleVal, uint32_t origParam, uint32_t origCmdHi,
                           unsigned char isAsync);
    virtual int cancel(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int setMedia(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi, int shuffleVal,
                         unsigned char isAsync, int unusedFlag, unsigned char pauseFlag);
    virtual int setVolume(int vol, int ramp, uint32_t origParam, uint32_t origCmdHi,
                          unsigned char isAsync);
    virtual int setEqualizer(int eqType, int bandLow, int bandMid, int bandHigh,
                             uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int setAudioNormalizer(int mode, uint32_t origParam, uint32_t origCmdHi,
                                   unsigned char isAsync);
    virtual int seekAndPlay(const int *seekParams, int playParam, uint32_t origParam,
                            uint32_t origCmdHi, unsigned char isAsync);
    virtual int setMediaPlaying(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi,
                                int shuffleVal, unsigned char isAsync);
    virtual int onTrackReady(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int onAudioError(int errCode, uint32_t origParam, uint32_t origCmdHi,
                             unsigned char isAsync);
    virtual int onTrackInfo(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int onBufferReady(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int onStreamEnd(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync);
    virtual int onAudioPortEvent(int portEventKind, int portEventValue, uint32_t origParam,
                                 uint32_t origCmdHi, unsigned char isAsync);
    virtual int suspendForDb(uint32_t origParam, uint32_t origCmdHi, unsigned char suspendFlag,
                             unsigned char isAsync);

    int enqueueCmd(uint64_t cmdHdr, const StateCmdPayload *req);

    int checkAndDeferNode(StateQueueNode *node, unsigned char *outDeferred);
    int runThreadLoop();
    static int threadEntry(SceSize args, void *argp);

    static int thunkTrackReady(StateManager *self);
    static int thunkTrackInfo(StateManager *self);
    static int thunkBufferReady(StateManager *self);
    static int thunkAudioError(StateManager *self, int errCode);

private:
    SceUID m_initSema;
    SceUID m_threadId;
    CmdQueue *m_cmdQueue;
    StateManagerCallbacks m_cbs;
    int m_padAlign;
    struct StateMachineStorage {
        unsigned char head[116];
        void *activeMedia;
        unsigned char tail[4600 - 120];
    } m_stateMachine;
    unsigned char m_decoderMgrArena[56];
    uint64_t m_sharedSyncArena[8];
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_STATE_MANAGER_H */
