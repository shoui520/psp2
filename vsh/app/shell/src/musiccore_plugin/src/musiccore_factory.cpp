/*
 * SceShell musiccore_plugin - player pool and object factory.
 * Original address range (3.65-CEX): 0x811D4E5C - 0x811D555C
 */

#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_sync.h"

static const int MUSICCORE_ERR_FAILED = -1879113726; /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724; /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */

namespace sce {
namespace MusicCore {

class BaseObject {
public:
    virtual ~BaseObject() {}
};

extern unsigned int calcControllerSize();
extern unsigned int calcMusicPlayerSize(int maxTracks);
extern unsigned int calcPlaylistPlayerSize(int maxItems);
extern unsigned int calcStreamPlayerSize(int maxStreams);

extern BaseObject *constructMusicPlayer(void *mem, void *factory, int maxTracks,
                                        int *outErr, unsigned int slotSize, unsigned int hdrSize);
extern BaseObject *constructPlaylistPlayer(void *mem, void *factory, int maxItems,
                                           int *outErr, unsigned int slotSize, unsigned int hdrSize);
extern BaseObject *constructStreamPlayer(void *mem, void *factory, int maxStreams,
                                         int *outErr, unsigned int slotSize, unsigned int hdrSize);
extern BaseObject *constructController(void *mem, void *factory, int *outErr, unsigned int slotSize);
extern BaseObject *constructWorkBuffer(void *mem, unsigned int bufSize, unsigned int slotSize);

static unsigned int alignUp8(unsigned int size)
{
    return ((size - 1) & ~7u) + 8;
}

static unsigned int calcPlayerElemSize(char playerKind, int capacity)
{
    if (playerKind == 0)
        return calcMusicPlayerSize(capacity);
    if (playerKind == 1)
        return calcPlaylistPlayerSize(capacity);
    if (playerKind == 2)
        return calcStreamPlayerSize(capacity);
    return 0;
}

class PlayerPool : public CacheBufferPool {
public:
    static unsigned int calcSize(char playerKind, unsigned char count, int capacity);

    PlayerPool(void *factory, char playerKind, unsigned char count, int capacity,
               unsigned int totalBytes, unsigned int headerSize);
    virtual ~PlayerPool();

    void *getPlayer(unsigned char index);

private:
    char m_playerKind;
    unsigned char m_playerCount;
    int m_capacity;
};

unsigned int PlayerPool::calcSize(char playerKind, unsigned char count, int capacity)
{
    unsigned int elemSize = 0;
    if (playerKind >= 0 && playerKind <= 2)
        elemSize = alignUp8(calcPlayerElemSize(playerKind, capacity));
    return CacheBufferPool::calcBlocksTotalSize(elemSize, count);
}

PlayerPool::PlayerPool(void *factory, char playerKind, unsigned char count, int capacity,
                       unsigned int totalBytes, unsigned int headerSize)
    : CacheBufferPool(calcPlayerElemSize(playerKind, capacity), count, totalBytes, headerSize),
      m_playerKind(playerKind),
      m_playerCount(count),
      m_capacity(capacity)
{
    if (factory == NULL)
        return;
    if (playerKind < 0 || playerKind > 2)
        return;
    if (totalBytes < calcSize(playerKind, count, capacity) + 24 || totalBytes < 24)
        return;

    unsigned int rawElemSize = calcPlayerElemSize(m_playerKind, m_capacity);
    if (CacheBufferPool::calcBlocksTotalSize(rawElemSize, m_playerCount) + 24 > totalBytes)
        return;

    for (unsigned int i = 0; i < m_blockCount; ++i) {
        void *slot = acquireBuffer();
        if (slot == NULL)
            continue;

        int err = 0;
        BaseObject *obj = NULL;
        unsigned int slotSize = getBlockPayloadSize();

        if (m_playerKind == 0) {
            obj = constructMusicPlayer(slot, factory, m_capacity, &err, slotSize, 472);
        } else if (m_playerKind == 1) {
            obj = constructPlaylistPlayer(slot, factory, m_capacity, &err, slotSize, 6648);
        } else if (m_playerKind == 2) {
            obj = constructStreamPlayer(slot, factory, m_capacity, &err, slotSize, 496);
        } else {
            err = MUSICCORE_ERR_INVALID_ARG;
        }

        if (err != 0) {
            if (obj != NULL)
                obj->~BaseObject();
            releaseBuffer(slot);
        } else if (obj == NULL) {
            releaseBuffer(slot);
        } else {
            setBufferPinned(slot, 1);
        }
    }
}

PlayerPool::~PlayerPool()
{
    for (unsigned int i = 0; i < m_blockCount; ++i) {
        BaseObject *obj = (BaseObject *)getPayloadAt(i);
        if (obj != NULL)
            obj->~BaseObject();
    }
}

void *PlayerPool::getPlayer(unsigned char index)
{
    BufferBlock *hdr = getBlockAt(index);
    if (hdr != NULL && hdr->m_pinned == 1)
        return hdr->m_payloadPtr;
    return NULL;
}

class ObjFactory {
public:
    static unsigned int calcPayloadSize(int numMusic, int numPlaylist, int numStream);
    static unsigned int calcTotalSize(int numMusic, int numPlaylist, int numStream);

