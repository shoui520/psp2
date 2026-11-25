/*
 * SceShell musiccore_plugin - DecoderManager, WorkerThreadPool, and CodecAdapter
 * (AT9/MP3/MP4/WAV codec adapters and media format probing).
 * Original address range (3.65-CEX): 0x811E91C4 - 0x811EA894
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <thread/thread.h>

extern "C" {
size_t sce_paf_strlen(const char *str);
int sce_paf_strncmp(const char *lhs, const char *rhs, size_t len);
char *sce_paf_strrchr(const char *str, int ch);
int sce_paf_strcasecmp(const char *lhs, const char *rhs);
}

namespace sce {
namespace MusicCore {

extern void *allocFromHeap(size_t size);
extern bool checkLocalMediaPath(const char *path);

static const int MUSICCORE_ERR_FAILED = -1879113726;       /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;    /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723;  /* 0x8FFF0005 */
static const int MUSICCORE_ERR_BAD_STATE = -1879113721;    /* 0x8FFF0007 */
static const int MUSICCORE_ERR_NOT_FOUND = -1879113720;    /* 0x8FFF0008 */
static const int MUSICCORE_ERR_UNSUPPORTED = -1879113719;  /* 0x8FFF0009 */
static const int MUSICCORE_ERR_BAD_PARAM = -1879113718;    /* 0x8FFF000A */
static const int MUSICCORE_ERR_FORMAT = -1879113717;       /* 0x8FFF000B */

enum CodecFormatId {
    CODEC_FORMAT_WAV = 0,
    CODEC_FORMAT_MP3 = 1,
    CODEC_FORMAT_MP4 = 4,
    CODEC_FORMAT_AT9 = 6,
    CODEC_FORMAT_UNKNOWN = 7
};

enum MediaSourceKind {
    MEDIA_SOURCE_HTTP = 2,
    MEDIA_SOURCE_LOCAL = 3,
    MEDIA_SOURCE_INVALID = 4
};

static const uint32_t FOURCC_MP3 = 0x2033504Du; /* "MP3 " (little-endian "3PM ") */
static const uint32_t FOURCC_MP4 = 0x2034504Du; /* "MP4 " (little-endian "4PM ") */
static const uint32_t FOURCC_WAV = 0x20574156u; /* "WAV " (little-endian "VAW ") */
static const uint32_t FOURCC_AT9 = 0x20395441u; /* "AT9 " (little-endian "9TA ") */

struct SyncMutex {
    SceUID mutexId;
    int reserved;
};

struct SyncCond {
    SceUID condId;
    int reserved;
};

extern void syncMutexInit(SyncMutex *mtx, const char *name);
extern void syncMutexDestroy(SyncMutex *mtx);
extern int syncMutexLock(SyncMutex *mtx);
extern int syncMutexUnlock(SyncMutex *mtx);

extern void syncCondInit(SyncCond *cond, const char *name);
extern void syncCondDestroy(SyncCond *cond);
extern int syncCondWait(SyncCond *cond, SyncMutex *mtx, unsigned int mask, unsigned int timeout);
extern int syncCondSignal(SyncCond *cond, unsigned int mask);

extern int workerThreadCreate(const char *name, SceUID *outThid, SceKernelThreadEntry entry,
                              int priority, SceSize stackSize);
extern int workerThreadStart(SceUID thid, SceSize argSize, void *argData);
extern int workerThreadWaitEnd(SceUID thid);
extern int workerThreadDelete(SceUID thid);

struct MediaInfoBase {
    virtual ~MediaInfoBase() {}
    virtual int parseInfo(int parseMode, int parseFlags) = 0;

    uint32_t formatFourCC;
    uint8_t isRetained;
};

struct DecoderInstance;
struct CodecFactory;

extern CodecFactory *getMp3Factory();
extern CodecFactory *getMp4Factory();
extern CodecFactory *getAt9Factory();
extern CodecFactory gWavFactory;

extern int getMp3StackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
extern int getMp4StackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
extern int getAt9StackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
extern int getWavStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);

extern DecoderInstance *createMp3Decoder(CodecFactory *factory, int mode, void *outCtx,
                                         void *startFn, void *enqueueFn, void *stopFn, void *flushFn);
extern DecoderInstance *createMp4Decoder(CodecFactory *factory, int mode, void *outCtx,
                                         void *startFn, void *enqueueFn, void *stopFn, void *flushFn);
extern DecoderInstance *createAt9Decoder(CodecFactory *factory, int mode, void *outCtx,
                                         void *startFn, void *enqueueFn, void *stopFn, void *flushFn, int extraParam);
extern DecoderInstance *createWavDecoder(CodecFactory *factory, void *outCtx,
                                         void *startFn, void *enqueueFn, void *stopFn, void *flushFn);

extern int destroyCodecDecoder(CodecFactory *factory, DecoderInstance *decoder);
extern int destroyWavDecoder(CodecFactory *factory, DecoderInstance *decoder);

extern MediaInfoBase *allocMp3MediaInfo(CodecFactory *factory, const char *path);
extern MediaInfoBase *allocMp4MediaInfo(CodecFactory *factory, const char *path);
extern MediaInfoBase *allocAt9MediaInfo(CodecFactory *factory, const char *path);
extern MediaInfoBase *allocWavMediaInfo(CodecFactory *factory, const char *path);
extern void releaseFactoryMediaInfo(CodecFactory *factory, MediaInfoBase **mediaInfo);

extern void decoderSetListener(DecoderInstance *decoder, void *listener);
extern unsigned int decoderRequestStart(DecoderInstance *decoder, MediaInfoBase *mediaInfo,
                                        int startMs, int flagPrimary, int flagSecondary);
extern unsigned int decoderRequestStop(DecoderInstance *decoder);
extern unsigned int decoderRequestPause(DecoderInstance *decoder);
extern unsigned int decoderRequestSeek(DecoderInstance *decoder, int posMs);
extern unsigned int decoderRequestPosition(DecoderInstance *decoder);
extern void *decoderGetParserAdapter(DecoderInstance *decoder);

