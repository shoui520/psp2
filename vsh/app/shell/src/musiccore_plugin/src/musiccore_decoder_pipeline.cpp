/*
 * SceShell musiccore_plugin - ActiveFilterListener and DecoderPipeline
 * (asynchronous parser/decoder/audio-output filter orchestration FSM).
 * Original address range (3.65-CEX): 0x811F60E4 - 0x811F7603
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_decoder_manager.h"
#include "musiccore_decoder_pipeline.h"
#include "musiccore_heap.h"
#include "musiccore_sync.h"

namespace sce {
namespace MusicCore {

/* Pipeline stage completion masks (0x81545000, 0x81545002, 0x81545004) */
static uint16_t g_runningStageMask = 0x0110u;
static uint16_t g_eosStageMask = 0x0220u;
static uint16_t g_startedStageMask = 0x0440u;

static const int PIPELINE_CMD_NONE = 0x10000;
static const int PIPELINE_CMD_START = 0x10001;
static const int PIPELINE_CMD_STOP = 0x10002;
static const int PIPELINE_CMD_QUERY_META = 0x10003;
static const int PIPELINE_CMD_SET_SPEED = 0x10004;
static const int PIPELINE_CMD_GET_POSITION = 0x10005;
static const int PIPELINE_CMD_SHUTDOWN = -917505; /* 0xFFF1FFFF */

static const uint32_t PIPELINE_EVT_NONE = 0x20000u;
static const uint32_t PIPELINE_EVT_ALL_RUNNING = 0x20001u;
static const uint32_t PIPELINE_EVT_ALL_STOPPED = 0x20002u;
static const uint32_t PIPELINE_EVT_ALL_EOS = 0x20003u;
static const uint32_t PIPELINE_EVT_PARSER_EOS = 0x20005u;
static const uint32_t PIPELINE_EVT_ERROR = 0xFFFFFFFFu;

static const int STAGE_STATE_IDLE = 1;
static const int STAGE_STATE_RUNNING = 2;
static const int STAGE_STATE_SPEED_STOPPING = 65;
static const int STAGE_STATE_SPEED_STARTING = 66;
static const int STAGE_STATE_STARTING = 272;
static const int STAGE_STATE_EOS_STOPPING = 512;
static const int STAGE_STATE_CMD_STOPPING = 544;
static const int STAGE_STATE_SPEED_WAIT_STOP = 576;
static const int STAGE_STATE_ERR_FROM_RUNNING = 4608;
static const int STAGE_STATE_ERR_FROM_STARTING = 4624;
static const int STAGE_STATE_ERR_FROM_STOPPING = 4640;
static const int STAGE_STATE_ERR_FROM_SPEED = 4672;

static const int PIPELINE_ERR_BAD_STATE = 33;
static const int PIPELINE_ERR_NOT_READY = 49;
static const int PIPELINE_ERR_QUEUE_FULL = 66;
static const int PIPELINE_ERR_QUEUE_EMPTY = 67;
static const int PIPELINE_ERR_BAD_PARAM = 241;

class ActiveFilterControl {
public:
    virtual ~ActiveFilterControl() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
};

class PipeProducerBase {
public:
    virtual ~PipeProducerBase() {}
};

class PipeConsumerBase {
public:
    virtual ~PipeConsumerBase() {}
};

class ParserFilter : public PipeProducerBase, public ActiveFilterControl {
public:
    virtual ~ParserFilter() {}
    virtual int acquireOutputBuffer() = 0;
    virtual int commitOutputBuffer() = 0;
    virtual int queryTagMetadata(void *tagOut) = 0;
    virtual int flushOutputPipe() = 0;
    virtual int onStart() = 0;
    virtual int onStop() = 0;
    virtual int readFrame(void *outPkt) = 0;
    virtual int findCachedMediaInfo(const char *path, void **outInfo) = 0;
    virtual int setSourceItem(void *mediaItem) = 0;
    virtual int queryMetadata(void *metaOut) = 0;
    virtual int isEndOfStream() = 0;
    virtual void *getCodecConfig(uint32_t *outSize) = 0;
    virtual int setPlaySpeed(int speed) = 0;
    virtual int setStartPosition(int startMs) = 0;
    virtual int setEndPosition(int endMs) = 0;
    virtual int getDurationMs() = 0;
    virtual int seekPosition(int positionMs) = 0;
    virtual int resetPosition() = 0;
    virtual int getCurrentSpeed() = 0;
    virtual int getForwardPositionMs() = 0;
    virtual int getReversePositionMs() = 0;

    ActiveFilterControl *asActiveControl() { return static_cast<ActiveFilterControl *>(this); }
};

