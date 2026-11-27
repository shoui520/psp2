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

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
size_t sce_paf_strlen(const char *str);
int sceKernelGetSystemTime(SceKernelSysClock *clock);
}

static inline void *musiccoreAllocFromPafHeap(size_t size, void *heap)
{
	return operator new(size, static_cast<paf::memory::HeapAllocator *>(heap));
}

static const int MUSICCORE_ERR_FAILED = -1879113726; /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724; /* 0x8FFF0004 */
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

struct OpenParams {
    int sampleRate;
    int channels;
    int grainSize;
    int audioMode;
    int priorityHint;
};

struct OpenTrackPayload {
    void *vtable;
    char slot;
    int trackParam;
    int sampleRate;
    int channels;
    int grainSize;
    int reserved;
    int audioMode;
    int priorityHint;
};

struct CmdResult {
    int errCode;
    unsigned char outByte;
};

struct ContentInfoBuffers {
    char *titleBuf;
    size_t titleLen;
    char *artistBuf;
    size_t artistLen;
    char *albumBuf;
    size_t albumLen;
};

struct PlaybackStatusOut {
    unsigned char bytes[3];
    unsigned char padHeader[13];
    int16_t flags;
    int statusWordLow;
    int statusWordHigh;
    int playState;
    int contentType;
    int padAlign;
    int64_t trackId;
    int64_t trackItemId;
    int64_t playlistId;
    int words[19];
};

static ObjFactory *g_ctrlFactory = NULL;
static void *g_ctrlInstance = NULL;
static paf::thread::Mutex g_ctrlObjMutex("SceShellMusicCoreCtrlObj", 0);

static uint64_t makeCmdHeader(uint32_t param, uint32_t cmdAndTarget)
{
    return ((uint64_t)cmdAndTarget << 32) | (uint64_t)param;
}

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

int ctrlOpenSimple(uint32_t param, unsigned char channelId, int trackParam, unsigned char *outSlot)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    CmdResult res;
    res.errCode = 0;
    res.outByte = 0;
    uint64_t hdr = makeCmdHeader(param, 0x810B0000u | channelId);
    int ret = controllerSendCommand(g_ctrlInstance, hdr, &trackParam, 36, NULL, &res);
    if (ret == 0)
        *outSlot = res.outByte;
    return ret;
}

int ctrlOpenWithParams(uint32_t param, unsigned char channelId, int trackParam,
                       unsigned char *outSlot, const OpenParams *openParams)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    OpenTrackPayload payload;
    payload.vtable = NULL;
    payload.slot = -1;
    payload.trackParam = trackParam;
    payload.sampleRate = openParams->sampleRate;
    payload.channels = openParams->channels;
    payload.grainSize = openParams->grainSize;
    payload.reserved = 0;
    payload.audioMode = openParams->audioMode;
    payload.priorityHint = openParams->priorityHint;

    CmdResult res;
    res.errCode = 0;
    res.outByte = 0;
    uint64_t hdr = makeCmdHeader(param, 0x810B0000u | channelId);
    int ret = controllerSendCommand(g_ctrlInstance, hdr, &payload, sizeof(payload), NULL, &res);
    if (ret == 0)
        *outSlot = res.outByte;
    return ret;
}

