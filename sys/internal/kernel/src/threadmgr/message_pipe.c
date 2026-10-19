/*
 * SceKernelThreadMgr: message_pipe.c, the message pipes (SceUIDMsgPipeClass,
 * a subclass of SceUIDEventClass). A pipe is a ring buffer in a memory
 * block of the process it belongs to; senders and receivers move data
 * through it in vectors of buffers, waiting (sender or receiver side, one
 * at a time) when it is full or empty.
 * The user syscall wrappers are still
 * threadmgr.c's stubs.
 */

#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "timer.h"
#include "fast_mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>
#include <acmgr/acmgr.h>
#include <threadmgr/threadmgr.h>
#include <modulemgr/modulemgr.h>

#define MSG_PIPE_WAIT_SEND		1	/* waitDirection: senders wait */
#define MSG_PIPE_WAIT_RECEIVE		2	/* receivers wait */
#define MSG_PIPE_MODE_ALL		0x00000001	/* move all of it, or nothing */
#define MSG_PIPE_MODE_KERNEL_VECTOR	0x00000010	/* the vector array is kernel memory */
#define MSG_PIPE_MODE_PEEK		0x00000100	/* receive without consuming */
#define MSG_PIPE_ATTR_UNLIMITED		0x00004000
#define EVENT_PATTERN_RESERVED_BIT	0x00010000
#define SDK_VERSION_MSG_PIPE_EMPTY	0x03100000	/* PLACEHOLDER: from here an empty pipe ... */

/* the ring buffer (pipe +0x38) */
struct ThreadmgrMsgPipeRing {
	uint8_t *base;				/* 0x38 */
	uint8_t *write;				/* 0x3c */
	uint8_t *read;				/* 0x40 */
	SceSize size;				/* 0x44 */
	SceSize used;				/* 0x48 */
};

/* SceUIDMsgPipeObject (0x78) */
typedef struct ThreadmgrMsgPipe {
	ThreadmgrEvent event;
	struct ThreadmgrMsgPipeRing ring;	/* 0x38: from here on cleared by the constructor */
	uint32_t partial;			/* 0x4c: the first waiter is part-way served */
	SceUID uid;				/* 0x50 */
	SceUID processId;			/* 0x54: the process the buffer is in */
	SceUID memBlockId;			/* 0x58: the buffer's, or -1 */
	int waitDirection;			/* 0x5c: MSG_PIPE_WAIT_* */
	int waitCount;				/* 0x60 */
	SceUID creatorPid;			/* 0x64 */
	uint32_t flags;				/* 0x68: bit 0: openCapability is set */
	int openCapability;			/* 0x6c: needed to open it (SceSblACMgrForKernel_8241AB5C) */
	ListNode processLink;	/* 0x70: in its process's msg pipes */
} ThreadmgrMsgPipe;

_Static_assert(sizeof(ThreadmgrMsgPipe) == 0x78, "SceUIDMsgPipeObject size");

#define MSG_PIPE_CLASS()	(s_threadmgrRuntime.manager->msgPipeClass)
#define MSG_PIPE_QUEUE(pipe)	((struct ThreadmgrWaitNode *)&(pipe)->event.next)

static inline int MsgPipeUnknownIdError(void)
{
	return !ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID :
			SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID | SYSCALL_ERROR_BIT;
}

/* tells a waiter how much it has moved (in its own process's memory) */
static inline int setMsgPipeResult(struct ThreadCB *thread, SceSize *moved)
{
	if (thread->timeoutProcessId == SCE_KERNEL_PROCESS_ID) {
		*thread->msgPipeResult = *moved;
		return 0;
	}
	return sceKernelCopyToUserProc(thread->timeoutProcessId, thread->msgPipeResult, moved,
					sizeof(*moved));
}

static int uidMsgPipe_Constructor(void *object)
{
	memset(&((ThreadmgrMsgPipe *)object)->ring, 0,
	       sizeof(ThreadmgrMsgPipe) - offsetof(ThreadmgrMsgPipe, ring));
	return 0;
}

/*
 * SceUIDMsgPipeClass's destructor: off its process's list (retrying while
 * the list is busy), waiters told what they moved, the owner's counts and
 * the buffer released.
 */
static int uidMsgPipe_Destructor(void *object)
{
	ThreadmgrMsgPipe *pipe = object;
	struct ThreadmgrWaitNode *queue = MSG_PIPE_QUEUE(pipe), *node;
	struct ThreadCB *thread;
	SceProcessObject *process;
	SceObjectBase *pObject;
	SceUID memBlockId, pid;
	SceSize moved;
	int intr, ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 4892);
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pipe->event.lock);
		if (sceGUIDGetObject(pipe->processId, &pObject) < 0)
			break;
		if (sceKernelTryLockFastMutex(&process->msgPipeLock) >= 0) {
			ListLinkRemove(&pipe->processLink);
			if (sceKernelUnlockFastMutex(&process->msgPipeLock) < 0) {
				SCE_DBG_ASSERT_FAIL(4912);
				sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
				return 0;
			}
			break;
		}
	process = (SceProcessObject *)pObject;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
	}
	memBlockId = pipe->memBlockId;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pipe->event.lock);
	for (node = pipe->event.next; node != queue; node = node->next) {
		if (!(node->owner & 1))
			continue;
		thread = (struct ThreadCB *)node;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
			if (pipe->waitDirection == MSG_PIPE_WAIT_SEND) {
				if (thread->msgPipeResult != NULL) {
					moved = thread->msgPipeSize - thread->msgPipeRemaining;
					ret = setMsgPipeResult(thread, &moved);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 4948,
							"warning: uidMsgPipe_Destructor(): sceKernelCopyToUserProc()=0x%08X",
							ret);
				}
			} else if (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE) {
				if (thread->msgPipeResult != NULL) {
					moved = thread->msgPipeSize - thread->msgPipeRemaining;
					ret = setMsgPipeResult(thread, &moved);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 4964,
							"warning: uidMsgPipe_Destructor(): sceKernelCopyToUserProc()=0x%08X",
							ret);
				}
			} else {
				SCE_DBG_PRINTF(0, 4970,
					" k: uidMsgPipe_Destructor(), unexpected waitType(0x%08X)\n",
					pipe->waitDirection);
			}
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
	pid = pipe->event.ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
			if (pipe->event.attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (!(pipe->event.attr & MSG_PIPE_ATTR_UNLIMITED))
				process->uidCount--;
			else
				process->uidUnlimitedCount--;
			process->msgPipeMemory -= pipe->ring.size;
			process->msgPipeCount--;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&process->resourceLock, intr);
		}
		sceGUIDReleaseObject(pid);
	}
	if (memBlockId == -1 || sceKernelFreeMemBlock(memBlockId) >= 0)
		memset(&pipe->ring, 0, sizeof(ThreadmgrMsgPipe) - offsetof(ThreadmgrMsgPipe, ring));
	else
		SCE_DBG_ASSERT_LEVEL(0, 0, 5001);
	return 0;
}

/* a pipe's information */
static int getMsgPipeInfo(SceUID msgPipeId, SceKernelMsgPipeInfo *info)
{
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject) < 0) {
		ret = MsgPipeUnknownIdError();
	} else {
		pipe = (ThreadmgrMsgPipe *)pObject;
		info->msgPipeId = msgPipeId;
		strncpy(info->name, SceSysmemForDriver_E655852F(msgPipeId), sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(&pipe->event.lock);
		info->bufferSize = pipe->ring.size;
		info->freeSize = pipe->ring.size - pipe->ring.used;
		info->attr = pipe->event.attr;
		if (pipe->waitDirection == MSG_PIPE_WAIT_SEND)
			info->numSendWaitThreads = pipe->event.waiting;
		else if (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE)
			info->numReceiveWaitThreads = pipe->event.waiting;
		sceKernelSpinlockLowUnlock(&pipe->event.lock);
		ret = 0;
	}
	/* Sony's: released even when the reference failed */
	sceGUIDReleaseObject(msgPipeId);
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* memory of process pid (0: the caller's) into the kernel */
static inline __attribute__((always_inline)) int copyToKernel(SceUID pid, void *dst,
							       const void *src, SceSize size)
{
	int ret;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (pid == SCE_KERNEL_PROCESS_ID) {
		__builtin_memcpy(dst, src, size);
		return 0;
	}
	ret = sceKernelCopyFromUserProc(pid, dst, src, size);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 214,
			"sceKernelCopyFromUserProc(0x%08x, %p, 0x%08x, 0x%08x): error 0x%08x\n",
			pid, dst, src, size, ret);
	return ret;
}

/* kernel memory out to process pid (0: the caller's) */
static inline __attribute__((always_inline)) int copyFromKernel(SceUID pid, void *dst,
								 const void *src, SceSize size)
{
	int ret;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (pid == SCE_KERNEL_PROCESS_ID) {
		memcpy(dst, src, size);
		return 0;
	}
	ret = sceKernelCopyToUserProc(pid, dst, src, size);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 234,
			"sceKernelCopyToUserProc(0x%08X, 0x%08x, %p, 0x%08x): error 0x%08x\n",
			pid, dst, src, size, ret);
	return ret;
}

/* size bytes from src into the ring at its write end (wrapping once) */
static inline __attribute__((always_inline)) int sendPipe(struct ThreadmgrMsgPipeRing *ring,
	SceUID pid, const uint8_t *src, SceSize size)
{
	uint8_t *end;
	SceSize chunk, rest;
	int ret;

	if (ring->read <= ring->write) {
		/* first up to the end of the buffer */
		chunk = ring->base + ring->size - ring->write;
		if (size < chunk)
			chunk = size;
		ret = copyToKernel(pid, ring->write, src, chunk);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 254, "copyToKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n",
				       pid, ring->write, src, chunk, ret);
			return ret;
		}
		src += chunk;
		rest = size - chunk;
		ring->write += chunk;
		end = ring->base + ring->size;
		ring->used += chunk;
		if (end <= ring->write)
			ring->write = ring->base;
	} else {
		end = ring->base + ring->size;
		rest = size;
	}
	if (end < ring->write + rest)
		SCE_PANIC(267);
	ret = copyToKernel(pid, ring->write, src, rest);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 272, "copyToKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n", pid,
			       ring->write, src, rest, ret);
		return ret;
	}
	ring->write += rest;
	ring->used += rest;
	return 0;
}

/*
 * size bytes of the vector, from buffer *index at *offset on, into the
 * ring; the position moves on unless keep is set.
 */
int sendPipeVector(struct ThreadmgrMsgPipeRing *ring, SceUID pid,
			  const SceKernelMsgPipeVector *vector, int *index, SceSize *offset,
			  SceSize size, int keep, int kernelVector)
{
	SceKernelMsgPipeVector buffer;
	SceSize off = *offset, len;
	int i = *index, ret;

	for (;;) {
		if (kernelVector) {
			buffer = vector[i];
		} else {
			ret = copyToKernel(pid, &buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 305,
					"copyToKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n", pid,
					&buffer, &vector[i], sizeof(buffer), ret);
				return ret;
			}
		}
		len = buffer.bufSize - off;
		if (size <= len)
			len = size;
		ret = sendPipe(ring, pid, (const uint8_t *)buffer.pBase + off, len);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 313, "sendPipe(%p, 0x%08X, 0x%08X, 0x%08x): error 0x%08x\n",
				       ring, pid, (uint8_t *)buffer.pBase + off, len, ret);
			return ret;
		}
		off += len;
		size -= len;
		if (buffer.bufSize <= off) {
			off = 0;
			i++;
		}
		if (size == 0)
			break;
	}
	if (!keep) {
		*index = i;
		*offset = off;
	}
	return ret;
}

/* size bytes from the ring at its read end (wrapping once) into dst */
static inline __attribute__((always_inline)) int recvPipe(struct ThreadmgrMsgPipeRing *ring,
	SceUID pid, uint8_t *dst, SceSize size)
{
	uint8_t *end;
	SceSize chunk, rest;
	int ret;

	if (ring->write <= ring->read) {
		chunk = ring->base + ring->size - ring->read;
		if (size < chunk)
			chunk = size;
		ret = copyFromKernel(pid, dst, ring->read, chunk);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 352, "copyFromKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n",
				       pid, dst, ring->read, chunk, ret);
			return ret;
		}
		dst += chunk;
		rest = size - chunk;
		ring->read += chunk;
		end = ring->base + ring->size;
		ring->used -= chunk;
		if (end <= ring->read)
			ring->read = ring->base;
	} else {
		end = ring->base + ring->size;
		rest = size;
	}
	if (end < ring->read + rest)
		SCE_PANIC(365);
	ret = copyFromKernel(pid, dst, ring->read, rest);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 370, "copyFromKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n",
			       pid, dst, ring->read, rest, ret);
		return ret;
	}
	ring->read += rest;
	ring->used -= rest;
	return 0;
}

/* size bytes from the ring into the vector, from buffer *index at *offset on */
int recvPipeVector(struct ThreadmgrMsgPipeRing *ring, SceUID pid,
			  const SceKernelMsgPipeVector *vector, int *index, SceSize *offset,
			  SceSize size, int kernelVector)
{
	SceKernelMsgPipeVector buffer;
	SceSize off = *offset, len;
	int i = *index, ret;

	for (;;) {
		if (kernelVector) {
			buffer = vector[i];
		} else {
			ret = copyToKernel(pid, &buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 404,
					"copyToKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n", pid,
					&buffer, &vector[i], sizeof(buffer), ret);
				return ret;
			}
		}
		len = buffer.bufSize - off;
		if (size <= len)
			len = size;
		ret = recvPipe(ring, pid, (uint8_t *)buffer.pBase + off, len);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 412, "recvPipe(%p, 0x%08X, 0x%08X, 0x%08x): error 0x%08x\n",
				       ring, pid, (uint8_t *)buffer.pBase + off, len, ret);
			return ret;
		}
		off += len;
		size -= len;
		if (buffer.bufSize <= off) {
			off = 0;
			i++;
		}
		if (size == 0)
			break;
	}
	*index = i;
	*offset = off;
	return ret;
}

