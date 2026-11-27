/*
 * SceShell musiccore_plugin - controller client API wrappers.
 * Original address range (3.65-CEX): 0x811D2030 - 0x811D3484
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>
#include <memory/heap_allocator.h>

#include "musiccore_ctrl_client.h"

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
size_t sce_paf_strlen(const char *str);
int sceKernelGetSystemTime(SceKernelSysClock *clock);
}

static inline void *musiccoreAllocFromPafHeap(size_t size, void *heap)
{
    return operator new(size, static_cast<paf::memory::HeapAllocator *>(heap));
}

static const int MUSICCORE_ERR_FAILED = -1879113726;      /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;   /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */

namespace sce {
namespace MusicCore {

class BaseObject {
public:
    virtual ~BaseObject() {}
};

class ObjFactory : public BaseObject {
public:
    static unsigned int calcTotalSize(int numMusic, int numPlaylist, int numStream);
    ObjFactory(int numMusic, int numPlaylist, int numStream,
               unsigned int totalBytes, unsigned int headerSize);
    int createController(void **outCtrl);
    int destroyController(BaseObject *ctrl);
};

extern void *getLocalHeap();
extern void *localAlloc(SceSize size);
extern void localFree(void *ptr);

extern int controllerSendCommand(void *ctrl, uint64_t cmdHeader, const void *inBuf,
                                 unsigned int inSize, SceUInt *timeout, void *outResult);
extern int controllerGetContentInfo(void *ctrl, unsigned char channelId, void *infoBuf);
extern int controllerGetStatus(void *ctrl, unsigned char channelId, void *statusBuf,
                               void *outPos, void *outPosValid, int *outMode,
                               void *workBuf, unsigned char *outFlag, int queryFlags);
extern int controllerGetState(void *ctrl, unsigned char channelId, void *outState);

/* Polymorphic command payloads (vtables 0x81532CB0..0x81532DF0) */

struct PlayCmdPayload {
    virtual ~PlayCmdPayload() {}
    int fadeMs;
};

struct PauseCmdPayload {
    virtual ~PauseCmdPayload() {}
    unsigned char pauseMode;
    unsigned char padMode[3];
    int fadeMs;
};

struct ControlCmdPayload {
    virtual ~ControlCmdPayload() {}
    ControlParams control;
};

struct RepeatCmdPayload {
    virtual ~RepeatCmdPayload() {}
    int repeatMode;
};

struct ShuffleCmdPayload {
    virtual ~ShuffleCmdPayload() {}
    uint32_t shuffleSeed;
};

struct SetUriDataCmdPayload {
    virtual ~SetUriDataCmdPayload() {}
    int reserved;
    const char *uri;
    int listIndex;
    int trackIndex;
    int padTrackId;
    int64_t trackId;
    int shuffleFlag;
    int contentKind;
    int extraMeta;
    int padTail;
};

struct SetUriStringCmdPayload {
    virtual ~SetUriStringCmdPayload() {}
    int reserved;
    size_t uriLen;
    const char *uri;
    int uriFlags;
};

struct VolumeCmdPayload {
    virtual ~VolumeCmdPayload() {}
    int volume;
    int rampMs;
};

struct EqualizerCmdPayload {
    virtual ~EqualizerCmdPayload() {}
    int eqType;
    int bandLow;
    int bandMid;
    int bandHigh;
};

struct NormalizerCmdPayload {
    virtual ~NormalizerCmdPayload() {}
    int enableNormalizer;
};

struct SeekAndPlayCmdPayload {
    virtual ~SeekAndPlayCmdPayload() {}
    ControlParams control;
    int fadeMs;
};

struct CapCbCmdPayload {
    virtual ~CapCbCmdPayload() {}
    void *callback;
};

struct StatusCbCmdPayload {
    virtual ~StatusCbCmdPayload() {}
    void *callback;
};

struct ExtraCbCmdPayload {
    virtual ~ExtraCbCmdPayload() {}
    void *callback;
};

struct TimeCbCmdPayload {
    virtual ~TimeCbCmdPayload() {}
    void *callback;
    int intervalUs;
};

struct MediaCbCmdPayload {
    virtual ~MediaCbCmdPayload() {}
    void *callback;
};

struct DbOpenCmdPayload {
    virtual ~DbOpenCmdPayload() {}
    unsigned char suspendFlag;
};

struct DbQueryCmdPayload {
    virtual ~DbQueryCmdPayload() {}
    const void *trackIds;
    int trackCount;
    int queryOpt;
};

struct OpenChannelCmdPayload {
    virtual ~OpenChannelCmdPayload() {}
    signed char slot;
    unsigned char padSlot[3];
    int priority;
    int sampleRate;
    int channels;
    int grainSize;
    int reserved;
    int audioMode;
    int priorityHint;
};

struct CloseChannelCmdPayload {
    virtual ~CloseChannelCmdPayload() {}
};

struct IdleCbCmdPayload {
    virtual ~IdleCbCmdPayload() {}
    void *callback;
};

struct CmdResult {
    int errCode;
    unsigned char outByte;
};

struct RawPlaybackStatus {
    unsigned char bytes[3];
    unsigned char padHeader[25];
    int playState;
    int contentType;
    int padAlign;
    int64_t trackId;
    int64_t trackItemId;
    int64_t playlistId;
    int words[19];
    unsigned char tail[4];
};

static ObjFactory *g_ctrlFactory = NULL;
static void *g_ctrlInstance = NULL;
static paf::thread::Mutex g_ctrlObjMutex("SceShellMusicCoreCtrlObj", 0);

static uint64_t makeCmdHeader(uint32_t seqId, uint32_t cmdAndTarget)
{
    return ((uint64_t)cmdAndTarget << 32) | (uint64_t)seqId;
}

/* 0x811D2030 */
int ctrlInit()
{
    if (g_ctrlInstance != NULL)
        return 0;

    SceSize totalSize = ObjFactory::calcTotalSize(8, 1, 2);
    void *mem = localAlloc(totalSize);
    if (mem == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    ObjFactory *factory = new (mem) ObjFactory(8, 1, 2, totalSize, 24);
    g_ctrlFactory = factory;
    if (factory == NULL) {
        localFree(mem);
        return MUSICCORE_ERR_FAILED;
    }

    void *ctrl = NULL;
    int ret = factory->createController(&ctrl);
    if (ctrl == NULL)
        return MUSICCORE_ERR_NO_MEMORY;
    if (ret != 0) {
        g_ctrlFactory->destroyController((BaseObject *)ctrl);
        return ret;
    }

    g_ctrlObjMutex.Lock();
    g_ctrlInstance = ctrl;
    g_ctrlObjMutex.Unlock();
    return 0;
}

/* 0x811D2114 */
int ctrlTerm()
{
    if (g_ctrlInstance != NULL) {
        void *ctrl = g_ctrlInstance;
        g_ctrlObjMutex.Lock();
        g_ctrlInstance = NULL;
        g_ctrlObjMutex.Unlock();

        if (g_ctrlFactory != NULL) {
            g_ctrlFactory->destroyController((BaseObject *)ctrl);
            g_ctrlFactory->~ObjFactory();
            localFree(g_ctrlFactory);
            g_ctrlFactory = NULL;
        }
    }
    return 0;
}

/* 0x811D2174 */
int ctrlOpenSimple(uint32_t seqId, unsigned char channelType, int priority,
                   unsigned char *outSlot)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    OpenChannelCmdPayload payload;
    payload.slot = -1;
    payload.priority = priority;
    payload.sampleRate = 5;
    payload.channels = 0;
    payload.grainSize = 0;
    payload.reserved = 0;
    payload.audioMode = 0;
    payload.priorityHint = 0;

    CmdResult res;
    res.errCode = 0;
    res.outByte = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x810B0000u | channelType);
    int ret = controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, &res);
    if (ret == 0)
        *outSlot = res.outByte;
    return ret;
}

