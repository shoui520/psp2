/* SceIofilemgr's queues of requests (Sony's sched_io_request_queue.c). */
#include "iofilemgr_internal.h"

/* name unknown: diagnostic flag for the optional PFS read hook. */
#if FW_VERSION >= 0x04000000
int g_ioPfsPreadEnabled __attribute__((section(".data"))) = 0;
int g_ioPfsReadEnabled __attribute__((section(".data"))) = 0;
int g_ioPfsWriteEnabled __attribute__((section(".data"))) = 0;
int g_ioPfsPwriteEnabled __attribute__((section(".data"))) = 0;
#endif
int sceIoSchedulerIsDataIo(IoRequest *req);
int sceIoSchedulerIsStorageIo(IoRequest *req);

int readIoChunkByPread(IoRequest *req, IoChunk *chunk)
{
	IofilemgrPreadParam param;
	int ret;

	SCE_DBG_ASSERT(req != NULL, 651);
	SCE_DBG_ASSERT(req->type == IO_REQUEST_READ || req->type == IO_REQUEST_PREAD, 653);
	param.fd = req->args.rw.fd;
	if (chunk != NULL) {
		param.data = (void *)chunk->data;
		param.size = chunk->size;
		param.offset = req->args.rw.offset + chunk->offset;
	} else {
		param.data = NULL;
		param.size = 0;
		param.offset = 0;
	}
	if (req->mntOpt & 0x10000) {
		SCE_DBG_ASSERT(g_ioPfsPreadEnabled != 0, 673);
		ret = ksceKernelRunWithStack(0x2000, NULL, &param);
	} else {
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsPread, &param);
	}
	if (ret == SCE_ERROR_ERRNO_EUNSUP)
		SCE_DBG_ASSERT(0, 697);
	return ret;
}

/* name unknown: allocate the queue for one scheduler priority. */
IoRequestQueue *sceIoRequestQueueCreateIoRequestQueue(IoScheduler *sched, int priority)
{
	IoRequestQueue *queue;

	queue = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*queue));
	if (queue != NULL) {
		queue->sched = sched;
		queue->priority = priority;
		queue->timeout = 10000000;
		queue->head = NULL;
		queue->tail = NULL;
	}
	return queue;
}

int sceIoRequestQueueAddIoRequest(IoRequestQueue *queue, IoRequest *req)
{
	SCE_DBG_ASSERT(req->isRunning != 1, 95);
	SCE_DBG_ASSERT(req->isCanceled != 1, 96);
	SCE_DBG_ASSERT(req->next == NULL, 97);
	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 98);
	SCE_DBG_ASSERT((unsigned int)req->priority <= 15, 99);
	req->next = NULL;
	req->prev = NULL;
	SCE_DBG_PRINTF_LEVEL(2, 0, 107, "%s %d : tid 0x%x, prio %d\n",
			"sceIoRequestQueueAddIoRequest", 107, ksceKernelGetThreadId(),
			ksceKernelGetThreadCurrentPriority());
	if (queue->tail != NULL) {
		SCE_DBG_ASSERT(queue->tail->next != req, 113);
		queue->tail->next = req;
		req->prev = queue->tail;
		queue->tail = req;
	} else {
		queue->head = req;
		queue->tail = req;
	}
	req->time = sceKernelGetSystemTimeWide();
	return 0;
}