/* clears the pipe's data bit when nothing is left to receive (older SDKs: even with data) */
static inline void updateMsgPipeDataPattern(ThreadmgrMsgPipe *pipe)
{
	SceUInt32 version;

	if (pipe->ring.write == pipe->ring.read &&
	    (pipe->ring.used == 0 ||
	     (pipe->creatorPid != SCE_KERNEL_PROCESS_ID &&
	      SceSysrootForDriver_CAE820B2(pipe->creatorPid, &version) >= 0 &&
	      version < SDK_VERSION_MSG_PIPE_EMPTY)))
		pipe->event.pattern &= ~EVENT_PATTERN_RESERVED_BIT;
}

/*
 * SceUIDMsgPipeClass's release method: thread leaves the pipe's waiters;
 * the next waiter is served from what is in the ring.
 */
static int uidMsgPipe_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	ThreadmgrMsgPipe *pipe = object;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitNode *queue = MSG_PIPE_QUEUE(pipe), *node;
	uint8_t *write, *read;
	SceSize used, n, remaining, moved;
	int locked, ret = 0;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5142);
	SCE_DBG_ASSERT_CHECK(pipe->event.lock != 0, 5145);
	if (thread->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE)
		goto done;
	pipe->partial = 0;
	for (node = pipe->event.next; node != queue; node = node->next) {
		if (!(node->owner & 1))
			continue;
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 5167);
		locked = SceCpuForDriver_5AC9D394(THREAD_LOCK(waiter));
		if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE) {
			if (locked == 0)
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			continue;
		}
		if (pipe->waitDirection == MSG_PIPE_WAIT_SEND) {
			SCE_DBG_ASSERT_CHECK(waiter->msgPipeSize == waiter->msgPipeRemaining, 5184);
			ret = 0;
			pipe->partial = waiter->msgPipeSize != waiter->msgPipeRemaining;
		} else if (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE) {
			SCE_DBG_ASSERT_PRINTF(waiter->msgPipeSize == waiter->msgPipeRemaining, 5193,
				" k(%d): uidMsgPipe_ReleaseMethod(), relThreadId=0x%08X(%s), "
				"waitThreadId=0x%08X(%s), orgSize=0x%08X, size=0x%08X, numWait=0x%08X, "
				"numWaitThreads=0x%08X", ThreadmgrCpuId(), thread->threadId,
				SceSysmemForDriver_E655852F(thread->threadId), waiter->threadId,
				SceSysmemForDriver_E655852F(waiter->threadId), waiter->msgPipeSize,
				waiter->msgPipeRemaining, pipe->waitCount, pipe->event.waiting);
			used = pipe->ring.used;
			n = waiter->msgPipeRemaining;
			if (used < n)
				n = used;
			if (n == 0) {
				ret = 0;
			} else {
				write = pipe->ring.write;
				read = pipe->ring.read;
				ret = recvPipeVector(&pipe->ring, waiter->timeoutProcessId,
					waiter->msgPipeVector, &waiter->msgPipeVectorIndex,
					&waiter->msgPipeVectorOffset, n,
					waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret < 0) {
					SCE_DBG_PRINTF(0, 5209,
						"error: uidMsgPipe_ReleaseMethod(), recvPipeVector()=0x%08X\n",
						ret);
					goto unlock;
				}
				remaining = waiter->msgPipeRemaining;
				waiter->msgPipeRemaining = remaining - n;
				if (waiter->msgPipeResult != NULL) {
					moved = waiter->msgPipeSize - (remaining - n);
					ret = setMsgPipeResult(waiter, &moved);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 5223,
							"warning: uidMsgPipe_ReleaseMethod(): sceKernelCopyToUserProc()=0x%08X",
							ret);
				}
				if (waiter->waitOptions & MSG_PIPE_MODE_PEEK) {
					pipe->ring.write = write;
					pipe->ring.read = read;
					pipe->ring.used = used;
				}
			}
			pipe->partial = waiter->msgPipeRemaining != 0 &&
					waiter->msgPipeSize != waiter->msgPipeRemaining;
		} else {
			ret = 0;
		}
unlock:
		if (locked == 0)
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		goto served;
	}
	ret = 0;
served:
	updateMsgPipeDataPattern(pipe);
	if (thread->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE && --pipe->waitCount == 0)
		pipe->waitDirection = 0;
done:
	SCE_DBG_ASSERT_CHECK(pipe->event.waiting != 0, 5274);
	pipe->event.waiting--;
	return ret;
}

/*
 * SceUIDMsgPipeClass's revalidation: waiting senders put what fits into
 * the ring, in queue order, until one does not fit.
 */
static int uidMsgPipe_RevalidateMethod(void *cls, void *object)
{
	ThreadmgrMsgPipe *pipe = object;
	struct ThreadmgrWaitNode *queue = MSG_PIPE_QUEUE(pipe), *node, *next;
	struct ThreadCB *waiter;
	SceSize n, moved, remaining;
	uint32_t wake = 0;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5049);
	SCE_DBG_ASSERT_CHECK(pipe->event.lock != 0, 5052);
	if (pipe->event.waiting == 0 || pipe->waitDirection != MSG_PIPE_WAIT_SEND)
		return 0;
	for (node = pipe->event.next; node != queue; node = next) {
		next = node->next;
		if (!(node->owner & 1))
			continue;
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 5073);
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
			n = pipe->ring.size - pipe->ring.used;
			if (waiter->msgPipeRemaining <= n)
				n = waiter->msgPipeRemaining;
			if ((waiter->waitOptions & MSG_PIPE_MODE_ALL) ?
			    waiter->msgPipeRemaining != n : n == 0)
				goto stop;
			ret = sendPipeVector(&pipe->ring, waiter->timeoutProcessId,
				waiter->msgPipeVector, &waiter->msgPipeVectorIndex,
				&waiter->msgPipeVectorOffset, n, 0,
				waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
			if (ret < 0)
				goto stop;
			remaining = waiter->msgPipeRemaining;
			waiter->msgPipeRemaining = remaining - n;
			if (waiter->msgPipeResult != NULL) {
				moved = waiter->msgPipeSize - (remaining - n);
				ret = setMsgPipeResult(waiter, &moved);
				if (ret < 0)
					SCE_DBG_PRINTF(0, 5105,
						"warning: uidMsgPipe_RevalidateMethod(): sceKernelCopyToUserProc()=0x%08X",
						ret);
			}
			/* Sony's: the last wakeup's result, not all of them */
			wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	return wake;
stop:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	return wake;
}

#define MSG_PIPE_EVENT_SENT		0x00000001	/* the pipe's event bits a transfer sets */
#define MSG_PIPE_EVENT_ERROR		0x00000010

/* what a syscall restarted after a suspend carries over of its transfer */
typedef struct ThreadmgrMsgPipeRestart {
	SceSize size;
	SceSize remaining;
	int index;
	SceSize offset;
} ThreadmgrMsgPipeRestart;

static inline ThreadmgrMsgPipeRestart *MsgPipeRestartState(void)
{
	return SceThreadmgrForDriver_3A72C6D8(s_threadmgrRuntime.manager->syscallRestartSlot);
}

/* the event bits a finished transfer of moved bytes sets (0 for none) */
static inline uint32_t MsgPipeTransferPattern(ThreadmgrMsgPipe *pipe, SceSize size,
					      SceSize remaining)
{
	SceUInt32 version;

	if (size == remaining)
		return 0;
	if (pipe->ring.write == pipe->ring.read &&
	    (pipe->ring.used == 0 ||
	     (pipe->creatorPid != SCE_KERNEL_PROCESS_ID &&
	      SceSysrootForDriver_CAE820B2(pipe->creatorPid, &version) >= 0 &&
	      version < SDK_VERSION_MSG_PIPE_EMPTY)))
		return MSG_PIPE_EVENT_SENT;
	return MSG_PIPE_EVENT_SENT | EVENT_PATTERN_RESERVED_BIT;
}

/*
 * Sends the vector's count buffers (in process pid) into the pipe: straight
 * to waiting receivers first, then into the ring; with
 * MSG_PIPE_MODE_ALL all of it or nothing, else whatever fits; waits
 * otherwise. *result gets the bytes sent, *wakeOut whether a reschedule is
 * due.
 */
int sendMsgPipeVectorCore(SceUID msgPipeId, SceUID pid, const SceKernelMsgPipeVector *vector,
				 int count, uint32_t mode, SceSize *result, SceUInt32 *timeout,
				 int callbacks, uint32_t *wakeOut)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *queue, *node, *pos;
	struct ThreadCB *waiter;
	ThreadmgrMsgPipeRestart *state;
	SceKernelMsgPipeVector buffer;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	SceUInt32 timeoutValue, *wait;
	SceSize total = 0, remaining = 0, offset = 0, n, moved, savedUsed;
	uint8_t *savedRead;
	uint64_t wakeup;
	uint32_t wake, pattern, attr;
	bool queued;
	int index = 0, locked = 0, i, ret, cbRet;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1909);
	ret = sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject);
	if (ret < 0)
		return MsgPipeUnknownIdError();
	sceKernelSpinlockLowLock(&pipe->event.lock);
	locked = 1;
	wait = timeout;
	if (timeout != NULL && pid != SCE_KERNEL_PROCESS_ID) {
		ret = sceKernelCopyFromUserProc(pid, &timeoutValue, timeout, sizeof(timeoutValue));
		if (ret < 0) {
			wake = 0;
			total = 0;
			goto fail;
		}
		pipe = (ThreadmgrMsgPipe *)pObject;
		wait = &timeoutValue;
	}
	if (SceThreadmgrForDriver_20C228E4()) {
		/* a restarted syscall goes on where it was */
		state = MsgPipeRestartState();
		if (state == NULL) {
			SCE_DBG_ASSERT_FAIL(1951);
			total = 0;
			debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
			wake = 0;
			goto fail;
		}
		SCE_DBG_ASSERT_CHECK(state->size != 0, 1957);
		total = state->size;
		remaining = state->remaining;
		index = state->index;
		offset = state->offset;
	} else if (!(mode & MSG_PIPE_MODE_KERNEL_VECTOR) && pid != SCE_KERNEL_PROCESS_ID) {
		for (i = 0; i < count; i++) {
			ret = sceKernelCopyFromUserProc(pid, &buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 1987,
					"sceKernelCopyFromUserProc(0x%08x, %p, 0x%08x, 0x%08x): error 0x%08x\n",
					pid, &buffer, &vector[i], sizeof(buffer), ret);
				wake = 0;
				total = 0;
				goto fail;
			}
			if (buffer.pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				wake = 0;
				total = 0;
				goto fail;
			}
			if (pipe->ring.size < buffer.bufSize ||
			    pipe->ring.size < (remaining += buffer.bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				wake = 0;
				total = 0;
				goto fail;
			}
		}
		total = remaining;
	} else {
		for (i = 0; i < count; i++) {
			if (vector[i].pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				wake = 0;
				total = 0;
				goto fail;
			}
			if (pipe->ring.size < vector[i].bufSize ||
			    pipe->ring.size < (remaining += vector[i].bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				wake = 0;
				total = 0;
				goto fail;
			}
		}
		total = remaining;
	}
	if (pipe->processId == -1) {
		wake = 0;
		ret = SCE_KERNEL_ERROR_MSG_PIPE_DELETED;
		goto fail;
	}
	if (remaining == 0) {
		wake = 0;
		ret = 0;
		if (result != NULL) {
			if (pid == SCE_KERNEL_PROCESS_ID) {
				*result = 0;
			} else {
				ret = sceKernelCopyToUserProc(pid, result, &remaining, sizeof(remaining));
				if (ret > 0)
					ret = 0;
			}
		}
		goto fail;
	}
	if (pipe->ring.size != 0 && pipe->ring.size < remaining) {
		wake = 0;
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto fail;
	}
	if (wait != NULL)
		getWakeupTime(&wakeup, *wait);
	queue = MSG_PIPE_QUEUE(pipe);
	savedRead = NULL;
	savedUsed = 0;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&pipe->event.lock);
			cbRet = callCallbackAll(thread);
			if (cbRet < 0) {
				sceGUIDReleaseObject(msgPipeId);
				return cbRet;
			}
			sceKernelSpinlockLowLock(&pipe->event.lock);
		}
		wake = 0;
		if (pipe->waitDirection == MSG_PIPE_WAIT_SEND) {
			/* senders already wait: queue behind them unless more urgent */
			if (!(pipe->event.attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S)) {
				queued = true;
				goto decided;
			}
			for (pos = pipe->event.next; pos != queue; pos = pos->next) {
				if (!(pos->owner & 1))
					continue;
				waiter = (struct ThreadCB *)pos;
				SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 2071);
				sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
				if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
					if (pipe->partial ||
					    waiter->currentPriority <= thread->currentPriority) {
						sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
						queued = true;
						goto decided;
					}
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					break;
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			}
		}
		/* waiting receivers take it straight from the caller's buffers */
		node = pipe->event.next;
		for (;;) {
			if (pipe->waitCount < 1 || pipe->waitDirection != MSG_PIPE_WAIT_RECEIVE)
				goto ring;
			SCE_DBG_ASSERT_CHECK((uint32_t)pipe->waitCount <= pipe->event.waiting, 2097);
			SCE_DBG_ASSERT_CHECK(node != queue, 2098);
			pos = node;
			node = node->next;
			if (!(pos->owner & 1))
				continue;
			waiter = (struct ThreadCB *)pos;
			SCE_DBG_ASSERT_PRINTF(waiter->magic == THREAD_CB_MAGIC, 2109,
				" k: error: sendMsgPipeVectorCore(), sendProcessId=0x%08X, msgPipeId=0x%08X, threadId=0x%08X",
				pid, msgPipeId, currentThreadContext()->threadId);
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE) {
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				continue;
			}
			n = waiter->msgPipeRemaining;
			if (remaining < n)
				n = remaining;
			if (n != 0) {
				if (waiter->waitOptions & MSG_PIPE_MODE_PEEK) {
					/* a peek: through the ring's empty start, which is then restored */
					savedRead = pipe->ring.read;
					savedUsed = pipe->ring.used;
					pipe->ring.read = pipe->ring.write;
					pipe->ring.used = 0;
				}
				ret = sendPipeVector(&pipe->ring, pid, vector, &index, &offset, n,
					waiter->waitOptions & MSG_PIPE_MODE_PEEK,
					mode & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret >= 0)
					ret = recvPipeVector(&pipe->ring, waiter->timeoutProcessId,
						waiter->msgPipeVector, &waiter->msgPipeVectorIndex,
						&waiter->msgPipeVectorOffset, n,
						waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret < 0) {
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					goto fail;
				}
				if (waiter->waitOptions & MSG_PIPE_MODE_PEEK) {
					SCE_DBG_ASSERT_CHECK(savedRead != NULL, 2159);
					pipe->ring.read = savedRead;
					pipe->ring.used = savedUsed;
				} else {
					remaining -= n;
				}
				waiter->msgPipeRemaining -= n;
			}
			if (waiter->msgPipeResult != NULL) {
				moved = waiter->msgPipeSize - waiter->msgPipeRemaining;
				ret = setMsgPipeResult(waiter, &moved);
				if (ret < 0) {
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					goto fail;
				}
			}
			if ((waiter->waitOptions & MSG_PIPE_MODE_ALL) && waiter->msgPipeRemaining != 0) {
				pipe->partial = 1;
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				goto ring;
			}
			pipe->partial = 0;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			if (remaining == 0) {
				queued = false;
				goto decided;
			}
		}
