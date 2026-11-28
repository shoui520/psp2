/*
 * SceShell musiccore_plugin - SyncCond, BufferBlock, BufferPool, SyncMutex,
 * worker thread helpers, and LocalCacheIo (paf::LocalFile-backed cached I/O
 * with standby resume recovery).
 * Original address range (3.65-CEX): 0x811F360C - 0x811F51FB
 */

#include "musiccore_cache_io_engine.h"
#include "musiccore_local_cache_io.h"
#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

extern "C" {
void *sce_paf_memset(void *dst, int ch, SceSize len);
char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
SceSize sce_paf_strlen(const char *str);
int sceKernelClearEventFlag(SceUID evfid, unsigned int bitptn);
int sceKernelSetEventFlag(SceUID evfid, unsigned int bitptn);
int sceKernelDeleteEventFlag(SceUID evfid);
int sceKernelSignalSema(SceUID semaid, int signalCount);
int sceKernelDeleteSema(SceUID semaid);
int sceKernelDeleteThread(SceUID thid);
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
}

namespace sce {
namespace MusicCore {

extern void *allocFromHeap(size_t size);
extern char *allocCharArray(size_t count);
extern uint32_t *allocUint32Array(size_t count);

static const int MC_ERR_INTERNAL = -1879113726;       /* 0x8FFF0002 */
static const int MC_ERR_INVALID_ARG = -1879113723;    /* 0x8FFF0005 */
static const int KERNEL_WAIT_TIMEOUT = -2147319803;   /* 0x80028005 */

static const int PAF_LOCALFILE_SUSPENDED = -2135994332; /* 0x80AF5024 */
static const int CACHE_ERR_BAD_PARAM = -2147434241;     /* 0x8000C0FF */
static const int CACHE_ERR_READ_IO = -2147434493;       /* 0x8000C003 */
static const int CACHE_ERR_GET_SIZE = -2147434486;      /* 0x8000C00A */

static const int IO_ERR_OPEN_FAIL = -2146394111;      /* 0x8010A001 */
static const int IO_ERR_CLOSE_FAIL = -2146394110;     /* 0x8010A002 */
static const int IO_ERR_OPEN_IO = -2146394109;        /* 0x8010A003 */
static const int IO_ERR_READ_IO = -2146394108;        /* 0x8010A004 */
static const int IO_ERR_NOT_OPEN = -2146394107;       /* 0x8010A005 */
static const int IO_ERR_EOF = -2146394105;            /* 0x8010A007 */
static const int IO_ERR_NOT_INIT = -2146390017;       /* 0x8010AFFF */
static const int IO_ERR_BAD_RANGE = -2146390031;      /* 0x8010AFF1 */

static const int64_t SEEK_ERR_BAD_PARAM = (int64_t)0x800000008000C0FFULL;
static const int64_t SEEK_ERR_IO_FAIL = (int64_t)0x800000008000C004ULL;

static void noopCacheNotify()
{
}

/* ===== SyncMutex (0x811F3AB8 - 0x811F3BA3) ===== */

class SyncMutex {
public:
    explicit SyncMutex(const char *name);
    virtual ~SyncMutex();

    bool isValid() const;
    int lock();
    int unlock();

private:
    SceUID m_semaId;
};

/* 0x811F3AB8 */
SyncMutex::SyncMutex(const char *name)
    : m_semaId(-1)
{
    SceUID sema = sceKernelCreateSema(name, 0x2000u, 1, 1, NULL);
    if (sema > 0) {
        m_semaId = sema;
    }
}

/* 0x811F3AF4 / 0x811F3B1C */
SyncMutex::~SyncMutex()
{
    if (m_semaId != -1) {
        sceKernelDeleteSema(m_semaId);
        m_semaId = -1;
    }
}

/* 0x811F3B48 */
bool SyncMutex::isValid() const
{
    return m_semaId > 0;
}

/* 0x811F3B52 */
int SyncMutex::lock()
{
    if (m_semaId <= 0) {
        return MC_ERR_INTERNAL;
    }
    if (sceKernelWaitSema(m_semaId, 1, NULL) != 0) {
        return MC_ERR_INTERNAL;
    }
    return 0;
}

/* 0x811F3B7C */
int SyncMutex::unlock()
{
    if (m_semaId <= 0) {
        return MC_ERR_INTERNAL;
    }
    if (sceKernelSignalSema(m_semaId, 1) != 0) {
        return MC_ERR_INTERNAL;
    }
    return 0;
}

/* ===== SyncCond (0x811F360C - 0x811F3743) ===== */

class SyncCond {
public:
    explicit SyncCond(const char *name);
    virtual ~SyncCond();