int sceIoRequestQueueRemoveIoRequest(IoRequestQueue *queue, IoRequest *req)
{
	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 132);
	SCE_DBG_ASSERT((unsigned int)req->priority <= 15, 133);
	if (req->isCanceled == 1)
		return SCE_ERROR_ERRNO_ENOENT;
	if (req->isRunning == 1)
		return SCE_ERROR_ERRNO_EBUSY;
	if (queue->head == queue->tail) {
		SCE_DBG_ASSERT(queue->head != NULL, 148);
		SCE_DBG_ASSERT(queue->head == req, 149);
		SCE_DBG_ASSERT(req->next == NULL, 150);
		SCE_DBG_ASSERT(req->prev == NULL, 151);
		queue->head = NULL;
		queue->tail = NULL;
	} else if (req == queue->head) {
		SCE_DBG_ASSERT(req->next->prev == req, 155);
		req->next->prev = NULL;
		queue->head = req->next;
	} else if (req == queue->tail) {
		SCE_DBG_ASSERT(req->next == NULL, 159);
		SCE_DBG_ASSERT(req->prev->next == req, 160);
		req->prev->next = NULL;
		queue->tail = req->prev;
	} else {
		req->prev->next = req->next;
		req->next->prev = req->prev;
	}
	SCE_DBG_ASSERT(req != req->next, 169);
	return 0;
}

IoRequest *sceIoRequestQueueGetIoRequest(IoRequestQueue *queue, int *waiting)
{
	IoRequest *req, *running;
	SceUID thid;
	int fd, ret, priority, conflict;
	unsigned int type;

	SCE_DBG_ASSERT(queue != NULL, 249);
	req = queue->head;
	*waiting = 0;
	if (getIofilemgrTls()->isCountedWorker == 1) {
		thid = ksceKernelGetThreadId();
		while (req != NULL && req->thid != thid &&
				(req->isRunning != 1 ||
				 (int)req->doneChunkNum - (int)req->runningChunkNum <= 0) &&
				req->isWaiting != 1) {
			req = req->next;
			*waiting = 1;
		}
		if (req == NULL)
			return NULL;
		priority = req->priority;
		for (running = queue->sched->runningHead; running != NULL && running != req;
				running = running->waitNext) {
			if (running->priority > priority)
				continue;
			fd = req->args.fd.fd;
			if (fd <= 0) {
				if (strncmp(req->args.path.path, running->args.path.path, 0x400) == 0)
					return NULL;
			} else {
				type = running->type - 1;
				if (type <= 26 && (0x05857981U & (1U << type))) {
					ret = vfs_check_fd_vnode(running->args.path.path, fd);
					if (ret < 0)
						goto nextPriority;
					conflict = ret == 0;
				} else {
					conflict = running->args.fd.fd == fd;
				}
				if (conflict)
					return NULL;
nextPriority:
				priority = req->priority;
			}
		}
		if (priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1))
			return NULL;
		if (req->type == IO_REQUEST_WRITE || req->type == IO_REQUEST_PWRITE)
			return NULL;
	}
	if (req != NULL)
		SCE_DBG_ASSERT(req->isCanceled == 0, 285);
	return req;
}

int sceIoRequestQueueAssignIoChunksToIoRequest(IoRequestQueue *queue, IoRequest *req,
					     unsigned int maxChunks)
{
	IoChunk *chunk;
	SceSize chunkSize, size, remain, offset;
	unsigned int count;
	int dipsw;

	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 301);
	if (req->ioRemain == 0 || maxChunks == 0 || req->ioData == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (req->priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1) &&
			req->chunkAlignment == 1) {
		SCE_DBG_ASSERT(req->cacheFlags != 0, 317);
		SCE_DBG_ASSERT(req->chunkNum == 0, 318);
		chunkSize = req->cacheFlags;
	} else {
		switch (req->type) {
		case IO_REQUEST_READ:
		case IO_REQUEST_PREAD:
			chunkSize = queue->sched->readChunkSize;
			break;
		case IO_REQUEST_WRITE:
		case IO_REQUEST_PWRITE:
			chunkSize = queue->sched->writeChunkSize;
			break;
		default:
			chunkSize = 0;
			SCE_DBG_ASSERT(0, 333);
			break;
		}
	}
	count = req->chunkNum;
	if (count >= maxChunks)
		return count;
	remain = req->ioRemain;
	size = remain;
	while (size != 0) {
		if (chunkSize != 0) {
			if (!(req->priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1) &&
					req->chunkAlignment == 1)) {
				if (!(req->mntOpt & 0x10000) ||
						(req->type != IO_REQUEST_READ && req->type != IO_REQUEST_PREAD))
					goto wholeChunk;
				dipsw = ksceKernelCheckDipsw(231);
				remain = req->ioRemain;
				if (dipsw != 0)
					goto wholeChunk;
			}
			offset = req->chunkOffset;
			if (offset != 0) {
				size = chunkSize - offset;
				req->chunkOffset = 0;
				if (size >= remain)
					size = remain;
			} else {
wholeChunk:
				size = chunkSize < remain ? chunkSize : remain;
			}
		}
		chunk = sceIoChunkCreateIoChunk(&req->chunkPool, req,
				(SceUIntPtr)req->ioData, req->ioDone, size);
		if (chunk == NULL)
			break;
		if (req->chunkTail != NULL)
			req->chunkTail->next = chunk;
		else
			req->chunkHead = chunk;
		req->chunkTail = chunk;
		count++;
		req->ioDone += size;
		req->chunkNum++;
		size = req->ioRemain - size;
		remain = size;
		req->ioRemain = size;
		if (count >= maxChunks)
			break;
	}
	return count;
}

