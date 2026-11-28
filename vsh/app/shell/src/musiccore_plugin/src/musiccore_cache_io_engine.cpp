/*
 * SceShell musiccore_plugin - CacheIoEngine (synchronous double-buffered stream cache
 * and asynchronous ring-buffered prefetch cache with worker threads).
 * Original address range (3.65-CEX): 0x8120305C - 0x81206DF0
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_cache_io_engine.h"
#include "musiccore_pipeline_types.h"

#include <memory/heap_allocator.h>

extern "C" {
void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
void *ScePafStdc_3CF19248(SceSize size, void *heap);
}

static inline void *musiccoreAllocFromPafHeap(SceSize size, void *heap)
{
	return operator new(size, static_cast<paf::memory::HeapAllocator *>(heap));
}

namespace sce {
namespace MusicCore {

static const int CACHE_ERR_INVALID_ARG = -2147434241;    /* 0x8000BFFFu */
static const int CACHE_ERR_NO_CALLBACKS = -2147434255;   /* 0x8000BFF1u */
static const int CACHE_ERR_OPEN_FAILED = -2147434495;    /* 0x8000BF01u */
static const int CACHE_ERR_CLOSE_FAILED = -2147434494;   /* 0x8000BF02u */
static const int CACHE_ERR_NOT_OPEN = -2147434491;       /* 0x8000BF05u */
static const int CACHE_ERR_ALREADY_OPEN = -2147434490;   /* 0x8000BF06u */
static const int CACHE_ERR_EOF = -2147434489;            /* 0x8000BF07u */
static const int CACHE_ERR_NO_MEMORY = -2147430401;      /* 0x8000CFFFu */

enum CacheBlockState {
    CACHE_BLOCK_EMPTY = 1,
    CACHE_BLOCK_READY = 2,
    CACHE_BLOCK_SYNC_LOAD = 4,
    CACHE_BLOCK_ASYNC_LOAD = 8
};

enum CacheDirectionMode {
    CACHE_DIR_NONE = 0,
    CACHE_DIR_FORWARD = 1,
    CACHE_DIR_REVERSE = 2
};

/* Global buffer pool and worker pool helpers (musiccore_heap.cpp, musiccore_decoder_manager.cpp) */
extern void *getBufferPool();
extern void *bufferPoolAllocPrefetchRing(void *pool);
extern void bufferPoolFreePrefetchRing(void *pool, void *buf);
extern void *bufferPoolAllocSyncBlock(void *pool);
extern void bufferPoolFreeSyncBlock(void *pool, void *buf);

extern void *getWorkerThreadPool();
extern int workerPoolSpawn(void *pool, int workerKind, void *entryFn, void *arg, SceUID *outThreadId);
extern int workerPoolJoin(void *pool, SceUID threadId);

struct PrefetchWorkerArg;

/* 0x8120305C */
static uint8_t *allocByteArray(int count)
{
    return (uint8_t *)ScePafStdc_3CF19248((SceSize)count, getLocalHeap());
}

/* 0x81203070 */
static void *allocWordArray(int count)
{
    return ScePafStdc_3CF19248((SceSize)(4 * count), getLocalHeap());
}

/* 0x81206DDC */
static int64_t *allocInt64Array(int count)
{
    return (int64_t *)ScePafStdc_3CF19248((SceSize)(8 * count), getLocalHeap());
}

/* 208-byte CacheIoEngine object (vtable 0x81534504) */
class CacheIoEngine {
public:
    CacheIoEngine(uint8_t syncStreamCount, const uint32_t *syncBlockSizes,
                  uint8_t asyncStreamCount, const AsyncRingConfig *asyncConfigs,
                  uint8_t enableWorkers, int threadPriority,
                  void *userCtx, void *extraCtx);
    virtual ~CacheIoEngine();

    int initSyncCaches(const uint32_t *syncBlockSizes);
    int destroySyncCaches();
    int initAsyncCaches(const AsyncRingConfig *asyncConfigs);
    int destroyAsyncCaches();
    int initAsyncWorkers(const AsyncRingConfig *asyncConfigs);
    int destroyAsyncWorkers();

    int setCallbacks(StreamOpenCallback openCb, StreamCloseCallback closeCb,
                     StreamSeekCallback seekCb, StreamReadCallback readCb,
                     StreamStatSizeCallback statSizeCb, StreamErrorCallback errCb);
    uint8_t isReady() const { return m_isInitialized; }
    int openStream(void *streamHandle);
    int closeStream();
    bool isStreamOpen() const { return m_streamHandle != NULL; }
    int getStreamSize(int64_t *outSize) const;
    int getStreamPath(const char **outPath) const;
    int setStreamPath(const char *path);
    int setDirectionHint(int directionMode, int forcePrefetch);

    int readSync(uint8_t streamId, int64_t offset, uint32_t length,
                 void **outPtr, uint8_t *outDidIo, uint32_t *outAvail);
    int readAsync(uint8_t streamId, int64_t offset, uint32_t length, uint8_t pinBlocks,
                  void **outPtr, PrefetchPinToken **outToken,
                  uint8_t *outWokeWorker, uint32_t *outAvail);
    int releaseAsyncPin(uint8_t streamId, PrefetchPinToken **ioToken);

    void runPrefetchWorker(uint8_t streamId);
    static int prefetchWorkerEntry(SceSize args, void *argp);

private:
    int findBlockSlot(int startUnused, int64_t targetOffset, int stateMask,
                      int slotCount, int directionMode,
                      const int64_t *slotOffsets, const int *slotStates,
                      int startIndex, int *outErr);
    int prepareBlockSlot(int slotIndex, int64_t targetOffset, uint32_t blockSize,
                         int slotCount, int directionMode,
                         uint8_t **slotBuffers, int64_t *slotOffsets, int *slotStates,
                         int *slotRefCounts, int *slotValidBytes,
                         int protectedSlotA, int protectedSlotB,
                         uint8_t syncReadFlag, uint8_t *outNeedsLoad);
    uint64_t scoreCandidateSlot(int candidateIdx, int64_t firstOffset, uint8_t spansTwoBlocks,
                                int directionMode, int lastUsedSlot, uint32_t blockSize,
                                int usableSlots, int64_t *slotOffsets,
                                const int *slotStates, const int *slotRefCounts,
                                int protectedSlotA, int protectedSlotB,
                                int runStart, int runLength,
                                int64_t windowStart, int64_t windowEnd);
    int findLongestContiguousRun(int slotCount, const int64_t *slotOffsets, uint32_t blockSize,
                                 int64_t windowStart, int64_t windowEnd, int *outRunLength);
    int fetchAsyncBlocks(uint8_t streamId, int64_t offset, uint32_t length, uint8_t pinBlocks,
                         void **outPtr, PrefetchPinToken **outToken,
                         uint8_t *outWokeWorker, int *outErr);
    void drainPendingAsyncLoads(uint8_t streamId);
    int loadSyncBlock(uint8_t slotIdx, bool useSecondHalf, int64_t alignedOffset,
                      uint8_t *outDidIo, uint32_t *outAvail, int64_t reqOffset);

    uint8_t m_syncStreamCount;              /* +4 */
    uint8_t m_padSyncCount[3];              /* +5..+7 */
    uint32_t *m_syncBlockSizes;             /* +8 */
    void **m_syncRawBuffers;                /* +12 */
    uint8_t **m_syncFirstHalves;            /* +16 */
    uint8_t **m_syncSecondHalves;           /* +20 */
    int64_t *m_syncFirstOffsets;            /* +24 */
    int64_t *m_syncSecondOffsets;           /* +28 */
    int *m_syncFirstValidBytes;             /* +32 */
    int *m_syncSecondValidBytes;            /* +36 */

    uint8_t m_asyncStreamCount;             /* +40 */
    uint8_t m_padAsyncCount[3];             /* +41..+43 */
    uint32_t *m_asyncBlockSizes;            /* +44 */
    uint32_t *m_asyncBlockCounts;           /* +48 */
    int64_t *m_asyncWindowStarts;           /* +52 */
    int64_t *m_asyncWindowEnds;             /* +56 */
    void **m_asyncLastReadPtrs;             /* +60 */
    int *m_asyncLastSlots;                  /* +64 */
    uint8_t ***m_asyncSlotPtrs;             /* +68 */
    int **m_asyncSlotValidBytes;            /* +72 */
    int **m_asyncSlotStates;                /* +76 */
    int64_t **m_asyncSlotOffsets;           /* +80 */
    int **m_asyncSlotRefCounts;             /* +84 */
    void **m_asyncRingRawBufs;              /* +88 */
    int m_workerPriority;                   /* +92 */
    uint8_t m_closingStream;                /* +96 */
    uint8_t m_padClosing[3];                /* +97..+99 */
    SyncEvent *m_asyncWakeEvents;           /* +100 */
    SyncMutex *m_asyncLocks;                /* +104 */
    SceUID *m_asyncThreadIds;               /* +108 */
    uint8_t *m_asyncWakeRequested;          /* +112 */
    uint8_t *m_asyncWorkerExiting;          /* +116 */
    uint8_t *m_asyncWorkerBusy;             /* +120 */

    SyncMutex m_fileSeekLock;               /* +124 */
    const char *m_streamPath;               /* +132 */
    void *m_streamHandle;                   /* +136 */
    int m_padFileSize;                      /* +140 */
    int64_t m_streamSize;                   /* +144 */
    int m_directionMode;                    /* +152 */
    int m_forcePrefetch;                    /* +156 */
    uint8_t m_syncInitialized;              /* +160 */
    uint8_t m_asyncInitialized;             /* +161 */
    uint8_t m_workersInitialized;           /* +162 */
    uint8_t m_isInitialized;                /* +163 */

    StreamOpenCallback m_openCb;            /* +164 */
    StreamCloseCallback m_closeCb;          /* +168 */
    StreamSeekCallback m_seekCb;            /* +172 */
    StreamReadCallback m_readCb;            /* +176 */
    StreamStatSizeCallback m_statSizeCb;    /* +180 */
    StreamErrorCallback m_errCb;            /* +184 */
    uint8_t m_callbacksSet;                 /* +188 */
    uint8_t m_padCallbacks[3];              /* +189..+191 */
    int m_lastSyncError;                    /* +192 */
    int m_lastAsyncError;                   /* +196 */
    void *m_userCtx;                        /* +200 */
    void *m_extraCtx;                       /* +204 */
};

