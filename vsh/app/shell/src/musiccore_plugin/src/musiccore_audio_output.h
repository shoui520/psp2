/*
 * SceShell musiccore_plugin - AudioOutput class and parameter declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_AUDIO_OUTPUT_H
#define VSH_APP_SHELL_MUSICCORE_AUDIO_OUTPUT_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

struct AudioPcmSlot {
    int sampleRate;
    int remainingFrames;
    int timestampMs;
    int channelCount;
    void *pcmData;
    void (*releaseCallback)(void *pcmData);
    int inUse;
};

struct AudioOutputInitParam {
    int grainSize;
    int sampleRate;
    int audioMode;
};

struct AudioEqParam {
    unsigned int effectType;
    int bandLow;
    int bandMid;
    int bandHigh;
};

struct SoundOutExclusiveParam {
    int mode;
    int reserved;
    int flags;
    int param;
    int extraParam;
    int auxParam;
};

class AudioOutput {
public:
    typedef int (*OutputStartFn)(AudioOutput *);
    typedef int (*OutputStopFn)(AudioOutput *);
    typedef int (*OutputEnqueueFn)(AudioOutput *, int, int, int, int, void *, void (*)(void *));
    typedef int (*OutputFlushFn)(AudioOutput *);
    typedef void (*PositionCallback)(void *, int);
    typedef void (*BgmAdoptCallback)(void *, int, int);
    typedef void (*FadeCompleteCallback)(void *, int, unsigned int);

    OutputStartFn onStart;
    OutputStopFn onStop;
    OutputEnqueueFn onEnqueue;
    OutputFlushFn onFlush;
    SceUID outputThreadId;
    uint8_t quitRequested;
    uint8_t padQuit[3];
    SceUID mainStatusFlagId;
    SceUID totalMutexId;
    int stopRequested;
    uint8_t openPortRequested;
    uint8_t closePortRequested;
    uint8_t padPortReq[2];
    SceUID stoppedFlagId;
    uint8_t initialized;
    uint8_t padInit[3];
    int channelCount;
    int grainSize;
    int sampleRate;
    int audioMode;
    int currentVolume;
    int queuedSlotCount;
    int eqEffectType;
    int eqBandLow;
    int eqBandMid;
    int eqBandHigh;
    int alcMode;
    int audioPort;
    void *pcmOutBuf[3];
    int activeBufIndex;
    AudioPcmSlot *pcmQueue;
    int queueWriteIdx;
    int queueReadIdx;
    PositionCallback positionCallback;
    void *positionCallbackCtx;
    int lastReportedTimeMs;
    BgmAdoptCallback bgmAdoptCallback;
    void *bgmAdoptCallbackCtx;
    int currentAdoptedPriority;
    int targetBgmPriority;
    int appliedBgmPriority;
    FadeCompleteCallback fadeCompleteCallback;
    void *fadeCompleteCallbackCtx;
    unsigned int targetLinearVolume;
    float baseVolumeDb;
    int scaleFadeRemainMs;
    int scaleFadeMode;
    float currentFadeScale;
    int dbFadeRemainMs;
    unsigned int dbFadeMode;
    float currentFadeDbOffset;
    uint8_t isMuted;
    uint8_t padMuted[3];
    SoundOutExclusiveParam exclusiveParam;

    AudioOutput();
    void resetBasicFields();
    ~AudioOutput();

    int setCallbacks(BgmAdoptCallback adoptCb, FadeCompleteCallback fadeCb, void *ctx);
    int setPositionCallback(PositionCallback posCb, void *ctx);
    int setBgmWatchEnabled(uint8_t enable);
    int setBgmPriority(int priority, uint8_t triggerWatch);
    int setExclusiveBgmPriority(int priority, const SoundOutExclusiveParam *param, uint8_t triggerWatch);
    int initialize(const AudioOutputInitParam *initParam, int skipPcmAlloc);
    int finalize();
    static float linearToDb(float linearVol);
    int openPort(unsigned int linearVol, const AudioEqParam *eqParam, unsigned int alc);
    int closePort();
    int startOutput();
    static int startOutputThunk(AudioOutput *self);
    int stopOutput();
    static int stopOutputThunk(AudioOutput *self);
    int enqueuePcm(int timestampMs, int channels, int rate, int byteSize,
                   void *pcmData, void (*releaseCb)(void *));
    static int enqueuePcmThunk(AudioOutput *self, int timestampMs, int channels, int rate,
                               int byteSize, void *pcmData, void (*releaseCb)(void *));
    int flushQueue();
    static int flushQueueThunk(AudioOutput *self);
    int setScaleFade(int modeCmd, int durationMs, uint8_t *outActive);
    int setVolumeOrDbFade(unsigned int volCmd, int durationMs, uint8_t *outActive);
    int setEffectType(int effectType);
    int setAlcMode(int mode);
    int setMuted(uint8_t muted, int *outDelayMs);
    float calcDbFadeDelta(float curOffsetDb, float targetDb, float stepMs, int remainMs);
    float calcScaleFadeDelta(float curScale, float targetScale, float stepMs, int remainMs);
    void applyFadeOutRamp();
    int fillActivePcmBuffer();
    int prepareOutputBuffer();
    int updateAudioConfig(int *outTimestampMs);
    int updateBgmAdoption();
    static float dbToLinear(float db);
    int runOutputLoop();
    static int outputThreadEntry(SceSize args, void *argp);
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_AUDIO_OUTPUT_H */