class DecoderFilter
    : public PipeProducerBase
    , public PipeConsumerBase
    , public ActiveFilterControl {
public:
    virtual ~DecoderFilter() {}
    virtual int openDecoder() = 0;
    virtual int closeDecoder() = 0;
    virtual int decodePacket(void *inputPacket, void *outputPcm) = 0;
    virtual int flushDecoder() = 0;
    virtual int resetDecoder() = 0;
    virtual int setInputPipe(void *pipe) = 0;
    virtual int setOutputSink(void *sink) = 0;
    virtual int setActiveMode(int mode) = 0;
    virtual int getCodecType() = 0;
    virtual int setCodecConfig(void *configData, uint32_t configSize) = 0;

    ActiveFilterControl *asActiveControl() { return static_cast<ActiveFilterControl *>(this); }
};

class BufRefPipe {
public:
    virtual ~BufRefPipe() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
    virtual int acquireWriteBuffer(void **outBuf) = 0;
    virtual int commitWriteBuffer(void *buf) = 0;
    virtual int clearBuffers() = 0;
    virtual int hasPendingData() = 0;
};

class AudioOutputSink {
public:
    virtual ~AudioOutputSink() {}
    virtual void setEventListener(void *listener) = 0;
    virtual int isReady() = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
    virtual void setListener(AudioOutputListener *listener) = 0;
    virtual int getPlaybackPositionMs() = 0;
    virtual void setResumePositionMs(int positionMs) = 0;
    virtual void flushRingBuffer() = 0;
    virtual int hasBufferedAudio() = 0;
};

class PipelineEventCallback {
public:
    virtual ~PipelineEventCallback() {}
    virtual void onParserEos() = 0;                            /* +0x08 (8) */
    virtual void onStopped(int positionMs, int errCode) = 0;   /* +0x0C (12) */
    virtual void onProgress(int positionMs, int speedMode) = 0;/* +0x10 (16) */
    virtual void onStarted(int errCode, int durationMs) = 0;   /* +0x14 (20) */
    virtual void onStopComplete(int errCode) = 0;              /* +0x18 (24) */
    virtual void onMetaComplete(int errCode) = 0;              /* +0x1C (28) */
    virtual void onSpeedComplete(int errCode) = 0;             /* +0x20 (32) */
    virtual void onPositionComplete(int positionMs, int errCode) = 0; /* +0x24 (36) */
};

/* 0x811F64B6 */
int PipelineEventQueue::push(uint32_t eventCode, int statusCode,
                             uint16_t sourceStage, uint16_t flagsSnapshot)
{
    if (tailIndex < 0) {
        return PIPELINE_ERR_QUEUE_FULL;
    }

    PipelineEventEntry &slot = entries[tailIndex];
    slot.eventCode = eventCode;
    slot.statusCode = statusCode;
    slot.sourceStage = sourceStage;
    slot.stageFlagsSnapshot = flagsSnapshot;

    if (headIndex < 0) {
        headIndex = tailIndex;
    }

    if (tailIndex + 1 == capacity) {
        tailIndex = (headIndex != 0) ? 0 : -1;
    } else if (tailIndex + 1 == headIndex) {
        tailIndex = -1;
    } else {
        tailIndex++;
    }
    return 0;
}

/* 0x811F6D94 */
int PipelineEventQueue::pop(PipelineEventEntry *outEntry)
{
    if (headIndex < 0) {
        return PIPELINE_ERR_QUEUE_EMPTY;
    }

    PipelineEventEntry &slot = entries[headIndex];
    *outEntry = slot;

    slot.eventCode = PIPELINE_EVT_NONE;
    slot.statusCode = -1;
    slot.sourceStage = 0;
    slot.stageFlagsSnapshot = 0xFFFFu;

    if (tailIndex < 0) {
        tailIndex = headIndex;
    }

    if (headIndex + 1 == capacity) {
        headIndex = (tailIndex != 0) ? 0 : -1;
    } else if (headIndex + 1 == tailIndex) {
        headIndex = -1;
    } else {
        headIndex++;
    }
    return 0;
}

/* 0x811F714C */
int DecoderPipeline::workerThreadEntry(SceSize argSize, void *argData)
{
    (void)argSize;
    DecoderPipeline **selfPtr = (DecoderPipeline **)argData;
    if (selfPtr == NULL) {
        return -1;
    }
    (*selfPtr)->runEventLoop();
    exitWorkerThread();
    return 1;
}