    int wait(SyncMutex *mutex, unsigned int waitBits, SceUInt *timeout);
    int signal(unsigned int signalBits);

private:
    SceUID m_eventFlagId;
};

/* 0x811F360C */
SyncCond::SyncCond(const char *name)
    : m_eventFlagId(-1)
{
    SceUID evf = sceKernelCreateEventFlag(name, 0x1000u, 0, NULL);
    m_eventFlagId = (evf > 0) ? evf : -1;
}

/* 0x811F3644 / 0x811F366A */
SyncCond::~SyncCond()
{
    if (m_eventFlagId != -1) {
        sceKernelDeleteEventFlag(m_eventFlagId);
        m_eventFlagId = -1;
    }
}

/* 0x811F3694 */
int SyncCond::wait(SyncMutex *mutex, unsigned int waitBits, SceUInt *timeout)
{
    if (m_eventFlagId <= 0) {
        return MC_ERR_INTERNAL;
    }
    if (sceKernelClearEventFlag(m_eventFlagId, 0u) != 0) {
        return MC_ERR_INTERNAL;
    }

    mutex->unlock();
    unsigned int outBits = 0;
    int ret = sceKernelWaitEventFlag(m_eventFlagId, waitBits, 3u, &outBits, timeout);
    if (ret != 0 && ret != KERNEL_WAIT_TIMEOUT) {
        return MC_ERR_INTERNAL;
    }
    mutex->lock();
    return 0;
}

/* 0x811F371E */
int SyncCond::signal(unsigned int signalBits)
{
    if (m_eventFlagId <= 0) {
        return MC_ERR_INTERNAL;
    }
    if (sceKernelSetEventFlag(m_eventFlagId, signalBits) != 0) {
        return MC_ERR_INTERNAL;
    }
    return 0;
}

/* ===== BufferBlock & BufferPool (0x811F3744 - 0x811F3AB7) ===== */

class BufferBlock {
public:
    BufferBlock(uint32_t payloadSize, uint32_t totalBytes, uint32_t headerSize);
    virtual ~BufferBlock();
    virtual uint32_t getStride() const;

    uint8_t m_inUse;
    uint8_t m_pinned;
    uint16_t m_padFlags;
    uint32_t m_payloadSize;
    void *m_payloadPtr;
};

/* 0x811F3744 */
BufferBlock::BufferBlock(uint32_t payloadSize, uint32_t totalBytes, uint32_t headerSize)
    : m_inUse(0)
    , m_pinned(0)
    , m_padFlags(0)
    , m_payloadSize(payloadSize)
    , m_payloadPtr(NULL)
{
    uint32_t alignedHeader = ((headerSize - 1u) & 0xFFFFFFF8u) + 8u;
    uint32_t alignedPayload = ((payloadSize - 1u) & 0xFFFFFFF8u) + 8u;
    if (alignedHeader <= totalBytes && alignedHeader + alignedPayload <= totalBytes) {
        uint8_t *selfBytes = reinterpret_cast<uint8_t *>(this);
        m_payloadPtr = selfBytes + alignedHeader;
    }
}

/* 0x811F3780 / 0x811F3798 */
BufferBlock::~BufferBlock()
{
    m_inUse = 0;
    m_pinned = 0;
    m_payloadPtr = NULL;
    m_payloadSize = 0;
}

uint32_t BufferBlock::getStride() const
{
    return ((m_payloadSize - 1u) & 0xFFFFFFF8u) + 24u;
}

class CacheBufferPool {
public:
    static uint32_t calcBlocksTotalSize(uint32_t payloadSize, uint32_t blockCount);
    static uint32_t calcPoolTotalSize(uint32_t payloadSize, uint32_t blockCount);

    CacheBufferPool(uint32_t payloadSize, uint32_t blockCount, uint32_t arenaSize, uint32_t headerSize);
    virtual ~CacheBufferPool();