struct PrefetchWorkerArg {
    CacheIoEngine *engine;
    uint8_t streamId;
    uint8_t padTail[3];
};

/* 0x81203294 */
int CacheIoEngine::destroySyncCaches()
{
    if (m_syncBlockSizes != NULL && m_syncRawBuffers != NULL &&
        m_syncFirstHalves != NULL && m_syncSecondHalves != NULL &&
        m_syncFirstOffsets != NULL && m_syncSecondOffsets != NULL &&
        m_syncFirstValidBytes != NULL && m_syncSecondValidBytes != NULL) {
        for (uint8_t idx = 0; idx < m_syncStreamCount; ++idx) {
            if (m_syncRawBuffers[idx] != NULL) {
                bufferPoolFreeSyncBlock(getBufferPool(), m_syncRawBuffers[idx]);
            }
        }
    }

    if (m_syncFirstHalves != NULL) {
        delete[] m_syncFirstHalves;
        m_syncFirstHalves = NULL;
    }
    if (m_syncSecondHalves != NULL) {
        delete[] m_syncSecondHalves;
        m_syncSecondHalves = NULL;
    }
    if (m_syncFirstOffsets != NULL) {
        delete[] m_syncFirstOffsets;
        m_syncFirstOffsets = NULL;
    }
    if (m_syncSecondOffsets != NULL) {
        delete[] m_syncSecondOffsets;
        m_syncSecondOffsets = NULL;
    }
    if (m_syncFirstValidBytes != NULL) {
        delete[] m_syncFirstValidBytes;
        m_syncFirstValidBytes = NULL;
    }
    if (m_syncSecondValidBytes != NULL) {
        delete[] m_syncSecondValidBytes;
        m_syncSecondValidBytes = NULL;
    }
    if (m_syncRawBuffers != NULL) {
        delete[] m_syncRawBuffers;
        m_syncRawBuffers = NULL;
    }
    if (m_syncBlockSizes != NULL) {
        delete[] m_syncBlockSizes;
        m_syncBlockSizes = NULL;
    }
    m_syncInitialized = 0;
    return 0;
}

/* 0x81203352 */
int CacheIoEngine::destroyAsyncCaches()
{
    if (m_asyncBlockSizes != NULL && m_asyncBlockCounts != NULL &&
        m_asyncWindowStarts != NULL && m_asyncWindowEnds != NULL &&
        m_asyncLastReadPtrs != NULL && m_asyncLastSlots != NULL &&
        m_asyncSlotPtrs != NULL && m_asyncSlotValidBytes != NULL &&
        m_asyncSlotStates != NULL && m_asyncSlotOffsets != NULL &&
        m_asyncSlotRefCounts != NULL && m_asyncRingRawBufs != NULL) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            if (m_asyncSlotPtrs[idx] != NULL) {
                delete[] m_asyncSlotPtrs[idx];
                m_asyncSlotPtrs[idx] = NULL;
            }
            if (m_asyncSlotValidBytes[idx] != NULL) {
                delete[] m_asyncSlotValidBytes[idx];
                m_asyncSlotValidBytes[idx] = NULL;
            }
            if (m_asyncSlotStates[idx] != NULL) {
                delete[] m_asyncSlotStates[idx];
                m_asyncSlotStates[idx] = NULL;
            }
            if (m_asyncSlotOffsets[idx] != NULL) {
                delete[] m_asyncSlotOffsets[idx];
                m_asyncSlotOffsets[idx] = NULL;
            }
            if (m_asyncSlotRefCounts[idx] != NULL) {
                delete[] m_asyncSlotRefCounts[idx];
                m_asyncSlotRefCounts[idx] = NULL;
            }
            if (m_asyncRingRawBufs[idx] != NULL) {
                bufferPoolFreePrefetchRing(getBufferPool(), m_asyncRingRawBufs[idx]);
            }
        }
    }

    if (m_asyncBlockSizes != NULL) { delete[] m_asyncBlockSizes; m_asyncBlockSizes = NULL; }
    if (m_asyncBlockCounts != NULL) { delete[] m_asyncBlockCounts; m_asyncBlockCounts = NULL; }
    if (m_asyncWindowStarts != NULL) { delete[] m_asyncWindowStarts; m_asyncWindowStarts = NULL; }
    if (m_asyncWindowEnds != NULL) { delete[] m_asyncWindowEnds; m_asyncWindowEnds = NULL; }
    if (m_asyncLastReadPtrs != NULL) { delete[] m_asyncLastReadPtrs; m_asyncLastReadPtrs = NULL; }
    if (m_asyncLastSlots != NULL) { delete[] m_asyncLastSlots; m_asyncLastSlots = NULL; }
    if (m_asyncSlotPtrs != NULL) { delete[] m_asyncSlotPtrs; m_asyncSlotPtrs = NULL; }
    if (m_asyncSlotValidBytes != NULL) { delete[] m_asyncSlotValidBytes; m_asyncSlotValidBytes = NULL; }
    if (m_asyncSlotStates != NULL) { delete[] m_asyncSlotStates; m_asyncSlotStates = NULL; }
    if (m_asyncSlotOffsets != NULL) { delete[] m_asyncSlotOffsets; m_asyncSlotOffsets = NULL; }
    if (m_asyncSlotRefCounts != NULL) { delete[] m_asyncSlotRefCounts; m_asyncSlotRefCounts = NULL; }
    if (m_asyncRingRawBufs != NULL) { delete[] m_asyncRingRawBufs; m_asyncRingRawBufs = NULL; }

    if (m_asyncLocks != NULL) {
        for (int idx = (int)m_asyncStreamCount - 1; idx >= 0; --idx) {
            (m_asyncLocks[idx]).~SyncMutex();
        }
        localFree(m_asyncLocks);
        m_asyncLocks = NULL;
    }

    m_asyncInitialized = 0;
    return 0;
}

/* 0x81203598 */
int CacheIoEngine::destroyAsyncWorkers()
{
    if (m_asyncWakeEvents != NULL && m_asyncLocks != NULL &&
        m_asyncThreadIds != NULL && m_asyncWakeRequested != NULL &&
        m_asyncWorkerExiting != NULL && m_asyncWorkerBusy != NULL) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            if (isValidWorkerThreadId(m_asyncThreadIds[idx]) != 0) {
                m_asyncLocks[idx].lock();
                m_asyncWorkerExiting[idx] = 1;
                m_asyncWakeEvents[idx].signal(0xFFFFFFFFu);
                m_asyncLocks[idx].unlock();
            }
        }
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            if (isValidWorkerThreadId(m_asyncThreadIds[idx]) != 0) {
                workerPoolJoin(getWorkerThreadPool(), m_asyncThreadIds[idx]);
            }
        }
    }

    if (m_asyncWakeEvents != NULL) {
        for (int idx = (int)m_asyncStreamCount - 1; idx >= 0; --idx) {
            (m_asyncWakeEvents[idx]).~SyncCond();
        }
        localFree(m_asyncWakeEvents);
        m_asyncWakeEvents = NULL;
    }
    if (m_asyncThreadIds != NULL) { delete[] m_asyncThreadIds; m_asyncThreadIds = NULL; }
    if (m_asyncWakeRequested != NULL) { delete[] m_asyncWakeRequested; m_asyncWakeRequested = NULL; }
    if (m_asyncWorkerExiting != NULL) { delete[] m_asyncWorkerExiting; m_asyncWorkerExiting = NULL; }
    if (m_asyncWorkerBusy != NULL) { delete[] m_asyncWorkerBusy; m_asyncWorkerBusy = NULL; }

    m_workersInitialized = 0;
    return 0;
}

/* 0x812036E8 */
int CacheIoEngine::initAsyncWorkers(const AsyncRingConfig *asyncConfigs)
{
    if (m_asyncStreamCount == 0)
        return 0;
    if (m_asyncStreamCount > 127 || asyncConfigs == NULL)
        return CACHE_ERR_INVALID_ARG;

    m_asyncWakeEvents = (SyncEvent *)ScePafStdc_3CF19248(sizeof(SyncEvent) * m_asyncStreamCount, getLocalHeap());
    if (m_asyncWakeEvents != NULL) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            new (&m_asyncWakeEvents[idx]) SyncCond("SceShellMusicCoreCommonCond");
        }
    }
    m_asyncThreadIds = (SceUID *)allocWordArray(m_asyncStreamCount);
    m_asyncWakeRequested = allocByteArray(m_asyncStreamCount);
    m_asyncWorkerExiting = allocByteArray(m_asyncStreamCount);
    m_asyncWorkerBusy = allocByteArray(m_asyncStreamCount);

    if (m_asyncWakeEvents == NULL || m_asyncThreadIds == NULL ||
        m_asyncWakeRequested == NULL || m_asyncWorkerExiting == NULL ||
        m_asyncWorkerBusy == NULL) {
        destroyAsyncWorkers();
        return CACHE_ERR_NO_MEMORY;
    }

    for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
        m_asyncWakeRequested[idx] = 0;
        m_asyncWorkerExiting[idx] = 0;
        m_asyncWorkerBusy[idx] = 0;

        PrefetchWorkerArg *arg = (PrefetchWorkerArg *)musiccoreAllocFromPafHeap(sizeof(PrefetchWorkerArg), getLocalHeap());
        if (arg != NULL) {
            arg->engine = this;
            arg->streamId = (uint8_t)(idx + 1);
        }
        if (workerPoolSpawn(getWorkerThreadPool(), 0, (void *)prefetchWorkerEntry,
                            arg, &m_asyncThreadIds[idx]) != 0) {
            if (arg != NULL)
                localFree(arg);
            m_asyncThreadIds[idx] = -1;
            return CACHE_ERR_NO_MEMORY;
        }
    }

    m_workersInitialized = 1;
    return 0;
}