/* 0x811F6108 */
DecoderPipeline::DecoderPipeline(ParserFilter *parser, BufRefPipe *pipe,
                                 DecoderFilter *decoder, AudioOutputSink *audioSink)
    : m_parser(parser)
    , m_pipe(pipe)
    , m_decoder(decoder)
    , m_audioSink(audioSink)
    , m_audioControl(audioSink)
    , m_callback(NULL)
    , m_state(STAGE_STATE_IDLE)
    , m_workerThreadId(-1)
    , m_mutex("SceShellMusicCoreDecoderMgr")
    , m_cond("SceShellMusicCoreDecoderMgr")
    , m_stageFlags(0)
    , m_padStageFlags(0)
    , m_pendingCommand(PIPELINE_CMD_NONE)
    , m_startMediaItem(NULL)
    , m_startPositionMs(0)
    , m_endPositionMs(0)
    , m_startPlaySpeed(0)
    , m_pendingSpeed(0)
    , m_pendingMetaOut(NULL)
    , m_reservedStateWord(0)
    , m_padReservedWord(0)
    , m_pendingError(-1)
    , m_fallbackPositionMs(-1)
    , m_trackDurationMs(0)
    , m_threadArg(NULL)
    , m_usesThreadPool(1)
{
    m_eventQueue.entries = m_eventStorage;
    m_eventQueue.capacity = 16;
    m_eventQueue.headIndex = -1;
    m_eventQueue.tailIndex = 0;

    if (m_audioSink != NULL) {
        m_audioSink->setListener(this);
    }

    if (m_decoder != NULL) {
        g_runningStageMask |= 0x10u;
        g_eosStageMask |= 0x20u;
        g_startedStageMask |= 0x40u;
    } else {
        g_runningStageMask &= (uint16_t)~0x10u;
        g_eosStageMask &= (uint16_t)~0x20u;
        g_startedStageMask &= (uint16_t)~0x40u;
    }

    m_threadArg = (DecoderPipeline **)localAlloc(sizeof(DecoderPipeline *));
    if (m_threadArg != NULL) {
        *m_threadArg = this;
        if (WorkerThreadPool::getInstance()->startWorker(
                3, reinterpret_cast<int (*)(void *, void *)>(&DecoderPipeline::workerThreadEntry),
                m_threadArg, NULL, &m_workerThreadId) != 0) {
            m_workerThreadId = -1;
            delete m_threadArg;
            m_threadArg = NULL;
        }
    }
}

/* 0x811F6272 / 0x811F62FA */
DecoderPipeline::~DecoderPipeline()
{
    m_mutex.lock();
    m_state = -1;
    m_pendingCommand = PIPELINE_CMD_SHUTDOWN;
    m_cond.signal(0xFFFFFFFFu);
    m_mutex.unlock();

    if (m_usesThreadPool == 1u) {
        WorkerThreadPool::getInstance()->joinWorker(m_workerThreadId);
    } else {
        joinWorkerThread(m_workerThreadId);
    }

    if (m_threadArg != NULL) {
        delete m_threadArg;
        m_threadArg = NULL;
    }
}

/* 0x811F630A */
bool DecoderPipeline::isReady() const
{
    if (m_workerThreadId < 0) {
        return false;
    }
    if (!m_mutex.isValid()) {
        return false;
    }
    return m_cond.isValid();
}

void DecoderPipeline::stopAllStages()
{
    m_audioControl->stop();
    if (m_decoder != NULL) {
        m_decoder->asActiveControl()->stop();
    }
    m_parser->asActiveControl()->stop();
}

/* 0x811F6336 */
void DecoderPipeline::handleCommandInEosStopping(int command)
{
    switch (command) {
    case PIPELINE_CMD_START:
        if (m_callback != NULL) {
            m_callback->onStarted(PIPELINE_ERR_BAD_STATE, -1);
        }
        break;
    case PIPELINE_CMD_STOP:
        m_state = STAGE_STATE_CMD_STOPPING;
        break;
    case PIPELINE_CMD_QUERY_META:
        m_parser->setSourceItem(m_pendingMetaOut);
        if (m_callback != NULL) {
            m_callback->onMetaComplete(0);
        }
        break;
    case PIPELINE_CMD_SET_SPEED:
        m_state = STAGE_STATE_SPEED_WAIT_STOP;
        break;
    case PIPELINE_CMD_GET_POSITION:
        if (m_callback != NULL) {
            m_callback->onPositionComplete(-1, PIPELINE_ERR_BAD_STATE);
        }
        break;
    default:
        break;
    }
}

