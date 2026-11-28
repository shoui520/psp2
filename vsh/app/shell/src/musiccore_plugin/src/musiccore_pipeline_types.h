/*
 * SceShell musiccore_plugin - Shared decoder pipeline, filter, pipe, and
 * audio output interfaces.
 * Verified against 3.65-CEX vtables (0x81534564 - 0x81534A50).
 */

#ifndef VSH_APP_SHELL_MUSICCORE_PIPELINE_TYPES_H
#define VSH_APP_SHELL_MUSICCORE_PIPELINE_TYPES_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_heap.h"
#include "musiccore_sync.h"

namespace sce {
namespace MusicCore {

typedef SyncCond SyncEvent;

/* Audio output callbacks passed into AudioOutputFilter ("SceShellMusicCoreAIF") */
typedef int (*AudioBufferConsumedCallback)(void *pcmBuffer);
typedef int (*AudioOpenCallback)(void *userContext);
typedef int (*AudioCloseCallback)(void *userContext);
typedef int (*AudioWriteCallback)(void *userContext, int timestampMs, int channels,
                                  int sampleRate, int dataBytes, void *pcmBuffer,
                                  AudioBufferConsumedCallback onBufferConsumed);
typedef void (*AudioFlushCallback)(void *userContext);

/* Pipeline event listener (implemented by DecoderPipeline) */
class PipelineEventListener {
public:
    virtual ~PipelineEventListener() {}
    virtual void onFilterEvent(int eventCode, void *senderFilter, int detailCode) = 0;
};

/* Playback position listener (called when integer seconds change) */
class PositionChangeListener {
public:
    virtual ~PositionChangeListener() {}
    virtual void onPositionSecondChanged(int positionMs) = 0;
};

/* 76-byte frame packet carried through FixedFramePipe, BufRefPipe, and AudioOutputFilter */
struct MediaFrameBuffer {
    void *frameData;            /* +0 */
    uint32_t maxBufferSize;     /* +4 */
    int payloadSize;            /* +8 */
    float timestampMs;          /* +12 */
    float frameDurationMs;      /* +16 */
    int sampleRate;             /* +20 */
    int channelConfig;          /* +24 */
    int packetFlags;            /* +28: 0=normal, 1=trick, 2=eos, 3=error, 4=skip, 255=unset */
    uint8_t discontinuity;      /* +32 */
    uint8_t padDiscontinuity[3];/* +33..+35 */
    int frameFlags;             /* +36 */
    uint16_t leadingSkip;       /* +40 */
    uint16_t trailingSkip;      /* +42 */
    uint8_t mpegVersion;        /* +44 */
    uint8_t padCodecMeta[3];    /* +45..+47 */
    uint32_t extSampleRate;     /* +48 */
    uint32_t samplesPerFrame;   /* +52 */
    int streamErrorCode;        /* +56 */
    uint32_t superframeBytes;   /* +60 */
    uint8_t channelCount;       /* +64 */
    uint8_t padChannelCount[3]; /* +65..+67 */
    uint32_t trimStartSample;   /* +68 */
    uint32_t trimEndSample;     /* +72 */
};

/* Base filter interface (vtable 0x81534588) */
class FilterBase {
public:
    FilterBase() {}
    virtual ~FilterBase() {}

    virtual void setEventListener(PipelineEventListener *listener) = 0;
    virtual bool isInitialized() const = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
};

/* Active thread-backed filter base (40 bytes, 0x81206E14 - 0x81207087) */
class ActiveFilterBase : public FilterBase {
public:
    ActiveFilterBase(int workerKind, const char *threadName);
    virtual ~ActiveFilterBase();

    virtual void setEventListener(PipelineEventListener *listener);
    virtual bool isInitialized() const;
    virtual int start();
    virtual int stop();
    virtual int runLoop() = 0;

    bool isRunning() const;
    void workerThreadMain();

protected:
    PipelineEventListener *m_eventListener; /* +4 */
    SyncEvent m_wakeEvent;                  /* +8 */
    SyncMutex m_lock;                       /* +16 */
    SceUID m_workerId;                      /* +24 */
    int m_runState;                         /* +28: 1=running, 2=stopped, 3=exiting */
    ActiveFilterBase **m_workerSelfArg;     /* +32 */
    uint8_t m_useWorkerPool;                /* +36 */
    uint8_t m_padWorkerPool[3];             /* +37..+39 */
};

/* Pipe producer notification interface (vtable 0x8153478C) */
class PipeProducerControl {
public:
    PipeProducerControl() {}
    virtual ~PipeProducerControl() {}

