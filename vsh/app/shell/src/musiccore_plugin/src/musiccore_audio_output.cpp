/*
 * SceShell musiccore_plugin - AudioOutput (BGM audio port, PCM ring queue,
 * volume/dB ramp curves, EQ/ALC, and BGM priority adoption).
 * Original address range (3.65-CEX): 0x811E6D64 - 0x811E91C4
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, size_t len);
void *sce_paf_memcpy(void *dst, const void *src, size_t len);
void *sce_paf_memmove(void *dst, const void *src, size_t len);

int sceAudioOutOpenPort(int type, int len, int freq, int mode);
int sceAudioOutReleasePort(int port);
int sceAudioOutOutput(int port, const void *buf);
int sceAudioOutSetVolume(int port, int flag, const int *vol);
int sceAudioOutSetConfig(int port, SceSize len, int freq, int mode);
int sceAudioOutSetEffectType(int type);
int sceAudioOutSetAlcMode(int mode);
int sceAudioOutGetAdopt(int type);

int sceAppMgrAcquireBgmPortWithPriority(int priority);
int sceAppMgrReleaseBgmPort(void);
int sceAppMgrGetCurrentBgmState2(void *state);

int sceShellAcquireSoundOutExclusive(int mode, int reserved, int flags, int extraParam, int auxParam);
int sceShellReleaseSoundOutExclusive(void);

int sceKernelSetEventFlag(SceUID evfid, unsigned int bitpattern);
int sceKernelClearEventFlag(SceUID evfid, unsigned int bitpattern);
int sceKernelWaitEventFlag(SceUID evfid, unsigned int bitpattern, unsigned int waitMode,
                           unsigned int *resultPat, SceUInt *timeout);
int sceKernelDeleteEventFlag(SceUID evfid);
int sceKernelDeleteMutex(SceUID mutexid);
int sceKernelUnlockMutex(SceUID mutexid, int unlockCount);
int sceKernelDeleteThread(SceUID thid);
int sceKernelDelayThread(SceUInt delay);
int sceKernelExitThread(int status);
}

#include "musiccore_heap.h"

namespace sce {
namespace MusicCore {

static const int MUSICCORE_ERR_FAILED = -1879113726;      /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;   /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */
static const int MUSICCORE_ERR_UNDERRUN = -1879113711;    /* 0x8FFF0011 */
static const int MUSICCORE_ERR_BUSY = -1879113710;        /* 0x8FFF0012 */
static const int MUSICCORE_ERR_QUEUE_FULL = -1879113709;  /* 0x8FFF0013 */

static const int KERNEL_ERR_INVALID_ID = -2147319801;     /* 0x80028007 */
static const int KERNEL_ERR_NO_MEMORY = -2147323637;      /* 0x8002710B */
static const int AUDIOOUT_ERR_PORT_FULL = -2144993278;    /* 0x80260002 */
static const int AUDIOOUT_ERR_NO_MEMORY = -2144993267;    /* 0x8026000D */

static const int AUDIOOUT_PORT_TYPE_BGM = 1;
static const int AUDIOOUT_VOL_FLAG_LR = 0x3;
static const int AUDIOOUT_VOL_MAX = 0x8000;
static const int AUDIOOUT_MAX_SLOTS = 16;
static const int AUDIOOUT_PCM_BUF_SIZE = 0x2000;
static const int AUDIOOUT_THREAD_PRIORITY = 0x100000E1;
static const int AUDIOOUT_EXCLUSIVE_PRIORITY = 0x7FFFFFFF;
static const float AUDIOOUT_MIN_DB = -96.4f;

enum AudioOutStatusBits {
    AOUT_FLAG_STARTED = 0x01,
    AOUT_FLAG_HAS_DATA = 0x02,
    AOUT_FLAG_BGM_WATCH = 0x10
};

enum AudioFadeMode {
    FADE_MODE_IN = 0,
    FADE_MODE_OUT = 1,
    FADE_MODE_TARGET = 2,
    FADE_MODE_IDLE = 3
};

struct VolumeGainEntry {
    double db;
    double linear;
};

static const VolumeGainEntry kVolumeGainTable[15] = {
    {   0.0, 32768.000 },
    {  -1.0, 29204.511 },
    {  -3.0, 23197.967 },
    {  -5.0, 18426.801 },
    {  -7.0, 14636.928 },
    { -10.0, 10362.151 },
    { -15.0,  5827.066 },
    { -20.0,  3276.800 },
    { -25.0,  1842.680 },
    { -30.0,  1036.215 },
    { -40.0,   327.680 },
    { -50.0,   103.622 },
    { -60.0,    32.768 },
    { -75.0,     5.827 },
    { -96.4,     0.496 }
};

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

struct BgmCurrentState {
    uint8_t header[8];
    int priority;
    uint8_t trailing[20];
};

static int mapKernelError(int err)
{
    if (err == KERNEL_ERR_INVALID_ID) {
        return MUSICCORE_ERR_BUSY;
    }
    if (err == KERNEL_ERR_NO_MEMORY) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    return MUSICCORE_ERR_FAILED;
}

static int mapAudioOutError(int err)
{
    if (err == AUDIOOUT_ERR_NO_MEMORY) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    return MUSICCORE_ERR_FAILED;
}

static int roundToInt(float val)
{
    double rounded = (val >= 0.0f) ? ((double)val + 0.5) : ((double)val - 0.5);
    return (int)rounded;
}

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

    /* 0x811E6D64 */
    AudioOutput();

    /* 0x811E6E22 */
    void resetBasicFields();

    /* 0x811E6E52 */
    ~AudioOutput();

    /* 0x811E6E54 */
    int setCallbacks(BgmAdoptCallback adoptCb, FadeCompleteCallback fadeCb, void *ctx);

    /* 0x811E6E68 */
    int setPositionCallback(PositionCallback posCb, void *ctx);

    /* 0x811E6E72 */
    int setBgmWatchEnabled(uint8_t enable);

    /* 0x811E6EF8 */
    int setBgmPriority(int priority, uint8_t triggerWatch);

    /* 0x811E6F14 */
    int setExclusiveBgmPriority(int priority, const SoundOutExclusiveParam *param, uint8_t triggerWatch);

    /* 0x811E6F5E */
    int initialize(const AudioOutputInitParam *initParam, int skipPcmAlloc);

    /* 0x811E7278 */
    int finalize();

    /* 0x811E732E */
    static float linearToDb(float linearVol);

    /* 0x811E7402 */
    int openPort(unsigned int linearVol, const AudioEqParam *eqParam, unsigned int alc);

    /* 0x811E7584 */
    int closePort();

    /* 0x811E7670 */
    int startOutput();

    /* 0x811E7718 */
    static int startOutputThunk(AudioOutput *self);

    /* 0x811E7720 */
    int stopOutput();

    /* 0x811E77A0 */
    static int stopOutputThunk(AudioOutput *self);

    /* 0x811E77A8 */
    int enqueuePcm(int timestampMs, int channels, int rate, int byteSize,
                   void *pcmData, void (*releaseCb)(void *));

    /* 0x811E78A8 */
    static int enqueuePcmThunk(AudioOutput *self, int timestampMs, int channels, int rate,
                               int byteSize, void *pcmData, void (*releaseCb)(void *));

    /* 0x811E78C0 */
    int flushQueue();

    /* 0x811E797E */
    static int flushQueueThunk(AudioOutput *self);

    /* 0x811E7986 */
    int setScaleFade(int modeCmd, int durationMs, uint8_t *outActive);

    /* 0x811E7AEC */
    int setVolumeOrDbFade(unsigned int volCmd, int durationMs, uint8_t *outActive);

    /* 0x811E7CC6 */
    int setEffectType(int effectType);

    /* 0x811E7D38 */
    int setAlcMode(int mode);

    /* 0x811E7DA6 */
    int setMuted(uint8_t muted, int *outDelayMs);

    /* 0x811E7ED8 */
    float calcDbFadeDelta(float curOffsetDb, float targetDb, float stepMs, int remainMs);

    /* 0x811E8166 */
    float calcScaleFadeDelta(float curScale, float targetScale, float stepMs, int remainMs);

    /* 0x811E8222 */
    void applyFadeOutRamp();

    /* 0x811E83D0 */
    int fillActivePcmBuffer();

    /* 0x811E84F8 */
    int prepareOutputBuffer();

    /* 0x811E85B0 */
    int updateAudioConfig(int *outTimestampMs);

    /* 0x811E872A */
    int updateBgmAdoption();

    /* 0x811E88CE */
    static float dbToLinear(float db);

    /* 0x811E89A2 */
    int runOutputLoop();

    /* 0x811E91B0 */
    static int outputThreadEntry(SceSize args, void *argp);
};