/* 0x811F639C */
void DecoderPipeline::handleCommandInRunning(int command)
{
    switch (command) {
    case PIPELINE_CMD_START:
        if (m_callback != NULL) {
            m_callback->onStarted(PIPELINE_ERR_BAD_STATE, -1);
        }
        break;
    case PIPELINE_CMD_STOP:
        stopAllStages();
        m_state = STAGE_STATE_CMD_STOPPING;
        break;
    case PIPELINE_CMD_QUERY_META: {
        int metaErr = m_parser->queryMetadata(m_pendingMetaOut);
        if (m_callback != NULL) {
            m_callback->onMetaComplete(metaErr);
        }
        break;
    }
    case PIPELINE_CMD_SET_SPEED:
        stopAllStages();
        m_state = STAGE_STATE_SPEED_STOPPING;
        break;
    case PIPELINE_CMD_GET_POSITION: {
        if (m_callback == NULL) {
            break;
        }
        int positionMs = m_audioSink->getPlaybackPositionMs();
        if (positionMs < 0) {
            int speed = m_parser->getCurrentSpeed();
            if (speed > 0) {
                positionMs = m_parser->getForwardPositionMs();
                if (positionMs < 0) {
                    positionMs = 0;
                }
            } else if (speed < 0) {
                positionMs = m_parser->getReversePositionMs();
                if (positionMs < 0) {
                    positionMs = m_parser->getDurationMs();
                    if (positionMs < 0) {
                        m_callback->onPositionComplete(-1, -1);
                        break;
                    }
                }
            } else {
                m_callback->onPositionComplete(-1, -1);
                break;
            }
        }
        m_callback->onPositionComplete(positionMs, 0);
        break;
    }
    default:
        break;
    }
}

/* 0x811F654E */
void DecoderPipeline::handleCommandInIdle(int command)
{
    switch (command) {
    case PIPELINE_CMD_START: {
        int err = m_parser->setSourceItem(m_startMediaItem);
        if (err != 0) {
            if (m_callback != NULL) {
                m_callback->onStarted(err, -1);
            }
            return;
        }

        m_parser->setPlaySpeed(m_startPlaySpeed);
        m_parser->setStartPosition(m_startPositionMs);
        m_parser->setEndPosition(m_endPositionMs);

        int durationMs = m_parser->getDurationMs();
        m_trackDurationMs = durationMs;
        if (durationMs < 0 && durationMs != -2) {
            if (m_callback != NULL) {
                m_callback->onStarted(durationMs, -1);
            }
            return;
        }

        if (m_startPlaySpeed > 0) {
            m_fallbackPositionMs = (m_startPositionMs >= 0) ? m_startPositionMs : 0;
        } else if (m_startPlaySpeed < 0) {
            m_fallbackPositionMs = (m_endPositionMs >= 0) ? m_endPositionMs : durationMs;
        }

        uint32_t configSize = 0;
        if (m_decoder != NULL) {
            void *configData = m_parser->getCodecConfig(&configSize);
            m_decoder->setCodecConfig(configData, configSize);
        }

        int parserErr = m_parser->asActiveControl()->start();
        int decoderErr = 0;
        if (m_decoder != NULL) {
            decoderErr = m_decoder->asActiveControl()->start();
        }
        int audioErr = m_audioControl->start();

        uint16_t startedBits = 0;
        if (parserErr == 0) {
            startedBits |= 0x0400u;
        }
        if (decoderErr == 0) {
            startedBits |= 0x0040u;
        }
        if (audioErr == 0) {
            startedBits |= 0x0004u;
        }

        if ((g_startedStageMask & startedBits) != 0u) {
            if ((g_startedStageMask & startedBits) != g_startedStageMask) {
                m_mutex.lock();
                uint16_t failedStage = 3u;
                int failedErr = audioErr;
                if (parserErr != 0) {
                    failedStage = 1u;
                    failedErr = parserErr;
                } else if (decoderErr != 0) {
                    failedStage = 2u;
                    failedErr = decoderErr;
                }
                m_eventQueue.push(PIPELINE_EVT_ERROR, failedErr, failedStage, m_stageFlags);
                m_mutex.unlock();
            }
            m_state = STAGE_STATE_STARTING;
        } else {
            if ((startedBits & 0x0004u) != 0u) {
                m_audioControl->stop();
            }
            if (m_callback != NULL) {
                m_callback->onStarted(parserErr, -1);
            }
        }
        break;
    }
    case PIPELINE_CMD_STOP:
        if (m_pipe != NULL) {
            m_pipe->clearBuffers();
        }
        if (m_audioSink != NULL) {
            m_audioSink->flushRingBuffer();
        }
        if (m_callback != NULL) {
            m_callback->onStopComplete(0);
        }
        break;
    case PIPELINE_CMD_QUERY_META:
        if (m_callback != NULL) {
            m_callback->onMetaComplete(PIPELINE_ERR_BAD_STATE);
        }
        break;
    case PIPELINE_CMD_SET_SPEED:
        if (m_callback != NULL) {
            m_callback->onSpeedComplete(PIPELINE_ERR_NOT_READY);
        }
        break;
    case PIPELINE_CMD_GET_POSITION:
        if (m_callback != NULL) {
            m_callback->onPositionComplete(-1, PIPELINE_ERR_BAD_STATE);
        }
        break;
    default:
        break;
    }
}

