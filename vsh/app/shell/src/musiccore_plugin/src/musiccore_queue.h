/*
 * SceShell musiccore_plugin - Shared CmdNode, CmdAckWaiter, and CmdQueue declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_QUEUE_H
#define VSH_APP_SHELL_MUSICCORE_QUEUE_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>

namespace sce {
namespace MusicCore {

struct CmdAckWaiter {
    virtual ~CmdAckWaiter() {}
    paf::thread::Cond *cond;
    paf::thread::Mutex *mutex;
    int signalCount;
};

enum CmdNodeState {
    CMD_STATE_FREE = 0,
    CMD_STATE_ALLOCATED = 1,
    CMD_STATE_QUEUED = 2,
    CMD_STATE_PROCESSING = 3,
    CMD_STATE_DONE = 4,
    CMD_STATE_DEFERRED = 5
};

class CmdNode {
public:
    CmdNode(unsigned int inCapArg, unsigned int outCapArg, int *outErr,
            unsigned int totalBytes, unsigned int headerBytes);
    virtual ~CmdNode();

    int getState() const { return state; }

public:
    unsigned int padAlign;
    uint32_t param;
    union {
        uint16_t flagsAndTarget;
        struct {
            uint8_t targetChannel;
            uint8_t flagsByte;
        };
    };
    uint16_t cmdCode;
    void *inBuf;
    unsigned int inSize;
    unsigned int inCap;
    void *outBuf;
    unsigned int outSize;
    unsigned int outCap;
    int state;
    CmdAckWaiter *waiter;
    CmdNode *next;
    CmdNode *prev;
};

class CmdQueue {
public:
    static unsigned int calcRequiredSize(unsigned int inSize, unsigned int outSize,
                                         unsigned int count);

    CmdQueue(const char *name, unsigned int inSize, unsigned int outSize,
             unsigned int count, SceUID *outErr, unsigned int totalBytes);
    virtual ~CmdQueue();

    unsigned int getTotalBytes() const;
    int allocNode(CmdNode **outNode, SceUInt *timeout);
    int freeNode(CmdNode *node, SceUInt *timeout);
    int enqueue(CmdNode *node, CmdAckWaiter *waiter, SceUInt *timeout);
    int enqueueDeferred(CmdNode *node, SceUInt *timeout);
    int dequeue(CmdNode **outNode, SceUInt *timeout);
    int complete(CmdNode *node);
    int peekAt(CmdNode **outNode, int index);
    int moveToFront(CmdNode *node, unsigned int index);
    int requeue(CmdNode *node, unsigned char deferSignal);
    int flushRequeued(int maxCount);

private:
    bool mergeSeekDelta(unsigned int prevStep, unsigned int *curStep);
    bool canCoalesce(CmdNode *existing, CmdNode *incoming);
    int appendOrCoalesce(CmdNode *node, bool *outCoalesced);
    int insertAtFront(CmdNode *node, unsigned int index);
    int findByCmdCode(CmdNode *target, unsigned int *inOutIndex);

    bool isValid() const
    {
        return m_freeSema != -1 && m_readySema != -1 &&
               m_poolBase != NULL && m_mutex != -1;
    }

    bool isNodeInRange(const CmdNode *node) const
    {
        if (node == NULL || m_capacity == 0)
            return false;
        uintptr_t base = (uintptr_t)m_poolBase;
        uintptr_t last = base + m_stride * m_capacity - m_stride;
        return (uintptr_t)node <= last;
    }

    int lockQueue(SceUInt *timeout);
    void unlockQueue();

    void unlinkNode(CmdNode *node)
    {
        CmdNode *nextNode = node->next;
        CmdNode *prevNode = node->prev;
        if (nextNode != NULL)
            nextNode->prev = prevNode;
        if (prevNode != NULL)
            prevNode->next = nextNode;
        if (m_head == node)
            m_head = nextNode;
        node->next = NULL;
        node->prev = NULL;
    }

private:
    char *m_poolBase;
    CmdNode *m_head;
    unsigned int m_reservedHead;
    unsigned int m_reservedTail;
    unsigned int m_capacity;
    unsigned int m_stride;
    SceUID m_freeSema;
    SceUID m_readySema;
    SceUID m_mutex;
    int m_pendingRequeueCount;
    int m_deferredWaitCount;
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_QUEUE_H */
