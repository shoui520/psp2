/*
 * SceShell musiccore_plugin - ActiveFilterBase, DecoderFilterBase, ParserFilterBase,
 * FixedFramePipe, and pipeline factory helpers.
 * Original address range (3.65-CEX): 0x81206DF0 - 0x81207E73
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_codec_decoders.h"
#include "musiccore_decoder_pipeline.h"
#include "musiccore_pipeline_types.h"

#include <memory/heap_allocator.h>

extern "C" {
void *ScePafStdc_3CF19248(SceSize size, void *heap);
}

static inline void *musiccoreAllocFromPafHeap(SceSize size, void *heap)
{
	return operator new(size, static_cast<paf::memory::HeapAllocator *>(heap));
}

namespace sce {
namespace MusicCore {

static const int PIPE_ERR_INVALID_ARG = 241;
static const int PIPE_ERR_NO_MEMORY = 4;
static const int PIPE_ERR_FULL = 66;
static const int PIPE_ERR_EMPTY = 67;
static const int PIPE_ERR_OUT_OF_ORDER = 68;
static const int PARSER_ERR_UNDERFLOW = 5;

enum FilterRunState {
    FILTER_STATE_RUNNING = 1,
    FILTER_STATE_STOPPED = 2,
    FILTER_STATE_EXITING = 3
};

enum FilterEventCode {
    FILTER_EVENT_ERROR = -1,
    FILTER_EVENT_STARTED = 1,
    FILTER_EVENT_STOPPED = 2,
    FILTER_EVENT_STALLED = 3,
    FILTER_EVENT_RESUMED = 4
};

extern void *getWorkerThreadPool();
extern int workerPoolSpawn(void *pool, int workerKind, void *entryFn, void *arg, SceUID *outThreadId);
extern int workerPoolJoin(void *pool, SceUID threadId);

static int activeFilterThreadEntry(SceSize args, void *argp)
{
    (void)args;
    ActiveFilterBase **holder = (ActiveFilterBase **)argp;
    ActiveFilterBase *self = *holder;
    if (self == NULL)
        exitWorkerThread();
    self->workerThreadMain();
    exitWorkerThread();
    return 1;
}

/* ===== ActiveFilterBase (0x81206E14 - 0x81207087) ===== */

/* 0x81206E14 / 0x81206EA4 */
ActiveFilterBase::ActiveFilterBase(int workerKind, const char *threadName)
    : m_eventListener(NULL)
    , m_wakeEvent(threadName)
    , m_lock(threadName)
    , m_workerId(-1)
    , m_runState(FILTER_STATE_STOPPED)
    , m_workerSelfArg(NULL)
    , m_useWorkerPool(1)
{
    m_workerSelfArg = (ActiveFilterBase **)musiccoreAllocFromPafHeap(sizeof(ActiveFilterBase *), getLocalHeap());
    if (m_workerSelfArg != NULL) {
        *m_workerSelfArg = this;
        if (workerPoolSpawn(getWorkerThreadPool(), workerKind, (void *)activeFilterThreadEntry,
                            m_workerSelfArg, &m_workerId) != 0) {
            m_workerId = -1;
            localFree(m_workerSelfArg);
            m_workerSelfArg = NULL;
        }
    }
}

/* 0x81206EAC / 0x81206F1C / 0x81206F2C */
ActiveFilterBase::~ActiveFilterBase()
{
    m_lock.lock();
    m_runState = FILTER_STATE_EXITING;
    m_wakeEvent.signal(0xFFFFFFFFu);
    m_lock.unlock();

    if (m_useWorkerPool == 1)
        workerPoolJoin(getWorkerThreadPool(), m_workerId);
    else
        joinAndDeleteWorkerThread(m_workerId);

    if (m_workerSelfArg != NULL) {
        localFree(m_workerSelfArg);
        m_workerSelfArg = NULL;
    }
}

/* 0x81206F34 */
void ActiveFilterBase::setEventListener(PipelineEventListener *listener)
{
    m_eventListener = listener;
}

/* 0x81206F38 */
bool ActiveFilterBase::isInitialized() const
{
    if (m_workerId < 0)
        return false;
    if (!m_lock.isValid())
        return false;
    return m_wakeEvent.isValid();
}