/* 0x811F67BC */
void DecoderPipeline::handleEventInErrorStopping(uint32_t eventCode, int statusCode, uint16_t sourceStage)
{
    (void)statusCode;
    (void)sourceStage;
    if (eventCode != PIPELINE_EVT_ALL_STOPPED) {
        return;
    }

    int positionMs = m_audioSink->getPlaybackPositionMs();
    if (positionMs < 0) {
        positionMs = m_fallbackPositionMs;
    }

    if (m_pipe != NULL) {
        m_pipe->clearBuffers();
    }
    if (m_audioSink != NULL) {
        m_audioSink->flushRingBuffer();
    }

    if (m_callback != NULL) {
        switch (m_state) {
        case STAGE_STATE_ERR_FROM_SPEED:
            m_callback->onStopped(positionMs, m_pendingError);
            m_callback->onSpeedComplete(m_pendingError);
            break;
        case STAGE_STATE_ERR_FROM_STOPPING:
            m_callback->onStopped(positionMs, m_pendingError);
            m_callback->onStopComplete(m_pendingError);
            break;
        case STAGE_STATE_ERR_FROM_STARTING:
            m_callback->onStarted(m_pendingError, -1);
            break;
        case STAGE_STATE_ERR_FROM_RUNNING:
            m_callback->onStopped(positionMs, m_pendingError);
            break;
        default:
            break;
        }
    }

    m_state = STAGE_STATE_IDLE;
    m_pendingError = -1;
}

/* 0x811F68A6 */
void DecoderPipeline::handleEventInSpeedChange(uint32_t eventCode, int statusCode, uint16_t sourceStage)
{
    (void)sourceStage;
    if (eventCode == PIPELINE_EVT_ERROR) {
        if (m_state == STAGE_STATE_SPEED_STARTING) {
            stopAllStages();
        }
        m_pendingError = statusCode;
        m_state = STAGE_STATE_ERR_FROM_SPEED;
        return;
    }

    switch (eventCode) {
    case PIPELINE_EVT_ALL_RUNNING:
        if (m_state == STAGE_STATE_SPEED_STARTING) {
            m_state = STAGE_STATE_RUNNING;
            if (m_callback != NULL) {
                m_callback->onSpeedComplete(0);
            }
        }
        break;
    case PIPELINE_EVT_ALL_STOPPED:
        if (m_state == STAGE_STATE_SPEED_STOPPING) {
            int positionMs = m_audioSink->getPlaybackPositionMs();
            if (m_pipe != NULL) {
                m_pipe->clearBuffers();
            }
            if (m_audioSink != NULL) {
                m_audioSink->flushRingBuffer();
                m_audioSink->setResumePositionMs(positionMs);
            }
            if (positionMs < 0) {
                positionMs = m_fallbackPositionMs;
            }

            m_parser->seekPosition(positionMs);
            m_parser->setPlaySpeed(m_pendingSpeed);

            int audioErr = m_audioControl->start();
            int decoderErr = 0;
            if (m_decoder != NULL) {
                decoderErr = m_decoder->asActiveControl()->start();
            }
            int parserErr = m_parser->asActiveControl()->start();

            uint16_t startedBits = 0;
            if (parserErr == 0) {
                startedBits |= 0x0004u;
            }
            if (decoderErr == 0) {
                startedBits |= 0x0040u;
            }
            if (audioErr == 0) {
                startedBits |= 0x0400u;
            }

            if ((g_startedStageMask & startedBits) != 0u) {
                if ((g_startedStageMask & startedBits) != g_startedStageMask) {
                    m_mutex.lock();
                    uint16_t failedStage = 1u;
                    int failedErr = audioErr;
                    if (parserErr != 0) {
                        failedStage = 3u;
                        failedErr = parserErr;
                    } else if (decoderErr != 0) {
                        failedStage = 2u;
                        failedErr = decoderErr;
                    }
                    m_eventQueue.push(PIPELINE_EVT_ERROR, failedErr, failedStage, m_stageFlags);
                    m_mutex.unlock();
                }
                m_state = STAGE_STATE_SPEED_STARTING;
            } else {
                if ((startedBits & 0x0004u) != 0u) {
                    m_audioControl->stop();
                }
                if (m_callback != NULL) {
                    m_callback->onStopped(positionMs, audioErr);
                    m_callback->onSpeedComplete(audioErr);
                }
                m_state = STAGE_STATE_IDLE;
            }
        }
        break;
    case PIPELINE_EVT_ALL_EOS:
        if (m_state == STAGE_STATE_SPEED_STOPPING) {
            m_state = STAGE_STATE_SPEED_WAIT_STOP;
        }
        break;
    default:
        break;
    }
}