/* 0x811E6D64 */
AudioOutput::AudioOutput()
{
    isMuted = 0;
    initialized = 0;
    closePortRequested = 0;
    openPortRequested = 0;
    quitRequested = 0;
    onStart = NULL;
    onStop = NULL;
    onEnqueue = NULL;
    onFlush = NULL;
    outputThreadId = -1;
    mainStatusFlagId = -1;
    totalMutexId = -1;
    stopRequested = 0;
    stoppedFlagId = -1;
    channelCount = 0;
    grainSize = 0;
    sampleRate = 0;
    audioMode = 0;
    currentVolume = AUDIOOUT_VOL_MAX;
    queuedSlotCount = 0;
    eqEffectType = 0;
    eqBandLow = 0;
    eqBandMid = 0;
    eqBandHigh = 0;
    alcMode = 0;
    audioPort = -1;
    pcmQueue = NULL;
    queueWriteIdx = 0;
    queueReadIdx = 0;
    positionCallback = NULL;
    positionCallbackCtx = NULL;
    lastReportedTimeMs = -1;
    bgmAdoptCallback = NULL;
    bgmAdoptCallbackCtx = NULL;
    currentAdoptedPriority = -1;
    targetBgmPriority = -1;
    appliedBgmPriority = -1;
    fadeCompleteCallback = NULL;
    fadeCompleteCallbackCtx = NULL;
    targetLinearVolume = AUDIOOUT_VOL_MAX;
    baseVolumeDb = 0.0f;
    scaleFadeRemainMs = 0;
    scaleFadeMode = FADE_MODE_IDLE;
    currentFadeScale = 1.0f;
    dbFadeRemainMs = 0;
    dbFadeMode = FADE_MODE_IDLE;
    currentFadeDbOffset = 0.0f;
    exclusiveParam.mode = 5;
    exclusiveParam.reserved = 0;
    exclusiveParam.flags = 0;
    exclusiveParam.param = 0;
    exclusiveParam.extraParam = 0;
    exclusiveParam.auxParam = 0;
    pcmOutBuf[0] = NULL;
    pcmOutBuf[1] = NULL;
    pcmOutBuf[2] = NULL;
    activeBufIndex = 0;
}

/* 0x811E6E22 */
void AudioOutput::resetBasicFields()
{
    initialized = 0;
    pcmOutBuf[0] = NULL;
    pcmOutBuf[1] = NULL;
    pcmOutBuf[2] = NULL;
    channelCount = 0;
    grainSize = 0;
    sampleRate = 0;
    audioMode = 0;
    currentVolume = AUDIOOUT_VOL_MAX;
    queuedSlotCount = 0;
    eqEffectType = 0;
    eqBandLow = 0;
    eqBandMid = 0;
    eqBandHigh = 0;
    alcMode = 0;
    onStart = NULL;
    onStop = NULL;
    onEnqueue = NULL;
    onFlush = NULL;
}

/* 0x811E6E52 */
AudioOutput::~AudioOutput()
{
}

/* 0x811E6E54 */
int AudioOutput::setCallbacks(BgmAdoptCallback adoptCb, FadeCompleteCallback fadeCb, void *ctx)
{
    bgmAdoptCallback = adoptCb;
    bgmAdoptCallbackCtx = ctx;
    fadeCompleteCallback = fadeCb;
    fadeCompleteCallbackCtx = ctx;
    return 0;
}

/* 0x811E6E68 */
int AudioOutput::setPositionCallback(PositionCallback posCb, void *ctx)
{
    positionCallbackCtx = ctx;
    positionCallback = posCb;
    return 0;
}

/* 0x811E6E72 */
int AudioOutput::setBgmWatchEnabled(uint8_t enable)
{
    int ret;
    if (enable == 1) {
        ret = sceKernelSetEventFlag(mainStatusFlagId, AOUT_FLAG_BGM_WATCH);
    } else {
        ret = sceKernelClearEventFlag(mainStatusFlagId, ~AOUT_FLAG_BGM_WATCH);
    }
    if (ret < 0) {
        return mapKernelError(ret);
    }
    return 0;
}

/* 0x811E6EF8 */
int AudioOutput::setBgmPriority(int priority, uint8_t triggerWatch)
{
    targetBgmPriority = priority;
    if (triggerWatch == 1) {
        return setBgmWatchEnabled(1);
    }
    return 0;
}

/* 0x811E6F14 */
int AudioOutput::setExclusiveBgmPriority(int priority, const SoundOutExclusiveParam *param, uint8_t triggerWatch)
{
    targetBgmPriority = priority;
    if (priority == AUDIOOUT_EXCLUSIVE_PRIORITY && param != NULL) {
        exclusiveParam = *param;
    }
    if (triggerWatch == 1) {
        return setBgmWatchEnabled(1);
    }
    return 0;
}