int sceIoRequestQueueChunkIoRequest(IoScheduler *sched, IoRequest *req, int canChunk)
{
	IoRequestQueue *queue = sched->queue[req->priority];
	int ret;

	if (req->ioRemain == 0 || canChunk == 0)
		return 0;
	if (req->isRunning == 1) {
		if (req->chunkNum != 0)
			return 0;
	} else if (req->chunkNum == IO_REQUEST_CHUNK_NUM) {
		return 0;
	}
	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 409);
	ret = sceIoRequestQueueAssignIoChunksToIoRequest(queue, req, IO_REQUEST_CHUNK_NUM);
	if (ret < 0)
		SCE_DBG_ASSERT(0, 415);
	return ret;
}

int sceIoRequestQueueFinishAndDestroyIoChunk(IoRequest *req, IoChunk *chunk, int result)
{
	IofilemgrLseekParam param;
	SceOff position;

	SCE_DBG_ASSERT(req != NULL, 425);
	switch (req->type) {
	case IO_REQUEST_READ:
		if (req->isFinished == 1)
			break;
		if (result < 0) {
			if (req->args.rw.result == 0)
				req->args.rw.result = result;
			req->isFinished = 1;
			break;
		}
		req->args.rw.result += result;
		if (chunk != NULL && chunk->offset <= req->ioDone) {
			param.fd = req->args.rw.fd;
			param.whence = SCE_SEEK_SET;
			param.offset = req->args.rw.offset + chunk->offset + result;
			if (req->readPosition < param.offset) {
				position = sceKernelExtendKernelStackWide(0x2000,
						(SceInt64 (*)(void *))vfsLseek, &param);
				if (position < 0) {
					req->args.rw.result = position;
					req->isFinished = 1;
				} else {
					req->readPosition = position;
				}
			}
		}
		break;
	case IO_REQUEST_WRITE:
		if (req->isFinished != 1) {
			if (result >= 0) {
				req->args.write.result += result;
			} else {
				if (req->args.write.result == 0)
					req->args.write.result = result;
				req->isFinished = 1;
			}
		}
		break;
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		if (req->isFinished != 1) {
			if (result >= 0) {
				req->args.rw.result += result;
			} else {
				if (req->args.rw.result == 0)
					req->args.rw.result = result;
				req->isFinished = 1;
			}
		}
		break;
	default:
		SCE_DBG_ASSERT(0, 544);
		break;
	}
	if (chunk != NULL) {
		SCE_DBG_ASSERT(req->runningChunkNum != 0, 548);
		req->runningChunkNum--;
		SCE_DBG_ASSERT(req->doneChunkNum != 0, 551);
		req->doneChunkNum--;
		sceIoChunkDestroyIoChunk(&req->chunkPool, chunk);
	}
	return 0;
}