/* 0x81206F64 */
int ActiveFilterBase::start()
{
    m_lock.lock();
    if (m_runState != FILTER_STATE_RUNNING) {
        m_runState = FILTER_STATE_RUNNING;
        m_wakeEvent.signal(1u);
    }
    m_lock.unlock();
    return 0;
}

/* 0x81206F9A */
int ActiveFilterBase::stop()
{
    m_lock.lock();
    if (m_runState != FILTER_STATE_STOPPED) {
        m_runState = FILTER_STATE_STOPPED;
        m_wakeEvent.signal(1u);
    }
    m_lock.unlock();
    return 0;
}

/* 0x8120707A */
bool ActiveFilterBase::isRunning() const
{
    return m_runState == FILTER_STATE_RUNNING;
}

/* 0x81206FD0 */
void ActiveFilterBase::workerThreadMain()
{
    int lastReportedState = FILTER_STATE_STOPPED;

    for (;;) {
        m_lock.lock();
        while (m_runState == FILTER_STATE_STOPPED) {
            if (lastReportedState != FILTER_STATE_STOPPED) {
                lastReportedState = FILTER_STATE_STOPPED;
                if (m_eventListener != NULL)
                    m_eventListener->onFilterEvent(FILTER_EVENT_STOPPED, this, 0);
            }
            m_wakeEvent.wait(&m_lock, 1u, NULL);
            m_lock.unlock();
            m_lock.lock();
        }

        if (m_runState == FILTER_STATE_EXITING) {
            m_lock.unlock();
            return;
        }

        if (m_runState == FILTER_STATE_RUNNING) {
            if (lastReportedState != FILTER_STATE_RUNNING) {
                lastReportedState = FILTER_STATE_RUNNING;
                if (m_eventListener != NULL)
                    m_eventListener->onFilterEvent(FILTER_EVENT_STARTED, this, 0);
            }
            m_lock.unlock();
            runLoop();
        } else {
            m_lock.unlock();
        }
    }
}

/* ===== DecoderFilterBase (0x812070AC - 0x81207497) ===== */

/* 0x812070AC */
DecoderFilterBase::DecoderFilterBase(int workerKind, const char *threadName)
    : ActiveFilterBase(workerKind, threadName)
    , m_inputPipe(NULL)
    , m_outputPipe(NULL)
{
}

/* 0x812070EC / 0x81207122 */
DecoderFilterBase::~DecoderFilterBase()
{
}

/* 0x8120715E */
void DecoderFilterBase::setInputPipe(PipeInterface *pipe)
{
    m_inputPipe = pipe;
}

void DecoderFilterBase::setOutputPipe(PipeInterface *pipe)
{
    m_outputPipe = pipe;
}

/* 0x81207162 */
int DecoderFilterBase::onPipeConsumed(int frameFlags)
{
    m_lock.lock();
    int ret = onFrameConsumed(frameFlags);
    m_wakeEvent.signal(4u);
    m_lock.unlock();
    return ret;
}

/* 0x81207192 */
int DecoderFilterBase::onPipeUnderflow()
{
    m_lock.lock();
    m_wakeEvent.signal(4u);
    m_lock.unlock();
    return 0;
}

/* 0x812071B4 */
int DecoderFilterBase::onPipeProduced()
{
    m_lock.lock();
    m_wakeEvent.signal(2u);
    m_lock.unlock();
    return 0;
}

int DecoderFilterBase::onPipeFull()
{
    return onPipeProduced();
}