/* 0x811E6F5E */
int AudioOutput::initialize(const AudioOutputInitParam *initParam, int skipPcmAlloc)
{
    if (initialized == 1) {
        return 0;
    }
    if (initParam == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    SceKernelMutexOptParam mutexOpt;
    sce_paf_memset(&mutexOpt, 0, sizeof(mutexOpt));
    mutexOpt.size = sizeof(mutexOpt);
    mutexOpt.ceilingPriority = AUDIOOUT_THREAD_PRIORITY;

    SceUID mutexId = sceKernelCreateMutex("SceShellMusicCoreAOutTotal", 0, 0, &mutexOpt);
    if (mutexId < 0) {
        return mapKernelError(mutexId);
    }

    totalMutexId = mutexId;
    quitRequested = 0;
    queueWriteIdx = 0;
    queueReadIdx = 0;
    activeBufIndex = 0;
    channelCount = 0;
    grainSize = 0;
    sampleRate = 0;
    audioMode = 0;
    currentVolume = AUDIOOUT_VOL_MAX;
    queuedSlotCount = 0;
    eqEffectType = 0;
    eqBandLow = 0;
    eqBandMid = 0;
    eqBandHigh = 0;
    alcMode = 0;
    initialized = 1;

    sceKernelLockMutex(mutexId, 1, NULL);
    audioPort = -1;
    sampleRate = initParam->sampleRate;
    grainSize = initParam->grainSize;
    audioMode = initParam->audioMode;
    if ((uint8_t)audioMode == 1) {
        channelCount = 2;
    } else if ((uint8_t)audioMode == 0) {
        channelCount = 1;
    }
    sceKernelUnlockMutex(totalMutexId, 1);

    onStart = &AudioOutput::startOutputThunk;
    onStop = &AudioOutput::stopOutputThunk;
    onEnqueue = &AudioOutput::enqueuePcmThunk;
    onFlush = &AudioOutput::flushQueueThunk;

    SceUID thid = sceKernelCreateThread("SceShellMusicCoreAudioOutput",
                                        &AudioOutput::outputThreadEntry,
                                        AUDIOOUT_THREAD_PRIORITY,
                                        0x1000u, 0, 0, NULL);
    if (thid < 0) {
        return mapKernelError(thid);
    }
    outputThreadId = thid;

    SceUID evfId = sceKernelCreateEventFlag("SceShellMusicCoreAOutMainStatus", 0, 0, NULL);
    if (evfId < 0) {
        return mapKernelError(evfId);
    }
    mainStatusFlagId = evfId;

    int ret = sceKernelSetEventFlag(evfId, AOUT_FLAG_BGM_WATCH);
    if (ret != 0) {
        return mapKernelError(ret);
    }

    SceUID stopEvfId = sceKernelCreateEventFlag("SceShellMusicCoreAOutStopped", 0, 0, NULL);
    if (stopEvfId < 0) {
        return mapKernelError(stopEvfId);
    }
    stoppedFlagId = stopEvfId;

    pcmQueue = static_cast<AudioPcmSlot *>(localAlloc(sizeof(AudioPcmSlot) * AUDIOOUT_MAX_SLOTS));
    if (pcmQueue == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }

    if (skipPcmAlloc == 0) {
        pcmOutBuf[0] = localAlloc(AUDIOOUT_PCM_BUF_SIZE);
        if (pcmOutBuf[0] == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        pcmOutBuf[1] = localAlloc(AUDIOOUT_PCM_BUF_SIZE);
        if (pcmOutBuf[1] == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        pcmOutBuf[2] = localAlloc(AUDIOOUT_PCM_BUF_SIZE);
        if (pcmOutBuf[2] == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
    }

    for (int i = 0; i < AUDIOOUT_MAX_SLOTS; ++i) {
        pcmQueue[i].inUse = 0;
    }

    AudioOutput *selfPtr = this;
    ret = sceKernelStartThread(outputThreadId, sizeof(selfPtr), &selfPtr);
    if (ret < 0) {
        return mapKernelError(ret);
    }
    return 0;
}

/* 0x811E7278 */
int AudioOutput::finalize()
{
    if (initialized == 0) {
        return 0;
    }

    int port = audioPort;
    initialized = 0;
    if (port != -1) {
        sceKernelLockMutex(totalMutexId, 1, NULL);
        audioPort = -1;
        sceKernelUnlockMutex(totalMutexId, 1);
        sceAudioOutReleasePort(port);
    }

    localFree(pcmQueue);
    localFree(pcmOutBuf[0]);
    localFree(pcmOutBuf[1]);
    localFree(pcmOutBuf[2]);
    pcmQueue = NULL;
    pcmOutBuf[0] = NULL;
    pcmOutBuf[1] = NULL;
    pcmOutBuf[2] = NULL;

    onStart = NULL;
    onStop = NULL;
    onEnqueue = NULL;
    onFlush = NULL;
    quitRequested = 1;

    sceKernelSetEventFlag(mainStatusFlagId, 0xFFFFFFFFu);
    if (sceKernelWaitThreadEnd(outputThreadId, NULL, NULL) >= 0) {
        sceKernelDeleteThread(outputThreadId);
    }
    outputThreadId = -1;
    sceKernelDeleteEventFlag(mainStatusFlagId);
    sceKernelDeleteEventFlag(stoppedFlagId);

    if (totalMutexId != -1) {
        sceKernelDeleteMutex(totalMutexId);
        totalMutexId = -1;
    }
    return 0;
}

/* 0x811E732E */
float AudioOutput::linearToDb(float linearVol)
{
    double val = (double)linearVol;
    double hiDb = 0.0;
    double loDb = 0.0;
    double hiLin = 0.0;
    double loLin = 0.0;

    if (val >= kVolumeGainTable[7].linear) {
        for (int idx = 0; idx <= 7; ++idx) {
            if (val >= kVolumeGainTable[idx].linear) {
                if (idx == 0) {
                    return (float)kVolumeGainTable[0].db;
                }
                hiDb = kVolumeGainTable[idx - 1].db;
                loLin = kVolumeGainTable[idx].linear;
                loDb = kVolumeGainTable[idx].db;
                hiLin = kVolumeGainTable[idx - 1].linear;
                break;
            }
        }
    } else {
        int idx;
        for (idx = 7; idx < 15; ++idx) {
            if (val >= kVolumeGainTable[idx].linear) {
                if (idx == 0) {
                    return (float)kVolumeGainTable[0].db;
                }
                hiDb = kVolumeGainTable[idx - 1].db;
                loLin = kVolumeGainTable[idx].linear;
                loDb = kVolumeGainTable[idx].db;
                hiLin = kVolumeGainTable[idx - 1].linear;
                break;
            }
        }
        if (idx >= 15) {
            return (float)kVolumeGainTable[14].db;
        }
    }

    double slope = (hiDb - loDb) / (hiLin - loLin);
    return (float)(loDb + (val - loLin) * slope);
}

/* 0x811E7402 */
int AudioOutput::openPort(unsigned int linearVol, const AudioEqParam *eqParam, unsigned int alc)
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }
    if (eqParam == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }

    if (audioPort != -1) {
        openPortRequested = 0;
        sceKernelUnlockMutex(totalMutexId, 1);
        return 0;
    }

    targetLinearVolume = linearVol;
    if (linearVol != 0) {
        float db = linearToDb((float)linearVol);
        baseVolumeDb = (db < AUDIOOUT_MIN_DB) ? AUDIOOUT_MIN_DB : db;
    } else {
        baseVolumeDb = AUDIOOUT_MIN_DB;
    }

    currentFadeScale = 0.0f;
    currentFadeDbOffset = AUDIOOUT_MIN_DB;
    currentVolume = AUDIOOUT_VOL_MAX;
    alcMode = (alc >= 2u) ? 0 : (int)alc;

    unsigned int eqType = eqParam->effectType;
    if (eqType > 4u) {
        eqType = 0;
        eqBandLow = 0;
        eqBandMid = 0;
        eqBandHigh = 0;
    }
    eqEffectType = (int)eqType;

    int status = 0;
    openPortRequested = 1;
    do {
        sceKernelUnlockMutex(totalMutexId, 1);
        sceKernelDelayThread(20000);
        int lockRet = sceKernelLockMutex(totalMutexId, 1, NULL);
        if (lockRet < 0) {
            openPortRequested = 0;
            status = mapKernelError(lockRet);
            break;
        }
    } while (openPortRequested == 1);

    if (audioPort == -1) {
        status = MUSICCORE_ERR_FAILED;
    }
    sceKernelUnlockMutex(totalMutexId, 1);
    return status;
}

/* 0x811E7584 */
int AudioOutput::closePort()
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }

    if (audioPort == -1) {
        closePortRequested = 0;
        sceKernelUnlockMutex(totalMutexId, 1);
        return 0;
    }

    closePortRequested = 1;
    int status = 0;
    do {
        sceKernelUnlockMutex(totalMutexId, 1);
        sceKernelDelayThread(20000);
        int lockRet = sceKernelLockMutex(totalMutexId, 1, NULL);
        if (lockRet < 0) {
            closePortRequested = 0;
            status = mapKernelError(lockRet);
            break;
        }
    } while (closePortRequested == 1);

    if (audioPort != -1) {
        status = MUSICCORE_ERR_FAILED;
    }
    sceKernelUnlockMutex(totalMutexId, 1);
    return status;
}

