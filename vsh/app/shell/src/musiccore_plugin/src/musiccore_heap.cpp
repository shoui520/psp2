/*
 * SceShell musiccore_plugin - local heap allocator and static buffer pool.
 * Original address range (3.65-CEX): 0x811D3484 - 0x811D3964
 */

#include <kernel.h>
#include <libsysmodule.h>
#include <stddef.h>
#include <stdint.h>

#include <memory/heap_allocator.h>
#include <std/new>

#include "musiccore_decoder_manager.h"
#include "musiccore_heap.h"

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
int sceSysmoduleLoadModuleInternal(uint32_t id);
int sceSysmoduleUnloadModuleInternal(uint32_t id);
int sceSysmoduleIsLoadedInternal(uint32_t id);
}

#ifndef SCE_SYSMODULE_INTERNAL_AUDIOCODEC
#define SCE_SYSMODULE_INTERNAL_AUDIOCODEC 0x80000016
#endif

static const int MUSICCORE_ERR_FAILED = -1879113726; /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724; /* 0x8FFF0004 */

static const SceSize MUSICCORE_LOCAL_HEAP_SIZE = 0x2A8000;
static const SceSize MUSICCORE_POOL_TOTAL_SIZE = 0x1ABEC0;
static const SceSize MUSICCORE_LARGE_BUF_SIZE = 542720;
static const SceSize MUSICCORE_MEDIUM_BUF_SIZE = 32832;