struct ParserAdapterBase {
    virtual ~ParserAdapterBase() {}
    virtual int reservedA() { return 0; }
    virtual int reservedB() { return 0; }
    virtual int reservedC() { return 0; }
    virtual int reservedD() { return 0; }
    virtual int reservedE() { return 0; }
    virtual int reservedF() { return 0; }
    virtual int reservedG() { return 0; }
    virtual int reservedH() { return 0; }
    virtual int findCachedMediaInfo(const char *path, MediaInfoBase **outInfo) = 0;
};

struct ThreadSpec {
    const char *name;
    int priority;
    SceSize stackSize;
};

struct CodecProbeEntry {
    uint8_t tried;
    uint8_t pad[3];
    int order;
    int codecType;
};

static const CodecProbeEntry kDefaultCodecProbeTable[4] = {
    { 0, { 0, 0, 0 }, -1, CODEC_FORMAT_WAV },
    { 0, { 0, 0, 0 }, -1, CODEC_FORMAT_MP3 },
    { 0, { 0, 0, 0 }, -1, CODEC_FORMAT_MP4 },
    { 0, { 0, 0, 0 }, -1, CODEC_FORMAT_AT9 }
};

/* 0x811E9FAA */
static int mapDecoderError(unsigned int decErr)
{
    switch (decErr) {
    case 0x00u:
        return 0;
    case 0x04u:
        return MUSICCORE_ERR_NO_MEMORY;
    case 0x11u:
        return MUSICCORE_ERR_NOT_FOUND;
    case 0x15u:
        return MUSICCORE_ERR_BAD_PARAM;
    case 0x21u:
        return MUSICCORE_ERR_BAD_STATE;
    case 0x51u:
        return MUSICCORE_ERR_UNSUPPORTED;
    case 0x5Fu:
    case 0x61u:
        return MUSICCORE_ERR_FORMAT;
    default:
        return MUSICCORE_ERR_FAILED;
    }
}

class ActiveFilterListener {
public:
    virtual ~ActiveFilterListener() {}
    virtual void onEndOfStream() = 0;
    virtual void onDecodeError(int stage, unsigned int decErr) = 0;
    virtual void onReservedEvent() {}
    virtual void onOpenComplete(int status, int param) = 0;
    virtual void onStopComplete(int status) = 0;
    virtual void onPauseComplete(int status) = 0;
    virtual void onSeekComplete(int status) = 0;
    virtual void onPositionReady(int posMs, int status) = 0;
};

class CodecAdapter : public ActiveFilterListener {
public:
    typedef int (*MediaReadyCallback)(void *ctx, void *userCtx);
    typedef int (*ErrorCallback)(void *ctx, int err);
    typedef int (*EndOfStreamCallback)(void *ctx);

    void *outputCtx;
    void *outputStartFn;
    void *outputStopFn;
    void *outputEnqueueFn;
    void *outputFlushFn;
    MediaReadyCallback mediaReadyCb;
    void *pendingUserCtx;
    ErrorCallback errorCb;
    EndOfStreamCallback eosCb;
    void *eventCbCtx;
    DecoderInstance *decoder;
    SyncMutex waitMutex;
    SyncCond waitCond;
    int lastPositionMs;
    int lastAsyncStatus;
    int lastAsyncParam;
    uint8_t lastEventType;
    uint8_t pad77[3];

    /* 0x811E9ED0 */
    CodecAdapter(const char *mutexName, const char *condName);

    /* 0x811E9F2C / 0x811E9F58 */
    virtual ~CodecAdapter();

    /* 0x811EA23E */
    virtual void onEndOfStream();

    /* 0x811EA24A */
    virtual void onDecodeError(int stage, unsigned int decErr);

    /* 0x811EA266 */
    virtual void onOpenComplete(int status, int param);

    /* 0x811EA298 */
    virtual void onStopComplete(int status);

    /* 0x811EA2C2 */
    virtual void onPauseComplete(int status);

    /* 0x811EA2EC */
    virtual void onSeekComplete(int status);

    /* 0x811EA316 */
    virtual void onPositionReady(int posMs, int status);

    /* Pure virtual codec factory hooks */
    virtual int openDecoder(int mode, int extraParam) = 0;
    virtual int closeDecoder() = 0;

    /* 0x811EA016 */
    virtual int startPlay(MediaInfoBase *mediaInfo, int startMs, int flagPrimary, int flagSecondary, void *userCtx);

    /* 0x811EA0A8 */
    virtual int pausePlay();

    /* 0x811EA0CE */
    virtual int stopPlay();

    /* 0x811EA120 */
    virtual int seekPlay(int mode, int posMs);

    /* 0x811EA176 */
    virtual int getPosition(int *outPosMs);

    /* 0x811EA1D8 */
    virtual int isOpened();

    /* 0x811EA1E8 */
    virtual int getMetadata(MediaInfoBase *mediaInfo, int *outVal);

    /* 0x811EA206 */
    virtual int queryCachedInfo(const char *path, MediaInfoBase **outInfo);

    /* Pure virtual format fourCC matcher */
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo) = 0;

    /* 0x811E9F88 */
    int setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn, void *stopFn, void *flushFn);

    /* 0x811E9F9C */
    int setEventCallbacks(MediaReadyCallback readyCb, ErrorCallback errCb,
                          EndOfStreamCallback endCb, void *cbCtx);
};

/* 0x811E9ED0 */
CodecAdapter::CodecAdapter(const char *mutexName, const char *condName)
    : outputCtx(NULL),
      outputStartFn(NULL),
      outputStopFn(NULL),
      outputEnqueueFn(NULL),
      outputFlushFn(NULL),
      mediaReadyCb(NULL),
      pendingUserCtx(NULL),
      errorCb(NULL),
      eosCb(NULL),
      eventCbCtx(NULL),
      decoder(NULL),
      lastPositionMs(-1),
      lastAsyncStatus(-1),
      lastAsyncParam(-1),
      lastEventType(0)
{
    syncMutexInit(&waitMutex, mutexName);
    syncCondInit(&waitCond, condName);
}

/* 0x811E9F2C */
CodecAdapter::~CodecAdapter()
{
    syncCondDestroy(&waitCond);
    syncMutexDestroy(&waitMutex);
}