ring:
		/* the rest into the ring, as far as it fits */
		queued = false;
		if (remaining != 0) {
			n = pipe->ring.size - pipe->ring.used;
			if (remaining <= n)
				n = remaining;
			if ((mode & MSG_PIPE_MODE_ALL) ? remaining == n : n != 0) {
				ret = sendPipeVector(&pipe->ring, pid, vector, &index, &offset, n, 0,
						     mode & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret < 0)
					goto fail;
				remaining -= n;
			}
		}
decided:
		if (result != NULL) {
			moved = total - remaining;
			if (pid == SCE_KERNEL_PROCESS_ID) {
				*result = moved;
			} else {
				ret = sceKernelCopyToUserProc(pid, result, &moved, sizeof(moved));
				if (ret < 0)
					goto fail;
			}
		}
		ret = 0;
		if (!(mode & MSG_PIPE_MODE_ALL) ? total != remaining : remaining == 0)
			goto fail;
		/* wait for room or receivers */
		pipe->waitDirection = MSG_PIPE_WAIT_SEND;
		pipe->waitCount++;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->msgPipeVector = vector;
		thread->msgPipeRemaining = remaining;
		thread->msgPipeSize = total;
		thread->msgPipeResult = result;
		thread->waitOptions = mode;
		thread->msgPipeVectorIndex = index;
		thread->msgPipeVectorOffset = offset;
		thread->timeoutProcessId = pid;
		if (!queued)
			pipe->partial = total != remaining;
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_WAKE_PENDING);
		attr = pipe->event.attr;
		if (attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S)
			attr |= SCE_KERNEL_ATTR_TH_PRIO;
		else
			attr &= ~SCE_KERNEL_ATTR_TH_PRIO;
		pipe->event.attr = attr;
		ret = waitWaitableObject(msgPipeId, (uintptr_t)queue, SCE_KERNEL_WAITTYPE_MSG_PIPE, &wakeup,
					 wait, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		index = thread->msgPipeVectorIndex;
		offset = thread->msgPipeVectorOffset;
		remaining = thread->msgPipeRemaining;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!callbacks)
			goto fail;
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	goto report;
fail:
	if (!locked)
		goto check;
report:
	/* the pipe's event tells what happened */
	pattern = MsgPipeTransferPattern(pipe, total, remaining);
	if (ret < 0 && ret != SCE_KERNEL_ERROR_THREAD_STOPPED &&
	    ret != SCE_KERNEL_ERROR_THREAD_SUSPENDED)
		pattern |= MSG_PIPE_EVENT_ERROR;
	if (pattern != 0) {
		pattern = setEventWithNotifyCallback(msgPipeId, pattern,
			(uint64_t)((total - remaining) | (uint32_t)(ret >> 31)) << 32 | (uint32_t)ret,
			pattern, &pipe->event);
		if ((int)pattern < 0)
			SCE_PANIC(SCE_LINE(2340, 2332));
		wake |= pattern;
	}
	sceKernelSpinlockLowUnlock(&pipe->event.lock);
	sceGUIDReleaseObject(msgPipeId);
check:
	if (callbacks) {
callbacks:
		if (thread->flags & THREAD_FLAG_CALLBACKS) {
			cbRet = callCallbackAll(thread);
			if (cbRet < 0)
				return cbRet;
		}
	}
	if (wakeOut != NULL)
		*wakeOut = wake;
	if (thread->flags & THREAD_FLAG_RESUMED_WAIT) {
		/* the syscall will be restarted: keep where it got to */
		SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 2358);
		state = MsgPipeRestartState();
		if (state == NULL) {
			SCE_DBG_ASSERT_FAIL(2363);
			debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		} else {
			state->size = total;
			state->remaining = remaining;
			state->index = index;
			state->offset = offset;
		}
	}
	return ret;
}

#define MSG_PIPE_EVENT_RECEIVED		0x00000002
#define MSG_PIPE_MODE_RECEIVING		0x40000000	/* set in a waiting receiver's options */

/*
 * Receives into the vector's count buffers (in process pid): from the ring
 * first, then straight from waiting senders (who also refill the ring);
 * with MSG_PIPE_MODE_ALL all of it or nothing, MSG_PIPE_MODE_PEEK leaves the
 * data in place; waits otherwise.
 */
int recvMsgPipeVectorCore(SceUID msgPipeId, SceUID pid, const SceKernelMsgPipeVector *vector,
				 int count, uint32_t mode, SceSize *result, SceUInt32 *timeout,
				 int callbacks, uint32_t *wakeOut)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *queue, *node, *pos;
	struct ThreadCB *waiter;
	ThreadmgrMsgPipeRestart *state;
	SceKernelMsgPipeVector buffer;
	ThreadmgrMsgPipe *pipe;
	SceUInt32 timeoutValue, *wait;
	SceSize total = 0, remaining = 0, offset = 0, n, m, moved, senderRemaining;
	SceSize savedUsed;
	uint8_t *savedWrite, *savedRead;
	uint64_t wakeup;
	uint32_t wake, pattern, attr, peek;
	bool progressed;
	int index = 0, locked = 0, i, ret, cbRet;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3199);
	ret = sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), (SceObjectBase **)&pipe);
	if (ret < 0)
		return MsgPipeUnknownIdError();
	sceKernelSpinlockLowLock(&pipe->event.lock);
	locked = 1;
	wait = timeout;
	if (timeout != NULL && pid != SCE_KERNEL_PROCESS_ID) {
		ret = sceKernelCopyFromUserProc(pid, &timeoutValue, timeout, sizeof(timeoutValue));
		if (ret < 0) {
			wake = 0;
			total = 0;
			goto fail;
		}
		wait = &timeoutValue;
	}
	if (SceThreadmgrForDriver_20C228E4()) {
		/* a restarted syscall goes on where it was */
		state = MsgPipeRestartState();
		if (state == NULL) {
			SCE_DBG_ASSERT_FAIL(3241);
			total = 0;
			debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
			wake = 0;
			goto fail;
		}
		SCE_DBG_ASSERT_CHECK(state->size != 0, 3247);
		total = state->size;
		remaining = state->remaining;
		index = state->index;
		offset = state->offset;
	} else if (!(mode & MSG_PIPE_MODE_KERNEL_VECTOR) && pid != SCE_KERNEL_PROCESS_ID) {
		for (i = 0; i < count; i++) {
			ret = sceKernelCopyFromUserProc(pid, &buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 3277,
					"sceKernelCopyFromUserProc(0x%08x, %p, 0x%08x, 0x%08x): 0x%08x\n",
					pid, &buffer, &vector[i], sizeof(buffer), ret);
				wake = 0;
				total = 0;
				goto fail;
			}
			if (buffer.pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				wake = 0;
				total = 0;
				goto fail;
			}
			if (pipe->ring.size < buffer.bufSize ||
			    pipe->ring.size < (remaining += buffer.bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				wake = 0;
				total = 0;
				goto fail;
			}
		}
		total = remaining;
	} else {
		for (i = 0; i < count; i++) {
			if (vector[i].pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				wake = 0;
				total = 0;
				goto fail;
			}
			if (pipe->ring.size < vector[i].bufSize ||
			    pipe->ring.size < (remaining += vector[i].bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				wake = 0;
				total = 0;
				goto fail;
			}
		}
		total = remaining;
	}
	if (pipe->processId == -1) {
		wake = 0;
		ret = SCE_KERNEL_ERROR_MSG_PIPE_DELETED;
		goto fail;
	}
	if (remaining == 0) {
		wake = 0;
		ret = 0;
		if (result != NULL) {
			if (pid == SCE_KERNEL_PROCESS_ID) {
				*result = 0;
			} else {
				ret = sceKernelCopyToUserProc(pid, result, &remaining, sizeof(remaining));
				if (ret > 0)
					ret = 0;
			}
		}
		goto fail;
	}
	if (pipe->ring.size != 0 && pipe->ring.size < remaining) {
		wake = 0;
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto fail;
	}
	if (wait != NULL)
		getWakeupTime(&wakeup, *wait);
	queue = MSG_PIPE_QUEUE(pipe);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&pipe->event.lock);
			cbRet = callCallbackAll(thread);
			if (cbRet < 0) {
				sceGUIDReleaseObject(msgPipeId);
				return cbRet;
			}
			sceKernelSpinlockLowLock(&pipe->event.lock);
		}
		wake = 0;
		if (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE) {
			/* receivers already wait: queue behind them unless more urgent */
			if (!(pipe->event.attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R)) {
				progressed = true;
				goto decided;
			}
			for (pos = pipe->event.next; pos != queue; pos = pos->next) {
				if (!(pos->owner & 1))
					continue;
				waiter = (struct ThreadCB *)pos;
				SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 3361);
				sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
				if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
					if (pipe->partial ||
					    waiter->currentPriority <= thread->currentPriority) {
						sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
						progressed = true;
						goto decided;
					}
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					break;
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			}
		}
		/* what the ring has first */
		savedUsed = pipe->ring.used;
		savedWrite = pipe->ring.write;
		savedRead = pipe->ring.read;
		if (savedUsed != 0) {
			n = remaining;
			if (savedUsed < n)
				n = savedUsed;
			ret = recvPipeVector(&pipe->ring, pid, vector, &index, &offset, n,
					     mode & MSG_PIPE_MODE_KERNEL_VECTOR);
			if (ret < 0) {
				wake = 0;
				goto fail;
			}
			remaining -= n;
		}
		/* then from the waiting senders, who refill the ring with what fits */
		peek = mode & MSG_PIPE_MODE_PEEK;
		node = pipe->event.next;
		while (pipe->waitDirection == MSG_PIPE_WAIT_SEND && pipe->waitCount > 0) {
			SCE_DBG_ASSERT_CHECK((uint32_t)pipe->waitCount <= pipe->event.waiting, 3401);
			SCE_DBG_ASSERT_CHECK(node != queue, 3402);
			pos = node;
			node = node->next;
			if (!(pos->owner & 1))
				continue;
			waiter = (struct ThreadCB *)pos;
			SCE_DBG_ASSERT_PRINTF(waiter->magic == THREAD_CB_MAGIC, 3415,
				" k: error: recvMsgPipeVectorCore(), recvProcessId=0x%08X, msgPipeId=0x%08X, threadId=0x%08X",
				pid, msgPipeId, currentThreadContext()->threadId);
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE) {
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				continue;
			}
			if (remaining != 0) {
				n = waiter->msgPipeRemaining;
				if (remaining < n)
					n = remaining;
				ret = sendPipeVector(&pipe->ring, waiter->timeoutProcessId,
					waiter->msgPipeVector, &waiter->msgPipeVectorIndex,
					&waiter->msgPipeVectorOffset, n, peek,
					waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret >= 0)
					ret = recvPipeVector(&pipe->ring, pid, vector, &index, &offset, n,
							     mode & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret < 0)
					goto unlockFail;
				remaining -= n;
				if (peek) {
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					goto restore;
				}
				progressed = true;
				senderRemaining = waiter->msgPipeRemaining - n;
				waiter->msgPipeRemaining = senderRemaining;
			} else {
				if (peek) {
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					goto restore;
				}
				senderRemaining = waiter->msgPipeRemaining;
				progressed = false;
			}
			m = pipe->ring.size - pipe->ring.used;
			if (senderRemaining <= m)
				m = senderRemaining;
			if ((waiter->waitOptions & MSG_PIPE_MODE_ALL) ? m == senderRemaining : m != 0) {
				ret = sendPipeVector(&pipe->ring, waiter->timeoutProcessId,
					waiter->msgPipeVector, &waiter->msgPipeVectorIndex,
					&waiter->msgPipeVectorOffset, m, 0,
					waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
				if (ret < 0)
					goto unlockFail;
				progressed = true;
				senderRemaining = waiter->msgPipeRemaining - m;
				waiter->msgPipeRemaining = senderRemaining;
			}
			if (waiter->msgPipeResult != NULL) {
				moved = waiter->msgPipeSize - senderRemaining;
				ret = setMsgPipeResult(waiter, &moved);
				if (ret < 0)
					goto unlockFail;
			}
			if (((waiter->waitOptions & MSG_PIPE_MODE_ALL) || !progressed) &&
			    waiter->msgPipeRemaining != 0) {
				/* the sender stays, part-served */
				progressed = false;
				pipe->partial = waiter->msgPipeRemaining != waiter->msgPipeSize;
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				goto decided;
			}
			pipe->partial = 0;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		progressed = false;
		if (peek) {
restore:
			/* a peek leaves the ring as it was */
			progressed = false;
			pipe->ring.write = savedWrite;
			pipe->ring.read = savedRead;
			pipe->ring.used = savedUsed;
		}
decided:
		if (result != NULL) {
			moved = total - remaining;
			if (pid == SCE_KERNEL_PROCESS_ID) {
				*result = moved;
			} else {
				ret = sceKernelCopyToUserProc(pid, result, &moved, sizeof(moved));
				if (ret < 0)
					goto fail;
			}
		}
		if (!(mode & MSG_PIPE_MODE_ALL) ? total != remaining : remaining == 0) {
			ret = 0;
			goto fail;
		}
		/* wait for data or senders */
		updateMsgPipeDataPattern(pipe);
		pipe->waitDirection = MSG_PIPE_WAIT_RECEIVE;
		pipe->waitCount++;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->msgPipeRemaining = remaining;
		thread->msgPipeVector = vector;
		thread->waitOptions = mode | MSG_PIPE_MODE_RECEIVING;
		thread->msgPipeSize = total;
		thread->msgPipeResult = result;
		thread->msgPipeVectorIndex = index;
		thread->msgPipeVectorOffset = offset;
		thread->timeoutProcessId = pid;
		if (!progressed)
			pipe->partial = total != remaining;
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_WAKE_PENDING);
		attr = pipe->event.attr;
		if (attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R)
			attr |= SCE_KERNEL_ATTR_TH_PRIO;
		else
			attr &= ~SCE_KERNEL_ATTR_TH_PRIO;
		pipe->event.attr = attr;
		ret = waitWaitableObject(msgPipeId, (uintptr_t)queue, SCE_KERNEL_WAITTYPE_MSG_PIPE, &wakeup,
					 wait, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		index = thread->msgPipeVectorIndex;
		offset = thread->msgPipeVectorOffset;
		remaining = thread->msgPipeRemaining;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!callbacks)
			goto fail;
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	goto report;
unlockFail:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
fail:
	if (!locked)
		goto check;