/* 0x812038D0 */
int CacheIoEngine::initSyncCaches(const uint32_t *syncBlockSizes)
{
    if (m_syncStreamCount == 0)
        return 0;
    if (m_syncStreamCount > 127 || syncBlockSizes == NULL)
        return CACHE_ERR_INVALID_ARG;

    m_syncBlockSizes = (uint32_t *)allocWordArray(m_syncStreamCount);
    m_syncRawBuffers = (void **)allocWordArray(m_syncStreamCount);
    m_syncFirstHalves = (uint8_t **)allocWordArray(m_syncStreamCount);
    m_syncSecondHalves = (uint8_t **)allocWordArray(m_syncStreamCount);
    m_syncFirstOffsets = allocInt64Array(m_syncStreamCount);
    m_syncSecondOffsets = allocInt64Array(m_syncStreamCount);
    m_syncFirstValidBytes = (int *)allocWordArray(m_syncStreamCount);
    m_syncSecondValidBytes = (int *)allocWordArray(m_syncStreamCount);

    if (m_syncBlockSizes == NULL || m_syncRawBuffers == NULL ||
        m_syncFirstHalves == NULL || m_syncSecondHalves == NULL ||
        m_syncFirstOffsets == NULL || m_syncSecondOffsets == NULL ||
        m_syncFirstValidBytes == NULL || m_syncSecondValidBytes == NULL) {
        destroySyncCaches();
        return CACHE_ERR_NO_MEMORY;
    }

    int status = 0;
    for (uint8_t idx = 0; idx < m_syncStreamCount; ++idx) {
        m_syncBlockSizes[idx] = syncBlockSizes[idx];
        void *rawBuf = bufferPoolAllocSyncBlock(getBufferPool());
        m_syncRawBuffers[idx] = rawBuf;
        if (rawBuf != NULL) {
            uintptr_t alignedAddr = ((uintptr_t)rawBuf + 63u) & ~(uintptr_t)63u;
            uint8_t *firstHalf = (uint8_t *)alignedAddr;
            m_syncFirstHalves[idx] = firstHalf;
            m_syncSecondHalves[idx] = firstHalf + m_syncBlockSizes[idx];
            m_syncFirstOffsets[idx] = -1;
            m_syncSecondOffsets[idx] = -1;
            m_syncFirstValidBytes[idx] = -1;
            m_syncSecondValidBytes[idx] = -1;
        } else {
            status = CACHE_ERR_NO_MEMORY;
        }
    }
    m_syncInitialized = 1;
    return status;
}

/* 0x81203A7C */
int CacheIoEngine::initAsyncCaches(const AsyncRingConfig *asyncConfigs)
{
    if (m_asyncStreamCount == 0)
        return 0;
    if (m_asyncStreamCount > 127 || asyncConfigs == NULL)
        return CACHE_ERR_INVALID_ARG;

    m_asyncBlockSizes = (uint32_t *)allocWordArray(m_asyncStreamCount);
    m_asyncBlockCounts = (uint32_t *)allocWordArray(m_asyncStreamCount);
    m_asyncWindowStarts = allocInt64Array(m_asyncStreamCount);
    m_asyncWindowEnds = allocInt64Array(m_asyncStreamCount);
    m_asyncLastReadPtrs = (void **)allocWordArray(m_asyncStreamCount);
    m_asyncLastSlots = (int *)allocWordArray(m_asyncStreamCount);
    m_asyncSlotPtrs = (uint8_t ***)allocWordArray(m_asyncStreamCount);
    m_asyncSlotValidBytes = (int **)allocWordArray(m_asyncStreamCount);
    m_asyncSlotStates = (int **)allocWordArray(m_asyncStreamCount);
    m_asyncSlotOffsets = (int64_t **)allocWordArray(m_asyncStreamCount);
    m_asyncSlotRefCounts = (int **)allocWordArray(m_asyncStreamCount);
    m_asyncRingRawBufs = (void **)allocWordArray(m_asyncStreamCount);

    m_asyncLocks = (SyncMutex *)ScePafStdc_3CF19248(sizeof(SyncMutex) * m_asyncStreamCount, getLocalHeap());
    if (m_asyncLocks != NULL) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            new (&m_asyncLocks[idx]) SyncMutex("SceShellMusicCoreCommonLock");
        }
    }

    if (m_asyncBlockSizes == NULL || m_asyncBlockCounts == NULL ||
        m_asyncWindowStarts == NULL || m_asyncWindowEnds == NULL ||
        m_asyncLastReadPtrs == NULL || m_asyncLastSlots == NULL ||
        m_asyncSlotPtrs == NULL || m_asyncSlotValidBytes == NULL ||
        m_asyncSlotStates == NULL || m_asyncSlotOffsets == NULL ||
        m_asyncSlotRefCounts == NULL || m_asyncRingRawBufs == NULL ||
        m_asyncLocks == NULL) {
        destroyAsyncCaches();
        return CACHE_ERR_NO_MEMORY;
    }

    for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
        m_asyncBlockSizes[idx] = asyncConfigs[idx].blockSize;
        m_asyncBlockCounts[idx] = asyncConfigs[idx].blockCount;
        m_asyncWindowStarts[idx] = -1;
        m_asyncWindowEnds[idx] = -1;
        m_asyncLastReadPtrs[idx] = NULL;
        m_asyncLastSlots[idx] = -1;

        int slotTotal = (int)(m_asyncBlockCounts[idx] + 1u);
        m_asyncSlotPtrs[idx] = (uint8_t **)allocWordArray(slotTotal);
        m_asyncSlotValidBytes[idx] = (int *)allocWordArray(slotTotal);
        m_asyncSlotStates[idx] = (int *)allocWordArray(slotTotal);
        m_asyncSlotOffsets[idx] = allocInt64Array(slotTotal);
        m_asyncSlotRefCounts[idx] = (int *)allocWordArray(slotTotal);
        m_asyncRingRawBufs[idx] = bufferPoolAllocPrefetchRing(getBufferPool());

        if (m_asyncSlotPtrs[idx] == NULL || m_asyncSlotValidBytes[idx] == NULL ||
            m_asyncSlotStates[idx] == NULL || m_asyncSlotOffsets[idx] == NULL ||
            m_asyncSlotRefCounts[idx] == NULL || m_asyncRingRawBufs[idx] == NULL) {
            if (m_asyncSlotPtrs[idx] != NULL) { delete[] m_asyncSlotPtrs[idx]; m_asyncSlotPtrs[idx] = NULL; }
            if (m_asyncSlotValidBytes[idx] != NULL) { delete[] m_asyncSlotValidBytes[idx]; m_asyncSlotValidBytes[idx] = NULL; }
            if (m_asyncSlotStates[idx] != NULL) { delete[] m_asyncSlotStates[idx]; m_asyncSlotStates[idx] = NULL; }
            if (m_asyncSlotOffsets[idx] != NULL) { delete[] m_asyncSlotOffsets[idx]; m_asyncSlotOffsets[idx] = NULL; }
            if (m_asyncSlotRefCounts[idx] != NULL) { delete[] m_asyncSlotRefCounts[idx]; m_asyncSlotRefCounts[idx] = NULL; }
            if (m_asyncRingRawBufs[idx] != NULL) {
                bufferPoolFreePrefetchRing(getBufferPool(), m_asyncRingRawBufs[idx]);
                m_asyncRingRawBufs[idx] = NULL;
            }
            return CACHE_ERR_NO_MEMORY;
        }

        uintptr_t baseAddr = ((uintptr_t)m_asyncRingRawBufs[idx] + 63u) & ~(uintptr_t)63u;
        uint8_t *basePtr = (uint8_t *)baseAddr;
        for (uint32_t slot = 0; slot < m_asyncBlockCounts[idx] + 1u; ++slot) {
            m_asyncSlotPtrs[idx][slot] = basePtr + slot * m_asyncBlockSizes[idx];
            m_asyncSlotValidBytes[idx][slot] = -1;
            m_asyncSlotStates[idx][slot] = CACHE_BLOCK_EMPTY;
            m_asyncSlotOffsets[idx][slot] = -1;
            m_asyncSlotRefCounts[idx][slot] = 0;
        }
    }

    m_asyncInitialized = 1;
    return 0;
}

/* 0x81203ED0 */
CacheIoEngine::CacheIoEngine(uint8_t syncStreamCount, const uint32_t *syncBlockSizes,
                             uint8_t asyncStreamCount, const AsyncRingConfig *asyncConfigs,
                             uint8_t enableWorkers, int threadPriority,
                             void *userCtx, void *extraCtx)
    : m_syncStreamCount(syncStreamCount)
    , m_syncBlockSizes(NULL)
    , m_syncRawBuffers(NULL)
    , m_syncFirstHalves(NULL)
    , m_syncSecondHalves(NULL)
    , m_syncFirstOffsets(NULL)
    , m_syncSecondOffsets(NULL)
    , m_syncFirstValidBytes(NULL)
    , m_syncSecondValidBytes(NULL)
    , m_asyncStreamCount(asyncStreamCount)
    , m_asyncBlockSizes(NULL)
    , m_asyncBlockCounts(NULL)
    , m_asyncWindowStarts(NULL)
    , m_asyncWindowEnds(NULL)
    , m_asyncLastReadPtrs(NULL)
    , m_asyncLastSlots(NULL)
    , m_asyncSlotPtrs(NULL)
    , m_asyncSlotValidBytes(NULL)
    , m_asyncSlotStates(NULL)
    , m_asyncSlotOffsets(NULL)
    , m_asyncSlotRefCounts(NULL)
    , m_asyncRingRawBufs(NULL)
    , m_workerPriority(threadPriority)
    , m_closingStream(0)
    , m_asyncWakeEvents(NULL)
    , m_asyncLocks(NULL)
    , m_asyncThreadIds(NULL)
    , m_asyncWakeRequested(NULL)
    , m_asyncWorkerExiting(NULL)
    , m_asyncWorkerBusy(NULL)
    , m_fileSeekLock("SceShellMusicCoreCacheIoFileSeekLock")
    , m_streamPath(NULL)
    , m_streamHandle(NULL)
    , m_padFileSize(0)
    , m_streamSize(-1)
    , m_directionMode(CACHE_DIR_NONE)
    , m_forcePrefetch(0)
    , m_syncInitialized(0)
    , m_asyncInitialized(0)
    , m_workersInitialized(0)
    , m_isInitialized(0)
    , m_openCb(NULL)
    , m_closeCb(NULL)
    , m_seekCb(NULL)
    , m_readCb(NULL)
    , m_statSizeCb(NULL)
    , m_errCb(NULL)
    , m_callbacksSet(0)
    , m_lastSyncError(-1)
    , m_lastAsyncError(-1)
    , m_userCtx(userCtx)
    , m_extraCtx(extraCtx)
{
    if (initAsyncCaches(asyncConfigs) != 0 || initSyncCaches(syncBlockSizes) != 0) {
        destroyAsyncCaches();
        destroySyncCaches();
        return;
    }

    if (m_asyncStreamCount != 0 && enableWorkers != 0 && initAsyncWorkers(asyncConfigs) != 0) {
        destroyAsyncWorkers();
        destroyAsyncCaches();
        destroySyncCaches();
        return;
    }

    m_isInitialized = 1;
}