/* 0x811D2226 */
int ctrlOpenWithParams(uint32_t seqId, unsigned char channelType, int priority,
                       unsigned char *outSlot, const OpenParams *openParams)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    OpenChannelCmdPayload payload;
    payload.slot = -1;
    payload.priority = priority;
    payload.sampleRate = openParams->sampleRate;
    payload.channels = openParams->channels;
    payload.grainSize = openParams->grainSize;
    payload.reserved = 0;
    payload.audioMode = openParams->audioMode;
    payload.priorityHint = openParams->priorityHint;

    CmdResult res;
    res.errCode = 0;
    res.outByte = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x810B0000u | channelType);
    int ret = controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, &res);
    if (ret == 0)
        *outSlot = res.outByte;
    return ret;
}

/* 0x811D22F4 */
int ctrlClose(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    CloseChannelCmdPayload payload;
    uint64_t hdr = makeCmdHeader(seqId, 0x810C0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2372 */
int ctrlSetActiveSlot(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x810A0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

/* 0x811D23E4 */
int ctrlSetUriData(uint32_t seqId, unsigned char channelId,
                   const TrackContentDesc *contentDesc)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    if (contentDesc == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    SetUriDataCmdPayload payload;
    payload.reserved = 0;
    payload.uri = contentDesc->uri;
    payload.listIndex = contentDesc->listIndex;
    payload.trackIndex = contentDesc->trackIndex;
    payload.padTrackId = 0;
    payload.trackId = contentDesc->trackId;
    payload.shuffleFlag = contentDesc->shuffleFlag;
    payload.contentKind = contentDesc->contentKind;
    payload.extraMeta = contentDesc->extraMeta;
    payload.padTail = 0;

    uint64_t hdr = makeCmdHeader(seqId, 0x31110000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D24A2 */
int ctrlSetUriString(uint32_t seqId, unsigned char channelId, const char *uri,
                     int uriFlags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    if (uri == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    SetUriStringCmdPayload payload;
    payload.reserved = 0;
    size_t len = sce_paf_strlen(uri);
    if (len > 0x400u)
        len = 0x400u;
    payload.uriLen = len;
    payload.uri = uri;
    payload.uriFlags = uriFlags;

    uint64_t hdr = makeCmdHeader(seqId, 0x31120000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2564 */
int ctrlResumeAudio(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x311A0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

/* 0x811D25D6 */
int ctrlPlay(uint32_t seqId, unsigned char channelId, int fadeMs)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    PlayCmdPayload payload;
    payload.fadeMs = fadeMs;

    uint64_t hdr = makeCmdHeader(seqId, 0x310A0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2658 */
int ctrlPause(uint32_t seqId, unsigned char channelId, unsigned char pauseMode,
              int fadeMs)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    PauseCmdPayload payload;
    payload.pauseMode = pauseMode;
    payload.fadeMs = fadeMs;

    uint64_t hdr = makeCmdHeader(seqId, 0x310B0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D26E0 */
int ctrlControl(uint32_t seqId, unsigned char channelId,
                const ControlParams *ctrlParams)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    ControlCmdPayload payload;
    payload.control.seekTarget = -1;
    payload.control.speedValue = (int)0x80000000u;
    payload.control.trackIndex = -2;
    payload.control.seekMode = -2;
    payload.control.seekPositionMs = -2;
    sce_paf_memset(&payload.control, 0, sizeof(payload.control));
    payload.control = *ctrlParams;

    uint64_t hdr = makeCmdHeader(seqId, 0x310D0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D27B2 */
int ctrlSetRepeat(uint32_t seqId, unsigned char channelId, int repeatMode)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    RepeatCmdPayload payload;
    payload.repeatMode = repeatMode;

    uint64_t hdr = makeCmdHeader(seqId, 0x310E0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2834 */
int ctrlSetShuffle(uint32_t seqId, unsigned char channelId, int enableShuffle)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    ShuffleCmdPayload payload;
    if (enableShuffle != 0) {
        SceKernelSysClock clockVal;
        sceKernelGetSystemTime(&clockVal);
        payload.shuffleSeed = (clockVal.quad == 0) ? 0x00EFDD50u : (uint32_t)clockVal.quad;
    } else {
        payload.shuffleSeed = 0;
    }

    uint64_t hdr = makeCmdHeader(seqId, 0x310F0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2902 */
int ctrlSetVolume(uint32_t seqId, unsigned char channelId, int volume)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    VolumeCmdPayload payload;
    payload.volume = volume;
    payload.rampMs = -1;

    uint64_t hdr = makeCmdHeader(seqId, 0x31140000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D298C */
int ctrlSetEqualizer(uint32_t seqId, unsigned char channelId,
                     int eqType, int bandLow, int bandMid, int bandHigh)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    EqualizerCmdPayload payload;
    sce_paf_memset(&payload.eqType, 0, sizeof(int) * 4);
    payload.eqType = eqType;
    payload.bandLow = bandLow;
    payload.bandMid = bandMid;
    payload.bandHigh = bandHigh;

    uint64_t hdr = makeCmdHeader(seqId, 0x31150000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2A3E */
int ctrlSetAudioNormalizer(uint32_t seqId, unsigned char channelId, int enableNormalizer)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    NormalizerCmdPayload payload;
    payload.enableNormalizer = enableNormalizer;

    uint64_t hdr = makeCmdHeader(seqId, 0x31160000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2AC0 */
int ctrlSetIdleCallback(uint32_t seqId, void *callback)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    IdleCbCmdPayload payload;
    payload.callback = callback;

    uint64_t hdr = makeCmdHeader(seqId, 0x810D000Fu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2B3A */
int ctrlSetCapCallback(uint32_t seqId, void *callback)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    CapCbCmdPayload payload;
    payload.callback = callback;

    uint64_t hdr = makeCmdHeader(seqId, 0x8101000Fu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2BB4 */
int ctrlSetStatusCallback(uint32_t seqId, void *callback)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    StatusCbCmdPayload payload;
    payload.callback = callback;

    uint64_t hdr = makeCmdHeader(seqId, 0x810300FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2C30 */
int ctrlSetExtraCallback(uint32_t seqId, void *callback)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    ExtraCbCmdPayload payload;
    payload.callback = callback;

    uint64_t hdr = makeCmdHeader(seqId, 0x810400FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2CAA */
int ctrlSetTimeCallback(uint32_t seqId, void *callback, int intervalUs)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    TimeCbCmdPayload payload;
    payload.callback = callback;
    payload.intervalUs = intervalUs;

    uint64_t hdr = makeCmdHeader(seqId, 0x810500FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2D2E */
int ctrlSetMediaCallback(uint32_t seqId, void *callback)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    MediaCbCmdPayload payload;
    payload.callback = callback;

    uint64_t hdr = makeCmdHeader(seqId, 0x810600FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2DA8 */
int ctrlDbOpen(uint32_t seqId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    DbOpenCmdPayload payload;
    payload.suspendFlag = 0;

    uint64_t hdr = makeCmdHeader(seqId, 0x810700FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2E20 */
int ctrlDbSuspend(uint32_t seqId, unsigned char suspendFlag)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    DbOpenCmdPayload payload;
    payload.suspendFlag = suspendFlag;

    uint64_t hdr = makeCmdHeader(seqId, 0x810700FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2E9C */
int ctrlDbClose(uint32_t seqId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x810800FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

/* 0x811D2F04 */
int ctrlDbQuery(uint32_t seqId, const void *trackIds, int trackCount, int queryOpt)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    DbQueryCmdPayload payload;
    payload.trackIds = trackIds;
    payload.trackCount = trackCount;
    payload.queryOpt = queryOpt;

    uint64_t hdr = makeCmdHeader(seqId, 0x810900FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D2F86 */
int ctrlGetContentInfo(unsigned char channelId, ContentInfoBuffers *info)
{
    if (info == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int ret = (g_ctrlInstance == NULL) ? MUSICCORE_ERR_FAILED : 0;
    char *titlePtr = NULL;
    char *artistPtr = NULL;
    char *albumPtr = NULL;

    if (ret == 0) {
        titlePtr = info->titleBuf;
        artistPtr = info->artistBuf;
        albumPtr = info->albumBuf;
        ret = controllerGetContentInfo(g_ctrlInstance, channelId, info);
    }

    if (info->titleBuf != NULL && info->titleLen != 0) {
        if (ret != 0)
            info->titleBuf[0] = '\0';
        info->titleLen = sce_paf_strlen(titlePtr) + 1;
    } else {
        info->titleLen = 0;
    }

    if (info->artistBuf != NULL && info->artistLen != 0) {
        if (ret != 0)
            info->artistBuf[0] = '\0';
        info->artistLen = sce_paf_strlen(artistPtr) + 1;
    } else {
        info->titleLen = 0;
    }

    size_t albumLen = 0;
    if (info->albumBuf != NULL && info->albumLen != 0) {
        if (ret != 0)
            info->albumBuf[0] = '\0';
        albumLen = sce_paf_strlen(albumPtr) + 1;
    }
    info->albumLen = albumLen;
    return ret;
}

/* 0x811D305A */
int ctrlGetPlaybackStatus(unsigned char channelId, PlaybackStatusOut *outStatus,
                          void *outPos, void *outPosValid)
{
    void *heap = getLocalHeap();
    void *workBuf = musiccoreAllocFromPafHeap(2088, heap);
    if (workBuf == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    RawPlaybackStatus rawStatus;
    int mode = 4;
    unsigned char flag = 0;
    int ret = controllerGetStatus(g_ctrlInstance, channelId, &rawStatus, outPos, outPosValid,
                                  &mode, workBuf, &flag, 2);
    ::operator delete(workBuf);
    if (ret != 0)
        return ret;

    outStatus->bytes[0] = rawStatus.bytes[0];
    outStatus->bytes[1] = rawStatus.bytes[1];
    outStatus->bytes[2] = rawStatus.bytes[2];
    outStatus->flags = -1;
    outStatus->statusWordLow = 0;
    outStatus->statusWordHigh = 0;
    outStatus->playState = rawStatus.playState;
    outStatus->contentType = rawStatus.contentType;
    outStatus->trackId = rawStatus.trackId;
    outStatus->trackItemId = rawStatus.trackItemId;
    outStatus->playlistId = rawStatus.playlistId;
    for (int i = 0; i < 19; ++i)
        outStatus->words[i] = rawStatus.words[i];
    return 0;
}

/* 0x811D317A */
int ctrlGetPlaybackMode(unsigned char channelId, int unusedMode, int *outMode,
                        void *workBuf, unsigned char *outFlag)
{
    (void)unusedMode;
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char rawStatus[144];
    int zeroPos = 0;
    unsigned char flag = 0;
    int mode = 4;
    int ret = controllerGetStatus(g_ctrlInstance, channelId, rawStatus, &zeroPos, &flag,
                                  &mode, workBuf, outFlag, 0);
    if (ret == 0) {
        if (mode == 1) {
            *outMode = 1;
        } else if (mode == 2) {
            *outMode = 2;
        } else if (mode == 3) {
            *outMode = 3;
        } else {
            *outMode = 4;
            *outFlag = 0;
        }
    }
    return ret;
}

/* 0x811D321A */
int ctrlGetState(unsigned char channelId, void *outState)
{
    g_ctrlObjMutex.Lock();
    int ret = MUSICCORE_ERR_FAILED;
    if (g_ctrlInstance != NULL)
        ret = controllerGetState(g_ctrlInstance, channelId, outState);
    g_ctrlObjMutex.Unlock();
    return ret;
}

/* 0x811D325A */
int ctrlSuspendAudio(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x31180000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

/* 0x811D32CC */
int ctrlSeekAndPlay(uint32_t seqId, unsigned char channelId,
                    const ControlParams *ctrlParams, int fadeMs)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    SeekAndPlayCmdPayload payload;
    payload.control = *ctrlParams;
    payload.fadeMs = fadeMs;

    uint64_t hdr = makeCmdHeader(seqId, 0x31170000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, NULL);
}

/* 0x811D336C */
int ctrlStop(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x31190000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

/* 0x811D33DE */
int ctrlClearAudio(uint32_t seqId, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char emptyPayload = 0;
    uint64_t hdr = makeCmdHeader(seqId, 0x311B0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &emptyPayload, sizeof(emptyPayload),
                                 NULL, NULL);
}

} /* namespace MusicCore */
} /* namespace sce */