report:
	updateMsgPipeDataPattern(pipe);
	pattern = total == remaining ? 0 : MSG_PIPE_EVENT_RECEIVED;
	if (ret < 0 && ret != SCE_KERNEL_ERROR_THREAD_STOPPED &&
	    ret != SCE_KERNEL_ERROR_THREAD_SUSPENDED)
		pattern |= MSG_PIPE_EVENT_ERROR;
	if (pattern != 0) {
		pattern = setEventWithNotifyCallback(msgPipeId, pattern,
			(uint64_t)((total - remaining) | (uint32_t)(ret >> 31)) << 32 | (uint32_t)ret,
			pattern, &pipe->event);
		if ((int)pattern < 0)
			SCE_PANIC(SCE_LINE(3655, 3647));
		wake |= pattern;
	}
	sceKernelSpinlockLowUnlock(&pipe->event.lock);
	sceGUIDReleaseObject(msgPipeId);
check:
	if (callbacks) {
callbacks:
		if (thread->flags & THREAD_FLAG_CALLBACKS) {
			cbRet = callCallbackAll(thread);
			if (cbRet < 0)
				ret = cbRet;
		}
	}
	if (wakeOut != NULL)
		*wakeOut = wake;
	if (thread->flags & THREAD_FLAG_RESUMED_WAIT) {
		/* the syscall will be restarted: keep where it got to */
		SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 3672);
		state = MsgPipeRestartState();
		if (state == NULL) {
			SCE_DBG_ASSERT_FAIL(3677);
			debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		} else {
			state->size = total;
			state->remaining = remaining;
			state->index = index;
			state->offset = offset;
		}
	}
	return ret;
}

#define MSG_PIPE_MODE_ASYNC		0x00000010	/* at the API: queue it to the async threads */
#define MSG_PIPE_SEND_MODES		(MSG_PIPE_MODE_ALL | MSG_PIPE_MODE_ASYNC)
#define MSG_PIPE_RECEIVE_MODES		(MSG_PIPE_MODE_ALL | MSG_PIPE_MODE_ASYNC | MSG_PIPE_MODE_PEEK)
#define MSG_PIPE_ASYNC_THREADS		8
#define MSG_PIPE_ASYNC_MAX		0x100	/* queued requests per process */
#define MSG_PIPE_ASYNC_SEND		0
#define MSG_PIPE_ASYNC_RECEIVE		1
#define MSG_PIPE_ASYNC_PRIORITY		0x40
#define SDK_VERSION_ASYNC_NO_RESULT	0x01ffffff	/* newer SDKs get no async result */

/* a send or receive queued to the async threads (0x30, in the process's heap) */
typedef struct ThreadmgrMsgPipeAsyncRequest {
	struct ThreadmgrMsgPipeAsyncRequest *next;
	struct ThreadmgrMsgPipeAsyncRequest *prev;
	int type;				/* 0x08: MSG_PIPE_ASYNC_* */
	int priority;				/* 0x0c: the caller's */
	uint32_t cpuAffinityMask;		/* 0x10: the caller's */
	SceUID msgPipeId;			/* 0x14 */
	SceUID pid;				/* 0x18 */
	SceKernelMsgPipeVector *vector;		/* 0x1c: a copy in the process's heap */
	int count;				/* 0x20 */
	uint32_t mode;				/* 0x24 */
	SceSize *result;			/* 0x28 */
	SceUInt32 *timeout;			/* 0x2c */
} ThreadmgrMsgPipeAsyncRequest;

/* a send or receive call's arguments (the syscall wrappers fill it) */
typedef struct ThreadmgrMsgPipeCall {
	SceUID msgPipeId;
	SceUID pid;
	const SceKernelMsgPipeVector *vector;	/* 0x08 */
	int count;				/* 0x0c */
	uint32_t mode;				/* 0x10 */
	SceSize *result;			/* 0x14 */
	SceUInt32 *timeout;			/* 0x18 */
} ThreadmgrMsgPipeCall;

/*
 * Each of the 8 "SceKernelMsgPipeAsync%d" threads takes queued requests
 * and runs them at the caller's priority and CPUs.
 */
static int msgPipeAsyncThread(SceSize args, void *argp)
{
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrMsgPipeAsyncRequest *request;
	SceProcessObject *process;
	int n = *(int *)argp, intr, ret;

	sceKernelLockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1, NULL);
	for (;;) {
		do {
			ret = sceKernelWaitCond(s_threadmgrRuntime.manager->msgPipeAsyncCond, NULL);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 454,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelWaitCond()=0x%08X\n",
					ret);
				SCE_PANIC(455);
			}
			manager = s_threadmgrRuntime.manager;
			request = (ThreadmgrMsgPipeAsyncRequest *)manager->msgPipeAsyncNext;
		} while (request == (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext);
		do {
			request->next->prev = request->prev;
			request->prev->next = request->next;
			if (request->type == MSG_PIPE_ASYNC_SEND ||
			    request->type == MSG_PIPE_ASYNC_RECEIVE)
				manager->msgPipeAsyncPid[n] = request->pid;
			ret = sceKernelUnlockMutex(manager->msgPipeAsyncMutex, 1);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 484,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelUnlockMutex()=0x%08X\n",
					ret);
				SCE_PANIC(485);
			}
			ret = sceKernelChangeThreadPriority2(0, request->priority);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 491,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelChangeThreadPriority2(0x%08X)=0x%08X\n",
					request->priority, ret);
				SCE_PANIC(492);
			}
			ret = sceKernelChangeThreadCpuAffinityMask(0, request->cpuAffinityMask);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 496,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelChangeThreadCpuAffinityMask(0x%08X)=0x%08X\n",
					request->cpuAffinityMask, ret);
				SCE_PANIC(497);
			}
			intr = SceCpuForDriver_821FC0EE();
			if (request->type == MSG_PIPE_ASYNC_SEND) {
				sendMsgPipeVectorCore(request->msgPipeId, request->pid, request->vector,
					request->count, request->mode, request->result, request->timeout,
					0, NULL);
				ret = SceSysrootForKernel_7968F091(request->pid, request->vector);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 520,
						"*** fatal error: msgPipeAsyncThread() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X\n",
						request->pid, request->vector, ret);
					SCE_PANIC(521);
				}
				ret = sceGUIDGetObjectWithClass(request->pid,
					s_threadmgrRuntime.manager->processClass,
					(SceObjectBase **)&process);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 527,
						"*** fatal error: msgPipeAsyncThread() -> sceGUIDGetObjectWithClass(0x%08X, %p, %p)=0x%08X\n",
						request->pid, s_threadmgrRuntime.manager->processClass,
						&process, ret);
					SCE_PANIC(528);
				}
				ret = SceSysrootForKernel_7968F091(request->pid, request);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 533,
						"*** fatal error: msgPipeAsyncThread() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X\n",
						request->pid, request, ret);
					SCE_PANIC(534);
				}
				SceCpuForDriver_A4884C4E(&process->msgPipeAsyncCount, 1);
			} else {
				if (request->type != MSG_PIPE_ASYNC_RECEIVE) {
					SCE_DBG_PRINTF(0xb, 571,
						"*** fatal error: msgPipeAsyncThread(), unexpected type(%d)\n",
						request->type);
					SCE_PANIC(572);
				}
				recvMsgPipeVectorCore(request->msgPipeId, request->pid, request->vector,
					request->count, request->mode, request->result, request->timeout,
					0, NULL);
				ret = SceSysrootForKernel_7968F091(request->pid, request->vector);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 552,
						"*** fatal error: msgPipeAsyncThread() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X\n",
						request->pid, request->vector, ret);
					SCE_PANIC(553);
				}
				ret = sceGUIDGetObjectWithClass(request->pid,
					s_threadmgrRuntime.manager->processClass,
					(SceObjectBase **)&process);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 559,
						"*** fatal error: msgPipeAsyncThread() -> sceGUIDGetObjectWithClass(0x%08X, %p, %p)=0x%08X\n",
						request->pid, s_threadmgrRuntime.manager->processClass,
						&process, ret);
					SCE_PANIC(560);
				}
				ret = SceSysrootForKernel_7968F091(request->pid, request);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 565,
						"*** fatal error: msgPipeAsyncThread() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X\n",
						request->pid, request, ret);
					SCE_PANIC(566);
				}
				SceCpuForDriver_A4884C4E(&process->msgPipeAsyncCount, 1);
			}
			sceKernelCpuResumeIntr(intr);
			ret = sceKernelChangeThreadPriority2(0, MSG_PIPE_ASYNC_PRIORITY);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 584,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelChangeThreadPriority2(0x%08X)=0x%08X\n",
					MSG_PIPE_ASYNC_PRIORITY, ret);
				SCE_PANIC(585);
			}
			ret = sceKernelChangeThreadCpuAffinityMask(0, 0xf);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 590,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelChangeThreadCpuAffinityMask(0x%08X)=0x%08X\n",
					0xf, ret);
				SCE_PANIC(591);
			}
			ret = sceKernelLockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1, NULL);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 597,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelLockMutex()=0x%08X\n",
					ret);
				SCE_PANIC(598);
			}
			ret = sceKernelSignalCond(s_threadmgrRuntime.manager->msgPipeAsyncDoneCond[n]);
			manager = s_threadmgrRuntime.manager;
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 604,
					"*** fatal error: msgPipeAsyncThread() -> sceKernelSignalCond()=0x%08X\n",
					ret);
				SCE_PANIC(605);
			}
			request = (ThreadmgrMsgPipeAsyncRequest *)manager->msgPipeAsyncNext;
			manager->msgPipeAsyncPid[n] = -1;
		} while (request != (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext);
	}
}

/* queues a recv to the async threads, with a copy of the vector in the process's heap */
static inline __attribute__((always_inline)) int recvMsgPipeVectorAsync(ThreadmgrMsgPipeCall *call)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrMsgPipeAsyncRequest *request;
	SceKernelMsgPipeVector *vector;
	SceProcessObject *process;
	SceUInt32 version;
	uint32_t mask;
	int priority, ret;

	if (thread == NULL) {
		priority = 0x10000100;
		mask = 0xf;
	} else {
		mask = thread->currentCpuAffinityMask;
		priority = thread->currentPriority;
		if (mask == 0)
			mask = thread->process->cpuAffinity;
	}
	/* Sony's: the process is read through the thread even without one */
	process = thread->process;
	if (SceCpuForDriver_1F157DC3((SceInt32 *)&process->msgPipeAsyncCount, 1, MSG_PIPE_ASYNC_MAX) != 0)
		return SCE_KERNEL_ERROR_MSG_PIPE_ASYNC_REQUEST_OVF;
	vector = sceKernelSysrootAllocRemoteProcessHeap(call->pid, call->count * sizeof(*vector), NULL);
	if (vector == NULL) {
		SCE_DBG_PRINTF(0xf, 3729,
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X, 0x%08X, 0x%08X)=NULL\n",
			call->pid, call->count * sizeof(*vector), 0);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto unaccount;
	}
	ret = copyToKernel(call->pid, vector, call->vector, call->count * sizeof(*vector));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 3739, "copyToKernel(0x%08X, %p, %p, 0x%08x): error 0x%08x\n",
			       call->pid, vector, call->vector, call->count * sizeof(*vector), ret);
		goto freeVector;
	}
	call->vector = vector;
	request = sceKernelSysrootAllocRemoteProcessHeap(call->pid, sizeof(*request), NULL);
	if (request == NULL) {
		SCE_DBG_PRINTF(0xf, 3749,
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X, 0x%08X, 0x%08X)=NULL\n",
			call->pid, sizeof(*request), 0);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto freeVector;
	}
	request->priority = priority;
	request->cpuAffinityMask = mask;
	request->type = MSG_PIPE_ASYNC_RECEIVE;
	request->pid = call->pid;
	request->msgPipeId = call->msgPipeId;
	request->vector = vector;
	request->count = call->count;
	request->mode = call->mode;
	request->result = call->result;
	request->timeout = call->timeout;
	if (request->pid == SCE_KERNEL_PROCESS_ID) {
		version = SDK_VERSION_KERNEL;
		request->result = NULL;
	} else if (SceSysrootForDriver_CAE820B2(request->pid, &version) < 0) {
		version = 0;
	} else if (version > SDK_VERSION_ASYNC_NO_RESULT) {
		request->result = NULL;
	}
	ret = sceKernelLockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1, NULL);
	if (ret >= 0) {
		manager = s_threadmgrRuntime.manager;
		request->next = (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext;
		request->prev = (ThreadmgrMsgPipeAsyncRequest *)manager->msgPipeAsyncPrev;
		manager->msgPipeAsyncPrev = (uintptr_t)request;
		request->prev->next = request;
		ret = sceKernelSignalCond(manager->msgPipeAsyncCond);
		if (ret < 0) {
			sceKernelUnlockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1);
		} else {
			ret = sceKernelUnlockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1);
			if (ret >= 0)
				return ret;
		}
	}
	SceSysrootForKernel_7968F091(call->pid, request);