/* 0x811E9F88 */
int CodecAdapter::setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn,
                                     void *stopFn, void *flushFn)
{
    outputCtx = outCtx;
    outputStartFn = startFn;
    outputStopFn = stopFn;
    outputEnqueueFn = enqueueFn;
    outputFlushFn = flushFn;
    return 0;
}

/* 0x811E9F9C */
int CodecAdapter::setEventCallbacks(MediaReadyCallback readyCb, ErrorCallback errCb,
                                    EndOfStreamCallback endCb, void *cbCtx)
{
    mediaReadyCb = readyCb;
    errorCb = errCb;
    eosCb = endCb;
    eventCbCtx = cbCtx;
    return 0;
}

/* 0x811EA016 */
int CodecAdapter::startPlay(MediaInfoBase *mediaInfo, int startMs, int flagPrimary,
                            int flagSecondary, void *userCtx)
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (mediaInfo == NULL || userCtx == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    syncMutexLock(&waitMutex);
    pendingUserCtx = userCtx;
    unsigned int decErr = decoderRequestStart(decoder, mediaInfo, startMs, flagPrimary, flagSecondary);
    if (decErr != 0) {
        syncMutexUnlock(&waitMutex);
        return mapDecoderError(decErr);
    }

    syncCondWait(&waitCond, &waitMutex, 1u, 0);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    syncMutexUnlock(&waitMutex);

    if (status == 0 && mediaReadyCb != NULL) {
        return mediaReadyCb(eventCbCtx, pendingUserCtx);
    }
    return status;
}

/* 0x811EA0A8 */
int CodecAdapter::pausePlay()
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (decoderRequestPause(decoder) != 0) {
        return mapDecoderError((unsigned int)lastAsyncStatus);
    }
    return 0;
}

/* 0x811EA0CE */
int CodecAdapter::stopPlay()
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }

    syncMutexLock(&waitMutex);
    unsigned int decErr = decoderRequestStop(decoder);
    if (decErr != 0) {
        syncMutexUnlock(&waitMutex);
        return mapDecoderError(decErr);
    }

    syncCondWait(&waitCond, &waitMutex, 2u, 0);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    syncMutexUnlock(&waitMutex);
    return status;
}

/* 0x811EA120 */
int CodecAdapter::seekPlay(int mode, int posMs)
{
    (void)mode;
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }

    syncMutexLock(&waitMutex);
    unsigned int decErr = decoderRequestSeek(decoder, posMs);
    if (decErr != 0) {
        syncMutexUnlock(&waitMutex);
        return mapDecoderError(decErr);
    }

    syncCondWait(&waitCond, &waitMutex, 16u, 0);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    syncMutexUnlock(&waitMutex);
    return status;
}

/* 0x811EA176 */
int CodecAdapter::getPosition(int *outPosMs)
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (outPosMs == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    syncMutexLock(&waitMutex);
    if (decoderRequestPosition(decoder) != 0) {
        syncMutexUnlock(&waitMutex);
        return MUSICCORE_ERR_FAILED;
    }

    syncCondWait(&waitCond, &waitMutex, 32u, 0);
    *outPosMs = lastPositionMs;
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    syncMutexUnlock(&waitMutex);
    return status;
}

/* 0x811EA1D8 */
int CodecAdapter::isOpened()
{
    return (decoder != NULL) ? 0 : MUSICCORE_ERR_FAILED;
}

/* 0x811EA1E8 */
int CodecAdapter::getMetadata(MediaInfoBase *mediaInfo, int *outVal)
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (mediaInfo == NULL || outVal == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }
    *outVal = 0;
    return 0;
}

/* 0x811EA206 */
int CodecAdapter::queryCachedInfo(const char *path, MediaInfoBase **outInfo)
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    ParserAdapterBase *parser = static_cast<ParserAdapterBase *>(decoderGetParserAdapter(decoder));
    if (parser == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (parser->findCachedMediaInfo(path, outInfo) != 0) {
        *outInfo = NULL;
    }
    return 0;
}

/* 0x811EA23E */
void CodecAdapter::onEndOfStream()
{
    if (eosCb != NULL) {
        eosCb(eventCbCtx);
    }
}

/* 0x811EA24A */
void CodecAdapter::onDecodeError(int stage, unsigned int decErr)
{
    (void)stage;
    if (errorCb != NULL) {
        errorCb(eventCbCtx, mapDecoderError(decErr));
    }
}

/* 0x811EA266 */
void CodecAdapter::onOpenComplete(int status, int param)
{
    syncMutexLock(&waitMutex);
    lastAsyncStatus = status;
    lastAsyncParam = param;
    lastEventType = 1;
    syncCondSignal(&waitCond, 1u);
    syncMutexUnlock(&waitMutex);
}

/* 0x811EA298 */
void CodecAdapter::onStopComplete(int status)
{
    syncMutexLock(&waitMutex);
    lastAsyncStatus = status;
    lastEventType = 2;
    syncCondSignal(&waitCond, 2u);
    syncMutexUnlock(&waitMutex);
}

/* 0x811EA2C2 */
void CodecAdapter::onPauseComplete(int status)
{
    syncMutexLock(&waitMutex);
    lastAsyncStatus = status;
    lastEventType = 3;
    syncCondSignal(&waitCond, 4u);
    syncMutexUnlock(&waitMutex);
}

/* 0x811EA2EC */
void CodecAdapter::onSeekComplete(int status)
{
    syncMutexLock(&waitMutex);
    lastAsyncStatus = status;
    lastEventType = 4;
    syncCondSignal(&waitCond, 16u);
    syncMutexUnlock(&waitMutex);
}

/* 0x811EA316 */
void CodecAdapter::onPositionReady(int posMs, int status)
{
    syncMutexLock(&waitMutex);
    lastPositionMs = posMs;
    lastAsyncStatus = status;
    lastEventType = 5;
    syncCondSignal(&waitCond, 32u);
    syncMutexUnlock(&waitMutex);
}

/* ===== CodecAdapterAt9 (0x811EA348 - 0x811EA4A7) ===== */