/* 0x811F6AA4 */
void DecoderPipeline::handleEventInStarting(uint32_t eventCode, int statusCode, uint16_t sourceStage)
{
    (void)sourceStage;
    if (eventCode == PIPELINE_EVT_ERROR) {
        stopAllStages();
        m_pendingError = statusCode;
        m_state = STAGE_STATE_ERR_FROM_STARTING;
        return;
    }

    if (eventCode == PIPELINE_EVT_ALL_RUNNING) {
        m_state = STAGE_STATE_RUNNING;
        if (m_callback != NULL) {
            m_callback->onStarted(0, m_trackDurationMs);
        }
    }
}

/* 0x811F6B2E */
void DecoderPipeline::handleEventInStopping(uint32_t eventCode, int statusCode, uint16_t sourceStage)
{
    (void)sourceStage;
    if (eventCode == PIPELINE_EVT_ERROR) {
        m_pendingError = statusCode;
        switch (m_state) {
        case STAGE_STATE_SPEED_WAIT_STOP:
            m_state = STAGE_STATE_ERR_FROM_SPEED;
            break;
        case STAGE_STATE_CMD_STOPPING:
            m_state = STAGE_STATE_ERR_FROM_STOPPING;
            break;
        case STAGE_STATE_EOS_STOPPING:
            m_state = STAGE_STATE_ERR_FROM_RUNNING;
            break;
        default:
            break;
        }
        return;
    }

    if (eventCode != PIPELINE_EVT_ALL_STOPPED) {
        return;
    }

    int positionMs = m_audioSink->getPlaybackPositionMs();
    if (positionMs < 0) {
        positionMs = m_fallbackPositionMs;
    }

    if (m_callback != NULL) {
        switch (m_state) {
        case STAGE_STATE_ERR_FROM_STOPPING:
            if (m_pipe != NULL) {
                m_pipe->clearBuffers();
            }
            if (m_audioSink != NULL) {
                m_audioSink->flushRingBuffer();
            }
            m_callback->onStopped(positionMs, m_pendingError);
            m_callback->onStopComplete(m_pendingError);
            break;
        case STAGE_STATE_SPEED_WAIT_STOP:
            if (m_pipe != NULL) {
                m_pipe->clearBuffers();
            }
            if (m_audioSink != NULL) {
                m_audioSink->flushRingBuffer();
            }
            m_callback->onStopped(positionMs, 0);
            m_callback->onSpeedComplete(PIPELINE_ERR_BAD_STATE);
            break;
        case STAGE_STATE_CMD_STOPPING:
            if (m_pipe != NULL) {
                m_pipe->clearBuffers();
            }
            if (m_audioSink != NULL) {
                m_audioSink->flushRingBuffer();
            }
            m_callback->onStopped(positionMs, 0);
            m_callback->onStopComplete(0);
            break;
        case STAGE_STATE_EOS_STOPPING:
            if (m_audioSink != NULL) {
                m_audioSink->setResumePositionMs(-1);
            }
            m_callback->onStopped(positionMs, 0);
            break;
        default:
            break;
        }
    }

    m_state = STAGE_STATE_IDLE;
}

/* 0x811F6CA2 */
void DecoderPipeline::handleEventInRunning(uint32_t eventCode, int statusCode, uint16_t sourceStage)
{
    (void)sourceStage;
    if (eventCode == PIPELINE_EVT_ERROR) {
        stopAllStages();
        m_pendingError = statusCode;
        m_state = STAGE_STATE_ERR_FROM_RUNNING;
        return;
    }

    if (eventCode == PIPELINE_EVT_ALL_EOS) {
        bool hasRemaining = false;
        if (m_pipe != NULL) {
            hasRemaining = (m_pipe->hasPendingData() != 0);
        }
        if (m_parser != NULL && m_parser->isEndOfStream() == 0 && m_audioSink != NULL) {
            if ((m_audioSink->hasBufferedAudio() != 0) || hasRemaining) {
                hasRemaining = true;
            }
        }
        if (!hasRemaining) {
            m_state = STAGE_STATE_EOS_STOPPING;
            if (m_parser != NULL && m_parser->isEndOfStream() == 0) {
                m_audioControl->stop();
            }
            if (m_decoder != NULL) {
                m_decoder->asActiveControl()->stop();
            }
            m_parser->asActiveControl()->stop();
        }
    } else if (eventCode == PIPELINE_EVT_PARSER_EOS) {
        if (m_callback != NULL) {
            m_callback->onParserEos();
        }
    }
}