/* 0x81204046 / 0x8120407C */
CacheIoEngine::~CacheIoEngine()
{
    destroyAsyncWorkers();
    destroyAsyncCaches();
    destroySyncCaches();
    m_isInitialized = 0;
}

/* 0x812040B6 */
int CacheIoEngine::setCallbacks(StreamOpenCallback openCb, StreamCloseCallback closeCb,
                                StreamSeekCallback seekCb, StreamReadCallback readCb,
                                StreamStatSizeCallback statSizeCb, StreamErrorCallback errCb)
{
    if (openCb == NULL || closeCb == NULL || seekCb == NULL ||
        readCb == NULL || statSizeCb == NULL || errCb == NULL) {
        return CACHE_ERR_INVALID_ARG;
    }
    if (m_streamHandle != NULL)
        return CACHE_ERR_ALREADY_OPEN;

    if (m_syncInitialized != 0) {
        for (uint8_t idx = 0; idx < m_syncStreamCount; ++idx) {
            m_syncFirstOffsets[idx] = -1;
            m_syncSecondOffsets[idx] = -1;
            m_syncFirstValidBytes[idx] = -1;
            m_syncSecondValidBytes[idx] = -1;
        }
    }
    if (m_asyncInitialized != 0) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            m_asyncWindowStarts[idx] = -1;
            m_asyncWindowEnds[idx] = -1;
            for (uint32_t slot = 0; slot < m_asyncBlockCounts[idx] + 1u; ++slot) {
                m_asyncSlotValidBytes[idx][slot] = -1;
                m_asyncSlotStates[idx][slot] = CACHE_BLOCK_EMPTY;
                m_asyncSlotOffsets[idx][slot] = -1;
                m_asyncSlotRefCounts[idx][slot] = 0;
            }
        }
    }

    m_openCb = openCb;
    m_closeCb = closeCb;
    m_seekCb = seekCb;
    m_readCb = readCb;
    m_statSizeCb = statSizeCb;
    m_errCb = errCb;
    m_callbacksSet = 1;
    return 0;
}

/* 0x81204252 */
int CacheIoEngine::openStream(void *streamHandle)
{
    if (m_callbacksSet == 0)
        return CACHE_ERR_NO_CALLBACKS;

    uint32_t fileSize = 0;
    int ret = m_statSizeCb(m_streamPath, &fileSize);
    if (ret < 0) {
        m_streamSize = -1;
        return ret;
    }

    m_streamSize = (int64_t)fileSize;
    if (m_openCb(m_streamPath, &streamHandle, m_userCtx, m_extraCtx) != 0)
        return CACHE_ERR_OPEN_FAILED;

    m_streamHandle = streamHandle;
    return 0;
}

/* 0x812042EA */
int CacheIoEngine::closeStream()
{
    if (m_callbacksSet == 0)
        return CACHE_ERR_NO_CALLBACKS;

    if (m_workersInitialized != 0) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            m_asyncLocks[idx].lock();
            m_closingStream = 1;
            while (m_asyncWorkerBusy[idx] != 0) {
                m_asyncWakeEvents[idx].signal(1u);
                m_asyncWakeEvents[idx].wait(&m_asyncLocks[idx], 16u, NULL);
            }
        }
    }

    int closeRet = -1;
    if (m_streamHandle != NULL)
        closeRet = m_closeCb(m_streamHandle, m_userCtx, m_extraCtx);

    if (m_workersInitialized != 0) {
        m_closingStream = 0;
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            m_asyncLocks[idx].unlock();
        }
    }

    if (closeRet < 0)
        return CACHE_ERR_CLOSE_FAILED;
    m_streamHandle = NULL;
    return 0;
}

/* 0x812043C8 */
int CacheIoEngine::getStreamSize(int64_t *outSize) const
{
    *outSize = m_streamSize;
    return 0;
}

/* 0x812043D4 */
int CacheIoEngine::getStreamPath(const char **outPath) const
{
    *outPath = m_streamPath;
    return 0;
}

/* 0x812043DE */
int CacheIoEngine::setStreamPath(const char *path)
{
    if (m_streamHandle != NULL)
        return CACHE_ERR_ALREADY_OPEN;

    if (m_syncInitialized != 0) {
        for (uint8_t idx = 0; idx < m_syncStreamCount; ++idx) {
            m_syncFirstOffsets[idx] = -1;
            m_syncSecondOffsets[idx] = -1;
            m_syncFirstValidBytes[idx] = -1;
            m_syncSecondValidBytes[idx] = -1;
        }
    }
    if (m_asyncInitialized != 0) {
        for (uint8_t idx = 0; idx < m_asyncStreamCount; ++idx) {
            m_asyncWindowStarts[idx] = -1;
            m_asyncWindowEnds[idx] = -1;
            for (uint32_t slot = 0; slot < m_asyncBlockCounts[idx] + 1u; ++slot) {
                m_asyncSlotValidBytes[idx][slot] = -1;
                m_asyncSlotStates[idx][slot] = CACHE_BLOCK_EMPTY;
                m_asyncSlotOffsets[idx][slot] = -1;
                m_asyncSlotRefCounts[idx][slot] = 0;
            }
        }
    }

    m_streamPath = path;
    return 0;
}

/* 0x81204518 */
int CacheIoEngine::setDirectionHint(int directionMode, int forcePrefetch)
{
    if (directionMode >= 3)
        return CACHE_ERR_INVALID_ARG;
    m_directionMode = directionMode;
    m_forcePrefetch = forcePrefetch;
    return 0;
}

int CacheIoEngine::loadSyncBlock(uint8_t slotIdx, bool useSecondHalf, int64_t alignedOffset,
                                 uint8_t *outDidIo, uint32_t *outAvail, int64_t reqOffset)
{
    uint8_t *dstBuf = useSecondHalf ? m_syncSecondHalves[slotIdx] : m_syncFirstHalves[slotIdx];
    int64_t *dstOffset = useSecondHalf ? &m_syncSecondOffsets[slotIdx] : &m_syncFirstOffsets[slotIdx];
    int *dstValid = useSecondHalf ? &m_syncSecondValidBytes[slotIdx] : &m_syncFirstValidBytes[slotIdx];

    int foundStream = -1;
    bool foundSecondHalf = false;
    for (uint8_t probe = 0; probe < m_syncStreamCount; ++probe) {
        if (m_syncFirstOffsets[probe] == alignedOffset) {
            foundStream = probe;
            foundSecondHalf = false;
            break;
        }
        if (m_syncSecondOffsets[probe] == alignedOffset) {
            foundStream = probe;
            foundSecondHalf = true;
            break;
        }
    }

    int bytesRead = 0;
    if (foundStream >= 0) {
        const uint8_t *srcBuf = foundSecondHalf ? m_syncSecondHalves[foundStream] : m_syncFirstHalves[foundStream];
        bytesRead = foundSecondHalf ? m_syncSecondValidBytes[foundStream] : m_syncFirstValidBytes[foundStream];
        sce_paf_memcpy(dstBuf, srcBuf, (SceSize)bytesRead);
    } else {
        m_fileSeekLock.lock();
        int64_t seekPos = m_seekCb(m_streamHandle, alignedOffset, 0, m_userCtx, m_extraCtx);
        if (seekPos < 0) {
            m_fileSeekLock.unlock();
            if (outAvail != NULL)
                *outAvail = 0;
            return (int)seekPos;
        }
        *outDidIo = 1;
        bytesRead = m_readCb(m_streamHandle, dstBuf, m_syncBlockSizes[slotIdx], 1, m_userCtx, m_extraCtx);
        m_fileSeekLock.unlock();
        if (bytesRead < 0) {
            *dstOffset = -1;
            *dstValid = -1;
            return bytesRead;
        }
    }

    *dstOffset = alignedOffset;
    *dstValid = bytesRead;
    (void)reqOffset;
    return bytesRead;
}

