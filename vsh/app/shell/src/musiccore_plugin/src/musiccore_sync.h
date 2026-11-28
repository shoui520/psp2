/*
 * SceShell musiccore_plugin - Shared synchronization primitives (SyncMutex,
 * SyncCond), fixed-size buffer pool (BufferBlock, CacheBufferPool), and
 * worker thread helpers implemented in musiccore_sync_cache_io.cpp.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_SYNC_H
#define VSH_APP_SHELL_MUSICCORE_SYNC_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

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

class SyncCond {
public:
    explicit SyncCond(const char *name);
    virtual ~SyncCond();

    bool isValid() const { return m_eventFlagId > 0; }
    int wait(SyncMutex *mutex, unsigned int waitBits, SceUInt *timeout);
    int signal(unsigned int signalBits);

private:
    SceUID m_eventFlagId;
};

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

protected:
    BufferBlock *m_firstBlock;
    uint32_t m_blockCount;
    uint32_t m_blockStride;
};

int createWorkerThread(const char *name, SceUID *outThreadId,
                       SceKernelThreadEntry entry, int initPriority, SceSize stackSize);
int startWorkerThread(SceUID threadId, SceSize argSize, void *argData);
int joinAndDeleteWorkerThread(SceUID threadId);
int joinWorkerThread(SceUID threadId);
int deleteWorkerThread(SceUID threadId);
int exitWorkerThread();
int delayWorkerThread(SceUInt delayUs);
bool isValidWorkerThreadId(SceUID threadId);

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_SYNC_H */