    ObjFactory(int numMusic, int numPlaylist, int numStream,
               unsigned int totalBytes, unsigned int headerSize);
    virtual ~ObjFactory();

    int createController(void **outCtrl);
    int destroyController(BaseObject *ctrl);
    int createWorkBuf(void **outBuf);
    int destroyWorkBuf(BaseObject *buf);
    int getPlayerPool(char playerKind, PlayerPool **outPool);

private:
    CacheBufferPool *m_controllerPool;
    PlayerPool *m_playlistPool;
    PlayerPool *m_musicPool;
    PlayerPool *m_streamPool;
    CacheBufferPool *m_workBufPool;
};

unsigned int ObjFactory::calcPayloadSize(int numMusic, int numPlaylist, int numStream)
{
    unsigned int ctrlPoolSz = CacheBufferPool::calcPoolTotalSize(calcControllerSize(), 1);
    unsigned int musicSz = PlayerPool::calcSize(0, (unsigned char)numMusic, 1);
    unsigned int playlistSz = PlayerPool::calcSize(1, (unsigned char)numPlaylist, 10000);
    unsigned int streamSz = PlayerPool::calcSize(2, (unsigned char)numStream, 1);
    unsigned int workBufCount = (unsigned int)(2 * (numMusic + numPlaylist + numStream) + 6);
    unsigned int workBufSz = CacheBufferPool::calcPoolTotalSize(4224, workBufCount);
    return ctrlPoolSz + musicSz + playlistSz + streamSz + workBufSz + 72;
}

unsigned int ObjFactory::calcTotalSize(int numMusic, int numPlaylist, int numStream)
{
    return calcPayloadSize(numMusic, numPlaylist, numStream) + 24;
}

ObjFactory::ObjFactory(int numMusic, int numPlaylist, int numStream,
                       unsigned int totalBytes, unsigned int headerSize)
    : m_controllerPool(NULL),
      m_playlistPool(NULL),
      m_musicPool(NULL),
      m_streamPool(NULL),
      m_workBufPool(NULL)
{
    if (totalBytes < calcPayloadSize(numMusic, numPlaylist, numStream) + 24)
        return;

    unsigned int offset = alignUp8(headerSize);
    if (offset > totalBytes)
        return;

    char *cursor = (char *)this + offset;

    unsigned int ctrlElemSz = calcControllerSize();
    unsigned int ctrlPoolSz = CacheBufferPool::calcPoolTotalSize(ctrlElemSz, 1);
    if (offset + ctrlPoolSz > totalBytes)
        return;
    if (cursor != NULL)
        m_controllerPool = new (cursor) CacheBufferPool(ctrlElemSz, 1, ctrlPoolSz, 16);
    cursor += ctrlPoolSz;
    offset += ctrlPoolSz;

    unsigned int musicSz = PlayerPool::calcSize(0, (unsigned char)numMusic, 1) + 24;
    if (offset + musicSz > totalBytes)
        return;
    if (cursor != NULL)
        m_musicPool = new (cursor) PlayerPool(this, 0, (unsigned char)numMusic, 1, musicSz, 24);
    cursor += musicSz;
    offset += musicSz;

    unsigned int playlistSz = PlayerPool::calcSize(1, (unsigned char)numPlaylist, 10000) + 24;
    if (offset + playlistSz > totalBytes)
        return;
    if (cursor != NULL)
        m_playlistPool = new (cursor) PlayerPool(this, 1, (unsigned char)numPlaylist, 10000, playlistSz, 24);
    cursor += playlistSz;
    offset += playlistSz;

    unsigned int streamSz = PlayerPool::calcSize(2, (unsigned char)numStream, 1) + 24;
    if (offset + streamSz > totalBytes)
        return;
    if (cursor != NULL)
        m_streamPool = new (cursor) PlayerPool(this, 2, (unsigned char)numStream, 1, streamSz, 24);
    cursor += streamSz;
    offset += streamSz;

    unsigned int workBufCount = (unsigned int)(2 * (numMusic + numPlaylist + numStream) + 6);
    unsigned int workBufSz = CacheBufferPool::calcPoolTotalSize(4224, workBufCount);
    if (offset + workBufSz > totalBytes)
        return;
    if (cursor != NULL)
        m_workBufPool = new (cursor) CacheBufferPool(4224, workBufCount, workBufSz, 16);
}

ObjFactory::~ObjFactory()
{
    if (m_workBufPool != NULL) {
        m_workBufPool->~CacheBufferPool();
        m_workBufPool = NULL;
    }
    if (m_musicPool != NULL) {
        m_musicPool->~PlayerPool();
        m_musicPool = NULL;
    }
    if (m_playlistPool != NULL) {
        m_playlistPool->~PlayerPool();
        m_playlistPool = NULL;
    }
    if (m_streamPool != NULL) {
        m_streamPool->~PlayerPool();
        m_streamPool = NULL;
    }
    if (m_controllerPool != NULL) {
        m_controllerPool->~CacheBufferPool();
        m_controllerPool = NULL;
    }
}

int ObjFactory::createController(void **outCtrl)
{
    *outCtrl = NULL;
    if (m_controllerPool == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    int err = 0;
    void *slot = m_controllerPool->acquireBuffer();
    if (slot == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    unsigned int slotSize = m_controllerPool->getBlockPayloadSize();
    BaseObject *ctrl = constructController(slot, this, &err, slotSize);
    *outCtrl = ctrl;
    if (ctrl == NULL) {
        err = MUSICCORE_ERR_FAILED;
        m_controllerPool->releaseBuffer(slot);
    } else {
        m_controllerPool->setBufferPinned(slot, 1);
    }
    return err;
}

int ObjFactory::destroyController(BaseObject *ctrl)
{
    if (ctrl == NULL)
        return 0;
    if (m_controllerPool == NULL)
        return MUSICCORE_ERR_FAILED;

    if (m_controllerPool->isBufferPinned(ctrl) == 1) {
        ctrl->~BaseObject();
        m_controllerPool->setBufferPinned(ctrl, 0);
    }
    m_controllerPool->releaseBuffer(ctrl);
    return 0;
}

int ObjFactory::createWorkBuf(void **outBuf)
{
    *outBuf = NULL;
    if (m_workBufPool == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    void *slot = m_workBufPool->acquireBuffer();
    if (slot == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    unsigned int slotSize = m_workBufPool->getBlockPayloadSize();
    BaseObject *buf = constructWorkBuffer(slot, 4192, slotSize);
    *outBuf = buf;
    if (buf == NULL) {
        m_workBufPool->releaseBuffer(slot);
        return MUSICCORE_ERR_FAILED;
    }
    m_workBufPool->setBufferPinned(slot, 1);
    return 0;
}

int factoryDestroyWorkBuf(void *factory, void *workBuf)
{
    ObjFactory *objFactory = static_cast<ObjFactory *>(factory);
    return (objFactory != NULL)
        ? objFactory->destroyWorkBuf(static_cast<BaseObject *>(workBuf))
        : MUSICCORE_ERR_INVALID_ARG;
}

int ObjFactory::destroyWorkBuf(BaseObject *buf)
{
    if (buf == NULL)
        return 0;
    if (m_workBufPool == NULL)
        return MUSICCORE_ERR_FAILED;

    if (m_workBufPool->isBufferPinned(buf) == 1) {
        buf->~BaseObject();
        m_workBufPool->setBufferPinned(buf, 0);
    }
    m_workBufPool->releaseBuffer(buf);
    return 0;
}

int ObjFactory::getPlayerPool(char playerKind, PlayerPool **outPool)
{
    *outPool = NULL;
    PlayerPool *pool = NULL;
    if (playerKind == 0)
        pool = m_musicPool;
    else if (playerKind == 1)
        pool = m_playlistPool;
    else if (playerKind == 2)
        pool = m_streamPool;
    else
        return MUSICCORE_ERR_INVALID_ARG;

    *outPool = pool;
    if (pool == NULL)
        return MUSICCORE_ERR_FAILED;
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
