/*
 * SceShell musiccore_plugin - DecoderPipeline class and event queue declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_DECODER_PIPELINE_H
#define VSH_APP_SHELL_MUSICCORE_DECODER_PIPELINE_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_sync.h"

namespace sce {
namespace MusicCore {

class ParserFilter;
class BufRefPipe;
class DecoderFilter;
class AudioOutputSink;
class PipelineEventCallback;

class ActiveFilterListener {
public:
    virtual ~ActiveFilterListener() {}
    virtual void onFilterEvent(int eventKind, void *sourceFilter, int statusCode) = 0;
};

class AudioOutputListener {
public:
    virtual ~AudioOutputListener() {}
    virtual void onPlaybackProgress(int positionMs) = 0;
};

struct PipelineEventEntry {
    uint32_t eventCode;
    int statusCode;
    uint16_t sourceStage;
    uint16_t stageFlagsSnapshot;
};

struct PipelineEventQueue {
    PipelineEventEntry *entries;
    int capacity;
    int headIndex;
    int tailIndex;

    int push(uint32_t eventCode, int statusCode, uint16_t sourceStage, uint16_t flagsSnapshot);
    int pop(PipelineEventEntry *outEntry);
};

class DecoderPipeline : public ActiveFilterListener, public AudioOutputListener {
public:
    DecoderPipeline(ParserFilter *parser, BufRefPipe *pipe,
                    DecoderFilter *decoder, AudioOutputSink *audioSink);
    virtual ~DecoderPipeline();

    bool isReady() const;

    int requestStart(void *mediaItem, int startMs, int endMs, int playSpeed);
    int requestStop();
    int requestSetSpeed(int playSpeed);
    int requestQueryMetadata(void *metaOut);
    int requestGetPosition();

    ParserFilter *getParser() const { return m_parser; }
    BufRefPipe *getPipe() const { return m_pipe; }
    DecoderFilter *getDecoder() const { return m_decoder; }
    AudioOutputSink *getAudioSink() const { return m_audioSink; }
    void setCallback(PipelineEventCallback *callback) { m_callback = callback; }

    virtual void onFilterEvent(int eventKind, void *sourceFilter, int statusCode);
    virtual void onPlaybackProgress(int positionMs);

private:
    static int workerThreadEntry(SceSize argSize, void *argData);

    void runEventLoop();
    void stopAllStages();
    void handleCommandInEosStopping(int command);
    void handleCommandInRunning(int command);
    void handleCommandInIdle(int command);
    void handleEventInErrorStopping(uint32_t eventCode, int statusCode, uint16_t sourceStage);
    void handleEventInSpeedChange(uint32_t eventCode, int statusCode, uint16_t sourceStage);
    void handleEventInStarting(uint32_t eventCode, int statusCode, uint16_t sourceStage);
    void handleEventInStopping(uint32_t eventCode, int statusCode, uint16_t sourceStage);
    void handleEventInRunning(uint32_t eventCode, int statusCode, uint16_t sourceStage);
    void rejectCommand(int command);

    ParserFilter *m_parser;                   /* +8 */
    BufRefPipe *m_pipe;                       /* +12 */
    DecoderFilter *m_decoder;                 /* +16 */
    AudioOutputSink *m_audioSink;             /* +20 */
    AudioOutputSink *m_audioControl;          /* +24 */
    PipelineEventCallback *m_callback;        /* +28 */
    int m_state;                              /* +32 */
    int m_workerThreadId;                     /* +36 */
    SyncMutex m_mutex;                        /* +40 */
    SyncCond m_cond;                          /* +48 */
    uint16_t m_stageFlags;                    /* +56 */
    uint16_t m_padStageFlags;                 /* +58 */
    int m_pendingCommand;                     /* +60 */
    void *m_startMediaItem;                   /* +64 */
    int m_startPositionMs;                    /* +68 */
    int m_endPositionMs;                      /* +72 */
    int m_startPlaySpeed;                     /* +76 */
    int m_pendingSpeed;                       /* +80 */
    void *m_pendingMetaOut;                   /* +84 */
    uint16_t m_reservedStateWord;             /* +88 */
    uint16_t m_padReservedWord;               /* +90 */
    PipelineEventEntry m_eventStorage[16];    /* +92..+283 */
    PipelineEventQueue m_eventQueue;          /* +284..+299 */
    int m_pendingError;                       /* +300 */
    int m_fallbackPositionMs;                 /* +304 */
    int m_trackDurationMs;                    /* +308 */
    DecoderPipeline **m_threadArg;            /* +312 */
    uint8_t m_usesThreadPool;                 /* +316 */
    uint8_t m_padTail[3];                     /* +317..+319 */
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_DECODER_PIPELINE_H */
