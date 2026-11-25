/*
 * SceShell musiccore_plugin - player pool and object factory.
 * Original address range (3.65-CEX): 0x811D4E5C - 0x811D555C
 */

#include <std/new>
#include <stddef.h>
#include <stdint.h>

static const int MUSICCORE_ERR_FAILED = -1879113726; /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724; /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */

namespace sce {
namespace MusicCore {

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

class PlayerPool : public FixedPool {
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
    return FixedPool::calcBodySize(elemSize, count);
}

PlayerPool::PlayerPool(void *factory, char playerKind, unsigned char count, int capacity,
                       unsigned int totalBytes, unsigned int headerSize)
    : FixedPool(calcPlayerElemSize(playerKind, capacity), count, totalBytes, headerSize),
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
    if (FixedPool::calcBodySize(rawElemSize, m_playerCount) + 24 > totalBytes)
        return;

    for (unsigned int i = 0; i < m_count; ++i) {
        void *slot = allocSlot();
        if (slot == NULL)
            continue;

        int err = 0;
        BaseObject *obj = NULL;
        unsigned int slotSize = getSlotSize();

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
            freeSlot(slot);
        } else if (obj == NULL) {
            freeSlot(slot);
        } else {
            setSlotActive(slot, 1);
        }
    }
}

PlayerPool::~PlayerPool()
{
    for (unsigned int i = 0; i < m_count; ++i) {
        BaseObject *obj = (BaseObject *)getSlotObject(i);
        if (obj != NULL)
            obj->~BaseObject();
    }
}

void *PlayerPool::getPlayer(unsigned char index)
{
    PoolSlotHeader *hdr = getSlotHeader(index);
    if (hdr != NULL && hdr->active == 1)
        return hdr->object;
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
    FixedPool *m_controllerPool;
    PlayerPool *m_playlistPool;
    PlayerPool *m_musicPool;
    PlayerPool *m_streamPool;
    FixedPool *m_workBufPool;
};

unsigned int ObjFactory::calcPayloadSize(int numMusic, int numPlaylist, int numStream)
{
    unsigned int ctrlPoolSz = FixedPool::calcTotalSize(calcControllerSize(), 1);
    unsigned int musicSz = PlayerPool::calcSize(0, (unsigned char)numMusic, 1);
    unsigned int playlistSz = PlayerPool::calcSize(1, (unsigned char)numPlaylist, 10000);
    unsigned int streamSz = PlayerPool::calcSize(2, (unsigned char)numStream, 1);
    unsigned int workBufCount = (unsigned int)(2 * (numMusic + numPlaylist + numStream) + 6);
    unsigned int workBufSz = FixedPool::calcTotalSize(4224, workBufCount);
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
    unsigned int ctrlPoolSz = FixedPool::calcTotalSize(ctrlElemSz, 1);
    if (offset + ctrlPoolSz > totalBytes)
        return;
    if (cursor != NULL)
        m_controllerPool = new (cursor) FixedPool(ctrlElemSz, 1, ctrlPoolSz, 16);
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
    unsigned int workBufSz = FixedPool::calcTotalSize(4224, workBufCount);
    if (offset + workBufSz > totalBytes)
        return;
    if (cursor != NULL)
        m_workBufPool = new (cursor) FixedPool(4224, workBufCount, workBufSz, 16);
}

ObjFactory::~ObjFactory()
{
    if (m_workBufPool != NULL) {
        m_workBufPool->~FixedPool();
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
        m_controllerPool->~FixedPool();
        m_controllerPool = NULL;
    }
}

int ObjFactory::createController(void **outCtrl)
{
    *outCtrl = NULL;
    if (m_controllerPool == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    int err = 0;
    void *slot = m_controllerPool->allocSlot();
    if (slot == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    unsigned int slotSize = m_controllerPool->getSlotSize();
    BaseObject *ctrl = constructController(slot, this, &err, slotSize);
    *outCtrl = ctrl;
    if (ctrl == NULL) {
        err = MUSICCORE_ERR_FAILED;
        m_controllerPool->freeSlot(slot);
    } else {
        m_controllerPool->setSlotActive(slot, 1);
    }
    return err;
}

int ObjFactory::destroyController(BaseObject *ctrl)
{
    if (ctrl == NULL)
        return 0;
    if (m_controllerPool == NULL)
        return MUSICCORE_ERR_FAILED;

    if (m_controllerPool->isSlotActive(ctrl) == 1) {
        ctrl->~BaseObject();
        m_controllerPool->setSlotActive(ctrl, 0);
    }
    m_controllerPool->freeSlot(ctrl);
    return 0;
}

int ObjFactory::createWorkBuf(void **outBuf)
{
    *outBuf = NULL;
    if (m_workBufPool == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    void *slot = m_workBufPool->allocSlot();
    if (slot == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    unsigned int slotSize = m_workBufPool->getSlotSize();
    BaseObject *buf = constructWorkBuffer(slot, 4192, slotSize);
    *outBuf = buf;
    if (buf == NULL) {
        m_workBufPool->freeSlot(slot);
        return MUSICCORE_ERR_FAILED;
    }
    m_workBufPool->setSlotActive(slot, 1);
    return 0;
}

int ObjFactory::destroyWorkBuf(BaseObject *buf)
{
    if (buf == NULL)
        return 0;
    if (m_workBufPool == NULL)
        return MUSICCORE_ERR_FAILED;

    if (m_workBufPool->isSlotActive(buf) == 1) {
        buf->~BaseObject();
        m_workBufPool->setSlotActive(buf, 0);
    }
    m_workBufPool->freeSlot(buf);
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