/* 0x811E7670 */
int AudioOutput::startOutput()
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }

    stopRequested = 0;
    int flagRet = sceKernelSetEventFlag(mainStatusFlagId, AOUT_FLAG_STARTED);
    sceKernelUnlockMutex(totalMutexId, 1);
    if (flagRet < 0) {
        return mapKernelError(flagRet);
    }
    return 0;
}

/* 0x811E7718 */
int AudioOutput::startOutputThunk(AudioOutput *self)
{
    return self->startOutput();
}

/* 0x811E7720 */
int AudioOutput::stopOutput()
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }

    stopRequested = 1;
    unsigned int outBits = 0;
    int ret = sceKernelWaitEventFlag(stoppedFlagId, 1u, 0, &outBits, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }
    return 0;
}

/* 0x811E77A0 */
int AudioOutput::stopOutputThunk(AudioOutput *self)
{
    return self->stopOutput();
}

/* 0x811E77A8 */
int AudioOutput::enqueuePcm(int timestampMs, int channels, int rate, int byteSize,
                            void *pcmData, void (*releaseCb)(void *))
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }

    if (queuedSlotCount >= AUDIOOUT_MAX_SLOTS) {
        sceKernelUnlockMutex(totalMutexId, 1);
        return MUSICCORE_ERR_QUEUE_FULL;
    }

    if (sceKernelSetEventFlag(mainStatusFlagId, AOUT_FLAG_HAS_DATA) < 0) {
        sceKernelUnlockMutex(totalMutexId, 1);
    }

    AudioPcmSlot *slot = &pcmQueue[queueWriteIdx];
    slot->sampleRate = rate;
    slot->remainingFrames = byteSize / (2 * channels);
    slot->timestampMs = timestampMs;
    slot->channelCount = channels;
    slot->pcmData = pcmData;
    slot->releaseCallback = releaseCb;
    slot->inUse = 1;

    queueWriteIdx = (queueWriteIdx + 1) % AUDIOOUT_MAX_SLOTS;
    queuedSlotCount += 1;
    sceKernelUnlockMutex(totalMutexId, 1);
    return 0;
}

/* 0x811E78A8 */
int AudioOutput::enqueuePcmThunk(AudioOutput *self, int timestampMs, int channels, int rate,
                                 int byteSize, void *pcmData, void (*releaseCb)(void *))
{
    return self->enqueuePcm(timestampMs, channels, rate, byteSize, pcmData, releaseCb);
}

/* 0x811E78C0 */
int AudioOutput::flushQueue()
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }

    int wasStopped = stopRequested;
    int status = stopOutput();
    if (status == 0) {
        int clearRet = sceKernelClearEventFlag(mainStatusFlagId, ~AOUT_FLAG_HAS_DATA);
        if (clearRet >= 0) {
            for (int i = 0; i < AUDIOOUT_MAX_SLOTS; ++i) {
                int idx = (queueReadIdx + i) % AUDIOOUT_MAX_SLOTS;
                if (pcmQueue[idx].inUse != 0) {
                    pcmQueue[idx].releaseCallback(pcmQueue[idx].pcmData);
                    pcmQueue[idx].inUse = 0;
                }
            }
            queueWriteIdx = 0;
            queueReadIdx = 0;
            queuedSlotCount = 0;
        } else {
            status = mapKernelError(clearRet);
        }
    }

    if (wasStopped == 0) {
        return startOutput();
    }
    return status;
}

/* 0x811E797E */
int AudioOutput::flushQueueThunk(AudioOutput *self)
{
    return self->flushQueue();
}

/* 0x811E7986 */
int AudioOutput::setScaleFade(int modeCmd, int durationMs, uint8_t *outActive)
{
    if (initialized != 1) {
        *outActive = 0;
        return MUSICCORE_ERR_FAILED;
    }
    if (durationMs < 0) {
        *outActive = 0;
        return MUSICCORE_ERR_INVALID_ARG;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        *outActive = 0;
        return mapKernelError(ret);
    }

    int status = 0;
    int remainMs = 0;
    if (modeCmd == -1) {
        if ((unsigned int)scaleFadeMode > 2u || currentFadeScale >= 1.0f) {
            remainMs = 0;
        } else {
            remainMs = (int)((double)durationMs * (1.0 - (double)currentFadeScale));
        }
        scaleFadeRemainMs = remainMs;
        scaleFadeMode = FADE_MODE_IN;
    } else if (modeCmd == -2) {
        if (currentFadeScale > 0.0f) {
            if (currentFadeScale > 1.0f) {
                currentFadeScale = 1.0f;
            }
            remainMs = (int)((float)durationMs * currentFadeScale);
        } else {
            remainMs = 0;
        }
        scaleFadeRemainMs = remainMs;
        scaleFadeMode = FADE_MODE_OUT;
    } else {
        status = MUSICCORE_ERR_INVALID_ARG;
    }

    *outActive = (status == 0 && remainMs > 0) ? 1 : 0;
    sceKernelUnlockMutex(totalMutexId, 1);
    return status;
}

/* 0x811E7AEC */
int AudioOutput::setVolumeOrDbFade(unsigned int volCmd, int durationMs, uint8_t *outActive)
{
    if (initialized != 1) {
        *outActive = 0;
        return MUSICCORE_ERR_FAILED;
    }

    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        *outActive = 0;
        return mapKernelError(ret);
    }

    int status = 0;
    if (durationMs < -1) {
        durationMs = -1;
    }

    if (volCmd == (unsigned int)-1) {
        if (durationMs != -1) {
            dbFadeRemainMs = (currentFadeDbOffset >= 0.0f) ? 0 : durationMs;
        }
        dbFadeMode = FADE_MODE_IN;
    } else if (volCmd == (unsigned int)-2) {
        if (durationMs != -1) {
            dbFadeRemainMs = durationMs;
        }
        dbFadeMode = FADE_MODE_OUT;
    } else if (volCmd > (unsigned int)AUDIOOUT_VOL_MAX) {
        status = MUSICCORE_ERR_INVALID_ARG;
    } else {
        float prevBaseDb = baseVolumeDb;
        targetLinearVolume = volCmd;
        float newBaseDb;
        if (volCmd != 0) {
            newBaseDb = linearToDb((float)volCmd);
            if (newBaseDb < AUDIOOUT_MIN_DB) {
                newBaseDb = AUDIOOUT_MIN_DB;
            }
        } else {
            newBaseDb = AUDIOOUT_MIN_DB;
        }
        baseVolumeDb = newBaseDb;

        float shiftedOffset = (prevBaseDb - newBaseDb) + currentFadeDbOffset;
        if (shiftedOffset > 0.0f) {
            shiftedOffset = 0.0f;
        } else if (shiftedOffset < AUDIOOUT_MIN_DB) {
            shiftedOffset = AUDIOOUT_MIN_DB;
        }
        currentFadeDbOffset = shiftedOffset;

        if (durationMs == -1) {
            if (dbFadeMode == FADE_MODE_IDLE) {
                dbFadeMode = FADE_MODE_TARGET;
            }
        } else {
            dbFadeRemainMs = durationMs;
            dbFadeMode = FADE_MODE_TARGET;
        }
    }

    *outActive = (status == 0 && dbFadeRemainMs > 0) ? 1 : 0;
    sceKernelUnlockMutex(totalMutexId, 1);
    return status;
}

