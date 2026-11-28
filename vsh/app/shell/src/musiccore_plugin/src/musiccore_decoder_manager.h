/*
 * SceShell musiccore_plugin - DecoderManager, CodecAdapter, and WorkerThreadPool declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_DECODER_MANAGER_H
#define VSH_APP_SHELL_MUSICCORE_DECODER_MANAGER_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_decoder_pipeline.h"
#include "musiccore_sync.h"

namespace sce {
namespace MusicCore {

class MediaInfoBase;

struct ParserAdapterBase {
    virtual ~ParserAdapterBase() {}
    virtual int onPipeConsumed(int frameFlags) { (void)frameFlags; return 0; }
    virtual int onPipeUnderflow() { return 0; }
    virtual int onFrameConsumed(int frameFlags) { (void)frameFlags; return 0; }
    virtual int runLoop() { return 0; }
    virtual int openStream() { return 0; }
    virtual int closeStream() { return 0; }
    virtual int produceFrame(void *outPkt) { (void)outPkt; return 0; }
    virtual int findCachedMediaInfo(const char *path, MediaInfoBase **outInfo) = 0;
};

struct ThreadSpec {
    const char *name;
    int priority;
    SceSize stackSize;
};

class CodecEventListener {
public:
    virtual ~CodecEventListener() {}
    virtual void onEndOfStream() = 0;
    virtual void onDecodeError(int stage, unsigned int decErr) = 0;
    virtual void onBufferNotice() {}
    virtual void onOpenComplete(int status, int param) = 0;
    virtual void onStopComplete(int status) = 0;
    virtual void onPauseComplete(int status) = 0;
    virtual void onSeekComplete(int status) = 0;
    virtual void onPositionReady(int posMs, int status) = 0;
};

class CodecAdapter : public CodecEventListener {
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
    DecoderPipeline *decoder;
    SyncMutex waitMutex;
    SyncCond waitCond;
    int lastPositionMs;
    int lastAsyncStatus;
    int lastAsyncParam;
    uint8_t lastEventType;
    uint8_t padActive[3];

    CodecAdapter(const char *mutexName, const char *condName);
    virtual ~CodecAdapter();

    virtual void onEndOfStream();
    virtual void onDecodeError(int stage, unsigned int decErr);
    virtual void onOpenComplete(int status, int param);
    virtual void onStopComplete(int status);
    virtual void onPauseComplete(int status);
    virtual void onSeekComplete(int status);
    virtual void onPositionReady(int posMs, int status);

    virtual int openDecoder(int mode, int extraParam) = 0;
    virtual int closeDecoder() = 0;
    virtual int startPlay(MediaInfoBase *mediaInfo, int startMs, int endMs, int playSpeed, void *userCtx);
    virtual int pausePlay();
    virtual int stopPlay();
    virtual int seekPlay(int mode, int posMs);
    virtual int getPosition(int *outPosMs);
    virtual int isOpened();
    virtual int getMetadata(MediaInfoBase *mediaInfo, int *outVal);
    virtual int queryCachedInfo(const char *path, MediaInfoBase **outInfo);
    virtual int matchesMediaInfo(const MediaInfoBase *mediaInfo) = 0;

    int setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn, void *stopFn, void *flushFn);
    int setEventCallbacks(MediaReadyCallback readyCb, ErrorCallback errCb,
                          EndOfStreamCallback endCb, void *cbCtx);
};

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

    DecoderManager();
    virtual ~DecoderManager();

    static void getThreadSpecs(ThreadSpec *outSpecs, unsigned int *inOutCount);
    int setOutputCallbacks(void *outCtx, void *startFn, void *enqueueFn, void *stopFn, void *flushFn);
    int setEventCallbacks(CodecAdapter::MediaReadyCallback readyCb,
                          CodecAdapter::ErrorCallback errCb,
                          CodecAdapter::EndOfStreamCallback endCb,
                          void *cbCtx);
    int openCodec(int codecType, int sourceMode, int extraParam);
    int closeCodec();
    static bool probeSourcePath(const char *path, int *outSourceKind);
    static int releaseMediaInfo(MediaInfoBase **mediaInfoPtr, uint8_t releaseSemaphore);
    static int createMediaInfo(const char *path, int hintCodec, uint8_t doParse,
                               CodecAdapter *existingCodec, MediaInfoBase **outMediaInfo,
                               int parseMode, int parseFlags);
    int checkMediaCompat(const MediaInfoBase *mediaInfo);
    int startMedia(MediaInfoBase *mediaInfo, int startMs, int endMs,
                   int playSpeed, void *userCtx);
    int stopMedia();
    int seekMedia(int mode, int posMs);
};

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

    WorkerThreadPool();
    virtual ~WorkerThreadPool();

    static WorkerThreadPool *getInstance();
    int initialize(const ThreadSpec *specs, unsigned int count);
    int finalize();
    int startWorker(int kind, int (*taskFn)(void *, void *), void *taskCtx, void *taskParam, SceUID *outThid);
    int joinWorker(SceUID thid);
    static int workerThreadEntry(SceSize argSize, void *argData);
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_DECODER_MANAGER_H */