freeVector:
	SceSysrootForKernel_7968F091(call->pid, vector);
unaccount:
	SceCpuForDriver_A4884C4E(&thread->process->msgPipeAsyncCount, 1);
	return ret;
}

/* receives the call's vector (callbacks: run them while waiting) */
int recvMsgPipeVector(ThreadmgrMsgPipeCall *call, int callbacks)
{
	struct ThreadCB *thread;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (call->vector == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (call->count == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	if (call->mode & ~MSG_PIPE_RECEIVE_MODES) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	if (callbacks && currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_FAIL(3843);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (call->mode & MSG_PIPE_MODE_ASYNC) {
		ret = recvMsgPipeVectorAsync(call);
		if (callbacks)
			sceKernelCheckCallback();
		goto restore;
	}
	wake = 0;
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_FAIL(3863);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_FAIL(3869);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	} else if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	} else {
		thread = currentThreadContext();
		if ((thread->currentAttr & THREAD_ATTR_USER) &&
		    !sceKernelCheckThreadUserStackSelf())
			debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
		ret = recvMsgPipeVectorCore(call->msgPipeId, call->pid, call->vector, call->count,
			call->mode, call->result, call->timeout, callbacks, &wake);
		if (wake & 1)
			switchThread(0);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* queues a send to the async threads, with a copy of the vector in the process's heap */
static inline __attribute__((always_inline)) int sendMsgPipeVectorAsync(ThreadmgrMsgPipeCall *call)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrMsgPipeAsyncRequest *request;
	SceKernelMsgPipeVector *vector;
	SceProcessObject *process;
	SceUInt32 version;
	uint32_t mask;
	int priority, ret;

	if (thread == NULL) {
		priority = 0x10000100;
		mask = 0xf;
	} else {
		mask = thread->currentCpuAffinityMask;
		priority = thread->currentPriority;
		if (mask == 0)
			mask = thread->process->cpuAffinity;
	}
	/* Sony's: the process is read through the thread even without one */
	process = thread->process;
	if (SceCpuForDriver_1F157DC3((SceInt32 *)&process->msgPipeAsyncCount, 1, MSG_PIPE_ASYNC_MAX) != 0)
		return SCE_KERNEL_ERROR_MSG_PIPE_ASYNC_REQUEST_OVF;
	vector = sceKernelSysrootAllocRemoteProcessHeap(call->pid, call->count * sizeof(*vector), NULL);
	if (vector == NULL) {
		SCE_DBG_PRINTF(0xf, 2415,
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X, 0x%08X, 0x%08X)=NULL\n",
			call->pid, call->count * sizeof(*vector), 0);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto unaccount;
	}
	ret = copyToKernel(call->pid, vector, call->vector, call->count * sizeof(*vector));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 2425, "copyToKernel(0x%08x, %p, %p, 0x%08x): error 0x%08x\n",
			       call->pid, vector, call->vector, call->count * sizeof(*vector), ret);
		goto freeVector;
	}
	call->vector = vector;
	request = sceKernelSysrootAllocRemoteProcessHeap(call->pid, sizeof(*request), NULL);
	if (request == NULL) {
		SCE_DBG_PRINTF(0xf, 2435,
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X, 0x%08X, 0x%08X)=NULL\n",
			call->pid, sizeof(*request), 0);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto freeVector;
	}
	request->priority = priority;
	request->cpuAffinityMask = mask;
	request->type = MSG_PIPE_ASYNC_SEND;
	request->pid = call->pid;
	request->msgPipeId = call->msgPipeId;
	request->vector = vector;
	request->count = call->count;
	request->mode = call->mode;
	request->result = call->result;
	request->timeout = call->timeout;
	if (request->pid == SCE_KERNEL_PROCESS_ID) {
		version = SDK_VERSION_KERNEL;
		request->result = NULL;
	} else if (SceSysrootForDriver_CAE820B2(request->pid, &version) < 0) {
		version = 0;
	} else if (version > SDK_VERSION_ASYNC_NO_RESULT) {
		request->result = NULL;
	}
	ret = sceKernelLockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1, NULL);
	if (ret >= 0) {
		manager = s_threadmgrRuntime.manager;
		request->next = (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext;
		request->prev = (ThreadmgrMsgPipeAsyncRequest *)manager->msgPipeAsyncPrev;
		manager->msgPipeAsyncPrev = (uintptr_t)request;
		request->prev->next = request;
		ret = sceKernelSignalCond(manager->msgPipeAsyncCond);
		if (ret < 0) {
			sceKernelUnlockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1);
		} else {
			ret = sceKernelUnlockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1);
			if (ret >= 0)
				return ret;
		}
	}
	SceSysrootForKernel_7968F091(call->pid, request);
freeVector:
	SceSysrootForKernel_7968F091(call->pid, vector);
unaccount:
	SceCpuForDriver_A4884C4E(&thread->process->msgPipeAsyncCount, 1);
	return ret;
}

/* sends the call's vector (callbacks: run them while waiting) */
int sendMsgPipeVector(ThreadmgrMsgPipeCall *call, int callbacks)
{
	struct ThreadCB *thread;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (call->vector == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (call->count == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	if (call->mode & ~MSG_PIPE_SEND_MODES) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	if (callbacks && currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_FAIL(2528);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (call->mode & MSG_PIPE_MODE_ASYNC) {
		ret = sendMsgPipeVectorAsync(call);
		if (callbacks)
			sceKernelCheckCallback();
		goto restore;
	}
	wake = 0;
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_FAIL(2548);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_FAIL(2554);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	} else if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	} else {
		thread = currentThreadContext();
		if ((thread->currentAttr & THREAD_ATTR_USER) &&
		    !sceKernelCheckThreadUserStackSelf())
			debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
		ret = sendMsgPipeVectorCore(call->msgPipeId, call->pid, call->vector, call->count,
			call->mode, call->result, call->timeout, callbacks, &wake);
		if (wake & 1)
			switchThread(0);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

static int uidMsgPipeClassInit(struct ThreadmgrWaitableClass *cls)
{
	int ret;

	ret = uidEventSubclassInit(cls, "SceUIDMsgPipeClass", s_threadmgrRuntime.manager->eventClass,
		0x80, uidMsgPipe_Constructor, uidMsgPipe_Destructor,
		uidDefaultCancelWait, uidMsgPipe_RevalidateMethod,
		uidMsgPipe_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC(SCE_LINE(4858, 4850));
	return ret;
}

int startMsgPipe(void)
{
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(636);
	ret = uidMsgPipeClassInit(cls);
	s_threadmgrRuntime.manager->msgPipeClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 664, " sizeof(SceUIDMsgPipeObject)=%d\n", sizeof(ThreadmgrMsgPipe));
	return ret;
}

/* once processes exist: the async machinery and its 8 threads */
int startMsgPipeAfterProcess(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	SceKernelMutexOptParam opt;
	char name[32];
	int n, ret;

	manager->msgPipeAsyncNext = (uintptr_t)&manager->msgPipeAsyncNext;
	manager->msgPipeAsyncPrev = (uintptr_t)&manager->msgPipeAsyncNext;
	opt.size = sizeof(opt);
	opt.ceilingPriority = MSG_PIPE_ASYNC_PRIORITY;
	manager->msgPipeAsyncMutex = sceKernelCreateMutex("SceKernelMsgPipeAsync", 4, 0, &opt);
	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->msgPipeAsyncMutex >= 0, 697);
	s_threadmgrRuntime.manager->msgPipeAsyncCond = sceKernelCreateCond("SceKernelMsgPipeAsync",
		0, s_threadmgrRuntime.manager->msgPipeAsyncMutex, NULL);
	if (s_threadmgrRuntime.manager->msgPipeAsyncCond < 0)
		SCE_PANIC(705);
	for (n = 0; n < MSG_PIPE_ASYNC_THREADS; n++) {
		s_threadmgrRuntime.manager->msgPipeAsyncPid[n] = -1;
		snprintf(name, sizeof(name) - 1, "SceKernelMsgPipeAsync%d", n);
		s_threadmgrRuntime.manager->msgPipeAsyncThreadId[n] = sceKernelCreateThread(name,
			msgPipeAsyncThread, MSG_PIPE_ASYNC_PRIORITY, 0x1000, 0, 0xf, NULL);
		ret = s_threadmgrRuntime.manager->msgPipeAsyncThreadId[n];
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 721, " k: gpThreadMgrCB->msgPipeAsyncThreadId[%d]=0x%08X\n",
				       n, ret);
			SCE_PANIC(722);
		}
		if (sceKernelStartThread(ret, sizeof(n), &n) < 0)
			SCE_PANIC(726);
		s_threadmgrRuntime.manager->msgPipeAsyncDoneCond[n] = sceKernelCreateCond(
			"SceKernelMsgPipeAsyncReqComp", 0,
			s_threadmgrRuntime.manager->msgPipeAsyncMutex, NULL);
		if (s_threadmgrRuntime.manager->msgPipeAsyncDoneCond[n] < 0)
			SCE_PANIC(734);
	}
	return ret;
}

#define MSG_PIPE_EVENT_PROCESS_EXITED	0x00000200
#define MSG_PIPE_FROM_PROCESS_LINK(link) \
	((ThreadmgrMsgPipe *)((uintptr_t)(link) - offsetof(ThreadmgrMsgPipe, processLink)))

/*
 * A process exits: its queued async requests go, the async threads
 * serving it are cancelled and waited for, the pipes whose buffers it
 * holds lose them (waiters woken with SCE_KERNEL_ERROR_MSG_PIPE_DELETED),
 * and its msg pipe PUIDs are closed.
 */
int msgPipeProcessExit(SceProcessObject *process)
{
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrMsgPipeAsyncRequest *request, *next, *head;
	ListNode *link;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *queue, *node, *after;
	ThreadmgrMsgPipe *pipe;
	SceUInt32 timeout;
	SceSize got = 0;
	SceUID *vector, uid, memBlockId, waitId, guid;
	int n, i, num, intr, ret;

	ret = sceKernelLockMutex(s_threadmgrRuntime.manager->msgPipeAsyncMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 766,
			"*** fatal error: msgPipeProcessExit() -> sceKernelLockMutex()=0x%08X\n", ret);
		SCE_PANIC(767);
	}
	manager = s_threadmgrRuntime.manager;
	head = (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext;
	for (request = head->next; request != head; request = next) {
		next = request->next;
		if (request->type != MSG_PIPE_ASYNC_SEND && request->type != MSG_PIPE_ASYNC_RECEIVE) {
			SCE_DBG_PRINTF(0, 807,
				"*** fatal error: msgPipeProcessExit(), unexpected type(%d)\n",
				request->type);
			SCE_PANIC(808);
		}
		if (request->pid == process->pid) {
			next->prev = request->prev;
			request->prev->next = next;
			SceSysrootForKernel_7968F091(request->pid, request->vector);
			SceSysrootForKernel_7968F091(request->pid, request);
			manager = s_threadmgrRuntime.manager;
			head = (ThreadmgrMsgPipeAsyncRequest *)&manager->msgPipeAsyncNext;
		}
	}
	for (n = 0; n < MSG_PIPE_ASYNC_THREADS; n++) {
		while (manager->msgPipeAsyncPid[n] == process->pid) {
			/* the thread is on one of its requests: cancel its wait */
			intr = SceCpuForDriver_821FC0EE();
			ret = sceGUIDReferObjectWithClass(s_threadmgrRuntime.manager->msgPipeAsyncThreadId[n],
				s_threadmgrRuntime.manager->threadClass,
				(SceObjectBase **)&object);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 837,
					"*** fatal error: msgPipeProcessExit() -> sceGUIDReferObjectWithClass()=0x%08X\n",
					ret);
				SCE_PANIC(838);
			}
			thread = &object->context;
			for (;;) {
				sceKernelSpinlockLowLock(THREAD_LOCK(thread));
				if (!(thread->status & SCE_KERNEL_THREAD_STATUS_WAITING)) {
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					break;
				}
				SCE_DBG_ASSERT_PRINTF(thread->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE, 852,
					"*** error: msgPipeProcessExit(), status=0x%08X, threadId=0x%08X[%s], waitId=0x%08X",
					thread->waitType, thread->threadId, SceSysmemForDriver_E655852F(thread->threadId),
					thread->waitId);
				waitId = thread->waitId;
				ret = sceGUIDReferObjectWithClass(waitId, MSG_PIPE_CLASS(),
								   (SceObjectBase **)&pipe);
				if (ret < 0) {
					SCE_DBG_PRINTF(0, 856,
						"*** fatal error: msgPipeProcessExit() -> sceGUIDGetObjectWithClass(0x%08X)=0x%08X\n",
						waitId, ret);
					SCE_PANIC(857);
				}
				if (SceCpuForDriver_5AC9D394(&pipe->event.lock) >= 0) {
					wakeupAndDistributeThread(thread, (uintptr_t)MSG_PIPE_QUEUE(pipe),
								  SCE_KERNEL_ERROR_WAIT_CANCEL);
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					sceKernelSpinlockLowUnlock(&pipe->event.lock);
					sceGUIDReleaseObject(waitId);
					break;
				}
				/* lock order: let go and try again */
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				sceGUIDReleaseObject(thread->waitId);
			}
			ret = sceGUIDReleaseObject(s_threadmgrRuntime.manager->msgPipeAsyncThreadId[n]);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 877,
					"*** fatal error: msgPipeProcessExit() -> sceGUIDReleaseObject()=0x%08X\n",
					ret);
				SCE_PANIC(878);
			}
			sceKernelCpuResumeIntr(intr);
			timeout = 1000;
			ret = sceKernelWaitCond(s_threadmgrRuntime.manager->msgPipeAsyncDoneCond[n],
						 &timeout);
			if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT && ret < 0) {
				SCE_DBG_PRINTF(0, 892,
					"*** fatal error: msgPipeProcessExit() -> sceKernelWaitCond()=0x%08X\n",
					ret);
				SCE_PANIC(893);
			}
			manager = s_threadmgrRuntime.manager;
		}
	}
	ret = sceKernelUnlockMutex(manager->msgPipeAsyncMutex, 1);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 899,
			"*** fatal error: msgPipeProcessExit() -> sceKernelUnlockMutex()=0x%08X\n", ret);
		SCE_PANIC(900);
	}
	/* the pipes whose buffers are in its memory */
	if (lockFastMutex(&process->msgPipeLock) < 0) {
		SCE_DBG_ASSERT_FAIL(906);
		return 0;
	}
	for (;;) {
		link = process->msgPipes.next;
		if (link == &process->msgPipes)
			break;
		pipe = MSG_PIPE_FROM_PROCESS_LINK(link);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pipe->event.lock);
		ListLinkRemove(link);
		if (sceKernelUnlockFastMutex(&process->msgPipeLock) < 0) {
			SCE_DBG_ASSERT_FAIL(920);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
			return 0;
		}
		uid = pipe->uid;
		memBlockId = pipe->memBlockId;
		pipe->ring.write = pipe->ring.base;
		pipe->ring.read = pipe->ring.base;
		pipe->waitDirection = 0;
		pipe->ring.used = 0;
		pipe->processId = -1;
		pipe->memBlockId = -1;
		queue = MSG_PIPE_QUEUE(pipe);
		for (node = pipe->event.next; node != queue; node = after) {
			after = node->next;
			if (!(node->owner & 1))
				continue;
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 949);
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE)
				wakeupAndDistributeThread(waiter, (uintptr_t)queue,
							  SCE_KERNEL_ERROR_MSG_PIPE_DELETED);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		if ((int)setEventWithNotifyCallback(uid, MSG_PIPE_EVENT_ERROR,
				(uint64_t)0xffffffff << 32 | SCE_KERNEL_ERROR_MSG_PIPE_DELETED,
				MSG_PIPE_EVENT_ERROR, &pipe->event) < 0)
			SCE_PANIC(967);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pipe->event.lock, intr);
		if (memBlockId != -1 && sceKernelFreeMemBlock(memBlockId) < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 975);
			return 0;
		}
		if (lockFastMutex(&process->msgPipeLock) < 0) {
			SCE_DBG_ASSERT_FAIL(982);
			return 0;
		}
	}
	if (sceKernelUnlockFastMutex(&process->msgPipeLock) < 0) {
		SCE_DBG_ASSERT_FAIL(989);
		return 0;
	}
	/* its msg pipe PUIDs */
	num = scePUIDGetUIDVectorByClass(process->pid, MSG_PIPE_CLASS(), 5, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 1007);
		num = scePUIDGetUIDVectorByClass(process->pid, MSG_PIPE_CLASS(), PUID_VECTOR_LEVEL,
						  vector, num, &got);
		if (num <= (int)got) {
			for (i = 0; i < num; i++) {
				guid = SceSysmemForDriver_45D22597(process->pid, vector[i]);
				/* Sony's: interrupts stay suspended when the pipe is gone already */
				intr = SceCpuForDriver_821FC0EE();
				if (sceGUIDReferObjectWithClass(guid, MSG_PIPE_CLASS(),
								 (SceObjectBase **)&pipe) < 0)
					continue;
				if (SceSysmemForDriver_84A4AF5E(process->pid, vector[i]) < 0)
					SCE_PANIC(1030);
				sceKernelSpinlockLowLock(&pipe->event.lock);
				ret = setEventWithNotifyCallback(guid, MSG_PIPE_EVENT_PROCESS_EXITED,
					(uint64_t)(int64_t)process->pid, MSG_PIPE_EVENT_PROCESS_EXITED,
					&pipe->event);
				sceKernelSpinlockLowUnlock(&pipe->event.lock);
				if (ret < 0)
					SCE_PANIC(1037);
				if (sceGUIDReleaseObject(guid) < 0)
					SCE_PANIC(1042);
				sceKernelCpuResumeIntr(intr);
			}
			break;
		}
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 1049);
	}
	return 0;
}