int sceIoRequestQueueFinishIoRequest(IoScheduler *sched, IoRequest *req)
{
	IoRequestQueue *queue;

	SCE_DBG_ASSERT((unsigned int)req->priority <= 15, 565);
	queue = sched->queue[req->priority];
	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 569);
	SCE_DBG_ASSERT((unsigned int)req->priority <= 15, 570);
	if (queue->head == queue->tail) {
		SCE_DBG_ASSERT(req == queue->head, 573);
		SCE_DBG_ASSERT(req->next == NULL, 574);
		queue->head = req->next;
		queue->tail = req->next;
	} else if (req == queue->head) {
		req->next->prev = NULL;
		queue->head = req->next;
	} else if (req == queue->tail) {
		req->prev->next = NULL;
		queue->tail = req->prev;
	} else {
		req->prev->next = req->next;
		req->next->prev = req->prev;
	}
	SCE_DBG_ASSERT(req != req->next, 589);
	SCE_DBG_ASSERT(req->isRunning == 1, 591);
	req->isRunning = 0;
	req->isCanceled = 1;
	return 0;
}

static inline __attribute__((always_inline)) int readIoChunkByRead(IoRequest *req,
								 IoChunk *chunk)
{
	IofilemgrReadParam param;

	SCE_DBG_ASSERT_FUNC(req != NULL, 602, "readIoChunkByRead");
	SCE_DBG_ASSERT_FUNC(req->type == IO_REQUEST_READ, 603, "readIoChunkByRead");
	param.fd = req->args.rw.fd;
	param.data = chunk != NULL ? (void *)chunk->data : NULL;
	param.size = chunk != NULL ? chunk->size : 0;
	if (req->mntOpt & 0x10000) {
		SCE_DBG_ASSERT_FUNC(g_ioPfsReadEnabled != 0, 619, "readIoChunkByRead");
		return ksceKernelRunWithStack(0x2000, NULL, &param);
	}
	return ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsRead, &param);
}

int sceIoRequestQueueExecuteIoChunk(IoRequest *req, IoChunk *chunk)
{
	union {
		IofilemgrReadParam rw;
		IofilemgrPreadParam prw;
	} param;

	SCE_DBG_ASSERT(sceIoSchedulerIsDataIo(req) == 1, 709);
	SCE_DBG_ASSERT(req != NULL, 711);
	SCE_DBG_ASSERT(sceIoRequestIsValid(req), 712);
	switch (req->type) {
	case IO_REQUEST_READ:
		if (sceIoSchedulerIsStorageIo(req) == 1)
			return readIoChunkByPread(req, chunk);
		return readIoChunkByRead(req, chunk);
	case IO_REQUEST_WRITE:
		param.rw.fd = req->args.write.fd;
		param.rw.data = chunk != NULL ? (void *)chunk->data : NULL;
		param.rw.size = chunk != NULL ? chunk->size : 0;
		if (req->mntOpt & 0x10000) {
			SCE_DBG_ASSERT(g_ioPfsWriteEnabled != 0, 743);
			return ksceKernelRunWithStack(0x2000, NULL, &param.rw);
		}
		return ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsWrite, &param.rw);
	case IO_REQUEST_PREAD:
		return readIoChunkByPread(req, chunk);
	case IO_REQUEST_PWRITE:
		param.prw.fd = req->args.rw.fd;
		if (chunk != NULL) {
			param.prw.data = (void *)chunk->data;
			param.prw.size = chunk->size;
			param.prw.offset = req->args.rw.offset + chunk->offset;
		} else {
			param.prw.data = NULL;
			param.prw.size = 0;
			param.prw.offset = 0;
		}
		if (req->mntOpt & 0x10000) {
			SCE_DBG_ASSERT(g_ioPfsPwriteEnabled != 0, 789);
			return ksceKernelRunWithStack(0x2000, NULL, &param.prw);
		}
		return ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsPwrite, &param.prw);
	default:
		SCE_DBG_ASSERT(0, 811);
		return 0;
	}
}