class CodecAdapterAt9 : public CodecAdapter {
public:
    /* 0x811EA348 */
    CodecAdapterAt9(const char *mutexName, const char *condName)
        : CodecAdapter(mutexName, condName)
    {}

    /* 0x811EA360 / 0x811EA37C */
    virtual ~CodecAdapterAt9() {}

    /* 0x811EA39C */
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
    {
        if (getAt9Factory() == NULL) {
            return 0;
        }
        return getAt9StackSizes(parserStack, decoderStack, mgrStack);
    }

    /* 0x811EA3B8 */
    virtual int openDecoder(int sourceMode, int extraParam)
    {
        if (decoder != NULL) {
            return 0;
        }
        int ioMode;
        if (sourceMode == 2) {
            ioMode = 5;
        } else if (sourceMode == 3) {
            ioMode = 16;
        } else {
            return MUSICCORE_ERR_BAD_PARAM;
        }

        CodecFactory *factory = getAt9Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoder = createAt9Decoder(factory, ioMode, outputCtx, outputStartFn,
                                   outputEnqueueFn, outputStopFn, outputFlushFn, extraParam);
        if (decoder == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoderSetListener(decoder, this);
        return 0;
    }

    /* 0x811EA42A */
    virtual int closeDecoder()
    {
        if (decoder == NULL) {
            return 0;
        }
        CodecFactory *factory = getAt9Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        syncMutexLock(&waitMutex);
        decoderRequestStop(decoder);
        syncCondWait(&waitCond, &waitMutex, 2u, 0);
        int status = mapDecoderError((unsigned int)lastAsyncStatus);
        syncMutexUnlock(&waitMutex);
        if (status != 0) {
            return status;
        }
        int ret = destroyCodecDecoder(factory, decoder);
        decoder = NULL;
        return ret;
    }

    /* 0x811EA48C */
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo)
    {
        return (mediaInfo != NULL && mediaInfo->formatFourCC == FOURCC_AT9) ? 1 : 0;
    }
};

/* ===== CodecAdapterMp3 (0x811EA4A8 - 0x811EA603) ===== */

class CodecAdapterMp3 : public CodecAdapter {
public:
    /* 0x811EA4A8 */
    CodecAdapterMp3(const char *mutexName, const char *condName)
        : CodecAdapter(mutexName, condName)
    {}

    /* 0x811EA4C0 / 0x811EA4DC */
    virtual ~CodecAdapterMp3() {}

    /* 0x811EA4FC */
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
    {
        if (getMp3Factory() == NULL) {
            return 0;
        }
        return getMp3StackSizes(parserStack, decoderStack, mgrStack);
    }

    /* 0x811EA518 */
    virtual int openDecoder(int sourceMode, int extraParam)
    {
        (void)extraParam;
        if (decoder != NULL) {
            return 0;
        }
        int ioMode;
        if (sourceMode == 2) {
            ioMode = 5;
        } else if (sourceMode == 3) {
            ioMode = 16;
        } else {
            return MUSICCORE_ERR_BAD_PARAM;
        }

        CodecFactory *factory = getMp3Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoder = createMp3Decoder(factory, ioMode, outputCtx, outputStartFn,
                                   outputEnqueueFn, outputStopFn, outputFlushFn);
        if (decoder == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoderSetListener(decoder, this);
        return 0;
    }

    /* 0x811EA584 */
    virtual int closeDecoder()
    {
        if (decoder == NULL) {
            return 0;
        }
        CodecFactory *factory = getMp3Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        syncMutexLock(&waitMutex);
        decoderRequestStop(decoder);
        syncCondWait(&waitCond, &waitMutex, 2u, 0);
        int status = mapDecoderError((unsigned int)lastAsyncStatus);
        syncMutexUnlock(&waitMutex);
        if (status != 0) {
            return status;
        }
        int ret = destroyCodecDecoder(factory, decoder);
        decoder = NULL;
        return ret;
    }

    /* 0x811EA5E6 */
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo)
    {
        return (mediaInfo != NULL && mediaInfo->formatFourCC == FOURCC_MP3) ? 1 : 0;
    }
};

/* ===== CodecAdapterMp4 (0x811EA604 - 0x811EA75F) ===== */

class CodecAdapterMp4 : public CodecAdapter {
public:
    /* 0x811EA604 */
    CodecAdapterMp4(const char *mutexName, const char *condName)
        : CodecAdapter(mutexName, condName)
    {}

    /* 0x811EA61C / 0x811EA638 */
    virtual ~CodecAdapterMp4() {}

    /* 0x811EA658 */
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
    {
        if (getMp4Factory() == NULL) {
            return 0;
        }
        return getMp4StackSizes(parserStack, decoderStack, mgrStack);
    }

    /* 0x811EA674 */
    virtual int openDecoder(int sourceMode, int extraParam)
    {
        (void)extraParam;
        if (decoder != NULL) {
            return 0;
        }
        int ioMode;
        if (sourceMode == 2) {
            ioMode = 5;
        } else if (sourceMode == 3) {
            ioMode = 16;
        } else {
            return MUSICCORE_ERR_BAD_PARAM;
        }

        CodecFactory *factory = getMp4Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoder = createMp4Decoder(factory, ioMode, outputCtx, outputStartFn,
                                   outputEnqueueFn, outputStopFn, outputFlushFn);
        if (decoder == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoderSetListener(decoder, this);
        return 0;
    }

    /* 0x811EA6E0 */
    virtual int closeDecoder()
    {
        if (decoder == NULL) {
            return 0;
        }
        CodecFactory *factory = getMp4Factory();
        if (factory == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        syncMutexLock(&waitMutex);
        decoderRequestStop(decoder);
        syncCondWait(&waitCond, &waitMutex, 2u, 0);
        int status = mapDecoderError((unsigned int)lastAsyncStatus);
        syncMutexUnlock(&waitMutex);
        if (status != 0) {
            return status;
        }
        int ret = destroyCodecDecoder(factory, decoder);
        decoder = NULL;
        return ret;
    }

    /* 0x811EA742 */
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo)
    {
        return (mediaInfo != NULL && mediaInfo->formatFourCC == FOURCC_MP4) ? 1 : 0;
    }
};

/* ===== CodecAdapterWav (0x811EA760 - 0x811EA893) ===== */

class CodecAdapterWav : public CodecAdapter {
public:
    /* 0x811EA760 */
    CodecAdapterWav(const char *mutexName, const char *condName)
        : CodecAdapter(mutexName, condName)
    {}