    virtual int onPipeConsumed(int frameFlags) = 0;
    virtual int onPipeUnderflow() = 0;
};

/* Pipe consumer notification interface (vtable 0x81534778) */
class PipeConsumerControl {
public:
    PipeConsumerControl() {}
    virtual ~PipeConsumerControl() {}

    virtual int onPipeProduced() = 0;
    virtual int onPipeFull() = 0;
};

/* Pipe transport interface (vtable 0x81534740) */
class PipeInterface {
public:
    PipeInterface() {}
    virtual ~PipeInterface() {}

    virtual void setPositionListener(PositionChangeListener *listener) = 0;
    virtual bool isInitialized() const = 0;
    virtual int getPlayPositionMs() const = 0;
    virtual void setPlayPositionMs(int posMs) = 0;
    virtual void reset() = 0;
    virtual bool hasQueuedFrame() const = 0;
    virtual bool hasReadableFrame() const = 0;
    virtual int acquireWriteSlot(MediaFrameBuffer **outBuf) = 0;
    virtual int commitWriteSlot(MediaFrameBuffer *buf) = 0;
    virtual int acquireReadSlot(MediaFrameBuffer **outBuf) = 0;
    virtual int releaseReadSlot(MediaFrameBuffer *buf) = 0;
};

/* Base media info handle (musiccore_mp3_readers.cpp) */
struct MediaInfoHandle {
    void *vtablePtr;
    uint32_t mediaFourcc;
    uint8_t sharedFlag;
};

/* Parser filter base (48 bytes, multiple inheritance: PipeProducerControl + ActiveFilterBase) */
class ParserFilterBase : public PipeProducerControl, public ActiveFilterBase {
public:
    ParserFilterBase(int workerKind, const char *threadName);
    virtual ~ParserFilterBase();

    virtual int onPipeConsumed(int frameFlags);
    virtual int onPipeUnderflow();
    virtual int onFrameConsumed(int frameFlags) = 0;
    virtual int runLoop();
    virtual int openStream() = 0;
    virtual int closeStream() = 0;
    virtual int produceFrame(MediaFrameBuffer *outBuf) = 0;

    void setOutputPipe(PipeInterface *pipe);

protected:
    PipeInterface *m_outputPipe; /* +44 */
};

/* Common audio media parser base (vtable 0x8153467C, 23 primary slots) */
class AudioParserBase : public ParserFilterBase {
public:
    AudioParserBase(int workerKind, const char *threadName);
    virtual ~AudioParserBase();

    virtual int findCachedMediaInfo(const char *path, MediaInfoHandle **outInfo) = 0;
    virtual int setMediaInfo(MediaInfoHandle *mediaInfo) = 0;
    virtual int prefetchMediaInfo(MediaInfoHandle *mediaInfo) = 0;
    virtual bool hasNextMedia() const = 0;
    virtual void *getCodecConfig(uint32_t *outSize) = 0;
    virtual void setPlaySpeed(int speed) = 0;
    virtual int setStartPosition(int startMs) = 0;
    virtual int setEndPosition(int endMs) = 0;
    virtual int getDurationMs() = 0;
    virtual int seekPosition(int positionMs) = 0;
    virtual int resetPosition() = 0;
    virtual int getPlaySpeed() const = 0;
    virtual int getStartPosition() const = 0;
    virtual int getEndPosition() const = 0;
};

/* Decoder filter base (56 bytes: PipeProducerControl + PipeConsumerControl + ActiveFilterBase) */
class DecoderFilterBase
    : public PipeProducerControl
    , public PipeConsumerControl
    , public ActiveFilterBase {
public:
    DecoderFilterBase(int workerKind, const char *threadName);
    virtual ~DecoderFilterBase();

    virtual int onPipeConsumed(int frameFlags);
    virtual int onPipeUnderflow();
    virtual int onPipeProduced();
    virtual int onPipeFull();
    virtual int onFrameConsumed(int frameFlags) = 0;
    virtual int runLoop();
    virtual int openDecoder() = 0;
    virtual int closeDecoder() = 0;
    virtual int decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf) = 0;

    void setInputPipe(PipeInterface *pipe);
    void setOutputPipe(PipeInterface *pipe);

protected:
    PipeInterface *m_inputPipe;  /* +48 */
    PipeInterface *m_outputPipe; /* +52 */
};

/* Audio decoder base (56 bytes, 0x81209AA8 - 0x81209B57) */
class AudioDecoderBase : public DecoderFilterBase {
public:
    AudioDecoderBase(int workerKind, const char *threadName);
    virtual ~AudioDecoderBase();