/* 0x812071D8 */
int DecoderFilterBase::runLoop()
{
    if (m_inputPipe == NULL || m_outputPipe == NULL)
        return PIPE_ERR_INVALID_ARG;

    int openErr = openDecoder();
    if (openErr != 0) {
        m_lock.lock();
        if (m_eventListener != NULL)
            m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), openErr);
        if (isRunning())
            m_wakeEvent.wait(&m_lock, 1u, NULL);
        m_lock.unlock();
        return openErr;
    }

    MediaFrameBuffer *inFrame = NULL;
    MediaFrameBuffer *outFrame = NULL;
    bool stalled = false;

    for (;;) {
        m_lock.lock();
        if (!isRunning()) {
            m_lock.unlock();
            break;
        }

        if (m_outputPipe->acquireWriteSlot(&outFrame) == PIPE_ERR_FULL) {
            m_wakeEvent.wait(&m_lock, 5u, NULL);
            m_lock.unlock();
            continue;
        }

        if (m_inputPipe->acquireReadSlot(&inFrame) == PIPE_ERR_EMPTY) {
            stalled = true;
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_STALLED, static_cast<ActiveFilterBase *>(this), 0);
            m_wakeEvent.wait(&m_lock, 3u, NULL);
            m_lock.unlock();
            continue;
        }

        if (stalled) {
            stalled = false;
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_RESUMED, static_cast<ActiveFilterBase *>(this), 0);
        }
        m_lock.unlock();

        int decErr = decodeFrame(inFrame, outFrame);
        if (decErr != 0) {
            m_lock.lock();
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), decErr);
            if (!isRunning()) {
                m_lock.unlock();
                break;
            }
            m_wakeEvent.wait(&m_lock, 1u, NULL);
            m_lock.unlock();
            continue;
        }

        int commitErr = m_outputPipe->commitWriteSlot(outFrame);
        if (commitErr != 0) {
            m_lock.lock();
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), commitErr);
            if (!isRunning()) {
                m_lock.unlock();
                break;
            }
            m_wakeEvent.wait(&m_lock, 5u, NULL);
            m_lock.unlock();
            continue;
        }

        int relErr = m_inputPipe->releaseReadSlot(inFrame);
        if (relErr != 0) {
            m_lock.lock();
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), relErr);
            if (!isRunning()) {
                m_lock.unlock();
                break;
            }
            m_wakeEvent.wait(&m_lock, 3u, NULL);
            m_lock.unlock();
        }
    }

    return closeDecoder();
}

/* ===== ParserFilterBase (0x81207498 - 0x81207757) ===== */

/* 0x81207498 */
ParserFilterBase::ParserFilterBase(int workerKind, const char *threadName)
    : ActiveFilterBase(workerKind, threadName)
    , m_outputPipe(NULL)
{
}

/* 0x812074C8 / 0x812074F0 */
ParserFilterBase::~ParserFilterBase()
{
}

/* ===== AudioParserBase (vtable 0x8153467C) ===== */

AudioParserBase::AudioParserBase(int workerKind, const char *threadName)
    : ParserFilterBase(workerKind, threadName)
{
}

/* 0x81207894 / 0x812078B6 */
AudioParserBase::~AudioParserBase()
{
}

/* 0x8120751C */
void ParserFilterBase::setOutputPipe(PipeInterface *pipe)
{
    m_outputPipe = pipe;
}

/* 0x81207520 */
int ParserFilterBase::onPipeConsumed(int frameFlags)
{
    m_lock.lock();
    int ret = onFrameConsumed(frameFlags);
    m_wakeEvent.signal(4u);
    m_lock.unlock();
    return ret;
}

/* 0x81207550 */
int ParserFilterBase::onPipeUnderflow()
{
    m_lock.lock();
    m_wakeEvent.signal(4u);
    m_lock.unlock();
    return 0;
}