    /* 0x811EA778 / 0x811EA794 */
    virtual ~CodecAdapterWav() {}

    /* 0x811EA7B4 */
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
    {
        return getWavStackSizes(parserStack, decoderStack, mgrStack);
    }

    /* 0x811EA7C6 */
    virtual int openDecoder(int sourceMode, int extraParam)
    {
        (void)sourceMode;
        (void)extraParam;
        if (decoder != NULL) {
            return 0;
        }
        decoder = createWavDecoder(&gWavFactory, outputCtx, outputStartFn,
                                   outputEnqueueFn, outputStopFn, outputFlushFn);
        if (decoder == NULL) {
            return MUSICCORE_ERR_NO_MEMORY;
        }
        decoderSetListener(decoder, this);
        return 0;
    }

    /* 0x811EA812 */
    virtual int closeDecoder()
    {
        if (decoder == NULL) {
            return 0;
        }
        syncMutexLock(&waitMutex);
        decoderRequestStop(decoder);
        syncCondWait(&waitCond, &waitMutex, 2u, 0);
        int status = mapDecoderError((unsigned int)lastAsyncStatus);
        syncMutexUnlock(&waitMutex);
        if (status != 0) {
            return status;
        }
        int ret = destroyWavDecoder(&gWavFactory, decoder);
        decoder = NULL;
        return ret;
    }

    /* 0x811EA876 */
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo)
    {
        return (mediaInfo != NULL && mediaInfo->formatFourCC == FOURCC_WAV) ? 1 : 0;
    }
};

/* ===== Parser Adapter Semaphore (0x811E9AB4) ===== */

static paf::thread::Semaphore gParserAdapterSemaphore("SceShellMusicCorePAdapterNum", 2, 2, 0);

/* ===== DecoderManager (0x811E91C4 - 0x811E9AB3) ===== */

class DecoderManager {
public:
    CodecAdapter *activeCodec;
    int activeCodecType;
    void *outputCtx;
    void *outputStartFn;
    void *outputStopFn;
    void *outputEnqueueFn;
    void *outputFlushFn;
    CodecAdapter::MediaReadyCallback mediaReadyCb;
    CodecAdapter::ErrorCallback errorCb;
    CodecAdapter::EndOfStreamCallback eosCb;
    void *eventCbCtx;
    int reserved;

    /* 0x811E91C4 */
    DecoderManager()
        : activeCodec(NULL),
          activeCodecType(CODEC_FORMAT_UNKNOWN),
          outputCtx(NULL),
          outputStartFn(NULL),
          outputStopFn(NULL),
          outputEnqueueFn(NULL),
          outputFlushFn(NULL),
          mediaReadyCb(NULL),
          errorCb(NULL),
          eosCb(NULL),
          eventCbCtx(NULL),
          reserved(0)
    {}

    /* 0x811E91F0 / 0x811E91FE */
    virtual ~DecoderManager() {}

    /* 0x811E9212 */
    static void getThreadSpecs(ThreadSpec *outSpecs, unsigned int *inOutCount)
    {
        if (outSpecs == NULL || *inOutCount < 4u) {
            *inOutCount = 0;
            return;
        }

        unsigned int maxParser = 0;
        unsigned int maxDecoder = 0;
        unsigned int maxMgr = 0;
        unsigned int parserStack = 0;
        unsigned int decoderStack = 0;
        unsigned int mgrStack = 0;

        CodecAdapterMp3::queryStackSizes(&parserStack, &decoderStack, &mgrStack);
        if (parserStack > maxParser) { maxParser = parserStack; }
        if (decoderStack > maxDecoder) { maxDecoder = decoderStack; }
        if (mgrStack > maxMgr) { maxMgr = mgrStack; }

        CodecAdapterMp4::queryStackSizes(&parserStack, &decoderStack, &mgrStack);
        if (parserStack > maxParser) { maxParser = parserStack; }
        if (decoderStack > maxDecoder) { maxDecoder = decoderStack; }
        if (mgrStack > maxMgr) { maxMgr = mgrStack; }

        CodecAdapterWav::queryStackSizes(&parserStack, &decoderStack, &mgrStack);
        if (parserStack > maxParser) { maxParser = parserStack; }
        if (decoderStack > maxDecoder) { maxDecoder = decoderStack; }
        if (mgrStack > maxMgr) { maxMgr = mgrStack; }

        CodecAdapterAt9::queryStackSizes(&parserStack, &decoderStack, &mgrStack);
        if (parserStack > maxParser) { maxParser = parserStack; }
        if (decoderStack > maxDecoder) { maxDecoder = decoderStack; }
        if (mgrStack > maxMgr) { maxMgr = mgrStack; }

        outSpecs[0].name = "SceShellMusicCoreCacheIo";
        outSpecs[0].priority = 0x100000F1;
        outSpecs[0].stackSize = 0x2000u;

        outSpecs[1].name = "SceShellMusicCoreParser";
        outSpecs[1].priority = 0x100000F0;
        outSpecs[1].stackSize = maxParser;

        outSpecs[2].name = "SceShellMusicCoreDecoder";
        outSpecs[2].priority = 0x100000EE;
        outSpecs[2].stackSize = maxDecoder;

        outSpecs[3].name = "SceShellMusicCoreDecoderManager";
        outSpecs[3].priority = 0x100000E9;
        outSpecs[3].stackSize = maxMgr;

        *inOutCount = 4u;
    }