/* 0x81204534 */
int CacheIoEngine::readSync(uint8_t streamId, int64_t offset, uint32_t length,
                            void **outPtr, uint8_t *outDidIo, uint32_t *outAvail)
{
    if (m_callbacksSet == 0)
        return CACHE_ERR_NO_CALLBACKS;
    if (streamId == 0)
        return CACHE_ERR_INVALID_ARG;
    if (streamId > m_syncStreamCount) {
        *outPtr = NULL;
        return CACHE_ERR_INVALID_ARG;
    }
    if (m_syncInitialized == 0) {
        *outPtr = NULL;
        return CACHE_ERR_NO_MEMORY;
    }

    uint8_t slotIdx = (uint8_t)(streamId - 1);
    *outDidIo = 0;
    uint32_t blockSize = m_syncBlockSizes[slotIdx];
    if (length > blockSize) {
        *outPtr = NULL;
        return CACHE_ERR_INVALID_ARG;
    }
    if (m_streamHandle == NULL) {
        *outPtr = NULL;
        return CACHE_ERR_NOT_OPEN;
    }
    if (offset < 0 || (m_streamSize > 0 && offset > m_streamSize)) {
        *outPtr = NULL;
        return CACHE_ERR_EOF;
    }

    int64_t alignedOffset = (int64_t)((uint32_t)offset & ~(blockSize - 1u));
    int64_t nextBlockOffset = alignedOffset + (int64_t)blockSize;
    if (nextBlockOffset < 0) {
        *outPtr = NULL;
        return CACHE_ERR_INVALID_ARG;
    }

    int64_t reqEnd = offset + (int64_t)length;
    int offsetInBlock = (int)(offset - alignedOffset);
    int overflowBytes = (reqEnd > nextBlockOffset) ? (int)(length - (blockSize - (uint32_t)offsetInBlock)) : 0;

    bool firstHit = (m_syncFirstOffsets[slotIdx] == alignedOffset);
    bool secondHit = false;

    if (overflowBytes > 0) {
        secondHit = (m_syncSecondOffsets[slotIdx] == nextBlockOffset);
        if (firstHit && secondHit) {
            if ((uint32_t)m_syncFirstValidBytes[slotIdx] != blockSize) {
                *outPtr = NULL;
                return -1;
            }
            if (m_syncSecondValidBytes[slotIdx] >= overflowBytes) {
                *outPtr = m_syncFirstHalves[slotIdx] + offsetInBlock;
                return 0;
            }
            *outPtr = NULL;
            if (outAvail != NULL)
                *outAvail = length - (uint32_t)(overflowBytes - m_syncSecondValidBytes[slotIdx]);
            return CACHE_ERR_EOF;
        }

        if (!firstHit) {
            int firstRead = loadSyncBlock(slotIdx, false, alignedOffset, outDidIo, outAvail, offset);
            if (firstRead < 0) {
                *outPtr = NULL;
                return firstRead;
            }
            if ((uint32_t)firstRead < blockSize) {
                *outPtr = NULL;
                if (outAvail != NULL)
                    *outAvail = (uint32_t)(alignedOffset + firstRead - offset);
                return CACHE_ERR_EOF;
            }
        }

        if (!secondHit) {
            int secondRead = loadSyncBlock(slotIdx, true, nextBlockOffset, outDidIo, outAvail, offset);
            if (secondRead < 0) {
                *outPtr = NULL;
                return secondRead;
            }
            if (secondRead < overflowBytes) {
                *outPtr = NULL;
                if (outAvail != NULL)
                    *outAvail = (uint32_t)(nextBlockOffset + secondRead - offset);
                return CACHE_ERR_EOF;
            }
        }

        *outPtr = m_syncFirstHalves[slotIdx] + (offset - m_syncFirstOffsets[slotIdx]);
        return 0;
    }

    if (firstHit) {
        int64_t availEnd = m_syncFirstOffsets[slotIdx] + m_syncFirstValidBytes[slotIdx];
        if (reqEnd <= availEnd) {
            *outPtr = m_syncFirstHalves[slotIdx] + offsetInBlock;
            return 0;
        }
        *outPtr = NULL;
        if (outAvail != NULL)
            *outAvail = (uint32_t)(availEnd - offset);
        return CACHE_ERR_EOF;
    }

    if (m_syncSecondOffsets[slotIdx] == alignedOffset) {
        int64_t availEnd = m_syncSecondOffsets[slotIdx] + m_syncSecondValidBytes[slotIdx];
        if (reqEnd <= availEnd) {
            *outPtr = m_syncSecondHalves[slotIdx] + offsetInBlock;
            return 0;
        }
        *outPtr = NULL;
        if (outAvail != NULL)
            *outAvail = (uint32_t)(availEnd - offset);
        return CACHE_ERR_EOF;
    }

    if (m_directionMode < 0 || m_directionMode > 2) {
        m_fileSeekLock.unlock();
        *outPtr = NULL;
        return -1;
    }

    bool targetSecondHalf = false;
    if (m_directionMode == CACHE_DIR_REVERSE) {
        targetSecondHalf = !(m_syncSecondOffsets[slotIdx] >= 0 &&
                             alignedOffset == m_syncSecondOffsets[slotIdx] - (int64_t)blockSize);
    } else {
        targetSecondHalf = (m_syncFirstOffsets[slotIdx] >= 0 &&
                            alignedOffset == m_syncFirstOffsets[slotIdx] + (int64_t)blockSize);
    }

    int bytesRead = loadSyncBlock(slotIdx, targetSecondHalf, alignedOffset, outDidIo, outAvail, offset);
    if (bytesRead < 0) {
        *outPtr = NULL;
        return bytesRead;
    }

    int64_t validEnd = alignedOffset + bytesRead;
    if (validEnd >= reqEnd) {
        uint8_t *buf = targetSecondHalf ? m_syncSecondHalves[slotIdx] : m_syncFirstHalves[slotIdx];
        *outPtr = buf + offsetInBlock;
        return 0;
    }

    *outPtr = NULL;
    if (outAvail != NULL)
        *outAvail = (uint32_t)(validEnd - offset);
    return CACHE_ERR_EOF;
}

/* 0x812052C2 */
int CacheIoEngine::findBlockSlot(int startUnused, int64_t targetOffset, int stateMask,
                                 int slotCount, int directionMode,
                                 const int64_t *slotOffsets, const int *slotStates,
                                 int startIndex, int *outErr)
{
    (void)startUnused;
    if (slotOffsets == NULL || slotStates == NULL || slotCount <= 0 || startIndex >= slotCount) {
        *outErr = CACHE_ERR_INVALID_ARG;
        return -1;
    }

    int idx = startIndex;
    for (int checked = 0; checked < slotCount; ++checked) {
        bool offsetMatches = (targetOffset == -1 || slotOffsets[idx] == targetOffset);
        if (offsetMatches && (slotStates[idx] & stateMask) != 0)
            return idx;

        if (directionMode == CACHE_DIR_REVERSE) {
            if (--idx <= 0)
                idx = slotCount - 1;
        } else {
            if (++idx >= slotCount)
                idx = 0;
        }
    }
    return -1;
}

/* 0x81205384 */
int CacheIoEngine::prepareBlockSlot(int slotIndex, int64_t targetOffset, uint32_t blockSize,
                                    int slotCount, int directionMode,
                                    uint8_t **slotBuffers, int64_t *slotOffsets, int *slotStates,
                                    int *slotRefCounts, int *slotValidBytes,
                                    int protectedSlotA, int protectedSlotB,
                                    uint8_t syncReadFlag, uint8_t *outNeedsLoad)
{
    if (slotIndex == -1 || slotIndex >= slotCount)
        return 0;

    int curState = slotStates[slotIndex];
    switch (curState) {
    case CACHE_BLOCK_SYNC_LOAD:
    case CACHE_BLOCK_ASYNC_LOAD:
        return (slotOffsets[slotIndex] == targetOffset) ? 0 : -1;

    case CACHE_BLOCK_READY:
        if (slotOffsets[slotIndex] == targetOffset)
            return 0;
        if (slotRefCounts[slotIndex] != 0 || slotIndex == protectedSlotA || slotIndex == protectedSlotB)
            return -1;
        break;

    case CACHE_BLOCK_EMPTY:
        break;

    default:
        return 0;
    }

    int searchErr = 0;
    int existingSlot = findBlockSlot(0, targetOffset, CACHE_BLOCK_READY, slotCount, directionMode,
                                     slotOffsets, slotStates, 0, &searchErr);
    if (existingSlot != -1) {
        sce_paf_memcpy(slotBuffers[slotIndex], slotBuffers[existingSlot], blockSize);
        slotValidBytes[slotIndex] = slotValidBytes[existingSlot];
        slotRefCounts[slotIndex] = 0;
        slotOffsets[slotIndex] = slotOffsets[existingSlot];
        slotStates[slotIndex] = CACHE_BLOCK_READY;
        return 0;
    }

    *outNeedsLoad = 1;
    slotValidBytes[slotIndex] = -1;
    slotOffsets[slotIndex] = targetOffset;
    slotRefCounts[slotIndex] = 0;
    slotStates[slotIndex] = (syncReadFlag == 1) ? CACHE_BLOCK_SYNC_LOAD : CACHE_BLOCK_ASYNC_LOAD;
    return 0;
}

/* 0x8120550C */
uint64_t CacheIoEngine::scoreCandidateSlot(int candidateIdx, int64_t firstOffset, uint8_t spansTwoBlocks,
                                           int directionMode, int lastUsedSlot, uint32_t blockSize,
                                           int usableSlots, int64_t *slotOffsets,
                                           const int *slotStates, const int *slotRefCounts,
                                           int protectedSlotA, int protectedSlotB,
                                           int runStart, int runLength,
                                           int64_t windowStart, int64_t windowEnd)
{
    if (candidateIdx < 0 || usableSlots <= 0 || candidateIdx >= usableSlots ||
        slotOffsets == NULL || slotStates == NULL || slotRefCounts == NULL) {
        return 0;
    }

    int64_t secondOffset = -1;
    int nextIdx = -1;
    if (spansTwoBlocks == 1) {
        secondOffset = firstOffset + (int64_t)blockSize;
        nextIdx = candidateIdx + 1;
    }

    int runEnd = (runLength > 0) ? (runStart + runLength - 1) : runStart;
    int64_t existingOffset = slotOffsets[candidateIdx];

    uint64_t firstScore = 0;
    if (existingOffset == firstOffset) {
        switch (slotStates[candidateIdx]) {
        case CACHE_BLOCK_EMPTY:
            slotOffsets[candidateIdx] = -1;
            firstScore = 0x0200000000000000ULL;
            break;
        case CACHE_BLOCK_READY:
            firstScore = 0x0400000000000000ULL;
            break;
        case CACHE_BLOCK_SYNC_LOAD:
        case CACHE_BLOCK_ASYNC_LOAD:
            firstScore = 0x0300000000000000ULL;
            break;
        default:
            return 0;
        }
    } else if (slotStates[candidateIdx] == CACHE_BLOCK_READY) {
        if (slotRefCounts[candidateIdx] != 0 || candidateIdx == protectedSlotA || candidateIdx == protectedSlotB)
            return 0;
        firstScore = 0x0100000000000000ULL;
    } else if (slotStates[candidateIdx] == CACHE_BLOCK_EMPTY) {
        firstScore = 0x0200000000000000ULL;
    } else {
        return 0;
    }

    uint64_t secondScore = 0x0004000000000000ULL;
    if (spansTwoBlocks == 1) {
        switch (slotStates[nextIdx]) {
        case CACHE_BLOCK_EMPTY:
            secondScore = 0x0002000000000000ULL;
            break;
        case CACHE_BLOCK_READY:
            if (slotOffsets[nextIdx] == secondOffset) {
                secondScore = 0x0005000000000000ULL;
            } else if (slotRefCounts[nextIdx] == 0 && nextIdx != protectedSlotA && nextIdx != protectedSlotB) {
                secondScore = 0x0001000000000000ULL;
            } else {
                return 0;
            }
            break;
        case CACHE_BLOCK_SYNC_LOAD:
        case CACHE_BLOCK_ASYNC_LOAD:
            if (slotOffsets[nextIdx] != secondOffset)
                return 0;
            secondScore = 0x0003000000000000ULL;
            break;
        default:
            return 0;
        }
    }

    uint64_t evictionScore = 0;
    if (existingOffset == -1) {
        evictionScore = 0x0000000300000000ULL;
    } else if (directionMode == CACHE_DIR_REVERSE) {
        if (existingOffset < windowStart)
            evictionScore = 0x0000000100000000ULL;
        else if (existingOffset > windowEnd)
            evictionScore = 0x0000000200000000ULL;
    } else {
        if (existingOffset < windowStart)
            evictionScore = 0x0000000200000000ULL;
        else if (existingOffset > windowEnd)
            evictionScore = 0x0000000100000000ULL;
    }

    uint64_t alignmentScore = 0;
    if (runStart == -1) {
        alignmentScore = 0x0000010000000000ULL;
    } else if (candidateIdx < runStart) {
        uint32_t delta = (uint32_t)(runStart - candidateIdx) * blockSize;
        if ((int64_t)delta <= slotOffsets[runStart] && firstOffset == slotOffsets[runStart] - (int64_t)delta)
            alignmentScore = 0x0000020000000000ULL;
        else
            alignmentScore = 0x0000010000000000ULL;
    } else if (candidateIdx > runEnd) {
        int64_t delta = (int64_t)(candidateIdx - runEnd) * (int64_t)blockSize;
        if (slotOffsets[runEnd] + delta < m_streamSize && firstOffset == slotOffsets[runEnd] + delta)
            alignmentScore = 0x0000020000000000ULL;
        else
            alignmentScore = 0x0000010000000000ULL;
    }

    uint64_t distanceScore = 0;
    uint32_t stepDist = 0;
    if (lastUsedSlot != -1 && candidateIdx != lastUsedSlot) {
        if (directionMode == CACHE_DIR_REVERSE) {
            if (lastUsedSlot > candidateIdx) {
                distanceScore = 0x01000000ULL;
                stepDist = (uint32_t)(lastUsedSlot - candidateIdx);
            } else {
                distanceScore = 0x02000000ULL;
                stepDist = (uint32_t)(candidateIdx - lastUsedSlot);
            }
        } else {
            if (lastUsedSlot > candidateIdx) {
                distanceScore = 0x02000000ULL;
                stepDist = (uint32_t)(lastUsedSlot - candidateIdx);
            } else {
                distanceScore = 0x01000000ULL;
                stepDist = (uint32_t)(candidateIdx - lastUsedSlot);
            }
        }
        if (stepDist >= 256u)
            stepDist = 255u;
    }

    return firstScore + secondScore + evictionScore + alignmentScore +
           distanceScore + ((uint64_t)stepDist << 16) + 0xFFFFULL;
}