#define MSG_PIPE_KERNEL_ATTR_MASK	0x0000898f	/* what kernel callers may pass */
#define MSG_PIPE_USER_ATTR_MASK		0x0000098f	/* what a syscall may pass */
#define MSG_PIPE_OPT_FLAG_CAPABILITY		0x00000001
#define MSG_PIPE_OPT_FLAG_PID		0x00000002	/* the buffer goes in opt->pid's memory */
#define MSG_PIPE_OPT_FLAG_HEAP		0x00000100	/* the object goes in heap kind opt->heapKind */
#define MSG_PIPE_OPT_FLAGS		0x00000103
#define MSG_PIPE_MEMORY_KERNEL		0x00	/* the buffer's memory */
#define MSG_PIPE_MEMORY_USER		0x40
#define MSG_PIPE_MEMORY_USER_SHARED	0x41
#define MSG_PIPE_MEMORY_MAX		0x02000000	/* a process's buffers, from SDK 0x03100000 */
#define SDK_VERSION_MSG_PIPE_MEMORY	0x030fffff
#define MSG_PIPE_UID_ATTR		0x00030000	/* PLACEHOLDER: name */
#define MSG_PIPE_UID_ATTR_OPENABLE		0x0003c000	/* PLACEHOLDER: name */

/*
 * The buffer (bufferSize rounded up to pages, in memory of memType) and
 * the object, counted against the calling process's limits; lr, the
 * caller's return address, decides whether a shared buffer is allowed.
 */
SceUID sceKernelCreateMsgPipeWithLR(const char *name, int memType, SceUInt32 attr,
				    SceSize bufferSize, const ThreadmgrMsgPipeOptParam *option,
				    const void *lr)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *owner, *process;
	SceKernelAllocMemBlockKernelOpt blockOpt;
	UIDCreateOpt opt;
	ThreadmgrMsgPipeOptParam param;
	ThreadmgrMsgPipe *pipe;
	SceUInt32 heapAttr, blockType, version, flags, saved;
	SceSize size, memory;
	SceUID uid, blockId, pid, threadId;
	void *base;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 1088, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~MSG_PIPE_USER_ATTR_MASK) {
			SCE_DBG_PRINTF(0, 1102, " k: error: sceKernelCreateMsgPipe(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~MSG_PIPE_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF(0, 1115, " k: error: sceKernelCreateMsgPipe(%s), attr=0x%08X\n", name,
			       attr);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > sizeof(param)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	heapAttr = (attr & SCE_KERNEL_ATTR_OPENABLE) ? MSG_PIPE_UID_ATTR_OPENABLE :
							 MSG_PIPE_UID_ATTR;
	param.size = 0;
	param.flags = 0;
	param.capability = 0;
	param.pid = 0;
	param.heapKind = 0;
	flags = 0;
	if (option != NULL && option->size <= sizeof(param) &&
	    !(option->flags & ~MSG_PIPE_OPT_FLAGS)) {
#if FW_DEBUG
		__memcpy_chk(&param, option, option->size, sizeof(param));
#else
		memcpy(&param, option, option->size);
#endif
		flags = param.flags & MSG_PIPE_OPT_FLAG_PID;
		if (flags && SceSblACMgrForKernel_570D6AD3(0)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	}
	size = ((bufferSize + 0xfff) >> 12) * 0x1000;
	if (size == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	memset(&blockOpt, 0, sizeof(blockOpt));
	blockOpt.size = sizeof(blockOpt);
	if (memType == MSG_PIPE_MEMORY_USER) {
		pid = flags ? param.pid : sceKernelGetProcessId();
		blockOpt.pid = pid;
		blockOpt.attr = 0x70000080;
		blockType = 0x10c0d006;
	} else if (memType == MSG_PIPE_MEMORY_USER_SHARED &&
		   (!ThreadmgrInSyscall() || !SceSblACMgrForKernel_570D6AD3(0) ||
		    SceModulemgrForKernel_F1DE6949(thread->processId, (SceUInt32)lr))) {
		pid = flags ? param.pid : sceKernelGetProcessId();
		blockType = 0x10a0d006;
		blockOpt.pid = pid;
		blockOpt.attr = 0x70000080;
	} else if (memType == MSG_PIPE_MEMORY_KERNEL && !ThreadmgrInSyscall() &&
		   !SceSblACMgrForKernel_570D6AD3(0)) {
		blockOpt.attr = 0x70000000;
		blockType = 0x1020d006;
		pid = SCE_KERNEL_PROCESS_ID;
	} else {
		if (memType == MSG_PIPE_MEMORY_KERNEL && ThreadmgrInSyscall())
			SCE_DBG_ASSERT_FAIL(1181);
		ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
		goto restore;
	}
	blockId = sceKernelAllocMemBlockForKernel(name, blockType, size, &blockOpt);
	if (blockId < 0) {
		ret = blockId;
		goto restore;
	}
	sceKernelGetMemBlockBase(blockId, &base);
	if (param.flags & MSG_PIPE_OPT_FLAG_HEAP) {
		if (param.heapKind >= 0x80) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_OPEN_LIMIT;
			goto restore;
		}
		heapAttr |= param.heapKind + (ThreadmgrInSyscall() ? 2 : 1);
	}
	ret = sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					   (SceObjectBase **)&owner);
	if (ret < 0)
		goto freeBlock;
	ret = lockFastMutex(&owner->msgPipeLock);
	if (ret < 0)
		goto releaseOwner;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.pid = 0;
	opt.heapKind = 0;
	opt.type = 0;
	opt.attr = heapAttr;
	opt.ownerPid = 0;
	opt.cls = NULL;
	opt.flags = UID_CREATE_OPT_ATTR;
	if (ThreadmgrInSyscall() && !(attr & SCE_KERNEL_ATTR_OPENABLE)) {
		opt.pid = thread->processId;
		opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED) {
			attr |= MSG_PIPE_ATTR_UNLIMITED;
			opt.heapKind = 1;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		} else {
			opt.heapKind = 0;
		}
	}
	process = NULL;
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		memory = process->msgPipeMemory + size;
		if (memory > MSG_PIPE_MEMORY_MAX) {
			if (thread->processId == SCE_KERNEL_PROCESS_ID) {
				version = SDK_VERSION_KERNEL;
				goto tooMuch;
			}
			if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0)
				version = 0;
			else if (version > SDK_VERSION_MSG_PIPE_MEMORY)
				goto tooMuch;
			memory = process->msgPipeMemory + size;
		}
		/* Sony's: not taken back when a limit below fails */
		process->msgPipeMemory = memory;
		if (attr & SCE_KERNEL_ATTR_OPENABLE) {
			if (process->uidOpenableCount >= UID_OPENABLE_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF(0, 1315, "sceKernelCreateMsgPipeWithLR(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto unlockOwner;
			}
			if (++process->uidOpenableCount > process->uidOpenablePeak)
				process->uidOpenablePeak = process->uidOpenableCount;
		} else if (attr & MSG_PIPE_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= UID_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				SCE_DBG_PRINTF(0xb, 1332,
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				goto unlockOwner;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->msgPipeCount > process->msgPipePeak)
			process->msgPipePeak = process->msgPipeCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(MSG_PIPE_CLASS(), name, &opt,
					  (SceObjectBase **)&pipe);
	if (uid < 0) {
		ret = uid;
		if (ThreadmgrInSyscall()) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (attr & MSG_PIPE_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->msgPipeCount--;
			process->msgPipeMemory -= size;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		goto unlockOwner;
	}
	pipe->event.ownerPid = ThreadmgrInSyscall() ? thread->processId : SCE_KERNEL_PROCESS_ID;
	pipe->processLink.next = &owner->msgPipes;
	pipe->processLink.prev = owner->msgPipes.prev;
	owner->msgPipes.prev = &pipe->processLink;
	pipe->processLink.prev->next = &pipe->processLink;
	pipe->memBlockId = blockId;
	pipe->ring.size = size;
	pipe->uid = uid;
	pipe->processId = pid;
	pipe->ring.base = base;
	pipe->ring.write = base;
	pipe->ring.read = base;
	pipe->ring.used = 0;
	pipe->waitDirection = 0;
	pipe->waitCount = 0;
	pipe->creatorPid = sceKernelGetProcessId();
	if (param.flags & MSG_PIPE_OPT_FLAG_CAPABILITY) {
		pipe->flags |= 1;
		pipe->openCapability = param.capability;
	}
	threadId = ThreadmgrInSyscall() ? thread->userThreadId : thread->threadId;
	/* sceKernelEventObjectInit unlocks the object the class hands over locked */
	ret = sceKernelEventObjectInit(uid, attr,
		(uint64_t)(int64_t)threadId << 32 | (uint64_t)(int64_t)thread->processId, 4);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 1383);
	sceKernelCpuResumeIntr(intr);
	ret = sceKernelUnlockFastMutex(&owner->msgPipeLock);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 1390);
	sceGUIDReleaseObject(pid);
	ret = uid;
	goto restore;
tooMuch:
	ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE | SYSCALL_ERROR_BIT;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
unlockOwner:
	sceKernelCpuResumeIntr(intr);
	{
		int r = sceKernelUnlockFastMutex(&owner->msgPipeLock);
		SCE_DBG_ASSERT_CHECK(r >= 0, 1417);
	}
releaseOwner:
	sceGUIDReleaseObject(pid);
freeBlock:
	sceKernelFreeMemBlock(blockId);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUID sceKernelCreateMsgPipe(const char *name, int memType, SceUInt32 attr,
				      SceSize bufferSize, const ThreadmgrMsgPipeOptParam *option)
{
	return sceKernelCreateMsgPipeWithLR(name, memType, attr, bufferSize, option,
					    __builtin_return_address(0));
}

#define MSG_PIPE_EVENT_DELETED		0x00000008
#define MSG_PIPE_EVENT_OPENED		0x00000100

/* a kernel caller closes the pipe, which sets MSG_PIPE_EVENT_DELETED */
int sceKernelDeleteMsgPipe(SceUID msgPipeId)
{
	struct ThreadCB *thread;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1466, "sceKernelDeleteMsgPipe", "*** error: illegal context. msgPipeId=0x%08x[%s]\n",
				      msgPipeId, SceSysmemForDriver_E655852F(msgPipeId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject) < 0) {
		ret = MsgPipeUnknownIdError();
		goto resume;
	}
	pipe = (ThreadmgrMsgPipe *)pObject;
	if (sceGUIDClose(msgPipeId) < 0)
		SCE_PANIC_FUNC(SCE_LINE(1496, 1488), "sceKernelDeleteMsgPipe");
	if (ThreadmgrInSyscall()) {
		/* not for syscalls: they close their PUID */
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1503, "sceKernelDeleteMsgPipe");
		wake = 0;
	} else {
		thread = currentThreadContext();
		sceKernelSpinlockLowLock(&pipe->event.lock);
		wake = setEventWithNotifyCallback(msgPipeId, MSG_PIPE_EVENT_DELETED,
			(uint64_t)(int64_t)thread->threadId << 32 |
			(uint64_t)(int64_t)thread->processId, MSG_PIPE_EVENT_DELETED, &pipe->event);
		sceKernelSpinlockLowUnlock(&pipe->event.lock);
		if ((int)wake < 0)
			SCE_PANIC_FUNC(SCE_LINE(1507, 1499), "sceKernelDeleteMsgPipe");
	}
	if (sceGUIDReleaseObject(msgPipeId) < 0)
		SCE_PANIC_FUNC(SCE_LINE(1516, 1508), "sceKernelDeleteMsgPipe");
	ret = 0;
	if (wake & 1)
		switchThread(0);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the syscall opening a pipe by name, which sets MSG_PIPE_EVENT_OPENED */