namespace sce {
namespace MusicCore {

static paf::memory::HeapAllocator *g_localHeap = NULL;

void *getLocalHeap()
{
    return g_localHeap;
}

void *localAlloc(SceSize size)
{
    if (g_localHeap != NULL)
        return g_localHeap->sce_paf_malloc(size);
    return NULL;
}

void localFree(void *ptr)
{
    if (g_localHeap != NULL)
        g_localHeap->sce_paf_free(ptr);
}

static BufferPool g_bufferPool;

BufferPool::BufferPool()
    : m_backingMem(NULL)
{
    sce_paf_memset(m_largeSlots, 0, sizeof(m_largeSlots));
    sce_paf_memset(m_mediumSlots, 0, sizeof(m_mediumSlots));
    sce_paf_memset(m_typedSlots, 0, sizeof(m_typedSlots));
}

int BufferPool::term()
{
    for (int i = 0; i < 2; ++i) {
        if (m_largeSlots[i] != NULL) {
            delete m_largeSlots[i];
            m_largeSlots[i] = NULL;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (m_mediumSlots[i] != NULL) {
            delete m_mediumSlots[i];
            m_mediumSlots[i] = NULL;
        }
    }
    for (int i = 0; i < 5; ++i) {
        if (m_typedSlots[i] != NULL) {
            delete m_typedSlots[i];
            m_typedSlots[i] = NULL;
        }
    }
    if (m_backingMem != NULL) {
        delete[] (char *)m_backingMem;
        m_backingMem = NULL;
    }
    return 0;
}

BufferPool::~BufferPool()
{
    term();
}

BufferPool *BufferPool::getInstance()
{
    return &g_bufferPool;
}

static size_t alignUp8(size_t size)
{
    return ((size - 1) & ~7u) + 8;
}

int BufferPool::init()
{
    int ret = 0;
    char *cursor = (char *)localAlloc(MUSICCORE_POOL_TOTAL_SIZE);
    m_backingMem = cursor;

    for (int i = 0; i < 2; ++i) {
        void *mem = localAlloc(sizeof(BufferEntry));
        BufferEntry *entry = NULL;
        if (mem != NULL)
            entry = new (mem) BufferEntry(i, 0, cursor, MUSICCORE_LARGE_BUF_SIZE);
        m_largeSlots[i] = entry;
        if (entry == NULL) {
            ret = MUSICCORE_ERR_NO_MEMORY;
            break;
        }
        cursor += MUSICCORE_LARGE_BUF_SIZE;
    }

    for (int i = 0; i < 8; ++i) {
        void *mem = localAlloc(sizeof(BufferEntry));
        BufferEntry *entry = NULL;
        if (mem != NULL)
            entry = new (mem) BufferEntry(i, 1, cursor, MUSICCORE_MEDIUM_BUF_SIZE);
        m_mediumSlots[i] = entry;
        if (entry == NULL) {
            ret = MUSICCORE_ERR_NO_MEMORY;
            break;
        }
        cursor += MUSICCORE_MEDIUM_BUF_SIZE;
    }

    for (int idx = 0, kind = 2; idx < 5; ++idx, ++kind) {
        SceSize rawSize = 0;
        switch (kind) {
        case 2: rawSize = 250880; break;
        case 3: rawSize = 133120; break;
        case 4: rawSize = 0x4000; break;
        case 5: rawSize = 1216;   break;
        case 6: rawSize = 3072;   break;
        default: break;
        }
        SceSize alignedSize = (SceSize)alignUp8(rawSize);
        void *mem = localAlloc(sizeof(BufferEntry));
        BufferEntry *entry = NULL;
        if (mem != NULL)
            entry = new (mem) BufferEntry(idx, kind, cursor, alignedSize);
        m_typedSlots[idx] = entry;
        if (entry == NULL)
            return MUSICCORE_ERR_NO_MEMORY;
        cursor += alignedSize;
    }

    return ret;
}

void *BufferPool::acquireLarge()
{
    for (int i = 0; i < 2; ++i) {
        BufferEntry *entry = m_largeSlots[i];
        if (entry == NULL)
            break;
        if (!entry->inUse) {
            void *buf = entry->data;
            SceSize sz = entry->size;
            if (buf != NULL && sz != 0) {
                entry->inUse = true;
                sce_paf_memset(buf, 0, sz);
            }
            return buf;
        }
    }
    return NULL;
}

int BufferPool::releaseLarge(void *ptr)
{
    for (int i = 0; i < 2; ++i) {
        BufferEntry *entry = m_largeSlots[i];
        if (entry == NULL)
            break;
        if (entry->data == ptr) {
            entry->inUse = false;
            return 0;
        }
    }
    return MUSICCORE_ERR_FAILED;
}

void *BufferPool::acquireMedium()
{
    for (int i = 0; i < 8; ++i) {
        BufferEntry *entry = m_mediumSlots[i];
        if (entry == NULL)
            break;
        if (!entry->inUse) {
            void *buf = entry->data;
            SceSize sz = entry->size;
            if (buf != NULL && sz != 0) {
                entry->inUse = true;
                sce_paf_memset(buf, 0, sz);
            }
            return buf;
        }
    }
    return NULL;
}

int BufferPool::releaseMedium(void *ptr)
{
    for (int i = 0; i < 8; ++i) {
        BufferEntry *entry = m_mediumSlots[i];
        if (entry == NULL)
            break;
        if (entry->data == ptr) {
            entry->inUse = false;
            return 0;
        }
    }
    return MUSICCORE_ERR_FAILED;
}

void *BufferPool::acquireTyped(int kind)
{
    for (int i = 0; i < 5; ++i) {
        BufferEntry *entry = m_typedSlots[i];
        if (entry->kind == kind) {
            void *buf = entry->data;
            SceSize sz = entry->size;
            if (buf != NULL && sz != 0) {
                entry->inUse = true;
                sce_paf_memset(buf, 0, sz);
            }
            return buf;
        }
    }
    return NULL;
}

int BufferPool::releaseTyped(void *ptr)
{
    for (int i = 0; i < 5; ++i) {
        BufferEntry *entry = m_typedSlots[i];
        if (entry == NULL)
            return MUSICCORE_ERR_FAILED;
        if (entry->data == ptr) {
            entry->inUse = false;
            return 0;
        }
    }
    return 0;
}

void initLocalHeap()
{
    if (g_localHeap != NULL) {
        delete g_localHeap;
        g_localHeap = NULL;
    }

    g_localHeap = new paf::memory::HeapAllocator(
        NULL, MUSICCORE_LOCAL_HEAP_SIZE, "SceShellMusicCoreLocalHeap", NULL);

    BufferPool::getInstance()->init();

    unsigned int paramCount = 4u;
    ThreadSpec specs[4];
    DecoderManager::getThreadSpecs(specs, &paramCount);
    WorkerThreadPool::getInstance()->initialize(specs, paramCount);
}

void termLocalHeap()
{
    WorkerThreadPool::getInstance()->finalize();
    BufferPool::getInstance()->term();
    if (g_localHeap != NULL) {
        delete g_localHeap;
        g_localHeap = NULL;
    }
}

int makeChannelId(char type, char slot)
{
    return (type & 0xF) | ((slot & 0xF) << 4);
}

int getChannelType(char channelId)
{
    return channelId & 0xF;
}

int getChannelSlot(unsigned char channelId)
{
    return channelId >> 4;
}

int loadAudioCodecModule()
{
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC) < 0)
        return MUSICCORE_ERR_FAILED;
    return 0;
}

int unloadAudioCodecModule()
{
    if (sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC) < 0)
        return MUSICCORE_ERR_FAILED;
    return 0;
}

bool isAudioCodecModuleLoaded()
{
    return sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_AUDIOCODEC) == 0;
}

} /* namespace MusicCore */
} /* namespace sce */
