/*
 * SceShell musiccore_plugin - command node and command queue.
 * Original address range (3.65-CEX): 0x811D3964 - 0x811D4E5C
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>

#include "musiccore_queue.h"

static const int MUSICCORE_ERR_FAILED = -1879113726; /* 0x8FFF0002 */
static const int MUSICCORE_ERR_TIMEOUT = -1879113725; /* 0x8FFF0003 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724; /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */
static const int MUSICCORE_ERR_NOT_FOUND = -1879113716; /* 0x8FFF000C */
static const int MUSICCORE_ERR_QUEUE_TIMEOUT = -1879113714; /* 0x8FFF000E */
static const int KERNEL_ERR_WAIT_TIMEOUT = -2147319803; /* 0x80028005 */

namespace sce {
namespace MusicCore {

#define MUSICCORE_ALIGN_UP8(sz) ((((sz) - 1u) & ~7u) + 8u)

CmdNode::CmdNode(unsigned int inCapArg, unsigned int outCapArg, int *outErr,
                 unsigned int totalBytes, unsigned int headerBytes)
    : padAlign(0),
      param(0),
      flagsAndTarget(0),
      cmdCode(0),
      inBuf(NULL),
      inSize(0),
      inCap(inCapArg),
      outBuf(NULL),
      outSize(0),
      outCap(outCapArg),
      state(CMD_STATE_FREE),
      waiter(NULL),
      next(NULL),
      prev(NULL)
{
    unsigned int offset = MUSICCORE_ALIGN_UP8(headerBytes);
    if (totalBytes < offset) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    *outErr = 0;
    unsigned int afterIn = offset + inCap;
    if (totalBytes < afterIn) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    char *selfBytes = reinterpret_cast<char *>(this);
    inBuf = selfBytes + offset;
    if (totalBytes < afterIn + outCap) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }
    outBuf = selfBytes + afterIn;
}

CmdNode::~CmdNode()
{
    if (inBuf != NULL)
        inBuf = NULL;
    if (outBuf != NULL)
        outBuf = NULL;
    state = CMD_STATE_FREE;
}

int CmdQueue::lockQueue(SceUInt *timeout)
{
    if (!isValid())
        return MUSICCORE_ERR_FAILED;
    int ret = sceKernelLockMutex(m_mutex, 1, timeout);
    if (ret < 0) {
        if (ret == KERNEL_ERR_WAIT_TIMEOUT)
            return MUSICCORE_ERR_TIMEOUT;
        return ret;
    }
    return 0;
}

void CmdQueue::unlockQueue()
{
    if (isValid())
        sceKernelUnlockMutex(m_mutex, 1);
}

unsigned int CmdQueue::calcRequiredSize(unsigned int inSize, unsigned int outSize, unsigned int count)
{
    unsigned int nodeStride = MUSICCORE_ALIGN_UP8(inSize) + MUSICCORE_ALIGN_UP8(outSize) + 64 + 8;
    return nodeStride * count + 48;
}

unsigned int CmdQueue::getTotalBytes() const
{
    return m_stride * m_capacity + 48;
}

CmdQueue::CmdQueue(const char *name, unsigned int inSize, unsigned int outSize,
                   unsigned int count, SceUID *outErr, unsigned int totalBytes)
    : m_poolBase(NULL),
      m_head(NULL),
      m_reservedHead(0),
      m_reservedTail(0),
      m_capacity(count),
      m_stride(MUSICCORE_ALIGN_UP8(inSize) + MUSICCORE_ALIGN_UP8(outSize) + 64),
      m_freeSema(-1),
      m_readySema(-1),
      m_mutex(-1),
      m_pendingRequeueCount(0),
      m_deferredWaitCount(0)
{
    *outErr = 0;
    if (m_capacity == 0 || m_stride < 64) {
        *outErr = MUSICCORE_ERR_FAILED;
        return;
    }
    if (totalBytes < 48 || totalBytes < m_stride * m_capacity + 48) {
        *outErr = MUSICCORE_ERR_NO_MEMORY;
        return;
    }

    char *selfBytes = reinterpret_cast<char *>(this);
    char *cursor = selfBytes + 48u;
    m_poolBase = cursor;

    for (unsigned int i = 0; i < m_capacity; ++i) {
        uintptr_t alignedAddr = (((uintptr_t)cursor - 1) & ~(uintptr_t)7) + 8;
        CmdNode *slot = (CmdNode *)alignedAddr;
        CmdNode *node = NULL;
        if (slot != NULL)
            node = new (slot) CmdNode(inSize, outSize, outErr, m_stride, 64);
        if (node == NULL) {
            *outErr = MUSICCORE_ERR_NO_MEMORY;
            m_stride = i;
            return;
        }
        if (*outErr != 0) {
            node->~CmdNode();
            m_stride = i;
            return;
        }
        cursor = (char *)slot + m_stride;
    }

    SceUID freeSema = sceKernelCreateSema(name, 0, (int)m_capacity, (int)m_capacity, NULL);
    m_freeSema = freeSema;
    if (freeSema < 0) {
        *outErr = freeSema;
        m_freeSema = -1;
        return;
    }

    SceUID readySema = sceKernelCreateSema(name, 0, 0, (int)m_capacity, NULL);
    m_readySema = readySema;
    if (readySema < 0) {
        *outErr = readySema;
        m_readySema = -1;
        return;
    }

    SceUID mutex = sceKernelCreateMutex(name, 0, 0, NULL);
    m_mutex = mutex;
    if (mutex < 0) {
        *outErr = mutex;
        m_mutex = -1;
    }
}

CmdQueue::~CmdQueue()
{
    if (m_poolBase != NULL) {
        char *cursor = m_poolBase;
        for (unsigned int i = 0; i < m_capacity; ++i) {
            CmdNode *node = (CmdNode *)cursor;
            node->~CmdNode();
            cursor += m_stride;
        }
        m_poolBase = NULL;
    }
    if (m_freeSema != -1) {
        sceKernelCloseSema(m_freeSema);
        m_freeSema = -1;
    }
    if (m_readySema != -1) {
        sceKernelCloseSema(m_readySema);
        m_readySema = -1;
    }
    if (m_mutex != -1) {
        sceKernelCloseMutex(m_mutex);
        m_mutex = -1;
    }
}

int CmdQueue::allocNode(CmdNode **outNode, SceUInt *timeout)
{
    if (outNode == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    bool needBlockingWait = false;
    int waitRet = 0;

    do {
        SceUInt zeroTimeout = 0;
        waitRet = sceKernelWaitSema(m_freeSema, 1, &zeroTimeout);
        if (waitRet != KERNEL_ERR_WAIT_TIMEOUT)
            break;

        CmdNode *deferred = m_head;
        while (deferred != NULL && deferred->state != CMD_STATE_DEFERRED)
            deferred = deferred->next;

        if (deferred == NULL) {
            needBlockingWait = true;
        } else {
            int lockRet = lockQueue(timeout);
            if (lockRet != 0)
                return lockRet;
            unlinkNode(deferred);
            deferred->state = CMD_STATE_FREE;
            unlockQueue();
        }
    } while (!needBlockingWait);

    if (needBlockingWait)
        waitRet = sceKernelWaitSema(m_freeSema, 1, timeout);

    if (waitRet < 0) {
        if (waitRet == KERNEL_ERR_WAIT_TIMEOUT)
            return MUSICCORE_ERR_TIMEOUT;
        return waitRet;
    }

    int lockRet = lockQueue(timeout);
    if (lockRet != 0) {
        sceKernelSignalSema(m_freeSema, 1);
        return lockRet;
    }

    *outNode = NULL;
    char *cursor = m_poolBase;
    CmdNode *found = NULL;
    for (unsigned int i = 0; i < m_capacity; ++i) {
        CmdNode *candidate = (CmdNode *)cursor;
        if (candidate->state == CMD_STATE_FREE) {
            found = candidate;
            break;
        }
        cursor += m_stride;
    }

    int ret = 0;
    if (found != NULL) {
        found->state = CMD_STATE_ALLOCATED;
        *outNode = found;
    } else {
        ret = MUSICCORE_ERR_FAILED;
    }

    unlockQueue();
    return ret;
}

int CmdQueue::freeNode(CmdNode *node, SceUInt *timeout)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    int ret = lockQueue(timeout);
    if (ret != 0)
        return ret;

    bool released = false;
    if (node->state != CMD_STATE_FREE) {
        unlinkNode(node);
        node->state = CMD_STATE_FREE;
        released = true;
    }

    unlockQueue();

    if (released) {
        int sigRet = sceKernelSignalSema(m_freeSema, 1);
        if (sigRet < 0)
            return sigRet;
    }
    return ret;
}

bool CmdQueue::mergeSeekDelta(unsigned int prevStep, unsigned int *curStep)
{
    unsigned int cur = *curStep;
    if (cur <= 0xFFFFFFFBu)
        return true;

    if (cur > 0xFFFFFFFDu) {
        uint16_t prevHi = (uint16_t)(prevStep >> 16);
        if (prevStep == 0xFFFFFFFCu || prevStep == 0xFFFFFFFDu ||
            prevHi == 0xFFFDu || prevHi == 0xFFFEu) {
            *curStep = prevStep;
        }
        return true;
    }

    if (prevStep == 0xFFFFFFFCu) {
        if (cur == 0xFFFFFFFCu)
            *curStep = 0xFFFD0002u;
        else if (cur == 0xFFFFFFFDu)
            *curStep = 0xFFFFFFFFu;
        return true;
    }
    if (prevStep == 0xFFFFFFFDu) {
        if (cur == 0xFFFFFFFCu)
            *curStep = 0xFFFFFFFFu;
        else if (cur == 0xFFFFFFFDu)
            *curStep = 0xFFFE0002u;
        return true;
    }

    uint16_t prevHi = (uint16_t)(prevStep >> 16);
    if (prevHi == 0xFFFDu) {
        if (cur == 0xFFFFFFFCu) {
            unsigned int next = prevStep + 1;
            *curStep = next;
            if ((uint16_t)next > 0x14u)
                *curStep = 0xFFFD0014u;
        } else if (cur == 0xFFFFFFFDu) {
            unsigned int next = prevStep - 1;
            *curStep = next;
            if (prevStep == 0xFFFD0002u)
                *curStep = 0xFFFFFFFCu;
        }
        return true;
    }
    if (prevHi == 0xFFFEu) {
        if (cur == 0xFFFFFFFCu) {
            unsigned int next = prevStep - 1;
            *curStep = next;
            if (prevStep == 0xFFFE0002u)
                *curStep = 0xFFFFFFFDu;
        } else if (cur == 0xFFFFFFFDu) {
            unsigned int next = prevStep + 1;
            *curStep = next;
            if ((uint16_t)next > 0x14u)
                *curStep = 0xFFFE0014u;
        }
        return true;
    }

    if (prevStep != 0xFFFFFFFEu)
        return prevStep == 0xFFFFFFFFu;
    return true;
}

struct SeekTrackPayload {
    unsigned char header[160];
    unsigned char pauseFlag;
    unsigned char padPause[3];
    unsigned int stepDelta;
    unsigned int seekPos;
};

struct PlaylistSeekPayload {
    unsigned int stepDelta;
    unsigned int headerWords[6];
    unsigned int seekPos;
    unsigned int padAlign;
    unsigned int playlistId;
};

bool CmdQueue::canCoalesce(CmdNode *existing, CmdNode *incoming)
{
    if (existing == NULL || incoming == NULL || existing->state != CMD_STATE_QUEUED)
        return false;

    uint16_t existingCmd = existing->cmdCode;
    uint16_t incomingCmd = incoming->cmdCode;

    switch (existingCmd) {
    case 0x1103:
    case 0x1104:
    case 0x1108:
    case 0x1109:
    case 0x110C:
    case 0x110D:
    case 0x110E:
    case 0x1112:
    case 0x2105:
    case 0x21A1:
    case 0x21A2:
    case 0x21A3:
    case 0x21A4:
    case 0x21A5:
        return incomingCmd == existingCmd;

    case 0x1105:
    case 0x1106:
    case 0x1107:
        if (incomingCmd == 0x1105 || incomingCmd == 0x1106 || incomingCmd == 0x1107) {
            if (existingCmd == 0x1106 && incomingCmd == 0x1106) {
                SeekTrackPayload *existingPayload = (SeekTrackPayload *)existing->inBuf;
                SeekTrackPayload *incomingPayload = (SeekTrackPayload *)incoming->inBuf;
                if (existingPayload->pauseFlag == 1)
                    incomingPayload->pauseFlag = 1;
            }
            return true;
        }
        return false;

    case 0x110B:
        if (incomingCmd == 0x110B) {
            SeekTrackPayload *existingPayload = (SeekTrackPayload *)existing->inBuf;
            SeekTrackPayload *incomingPayload = (SeekTrackPayload *)incoming->inBuf;
            unsigned int prevPos = existingPayload->seekPos;
            bool merged = mergeSeekDelta(existingPayload->stepDelta, &incomingPayload->stepDelta);
            if (incomingPayload->seekPos == 0x80000000u && prevPos != 0x80000000u)
                incomingPayload->seekPos = prevPos;
            return merged;
        }
        return false;

    case 0x2102: {
        PlaylistSeekPayload *existingPl = (PlaylistSeekPayload *)existing->inBuf;
        PlaylistSeekPayload *incomingPl = (PlaylistSeekPayload *)incoming->inBuf;
        if (existingPl->playlistId != 0 && incomingPl->playlistId == existingPl->playlistId &&
            incomingCmd == 0x2102 && (existingPl->stepDelta >> 16) != 0xFFFCu) {
            bool merged = mergeSeekDelta(existingPl->stepDelta, &incomingPl->stepDelta);
            if (incomingPl->seekPos == 0x80000000u && existingPl->seekPos != 0x80000000u)
                incomingPl->seekPos = existingPl->seekPos;
            return merged;
        }
        return false;
    }

    default:
        return false;
    }
}

int CmdQueue::appendOrCoalesce(CmdNode *node, bool *outCoalesced)
{
    CmdNode *tail = m_head;
    if (tail == NULL) {
        node->next = NULL;
        node->prev = NULL;
        m_head = node;
        node->state = CMD_STATE_QUEUED;
        return 0;
    }

    unsigned int count = 1;
    while (tail->next != NULL) {
        tail = tail->next;
        ++count;
    }

    if (count >= m_capacity)
        return MUSICCORE_ERR_FAILED;

    bool coalesced = false;
    int ret = 0;

    if (canCoalesce(tail, node)) {
        node->prev = tail;
        node->next = tail->next;
        tail->next = node;
        node->state = CMD_STATE_QUEUED;

        unlinkNode(tail);
        tail->state = CMD_STATE_FREE;

        int sigRet = sceKernelSignalSema(m_freeSema, 1);
        if (sigRet >= 0)
            coalesced = true;
        else
            ret = sigRet;
    }

    if (ret == 0) {
        if (!coalesced) {
            node->prev = tail;
            node->next = tail->next;
            tail->next = node;
            node->state = CMD_STATE_QUEUED;
        }
        *outCoalesced = coalesced;
    }
    return ret;
}

int CmdQueue::insertAtFront(CmdNode *node, unsigned int index)
{
    if (index >= m_capacity)
        return MUSICCORE_ERR_INVALID_ARG;

    CmdNode *head = m_head;
    if (head != NULL) {
        CmdNode *prev = head->prev;
        if (prev != NULL) {
            prev->next = node;
            node->prev = prev;
        }
        head->prev = node;
    } else {
        node->prev = NULL;
    }
    node->next = head;
    m_head = node;
    return 0;
}

int CmdQueue::findByCmdCode(CmdNode *target, unsigned int *inOutIndex)
{
    if (target == NULL || m_head == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    CmdNode *cursor = m_head;
    unsigned int idx = 0;
    while (idx != *inOutIndex) {
        cursor = cursor->next;
        ++idx;
        if (cursor == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
    }

    while (cursor->cmdCode != target->cmdCode) {
        cursor = cursor->next;
        ++idx;
        if (cursor == NULL)
            return MUSICCORE_ERR_INVALID_ARG;
    }

    *inOutIndex = idx;
    return 0;
}

int CmdQueue::enqueue(CmdNode *node, CmdAckWaiter *waiter, SceUInt *timeout)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    int ret = lockQueue(timeout);
    if (ret != 0)
        return ret;

    bool enqueued = false;
    bool coalesced = false;
    bool replacedDeferred = false;
    int deferredSignals = 0;

    if (node->state != CMD_STATE_ALLOCATED) {
        ret = MUSICCORE_ERR_INVALID_ARG;
    } else {
        unsigned int searchIdx = 0;
        CmdNode *deferredMatch = NULL;

        while (true) {
            int findRet = findByCmdCode(node, &searchIdx);
            if (findRet == 0) {
                CmdNode *cursor = m_head;
                unsigned int idx = 0;
                while (cursor != NULL && idx != searchIdx) {
                    cursor = cursor->next;
                    ++idx;
                }
                if (cursor != NULL && cursor->state == CMD_STATE_DEFERRED) {
                    deferredMatch = cursor;
                    break;
                }
            }
            ++searchIdx;
            if (findRet != 0) {
                ret = appendOrCoalesce(node, &coalesced);
                break;
            }
        }

        if (deferredMatch != NULL) {
            replacedDeferred = true;
            unlinkNode(deferredMatch);
            deferredMatch->state = CMD_STATE_FREE;
            if (searchIdx == 0) {
                deferredSignals = m_deferredWaitCount;
                m_deferredWaitCount = 0;
            }
            ret = insertAtFront(node, searchIdx);
            if (ret == 0)
                node->state = CMD_STATE_QUEUED;
        }

        if (ret == 0) {
            enqueued = true;
            node->waiter = waiter;
        }
    }

    unlockQueue();

    if (!coalesced) {
        if (replacedDeferred) {
            int sigRet = sceKernelSignalSema(m_freeSema, 1);
            if (sigRet < 0)
                ret = sigRet;
        }
        if (deferredSignals != 0) {
            int sigRet = sceKernelSignalSema(m_readySema, deferredSignals);
            if (sigRet < 0)
                return sigRet;
        } else if (enqueued) {
            if (!replacedDeferred) {
                int sigRet = sceKernelSignalSema(m_readySema, 1);
                if (sigRet < 0)
                    return sigRet;
            }
            if (node->waiter != NULL) {
                CmdAckWaiter *ack = node->waiter;
                ack->mutex->Lock();
                if (ack->signalCount != 0) {
                    --ack->signalCount;
                } else {
                    ack->cond->Wait(0);
                    if (ack->signalCount != 0)
                        --ack->signalCount;
                }
                ack->mutex->Unlock();
            }
        }
    }

    return ret;
}

int CmdQueue::enqueueDeferred(CmdNode *node, SceUInt *timeout)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    int ret = lockQueue(timeout);
    if (ret != 0)
        return ret;

    bool signaled = false;
    if (node->state != CMD_STATE_ALLOCATED) {
        ret = MUSICCORE_ERR_INVALID_ARG;
    } else {
        if (m_head != NULL) {
            CmdNode *tail = m_head;
            unsigned int count = 1;
            while (tail->next != NULL) {
                tail = tail->next;
                ++count;
            }
            if (count >= m_capacity) {
                ret = MUSICCORE_ERR_FAILED;
            } else {
                node->prev = tail;
                node->next = tail->next;
                tail->next = node;
                node->state = CMD_STATE_QUEUED;
            }
        } else {
            node->next = NULL;
            node->prev = NULL;
            m_head = node;
            node->state = CMD_STATE_QUEUED;
        }

        if (ret == 0) {
            node->waiter = NULL;
            node->state = CMD_STATE_DEFERRED;
            signaled = true;
        }
    }

    unlockQueue();

    if (signaled) {
        int sigRet = sceKernelSignalSema(m_readySema, 1);
        if (sigRet < 0)
            return sigRet;
    }
    return ret;
}

int CmdQueue::dequeue(CmdNode **outNode, SceUInt *timeout)
{
    if (outNode == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    int waitRet = sceKernelWaitSema(m_readySema, 1, timeout);
    if (waitRet < 0) {
        if (waitRet == KERNEL_ERR_WAIT_TIMEOUT)
            return MUSICCORE_ERR_QUEUE_TIMEOUT;
        return waitRet;
    }

    int ret = lockQueue(timeout);
    if (ret != 0)
        return ret;

    bool handled = false;
    CmdNode *cursor = m_head;
    if (cursor != NULL) {
        int firstState = cursor->state;
        while (cursor != NULL) {
            if (firstState == CMD_STATE_DEFERRED) {
                ret = MUSICCORE_ERR_TIMEOUT;
                ++m_deferredWaitCount;
                handled = true;
                break;
            }
            if (cursor->state == CMD_STATE_QUEUED) {
                cursor->state = CMD_STATE_PROCESSING;
                *outNode = cursor;
                handled = true;
                break;
            }
            cursor = cursor->next;
        }
    }

    unlockQueue();

    if (!handled)
        return MUSICCORE_ERR_FAILED;
    return ret;
}

int CmdQueue::complete(CmdNode *node)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    int ret = 0;
    bool released = false;

    sceKernelLockMutex(m_mutex, 1, NULL);

    if (node->state == CMD_STATE_PROCESSING) {
        CmdAckWaiter *ack = node->waiter;
        node->state = CMD_STATE_DONE;
        if (ack != NULL) {
            ack->mutex->Lock();
            sceKernelDelayThread(200);
            ++ack->signalCount;
            ack->cond->Notify();
            ack->mutex->Unlock();
        } else {
            unlinkNode(node);
            node->state = CMD_STATE_FREE;
            released = true;
        }
    } else {
        ret = MUSICCORE_ERR_INVALID_ARG;
    }

    unlockQueue();

    if (released) {
        int sigRet = sceKernelSignalSema(m_freeSema, 1);
        if (sigRet < 0)
            return sigRet;
    }
    return ret;
}

int CmdQueue::peekAt(CmdNode **outNode, int index)
{
    if (outNode == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    sceKernelLockMutex(m_mutex, 1, NULL);

    bool found = false;
    CmdNode *cursor = m_head;
    int curIdx = 0;
    while (cursor != NULL) {
        if (curIdx == index) {
            *outNode = cursor;
            found = true;
            break;
        }
        cursor = cursor->next;
        ++curIdx;
    }

    unlockQueue();

    if (!found)
        return MUSICCORE_ERR_NOT_FOUND;
    return 0;
}

int CmdQueue::moveToFront(CmdNode *node, unsigned int index)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    sceKernelLockMutex(m_mutex, 1, NULL);

    unlinkNode(node);
    int ret = insertAtFront(node, index);
    if (ret == 0)
        unlockQueue();
    return ret;
}

int CmdQueue::requeue(CmdNode *node, unsigned char deferSignal)
{
    if (!isNodeInRange(node))
        return MUSICCORE_ERR_INVALID_ARG;
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    sceKernelLockMutex(m_mutex, 1, NULL);

    if (node->state != CMD_STATE_PROCESSING)
        return MUSICCORE_ERR_FAILED;

    node->state = CMD_STATE_QUEUED;
    unlockQueue();

    if (deferSignal == 1) {
        ++m_pendingRequeueCount;
    } else {
        int sigRet = sceKernelSignalSema(m_readySema, 1);
        if (sigRet < 0)
            return sigRet;
    }
    return 0;
}

int CmdQueue::flushRequeued(int maxCount)
{
    if (!isValid())
        return MUSICCORE_ERR_FAILED;

    while (m_pendingRequeueCount != 0 && maxCount != 0) {
        int sigRet = sceKernelSignalSema(m_readySema, 1);
        if (sigRet < 0)
            return sigRet;
        --maxCount;
        --m_pendingRequeueCount;
    }
    return 0;
}

} /* namespace MusicCore */
} /* namespace sce */