SceUID sceKernelOpenMsgPipeForUser(const char *userName)
{
	struct ThreadCB *thread =
		currentThreadContext();
	UIDCreateOpt opt;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	char name[32];
	uint32_t wake;
	SceUID puid;
	int ret;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 1597, "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	ret = copyinUIDNameWithAttr(name, userName, SCE_KERNEL_ATTR_OPENABLE);
	if (ret < 0)
		return ret;
	opt.attr = PUID_ATTR_OPENED;
	opt.object = NULL;
	opt.cls = MSG_PIPE_CLASS();
	opt.heapGuid = 0;
	opt.pid = 0;
	opt.heapKind = 0;
	opt.type = 0;
	opt.ownerPid = 0;
	opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_CLASS;
	puid = SceSysmemForKernel_201E970B(0, name, &opt);
	if (puid < 0)
		return puid;
	ret = SceSysmemForDriver_FE6D7FAE(0, puid, &pObject);
	if (ret < 0) {
		if (SceSysmemForDriver_84A4AF5E(0, puid) < 0)
			SCE_PANIC(SCE_LINE(1629, 1621));
		return ret;
	}
	pipe = (ThreadmgrMsgPipe *)pObject;
	if (pipe->flags & 1) {
		ret = SceSblACMgrForKernel_8241AB5C(currentThreadContext()->processId,
						    pipe->openCapability);
		if (ret < 0) {
			if (SceSysmemForDriver_84A4AF5E(0, puid) < 0)
				SCE_PANIC(SCE_LINE(1642, 1634));
			return ret;
		}
	}
	wake = sceKernelSetEventWithNotifyCallback(SceSysmemForDriver_45D22597(0, puid), MSG_PIPE_EVENT_OPENED,
		(uint64_t)(int64_t)thread->userThreadId << 32 | (uint64_t)(int64_t)thread->processId,
		MSG_PIPE_EVENT_OPENED);
	SCE_DBG_ASSERT_CHECK((int)wake >= 0, 1643);
	return puid;
}

/* SceThreadmgrForDriver_88206D29: the process that created the pipe of that name */
SceUID SceThreadmgrForDriver_88206D29(const char *name)
{
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	uint32_t saved;
	SceUID guid, ret;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	guid = SceSysmemForKernel_BDE00106(name);
	ret = guid;
	if (guid >= 0) {
		if (sceGUIDReferObjectWithClass(guid, MSG_PIPE_CLASS(), &pObject) < 0) {
			ret = MsgPipeUnknownIdError();
		} else {
		pipe = (ThreadmgrMsgPipe *)pObject;
			ret = pipe->creatorPid;
			sceGUIDReleaseObject(guid);
		}
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_67DD3BAD(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode, SceSize *result, SceUInt32 *timeout)
{
	ThreadmgrMsgPipeCall call;

	call.msgPipeId = msgPipeId;
	call.pid = sceKernelGetProcessId();
	call.vector = vector;
	call.count = count;
	call.mode = mode;
	call.result = result;
	call.timeout = timeout;
	return sendMsgPipeVector(&call, 0);
}

/* SceThreadmgrForDriver_F5074173: ksceKernelSendMsgPipe, one buffer */
int SceThreadmgrForDriver_F5074173(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode, SceSize *result,
	SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return SceThreadmgrForDriver_67DD3BAD(msgPipeId, &vector, 1, mode, result, timeout);
}

/* SceThreadmgrForDriver_2FC0FFF6: ksceKernelSendMsgPipeVectorCB */
int SceThreadmgrForDriver_2FC0FFF6(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode,
	SceSize *result, SceUInt32 *timeout)
{
	ThreadmgrMsgPipeCall call;

	call.msgPipeId = msgPipeId;
	call.pid = sceKernelGetProcessId();
	call.vector = vector;
	call.count = count;
	call.mode = mode;
	call.result = result;
	call.timeout = timeout;
	return sendMsgPipeVector(&call, 1);
}

/* SceThreadmgrForDriver_E6D80698: ksceKernelSendMsgPipeCB, one buffer */
int SceThreadmgrForDriver_E6D80698(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode, SceSize *result,
	SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return SceThreadmgrForDriver_2FC0FFF6(msgPipeId, &vector, 1, mode, result, timeout);
}

int SceThreadmgrForDriver_DA1F256B(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode,
	SceSize *result, SceUInt32 *timeout)
{
	ThreadmgrMsgPipeCall call;

	call.msgPipeId = msgPipeId;
	call.pid = sceKernelGetProcessId();
	call.vector = vector;
	call.count = count;
	call.mode = mode;
	call.result = result;
	call.timeout = timeout;
	return recvMsgPipeVector(&call, 0);
}

/* SceThreadmgrForDriver_2F9F75AA: ksceKernelReceiveMsgPipe, one buffer */
int SceThreadmgrForDriver_2F9F75AA(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode, SceSize *result,
	SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return SceThreadmgrForDriver_DA1F256B(msgPipeId, &vector, 1, mode, result, timeout);
}

int SceThreadmgrForDriver_DA5C9AC6(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				     int count, uint32_t mode,
	SceSize *result, SceUInt32 *timeout)
{
	ThreadmgrMsgPipeCall call;

	call.msgPipeId = msgPipeId;
	call.pid = sceKernelGetProcessId();
	call.vector = vector;
	call.count = count;
	call.mode = mode;
	call.result = result;
	call.timeout = timeout;
	return recvMsgPipeVector(&call, 1);
}

/* SceThreadmgrForDriver_D0262C55: ksceKernelReceiveMsgPipeCB, one buffer */
int SceThreadmgrForDriver_D0262C55(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode, SceSize *result,
	SceUInt32 *timeout)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return SceThreadmgrForDriver_DA5C9AC6(msgPipeId, &vector, 1, mode, result, timeout);
}

#define SDK_VERSION_TRY_EVENT		0x02100000	/* from here a try sets the pipe's event */

/*
 * Sends what can go now, never waiting: to waiting receivers, then into
 * the ring (MSG_PIPE_MODE_ALL: all of it or SCE_KERNEL_ERROR_MSG_PIPE_FULL).
 * The vector is the caller's own process's.
 */
int sceKernelTrySendMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				  int count, uint32_t mode, SceSize *result)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *waiter;
	SceKernelMsgPipeVector buffer;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	SceSize total = 0, remaining = 0, offset = 0, n, room, moved, savedUsed;
	SceUInt32 version;
	uint8_t *savedRead;
	uint32_t saved, wake = 0, pattern;
	int index = 0, i, intr, ret, direction;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 2720, "sceKernelTrySendMsgPipeVector", "*** error: unknown msgpipe id. msgPipeId=0x%08x\n",
					      msgPipeId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
		}
		goto resume;
	}
	pipe = (ThreadmgrMsgPipe *)pObject;
	sceKernelSpinlockLowLock(&pipe->event.lock);
	queue = MSG_PIPE_QUEUE(pipe);
	if (vector == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto report;
	}
	if (count == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto report;
	}
	if (mode & ~MSG_PIPE_MODE_ALL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto report;
	}
	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID) {
		for (i = 0; i < count; i++) {
			if (vector[i].pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				goto report;
			}
			if (pipe->ring.size < vector[i].bufSize ||
			    pipe->ring.size < (remaining += vector[i].bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				goto report;
			}
		}
	} else {
		for (i = 0; i < count; i++) {
			ret = sceKernelCopyFromUser(&buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF_FUNC(0xf, 2768, "sceKernelTrySendMsgPipeVector",
					"sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
					&buffer, &vector[i], sizeof(buffer), ret);
				goto report;
			}
			if (buffer.pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				goto report;
			}
			if (pipe->ring.size < buffer.bufSize ||
			    pipe->ring.size < (remaining += buffer.bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				goto report;
			}
		}
	}
	total = remaining;
	if (pipe->processId == -1) {
		ret = SCE_KERNEL_ERROR_MSG_PIPE_DELETED;
		goto report;
	}
	if (pipe->ring.size != 0 && pipe->ring.size < total) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto report;
	}
	direction = pipe->waitDirection;
	if (direction == MSG_PIPE_WAIT_SEND) {
		/* senders wait: only one more urgent than all of them goes first */
		if (currentThreadContext() == NULL ||
		    !(pipe->event.attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S)) {
			ret = SCE_KERNEL_ERROR_MSG_PIPE_FULL;
			goto report;
		}
		for (node = pipe->event.next; node != queue; node = node->next) {
			if (!(node->owner & 1))
				continue;
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 2817, "sceKernelTrySendMsgPipeVector");
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
				/* Sony's: the waiter stays locked on these two */
				if (pipe->partial)
					goto full;
				if (waiter->currentPriority <= thread->currentPriority) {
					ret = SCE_KERNEL_ERROR_MSG_PIPE_FULL;
					goto report;
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				break;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		direction = pipe->waitDirection;
	}
	if (mode & MSG_PIPE_MODE_ALL) {
		/* all of it must go: the ring's room and what the receivers take */
		room = pipe->ring.size - pipe->ring.used;
		if (direction == MSG_PIPE_WAIT_RECEIVE) {
			for (node = pipe->event.next; node != queue; node = node->next) {
				if (!(node->owner & 1))
					continue;
				waiter = (struct ThreadCB *)node;
				SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 2850, "sceKernelTrySendMsgPipeVector");
				if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE)
					continue;
				if (total <= room)
					goto send;
				room += waiter->msgPipeRemaining;
			}
		}
		if (room < total)
			goto full;
	}
send:
	savedUsed = 0;
	savedRead = NULL;
	node = pipe->event.next;
	while (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE && pipe->waitCount > 0) {
		SCE_DBG_ASSERT_CHECK_FUNC((uint32_t)pipe->waitCount <= pipe->event.waiting, 2871, "sceKernelTrySendMsgPipeVector");
		SCE_DBG_ASSERT_CHECK_FUNC(node != queue, 2872, "sceKernelTrySendMsgPipeVector");
		next = node->next;
		if (!(node->owner & 1)) {
			node = next;
			continue;
		}
		waiter = (struct ThreadCB *)node;
		node = next;
		SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 2880, "sceKernelTrySendMsgPipeVector");
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE) {
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			continue;
		}
		if (waiter->waitOptions & MSG_PIPE_MODE_PEEK) {
			savedRead = pipe->ring.read;
			savedUsed = pipe->ring.used;
			pipe->ring.read = pipe->ring.write;
			pipe->ring.used = 0;
		}
		n = remaining;
		if (waiter->msgPipeRemaining <= n)
			n = waiter->msgPipeRemaining;
		ret = sendPipeVector(&pipe->ring, 0, vector, &index, &offset, n,
				     waiter->waitOptions & MSG_PIPE_MODE_PEEK, 0);
		if (ret >= 0)
			ret = recvPipeVector(&pipe->ring, waiter->timeoutProcessId, waiter->msgPipeVector,
				&waiter->msgPipeVectorIndex, &waiter->msgPipeVectorOffset, n,
				waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
		if (ret < 0)
			goto unlockReport;
		if (!(waiter->waitOptions & MSG_PIPE_MODE_PEEK))
			remaining -= n;
		waiter->msgPipeRemaining -= n;
		if (waiter->msgPipeResult != NULL) {
			moved = waiter->msgPipeSize - waiter->msgPipeRemaining;
			ret = setMsgPipeResult(waiter, &moved);
			if (ret < 0)
				goto unlockReport;
		}
		if ((waiter->waitOptions & MSG_PIPE_MODE_ALL) && waiter->msgPipeRemaining != 0) {
			pipe->partial = 1;
		} else {
			pipe->partial = 0;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
		}
		if (waiter->waitOptions & MSG_PIPE_MODE_PEEK) {
			SCE_DBG_ASSERT_CHECK_FUNC(savedRead != NULL, 2950, "sceKernelTrySendMsgPipeVector");
			pipe->ring.read = savedRead;
			pipe->ring.used = savedUsed;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		if (remaining == 0)
			goto done;
	}
	if (remaining != 0) {
		n = pipe->ring.size - pipe->ring.used;
		if (remaining <= n)
			n = remaining;
		if ((mode & MSG_PIPE_MODE_ALL) ? remaining == n : n != 0) {
			ret = sendPipeVector(&pipe->ring, 0, vector, &index, &offset, n, 0, 0);
			if (ret < 0)
				goto report;
			remaining -= n;
		}
	}
done:
	if (result != NULL) {
		moved = total - remaining;
		if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID) {
			*result = moved;
		} else {
			ret = sceKernelCopyToUser(result, &moved, sizeof(moved));
			if (ret < 0)
				goto report;
		}
	}
	if (mode & MSG_PIPE_MODE_ALL)
		ret = 0;
	else
		ret = total != remaining ? 0 : SCE_KERNEL_ERROR_MSG_PIPE_FULL;
	goto report;