    virtual int onPipeConsumed(int frameFlags);
    virtual int onPipeUnderflow();
    virtual int onFrameConsumed(int frameFlags);
    virtual void configureDecoder(const void *configData, uint32_t configSize) = 0;
};

/* Audio output filter ("SceShellMusicCoreAIF", 92 bytes: FilterBase + PipeInterface) */
class AudioOutputFilter : public FilterBase, public PipeInterface {
public:
    AudioOutputFilter(uint32_t grainBytes, int slotCount, PipeProducerControl *producer,
                      AudioOpenCallback openCb, AudioCloseCallback closeCb,
                      AudioWriteCallback writeCb, AudioFlushCallback flushCb,
                      void *userContext);
    virtual ~AudioOutputFilter();

    virtual void setEventListener(PipelineEventListener *listener);
    virtual bool isInitialized() const;
    virtual int start();
    virtual int stop();

    virtual void setPositionListener(PositionChangeListener *listener);
    virtual int getPlayPositionMs() const;
    virtual void setPlayPositionMs(int posMs);
    virtual void reset();
    virtual bool hasQueuedFrame() const;
    virtual bool hasReadableFrame() const;
    virtual int acquireWriteSlot(MediaFrameBuffer **outBuf);
    virtual int commitWriteSlot(MediaFrameBuffer *buf);
    virtual int acquireReadSlot(MediaFrameBuffer **outBuf);
    virtual int releaseReadSlot(MediaFrameBuffer *buf);

    int handleBufferConsumed(void *pcmBuffer);

private:
    PipelineEventListener *m_eventListener; /* +8 */
    uint32_t m_grainBytes;                  /* +12 */
    int m_slotCount;                        /* +16 */
    void *m_pcmPool;                        /* +20 */
    MediaFrameBuffer *m_slots;              /* +24 */
    void *m_silenceBuf;                     /* +28 */
    int m_readIndex;                        /* +32 */
    int m_writeIndex;                       /* +36 */
    int m_inflightAudioCount;               /* +40 */
    int m_inflightSilenceCount;             /* +44 */
    PipeProducerControl *m_producer;        /* +48 */
    void *m_unusedConsumer;                 /* +52 */
    PositionChangeListener *m_posListener;  /* +56 */
    int m_playPositionMs;                   /* +60 */
    SceUID m_mutexId;                       /* +64 */
    uint8_t m_needsPrimeSilence;            /* +68 */
    uint8_t m_padPrime[3];                  /* +69..+71 */
    AudioOpenCallback m_openCb;             /* +72 */
    AudioCloseCallback m_closeCb;           /* +76 */
    AudioWriteCallback m_writeCb;           /* +80 */
    AudioFlushCallback m_flushCb;           /* +84 */
    void *m_userContext;                    /* +88 */
};

/* Pipeline component factories (0x81207758 - 0x8120784F) */
extern PipeInterface *createFixedFramePipe(uint32_t frameBytes, int slotCount,
                                           PipeProducerControl *producer,
                                           PipeConsumerControl *consumer);
extern PipeInterface *createBufRefPipe(int slotCount,
                                       PipeProducerControl *producer,
                                       PipeConsumerControl *consumer);
extern AudioOutputFilter *createAudioOutputFilter(uint32_t grainBytes, int slotCount,
                                                  PipeProducerControl *producer,
                                                  AudioOpenCallback openCb,
                                                  AudioCloseCallback closeCb,
                                                  AudioWriteCallback writeCb,
                                                  AudioFlushCallback flushCb,
                                                  void *userContext);
extern PipelineEventListener *createDecoderPipeline(ParserFilterBase *parser,
                                                    PipeInterface *pipe,
                                                    DecoderFilterBase *decoder,
                                                    AudioOutputFilter *audioSink);

/* DecoderPipeline helpers (musiccore_decoder_pipeline.cpp) */
extern int decoderPipelineIsReady(const PipelineEventListener *pipeline);
extern ParserFilterBase *decoderPipelineGetParser(PipelineEventListener *pipeline);
extern DecoderFilterBase *decoderPipelineGetDecoder(PipelineEventListener *pipeline);
extern AudioOutputFilter *decoderPipelineGetAudio(PipelineEventListener *pipeline);
extern PipeInterface *decoderPipelineGetPipe(PipelineEventListener *pipeline);

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_PIPELINE_TYPES_H */