/* 0x81207572 */
int ParserFilterBase::runLoop()
{
    if (m_outputPipe == NULL)
        return PIPE_ERR_INVALID_ARG;

    int openErr = openStream();
    if (openErr != 0) {
        m_lock.lock();
        if (m_eventListener != NULL)
            m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), openErr);
        if (isRunning())
            m_wakeEvent.wait(&m_lock, 1u, NULL);
        m_lock.unlock();
        return openErr;
    }

    MediaFrameBuffer *outFrame = NULL;
    bool firstIteration = true;
    bool stalled = false;

    for (;;) {
        m_lock.lock();
        if (!firstIteration && !isRunning()) {
            m_lock.unlock();
            break;
        }

        if (m_outputPipe->acquireWriteSlot(&outFrame) == PIPE_ERR_FULL) {
            m_wakeEvent.wait(&m_lock, 5u, NULL);
            m_lock.unlock();
            continue;
        }
        m_lock.unlock();

        int prodErr = produceFrame(outFrame);
        if (prodErr != 0) {
            m_lock.lock();
            if (prodErr == PARSER_ERR_UNDERFLOW) {
                if (!stalled) {
                    stalled = true;
                    if (m_eventListener != NULL)
                        m_eventListener->onFilterEvent(FILTER_EVENT_STALLED, static_cast<ActiveFilterBase *>(this), 0);
                }
            } else {
                if (m_eventListener != NULL)
                    m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), prodErr);
            }
            if (!isRunning()) {
                m_lock.unlock();
                break;
            }
            m_wakeEvent.wait(&m_lock, 1u, NULL);
            m_lock.unlock();
            firstIteration = false;
            continue;
        }

        if (stalled) {
            stalled = false;
            if (m_eventListener != NULL)
                m_eventListener->onFilterEvent(FILTER_EVENT_RESUMED, static_cast<ActiveFilterBase *>(this), 0);
        }

        m_outputPipe->commitWriteSlot(outFrame);
        firstIteration = false;
    }

    int closeErr = closeStream();
    if (closeErr != 0) {
        if (m_eventListener != NULL)
            m_eventListener->onFilterEvent(FILTER_EVENT_ERROR, static_cast<ActiveFilterBase *>(this), closeErr);
        return closeErr;
    }
    return 0;
}

/* ===== FixedFramePipe (52 bytes, 0x812078FC - 0x81207DE3) ===== */

class FixedFramePipe : public PipeInterface {
public:
    FixedFramePipe(uint32_t frameBytes, int slotCount,
                   PipeProducerControl *producer, PipeConsumerControl *consumer);
    virtual ~FixedFramePipe();

    virtual void setPositionListener(PositionChangeListener *listener) { m_posListener = listener; }
    virtual bool isInitialized() const;
    virtual int getPlayPositionMs() const { return m_playPositionMs; }
    virtual void setPlayPositionMs(int posMs) { m_playPositionMs = posMs; }
    virtual void reset();
    virtual bool hasQueuedFrame() const { return m_readIndex >= 0; }
    virtual bool hasReadableFrame() const { return m_readIndex >= 0; }
    virtual int acquireWriteSlot(MediaFrameBuffer **outBuf);
    virtual int commitWriteSlot(MediaFrameBuffer *buf);
    virtual int acquireReadSlot(MediaFrameBuffer **outBuf);
    virtual int releaseReadSlot(MediaFrameBuffer *buf);

private:
    uint32_t m_frameBytes;                  /* +4 */
    int m_slotCount;                        /* +8 */
    void *m_framePool;                      /* +12 */
    MediaFrameBuffer *m_slots;              /* +16 */
    int m_readIndex;                        /* +20 */
    int m_writeIndex;                       /* +24 */
    PipeProducerControl *m_producer;        /* +28 */
    PipeConsumerControl *m_consumer;        /* +32 */
    PositionChangeListener *m_posListener;  /* +36 */
    int m_playPositionMs;                   /* +40 */
    SyncMutex m_lock;                       /* +44 */
};