/* 0x81205926 */
int CacheIoEngine::findLongestContiguousRun(int slotCount, const int64_t *slotOffsets, uint32_t blockSize,
                                            int64_t windowStart, int64_t windowEnd, int *outRunLength)
{
    if (windowStart == -1 || windowEnd == -1 || slotCount <= 0 || slotOffsets == NULL || blockSize == 0) {
        *outRunLength = 0;
        return -1;
    }

    int bestLen = 0;
    int bestStart = -1;
    int curStart = -1;
    int curLen = 0;
    int64_t expectedOffset = slotOffsets[0];

    for (int idx = 0; idx < slotCount; ++idx) {
        int64_t off = slotOffsets[idx];
        if (expectedOffset == -1 || expectedOffset < windowStart || expectedOffset > windowEnd || expectedOffset != off) {
            if (curLen > bestLen) {
                bestLen = curLen;
                bestStart = curStart;
            }
            if (off == -1 || off < windowStart || off > windowEnd) {
                curStart = -1;
                curLen = 0;
                expectedOffset = (idx + 1 < slotCount) ? slotOffsets[idx + 1] : -1;
            } else {
                curStart = idx;
                expectedOffset = off + (int64_t)blockSize;
                curLen = 1;
            }
        } else {
            if (curStart == -1)
                curStart = idx;
            ++curLen;
            expectedOffset += (int64_t)blockSize;
        }
    }

    if (bestStart != -1)
        *outRunLength = bestLen;
    return bestStart;
}