void DecoderPipeline::rejectCommand(int command)
{
    if (m_callback == NULL) {
        return;
    }
    switch (command) {
    case PIPELINE_CMD_START:
        m_callback->onStarted(PIPELINE_ERR_BAD_STATE, -1);
        break;
    case PIPELINE_CMD_STOP:
        m_callback->onStopComplete(PIPELINE_ERR_BAD_STATE);
        break;
    case PIPELINE_CMD_SET_SPEED:
        m_callback->onSpeedComplete(PIPELINE_ERR_BAD_STATE);
        break;
    case PIPELINE_CMD_GET_POSITION:
        m_callback->onPositionComplete(-1, PIPELINE_ERR_BAD_STATE);
        break;
    default:
        break;
    }
}

/* 0x811F6E4C */
void DecoderPipeline::runEventLoop()
{
    m_mutex.lock();

    while (m_pendingCommand != PIPELINE_CMD_SHUTDOWN) {
        PipelineEventEntry evt;
        if (m_eventQueue.pop(&evt) == 0) {
            int curState = m_state;
            m_mutex.unlock();

            if (curState == STAGE_STATE_ERR_FROM_SPEED
                || curState == STAGE_STATE_ERR_FROM_STOPPING
                || curState == STAGE_STATE_ERR_FROM_STARTING
                || curState == STAGE_STATE_ERR_FROM_RUNNING) {
                handleEventInErrorStopping(evt.eventCode, evt.statusCode, evt.sourceStage);
            } else if (curState == STAGE_STATE_EOS_STOPPING
                       || curState == STAGE_STATE_CMD_STOPPING
                       || curState == STAGE_STATE_SPEED_WAIT_STOP) {
                handleEventInStopping(evt.eventCode, evt.statusCode, evt.sourceStage);
            } else if (curState == STAGE_STATE_STARTING) {
                handleEventInStarting(evt.eventCode, evt.statusCode, evt.sourceStage);
            } else if (curState == STAGE_STATE_RUNNING) {
                handleEventInRunning(evt.eventCode, evt.statusCode, evt.sourceStage);
            } else if (curState == STAGE_STATE_SPEED_STOPPING
                       || curState == STAGE_STATE_SPEED_STARTING) {
                handleEventInSpeedChange(evt.eventCode, evt.statusCode, evt.sourceStage);
            }

            m_mutex.lock();
        } else if (m_pendingCommand == PIPELINE_CMD_NONE) {
            m_cond.wait(&m_mutex, 3u, NULL);
        } else {
            int command = m_pendingCommand;
            int curState = m_state;
            m_pendingCommand = PIPELINE_CMD_NONE;
            m_mutex.unlock();

            if (curState == STAGE_STATE_IDLE) {
                handleCommandInIdle(command);
            } else if (curState == STAGE_STATE_RUNNING) {
                handleCommandInRunning(command);
            } else if (curState == STAGE_STATE_EOS_STOPPING) {
                handleCommandInEosStopping(command);
            } else {
                rejectCommand(command);
            }

            m_mutex.lock();
        }
    }

    m_mutex.unlock();
}

/* 0x811F7164 */
int DecoderPipeline::requestStart(void *mediaItem, int startMs, int endMs, int playSpeed)
{
    if (mediaItem == NULL || playSpeed == 0 || playSpeed <= -200 || playSpeed >= 200) {
        return PIPELINE_ERR_BAD_PARAM;
    }

    m_mutex.lock();
    m_startMediaItem = mediaItem;
    m_startPositionMs = startMs;
    m_endPositionMs = endMs;
    m_startPlaySpeed = playSpeed;
    m_pendingCommand = PIPELINE_CMD_START;
    m_cond.signal(1u);
    m_mutex.unlock();
    return 0;
}

/* 0x811F71C6 */
int DecoderPipeline::requestStop()
{
    m_mutex.lock();
    m_pendingCommand = PIPELINE_CMD_STOP;
    m_cond.signal(1u);
    m_mutex.unlock();
    return 0;
}

/* 0x811F71F0 */
int DecoderPipeline::requestSetSpeed(int playSpeed)
{
    if (playSpeed == 0 || playSpeed <= -200 || playSpeed >= 200) {
        return PIPELINE_ERR_BAD_PARAM;
    }

    m_mutex.lock();
    m_pendingSpeed = playSpeed;
    m_pendingCommand = PIPELINE_CMD_SET_SPEED;
    m_cond.signal(1u);
    m_mutex.unlock();
    return 0;
}

/* 0x811F7234 */
int DecoderPipeline::requestQueryMetadata(void *metaOut)
{
    if (metaOut == NULL) {
        return PIPELINE_ERR_BAD_PARAM;
    }

    m_mutex.lock();
    m_pendingMetaOut = metaOut;
    m_pendingCommand = PIPELINE_CMD_QUERY_META;
    m_cond.signal(1u);
    m_mutex.unlock();
    return 0;
}