full:
	wake = 0;
	ret = SCE_KERNEL_ERROR_MSG_PIPE_FULL;
	goto report;
unlockReport:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
report:
	/* older SDKs' callers get no event */
	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == SCE_KERNEL_PROCESS_ID) {
		version = SDK_VERSION_KERNEL;
	} else if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0 ||
		   version < SDK_VERSION_TRY_EVENT) {
		goto unlock;
	}
	if (total == remaining)
		pattern = 0;
	else if (pipe->ring.write == pipe->ring.read &&
		 (pipe->ring.used == 0 || version < SDK_VERSION_MSG_PIPE_EMPTY))
		pattern = MSG_PIPE_EVENT_SENT;
	else
		pattern = MSG_PIPE_EVENT_SENT | EVENT_PATTERN_RESERVED_BIT;
	if (ret != 0)
		pattern |= MSG_PIPE_EVENT_ERROR;
	pattern = setEventWithNotifyCallback(msgPipeId, pattern,
		(uint64_t)((total - remaining) | (uint32_t)(ret >> 31)) << 32 | (uint32_t)ret,
		pattern, &pipe->event);
	if ((int)pattern < 0)
		SCE_PANIC_FUNC(SCE_LINE(3044, 3036), "sceKernelTrySendMsgPipeVector");
	wake |= pattern;
unlock:
	sceKernelSpinlockLowUnlock(&pipe->event.lock);
	sceGUIDReleaseObject(msgPipeId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_85F4DC8C: ksceKernelTrySendMsgPipe, one buffer */
int SceThreadmgrForDriver_85F4DC8C(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return sceKernelTrySendMsgPipeVector(msgPipeId, &vector, 1, mode, result);
}

/*
 * Receives what is there now, never waiting: from the ring, then from
 * waiting senders (who refill the ring); MSG_PIPE_MODE_ALL: all of it or
 * SCE_KERNEL_ERROR_MSG_PIPE_EMPTY. The vector is the caller's own process's.
 */
int sceKernelTryReceiveMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				     int count, uint32_t mode, SceSize *result)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *queue, *node, *pos;
	struct ThreadCB *waiter;
	SceKernelMsgPipeVector buffer;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	SceSize total = 0, remaining = 0, offset = 0, n, m, available, moved, senderRemaining;
	SceSize savedUsed;
	SceUInt32 version;
	uint8_t *savedWrite, *savedRead;
	uint32_t saved, wake = 0, pattern, peek;
	bool progressed;
	int index = 0, i, intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 4039, "sceKernelTryReceiveMsgPipeVector", "*** error: unknown msgpipe id. msgPipeId=0x%08x\n",
					      msgPipeId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
		}
		goto resume;
	}
	pipe = (ThreadmgrMsgPipe *)pObject;
	sceKernelSpinlockLowLock(&pipe->event.lock);
	queue = MSG_PIPE_QUEUE(pipe);
	if (vector == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto report;
	}
	if (count == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto report;
	}
	if (mode & ~(MSG_PIPE_MODE_ALL | MSG_PIPE_MODE_PEEK)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto report;
	}
	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID) {
		for (i = 0; i < count; i++) {
			if (vector[i].pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				goto report;
			}
			if (pipe->ring.size < vector[i].bufSize ||
			    pipe->ring.size < (remaining += vector[i].bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				goto report;
			}
		}
	} else {
		for (i = 0; i < count; i++) {
			ret = sceKernelCopyFromUser(&buffer, &vector[i], sizeof(buffer));
			if (ret < 0) {
				SCE_DBG_PRINTF_FUNC(0xf, 4087, "sceKernelTryReceiveMsgPipeVector",
					"sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
					&buffer, &vector[i], sizeof(buffer), ret);
				goto report;
			}
			if (buffer.pBase == NULL) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
				goto report;
			}
			if (pipe->ring.size < buffer.bufSize ||
			    pipe->ring.size < (remaining += buffer.bufSize)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
				goto report;
			}
		}
	}
	total = remaining;
	if (pipe->processId == -1) {
		ret = SCE_KERNEL_ERROR_MSG_PIPE_DELETED;
		goto report;
	}
	if (pipe->ring.size != 0 && pipe->ring.size < total) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto report;
	}
	if (pipe->waitDirection == MSG_PIPE_WAIT_RECEIVE) {
		/* receivers wait: only one more urgent than all of them goes first */
		if (currentThreadContext() == NULL ||
		    !(pipe->event.attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R)) {
			ret = SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
			goto report;
		}
		for (pos = pipe->event.next; pos != queue; pos = pos->next) {
			if (!(pos->owner & 1))
				continue;
			waiter = (struct ThreadCB *)pos;
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 4136, "sceKernelTryReceiveMsgPipeVector");
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
				/* Sony's: the waiter stays locked on these two */
				if (pipe->partial)
					goto empty;
				if (waiter->currentPriority <= thread->currentPriority) {
					ret = SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
					goto report;
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				break;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
	}
	if (mode & MSG_PIPE_MODE_ALL) {
		/* all of it must come: the ring's data and what the senders give */
		available = pipe->ring.used;
		if (pipe->waitDirection == MSG_PIPE_WAIT_SEND) {
			for (pos = pipe->event.next; pos != queue; pos = pos->next) {
				if (!(pos->owner & 1))
					continue;
				waiter = (struct ThreadCB *)pos;
				SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 4167, "sceKernelTryReceiveMsgPipeVector");
				if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE)
					continue;
				if (total <= available)
					goto receive;
				available += waiter->msgPipeRemaining;
			}
		}
		if (available < total)
			goto empty;
	}
receive:
	savedUsed = pipe->ring.used;
	savedWrite = pipe->ring.write;
	savedRead = pipe->ring.read;
	if (savedUsed != 0) {
		n = savedUsed;
		if (total < n)
			n = total;
		ret = recvPipeVector(&pipe->ring, 0, vector, &index, &offset, n, 0);
		if (ret < 0)
			goto report;
		remaining = total - n;
	}
	peek = mode & MSG_PIPE_MODE_PEEK;
	node = pipe->event.next;
	while (pipe->waitDirection == MSG_PIPE_WAIT_SEND && pipe->waitCount > 0) {
		SCE_DBG_ASSERT_CHECK_FUNC((uint32_t)pipe->waitCount <= pipe->event.waiting, 4203, "sceKernelTryReceiveMsgPipeVector");
		SCE_DBG_ASSERT_CHECK_FUNC(node != queue, 4204, "sceKernelTryReceiveMsgPipeVector");
		pos = node;
		node = node->next;
		if (!(pos->owner & 1))
			continue;
		waiter = (struct ThreadCB *)pos;
		SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 4215, "sceKernelTryReceiveMsgPipeVector");
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitType != SCE_KERNEL_WAITTYPE_MSG_PIPE)
			goto next;
		if (remaining == 0) {
			if (peek)
				goto next;
			senderRemaining = waiter->msgPipeRemaining;
			progressed = false;
		} else {
			n = waiter->msgPipeRemaining;
			if (remaining < n)
				n = remaining;
			ret = sendPipeVector(&pipe->ring, waiter->timeoutProcessId, waiter->msgPipeVector,
				&waiter->msgPipeVectorIndex, &waiter->msgPipeVectorOffset, n, peek,
				waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
			if (ret >= 0)
				ret = recvPipeVector(&pipe->ring, 0, vector, &index, &offset, n, 0);
			if (ret < 0)
				goto unlockReport;
			remaining -= n;
			if (peek)
				goto next;
			progressed = true;
			senderRemaining = waiter->msgPipeRemaining - n;
			waiter->msgPipeRemaining = senderRemaining;
		}
		/* the sender refills the ring with what fits */
		m = pipe->ring.size - pipe->ring.used;
		if (senderRemaining <= m)
			m = senderRemaining;
		if ((waiter->waitOptions & MSG_PIPE_MODE_ALL) ? m == senderRemaining : m != 0) {
			ret = sendPipeVector(&pipe->ring, waiter->timeoutProcessId, waiter->msgPipeVector,
				&waiter->msgPipeVectorIndex, &waiter->msgPipeVectorOffset, m, 0,
				waiter->waitOptions & MSG_PIPE_MODE_KERNEL_VECTOR);
			if (ret < 0)
				goto unlockReport;
			progressed = true;
			senderRemaining = waiter->msgPipeRemaining - m;
			waiter->msgPipeRemaining = senderRemaining;
		}
		if (waiter->msgPipeResult != NULL) {
			moved = waiter->msgPipeSize - senderRemaining;
			ret = setMsgPipeResult(waiter, &moved);
			if (ret < 0)
				goto unlockReport;
		}
		if ((!(waiter->waitOptions & MSG_PIPE_MODE_ALL) && progressed) ||
		    waiter->msgPipeRemaining == 0) {
			pipe->partial = 0;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
		} else {
			pipe->partial = 1;
		}
next:
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	if (peek) {
		/* a peek leaves the ring as it was */
		pipe->ring.write = savedWrite;
		pipe->ring.read = savedRead;
		pipe->ring.used = savedUsed;
	}
	if (result != NULL) {
		moved = total - remaining;
		if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID) {
			*result = moved;
		} else {
			ret = sceKernelCopyToUser(result, &moved, sizeof(moved));
			if (ret < 0)
				goto report;
		}
	}
	if (mode & MSG_PIPE_MODE_ALL)
		ret = 0;
	else
		ret = total != remaining ? 0 : SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
	goto report;
empty:
	wake = 0;
	ret = SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
	goto report;
unlockReport:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
report:
	updateMsgPipeDataPattern(pipe);
	/* older SDKs' callers get no event */
	if (currentThreadContext() != NULL &&
	    currentThreadContext()->processId != SCE_KERNEL_PROCESS_ID &&
	    (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0 ||
	     version < SDK_VERSION_TRY_EVENT))
		goto unlock;
	pattern = total == remaining ? 0 : MSG_PIPE_EVENT_RECEIVED;
	if (ret != 0)
		pattern |= MSG_PIPE_EVENT_ERROR;
	pattern = setEventWithNotifyCallback(msgPipeId, pattern,
		(uint64_t)((total - remaining) | (uint32_t)(ret >> 31)) << 32 | (uint32_t)ret,
		pattern, &pipe->event);
	if ((int)pattern < 0)
		SCE_PANIC_FUNC(SCE_LINE(4383, 4375), "sceKernelTryReceiveMsgPipeVector");
	wake |= pattern;
unlock:
	sceKernelSpinlockLowUnlock(&pipe->event.lock);
	sceGUIDReleaseObject(msgPipeId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_2E95B628: ksceKernelTryReceiveMsgPipe, one buffer */
int SceThreadmgrForDriver_2E95B628(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result)
{
	SceKernelMsgPipeVector vector;

	vector.pBase = buffer;
	vector.bufSize = size;
	return sceKernelTryReceiveMsgPipeVector(msgPipeId, &vector, 1, mode, result);
}

/*
 * Empties the pipe and wakes every waiter with SCE_KERNEL_ERROR_WAIT_CANCEL;
 * the counts of senders and receivers woken go to the outputs.
 */
int sceKernelCancelMsgPipe(SceUID msgPipeId, int *numSendWaitThreads,
			   int *numReceiveWaitThreads)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *waiter;
	ThreadmgrMsgPipe *pipe;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret, direction, count, sending, receiving;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(msgPipeId, MSG_PIPE_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 4435, "sceKernelCancelMsgPipe", "*** error: unknown msgpipe id. msgPipeId=0x%08x\n",
					      msgPipeId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
		}
		goto resume;
	}
	pipe = (ThreadmgrMsgPipe *)pObject;
	queue = MSG_PIPE_QUEUE(pipe);
	sceKernelSpinlockLowLock(&pipe->event.lock);
	if (pipe->processId == -1) {
		sceKernelSpinlockLowUnlock(&pipe->event.lock);
		ret = SCE_KERNEL_ERROR_MSG_PIPE_DELETED;
		sceGUIDReleaseObject(msgPipeId);
		goto resume;
	}
	pipe->ring.write = pipe->ring.base;
	pipe->ring.read = pipe->ring.base;
	direction = pipe->waitDirection;
	pipe->ring.used = 0;
	pipe->event.pattern &= ~EVENT_PATTERN_RESERVED_BIT;
	pipe->waitDirection = 0;
	wake = 0;
	count = 0;
	for (node = pipe->event.next; node != queue; node = next) {
		next = node->next;
		if (!(node->owner & 1))
			continue;
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 4471, "sceKernelCancelMsgPipe");
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE) {
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue,
							  SCE_KERNEL_ERROR_WAIT_CANCEL);
			count++;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	sceKernelSpinlockLowUnlock(&pipe->event.lock);
	sceGUIDReleaseObject(msgPipeId);
	if (direction & MSG_PIPE_WAIT_SEND) {
		sending = count;
		receiving = 0;
	} else {
		receiving = (direction & MSG_PIPE_WAIT_RECEIVE) ? count : 0;
		sending = 0;
	}
	if (numSendWaitThreads != NULL)
		*numSendWaitThreads = sending;
	if (numReceiveWaitThreads != NULL)
		*numReceiveWaitThreads = receiving;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_207E0E18(SceUID msgPipeId, SceKernelMsgPipeInfo *info)
{
	SceKernelMsgPipeInfo local;
	uint32_t saved;
	SceSize size;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getMsgPipeInfo(msgPipeId, &local);
		if (ret >= 0) {
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_C7F5FFE0: the information of every msg pipe visible at level */
int SceThreadmgrForDriver_C7F5FFE0(int level, SceKernelMsgPipeInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelMsgPipeInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(MSG_PIPE_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(MSG_PIPE_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getMsgPipeInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* SceThreadmgrForDriver_E9E50096: the same for a process's msg pipe PUIDs */
int SceThreadmgrForDriver_E9E50096(SceUID pid, int level, SceKernelMsgPipeInfo *infos,
				   SceSize size, SceSize *count)
{
	SceKernelMsgPipeInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, MSG_PIPE_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid, MSG_PIPE_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getMsgPipeInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.msgPipeId = vector[i];
		if (scePUIDGetAttr(pid, info.msgPipeId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
			info.attr |= PUID_ATTR_OPENED;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}