int ctrlClose(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x810C0000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlReset(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x810A0000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlSetUriData(uint32_t param, unsigned char channelId, const void *uriData)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    if (uriData == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    uint64_t hdr = makeCmdHeader(param, 0x31110000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, uriData, 0, NULL, NULL);
}

int ctrlSetUriString(uint32_t param, unsigned char channelId, const char *uri)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    if (uri == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    size_t len = sce_paf_strlen(uri);
    uint64_t hdr = makeCmdHeader(param, 0x31120000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, uri, (unsigned int)(len + 1), NULL, NULL);
}

int ctrlCancel(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x311A0000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlPlay(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x310A0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlPause(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x310B0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlSeek(uint32_t param, unsigned char channelId, const int *seekParams)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    int payload[5];
    payload[0] = -1;
    payload[1] = (int)0x80000000u;
    payload[2] = -2;
    payload[3] = -2;
    payload[4] = -2;
    sce_paf_memset(payload, 0, sizeof(payload));
    payload[0] = seekParams[0];
    payload[1] = seekParams[1];
    payload[2] = seekParams[2];
    payload[3] = seekParams[3];
    payload[4] = seekParams[4];

    uint64_t hdr = makeCmdHeader(param, 0x310D0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, payload, sizeof(payload), NULL, NULL);
}

int ctrlStop(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x310E0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlFlush(uint32_t param, unsigned char channelId, int stampTime)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    SceKernelSysClock clockVal;
    clockVal.quad = 0;
    if (stampTime != 0)
        sceKernelGetSystemTime(&clockVal);

    uint64_t hdr = makeCmdHeader(param, 0x310F0000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, &clockVal, sizeof(clockVal), NULL, NULL);
}

int ctrlSkipNext(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x31140000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlSetEqualizer(uint32_t param, unsigned char channelId,
                     int eqType, int bandLow, int bandMid, int bandHigh)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    int payload[4];
    sce_paf_memset(payload, 0, sizeof(payload));
    payload[0] = eqType;
    payload[1] = bandLow;
    payload[2] = bandMid;
    payload[3] = bandHigh;

    uint64_t hdr = makeCmdHeader(param, 0x31150000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, payload, 20, NULL, NULL);
}

int ctrlSetAudioNormalizer(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x31160000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlSetIdleCallback(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810D000Fu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlNotifySystemState(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x8101000Fu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlSetStatusCallback(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810300FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlSetCapCallback(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810400FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlSetExtraCallback(uint32_t param)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x810500FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

int ctrlSetMetadataCallback(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810600FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlDbOpen(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810700FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlDbSuspend(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810700FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlDbClose(uint32_t param, int unusedFirst, int unusedSecond, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedFirst;
    (void)unusedSecond;
    uint32_t hi = (flags & 0x7F00u) | 0x810800FFu;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlDbQuery(uint32_t param)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x810900FFu);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 0, NULL, NULL);
}

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

int ctrlGetPlaybackMode(unsigned char channelId, int unusedArg, int *outMode,
                        void *workBuf, unsigned char *outFlag)
{
    (void)unusedArg;
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;

    unsigned char rawStatus[144];
    int zeroArg = 0;
    unsigned char flag = 0;
    int mode = 4;
    int ret = controllerGetStatus(g_ctrlInstance, channelId, rawStatus, &zeroArg, &flag,
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

int ctrlGetState(unsigned char channelId, void *outState)
{
    g_ctrlObjMutex.Lock();
    int ret = MUSICCORE_ERR_FAILED;
    if (g_ctrlInstance != NULL)
        ret = controllerGetState(g_ctrlInstance, channelId, outState);
    g_ctrlObjMutex.Unlock();
    return ret;
}

int ctrlNotifyStatus(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x31180000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlSeekAndPlay(uint32_t param, unsigned char channelId)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    uint64_t hdr = makeCmdHeader(param, 0x31170000u | channelId);
    return controllerSendCommand(g_ctrlInstance, hdr, NULL, 28, NULL, NULL);
}

int ctrlResetSpeed(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x31190000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

int ctrlRefreshDbId(uint32_t param, unsigned char channelId, int unusedArg, uint16_t flags)
{
    if (g_ctrlInstance == NULL)
        return MUSICCORE_ERR_FAILED;
    (void)unusedArg;
    uint32_t hi = (flags & 0x7F00u) | 0x311B0000u | channelId;
    return controllerSendCommand(g_ctrlInstance, makeCmdHeader(param, hi), NULL, 0, NULL, NULL);
}

} /* namespace MusicCore */
} /* namespace sce */

int musicCoreCtrlInit(void) { return sce::MusicCore::ctrlInit(); }
int musicCoreCtrlFini(void) { return sce::MusicCore::ctrlTerm(); }

int musicCoreCtrlOpenChannel(int seqId, SceUInt8 chType, int priority, SceUInt8 *outSlot)
{
    return sce::MusicCore::ctrlOpenSimple(static_cast<uint32_t>(seqId), chType, priority, outSlot);
}

int musicCoreCtrlOpenExChannel(int seqId, SceUInt8 chType, int priority, SceUInt8 *outSlot, void *param)
{
    return sce::MusicCore::ctrlOpenWithParams(
        static_cast<uint32_t>(seqId), chType, priority, outSlot,
        static_cast<const sce::MusicCore::OpenParams *>(param));
}

int musicCoreCtrlCloseChannel(int seqId)
{
    return sce::MusicCore::ctrlClose(static_cast<uint32_t>(seqId), 0, 0, 0);
}

int musicCoreCtrlSetActiveSlot(int seqId, SceUInt8 slot)
{
    return sce::MusicCore::ctrlReset(static_cast<uint32_t>(seqId), slot, 0, 0);
}

int musicCoreCtrlSetContent(int seqId)
{
    return sce::MusicCore::ctrlSetUriData(static_cast<uint32_t>(seqId), 0, NULL);
}

int musicCoreCtrlOpenUri(int seqId)
{
    return sce::MusicCore::ctrlSetUriString(static_cast<uint32_t>(seqId), 0, NULL);
}

int musicCoreCtrlResumeAudio(int flags, int mode)
{
    return sce::MusicCore::ctrlCancel(static_cast<uint32_t>(flags), static_cast<unsigned char>(mode), 0, 0);
}

int musicCoreCtrlPlay(int seqId, SceUInt8 slot, int param)
{
    (void)param;
    return sce::MusicCore::ctrlPlay(static_cast<uint32_t>(seqId), slot);
}

int musicCoreCtrlPause(int seqId, SceUInt8 slot, int mode, int param)
{
    (void)mode;
    (void)param;
    return sce::MusicCore::ctrlPause(static_cast<uint32_t>(seqId), slot);
}

int musicCoreCtrlControl(int seqId)
{
    return sce::MusicCore::ctrlSeek(static_cast<uint32_t>(seqId), 0, NULL);
}

int musicCoreCtrlSetRepeat(int seqId)
{
    return sce::MusicCore::ctrlStop(static_cast<uint32_t>(seqId), 0);
}

int musicCoreCtrlSetShuffle(int seqId)
{
    return sce::MusicCore::ctrlFlush(static_cast<uint32_t>(seqId), 0, 0);
}

int musicCoreCtrlSetVolume(int seqId)
{
    return sce::MusicCore::ctrlSkipNext(static_cast<uint32_t>(seqId), 0);
}

int musicCoreCtrlSetEq(int seqId)
{
    return sce::MusicCore::ctrlSetEqualizer(static_cast<uint32_t>(seqId), 0, 0, 0, 0, 0);
}

int musicCoreCtrlSetAvls(int seqId)
{
    return sce::MusicCore::ctrlSetAudioNormalizer(static_cast<uint32_t>(seqId), 0);
}

int musicCoreCtrlSetStatusCb(int mode, void *cb)
{
    (void)cb;
    return sce::MusicCore::ctrlSetIdleCallback(static_cast<uint32_t>(mode), 0, 0, 0);
}

int musicCoreCtrlSetCapCb(int mode, void *cb)
{
    (void)cb;
    return sce::MusicCore::ctrlNotifySystemState(static_cast<uint32_t>(mode), 0, 0, 0);
}

int musicCoreCtrlSetMediaCb(int mode, void *cb)
{
    (void)cb;
    return sce::MusicCore::ctrlSetStatusCallback(static_cast<uint32_t>(mode), 0, 0, 0);
}

int musicCoreCtrlSetNotifyCb(int mode, void *cb)
{
    (void)cb;
    return sce::MusicCore::ctrlSetCapCallback(static_cast<uint32_t>(mode), 0, 0, 0);
}

int musicCoreCtrlSetTimeCb(int mode, void *cb, int interval)
{
    (void)cb;
    (void)interval;
    return sce::MusicCore::ctrlSetExtraCallback(static_cast<uint32_t>(mode));
}

int musicCoreCtrlSetExtraCb(int mode, void *cb)
{
    (void)cb;
    return sce::MusicCore::ctrlSetMetadataCallback(static_cast<uint32_t>(mode), 0, 0, 0);
}

int musicCoreCtrlDbOpen(int seqId)
{
    return sce::MusicCore::ctrlDbOpen(static_cast<uint32_t>(seqId), 0, 0, 0);
}

int musicCoreCtrlDbSuspend(int seqId, int flag)
{
    return sce::MusicCore::ctrlDbSuspend(static_cast<uint32_t>(seqId), flag, 0, 0);
}

int musicCoreCtrlDbClose(int seqId)
{
    return sce::MusicCore::ctrlDbClose(static_cast<uint32_t>(seqId), 0, 0, 0);
}

int musicCoreCtrlDbQuery(int seqId, const void *buf, int mode, int opt)
{
    (void)buf;
    (void)mode;
    (void)opt;
    return sce::MusicCore::ctrlDbQuery(static_cast<uint32_t>(seqId));
}

int musicCoreCtrlGetMeta(SceUInt8 ch, void *metaDesc)
{
    return sce::MusicCore::ctrlGetContentInfo(ch, static_cast<sce::MusicCore::ContentInfoBuffers *>(metaDesc));
}

int musicCoreCtrlGetSlotStatus(SceUInt8 slot, void *outRaw, int *outTime, SceUInt8 *outValid)
{
    return sce::MusicCore::ctrlGetPlaybackStatus(
        slot, static_cast<sce::MusicCore::PlaybackStatusOut *>(outRaw), outTime, outValid);
}

int musicCoreCtrlGetSlotUri(SceUInt8 slot, int mode, int *outType, void *outUri, SceUInt8 *outValid)
{
    return sce::MusicCore::ctrlGetPlaybackMode(slot, mode, outType, outUri, outValid);
}

int musicCoreCtrlSetAvrcp(SceUInt8 enable, SceUInt8 *outPrev)
{
    return sce::MusicCore::ctrlGetState(enable, outPrev);
}

int musicCoreCtrlSuspendAudio(int flags, int mode)
{
    return sce::MusicCore::ctrlNotifyStatus(static_cast<uint32_t>(flags), static_cast<unsigned char>(mode), 0, 0);
}

int musicCoreCtrlSeek(void)
{
    return sce::MusicCore::ctrlSeekAndPlay(0, 0);
}

int musicCoreCtrlStop(void)
{
    return sce::MusicCore::ctrlResetSpeed(0, 0, 0, 0);
}

int musicCoreCtrlClearAudio(int flags, int mode)
{
    return sce::MusicCore::ctrlRefreshDbId(static_cast<uint32_t>(flags), static_cast<unsigned char>(mode), 0, 0);
}
