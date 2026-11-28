/*
 * SceShell musiccore_plugin - DecoderManager, WorkerThreadPool, and CodecAdapter
 * (AT9/MP3/MP4/WAV codec adapters and media format probing).
 * Original address range (3.65-CEX): 0x811E91C4 - 0x811EA894
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_codec_factories.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_decoder_pipeline.h"
#include "musiccore_heap.h"
#include "musiccore_mp3_readers.h"
#include "musiccore_sync.h"
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


extern int createWorkerThread(const char *name, SceUID *outThreadId, SceKernelThreadEntry entry,
                              int initPriority, SceSize stackSize);
extern int startWorkerThread(SceUID threadId, SceSize argSize, void *argData);
extern int joinWorkerThread(SceUID threadId);
extern int deleteWorkerThread(SceUID threadId);

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
      lastEventType(0),
      waitMutex(mutexName),
      waitCond(condName)
{
}

/* 0x811E9F2C */
CodecAdapter::~CodecAdapter()
{
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
int CodecAdapter::startPlay(MediaInfoBase *mediaInfo, int startMs, int endMs,
                            int playSpeed, void *userCtx)
{
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    if (mediaInfo == NULL || userCtx == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    waitMutex.lock();
    pendingUserCtx = userCtx;
    unsigned int decErr = static_cast<unsigned int>(decoder->requestStart(mediaInfo, startMs, endMs, playSpeed));
    if (decErr != 0) {
        waitMutex.unlock();
        return mapDecoderError(decErr);
    }

    waitCond.wait(&waitMutex, 1u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();

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
    if (decoder->requestSetSpeed(0) != 0) {
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

    waitMutex.lock();
    unsigned int decErr = static_cast<unsigned int>(decoder->requestStop());
    if (decErr != 0) {
        waitMutex.unlock();
        return mapDecoderError(decErr);
    }

    waitCond.wait(&waitMutex, 2u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
    return status;
}

/* 0x811EA120 */
int CodecAdapter::seekPlay(int mode, int posMs)
{
    (void)mode;
    if (decoder == NULL) {
        return MUSICCORE_ERR_FAILED;
    }

    waitMutex.lock();
    unsigned int decErr = static_cast<unsigned int>(decoder->requestSetSpeed(posMs));
    if (decErr != 0) {
        waitMutex.unlock();
        return mapDecoderError(decErr);
    }

    waitCond.wait(&waitMutex, 16u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
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

    waitMutex.lock();
    if (decoder->requestGetPosition() != 0) {
        waitMutex.unlock();
        return MUSICCORE_ERR_FAILED;
    }

    waitCond.wait(&waitMutex, 32u, NULL);
    *outPosMs = lastPositionMs;
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
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
    ParserAdapterBase *parser = reinterpret_cast<ParserAdapterBase *>(decoder->getParser());
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
    waitMutex.lock();
    lastAsyncStatus = status;
    lastAsyncParam = param;
    lastEventType = 1;
    waitCond.signal(1u);
    waitMutex.unlock();
}

/* 0x811EA298 */
void CodecAdapter::onStopComplete(int status)
{
    waitMutex.lock();
    lastAsyncStatus = status;
    lastEventType = 2;
    waitCond.signal(2u);
    waitMutex.unlock();
}

/* 0x811EA2C2 */
void CodecAdapter::onPauseComplete(int status)
{
    waitMutex.lock();
    lastAsyncStatus = status;
    lastEventType = 3;
    waitCond.signal(4u);
    waitMutex.unlock();
}

/* 0x811EA2EC */
void CodecAdapter::onSeekComplete(int status)
{
    waitMutex.lock();
    lastAsyncStatus = status;
    lastEventType = 4;
    waitCond.signal(16u);
    waitMutex.unlock();
}

/* 0x811EA316 */
void CodecAdapter::onPositionReady(int posMs, int status)
{
    waitMutex.lock();
    lastPositionMs = posMs;
    lastAsyncStatus = status;
    lastEventType = 5;
    waitCond.signal(32u);
    waitMutex.unlock();
}

/* ===== CodecAdapterAt9 (0x811EA348 - 0x811EA4A7) ===== */

class CodecAdapterAt9 : public CodecAdapter {
public:
    CodecAdapterAt9(const char *mutexName, const char *condName);
    virtual ~CodecAdapterAt9();
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
    virtual int openDecoder(int sourceMode, int extraParam);
    virtual int closeDecoder();
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo);
};

/* 0x811EA348 */
CodecAdapterAt9::CodecAdapterAt9(const char *mutexName, const char *condName)
    : CodecAdapter(mutexName, condName)
{}

/* 0x811EA360 / 0x811EA37C */
CodecAdapterAt9::~CodecAdapterAt9() {}

/* 0x811EA39C */
int CodecAdapterAt9::queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
{
    At9Factory *factory = getAt9Factory();
    if (factory == NULL) {
        return 0;
    }
    return factory->getBufferSizes(parserStack, decoderStack, mgrStack);
}

/* 0x811EA3B8 */
int CodecAdapterAt9::openDecoder(int sourceMode, int extraParam)
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

    At9Factory *factory = getAt9Factory();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder = reinterpret_cast<DecoderPipeline *>(factory->createPipeline(
        ioMode, reinterpret_cast<AudioOpenCallback>(outputStartFn),
        reinterpret_cast<AudioCloseCallback>(outputStopFn),
        reinterpret_cast<AudioWriteCallback>(outputEnqueueFn),
        reinterpret_cast<AudioFlushCallback>(outputFlushFn), outputCtx, extraParam));
    if (decoder == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder->setCallback(reinterpret_cast<PipelineEventCallback *>(this));
    return 0;
}

/* 0x811EA42A */
int CodecAdapterAt9::closeDecoder()
{
    if (decoder == NULL) {
        return 0;
    }
    At9Factory *factory = getAt9Factory();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    waitMutex.lock();
    decoder->requestStop();
    waitCond.wait(&waitMutex, 2u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
    if (status != 0) {
        return status;
    }
    int ret = getMp3Factory()->destroyPipeline(reinterpret_cast<PipelineEventListener *>(decoder));
    decoder = NULL;
    return ret;
}

/* 0x811EA48C */
int CodecAdapterAt9::matchesMediaInfo(const MediaInfoBase *mediaInfo)
{
    return (mediaInfo != NULL && mediaInfo->m_mediaFourcc == FOURCC_AT9) ? 1 : 0;
}

/* ===== CodecAdapterMp3 (0x811EA4A8 - 0x811EA603) ===== */

class CodecAdapterMp3 : public CodecAdapter {
public:
    CodecAdapterMp3(const char *mutexName, const char *condName);
    virtual ~CodecAdapterMp3();
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
    virtual int openDecoder(int sourceMode, int extraParam);
    virtual int closeDecoder();
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo);
};

/* 0x811EA4A8 */
CodecAdapterMp3::CodecAdapterMp3(const char *mutexName, const char *condName)
    : CodecAdapter(mutexName, condName)
{}

/* 0x811EA4C0 / 0x811EA4DC */
CodecAdapterMp3::~CodecAdapterMp3() {}

/* 0x811EA4FC */
int CodecAdapterMp3::queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
{
    Mp3Factory *factory = getMp3Factory();
    if (factory == NULL) {
        return 0;
    }
    return factory->getBufferSizes(parserStack, decoderStack, mgrStack);
}

/* 0x811EA518 */
int CodecAdapterMp3::openDecoder(int sourceMode, int extraParam)
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

    Mp3Factory *factory = getMp3Factory();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder = reinterpret_cast<DecoderPipeline *>(factory->createPipeline(
        ioMode, reinterpret_cast<AudioOpenCallback>(outputStartFn),
        reinterpret_cast<AudioCloseCallback>(outputStopFn),
        reinterpret_cast<AudioWriteCallback>(outputEnqueueFn),
        reinterpret_cast<AudioFlushCallback>(outputFlushFn), outputCtx));
    if (decoder == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder->setCallback(reinterpret_cast<PipelineEventCallback *>(this));
    return 0;
}

/* 0x811EA584 */
int CodecAdapterMp3::closeDecoder()
{
    if (decoder == NULL) {
        return 0;
    }
    Mp3Factory *factory = getMp3Factory();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    waitMutex.lock();
    decoder->requestStop();
    waitCond.wait(&waitMutex, 2u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
    if (status != 0) {
        return status;
    }
    int ret = factory->destroyPipeline(reinterpret_cast<PipelineEventListener *>(decoder));
    decoder = NULL;
    return ret;
}

/* 0x811EA5E6 */
int CodecAdapterMp3::matchesMediaInfo(const MediaInfoBase *mediaInfo)
{
    return (mediaInfo != NULL && mediaInfo->m_mediaFourcc == FOURCC_MP3) ? 1 : 0;
}

/* ===== CodecAdapterMp4 (0x811EA604 - 0x811EA75F) ===== */

class CodecAdapterMp4 : public CodecAdapter {
public:
    CodecAdapterMp4(const char *mutexName, const char *condName);
    virtual ~CodecAdapterMp4();
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
    virtual int openDecoder(int sourceMode, int extraParam);
    virtual int closeDecoder();
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo);
};

/* 0x811EA604 */
CodecAdapterMp4::CodecAdapterMp4(const char *mutexName, const char *condName)
    : CodecAdapter(mutexName, condName)
{}

/* 0x811EA61C / 0x811EA638 */
CodecAdapterMp4::~CodecAdapterMp4() {}

/* 0x811EA658 */
int CodecAdapterMp4::queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
{
    Mp4Factory *factory = getMp4FactoryInstance();
    if (factory == NULL) {
        return 0;
    }
    return static_cast<int>(factory->getHeapSizes(parserStack, decoderStack, mgrStack));
}

/* 0x811EA674 */
int CodecAdapterMp4::openDecoder(int sourceMode, int extraParam)
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

    Mp4Factory *factory = getMp4FactoryInstance();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder = reinterpret_cast<DecoderPipeline *>(factory->createPipeline(
        ioMode, reinterpret_cast<AudioOpenCallback>(outputStartFn),
        reinterpret_cast<AudioCloseCallback>(outputStopFn),
        reinterpret_cast<AudioWriteCallback>(outputEnqueueFn),
        reinterpret_cast<AudioFlushCallback>(outputFlushFn), outputCtx));
    if (decoder == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder->setCallback(reinterpret_cast<PipelineEventCallback *>(this));
    return 0;
}

/* 0x811EA6E0 */
int CodecAdapterMp4::closeDecoder()
{
    if (decoder == NULL) {
        return 0;
    }
    Mp4Factory *factory = getMp4FactoryInstance();
    if (factory == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    waitMutex.lock();
    decoder->requestStop();
    waitCond.wait(&waitMutex, 2u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
    if (status != 0) {
        return status;
    }
    int ret = getMp3Factory()->destroyPipeline(reinterpret_cast<PipelineEventListener *>(decoder));
    decoder = NULL;
    return ret;
}

/* 0x811EA742 */
int CodecAdapterMp4::matchesMediaInfo(const MediaInfoBase *mediaInfo)
{
    return (mediaInfo != NULL && mediaInfo->m_mediaFourcc == FOURCC_MP4) ? 1 : 0;
}

/* ===== CodecAdapterWav (0x811EA760 - 0x811EA893) ===== */

class CodecAdapterWav : public CodecAdapter {
public:
    CodecAdapterWav(const char *mutexName, const char *condName);
    virtual ~CodecAdapterWav();
    static int queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack);
    virtual int openDecoder(int sourceMode, int extraParam);
    virtual int closeDecoder();
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo);
};

/* 0x811EA760 */
CodecAdapterWav::CodecAdapterWav(const char *mutexName, const char *condName)
    : CodecAdapter(mutexName, condName)
{}

/* 0x811EA778 / 0x811EA794 */
CodecAdapterWav::~CodecAdapterWav() {}

/* 0x811EA7B4 */
int CodecAdapterWav::queryStackSizes(unsigned int *parserStack, unsigned int *decoderStack, unsigned int *mgrStack)
{
    return g_wavFactory.getBufferSizes(parserStack, decoderStack, mgrStack);
}

/* 0x811EA7C6 */
int CodecAdapterWav::openDecoder(int sourceMode, int extraParam)
{
    (void)sourceMode;
    (void)extraParam;
    if (decoder != NULL) {
        return 0;
    }
    decoder = reinterpret_cast<DecoderPipeline *>(g_wavFactory.createPipeline(
        reinterpret_cast<AudioOpenCallback>(outputStartFn),
        reinterpret_cast<AudioCloseCallback>(outputStopFn),
        reinterpret_cast<AudioWriteCallback>(outputEnqueueFn),
        reinterpret_cast<AudioFlushCallback>(outputFlushFn), outputCtx));
    if (decoder == NULL) {
        return MUSICCORE_ERR_NO_MEMORY;
    }
    decoder->setCallback(reinterpret_cast<PipelineEventCallback *>(this));
    return 0;
}

/* 0x811EA812 */
int CodecAdapterWav::closeDecoder()
{
    if (decoder == NULL) {
        return 0;
    }
    waitMutex.lock();
    decoder->requestStop();
    waitCond.wait(&waitMutex, 2u, NULL);
    int status = mapDecoderError((unsigned int)lastAsyncStatus);
    waitMutex.unlock();
    if (status != 0) {
        return status;
    }
    int ret = g_wavFactory.destroyPipeline(reinterpret_cast<PipelineEventListener *>(decoder));
    decoder = NULL;
    return ret;
}

/* 0x811EA876 */
int CodecAdapterWav::matchesMediaInfo(const MediaInfoBase *mediaInfo)
{
    return (mediaInfo != NULL && mediaInfo->m_mediaFourcc == FOURCC_WAV) ? 1 : 0;
}

/* ===== Parser Adapter Semaphore (0x811E9AB4) ===== */

static paf::thread::Semaphore gParserAdapterSemaphore("SceShellMusicCorePAdapterNum", 2, 2, 0);

/* ===== DecoderManager (0x811E91C4 - 0x811E9AB3) ===== */

/* 0x811E91C4 */
DecoderManager::DecoderManager()
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
DecoderManager::~DecoderManager() {}

/* 0x811E9212 */
void DecoderManager::getThreadSpecs(ThreadSpec *outSpecs, unsigned int *inOutCount)
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
int DecoderManager::setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn, void *stopFn, void *flushFn)
{
    outputCtx = outCtx;
    outputStartFn = startFn;
    outputStopFn = stopFn;
    outputEnqueueFn = enqueueFn;
    outputFlushFn = flushFn;
    return 0;
}

/* 0x811E9362 */
int DecoderManager::setEventCallbacks(CodecAdapter::MediaReadyCallback readyCb,
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
int DecoderManager::openCodec(int codecType, int sourceMode, int extraParam)
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
        void *mem = localAlloc(sizeof(CodecAdapterWav));
        if (mem != NULL) {
            codec = new (mem) CodecAdapterWav("SceShellMusicCoreCodecWav", "SceShellMusicCoreCodecWav");
        }
        activeCodec = codec;
        activeCodecType = CODEC_FORMAT_WAV;
        break;
    }
    case CODEC_FORMAT_MP3: {
        void *mem = localAlloc(sizeof(CodecAdapterMp3));
        if (mem != NULL) {
            codec = new (mem) CodecAdapterMp3("SceShellMusicCoreCodecMp3", "SceShellMusicCoreCodecMp3");
        }
        activeCodec = codec;
        activeCodecType = CODEC_FORMAT_MP3;
        break;
    }
    case CODEC_FORMAT_MP4: {
        void *mem = localAlloc(sizeof(CodecAdapterMp4));
        if (mem != NULL) {
            codec = new (mem) CodecAdapterMp4("SceShellMusicCoreCodecMp4", "SceShellMusicCoreCodecMp4");
        }
        activeCodec = codec;
        activeCodecType = CODEC_FORMAT_MP4;
        break;
    }
    case CODEC_FORMAT_AT9: {
        void *mem = localAlloc(sizeof(CodecAdapterAt9));
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
int DecoderManager::closeCodec()
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
bool DecoderManager::probeSourcePath(const char *path, int *outSourceKind)
{
    if (path == NULL) {
        return false;
    }
    if (sce_paf_strncmp(path, "sd0:", sce_paf_strlen("sd0:")) == 0) {
        *outSourceKind = MEDIA_SOURCE_LOCAL;
        return path[0] != '\0';
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
    return path[0] != '\0';
}

/* 0x811E9606 */
int DecoderManager::releaseMediaInfo(MediaInfoBase **mediaInfoPtr, uint8_t releaseSemaphore)
{
    MediaInfoBase *media = *mediaInfoPtr;
    if (media == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }

    void *factory = NULL;
    if (media->m_mediaFourcc == FOURCC_MP3) {
        factory = getMp3Factory();
    } else if (media->m_mediaFourcc == FOURCC_MP4) {
        factory = getMp4FactoryInstance();
    } else if (media->m_mediaFourcc == FOURCC_WAV) {
        factory = &g_wavFactory;
    } else if (media->m_mediaFourcc == FOURCC_AT9) {
        factory = getAt9Factory();
    }

    if (factory != NULL) {
        void *ptr = *mediaInfoPtr;
        getMp3Factory()->destroyMediaInfo(&ptr);
        *mediaInfoPtr = NULL;
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
int DecoderManager::createMediaInfo(const char *path, int hintCodec, uint8_t doParse,
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
            media = reinterpret_cast<MediaInfoBase *>(g_wavFactory.createMediaInfo(path));
            break;
        case CODEC_FORMAT_MP3:
            if (getMp3Factory() != NULL) {
                media = reinterpret_cast<MediaInfoBase *>(getMp3Factory()->createMediaInfo(path, 16));
            }
            break;
        case CODEC_FORMAT_MP4:
            if (getMp4FactoryInstance() != NULL) {
                media = reinterpret_cast<MediaInfoBase *>(getMp4FactoryInstance()->createMediaInfo(path, 16));
            }
            break;
        case CODEC_FORMAT_AT9:
            if (getAt9Factory() != NULL) {
                media = reinterpret_cast<MediaInfoBase *>(getAt9Factory()->createMediaInfo(path, 0));
            }
            break;
        default:
            break;
        }

        if (media != NULL && doParse == 1) {
            typedef int (*ParseInfoFn)(MediaInfoBase *, int, int);
            int parseRet = (*reinterpret_cast<ParseInfoFn **>(media))[2](media, parseMode, parseFlags);
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
int DecoderManager::checkMediaCompat(const MediaInfoBase *mediaInfo)
{
    if (activeCodec == NULL) {
        return 0;
    }
    return activeCodec->matchesMediaInfo(mediaInfo);
}

/* 0x811E9A20 */
int DecoderManager::startMedia(MediaInfoBase *mediaInfo, int startMs, int endMs,
                               int playSpeed, void *userCtx)
{
    MediaInfoBase *localMedia = mediaInfo;
    if (localMedia == NULL) {
        return MUSICCORE_ERR_INVALID_ARG;
    }
    if (activeCodec == NULL) {
        releaseMediaInfo(&localMedia, 1);
        return MUSICCORE_ERR_FAILED;
    }
    int ret = activeCodec->startPlay(localMedia, startMs, endMs, playSpeed, userCtx);
    if (localMedia->m_sharedFlag == 0) {
        releaseMediaInfo(&localMedia, 1);
    }
    return ret;
}

/* 0x811E9A88 */
int DecoderManager::stopMedia()
{
    if (activeCodec == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    return activeCodec->stopPlay();
}

/* 0x811E9A9E */
int DecoderManager::seekMedia(int mode, int posMs)
{
    if (activeCodec == NULL) {
        return MUSICCORE_ERR_FAILED;
    }
    return activeCodec->seekPlay(mode, posMs);
}

/* ===== WorkerThreadPool (0x811E9AF0 - 0x811E9ECF) ===== */

/* 0x811E9AF0 */
WorkerThreadPool::WorkerThreadPool()
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
WorkerThreadPool::~WorkerThreadPool()
{
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots[i].threadId != -1) {
            deleteWorkerThread(slots[i].threadId);
            slots[i].threadId = -1;
        }
    }
}

/* 0x811E9BAA */
int WorkerThreadPool::initialize(const ThreadSpec *specs, unsigned int count)
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
            err = createWorkerThread(specs[specIdx].name, &createdIds[i],
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
                deleteWorkerThread(createdIds[i]);
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
int WorkerThreadPool::finalize()
{
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots[i].threadId != -1) {
            deleteWorkerThread(slots[i].threadId);
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
int WorkerThreadPool::startWorker(int kind, int (*taskFn)(void *, void *), void *taskCtx, void *taskParam, SceUID *outThid)
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
        int ret = createWorkerThread(slots[recreateIdx].name, &slots[recreateIdx].threadId,
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
            int ret = startWorkerThread(slots[i].threadId, sizeof(WorkerThreadSlot), &slots[i]);
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
int WorkerThreadPool::joinWorker(SceUID thid)
{
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots[i].threadId == thid && slots[i].taskFn != NULL) {
            joinWorkerThread(thid);
            slots[i].taskFn = NULL;
            slots[i].taskCtx = NULL;
            slots[i].taskParam = NULL;
            return 0;
        }
    }
    return MUSICCORE_ERR_FAILED;
}

/* 0x811E9E8A */
int WorkerThreadPool::workerThreadEntry(SceSize argSize, void *argData)
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

static WorkerThreadPool gWorkerThreadPool;

/* 0x811E9BA0 */
WorkerThreadPool *WorkerThreadPool::getInstance()
{
    return &gWorkerThreadPool;
}

} // namespace MusicCore
} // namespace sce