/* 0x811E7CC6 */
int AudioOutput::setEffectType(int effectType)
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }
    if (effectType < 0 || effectType > 4) {
        return MUSICCORE_ERR_INVALID_ARG;
    }
    if (eqEffectType == effectType) {
        return 0;
    }
    if (audioPort >= 0) {
        int ret = sceAudioOutSetEffectType(effectType);
        if (ret < 0) {
            return mapAudioOutError(ret);
        }
    }
    eqEffectType = effectType;
    return 0;
}

/* 0x811E7D38 */
int AudioOutput::setAlcMode(int mode)
{
    if (initialized != 1) {
        return MUSICCORE_ERR_FAILED;
    }
    if ((unsigned int)mode > 1u) {
        return MUSICCORE_ERR_INVALID_ARG;
    }
    if (alcMode == mode) {
        return 0;
    }
    if (audioPort >= 0) {
        int ret = sceAudioOutSetAlcMode(mode);
        if (ret < 0) {
            return mapAudioOutError(ret);
        }
    }
    alcMode = mode;
    return 0;
}

/* 0x811E7DA6 */
int AudioOutput::setMuted(uint8_t muted, int *outDelayMs)
{
    int ret = sceKernelLockMutex(totalMutexId, 1, NULL);
    if (ret < 0) {
        return mapKernelError(ret);
    }

    int status = 0;
    if (audioPort < 0) {
        isMuted = muted;
    } else {
        int volLevel = (muted == 1) ? 0 : currentVolume;
        int vol[2] = { volLevel, volLevel };
        int volRet = sceAudioOutSetVolume(audioPort, AUDIOOUT_VOL_FLAG_LR, vol);
        if (volRet != 0) {
            status = mapAudioOutError(volRet);
        } else {
            isMuted = muted;
        }
    }

    int64_t delayUs = 0;
    if (status == 0) {
        int clampedGrain = (grainSize > 1024) ? 1024 : grainSize;
        delayUs = (1000000LL * (int64_t)clampedGrain) / (int64_t)sampleRate;
    }

    sceKernelUnlockMutex(totalMutexId, 1);

    if (status == 0) {
        if (outDelayMs == NULL) {
            sceKernelDelayThread((SceUInt)delayUs);
        } else {
            *outDelayMs = (int)(delayUs / 1000LL);
        }
    }
    return status;
}

/* 0x811E7ED8 */
float AudioOutput::calcDbFadeDelta(float curOffsetDb, float targetDb, float stepMs, int remainMs)
{
    int64_t targetMilliDb = (int64_t)(double)(targetDb * 1000.0f);
    int64_t currentMilliDb = (int64_t)(double)((baseVolumeDb + curOffsetDb) * 1000.0f);

    if (targetMilliDb > 0) {
        targetMilliDb = 0;
    } else if (targetMilliDb < -96400LL) {
        targetMilliDb = -96400LL;
    }

    if (currentMilliDb > 0) {
        currentMilliDb = 0;
    } else if (currentMilliDb < -96400LL) {
        currentMilliDb = -96400LL;
    }

    if (remainMs > 0 && stepMs < (float)remainMs) {
        float scaled = ((float)(targetMilliDb - currentMilliDb) * stepMs) / (float)remainMs;
        int64_t deltaMilliDb = (int64_t)((double)scaled + 0.5);
        int64_t nextMilliDb = currentMilliDb + deltaMilliDb;
        if (deltaMilliDb < 0) {
            if (nextMilliDb > targetMilliDb) {
                return (float)deltaMilliDb / 1000.0f;
            }
        } else {
            if (nextMilliDb < targetMilliDb) {
                return (float)deltaMilliDb / 1000.0f;
            }
        }
    }

    return (float)(targetMilliDb - currentMilliDb) / 1000.0f;
}

/* 0x811E8166 */
float AudioOutput::calcScaleFadeDelta(float curScale, float targetScale, float stepMs, int remainMs)
{
    int targetMilli = (int)(targetScale * 1000.0f);
    int currentMilli = (int)(curScale * 1000.0f);

    if (targetMilli > 1000) {
        targetMilli = 1000;
    } else if (targetMilli < 0) {
        targetMilli = 0;
    }

    if (currentMilli > 1000) {
        currentMilli = 1000;
    } else if (currentMilli < 0) {
        currentMilli = 0;
    }

    if (remainMs > 0 && stepMs < (float)remainMs) {
        float scaled = ((float)(targetMilli - currentMilli) * stepMs) / (float)remainMs;
        int deltaMilli = roundToInt(scaled);
        int nextMilli = currentMilli + deltaMilli;
        if (deltaMilli >= 0) {
            if (nextMilli < targetMilli) {
                return (float)deltaMilli / 1000.0f;
            }
        } else {
            if (nextMilli > targetMilli) {
                return (float)deltaMilli / 1000.0f;
            }
        }
    }

    return targetScale - curScale;
}

/* 0x811E8222 */
void AudioOutput::applyFadeOutRamp()
{
    int channels = channelCount;
    int frames = grainSize;
    int frameStride = 2 * (channels & 0x7F);
    int sampleBuf[8];

    if (frames <= 0) {
        return;
    }

    int byteOffset = 0;
    for (int frame = 0; frame < frames; ++frame) {
        uint8_t *pcm = static_cast<uint8_t *>(pcmOutBuf[activeBufIndex]);
        for (int ch = 0; ch < channels; ++ch) {
            int pos = byteOffset + ch * 2;
            sampleBuf[ch] = (int)pcm[pos] | ((int)(signed char)pcm[pos + 1] << 8);
        }

        if (frame >= frames / 2) {
            for (int ch = 0; ch < channels; ++ch) {
                int pos = byteOffset + ch * 2;
                pcm = static_cast<uint8_t *>(pcmOutBuf[activeBufIndex]);
                sampleBuf[ch] = (sampleBuf[ch] * frame) / frames;
                pcm[pos] = 0;
                pcm[pos + 1] = 0;
                channels = channelCount;
                frames = grainSize;
            }
        } else {
            int rampNum = frames - 2 * frame;
            for (int ch = 0; ch < channels; ++ch) {
                int pos = byteOffset + ch * 2;
                int scaled = (sampleBuf[ch] * rampNum) / frames;
                pcm = static_cast<uint8_t *>(pcmOutBuf[activeBufIndex]);
                sampleBuf[ch] = scaled;
                pcm[pos] = (uint8_t)(scaled & 0xFF);
                pcm[pos + 1] = (uint8_t)((scaled >> 8) & 0xFF);
                channels = channelCount;
                frames = grainSize;
            }
        }

        byteOffset += frameStride;
    }
}