    BufferBlock *getBlockAt(uint32_t index);
    uint32_t getBlockPayloadSize() const;
    void *acquireBuffer();
    void releaseBuffer(void *payloadPtr);
    void *getPayloadAt(uint32_t index);
    void setBufferPinned(void *payloadPtr, uint8_t pinned);
    uint8_t isBufferPinned(void *payloadPtr) const;

private:
    BufferBlock *m_firstBlock;
    uint32_t m_blockCount;
    uint32_t m_blockStride;
};

/* 0x811F391A */
uint32_t CacheBufferPool::calcBlocksTotalSize(uint32_t payloadSize, uint32_t blockCount)
{
    uint32_t alignedPayload = (payloadSize - 1u) & 0xFFFFFFF8u;
    return blockCount * (alignedPayload + 24u);
}

/* 0x811F392C */
uint32_t CacheBufferPool::calcPoolTotalSize(uint32_t payloadSize, uint32_t blockCount)
{
    return calcBlocksTotalSize(payloadSize, blockCount) + 16u;
}

/* 0x811F37BA / 0x811F386E */
CacheBufferPool::CacheBufferPool(uint32_t payloadSize, uint32_t blockCount,
                       uint32_t arenaSize, uint32_t headerSize)
    : m_firstBlock(NULL)
    , m_blockCount(blockCount)
    , m_blockStride(((payloadSize - 1u) & 0xFFFFFFF8u) + 24u)
{
    uint32_t alignedHeader = ((headerSize - 1u) & 0xFFFFFFF8u) + 8u;
    if (alignedHeader > arenaSize) {
        return;
    }

    uint32_t alignedStride = ((m_blockStride - 1u) & 0xFFFFFFF8u) + 8u;
    if (alignedHeader + alignedStride * blockCount > arenaSize) {
        return;
    }

    uint8_t *selfBytes = reinterpret_cast<uint8_t *>(this);
    uint8_t *basePtr = selfBytes + alignedHeader;
    m_firstBlock = (BufferBlock *)basePtr;

    uint8_t *cursor = basePtr;
    for (uint32_t idx = 0; idx < blockCount; ++idx) {
        if (cursor == NULL) {
            continue;
        }
        BufferBlock *block = new (cursor) BufferBlock(payloadSize, m_blockStride, sizeof(BufferBlock));
        if (block == NULL) {
            uint8_t *cleanup = (uint8_t *)m_firstBlock;
            for (uint32_t prev = 0; prev < idx; ++prev) {
                BufferBlock *constructed = (BufferBlock *)cleanup;
                uint32_t step = constructed->getStride();
                constructed->~BufferBlock();
                cleanup += step;
            }
            m_firstBlock = NULL;
            return;
        }
        cursor += block->getStride();
    }
}

/* 0x811F38B0 / 0x811F3902 / 0x811F3912 */
CacheBufferPool::~CacheBufferPool()
{
    if (m_blockCount != 0u && m_firstBlock != NULL) {
        uint8_t *cursor = (uint8_t *)m_firstBlock;
        for (uint32_t idx = 0; idx < m_blockCount; ++idx) {
            BufferBlock *block = (BufferBlock *)cursor;
            uint32_t step = block->getStride();
            block->~BufferBlock();
            cursor += step;
        }
    }
    m_firstBlock = NULL;
}

/* 0x811F387E */
BufferBlock *CacheBufferPool::getBlockAt(uint32_t index)
{
    if (index >= m_blockCount || m_firstBlock == NULL) {
        return NULL;
    }

    uint8_t *cursor = (uint8_t *)m_firstBlock;
    for (uint32_t stepIdx = 0; stepIdx < index && cursor != NULL; ++stepIdx) {
        BufferBlock *block = (BufferBlock *)cursor;
        cursor += block->getStride();
    }
    return (BufferBlock *)cursor;
}

/* 0x811F3940 */
uint32_t CacheBufferPool::getBlockPayloadSize() const
{
    return (m_firstBlock != NULL) ? m_firstBlock->m_payloadSize : 0u;
}

/* 0x811F394A */
void *CacheBufferPool::acquireBuffer()
{
    for (uint32_t idx = 0; idx < m_blockCount; ++idx) {
        BufferBlock *block = getBlockAt(idx);
        if (block != NULL && block->m_inUse == 0u) {
            block->m_inUse = 1u;
            if (block->m_payloadPtr != NULL) {
                return block->m_payloadPtr;
            }
        }
    }
    return NULL;
}

/* 0x811F39A6 */
void CacheBufferPool::releaseBuffer(void *payloadPtr)
{
    if (m_firstBlock == NULL) {
        return;
    }

    uint8_t *cursor = (uint8_t *)m_firstBlock;
    for (uint32_t idx = 0; idx < m_blockCount; ++idx) {
        BufferBlock *block = (BufferBlock *)cursor;
        if (block->m_payloadPtr == payloadPtr) {
            if (block->m_pinned == 0u && block->m_inUse == 1u) {
                block->m_inUse = 0u;
            }
            return;
        }
        cursor += block->getStride();
    }
}

/* 0x811F39FC */
void *CacheBufferPool::getPayloadAt(uint32_t index)
{
    BufferBlock *block = getBlockAt(index);
    return (block != NULL) ? block->m_payloadPtr : NULL;
}

/* 0x811F3A32 */
void CacheBufferPool::setBufferPinned(void *payloadPtr, uint8_t pinned)
{
    if (m_firstBlock == NULL) {
        return;
    }

    uint8_t *cursor = (uint8_t *)m_firstBlock;
    for (uint32_t idx = 0; idx < m_blockCount; ++idx) {
        BufferBlock *block = (BufferBlock *)cursor;
        if (block->m_payloadPtr == payloadPtr) {
            block->m_pinned = pinned;
            return;
        }
        cursor += block->getStride();
    }
}

/* 0x811F3A7A */
uint8_t CacheBufferPool::isBufferPinned(void *payloadPtr) const
{
    if (m_firstBlock == NULL) {
        return 0u;
    }

    uint8_t *cursor = (uint8_t *)m_firstBlock;
    for (uint32_t idx = 0; idx < m_blockCount; ++idx) {
        BufferBlock *block = (BufferBlock *)cursor;
        if (block->m_payloadPtr == payloadPtr) {
            return block->m_pinned;
        }
        cursor += block->getStride();
    }
    return 0u;
}

/* ===== Worker thread wrappers (0x811F3BA4 - 0x811F3C97) ===== */

/* 0x811F3BA4 */
int createWorkerThread(const char *name, SceUID *outThreadId,
                       SceKernelThreadEntry entry, int initPriority, SceSize stackSize)
{
    if (outThreadId == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    SceUID thid = sceKernelCreateThread(name, entry, initPriority, stackSize, 0, 0, NULL);
    if (thid < 0) {
        return MC_ERR_INTERNAL;
    }
    *outThreadId = thid;
    return 0;
}

/* 0x811F3BDC */
int startWorkerThread(SceUID threadId, SceSize argSize, void *argData)
{
    if (argData == NULL) {
        return MC_ERR_INVALID_ARG;
    }
    if (sceKernelStartThread(threadId, argSize, argData) != 0) {
        return MC_ERR_INTERNAL;
    }
    return 0;
}

/* 0x811F3BFC */
int joinAndDeleteWorkerThread(SceUID threadId)
{
    int exitStatus = 0;
    if (sceKernelWaitThreadEnd(threadId, &exitStatus, NULL) == 0) {
        sceKernelDeleteThread(threadId);
    }
    return 0;
}

/* 0x811F3C3A */
int joinWorkerThread(SceUID threadId)
{
    int exitStatus = 0;
    sceKernelWaitThreadEnd(threadId, &exitStatus, NULL);
    return 0;
}

/* 0x811F3C6A */
int deleteWorkerThread(SceUID threadId)
{
    sceKernelDeleteThread(threadId);
    return 0;
}

/* 0x811F3C74 */
int exitWorkerThread()
{
    return SceThreadmgrCoredumpTime_sceKernelExitThread(1);
}

/* 0x811F3C7E */
int delayWorkerThread(SceUInt delayUs)
{
    sceKernelDelayThread(delayUs);
    return 0;
}

/* 0x811F3C88 */
bool isValidWorkerThreadId(SceUID threadId)
{
    return threadId >= 0;
}

/* ===== LocalCacheIo (0x811F3C98 - 0x811F51FB) ===== */

static int mapCacheEngineError(uint32_t engineErr)
{
    switch (engineErr) {
    case 0x8000C003u:
        return IO_ERR_OPEN_IO;
    case 0x8000C004u:
        return IO_ERR_READ_IO;
    case 0x8000C005u:
        return IO_ERR_NOT_OPEN;
    case 0x8000C007u:
        return IO_ERR_EOF;
    case 0x8000C0FFu:
        return IO_ERR_BAD_RANGE;
    case 0x8000CFFFu:
    default:
        return -1;
    }
}

/* 0x811F3C98 */
LocalCacheIo *createLocalCacheIo(const char *path, int chunkSize, int blockCount, uint8_t readAhead)
{
    void *mem = allocFromHeap(sizeof(LocalCacheIo));
    if (mem == NULL) {
        return NULL;
    }
    return new (mem) LocalCacheIo(path, chunkSize, blockCount, readAhead);
}

/* 0x811F3CD2 */
LocalCacheIo *createLocalCacheIoEx(const char *path, int chunkSize, int blockCount,
                                   uint8_t readAhead, int extraChunkSize, int extraBlockCount)
{
    void *mem = allocFromHeap(sizeof(LocalCacheIo));
    if (mem == NULL) {
        return NULL;
    }
    return new (mem) LocalCacheIo(path, chunkSize, blockCount, readAhead,
                                  extraChunkSize, extraBlockCount);
}

bool LocalCacheIo::copyPath(const char *path)
{
    if (m_filePath != NULL) {
        delete[] m_filePath;
        m_filePath = NULL;
    }
    if (path == NULL) {
        return true;
    }
    SceSize len = sce_paf_strlen(path);
    m_filePath = allocCharArray(len + 1u);
    if (m_filePath == NULL) {
        return false;
    }
    sce_paf_memset(m_filePath, 0, len + 1u);
    sce_paf_strncpy(m_filePath, path, len);
    return true;
}

/* 0x811F3D14 */
LocalCacheIo::LocalCacheIo(const char *path, int chunkSize, int blockCount, uint8_t readAhead)
    : m_cacheHandle(NULL)
    , m_initialized(0)
    , m_filePath(NULL)
    , m_file()
    , m_priorityMode(1)
    , m_padPriority(0)
    , m_filePos(0)
    , m_chunkSize(chunkSize)
    , m_blockCount(blockCount)
{
    AsyncRingConfig cfg;
    cfg.blockSize = static_cast<uint32_t>(chunkSize);
    cfg.blockCount = static_cast<uint32_t>(blockCount);

    CacheIoEngine *engine = NULL;
    if (cacheIoCreate(0u, NULL, 1u, &cfg, readAhead, 0, NULL,
                      reinterpret_cast<void *>(&noopCacheNotify), &engine) != 0) {
        return;
    }
    m_cacheHandle = engine;
    if (cacheIoSetCallbacks(engine,
                            reinterpret_cast<StreamOpenCallback>(&openCallback),
                            reinterpret_cast<StreamCloseCallback>(&closeCallback),
                            reinterpret_cast<StreamSeekCallback>(&seekCallback),
                            reinterpret_cast<StreamReadCallback>(&readCallback),
                            reinterpret_cast<StreamStatSizeCallback>(&getFileSizeCallback),
                            reinterpret_cast<StreamErrorCallback>(&setThreadPriorityCallback)) != 0) {
        cacheIoDestroy(engine);
        return;
    }
    if (path != NULL && !copyPath(path)) {
        return;
    }
    if (cacheIoSetStreamPath(engine, m_filePath) != 0) {
        cacheIoDestroy(engine);
        return;
    }
    m_initialized = 1u;
}

/* 0x811F3E4E / 0x811F3FE4 */
LocalCacheIo::LocalCacheIo(const char *path, int chunkSize, int blockCount, uint8_t readAhead,
                           int extraChunkSize, int extraBlockCount)
    : m_cacheHandle(NULL)
    , m_initialized(0)
    , m_filePath(NULL)
    , m_file()
    , m_priorityMode(1)
    , m_padPriority(0)
    , m_filePos(0)
    , m_chunkSize(chunkSize)
    , m_blockCount(blockCount)
{
    AsyncRingConfig cfg;
    cfg.blockSize = static_cast<uint32_t>(chunkSize);
    cfg.blockCount = static_cast<uint32_t>(blockCount);

    uint32_t *extraSizes = allocUint32Array((size_t)extraBlockCount);
    if (extraSizes == NULL) {
        return;
    }
    for (int idx = 0; idx < extraBlockCount; ++idx) {
        extraSizes[idx] = (uint32_t)extraChunkSize;
    }

    CacheIoEngine *engine = NULL;
    int createErr = cacheIoCreate((uint8_t)extraBlockCount, extraSizes, 1u, &cfg, readAhead,
                                  0, NULL, reinterpret_cast<void *>(&noopCacheNotify), &engine);
    delete[] extraSizes;
    if (createErr != 0) {
        return;
    }
    m_cacheHandle = engine;

    if (cacheIoSetCallbacks(engine,
                            reinterpret_cast<StreamOpenCallback>(&openCallback),
                            reinterpret_cast<StreamCloseCallback>(&closeCallback),
                            reinterpret_cast<StreamSeekCallback>(&seekCallback),
                            reinterpret_cast<StreamReadCallback>(&readCallback),
                            reinterpret_cast<StreamStatSizeCallback>(&getFileSizeCallback),
                            reinterpret_cast<StreamErrorCallback>(&setThreadPriorityCallback)) != 0) {
        cacheIoDestroy(engine);
        return;
    }
    if (path != NULL && !copyPath(path)) {
        return;
    }
    if (cacheIoSetStreamPath(engine, m_filePath) != 0) {
        cacheIoDestroy(engine);
        return;
    }
    m_initialized = 1u;
}

/* 0x811F3FFE / 0x811F4080 / 0x811F4090 */
LocalCacheIo::~LocalCacheIo()
{
    if (m_cacheHandle != NULL) {
        cacheIoDestroy(static_cast<CacheIoEngine *>(m_cacheHandle));
    }
    if (m_filePath != NULL) {
        delete[] m_filePath;
        m_filePath = NULL;
    }
    m_initialized = 0;
}

/* 0x811F4098 */
int LocalCacheIo::isInitialized() const
{
    return m_initialized;
}

/* 0x811F409C */
int LocalCacheIo::open()
{
    if (m_initialized == 0u) {
        return -1;
    }
    return cacheIoOpenStream(static_cast<CacheIoEngine *>(m_cacheHandle), this);
}

/* 0x811F40E8 */
int LocalCacheIo::close()
{
    if (m_initialized == 0u) {
        return -1;
    }
    if (m_file.get() == NULL) {
        return 0;
    }
    return (cacheIoCloseStream(static_cast<CacheIoEngine *>(m_cacheHandle)) != 0) ? -1 : 0;
}

/* 0x811F4110 */
int LocalCacheIo::isOpen() const
{
    if (m_initialized == 0u) {
        return 0;
    }
    uint8_t opened = 0;
    if (cacheIoIsStreamOpen(static_cast<const CacheIoEngine *>(m_cacheHandle), &opened) != 0) {
        return 0;
    }
    return opened;
}

/* 0x811F414E */
int LocalCacheIo::getFileSize() const
{
    if (m_initialized == 0u) {
        return -1;
    }
    int64_t sizeVal = -1;
    if (cacheIoGetStreamSize(static_cast<const CacheIoEngine *>(m_cacheHandle), &sizeVal) != 0) {
        return -1;
    }
    return (int)sizeVal;
}

/* 0x811F419C */
int LocalCacheIo::setPath(const char *path)
{
    if (m_initialized == 0u) {
        return -1;
    }
    copyPath(path);
    return (cacheIoSetStreamPath(static_cast<CacheIoEngine *>(m_cacheHandle), m_filePath) != 0) ? -1 : 0;
}

/* 0x811F420C */
int LocalCacheIo::setPriorityMode(uint32_t priorityMode)
{
    if (m_initialized == 0u) {
        return -1;
    }
    if (m_file.get() != NULL) {
        return applyOpenFilePriority((int)priorityMode);
    }
    if (priorityMode > 1u) {
        return CACHE_ERR_BAD_PARAM;
    }
    m_priorityMode = (int)priorityMode;
    return 0;
}

/* 0x811F4240 */
int LocalCacheIo::applyOpenFilePriority(int priorityMode)
{
    if (m_initialized == 0u || m_file.get() == NULL) {
        return -1;
    }
    int pafPrio = 0;
    if (priorityMode == 0) {
        pafPrio = 1;
    } else if (priorityMode == 1) {
        pafPrio = 14;
    } else {
        return -1;
    }

    if (m_file->SetPriority(pafPrio) != 0) {
        return -1;
    }
    m_priorityMode = priorityMode;
    return 0;
}

/* 0x811F428E */
bool LocalCacheIo::isRegularFile(const char *path)
{
    if (path == NULL) {
        return false;
    }
    return paf::LocalFile::Exists(path) && paf::LocalFile::IsFile(path);
}

/* 0x811F42B4 */
int LocalCacheIo::openCallback(const char *path, void *userCtx)
{
    LocalCacheIo *self = (LocalCacheIo *)userCtx;
    if (self == NULL || path == NULL) {
        return CACHE_ERR_BAD_PARAM;
    }

    int32_t openErr = 0;
    paf::common::SharedPtr<paf::File> opened = paf::LocalFile::Open(path, 1u, 0x1FFu, &openErr);
    if (openErr != 0) {
        return IO_ERR_OPEN_FAIL;
    }

    self->m_file = opened;
    self->applyOpenFilePriority(self->m_priorityMode);
    self->m_filePos = 0;
    return 0;
}

/* 0x811F4446 */
int LocalCacheIo::closeCallback(void *userCtx)
{
    LocalCacheIo *self = (LocalCacheIo *)userCtx;
    if (self == NULL) {
        return CACHE_ERR_BAD_PARAM;
    }

    paf::common::SharedPtr<paf::File> activeFile = self->m_file;
    if (activeFile.get() == NULL) {
        return IO_ERR_BAD_RANGE;
    }

    if (activeFile->Close() != 0) {
        return IO_ERR_CLOSE_FAIL;
    }

    self->m_file.reset();
    self->m_filePos = 0;
    return 0;
}

bool LocalCacheIo::reopenAfterSuspend(paf::common::SharedPtr<paf::File> &activeFile)
{
    delayWorkerThread(1000000u);
    if (activeFile.get() != NULL) {
        activeFile->Close();
        activeFile.reset();
    }

    int32_t openErr = 0;
    activeFile = paf::LocalFile::Open(m_filePath, 1u, 0x1FFu, &openErr);
    if (openErr != 0) {
        activeFile.reset();
    }
    if (activeFile.get() == NULL) {
        return false;
    }

    m_file = activeFile;
    applyOpenFilePriority(m_priorityMode);
    return true;
}

/* 0x811F4598 */
int64_t LocalCacheIo::seekCallback(void *userCtx, int64_t offset, int whence)
{
    LocalCacheIo *self = (LocalCacheIo *)userCtx;
    if (self == NULL) {
        return SEEK_ERR_BAD_PARAM;
    }
    if (self->m_file.get() == NULL) {
        return SEEK_ERR_IO_FAIL;
    }

    paf::common::SharedPtr<paf::File> activeFile = self->m_file;
    for (;;) {
        int64_t pos = activeFile->Seek(offset, whence);
        if (pos != (int64_t)PAF_LOCALFILE_SUSPENDED) {
            if (pos >= 0) {
                self->m_filePos = pos;
                return pos;
            }
            return SEEK_ERR_IO_FAIL;
        }
        if (!self->reopenAfterSuspend(activeFile)) {
            return SEEK_ERR_IO_FAIL;
        }
    }
}

/* 0x811F4952 */
int LocalCacheIo::readCallback(void *userCtx, void *dst, size_t size)
{
    LocalCacheIo *self = (LocalCacheIo *)userCtx;
    if (self == NULL || self->m_file.get() == NULL) {
        return CACHE_ERR_BAD_PARAM;
    }

    paf::common::SharedPtr<paf::File> activeFile = self->m_file;
    for (;;) {
        int bytesRead = activeFile->Read(dst, size);
        if (bytesRead != PAF_LOCALFILE_SUSPENDED) {
            if (bytesRead >= 0) {
                self->m_filePos += bytesRead;
                return bytesRead;
            }
            return CACHE_ERR_READ_IO;
        }

        int64_t seekResult = PAF_LOCALFILE_SUSPENDED;
        do {
            if (!self->reopenAfterSuspend(activeFile)) {
                return CACHE_ERR_READ_IO;
            }
            seekResult = activeFile->Seek(self->m_filePos, 0);
        } while (seekResult == (int64_t)PAF_LOCALFILE_SUSPENDED);

        if (seekResult < 0) {
            return CACHE_ERR_READ_IO;
        }
    }
}

/* 0x811F4D2E */
int LocalCacheIo::getFileSizeCallback(const char *path, uint32_t *outSize)
{
    if (path == NULL || outSize == NULL) {
        return CACHE_ERR_BAD_PARAM;
    }
    int64_t sizeVal = paf::LocalFile::GetFileSize(path);
    if (sizeVal < 0) {
        return CACHE_ERR_GET_SIZE;
    }
    *outSize = (uint32_t)sizeVal;
    return 0;
}

/* 0x811F4D66 */
int LocalCacheIo::setThreadPriorityCallback(int priorityMode)
{
    int pafPrio = 0;
    if (priorityMode == 0) {
        pafPrio = 1;
    } else if (priorityMode == 1) {
        pafPrio = 14;
    } else {
        return CACHE_ERR_BAD_PARAM;
    }

    return (paf::LocalFile::SetThreadDefaultPriority(pafPrio) != 0) ? -1 : 0;
}

/* 0x811F4D9E */
int LocalCacheIo::setCacheHint(int hintMode)
{
    if (m_initialized == 0u) {
        return 0;
    }
    int engineMode = 0;
    if (hintMode == 0) {
        engineMode = 0;
    } else if (hintMode >= 1 && hintMode <= 2) {
        engineMode = 1;
    } else if (hintMode == 3) {
        engineMode = 2;
    } else {
        return 0;
    }
    return cacheIoSetDirectionHint(static_cast<CacheIoEngine *>(m_cacheHandle), engineMode, 0);
}

/* 0x811F4DCA */
void *LocalCacheIo::mapRangeSimple(int offset, int size, int unusedFlag, int *outErr, int extra)
{
    (void)unusedFlag;
    (void)extra;
    if (m_initialized == 0u) {
        *outErr = -1;
        return NULL;
    }
    if ((offset | size) < 0) {
        *outErr = IO_ERR_BAD_RANGE;
        return NULL;
    }

    void *mappedPtr = NULL;
    uint8_t reachedEof = 0;
    PrefetchPinToken *pinToken = NULL;
    uint32_t engineErr = static_cast<uint32_t>(cacheIoReadAsync(
        static_cast<CacheIoEngine *>(m_cacheHandle), 1u, (int64_t)offset,
        static_cast<uint32_t>(size), 0u, &mappedPtr, &pinToken, &reachedEof, NULL));
    if (engineErr != 0u) {
        *outErr = mapCacheEngineError(engineErr);
        return NULL;
    }
    *outErr = 0;
    return mappedPtr;
}

/* 0x811F4EFE */
void *LocalCacheIo::mapRange(int offset, int size, uint8_t flags,
                             int *outMappedSize, int *outErr, int extra)
{
    (void)extra;
    if (m_initialized == 0u) {
        *outErr = -1;
        return NULL;
    }
    if ((offset | size) < 0) {
        *outErr = IO_ERR_BAD_RANGE;
        return NULL;
    }

    *outMappedSize = 0;
    void *mappedPtr = NULL;
    uint32_t mappedBytes = 0;
    uint8_t reachedEof = 0;
    PrefetchPinToken *pinToken = NULL;
    uint32_t engineErr = static_cast<uint32_t>(cacheIoReadAsync(
        static_cast<CacheIoEngine *>(m_cacheHandle), 1u, (int64_t)offset,
        static_cast<uint32_t>(size), flags, &mappedPtr, &pinToken, &reachedEof, &mappedBytes));
    if (engineErr != 0u) {
        *outErr = mapCacheEngineError(engineErr);
        return NULL;
    }
    *outErr = 0;
    *outMappedSize = (int)mappedBytes;
    return mappedPtr;
}

/* 0x811F5062 */
int LocalCacheIo::checkRangeReadable(int sizeBytes, int *outErr)
{
    if (m_initialized == 0u) {
        *outErr = IO_ERR_NOT_INIT;
        return 0;
    }

    void *mappedPtr = NULL;
    uint8_t didIo = 0;
    uint32_t avail = 0;
    int err = cacheIoReadSync(static_cast<CacheIoEngine *>(m_cacheHandle),
                              1u, 0, (uint32_t)sizeBytes, &mappedPtr, &didIo, &avail);
    if (err != 0) {
        *outErr = (err == CACHE_ERR_BAD_PARAM) ? IO_ERR_BAD_RANGE : -1;
        return 0;
    }
    *outErr = 0;
    return 1;
}

/* 0x811F50CE */
int LocalCacheIo::prefetchRange(uint8_t flags, int offset, int size, int *outErr)
{
    if (m_initialized == 0u) {
        *outErr = -1;
        return 0;
    }
    if ((offset | size) < 0) {
        *outErr = IO_ERR_BAD_RANGE;
        return 0;
    }

    void *mappedPtr = NULL;
    uint8_t didIo = 0;
    uint32_t avail = 0;
    uint32_t engineErr = static_cast<uint32_t>(cacheIoReadSync(
        static_cast<CacheIoEngine *>(m_cacheHandle), flags, offset,
        static_cast<uint32_t>(size), &mappedPtr, &didIo, &avail));
    if (engineErr != 0u) {
        *outErr = mapCacheEngineError(engineErr);
        return 0;
    }
    *outErr = 0;
    return 0;
}

/* Synchronization & thread bridges used across musiccore_plugin TUs */
struct SyncEvent {
    void *vtablePtr;
    SceUID evfId;
};

void syncMutexConstruct(SyncMutex *mutex, const char *name)
{
    new (mutex) SyncMutex(name);
}

void syncMutexInit(SyncMutex *mutex, const char *name)
{
    new (mutex) SyncMutex(name);
}

void syncMutexDestroy(SyncMutex *mutex)
{
    if (mutex != NULL)
        mutex->~SyncMutex();
}

int syncMutexIsValid(const SyncMutex *mutex)
{
    return (mutex != NULL && mutex->isValid()) ? 1 : 0;
}

int syncMutexLock(SyncMutex *mutex)
{
    return (mutex != NULL) ? mutex->lock() : MC_ERR_INTERNAL;
}

int syncMutexUnlock(SyncMutex *mutex)
{
    return (mutex != NULL) ? mutex->unlock() : MC_ERR_INTERNAL;
}

void syncCondInit(SyncCond *cond, const char *name)
{
    new (cond) SyncCond(name);
}

void syncCondDestroy(SyncCond *cond)
{
    if (cond != NULL)
        cond->~SyncCond();
}

int syncCondWait(SyncCond *cond, SyncMutex *mutex, unsigned int waitBits, SceUInt timeoutUs)
{
    SceUInt timeout = timeoutUs;
    return (cond != NULL) ? cond->wait(mutex, waitBits, (timeoutUs != 0) ? &timeout : NULL) : MC_ERR_INTERNAL;
}

int syncCondSignal(SyncCond *cond, unsigned int signalBits)
{
    return (cond != NULL) ? cond->signal(signalBits) : MC_ERR_INTERNAL;
}

void syncEventConstruct(SyncEvent *ev, const char *name)
{
    new (reinterpret_cast<SyncCond *>(ev)) SyncCond(name);
}

void syncEventDestroy(SyncEvent *ev)
{
    if (ev != NULL)
        reinterpret_cast<SyncCond *>(ev)->~SyncCond();
}

int syncEventWait(SyncEvent *ev, SyncMutex *mutex, uint32_t waitBits, int timeoutMode)
{
    SceUInt timeout = static_cast<SceUInt>(timeoutMode);
    return (ev != NULL)
        ? reinterpret_cast<SyncCond *>(ev)->wait(mutex, waitBits, (timeoutMode > 0) ? &timeout : NULL)
        : MC_ERR_INTERNAL;
}

int syncEventSignal(SyncEvent *ev, uint32_t signalBits)
{
    return (ev != NULL) ? reinterpret_cast<SyncCond *>(ev)->signal(signalBits) : MC_ERR_INTERNAL;
}

int syncEventIsValid(const SyncEvent *ev)
{
    return (ev != NULL && ev->evfId > 0) ? 1 : 0;
}

int joinDetachedWorker(SceUID threadId)
{
    return joinAndDeleteWorkerThread(threadId);
}

void exitCurrentWorkerThread()
{
    exitWorkerThread();
}

int workerThreadDelete(SceUID threadId)
{
    return deleteWorkerThread(threadId);
}

/* FixedPool alias delegating to CacheBufferPool (used by musiccore_factory.cpp) */
struct PoolSlotHeader {
    unsigned char reserved[5];
    unsigned char active;
    unsigned char pad[6];
    void *object;
};

class FixedPool {
public:
    FixedPool(unsigned int elemSize, unsigned int count, unsigned int totalBytes,
              unsigned int headerSize);
    virtual ~FixedPool();

    static unsigned int calcBodySize(unsigned int elemSize, unsigned int count);
    static unsigned int calcTotalSize(unsigned int elemSize, unsigned int count);

    PoolSlotHeader *getSlotHeader(unsigned char index);
    unsigned int getSlotSize() const;
    void *allocSlot();
    void freeSlot(void *slot);
    void *getSlotObject(unsigned int index);
    void setSlotActive(void *slot, unsigned char active);
    int isSlotActive(void *slot);

public:
    void *m_poolMem;
    unsigned int m_count;
    unsigned int m_elemSize;
};

FixedPool::FixedPool(unsigned int elemSize, unsigned int count, unsigned int totalBytes,
                     unsigned int headerSize)
    : m_poolMem(NULL), m_count(count), m_elemSize(elemSize)
{
    new (reinterpret_cast<CacheBufferPool *>(this))
        CacheBufferPool(elemSize, count, totalBytes, headerSize);
}

FixedPool::~FixedPool()
{
    reinterpret_cast<CacheBufferPool *>(this)->~CacheBufferPool();
}

unsigned int FixedPool::calcBodySize(unsigned int elemSize, unsigned int count)
{
    return CacheBufferPool::calcBlocksTotalSize(elemSize, count);
}

unsigned int FixedPool::calcTotalSize(unsigned int elemSize, unsigned int count)
{
    return CacheBufferPool::calcPoolTotalSize(elemSize, count);
}

PoolSlotHeader *FixedPool::getSlotHeader(unsigned char index)
{
    return reinterpret_cast<PoolSlotHeader *>(
        reinterpret_cast<CacheBufferPool *>(this)->getBlockAt(index));
}

unsigned int FixedPool::getSlotSize() const
{
    return reinterpret_cast<const CacheBufferPool *>(this)->getBlockPayloadSize();
}

void *FixedPool::allocSlot()
{
    return reinterpret_cast<CacheBufferPool *>(this)->acquireBuffer();
}

void FixedPool::freeSlot(void *slot)
{
    reinterpret_cast<CacheBufferPool *>(this)->releaseBuffer(slot);
}

void *FixedPool::getSlotObject(unsigned int index)
{
    return reinterpret_cast<CacheBufferPool *>(this)->getPayloadAt(index);
}

void FixedPool::setSlotActive(void *slot, unsigned char active)
{
    reinterpret_cast<CacheBufferPool *>(this)->setBufferPinned(slot, active);
}

int FixedPool::isSlotActive(void *slot)
{
    return reinterpret_cast<CacheBufferPool *>(this)->isBufferPinned(slot);
}

int isWorkerThreadValid(SceUID threadId)
{
    return isValidWorkerThreadId(threadId) ? 1 : 0;
}

int workerThreadCreate(const char *name, SceUID *outThid, SceKernelThreadEntry entry,
                       int priority, unsigned int stackSize)
{
    return createWorkerThread(name, outThid, entry, priority, stackSize);
}

int workerThreadWaitEnd(SceUID thid)
{
    return joinWorkerThread(thid);
}

void workerThreadExitSelf()
{
    exitWorkerThread();
}

int workerThreadJoinRaw(SceUID thid)
{
    return joinWorkerThread(thid);
}

struct SyncMutexStorage {
    void *vtable;
    SceUID semaId;
};

struct SyncCondStorage {
    void *vtable;
    SceUID evfId;
};

void syncMutexInit(SyncMutexStorage *mtx, const char *name)
{
    new (reinterpret_cast<SyncMutex *>(mtx)) SyncMutex(name);
}

void syncMutexDestroy(SyncMutexStorage *mtx)
{
    if (mtx != NULL)
        reinterpret_cast<SyncMutex *>(mtx)->~SyncMutex();
}

int syncMutexIsValid(const SyncMutexStorage *mtx)
{
    return (mtx != NULL && reinterpret_cast<const SyncMutex *>(mtx)->isValid()) ? 1 : 0;
}

int syncMutexLock(SyncMutexStorage *mtx)
{
    return (mtx != NULL) ? reinterpret_cast<SyncMutex *>(mtx)->lock() : MC_ERR_INTERNAL;
}

int syncMutexUnlock(SyncMutexStorage *mtx)
{
    return (mtx != NULL) ? reinterpret_cast<SyncMutex *>(mtx)->unlock() : MC_ERR_INTERNAL;
}

void syncCondInit(SyncCondStorage *cond, const char *name)
{
    new (reinterpret_cast<SyncCond *>(cond)) SyncCond(name);
}

void syncCondDestroy(SyncCondStorage *cond)
{
    if (cond != NULL)
        reinterpret_cast<SyncCond *>(cond)->~SyncCond();
}

int syncCondWait(SyncCondStorage *cond, SyncMutexStorage *mtx, unsigned int mask, unsigned int *timeout)
{
    return (cond != NULL)
        ? reinterpret_cast<SyncCond *>(cond)->wait(reinterpret_cast<SyncMutex *>(mtx), mask, timeout)
        : MC_ERR_INTERNAL;
}

int syncCondSignal(SyncCondStorage *cond, unsigned int mask)
{
    return (cond != NULL) ? reinterpret_cast<SyncCond *>(cond)->signal(mask) : MC_ERR_INTERNAL;
}

struct StreamIoInterface;

StreamIoInterface *createLocalCacheIo(const char *path, int chunkSize, int blockCount, int readAhead)
{
    void *mem = allocFromHeap(sizeof(LocalCacheIo));
    if (mem == NULL)
        return NULL;
    return reinterpret_cast<StreamIoInterface *>(
        new (mem) LocalCacheIo(path, chunkSize, blockCount, readAhead));
}

StreamIoInterface *createLocalCacheIoEx(const char *path, int chunkSize, int blockCount,
                                        int readAhead, int extraChunk, int extraBlocks)
{
    void *mem = allocFromHeap(sizeof(LocalCacheIo));
    if (mem == NULL)
        return NULL;
    return reinterpret_cast<StreamIoInterface *>(
        new (mem) LocalCacheIo(path, chunkSize, blockCount, readAhead, extraChunk, extraBlocks));
}

} /* namespace MusicCore */
} /* namespace sce */