/* 0x81205A82 */
int CacheIoEngine::fetchAsyncBlocks(uint8_t streamId, int64_t offset, uint32_t length, uint8_t pinBlocks,
                                    void **outPtr, PrefetchPinToken **outToken,
                                    uint8_t *outWokeWorker, int *outErr)
{
    if (streamId > m_asyncStreamCount || outPtr == NULL || (pinBlocks == 1 && outToken == NULL)) {
        *outErr = CACHE_ERR_INVALID_ARG;
        return -1;
    }

    uint8_t idx = (uint8_t)(streamId - 1);
    SyncMutex *ringLock = &m_asyncLocks[idx];
    ringLock->lock();

    uint32_t blockSize = m_asyncBlockSizes[idx];
    uint32_t blockCount = m_asyncBlockCounts[idx];
    int totalSlots = (int)(blockCount + 1u);
    uint8_t **slotPtrs = m_asyncSlotPtrs[idx];
    int *slotValidBytes = m_asyncSlotValidBytes[idx];
    int *slotStates = m_asyncSlotStates[idx];
    int64_t *slotOffsets = m_asyncSlotOffsets[idx];
    int *slotRefCounts = m_asyncSlotRefCounts[idx];

    SyncEvent *wakeEvent = (m_workersInitialized != 0) ? &m_asyncWakeEvents[idx] : NULL;
    uint8_t *wakeReqFlag = (m_workersInitialized != 0) ? &m_asyncWakeRequested[idx] : NULL;

    int64_t firstOffset = (int64_t)((uint32_t)offset & ~(blockSize - 1u));
    int64_t secondOffset = -1;
    uint8_t spansTwoBlocks = 0;
    int overflowBytes = 0;
    if (firstOffset + (int64_t)blockSize < offset + (int64_t)length) {
        secondOffset = firstOffset + (int64_t)blockSize;
        overflowBytes = (int)(length - (blockSize - (uint32_t)(offset - firstOffset)));
        spansTwoBlocks = 1;
    }

    bool prefetchEnabled = (m_directionMode != CACHE_DIR_NONE && wakeReqFlag != NULL && wakeEvent != NULL);
    if (!prefetchEnabled) {
        int clearIdx = 0;
        int searchErr = 0;
        while (clearIdx < totalSlots) {
            int found = findBlockSlot(0, -1, CACHE_BLOCK_ASYNC_LOAD, totalSlots, 0,
                                      slotOffsets, slotStates, clearIdx, &searchErr);
            if (found == -1)
                break;
            slotOffsets[clearIdx] = -1;
            slotStates[clearIdx] = CACHE_BLOCK_EMPTY;
            clearIdx = found + 1;
        }
    }

    uint8_t needAsyncWake = 0;
    uint8_t needSyncLoad = 0;
    bool useSyncLoad = (!prefetchEnabled || m_asyncLastSlots[idx] == -1);
    uint8_t *primaryLoadFlag = useSyncLoad ? &needSyncLoad : &needAsyncWake;
    bool windowUpdated = false;

    if (prefetchEnabled) {
        uint32_t aheadBlocks = useSyncLoad ? ((2u * blockCount) / 6u) : ((2u * blockCount) / 3u);
        if (m_directionMode == CACHE_DIR_REVERSE) {
            int64_t curEnd = m_asyncWindowEnds[idx];
            int64_t threshold = (curEnd == -1 || curEnd < (int64_t)((aheadBlocks >> 1) * blockSize))
                                    ? firstOffset
                                    : (curEnd - (int64_t)((aheadBlocks >> 1) * blockSize));
            if (m_asyncWindowStarts[idx] == -1 || curEnd == -1 ||
                firstOffset < m_asyncWindowStarts[idx] || firstOffset > curEnd || firstOffset <= threshold) {
                int64_t newEnd = ((int64_t)((uint32_t)firstOffset + 3u * blockSize) < m_streamSize)
                                     ? (int64_t)((uint32_t)firstOffset + 3u * blockSize)
                                     : (int64_t)((uint32_t)m_streamSize & ~(blockSize - 1u));
                int64_t newStart = ((uint32_t)firstOffset > aheadBlocks * blockSize)
                                       ? (int64_t)((uint32_t)firstOffset - aheadBlocks * blockSize)
                                       : 0;
                if (newStart != m_asyncWindowStarts[idx] || newEnd != m_asyncWindowEnds[idx]) {
                    m_asyncWindowStarts[idx] = newStart;
                    m_asyncWindowEnds[idx] = newEnd;
                    windowUpdated = true;
                }
            }
        } else {
            int64_t anchorOffset = (spansTwoBlocks == 1) ? secondOffset : firstOffset;
            int64_t curStart = m_asyncWindowStarts[idx];
            int64_t threshold = (curStart == -1) ? anchorOffset : (curStart + (int64_t)((aheadBlocks >> 1) * blockSize));
            if (curStart == -1 || m_asyncWindowEnds[idx] == -1 ||
                anchorOffset < curStart || anchorOffset > m_asyncWindowEnds[idx] || anchorOffset >= threshold) {
                int64_t newStart = ((uint32_t)anchorOffset > 3u * blockSize)
                                       ? (int64_t)((uint32_t)anchorOffset - 3u * blockSize)
                                       : 0;
                uint32_t endCand = (uint32_t)anchorOffset + aheadBlocks * blockSize;
                int64_t newEnd = ((int64_t)endCand < m_streamSize)
                                     ? (int64_t)endCand
                                     : (int64_t)((uint32_t)m_streamSize & ~(blockSize - 1u));
                if (newStart != m_asyncWindowStarts[idx] || newEnd != m_asyncWindowEnds[idx]) {
                    m_asyncWindowStarts[idx] = newStart;
                    m_asyncWindowEnds[idx] = newEnd;
                    windowUpdated = true;
                }
            }
        }
    } else {
        m_asyncWindowStarts[idx] = -1;
        m_asyncWindowEnds[idx] = -1;
    }

    int runLen = 0;
    int runStart = findLongestContiguousRun(totalSlots, slotOffsets, blockSize,
                                            m_asyncWindowStarts[idx], m_asyncWindowEnds[idx], &runLen);

    int chosenFirst = -1;
    uint64_t bestScore = 0;
    int probeSlot = (m_asyncLastSlots[idx] == -1) ? 0 : m_asyncLastSlots[idx];
    int stepDir = (m_directionMode == CACHE_DIR_REVERSE) ? -1 : 1;
    for (uint32_t checked = 0; checked < blockCount; ++checked) {
        uint64_t score = scoreCandidateSlot(probeSlot, firstOffset, spansTwoBlocks, m_directionMode,
                                            m_asyncLastSlots[idx], blockSize, (int)blockCount,
                                            slotOffsets, slotStates, slotRefCounts,
                                            -1, -1, runStart, runLen,
                                            m_asyncWindowStarts[idx], m_asyncWindowEnds[idx]);
        if (score != 0 && score > bestScore) {
            bestScore = score;
            chosenFirst = probeSlot;
            if ((score >> 32) >= 0x04030000ULL)
                break;
        }
        if (stepDir < 0)
            probeSlot = (probeSlot <= 0) ? (int)(blockCount - 1u) : (probeSlot - 1);
        else
            probeSlot = (probeSlot + 1 >= (int)blockCount) ? 0 : (probeSlot + 1);
    }

    int chosenSecond = (spansTwoBlocks == 1 && chosenFirst != -1) ? (chosenFirst + 1) : -1;

    int prepErr = prepareBlockSlot(chosenFirst, firstOffset, blockSize, totalSlots, m_directionMode,
                                   slotPtrs, slotOffsets, slotStates, slotRefCounts, slotValidBytes,
                                   chosenFirst, chosenSecond, useSyncLoad ? 1 : 0, primaryLoadFlag);
    if (prepErr == 0 && spansTwoBlocks == 1) {
        prepErr = prepareBlockSlot(chosenSecond, secondOffset, blockSize, totalSlots, m_directionMode,
                                   slotPtrs, slotOffsets, slotStates, slotRefCounts, slotValidBytes,
                                   chosenFirst, chosenSecond, useSyncLoad ? 1 : 0, primaryLoadFlag);
    }
    if (prepErr != 0) {
        *outErr = prepErr;
        ringLock->unlock();
        return -1;
    }

    /* Schedule background prefetch slots across the active prefetch window */
    if (prefetchEnabled && m_asyncWindowStarts[idx] != -1 && windowUpdated) {
        if (m_directionMode == CACHE_DIR_REVERSE) {
            int anchor = (runStart != -1) ? runStart : chosenFirst;
            if (anchor >= 0 && anchor < totalSlots) {
                int64_t nextPrefetchOff = (slotOffsets[anchor] != -1 && slotOffsets[anchor] >= (int64_t)blockSize)
                                              ? (slotOffsets[chosenFirst] - (int64_t)blockSize)
                                              : -1;
                int slotCursor = -1 - anchor;
                while (slotCursor >= 0 && nextPrefetchOff != -1 && nextPrefetchOff >= m_asyncWindowStarts[idx]) {
                    if (prepareBlockSlot(slotCursor, nextPrefetchOff, blockSize, totalSlots, m_directionMode,
                                         slotPtrs, slotOffsets, slotStates, slotRefCounts, slotValidBytes,
                                         chosenFirst, chosenSecond, 0, &needAsyncWake) != 0) {
                        break;
                    }
                    nextPrefetchOff = (nextPrefetchOff >= (int64_t)blockSize)
                                          ? (nextPrefetchOff - (int64_t)blockSize)
                                          : -1;
                    --slotCursor;
                }
            }
        } else {
            int anchor = (runStart != -1) ? (runStart + runLen - 1) : chosenFirst;
            if (anchor >= 0 && anchor < totalSlots) {
                int64_t nextPrefetchOff = (slotOffsets[anchor] != -1)
                                              ? (slotOffsets[anchor] + (int64_t)blockSize)
                                              : -1;
                int slotCursor = (anchor < (int)blockCount - 1) ? (anchor + 1) : 0;
                for (uint32_t scheduled = 0; scheduled < blockCount; ++scheduled) {
                    if (nextPrefetchOff == -1 || nextPrefetchOff > m_asyncWindowEnds[idx])
                        break;
                    if (prepareBlockSlot(slotCursor, nextPrefetchOff, blockSize, totalSlots, m_directionMode,
                                         slotPtrs, slotOffsets, slotStates, slotRefCounts, slotValidBytes,
                                         chosenFirst, chosenSecond, 0, &needAsyncWake) != 0) {
                        break;
                    }
                    nextPrefetchOff += (int64_t)blockSize;
                    if (nextPrefetchOff >= m_streamSize)
                        nextPrefetchOff = -1;
                    slotCursor = (slotCursor < (int)blockCount - 1) ? (slotCursor + 1) : 0;
                }
            }
        }
    }

    ringLock->unlock();

    /* Perform any synchronous loads flagged above */
    if (needSyncLoad == 1) {
        m_fileSeekLock.lock();
        for (int slot = 0; slot < totalSlots; ++slot) {
            if (slotStates[slot] != CACHE_BLOCK_SYNC_LOAD)
                continue;
            int64_t seekPos = m_seekCb(m_streamHandle, slotOffsets[slot], 0, m_userCtx, m_extraCtx);
            int readBytes = 0;
            if (seekPos >= 0) {
                readBytes = m_readCb(m_streamHandle, slotPtrs[slot], blockSize, 0, m_userCtx, m_extraCtx);
                if (readBytes < 0)
                    *outErr = readBytes;
            } else {
                *outErr = (int)seekPos;
            }

            if (seekPos >= 0 && readBytes > 0) {
                slotStates[slot] = CACHE_BLOCK_READY;
                slotValidBytes[slot] = readBytes;
            } else {
                slotStates[slot] = CACHE_BLOCK_EMPTY;
                slotValidBytes[slot] = -1;
                slotOffsets[slot] = -1;
            }
            slotRefCounts[slot] = 0;
        }
        m_fileSeekLock.unlock();
    }

    if ((needAsyncWake == 1 || m_forcePrefetch != 0) && prefetchEnabled) {
        *wakeReqFlag = 1;
        wakeEvent->signal(1u);
        *outWokeWorker = 1;
    }

    if (chosenFirst < 0 || chosenFirst >= totalSlots) {
        *outErr = -1;
        return -1;
    }

    uint64_t waitedUs = 0;
    while (slotStates[chosenFirst] == CACHE_BLOCK_ASYNC_LOAD) {
        waitedUs += 50000ULL;
        sceKernelDelayThread(50000);
        if (waitedUs >= 10000000ULL) {
            *outErr = -1;
            return -1;
        }
    }

    if (slotStates[chosenFirst] != CACHE_BLOCK_READY) {
        *outErr = (slotStates[chosenFirst] == CACHE_BLOCK_EMPTY) ? CACHE_ERR_EOF : -1;
        return -1;
    }

    int firstValid = slotValidBytes[chosenFirst];
    int64_t loadedFirstOffset = slotOffsets[chosenFirst];
    int secondValid = -1;

    if (spansTwoBlocks == 1 && (uint32_t)firstValid == blockSize) {
        if (chosenSecond < 0 || chosenSecond >= totalSlots) {
            *outErr = -1;
            if (length > 0)
                *outErr = CACHE_ERR_EOF;
            return 0;
        }
        uint64_t secondWaitedUs = 0;
        while (slotStates[chosenSecond] == CACHE_BLOCK_ASYNC_LOAD) {
            secondWaitedUs += 50000ULL;
            sceKernelDelayThread(50000);
            if (secondWaitedUs >= 10000000ULL) {
                *outErr = -1;
                return -1;
            }
        }
        if (slotStates[chosenSecond] != CACHE_BLOCK_READY) {
            *outErr = -1;
            if (length > 0)
                *outErr = CACHE_ERR_EOF;
            return 0;
        }
        secondValid = slotValidBytes[chosenSecond];
    }

    if (prefetchEnabled)
        m_asyncLastSlots[idx] = chosenFirst;

    if (loadedFirstOffset > offset || (uint32_t)(offset - loadedFirstOffset) >= blockSize) {
        *outErr = -1;
        return -1;
    }

    uint32_t relOffset = (uint32_t)(offset - loadedFirstOffset);
    *outErr = 0;
    *outPtr = slotPtrs[chosenFirst] + relOffset;
    uint32_t availBytes = (uint32_t)firstValid - relOffset;
    if (spansTwoBlocks == 1 && secondValid > 0) {
        availBytes += (uint32_t)((overflowBytes <= secondValid) ? overflowBytes : secondValid);
    }

    if (pinBlocks != 0) {
        PrefetchPinToken *token = (PrefetchPinToken *)musiccoreAllocFromPafHeap(sizeof(PrefetchPinToken), getLocalHeap());
        if (token != NULL) {
            token->firstSlot = chosenFirst;
            token->secondSlot = -1;
            ++slotRefCounts[chosenFirst];
            if (overflowBytes > 0 && secondValid > 0) {
                ++slotRefCounts[chosenSecond];
                token->secondSlot = chosenSecond;
            }
        }
        *outToken = token;
        if (token == NULL) {
            *outErr = CACHE_ERR_NO_MEMORY;
            return 0;
        }
    }

    if (availBytes < length)
        *outErr = CACHE_ERR_EOF;
    return (int)availBytes;
}