/* 0x811E83D0 */
int AudioOutput::fillActivePcmBuffer()
{
    int channels = channelCount;
    int framesFilled = 0;
    int status = 0;
    int slotIdx = queueReadIdx % AUDIOOUT_MAX_SLOTS;
    int frameBytes = 2 * (channels & 0x7F);

    for (int consumed = 0; consumed < AUDIOOUT_MAX_SLOTS; ++consumed) {
        AudioPcmSlot *slot = &pcmQueue[slotIdx];
        uint8_t *dst = static_cast<uint8_t *>(pcmOutBuf[activeBufIndex]) + framesFilled * frameBytes;

        if (slot->inUse == 0 || slot->sampleRate != sampleRate || slot->channelCount != channels) {
            sce_paf_memset(dst, 0, (size_t)(grainSize - framesFilled) * (size_t)frameBytes);
            status = (slot->inUse == 0) ? MUSICCORE_ERR_UNDERRUN : MUSICCORE_ERR_FAILED;
            break;
        }

        if (framesFilled + slot->remainingFrames > grainSize) {
            int takeFrames = grainSize - framesFilled;
            sce_paf_memcpy(dst, slot->pcmData, (size_t)takeFrames * (size_t)frameBytes);
            uint8_t *srcBytes = static_cast<uint8_t *>(slot->pcmData);
            sce_paf_memmove(srcBytes, srcBytes + takeFrames * frameBytes,
                            (size_t)(slot->remainingFrames - takeFrames) * (size_t)frameBytes);
            slot->remainingFrames -= takeFrames;
            break;
        }

        sce_paf_memcpy(dst, slot->pcmData, (size_t)slot->remainingFrames * (size_t)frameBytes);
        framesFilled += slot->remainingFrames;
        slot->remainingFrames = 0;
        slot->inUse = 0;
        queuedSlotCount -= 1;
        slot->releaseCallback(slot->pcmData);

        channels = channelCount;
        slotIdx = (queueReadIdx + consumed + 1) % AUDIOOUT_MAX_SLOTS;
    }

    queueReadIdx = slotIdx;
    return status;
}

/* 0x811E84F8 */
int AudioOutput::prepareOutputBuffer()
{
    int needRamp = 0;
    unsigned int clearMask = 0;
    int status = fillActivePcmBuffer();

    if (status != 0) {
        needRamp = 1;
        if (status == MUSICCORE_ERR_UNDERRUN) {
            clearMask = AOUT_FLAG_HAS_DATA;
        }
    } else {
        int remainingTotal = 0;
        for (int i = 0; i < AUDIOOUT_MAX_SLOTS; ++i) {
            AudioPcmSlot *slot = &pcmQueue[(queueReadIdx + i) % AUDIOOUT_MAX_SLOTS];
            if (slot->inUse == 0 || slot->sampleRate != sampleRate || slot->channelCount != channelCount) {
                break;
            }
            remainingTotal += slot->remainingFrames;
        }
        if (remainingTotal == 0) {
            needRamp = 1;
        }
    }

    if (stopRequested == 1) {
        clearMask |= AOUT_FLAG_STARTED;
        applyFadeOutRamp();
    } else if (needRamp == 1) {
        applyFadeOutRamp();
    }

    if (clearMask != 0) {
        sceKernelClearEventFlag(mainStatusFlagId, ~clearMask);
    }
    return status;
}

/* 0x811E85B0 */
int AudioOutput::updateAudioConfig(int *outTimestampMs)
{
    AudioPcmSlot *slot = &pcmQueue[queueReadIdx];
    if (slot->inUse == 0) {
        return 0;
    }

    int newChannels = channelCount;
    int newGrain = grainSize;
    int newRate = sampleRate;
    int slotRate = slot->sampleRate;
    int slotChannels = slot->channelCount;
    int modeFlag = (newChannels != 1) ? 1 : 0;
    bool configChanged = false;

    if (newRate != slotRate) {
        newRate = slotRate;
        newGrain = 2048;
        if ((2048000000LL / (int64_t)slotRate) > 60000LL) {
            int rawGrain = (int)((60000LL * (int64_t)slotRate) / 1000000LL);
            newGrain = (rawGrain / 64) * 64;
            if (newGrain > 1024) {
                newGrain = (newGrain / 1024) * 1024;
            }
        }
        configChanged = true;
    }

    if (newChannels != slotChannels) {
        newChannels = slotChannels;
        if (slotChannels == 1) {
            modeFlag = 0;
        } else if (slotChannels == 2) {
            modeFlag = 1;
        }
        configChanged = true;
    }

    if (configChanged) {
        int newMode = modeFlag | (audioMode & ~0xFF);
        int ret = sceAudioOutSetConfig(audioPort, (SceSize)newGrain, newRate, newMode);
        if (ret < 0) {
            return mapAudioOutError(ret);
        }
        channelCount = newChannels;
        grainSize = newGrain;
        sampleRate = newRate;
        audioMode = newMode;
    }

    *outTimestampMs = pcmQueue[queueReadIdx].timestampMs;
    return 0;
}

/* 0x811E872A */
int AudioOutput::updateBgmAdoption()
{
    int targetPrio = targetBgmPriority;
    int prevAdopted = currentAdoptedPriority;

    if (targetPrio != appliedBgmPriority) {
        int ret;
        if (targetPrio == -1) {
            if (prevAdopted == AUDIOOUT_EXCLUSIVE_PRIORITY) {
                ret = sceShellReleaseSoundOutExclusive();
            } else {
                ret = sceAppMgrReleaseBgmPort();
            }
        } else {
            if (prevAdopted == AUDIOOUT_EXCLUSIVE_PRIORITY) {
                if (sceShellReleaseSoundOutExclusive() != 0) {
                    return MUSICCORE_ERR_FAILED;
                }
            }
            if (targetPrio == AUDIOOUT_EXCLUSIVE_PRIORITY) {
                ret = sceShellAcquireSoundOutExclusive(exclusiveParam.mode, 0,
                                                       exclusiveParam.flags,
                                                       exclusiveParam.extraParam,
                                                       exclusiveParam.auxParam);
            } else {
                ret = sceAppMgrAcquireBgmPortWithPriority(targetPrio);
            }
        }
        if (ret != 0) {
            return MUSICCORE_ERR_FAILED;
        }
    }

    if (targetPrio == -1) {
        currentAdoptedPriority = -1;
    } else if (targetPrio == AUDIOOUT_EXCLUSIVE_PRIORITY) {
        currentAdoptedPriority = AUDIOOUT_EXCLUSIVE_PRIORITY;
    } else {
        int adopt = sceAudioOutGetAdopt(AUDIOOUT_PORT_TYPE_BGM);
        if (adopt < 0) {
            return mapAudioOutError(adopt);
        }
        if (adopt == 0) {
            currentAdoptedPriority = -1;
        } else {
            currentAdoptedPriority = -1;
            BgmCurrentState bgmState;
            if (sceAppMgrGetCurrentBgmState2(&bgmState) == 0 && bgmState.priority <= targetPrio) {
                currentAdoptedPriority = targetPrio;
            }
        }
    }

    if (bgmAdoptCallback != NULL) {
        if (targetPrio != appliedBgmPriority) {
            int adopted = (currentAdoptedPriority != -1) ? 1 : 0;
            bgmAdoptCallback(bgmAdoptCallbackCtx, targetPrio, adopted);
        } else if (prevAdopted != currentAdoptedPriority) {
            if (currentAdoptedPriority == -1) {
                bgmAdoptCallback(bgmAdoptCallbackCtx, prevAdopted, 0);
            } else {
                bgmAdoptCallback(bgmAdoptCallbackCtx, currentAdoptedPriority, 1);
            }
        }
    }

    appliedBgmPriority = targetPrio;
    return 0;
}