/* 0x812078FC */
FixedFramePipe::FixedFramePipe(uint32_t frameBytes, int slotCount,
                               PipeProducerControl *producer, PipeConsumerControl *consumer)
    : m_frameBytes(frameBytes)
    , m_slotCount(slotCount)
    , m_framePool(NULL)
    , m_slots(NULL)
    , m_readIndex(-1)
    , m_writeIndex(-1)
    , m_producer(producer)
    , m_consumer(consumer)
    , m_posListener(NULL)
    , m_playPositionMs(-1)
    , m_lock("SceShellMusicCorePipe")
{
    m_framePool = BufferPool::getInstance()->acquireTyped(6);
    m_slots = (MediaFrameBuffer *)ScePafStdc_3CF19248(sizeof(MediaFrameBuffer) * (SceSize)slotCount,
                                                      getLocalHeap());
    if (m_framePool == NULL || m_slots == NULL) {
        if (m_framePool != NULL) {
            BufferPool::getInstance()->releaseTyped(m_framePool);
            m_framePool = NULL;
        }
        if (m_slots != NULL) {
            localFree(m_slots);
            m_slots = NULL;
        }
        return;
    }

    uintptr_t poolBase = (uintptr_t)m_framePool;
    uintptr_t offset = 0;
    for (int idx = 0; idx < slotCount; ++idx) {
        MediaFrameBuffer *slot = &m_slots[idx];
        slot->frameData = (void *)((poolBase + offset + 1u) & ~(uintptr_t)1u);
        slot->maxBufferSize = frameBytes;
        slot->payloadSize = -1;
        slot->timestampMs = -1.0f;
        slot->frameDurationMs = -1.0f;
        slot->sampleRate = -1;
        slot->packetFlags = 255;
        slot->discontinuity = 0;
        offset += frameBytes + 2u;
    }

    m_readIndex = -1;
    m_writeIndex = 0;
}

/* 0x81207A04 / 0x81207A3C */
FixedFramePipe::~FixedFramePipe()
{
    if (m_framePool != NULL) {
        BufferPool::getInstance()->releaseTyped(m_framePool);
        m_framePool = NULL;
    }
    if (m_slots != NULL) {
        delete[] m_slots;
        m_slots = NULL;
    }
}

/* 0x81207A78 */
bool FixedFramePipe::isInitialized() const
{
    return m_lock.isValid() && m_framePool != NULL && m_slots != NULL;
}

/* 0x81207A9C */
void FixedFramePipe::reset()
{
    m_lock.lock();
    m_readIndex = -1;
    m_writeIndex = 0;
    m_playPositionMs = -1;
    m_lock.unlock();
}

/* 0x81207ACA */
int FixedFramePipe::acquireWriteSlot(MediaFrameBuffer **outBuf)
{
    if (m_framePool == NULL || m_slots == NULL)
        return PIPE_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL) {
        *outBuf = NULL;
        return -1;
    }

    m_lock.lock();
    if (m_writeIndex < 0) {
        m_lock.unlock();
        *outBuf = NULL;
        m_consumer->onPipeFull();
        return PIPE_ERR_FULL;
    }
    if (m_writeIndex >= m_slotCount) {
        m_lock.unlock();
        *outBuf = NULL;
        return -1;
    }

    *outBuf = &m_slots[m_writeIndex];
    m_lock.unlock();
    return 0;
}

/* 0x81207B42 */
int FixedFramePipe::commitWriteSlot(MediaFrameBuffer *buf)
{
    if (buf == NULL)
        return PIPE_ERR_INVALID_ARG;
    if (m_framePool == NULL || m_slots == NULL)
        return PIPE_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL || m_writeIndex >= m_slotCount)
        return -1;

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return PIPE_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return PIPE_ERR_INVALID_ARG;

    m_lock.lock();
    if (slotIdx != m_writeIndex) {
        m_lock.unlock();
        return PIPE_ERR_OUT_OF_ORDER;
    }

    int nextWrite = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextWrite == m_readIndex) {
        m_writeIndex = -1;
    } else {
        m_writeIndex = nextWrite;
        if (m_readIndex < 0)
            m_readIndex = slotIdx;
    }
    m_lock.unlock();

    m_consumer->onPipeProduced();
    if (m_posListener != NULL) {
        if (m_playPositionMs < 0 ||
            (float)(m_playPositionMs / 1000) != (buf->timestampMs / 1000.0f)) {
            m_posListener->onPositionSecondChanged((int)buf->timestampMs);
        }
    }
    m_playPositionMs = (int)buf->timestampMs;
    return 0;
}

/* 0x81207C94 */
int FixedFramePipe::acquireReadSlot(MediaFrameBuffer **outBuf)
{
    if (m_framePool == NULL || m_slots == NULL)
        return PIPE_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL) {
        *outBuf = NULL;
        return -1;
    }

    m_lock.lock();
    if (m_readIndex < 0) {
        m_lock.unlock();
        *outBuf = NULL;
        return PIPE_ERR_EMPTY;
    }
    if (m_readIndex >= m_slotCount) {
        m_lock.unlock();
        *outBuf = NULL;
        m_producer->onPipeUnderflow();
        return -1;
    }

    *outBuf = &m_slots[m_readIndex];
    m_lock.unlock();
    return 0;
}