/* 0x812068D2 */
int CacheIoEngine::readAsync(uint8_t streamId, int64_t offset, uint32_t length, uint8_t pinBlocks,
                             void **outPtr, PrefetchPinToken **outToken,
                             uint8_t *outWokeWorker, uint32_t *outAvail)
{
    if (m_callbacksSet == 0)
        return CACHE_ERR_NO_CALLBACKS;
    if (streamId == 0 || streamId > m_asyncStreamCount || offset < 0)
        return CACHE_ERR_INVALID_ARG;
    if (m_streamHandle == NULL) {
        *outPtr = NULL;
        return CACHE_ERR_NOT_OPEN;
    }
    if (m_streamSize > 0 && offset > m_streamSize) {
        *outPtr = NULL;
        return CACHE_ERR_EOF;
    }
    if (m_asyncInitialized == 0) {
        *outPtr = NULL;
        return CACHE_ERR_NO_MEMORY;
    }
    if (pinBlocks != 0 && outToken == NULL) {
        *outPtr = NULL;
        return CACHE_ERR_INVALID_ARG;
    }

    uint8_t idx = (uint8_t)(streamId - 1);
    if (length > m_asyncBlockSizes[idx]) {
        *outPtr = NULL;
        return CACHE_ERR_INVALID_ARG;
    }

    int fetchErr = -1;
    int avail = fetchAsyncBlocks(streamId, offset, length, pinBlocks,
                                 &m_asyncLastReadPtrs[idx], outToken, outWokeWorker, &fetchErr);
    if (fetchErr != 0) {
        *outPtr = NULL;
        if (outAvail != NULL)
            *outAvail = 0;
        return fetchErr;
    }
    if ((uint32_t)avail >= length) {
        *outPtr = m_asyncLastReadPtrs[idx];
        return 0;
    }

    *outPtr = NULL;
    if (outAvail != NULL)
        *outAvail = (uint32_t)avail;
    return CACHE_ERR_EOF;
}

/* 0x81206A84 */
int CacheIoEngine::releaseAsyncPin(uint8_t streamId, PrefetchPinToken **ioToken)
{
    if (streamId == 0 || streamId > m_asyncStreamCount)
        return CACHE_ERR_INVALID_ARG;
    PrefetchPinToken *token = *ioToken;
    if (token == NULL)
        return CACHE_ERR_INVALID_ARG;
    if (m_asyncInitialized == 0)
        return CACHE_ERR_NO_MEMORY;

    uint8_t idx = (uint8_t)(streamId - 1);
    int *refCounts = m_asyncSlotRefCounts[idx];
    bool valid = true;
    if (token->firstSlot >= 0)
        valid = (refCounts[token->firstSlot] > 0);
    if (!valid || (token->secondSlot >= 0 && refCounts[token->secondSlot] <= 0))
        return CACHE_ERR_INVALID_ARG;

    if (token->firstSlot >= 0)
        --refCounts[token->firstSlot];
    if (token->secondSlot >= 0)
        --refCounts[token->secondSlot];

    localFree(token);
    *ioToken = NULL;
    return 0;
}

/* 0x81206B3A */
void CacheIoEngine::drainPendingAsyncLoads(uint8_t streamId)
{
    if (m_directionMode == CACHE_DIR_NONE)
        return;

    uint8_t idx = (uint8_t)(streamId - 1);
    SyncMutex *ringLock = &m_asyncLocks[idx];
    ringLock->lock();
    m_asyncWakeRequested[idx] = 0;
    int slot = m_asyncLastSlots[idx];
    ringLock->unlock();

    uint32_t blockCount = m_asyncBlockCounts[idx];
    uint32_t totalSlots = blockCount + 1u;
    if (slot < 0 || (uint32_t)slot >= totalSlots)
        slot = 0;

    for (uint32_t checked = 0; checked < totalSlots; ++checked) {
        if (m_closingStream != 0 || m_streamHandle == NULL)
            break;

        if (m_asyncSlotStates[idx][slot] == CACHE_BLOCK_ASYNC_LOAD) {
            int bytesRead = -1;
            m_fileSeekLock.lock();
            int64_t seekPos = m_seekCb(m_streamHandle, m_asyncSlotOffsets[idx][slot], 0, m_userCtx, m_extraCtx);
            if (seekPos >= 0) {
                bytesRead = m_readCb(m_streamHandle, m_asyncSlotPtrs[idx][slot],
                                     m_asyncBlockSizes[idx], 0, m_userCtx, m_extraCtx);
            }
            m_fileSeekLock.unlock();

            if (bytesRead < 0) {
                ringLock->lock();
                m_asyncSlotOffsets[idx][slot] = -1;
                m_asyncSlotStates[idx][slot] = CACHE_BLOCK_EMPTY;
                ringLock->unlock();
            } else {
                m_asyncSlotValidBytes[idx][slot] = bytesRead;
                m_asyncSlotStates[idx][slot] = CACHE_BLOCK_READY;
            }
        }

        if (m_directionMode == CACHE_DIR_REVERSE) {
            slot = (slot <= 0) ? (int)blockCount : (slot - 1);
        } else {
            slot = ((uint32_t)slot < blockCount) ? (slot + 1) : 0;
        }
    }
}

/* 0x81206CEE */
void CacheIoEngine::runPrefetchWorker(uint8_t streamId)
{
    if (m_asyncLocks == NULL || m_asyncWakeEvents == NULL ||
        m_asyncWakeRequested == NULL || m_asyncWorkerExiting == NULL ||
        m_asyncWorkerBusy == NULL) {
        return;
    }

    uint8_t idx = (uint8_t)(streamId - 1);
    SyncMutex *ringLock = &m_asyncLocks[idx];
    SyncEvent *wakeEvent = &m_asyncWakeEvents[idx];

    for (;;) {
        ringLock->lock();
        m_asyncWorkerBusy[idx] = 0;
        if (m_asyncWorkerExiting[idx] != 0)
            break;

        if (m_closingStream == 0 && m_asyncWakeRequested[idx] != 0) {
            m_asyncWorkerBusy[idx] = 1;
            ringLock->unlock();
            drainPendingAsyncLoads(streamId);
        } else {
            m_asyncWorkerBusy[idx] = 0;
            wakeEvent->signal(16u);
            wakeEvent->wait(ringLock, 1u, NULL);
            ringLock->unlock();
        }
    }

    m_asyncWorkerBusy[idx] = 0;
    wakeEvent->signal(16u);
    ringLock->unlock();
}

/* 0x81206DA4 */
int CacheIoEngine::prefetchWorkerEntry(SceSize args, void *argp)
{
    (void)args;
    PrefetchWorkerArg *arg = (PrefetchWorkerArg *)argp;
    if (arg == NULL)
        return 0;

    CacheIoEngine *engine = arg->engine;
    uint8_t streamId = arg->streamId;
    localFree(arg);

    if (engine == NULL || streamId == 0 || streamId > engine->m_asyncStreamCount)
        return 0;

    engine->runPrefetchWorker(streamId);
    exitWorkerThread();
    return 1;
}

/* ===== C-linkage style wrappers for CacheIoEngine (0x81203084 - 0x8120327C) ===== */

/* 0x81203084 */
int cacheIoCreate(uint8_t syncStreamCount, const uint32_t *syncBlockSizes,
                  uint8_t asyncStreamCount, const AsyncRingConfig *asyncConfigs,
                  uint8_t enableWorkers, int threadPriority,
                  void *userCtx, void *extraCtx, CacheIoEngine **outEngine)
{
    if (syncStreamCount != 0 && syncBlockSizes == NULL)
        return CACHE_ERR_INVALID_ARG;
    if (asyncStreamCount != 0 && asyncConfigs == NULL)
        return CACHE_ERR_INVALID_ARG;

    void *mem = musiccoreAllocFromPafHeap(sizeof(CacheIoEngine), getLocalHeap());
    if (mem == NULL) {
        *outEngine = NULL;
        return CACHE_ERR_NO_MEMORY;
    }

    CacheIoEngine *engine = new (mem) CacheIoEngine(syncStreamCount, syncBlockSizes,
                                                    asyncStreamCount, asyncConfigs,
                                                    enableWorkers, threadPriority,
                                                    userCtx, extraCtx);
    if (engine->isReady() != 0) {
        *outEngine = engine;
        return 0;
    }

    *outEngine = NULL;
    return CACHE_ERR_NO_MEMORY;
}

/* 0x8120313E */
int cacheIoDestroy(CacheIoEngine *engine)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    delete engine;
    return 0;
}

/* 0x81203156 */
int cacheIoSetCallbacks(CacheIoEngine *engine, StreamOpenCallback openCb, StreamCloseCallback closeCb,
                        StreamSeekCallback seekCb, StreamReadCallback readCb,
                        StreamStatSizeCallback statSizeCb, StreamErrorCallback errCb)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->setCallbacks(openCb, closeCb, seekCb, readCb, statSizeCb, errCb);
}

/* 0x8120317E */
int cacheIoSetStreamPath(CacheIoEngine *engine, const char *path)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->setStreamPath(path);
}

/* 0x81203192 */
int cacheIoGetStreamPath(const CacheIoEngine *engine, const char **outPath)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->getStreamPath(outPath);
}

/* 0x812031A6 */
int cacheIoOpenStream(CacheIoEngine *engine, void *streamHandle)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->openStream(streamHandle);
}

/* 0x812031BA */
int cacheIoCloseStream(CacheIoEngine *engine)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->closeStream();
}

/* 0x812031CE */
int cacheIoIsStreamOpen(const CacheIoEngine *engine, uint8_t *outOpen)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    *outOpen = engine->isStreamOpen() ? 1 : 0;
    return 0;
}

/* 0x812031E8 */
int cacheIoGetStreamSize(const CacheIoEngine *engine, int64_t *outSize)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->getStreamSize(outSize);
}

/* 0x812031FC */
int cacheIoSetDirectionHint(CacheIoEngine *engine, int directionMode, int forcePrefetch)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->setDirectionHint(directionMode, forcePrefetch);
}

/* 0x81203210 */
int cacheIoReadSync(CacheIoEngine *engine, uint8_t streamId, int64_t offset, uint32_t length,
                    void **outPtr, uint8_t *outDidIo, uint32_t *outAvail)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->readSync(streamId, offset, length, outPtr, outDidIo, outAvail);
}

/* 0x8120323C */
int cacheIoReadAsync(CacheIoEngine *engine, uint8_t streamId, int64_t offset, uint32_t length,
                     uint8_t pinBlocks, void **outPtr, PrefetchPinToken **outToken,
                     uint8_t *outWokeWorker, uint32_t *outAvail)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->readAsync(streamId, offset, length, pinBlocks, outPtr, outToken, outWokeWorker, outAvail);
}

/* 0x8120327C */
int cacheIoReleaseAsyncPin(CacheIoEngine *engine, uint8_t streamId, PrefetchPinToken **ioToken)
{
    if (engine == NULL)
        return CACHE_ERR_INVALID_ARG;
    return engine->releaseAsyncPin(streamId, ioToken);
}

} /* namespace MusicCore */
} /* namespace sce */