    /* 0x811E934E */
    int setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn, void *stopFn, void *flushFn)
    {
        outputCtx = outCtx;
        outputStartFn = startFn;
        outputStopFn = stopFn;
        outputEnqueueFn = enqueueFn;
        outputFlushFn = flushFn;
        return 0;
    }

    /* 0x811E9362 */
    int setEventCallbacks(CodecAdapter::MediaReadyCallback readyCb,
                          CodecAdapter::ErrorCallback errCb,
                          CodecAdapter::EndOfStreamCallback endCb,
                          void *cbCtx)
    {
        mediaReadyCb = readyCb;
        errorCb = errCb;
        eosCb = endCb;
        eventCbCtx = cbCtx;
        return 0;
    }

    /* 0x811E9370 */
    int openCodec(int codecType, int sourceMode, int extraParam)
    {
        if (activeCodec != NULL) {
            return 0;
        }
        if (outputCtx == NULL || outputStartFn == NULL ||
            outputEnqueueFn == NULL || outputStopFn == NULL) {
            return MUSICCORE_ERR_FAILED;
        }
        if (mediaReadyCb == NULL || errorCb == NULL || eosCb == NULL) {
            return MUSICCORE_ERR_FAILED;
        }

        CodecAdapter *codec = NULL;
        switch (codecType) {
        case CODEC_FORMAT_WAV: {
            void *mem = allocFromHeap(sizeof(CodecAdapterWav));
            if (mem != NULL) {
                codec = new (mem) CodecAdapterWav("SceShellMusicCoreCodecWav", "SceShellMusicCoreCodecWav");
            }
            activeCodec = codec;
            activeCodecType = CODEC_FORMAT_WAV;
            break;
        }
        case CODEC_FORMAT_MP3: {
            void *mem = allocFromHeap(sizeof(CodecAdapterMp3));
            if (mem != NULL) {
                codec = new (mem) CodecAdapterMp3("SceShellMusicCoreCodecMp3", "SceShellMusicCoreCodecMp3");
            }
            activeCodec = codec;
            activeCodecType = CODEC_FORMAT_MP3;
            break;
        }
        case CODEC_FORMAT_MP4: {
            void *mem = allocFromHeap(sizeof(CodecAdapterMp4));
            if (mem != NULL) {
                codec = new (mem) CodecAdapterMp4("SceShellMusicCoreCodecMp4", "SceShellMusicCoreCodecMp4");
            }
            activeCodec = codec;
            activeCodecType = CODEC_FORMAT_MP4;
            break;
        }
        case CODEC_FORMAT_AT9: {
            void *mem = allocFromHeap(sizeof(CodecAdapterAt9));
            if (mem != NULL) {
                codec = new (mem) CodecAdapterAt9("SceShellMusicCoreCodecAt9", "SceShellMusicCoreCodecAt9");
            }
            activeCodec = codec;
            activeCodecType = CODEC_FORMAT_AT9;
            break;
        }
        default:
            return MUSICCORE_ERR_UNSUPPORTED;
        }

        if (activeCodec == NULL) {
            activeCodecType = CODEC_FORMAT_UNKNOWN;
            return MUSICCORE_ERR_NO_MEMORY;
        }

        int ret = activeCodec->setOutputCallbacks(outputCtx, outputStartFn, outputEnqueueFn,
                                                  outputStopFn, outputFlushFn);
        if (ret != 0) {
            delete activeCodec;
            activeCodec = NULL;
            activeCodecType = CODEC_FORMAT_UNKNOWN;
            return ret;
        }

        ret = activeCodec->setEventCallbacks(mediaReadyCb, errorCb, eosCb, eventCbCtx);
        if (ret != 0) {
            delete activeCodec;
            activeCodec = NULL;
            activeCodecType = CODEC_FORMAT_UNKNOWN;
            return ret;
        }

        ret = activeCodec->openDecoder(sourceMode, extraParam);
        if (ret != 0) {
            delete activeCodec;
            activeCodec = NULL;
            activeCodecType = CODEC_FORMAT_UNKNOWN;
        }
        return ret;
    }

    /* 0x811E9510 */
    int closeCodec()
    {
        if (activeCodec == NULL) {
            return 0;
        }
        int ret = activeCodec->closeDecoder();
        delete activeCodec;
        activeCodec = NULL;
        activeCodecType = CODEC_FORMAT_UNKNOWN;
        return ret;
    }

    /* 0x811E953A */
    static bool probeSourcePath(const char *path, int *outSourceKind)
    {
        if (path == NULL) {
            return false;
        }
        if (sce_paf_strncmp(path, "sd0:", sce_paf_strlen("sd0:")) == 0) {
            *outSourceKind = MEDIA_SOURCE_LOCAL;
            return checkLocalMediaPath(path);
        }
        if (sce_paf_strncmp(path, "http:", sce_paf_strlen("http:")) == 0) {
            *outSourceKind = MEDIA_SOURCE_HTTP;
            return false;
        }
        if (sce_paf_strncmp(path, "vs0:", sce_paf_strlen("vs0:")) == 0 ||
            sce_paf_strncmp(path, "ur0:", sce_paf_strlen("ur0:")) == 0 ||
            sce_paf_strncmp(path, "ux0:", sce_paf_strlen("ux0:")) == 0) {
            *outSourceKind = MEDIA_SOURCE_LOCAL;
        } else {
            *outSourceKind = MEDIA_SOURCE_LOCAL;
        }
        return checkLocalMediaPath(path);
    }

    /* 0x811E9606 */
    static int releaseMediaInfo(MediaInfoBase **mediaInfoPtr, uint8_t releaseSemaphore)
    {
        MediaInfoBase *media = *mediaInfoPtr;
        if (media == NULL) {
            return MUSICCORE_ERR_INVALID_ARG;
        }

        CodecFactory *factory = NULL;
        if (media->formatFourCC == FOURCC_MP3) {
            factory = getMp3Factory();
        } else if (media->formatFourCC == FOURCC_MP4) {
            factory = getMp4Factory();
        } else if (media->formatFourCC == FOURCC_WAV) {
            factory = &gWavFactory;
        } else if (media->formatFourCC == FOURCC_AT9) {
            factory = getAt9Factory();
        }

        if (factory != NULL) {
            releaseFactoryMediaInfo(factory, mediaInfoPtr);
        } else {
            delete media;
            *mediaInfoPtr = NULL;
        }

        if (releaseSemaphore == 1) {
            gParserAdapterSemaphore.Release();
        }
        return 0;
    }

    /* 0x811E96F0 */
    static int createMediaInfo(const char *path, int hintCodec, uint8_t doParse,
                               CodecAdapter *existingCodec, MediaInfoBase **outMediaInfo,
                               int parseMode, int parseFlags)
    {
        if (path == NULL) {
            return MUSICCORE_ERR_BAD_PARAM;
        }

        MediaInfoBase *media = NULL;
        if (existingCodec != NULL) {
            existingCodec->queryCachedInfo(path, &media);
            if (media != NULL) {
                *outMediaInfo = media;
                return 0;
            }
        }

        int sourceKind = MEDIA_SOURCE_INVALID;
        if (!probeSourcePath(path, &sourceKind)) {
            return MUSICCORE_ERR_NOT_FOUND;
        }

        CodecProbeEntry probeTable[4];
        memcpy(probeTable, kDefaultCodecProbeTable, sizeof(probeTable));
        int nextOrder = 0;

        if (hintCodec != CODEC_FORMAT_UNKNOWN) {
            for (unsigned int i = 0; i < 4u; ++i) {
                if (probeTable[i].codecType == hintCodec) {
                    if (probeTable[i].order == -1) {
                        probeTable[i].order = nextOrder++;
                    }
                    break;
                }
            }
        }

        if (doParse == 1) {
            const char *ext = sce_paf_strrchr(path, '.');
            if (ext != NULL) {
                int extCodec = CODEC_FORMAT_UNKNOWN;
                if (sce_paf_strcasecmp(ext, ".mp3") == 0) {
                    extCodec = CODEC_FORMAT_MP3;
                } else if (sce_paf_strcasecmp(ext, ".mp4") == 0 ||
                           sce_paf_strcasecmp(ext, ".m4a") == 0 ||
                           sce_paf_strcasecmp(ext, ".3gp") == 0) {
                    extCodec = CODEC_FORMAT_MP4;
                } else if (sce_paf_strcasecmp(ext, ".wav") == 0) {
                    extCodec = CODEC_FORMAT_WAV;
                } else if (sce_paf_strcasecmp(ext, ".at9") == 0) {
                    extCodec = CODEC_FORMAT_AT9;
                }

                if (extCodec != CODEC_FORMAT_UNKNOWN) {
                    for (unsigned int i = 0; i < 4u; ++i) {
                        if (probeTable[i].codecType == extCodec) {
                            if (probeTable[i].order == -1) {
                                probeTable[i].order = nextOrder++;
                            }
                            break;
                        }
                    }
                }
            }

            for (unsigned int i = 0; i < 4u; ++i) {
                if (probeTable[i].order == -1) {
                    probeTable[i].order = nextOrder++;
                }
            }
        }

        gParserAdapterSemaphore.Acquire();
        int err = 0;

        for (unsigned int step = 0; step < 4u; ++step) {
            CodecProbeEntry *selected = NULL;
            for (unsigned int i = 0; i < 4u; ++i) {
                if (probeTable[i].tried == 0 && probeTable[i].order == (int)step) {
                    selected = &probeTable[i];
                    break;
                }
            }
            if (selected == NULL) {
                continue;
            }

            switch (selected->codecType) {
            case CODEC_FORMAT_WAV:
                media = allocWavMediaInfo(&gWavFactory, path);
                break;
            case CODEC_FORMAT_MP3:
                if (getMp3Factory() != NULL) {
                    media = allocMp3MediaInfo(getMp3Factory(), path);
                }
                break;
            case CODEC_FORMAT_MP4:
                if (getMp4Factory() != NULL) {
                    media = allocMp4MediaInfo(getMp4Factory(), path);
                }
                break;
            case CODEC_FORMAT_AT9:
                if (getAt9Factory() != NULL) {
                    media = allocAt9MediaInfo(getAt9Factory(), path);
                }
                break;
            default:
                break;
            }

            if (media != NULL && doParse == 1) {
                int parseRet = media->parseInfo(parseMode, parseFlags);
                if (parseRet != 0) {
                    releaseMediaInfo(&media, 0);
                    err = mapDecoderError((unsigned int)parseRet);
                    if (err != MUSICCORE_ERR_FORMAT) {
                        break;
                    }
                } else {
                    break;
                }
            }
            selected->tried = 1;
        }

        if (media != NULL) {
            *outMediaInfo = media;
            return 0;
        }

        if (err == 0) {
            err = MUSICCORE_ERR_FAILED;
        }
        if (*outMediaInfo == NULL) {
            gParserAdapterSemaphore.Release();
        }
        return err;
    }

    /* 0x811E9A0E */
    int checkMediaCompat(const MediaInfoBase *mediaInfo)
    {
        if (activeCodec == NULL) {
            return 0;
        }
        return activeCodec->matchesMediaInfo(mediaInfo);
    }

    /* 0x811E9A20 */
    int startMedia(MediaInfoBase *mediaInfo, int startMs, int flagPrimary,
                   int flagSecondary, void *userCtx)
    {
        MediaInfoBase *localMedia = mediaInfo;
        if (localMedia == NULL) {
            return MUSICCORE_ERR_INVALID_ARG;
        }
        if (activeCodec == NULL) {
            releaseMediaInfo(&localMedia, 1);
            return MUSICCORE_ERR_FAILED;
        }
        int ret = activeCodec->startPlay(localMedia, startMs, flagPrimary, flagSecondary, userCtx);
        if (localMedia->isRetained == 0) {
            releaseMediaInfo(&localMedia, 1);
        }
        return ret;
    }

    /* 0x811E9A88 */
    int stopMedia()
    {
        if (activeCodec == NULL) {
            return MUSICCORE_ERR_FAILED;
        }
        return activeCodec->stopPlay();
    }

    /* 0x811E9A9E */
    int seekMedia(int mode, int posMs)
    {
        if (activeCodec == NULL) {
            return MUSICCORE_ERR_FAILED;
        }
        return activeCodec->seekPlay(mode, posMs);
    }
};