/* 0x81207D08 */
int FixedFramePipe::releaseReadSlot(MediaFrameBuffer *buf)
{
    if (m_framePool == NULL || m_slots == NULL)
        return PIPE_ERR_NO_MEMORY;
    if (m_producer == NULL || m_consumer == NULL || m_readIndex >= m_slotCount)
        return -1;

    ptrdiff_t byteDelta = (const uint8_t *)buf - (const uint8_t *)m_slots;
    if (byteDelta < 0 || ((size_t)byteDelta % sizeof(MediaFrameBuffer)) != 0)
        return PIPE_ERR_INVALID_ARG;
    int slotIdx = (int)((size_t)byteDelta / sizeof(MediaFrameBuffer));
    if (slotIdx >= m_slotCount)
        return PIPE_ERR_INVALID_ARG;

    m_lock.lock();
    if (slotIdx != m_readIndex) {
        m_lock.unlock();
        return PIPE_ERR_OUT_OF_ORDER;
    }

    int nextRead = (slotIdx + 1 == m_slotCount) ? 0 : (slotIdx + 1);
    if (nextRead == m_writeIndex) {
        m_readIndex = -1;
    } else {
        m_readIndex = nextRead;
        if (m_writeIndex < 0)
            m_writeIndex = slotIdx;
    }
    int frameFlags = buf->frameFlags;
    m_lock.unlock();

    m_producer->onPipeConsumed(frameFlags);
    return 0;
}

/* ===== Pipeline component factories (0x81207758 - 0x8120784F) ===== */

/* 0x81207758 */
PipeInterface *createFixedFramePipe(uint32_t frameBytes, int slotCount,
                                    PipeProducerControl *producer,
                                    PipeConsumerControl *consumer)
{
    void *mem = musiccoreAllocFromPafHeap(sizeof(FixedFramePipe), getLocalHeap());
    if (mem == NULL)
        return NULL;
    return new (mem) FixedFramePipe(frameBytes, slotCount, producer, consumer);
}

/* 0x81207790 */
PipeInterface *createBufRefPipe(int slotCount,
                                PipeProducerControl *producer,
                                PipeConsumerControl *consumer)
{
    void *mem = musiccoreAllocFromPafHeap(sizeof(BufRefPipe), getLocalHeap());
    if (mem == NULL)
        return NULL;
    return new (mem) BufRefPipe(slotCount, producer, consumer);
}

/* 0x812077BE */
AudioOutputFilter *createAudioOutputFilter(uint32_t grainBytes, int slotCount,
                                           PipeProducerControl *producer,
                                           AudioOpenCallback openCb,
                                           AudioCloseCallback closeCb,
                                           AudioWriteCallback writeCb,
                                           AudioFlushCallback flushCb,
                                           void *userContext)
{
    void *mem = musiccoreAllocFromPafHeap(sizeof(AudioOutputFilter), getLocalHeap());
    if (mem == NULL)
        return NULL;
    return new (mem) AudioOutputFilter(grainBytes, slotCount, producer,
                                       openCb, closeCb, writeCb, flushCb, userContext);
}

/* 0x81207814 */
PipelineEventListener *createDecoderPipeline(ParserFilterBase *parser,
                                             PipeInterface *pipe,
                                             DecoderFilterBase *decoder,
                                             AudioOutputFilter *audioSink)
{
    void *mem = musiccoreAllocFromPafHeap(sizeof(DecoderPipeline), getLocalHeap());
    if (mem == NULL)
        return NULL;
    return reinterpret_cast<PipelineEventListener *>(new (mem) DecoderPipeline(
        reinterpret_cast<ParserFilter *>(parser),
        reinterpret_cast<BufRefPipe *>(pipe),
        reinterpret_cast<DecoderFilter *>(decoder),
        reinterpret_cast<AudioOutputSink *>(audioSink)));
}

} /* namespace MusicCore */
} /* namespace sce */