/* 0x811E88CE */
float AudioOutput::dbToLinear(float db)
{
    double val = (double)db;
    double hiDb = 0.0;
    double loDb = 0.0;
    double hiLin = 0.0;
    double loLin = 0.0;

    if (val >= kVolumeGainTable[7].db) {
        for (int idx = 0; idx <= 7; ++idx) {
            if (val >= kVolumeGainTable[idx].db) {
                if (idx == 0) {
                    return (float)kVolumeGainTable[0].linear;
                }
                hiDb = kVolumeGainTable[idx - 1].db;
                loDb = kVolumeGainTable[idx].db;
                hiLin = kVolumeGainTable[idx - 1].linear;
                loLin = kVolumeGainTable[idx].linear;
                break;
            }
        }
    } else {
        int idx;
        for (idx = 7; idx < 15; ++idx) {
            if (val >= kVolumeGainTable[idx].db) {
                if (idx == 0) {
                    return (float)kVolumeGainTable[0].linear;
                }
                hiDb = kVolumeGainTable[idx - 1].db;
                loDb = kVolumeGainTable[idx].db;
                hiLin = kVolumeGainTable[idx - 1].linear;
                loLin = kVolumeGainTable[idx].linear;
                break;
            }
        }
        if (idx >= 15) {
            return (float)kVolumeGainTable[14].linear;
        }
    }

    double slope = (loLin - hiLin) / (loDb - hiDb);
    return (float)(hiLin + (val - hiDb) * slope);
}

/* 0x811E89A2 */
int AudioOutput::runOutputLoop()
{
    int pendingTimestampMs = 0;

    while (initialized != 0) {
        unsigned int outBits = 0;
        if (sceKernelWaitEventFlag(mainStatusFlagId, AOUT_FLAG_STARTED | AOUT_FLAG_BGM_WATCH,
                                   1u, &outBits, NULL) < 0) {
            continue;
        }
        if (quitRequested == 1) {
            return 0;
        }

        if ((outBits & AOUT_FLAG_BGM_WATCH) != 0 && isMuted == 0) {
            updateBgmAdoption();
        }

        int lockRet = sceKernelLockMutex(totalMutexId, 1, NULL);
        if (lockRet < 0) {
            openPortRequested = 0;
            closePortRequested = 0;
            return mapKernelError(lockRet);
        }

        if (openPortRequested == 1) {
            int port = sceAudioOutOpenPort(AUDIOOUT_PORT_TYPE_BGM, grainSize, sampleRate, audioMode);
            while (port == AUDIOOUT_ERR_PORT_FULL) {
                sceKernelDelayThread(20000);
                port = sceAudioOutOpenPort(AUDIOOUT_PORT_TYPE_BGM, grainSize, sampleRate, audioMode);
            }
            if (port >= 0) {
                int initVol = 0;
                float effectiveDb = ((baseVolumeDb + currentFadeDbOffset) - AUDIOOUT_MIN_DB) * currentFadeScale + AUDIOOUT_MIN_DB;
                if (effectiveDb >= AUDIOOUT_MIN_DB) {
                    initVol = roundToInt(dbToLinear(effectiveDb));
                }

                int applyVol = (isMuted == 1) ? 0 : initVol;
                int vol[2] = { applyVol, applyVol };
                int volRet = sceAudioOutSetVolume(port, AUDIOOUT_VOL_FLAG_LR, vol);
                if (isMuted == 1 || volRet == 0) {
                    currentVolume = initVol;
                }

                if ((unsigned int)eqEffectType > 4u) {
                    eqEffectType = 0;
                } else {
                    sceAudioOutSetEffectType(eqEffectType);
                }

                if ((unsigned int)alcMode > 1u) {
                    alcMode = 0;
                } else {
                    sceAudioOutSetAlcMode(alcMode);
                }

                audioPort = port;
            }
        }
        openPortRequested = 0;

        if (closePortRequested == 1) {
            sceAudioOutSetEffectType(0);
            sceAudioOutSetAlcMode(0);
            eqEffectType = 0;
            eqBandLow = 0;
            eqBandMid = 0;
            eqBandHigh = 0;
            alcMode = 0;

            if (audioPort != -1) {
                currentVolume = AUDIOOUT_VOL_MAX;
                sceAudioOutOutput(audioPort, NULL);
                int port = audioPort;
                while (sceAudioOutReleasePort(port) == AUDIOOUT_ERR_PORT_FULL) {
                    sceKernelDelayThread(20000);
                }
            }

            currentVolume = 0;
            scaleFadeRemainMs = 0;
            scaleFadeMode = FADE_MODE_IDLE;
            currentFadeScale = 1.0f;
            dbFadeRemainMs = 0;
            dbFadeMode = FADE_MODE_IDLE;
            currentFadeDbOffset = 0.0f;
            audioPort = -1;
        }
        closePortRequested = 0;

        float stepMs = 50.0f;
        int outputStatus = MUSICCORE_ERR_FAILED;
        if (audioPort == -1 || (outBits & AOUT_FLAG_STARTED) == 0 || (outBits & AOUT_FLAG_HAS_DATA) == 0) {
            sceKernelSetEventFlag(stoppedFlagId, 1u);
        } else {
            sceKernelClearEventFlag(stoppedFlagId, ~1u);
            if (activeBufIndex == 0) {
                activeBufIndex = 1;
            } else {
                activeBufIndex = 0;
            }

            outputStatus = updateAudioConfig(&pendingTimestampMs);
            if (outputStatus == 0) {
                outputStatus = prepareOutputBuffer();
            }
        }

        if (outputStatus == 0 && grainSize != 0) {
            stepMs = ((float)grainSize * 1000.0f) / (float)sampleRate;
        }

        int prevScaleMode = scaleFadeMode;
        unsigned int prevDbMode = dbFadeMode;
        float scaleDelta = 0.0f;
        float dbDelta = 0.0f;
        bool fadeStepped = false;

        if (prevScaleMode == FADE_MODE_IN) {
            scaleDelta = calcScaleFadeDelta(currentFadeScale, 1.0f, stepMs, scaleFadeRemainMs);
            fadeStepped = true;
        } else if (prevScaleMode == FADE_MODE_OUT) {
            scaleDelta = calcScaleFadeDelta(currentFadeScale, 0.0f, stepMs, scaleFadeRemainMs);
            fadeStepped = true;
        }

        if (prevDbMode == FADE_MODE_IN || prevDbMode == FADE_MODE_TARGET) {
            dbDelta = calcDbFadeDelta(currentFadeDbOffset, baseVolumeDb, stepMs, dbFadeRemainMs);
            fadeStepped = true;
        } else if (prevDbMode == FADE_MODE_OUT) {
            dbDelta = calcDbFadeDelta(currentFadeDbOffset, AUDIOOUT_MIN_DB, stepMs, dbFadeRemainMs);
            fadeStepped = true;
        }

        int deferredFadeOutVol = 0;
        bool fadeFinished = false;
        if (fadeStepped) {
            currentFadeDbOffset += dbDelta;
            currentFadeScale += scaleDelta;
            float totalDb = ((baseVolumeDb + currentFadeDbOffset) - AUDIOOUT_MIN_DB) * currentFadeScale + AUDIOOUT_MIN_DB;
            int linearVol = 0;
            if (totalDb > AUDIOOUT_MIN_DB) {
                linearVol = roundToInt(dbToLinear(totalDb));
            }

            if (isMuted != 0 || audioPort == -1) {
                currentVolume = linearVol;
            } else if (linearVol != currentVolume) {
                if (prevDbMode == FADE_MODE_OUT) {
                    deferredFadeOutVol = linearVol;
                } else {
                    int vol[2] = { linearVol, linearVol };
                    if (sceAudioOutSetVolume(audioPort, AUDIOOUT_VOL_FLAG_LR, vol) == 0) {
                        currentVolume = linearVol;
                    }
                }
            }

            if (scaleFadeMode != FADE_MODE_IDLE) {
                bool done = false;
                if (scaleFadeMode == FADE_MODE_IN) {
                    if (currentFadeScale >= 1.0f ||
                        (unsigned int)linearVol == targetLinearVolume ||
                        (baseVolumeDb < AUDIOOUT_MIN_DB && linearVol == 0)) {
                        currentFadeScale = 1.0f;
                        done = true;
                    }
                } else if (scaleFadeMode == FADE_MODE_OUT) {
                    if (currentFadeScale <= 0.0f || linearVol == 0) {
                        currentFadeScale = 0.0f;
                        done = true;
                    }
                }

                float remain = (float)scaleFadeRemainMs;
                scaleFadeRemainMs = (remain > stepMs) ? roundToInt(remain - stepMs) : 0;
                if (done) {
                    scaleFadeMode = FADE_MODE_IDLE;
                }
            }

            if (dbFadeMode != FADE_MODE_IDLE) {
                bool done = false;
                if (dbFadeMode == FADE_MODE_IN) {
                    if (currentFadeDbOffset >= 0.0f ||
                        (unsigned int)linearVol == targetLinearVolume ||
                        (baseVolumeDb < AUDIOOUT_MIN_DB && linearVol == 0)) {
                        currentFadeDbOffset = 0.0f;
                        done = true;
                    }
                } else if (dbFadeMode == FADE_MODE_OUT) {
                    if (currentFadeDbOffset <= AUDIOOUT_MIN_DB || linearVol == 0) {
                        currentFadeDbOffset = AUDIOOUT_MIN_DB - baseVolumeDb;
                        done = true;
                    }
                } else if (dbFadeMode == FADE_MODE_TARGET) {
                    float combinedDb = baseVolumeDb + currentFadeDbOffset;
                    int checkVol = 0;
                    if (combinedDb >= AUDIOOUT_MIN_DB) {
                        checkVol = roundToInt(dbToLinear(combinedDb));
                    }
                    if (checkVol > AUDIOOUT_VOL_MAX) {
                        checkVol = AUDIOOUT_VOL_MAX;
                    }
                    if ((unsigned int)checkVol == targetLinearVolume) {
                        currentFadeDbOffset = 0.0f;
                        done = true;
                    }
                }

                float remain = (float)dbFadeRemainMs;
                dbFadeRemainMs = (remain > stepMs) ? roundToInt(remain - stepMs) : 0;
                if (done) {
                    dbFadeMode = FADE_MODE_IDLE;
                }
            }

            if (prevScaleMode != scaleFadeMode || prevDbMode != dbFadeMode) {
                fadeFinished = true;
            }
        }

        sceKernelUnlockMutex(totalMutexId, 1);

        if (fadeFinished && fadeCompleteCallback != NULL && fadeCompleteCallbackCtx != NULL) {
            fadeCompleteCallback(fadeCompleteCallbackCtx, prevScaleMode, prevDbMode);
        }

        if (outputStatus != 0) {
            sceKernelDelayThread(50000);
        } else {
            if (pendingTimestampMs >= 0) {
                if (positionCallback != NULL) {
                    positionCallback(positionCallbackCtx, pendingTimestampMs);
                }
                lastReportedTimeMs = pendingTimestampMs;
                pendingTimestampMs = -1;
            }

            if (sceAudioOutOutput(audioPort, pcmOutBuf[activeBufIndex]) >= 0 && prevScaleMode == FADE_MODE_OUT) {
                if (isMuted != 0 || audioPort == -1) {
                    currentVolume = deferredFadeOutVol;
                } else if (deferredFadeOutVol != currentVolume) {
                    int vol[2] = { deferredFadeOutVol, deferredFadeOutVol };
                    if (sceAudioOutSetVolume(audioPort, AUDIOOUT_VOL_FLAG_LR, vol) == 0) {
                        currentVolume = deferredFadeOutVol;
                    }
                }
            }
        }
    }

    return 0;
}

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* 0x811E91B0 */
int AudioOutput::outputThreadEntry(SceSize args, void *argp)
{
    (void)args;
    if (argp != NULL) {
        AudioOutput *self = *static_cast<AudioOutput **>(argp);
        self->runOutputLoop();
    }
    SceThreadmgrCoredumpTime_sceKernelExitThread(0);
    return 0;
}