/* ===== WorkerThreadPool (0x811E9AF0 - 0x811E9ECF) ===== */

struct WorkerThreadSlot {
    int kind;
    const char *name;
    int priority;
    SceSize stackSize;
    SceUID threadId;
    int (*taskFn)(void *ctx, void *param);
    void *taskCtx;
    void *taskParam;
};

class WorkerThreadPool {
public:
    static const int MAX_SLOTS = 5;
    WorkerThreadSlot slots[MAX_SLOTS];

    /* 0x811E9AF0 */
    WorkerThreadPool()
    {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            slots[i].kind = 4;
            slots[i].name = NULL;
            slots[i].priority = 0;
            slots[i].stackSize = 0;
            slots[i].threadId = -1;
            slots[i].taskFn = NULL;
            slots[i].taskCtx = NULL;
            slots[i].taskParam = NULL;
        }
    }

    /* 0x811E9B34 / 0x811E9B68 */
    virtual ~WorkerThreadPool()
    {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots[i].threadId != -1) {
                workerThreadDelete(slots[i].threadId);
                slots[i].threadId = -1;
            }
        }
    }

    /* 0x811E9BA0 */
    static WorkerThreadPool *getInstance();

    /* 0x811E9BAA */
    int initialize(const ThreadSpec *specs, unsigned int count)
    {
        if (count > (unsigned int)MAX_SLOTS) {
            return MUSICCORE_ERR_INVALID_ARG;
        }

        SceUID createdIds[MAX_SLOTS];
        for (int i = 0; i < MAX_SLOTS; ++i) {
            createdIds[i] = -1;
        }

        int err = 0;
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (err != 0) {
                break;
            }
            int specIdx = (i >= 4) ? 0 : i;
            if (slots[i].threadId == -1) {
                err = workerThreadCreate(specs[specIdx].name, &createdIds[i],
                                         &WorkerThreadPool::workerThreadEntry,
                                         specs[specIdx].priority, specs[specIdx].stackSize);
            } else {
                err = MUSICCORE_ERR_BAD_STATE;
            }
        }

        if (err == 0) {
            for (int i = 0; i < MAX_SLOTS; ++i) {
                if (createdIds[i] != -1) {
                    int specIdx = (i >= 4) ? 0 : i;
                    slots[i].kind = specIdx;
                    slots[i].name = specs[specIdx].name;
                    slots[i].priority = specs[specIdx].priority;
                    slots[i].stackSize = specs[specIdx].stackSize;
                    slots[i].threadId = createdIds[i];
                }
            }
        } else {
            for (int i = 0; i < MAX_SLOTS; ++i) {
                if (createdIds[i] != -1) {
                    workerThreadDelete(createdIds[i]);
                    slots[i].kind = 4;
                    slots[i].name = NULL;
                    slots[i].priority = 0;
                    slots[i].stackSize = 0;
                    slots[i].threadId = -1;
                }
            }
        }
        return err;
    }

    /* 0x811E9CFA */
    int finalize()
    {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots[i].threadId != -1) {
                workerThreadDelete(slots[i].threadId);
                slots[i].kind = 4;
                slots[i].name = NULL;
                slots[i].priority = 0;
                slots[i].stackSize = 0;
                slots[i].threadId = -1;
                slots[i].taskFn = NULL;
                slots[i].taskCtx = NULL;
                slots[i].taskParam = NULL;
            }
        }
        return 0;
    }

    /* 0x811E9D4A */
    int startWorker(int kind, int (*taskFn)(void *, void *), void *taskCtx, void *taskParam, SceUID *outThid)
    {
        if (taskFn == NULL || taskCtx == NULL || taskParam == NULL || outThid == NULL || kind >= 4) {
            return MUSICCORE_ERR_INVALID_ARG;
        }

        int recreateIdx = -1;
        if (kind == 0) {
            if (slots[0].threadId == -1) {
                recreateIdx = 0;
            } else if (slots[0].taskFn != NULL && slots[4].threadId == -1) {
                recreateIdx = 4;
            }
        } else if (slots[kind].threadId == -1) {
            recreateIdx = kind;
        }

        if (recreateIdx >= 0 && recreateIdx < MAX_SLOTS && slots[recreateIdx].kind == kind) {
            int ret = workerThreadCreate(slots[recreateIdx].name, &slots[recreateIdx].threadId,
                                         &WorkerThreadPool::workerThreadEntry,
                                         slots[recreateIdx].priority, slots[recreateIdx].stackSize);
            if (ret != 0 || slots[recreateIdx].threadId == -1) {
                return ret;
            }
        }

        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots[i].kind == kind && slots[i].threadId != -1 && slots[i].taskFn == NULL) {
                slots[i].taskFn = taskFn;
                slots[i].taskCtx = taskCtx;
                slots[i].taskParam = taskParam;
                int ret = workerThreadStart(slots[i].threadId, sizeof(WorkerThreadSlot), &slots[i]);
                if (ret != 0) {
                    slots[i].taskFn = NULL;
                    slots[i].taskCtx = NULL;
                    slots[i].taskParam = NULL;
                } else {
                    *outThid = slots[i].threadId;
                }
                return ret;
            }
        }
        return MUSICCORE_ERR_FAILED;
    }

    /* 0x811E9E56 */
    int joinWorker(SceUID thid)
    {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots[i].threadId == thid && slots[i].taskFn != NULL) {
                workerThreadWaitEnd(thid);
                slots[i].taskFn = NULL;
                slots[i].taskCtx = NULL;
                slots[i].taskParam = NULL;
                return 0;
            }
        }
        return MUSICCORE_ERR_FAILED;
    }

    /* 0x811E9E8A */
    static int workerThreadEntry(SceSize argSize, void *argData)
    {
        if (argData == NULL || argSize != sizeof(WorkerThreadSlot)) {
            return -1;
        }
        WorkerThreadSlot *slot = static_cast<WorkerThreadSlot *>(argData);
        if (slot->taskFn == NULL) {
            return -1;
        }
        return slot->taskFn(slot->taskCtx, slot->taskParam);
    }
};

static WorkerThreadPool gWorkerThreadPool;

/* 0x811E9BA0 */
WorkerThreadPool *WorkerThreadPool::getInstance()
{
    return &gWorkerThreadPool;
}

} // namespace MusicCore
} // namespace sce