/* 0x811F7268 */
int DecoderPipeline::requestGetPosition()
{
    m_mutex.lock();
    m_pendingCommand = PIPELINE_CMD_GET_POSITION;
    m_cond.signal(1u);
    m_mutex.unlock();
    return 0;
}

/* 0x811F7292 */
void DecoderPipeline::onFilterEvent(int eventKind, void *sourceFilter, int statusCode)
{
    if (eventKind == 5) {
        return;
    }

    void *parserCtrl = (m_parser != NULL) ? (void *)m_parser->asActiveControl() : NULL;
    void *decoderCtrl = (m_decoder != NULL) ? (void *)m_decoder->asActiveControl() : NULL;
    void *audioCtrl = (void *)m_audioControl;

    m_mutex.lock();
    uint16_t sourceStage = 0;
    bool signalCond = false;

    switch (eventKind) {
    case 1: /* Stage started */
        if (sourceFilter == parserCtrl) {
            sourceStage = 1u;
            m_stageFlags |= 0x0100u;
        } else if (sourceFilter == decoderCtrl) {
            sourceStage = 2u;
            m_stageFlags |= 0x0010u;
        } else if (sourceFilter == audioCtrl) {
            sourceStage = 3u;
            m_stageFlags |= 0x0001u;
        }
        if ((m_stageFlags & g_runningStageMask) == g_runningStageMask) {
            m_eventQueue.push(PIPELINE_EVT_ALL_RUNNING, statusCode, sourceStage, m_stageFlags);
            signalCond = true;
        }
        break;

    case 2: /* Stage stopped */
        if (sourceFilter == parserCtrl) {
            sourceStage = 1u;
            m_stageFlags &= (uint16_t)~0x0300u;
        } else if (sourceFilter == decoderCtrl) {
            sourceStage = 2u;
            m_stageFlags &= (uint16_t)~0x0030u;
        } else if (sourceFilter == audioCtrl) {
            sourceStage = 3u;
            m_stageFlags &= (uint16_t)~0x0003u;
        }
        if ((m_stageFlags & g_runningStageMask) == 0u) {
            m_eventQueue.push(PIPELINE_EVT_ALL_STOPPED, statusCode, sourceStage, m_stageFlags);
            signalCond = true;
        }
        break;

    case 3: { /* Stage reached EOS */
        bool parserEosPushed = false;
        if (sourceFilter == parserCtrl) {
            sourceStage = 1u;
            m_stageFlags |= 0x0200u;
            m_eventQueue.push(PIPELINE_EVT_PARSER_EOS, statusCode, 1u, m_stageFlags);
            parserEosPushed = true;
        } else if (sourceFilter == decoderCtrl) {
            sourceStage = 2u;
            m_stageFlags |= 0x0020u;
        } else if (sourceFilter == audioCtrl) {
            sourceStage = 3u;
            m_stageFlags |= 0x0002u;
        }
        if ((m_stageFlags & g_eosStageMask) == g_eosStageMask) {
            m_eventQueue.push(PIPELINE_EVT_ALL_EOS, statusCode, sourceStage, m_stageFlags);
            signalCond = true;
        } else if (parserEosPushed) {
            signalCond = true;
        }
        break;
    }

    case 4: /* Stage cleared EOS */
        if (sourceFilter == parserCtrl) {
            m_stageFlags &= (uint16_t)~0x0200u;
        } else if (sourceFilter == decoderCtrl) {
            m_stageFlags &= (uint16_t)~0x0020u;
        } else if (sourceFilter == audioCtrl) {
            m_stageFlags &= (uint16_t)~0x0002u;
        }
        break;

    case -1: /* Stage error */
        if (sourceFilter == parserCtrl) {
            sourceStage = 1u;
        } else if (sourceFilter == decoderCtrl) {
            sourceStage = 2u;
        } else if (sourceFilter == audioCtrl) {
            sourceStage = 3u;
        }
        m_eventQueue.push(PIPELINE_EVT_ERROR, statusCode, sourceStage, m_stageFlags);
        signalCond = true;
        break;

    default:
        break;
    }

    if (signalCond) {
        m_cond.signal(2u);
    }
    m_mutex.unlock();
}

/* 0x811F7594 (+ thunk at 0x811F75F6) */
void DecoderPipeline::onPlaybackProgress(int positionMs)
{
    if (m_callback == NULL) {
        return;
    }
    int speed = m_parser->getCurrentSpeed();
    if (speed == 0) {
        return;
    }

    int speedMode = 2;
    if (speed != 1) {
        speedMode = (speed > 0) ? 3 : 4;
    }
    m_callback->onProgress(positionMs, speedMode);
}

/* Forwarded C-style pipeline & decoder instance bridges */
struct PipelineEventListener;
} /* namespace MusicCore */
} /* namespace sce */