struct AudioInitParams;

void initAudioOutput(void *audioOut)
{
    new (audioOut) AudioOutput();
}

void stopAudioOutput(void *audioOut)
{
    if (audioOut != NULL)
        static_cast<AudioOutput *>(audioOut)->stopOutput();
}

void termAudioOutput(void *audioOut)
{
    if (audioOut != NULL) {
        static_cast<AudioOutput *>(audioOut)->finalize();
        static_cast<AudioOutput *>(audioOut)->~AudioOutput();
    }
}

int audioOutputSetCallbacks(void *audioOut, void *focusCb, void *errCb, void *userData)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setCallbacks(
              reinterpret_cast<AudioOutput::BgmAdoptCallback>(focusCb),
              reinterpret_cast<AudioOutput::FadeCompleteCallback>(errCb), userData)
        : -1;
}

int audioOutputSetPriority(void *audioOut, int priority, int flag)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setBgmPriority(priority, static_cast<uint8_t>(flag))
        : -1;
}

int audioOutputSetPriorityEx(void *audioOut, int priority, const void *extData, int flag)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setExclusiveBgmPriority(
              priority, static_cast<const SoundOutExclusiveParam *>(extData), static_cast<uint8_t>(flag))
        : -1;
}

int audioOutputStart(void *audioOut, const AudioInitParams *params, int flags)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->initialize(
              reinterpret_cast<const AudioOutputInitParam *>(params), flags)
        : -1;
}

int audioOutputGetState(void *audioOut, unsigned char channelId)
{
    (void)channelId;
    return (audioOut != NULL) ? 0 : -1;
}

int audioOutputSetAlcMode(void *audioOut, int mode)
{
    return (audioOut != NULL) ? static_cast<AudioOutput *>(audioOut)->setAlcMode(mode) : -1;
}

int audioOutputSetStateCallback(void *audioOut, void *cb, void *ctx)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setPositionCallback(
              reinterpret_cast<AudioOutput::PositionCallback>(cb), ctx)
        : -1;
}

int audioOutputSetVolume(void *audioOut, int volCmd, int durationMs, unsigned char *outActive)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setVolumeOrDbFade(
              static_cast<unsigned int>(volCmd), durationMs, outActive)
        : -1;
}

int audioOutConfigure(void *audioOut, int balance, const void *eq, int normalizer)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->openPort(
              static_cast<unsigned int>(balance),
              static_cast<const AudioEqParam *>(eq),
              static_cast<unsigned int>(normalizer))
        : -1;
}

int audioOutShutdown(void *audioOut)
{
    return (audioOut != NULL) ? static_cast<AudioOutput *>(audioOut)->closePort() : -1;
}

int audioOutSetFade(void *audioOut, int targetState, int fadeMs, uint8_t *outPending)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setScaleFade(targetState, fadeMs, outPending)
        : -1;
}

int audioOutSetVolumeRamp(void *audioOut, int targetState, int fadeMs, uint8_t *outPending)
{
    return (audioOut != NULL)
        ? static_cast<AudioOutput *>(audioOut)->setVolumeOrDbFade(
              static_cast<unsigned int>(targetState), fadeMs, outPending)
        : -1;
}

int audioOutSetEqType(void *audioOut, int eqType)
{
    return (audioOut != NULL) ? static_cast<AudioOutput *>(audioOut)->setEffectType(eqType) : -1;
}

} // namespace MusicCore
} // namespace sce
