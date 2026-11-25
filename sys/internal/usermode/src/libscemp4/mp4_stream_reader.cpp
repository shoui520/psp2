#include "mp4_internal.h"

static Mp4ListNode<Mp4StreamChunk *> *mp4ChunkListGetAt(Mp4ListNode<Mp4StreamChunk *> *node, uint32_t curIdx, uint32_t targetIdx)
{
	while (curIdx != targetIdx) {
		if (curIdx > targetIdx) {
			node = node->prev;
			curIdx--;
		} else {
			node = node->next;
			curIdx++;
		}
	}
	return node;
}

static Mp4ListNode<Mp4StreamChunk *> *mp4ChunkListFindNode(Mp4ListNode<Mp4StreamChunk *> *node, Mp4StreamChunk **valPtr)
{
	if (&node->value == valPtr)
		return node;
	Mp4ListNode<Mp4StreamChunk *> *found = mp4ChunkListFindNode(node->next, valPtr);
	if (found == node->next)
		node->next = NULL;
	return found;
}

static Mp4ListNode<Mp4StreamChunk *> *mp4ChunkListFindPrev(Mp4ListNode<Mp4StreamChunk *> *node, Mp4ListNode<Mp4StreamChunk *> *target)
{
	if (node == target)
		return node->prev;
	return mp4ChunkListFindPrev(node->next, target);
}

static Mp4StreamChunk *mp4ChunkListFront(const Mp4List<Mp4StreamChunk *> *list)
{
	if (list->count == 0)
		return NULL;
	if ((list->count >> 1) != 0)
		return mp4ChunkListGetAt(list->head, 0, 0)->value;
	return mp4ChunkListGetAt(list->tail, list->count - 1, 0)->value;
}

static void mp4ChunkListPushBack(Mp4List<Mp4StreamChunk *> *list, Mp4StreamChunk *chunk)
{
	sceKernelLockLwMutex(&list->mutex, 1, NULL);
	Mp4ListNode<Mp4StreamChunk *> *node = (Mp4ListNode<Mp4StreamChunk *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4StreamChunk *>));
	if (node != NULL) {
		node->prev = NULL;
		node->next = NULL;
		sceClibMemcpy_safe(&node->value, &chunk, sizeof(chunk));
	}
	if (list->tail != NULL) {
		list->tail->next = node;
		node->prev = list->tail;
	} else {
		list->head = node;
	}
	list->tail = node;
	list->count++;
	list->totalBytes += sizeof(Mp4ListNode<Mp4StreamChunk *>);
	sceKernelUnlockLwMutex(&list->mutex, 1);
}

static void mp4SignalReaderCond(Mp4FileStreamReader *reader)
{
	if (reader->mutexId != -1 && reader->condId != -1) {
		if (sceKernelLockMutex(reader->mutexId, 1, NULL) >= 0) {
			if (sceKernelSignalCond(reader->condId) >= 0)
				sceKernelUnlockMutex(reader->mutexId, 1);
		}
	}
}

static inline __attribute__((always_inline)) bool mp4ContainsSubstring(const char *haystack, const char *needle)
{
	if (haystack == NULL || haystack[0] == '\0')
		return false;

	SceSize hayLen = sceClibStrnlen(haystack, 1024);
	SceSize needleLen = sceClibStrnlen(needle, 1024);
	if (hayLen < needleLen)
		return false;

	SceSize limit = hayLen - needleLen;
	for (SceSize i = 0; i < limit; i++) {
		if (haystack[i] != needle[0])
			continue;
		bool matched = true;
		for (SceSize j = 0; j < needleLen; j++) {
			if (haystack[i + j] != needle[j]) {
				matched = false;
				break;
			}
		}
		if (matched)
			return true;
	}
	return false;
}

struct Mp4ThreadStartArg {
	void (*entry)(Mp4FileStreamReader *);
	Mp4FileStreamReader *reader;
};

static int mp4StreamingThreadTrampoline(SceSize argSize, void *argp)
{
	(void)argSize;
	Mp4ThreadStartArg *arg = (Mp4ThreadStartArg *)argp;
	arg->entry(arg->reader);
	return 0;
}

static void mp4StreamingThreadEntry(Mp4FileStreamReader *reader)
{
	reader->streamingLoop();
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
}

Mp4FileStreamReader::Mp4FileStreamReader(int32_t inFlags)
{
	SceKernelLwMutexOptParam opt;
	Mp4StreamChunk *nullChunk = NULL;

	this->chunkQueue.head = NULL;
	this->chunkQueue.tail = NULL;
	this->chunkQueue.count = 0;
	this->chunkQueue.totalBytes = 0;
	this->chunkQueue.isShallowCopy = 0;
	this->chunkQueue.beginIter = NULL;
	this->chunkQueue.endIter = NULL;

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->chunkQueue.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	Mp4ListNode<Mp4StreamChunk *> *sentinel = (Mp4ListNode<Mp4StreamChunk *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4StreamChunk *>));
	if (sentinel != NULL) {
		sentinel->prev = NULL;
		sentinel->next = NULL;
		sceClibMemcpy_safe(&sentinel->value, &nullChunk, sizeof(nullChunk));
	}
	this->chunkQueue.sentinel = sentinel;

	this->shuttingDown = 0;
	this->shutdownPad = 0;
	this->chunkMspace = NULL;
	this->condId = -1;
	this->mutexId = -1;
	this->readCursor = 0;
	this->cachedStartOffset = 0;
	this->cachedEndOffset = 0;
	this->fileSize = 0;
	this->mspaceBacking = NULL;
	this->threadRunning = 0;
	this->bypassCache = 0;
	this->cacheDisabled = 0;
	this->readErrorFlag = 0;
	this->stalledFlag = 0;
	this->threadId = 0;
	this->fileOps = NULL;
	this->streamFlags = inFlags;
	this->state = 0;
	this->activeFlag = 0;
}

inline __attribute__((always_inline)) int Mp4FileStreamReader::releaseChunk(Mp4StreamChunk *chunk)
{
	if (this->mutexId == -1 || this->condId == -1)
		return 0;

	if (sceKernelLockLwMutex(&this->queueMutex, 1, NULL) < 0)
		return -1;

	if (this->chunkMspace != NULL)
		sceClibMspaceFree(this->chunkMspace, chunk->buffer);

	sceKernelLockLwMutex(&this->chunkQueue.mutex, 1, NULL);
	if (this->chunkQueue.count != 0 && this->chunkQueue.head != NULL) {
		Mp4StreamChunk **frontVal = &mp4ChunkListGetAt(this->chunkQueue.head, 0, 0)->value;
		Mp4ListNode<Mp4StreamChunk *> *node = mp4ChunkListFindNode(this->chunkQueue.head, frontVal);
		if (node != NULL) {
			Mp4ListNode<Mp4StreamChunk *> *next = node->next;
			Mp4ListNode<Mp4StreamChunk *> *prev = mp4ChunkListFindPrev(node, node);
			if (node == this->chunkQueue.head) {
				this->chunkQueue.head = next;
				if (next != NULL)
					next->prev = NULL;
				if (this->chunkQueue.count == 2)
					this->chunkQueue.tail = next;
			} else {
				if (prev != NULL)
					prev->next = next;
				if (next != NULL)
					next->prev = prev;
			}
			if (next == NULL)
				this->chunkQueue.tail = prev;
			mp4MspaceFree(node);
			this->chunkQueue.count--;
			this->chunkQueue.totalBytes -= sizeof(Mp4ListNode<Mp4StreamChunk *>);
		}
	}
	sceKernelUnlockLwMutex(&this->chunkQueue.mutex, 1);

	if (sceKernelUnlockLwMutex(&this->queueMutex, 1) < 0)
		return -1;

	if (sceKernelLockMutex(this->mutexId, 1, NULL) < 0)
		return -1;
	if (sceKernelSignalCond(this->condId) < 0)
		return -1;
	if (sceKernelUnlockMutex(this->mutexId, 1) < 0)
		return -1;
	return 0;
}

Mp4FileStreamReader::~Mp4FileStreamReader()
{
	while (true) {
		int32_t remaining = (int32_t)this->chunkQueue.count;
		if (remaining <= 0) {
			mp4SignalReaderCond(this);
			break;
		}
		Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
		this->releaseChunk(front);
		mp4MspaceFree(front);
	}

	sceKernelDeleteLwMutex(&this->ioMutex);
	mp4AtomicStore32(&this->shuttingDown, 1);

	if (this->mutexId != -1 && this->condId != -1 &&
	    sceKernelLockMutex(this->mutexId, 1, NULL) >= 0 &&
	    sceKernelSignalCond(this->condId) >= 0 &&
	    sceKernelUnlockMutex(this->mutexId, 1) >= 0) {
		while (this->chunkQueue.count != 0) {
			Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
			if (front != NULL)
				this->releaseChunk(front);
			else
				break;
		}

		if (sceKernelDeleteLwMutex(&this->queueMutex) >= 0 &&
		    sceKernelDeleteCond(this->condId) >= 0 &&
		    sceKernelDeleteMutex(this->mutexId) >= 0) {
			if (this->chunkMspace != NULL)
				sceClibMspaceDestroy(this->chunkMspace);
		}
	}

	if (this->chunkQueue.isShallowCopy == 0) {
		if (this->chunkQueue.beginIter != NULL)
			delete this->chunkQueue.beginIter;
		if (this->chunkQueue.endIter != NULL)
			delete this->chunkQueue.endIter;
		if (this->chunkQueue.sentinel != NULL)
			mp4MspaceFree(this->chunkQueue.sentinel);
	}
	sceKernelDeleteLwMutex(&this->chunkQueue.mutex);
}

int Mp4FileStreamReader::seekTo(int64_t offset)
{
	if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
		return -1;

	this->readCursor = (uint64_t)(offset & ~63LL);
	while (true) {
		int32_t remaining = (int32_t)this->chunkQueue.count;
		if (remaining <= 0) {
			mp4SignalReaderCond(this);
			break;
		}
		Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
		if (front == NULL)
			break;
		this->releaseChunk(front);
		mp4MspaceFree(front);
	}

	if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
		return -1;
	return 0;
}

int Mp4FileStreamReader::suspendStreaming()
{
	if (this->bypassCache != 0)
		return (int)this->bypassCache;

	mp4AtomicStore32(&this->bypassCache, 1);
	int res = sceKernelLockLwMutex(&this->ioMutex, 1, NULL);
	if (res < 0)
		return res;

	while (true) {
		int32_t remaining = (int32_t)this->chunkQueue.count;
		if (remaining <= 0) {
			mp4SignalReaderCond(this);
			break;
		}
		Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
		if (front == NULL)
			break;
		this->releaseChunk(front);
		mp4MspaceFree(front);
	}

	return sceKernelUnlockLwMutex(&this->ioMutex, 1);
}

int Mp4FileStreamReader::resumeStreaming()
{
	if (this->bypassCache != 0)
		mp4AtomicStore32(&this->bypassCache, 0);
	return 0;
}

int Mp4FileStreamReader::closeFile()
{
	return this->fileOps->close(this->fileOps->objectPointer);
}

int Mp4FileStreamReader::stopAndClose()
{
	if (this->threadRunning == 0)
		return 0;

	mp4AtomicStore32(&this->activeFlag, 0);
	mp4AtomicStore32(&this->threadRunning, 0);

	while (true) {
		int32_t remaining = (int32_t)this->chunkQueue.count;
		if (remaining <= 0) {
			mp4SignalReaderCond(this);
			break;
		}
		sceKernelLockLwMutex(&this->ioMutex, 1, NULL);
		Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
		this->releaseChunk(front);
		mp4MspaceFree(front);
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
	}

	if (this->threadId >= 0) {
		if (sceKernelWaitThreadEnd(this->threadId, NULL, NULL) >= 0)
			sceKernelDeleteThread(this->threadId);
	}
	return this->closeFile();
}

int64_t Mp4FileStreamReader::queryFileSize()
{
	if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
		return 0;

	int64_t sz = (int64_t)this->fileOps->size(this->fileOps->objectPointer);
	if (sz < 0x80000)
		mp4AtomicStore32(&this->bypassCache, 1);

	if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
		return 0;
	return sz;
}

void Mp4FileStreamReader::streamingLoop()
{
	SceKernelLwMutexOptParam opt;

	mp4AtomicStore32(&this->threadRunning, 1);
	mp4AtomicStore32(&this->activeFlag, 1);

	if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
		return;

	if (this->bypassCache == 0) {
		this->mspaceBacking = (uint8_t *)mp4MspaceAllocZero(1, 0x30000);
		if (this->mspaceBacking == NULL) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			mp4AtomicStore32(&this->activeFlag, 0);
			return;
		}
	}

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if (sceKernelCreateLwMutex(&this->queueMutex, "filebuffer", 0, 0, &opt) < 0) {
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
		return;
	}

	this->mutexId = sceKernelCreateMutex("filebuffer", 0x2000, 0, NULL);
	if (this->mutexId < 0) {
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
		return;
	}

	this->condId = sceKernelCreateCond("filebuffer", 0x2000, this->mutexId, NULL);
	if (this->condId < 0) {
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
		return;
	}

	if (this->mspaceBacking != NULL) {
		this->chunkMspace = sceClibMspaceCreate(this->mspaceBacking, 0x30000);
		if (this->chunkMspace == NULL) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			return;
		}
	}

	this->cachedStartOffset = 0;
	this->cachedEndOffset = 0;
	if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
		return;

	int64_t totalSize = this->queryFileSize();
	while (totalSize == 0 && this->threadRunning == 1) {
		sceKernelDelayThread(90);
		totalSize = this->queryFileSize();
	}
	this->fileSize = (uint64_t)totalSize;

	while (this->threadRunning == 1) {
		if (this->bypassCache != 0 || this->cacheDisabled != 0) {
			sceKernelDelayThread(900);
			continue;
		}

		Mp4StreamChunk *chunk = (Mp4StreamChunk *)mp4MspaceAllocZero(1, sizeof(Mp4StreamChunk));
		if (chunk == NULL)
			continue;

		uint8_t *buf = NULL;
		if (this->mutexId != -1 && this->condId != -1 && this->shuttingDown != 1) {
			buf = (uint8_t *)sceClibMspaceMalloc(this->chunkMspace, 0x10000);
			if (this->shuttingDown == 1) {
				buf = NULL;
			} else if (buf == NULL) {
				if (sceKernelLockMutex(this->mutexId, 1, NULL) >= 0) {
					if (sceKernelWaitCond(this->condId, NULL) >= 0)
						sceKernelUnlockMutex(this->mutexId, 1);
				}
			}
		}

		chunk->buffer = buf;
		if (this->threadRunning != 1 || buf == NULL) {
			mp4MspaceFree(chunk);
			sceKernelDelayThread(90);
			continue;
		}

		if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
			break;
		if (this->threadRunning != 1) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			break;
		}

		uint64_t cursor = this->readCursor;
		chunk->fileOffset = cursor;
		chunk->reserved = 0;

		int32_t toRead = 0x10000;
		if ((int64_t)(cursor + 0x10000) > totalSize)
			toRead = (int32_t)(totalSize - (int64_t)cursor);

		int32_t got = 0;
		if (toRead > 0) {
			uint64_t zeroRetries = 0;
			int errRetries = 0;
			while (true) {
				int rd = this->fileOps->readOffset(this->fileOps->objectPointer, chunk->buffer, this->readCursor, (uint32_t)toRead);
				if (rd == 0) {
					sceKernelUnlockLwMutex(&this->ioMutex, 1);
					sceKernelDelayThread(100);
					if (zeroRetries > 100)
						mp4AtomicStore32(&this->stalledFlag, 1);
					zeroRetries++;
					if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
						break;
				} else if (rd > 0) {
					got = rd;
					break;
				} else {
					errRetries++;
					sceKernelDelayThread(200);
					got = 0;
				}
				if (errRetries > 0 || this->threadRunning != 1 || this->activeFlag == 0)
					break;
			}
		}

		if (got == toRead) {
			if (this->mutexId != -1 && this->condId != -1) {
				sceKernelLockLwMutex(&this->queueMutex, 1, NULL);
				mp4ChunkListPushBack(&this->chunkQueue, chunk);
				sceKernelUnlockLwMutex(&this->queueMutex, 1);
			}
			mp4AtomicStore32(&this->stalledFlag, 0);
			chunk->validSize = (uint32_t)got;
			mp4AtomicAdd64(&this->readCursor, (uint64_t)got);
			mp4AtomicStore64(&this->cachedEndOffset, this->readCursor);
			if ((int64_t)this->readCursor == totalSize)
				mp4AtomicStore64(&this->readCursor, 0);
		} else {
			mp4AtomicStore32(&this->readErrorFlag, 1);
			if (got <= 0) {
				if (this->mutexId != -1 && this->condId != -1) {
					sceKernelLockLwMutex(&this->queueMutex, 1, NULL);
					mp4ChunkListPushBack(&this->chunkQueue, chunk);
					sceKernelUnlockLwMutex(&this->queueMutex, 1);
				}
				this->releaseChunk(chunk);
				mp4MspaceFree(chunk);
			} else {
				if (this->mutexId != -1 && this->condId != -1) {
					sceKernelLockLwMutex(&this->queueMutex, 1, NULL);
					mp4ChunkListPushBack(&this->chunkQueue, chunk);
					sceKernelUnlockLwMutex(&this->queueMutex, 1);
				}
				chunk->validSize = (uint32_t)got;
				mp4AtomicAdd64(&this->readCursor, (uint64_t)got);
				mp4AtomicStore64(&this->cachedEndOffset, this->readCursor);
			}
		}

		if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
			break;
	}

	mp4MspaceFree(this->mspaceBacking);
}

int Mp4FileStreamReader::openAndStart(const char *filename, int32_t unused, SceAvPlayerFileReplacement *inFileOps, int32_t basePriority)
{
	(void)unused;
	SceKernelLwMutexOptParam opt;

	this->fileOps = inFileOps;
	mp4AtomicStore32(&this->bypassCache, 0);

	if (mp4ContainsSubstring(filename, "http://") || mp4ContainsSubstring(filename, "https://"))
		mp4AtomicStore32(&this->bypassCache, 1);

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if (sceKernelCreateLwMutex(&this->ioMutex, "filereadwrite", 0, 0, &opt) < 0)
		return MP4_ERR_GENERIC;

	if (this->fileOps->open(this->fileOps->objectPointer, filename) < 0)
		return MP4_ERR_OPEN_FAILED;

	this->state = 1;
	SceUID thid = sceKernelCreateThread("avPlayer FileStreaming", mp4StreamingThreadTrampoline, basePriority + 15, 0x4000, 0, 0, NULL);
	if (thid < 0)
		return MP4_ERR_THREAD_FAILED;

	this->threadId = thid;
	Mp4ThreadStartArg startArg;
	startArg.entry = mp4StreamingThreadEntry;
	startArg.reader = this;
	if (sceKernelStartThread(thid, sizeof(startArg), &startArg) < 0)
		return MP4_ERR_THREAD_FAILED;

	this->state = 2;
	return MP4_OK;
}

int Mp4FileStreamReader::readAt(uint8_t *dst, int64_t offset, SceSize length, uint32_t *outRead)
{
	if (this->threadRunning != 1)
		return MP4_ERR_BUFFER_UNDERRUN;
	if (this->readErrorFlag == 1 || this->state != 2)
		return MP4_ERR_EOF;
	if (this->fileSize != 0 && (uint64_t)offset + length > this->fileSize)
		return MP4_ERR_EOF;

	if (length >= 0x10000 || this->bypassCache != 0 || this->cacheDisabled != 0) {
		if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
			return MP4_ERR_GENERIC;

		int zeroRetries = 0;
		while (true) {
			int rd = this->fileOps->readOffset(this->fileOps->objectPointer, dst, (uint64_t)offset, length);
			if (rd < 0) {
				if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
					return MP4_ERR_GENERIC;
				return MP4_ERR_EOF;
			}
			if (rd == 0) {
				sceKernelDelayThread(100);
				if (zeroRetries > 500) {
					if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
						return MP4_ERR_GENERIC;
					return MP4_ERR_BUFFER_UNDERRUN;
				}
				zeroRetries++;
			}
			if (rd != 0 || this->threadRunning != 1 || this->activeFlag == 0) {
				if (rd > 0)
					*outRead = (uint32_t)rd;
				if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
					return MP4_ERR_GENERIC;
				return MP4_OK;
			}
		}
	}

	while ((int32_t)this->chunkQueue.count <= 0) {
		mp4SignalReaderCond(this);
		sceKernelDelayThread(90);
		if (this->threadRunning != 1 || this->activeFlag != 1)
			return MP4_ERR_STREAM_STOPPED;
	}

	while (sceKernelTryLockLwMutex(&this->ioMutex, 1) < 0) {
		if (this->threadRunning != 1)
			return MP4_ERR_STREAM_STOPPED;
		if (this->stalledFlag == 1)
			return MP4_ERR_BUFFER_UNDERRUN;
		sceKernelDelayThread(90);
	}

	Mp4StreamChunk *front = mp4ChunkListFront(&this->chunkQueue);
	if (front == NULL) {
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
		return MP4_ERR_STREAM_STOPPED;
	}
	mp4AtomicStore64(&this->cachedStartOffset, front->fileOffset);
	if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
		return MP4_ERR_GENERIC;

	if ((uint64_t)offset < this->cachedStartOffset || (uint64_t)offset > this->cachedEndOffset) {
		if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
			return MP4_ERR_GENERIC;
		if (this->threadRunning != 1) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			return MP4_ERR_STREAM_STOPPED;
		}
		mp4AtomicStore64(&this->readCursor, (uint64_t)offset);
		mp4AtomicStore64(&this->cachedEndOffset, this->readCursor);

		while (true) {
			int32_t remaining = (int32_t)this->chunkQueue.count;
			if (remaining <= 0) {
				mp4SignalReaderCond(this);
				break;
			}
			Mp4StreamChunk *drop = mp4ChunkListFront(&this->chunkQueue);
			this->releaseChunk(drop);
			mp4MspaceFree(drop);
		}
		if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
			return MP4_ERR_GENERIC;

		while ((int32_t)this->chunkQueue.count <= 0) {
			mp4SignalReaderCond(this);
			sceKernelDelayThread(90);
			if (this->threadRunning != 1)
				return MP4_ERR_STREAM_STOPPED;
		}
		if (this->threadRunning != 1)
			return MP4_ERR_STREAM_STOPPED;
		if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
			return MP4_ERR_GENERIC;
		front = mp4ChunkListFront(&this->chunkQueue);
		if (this->threadRunning != 1 || front == NULL) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			return MP4_ERR_STREAM_STOPPED;
		}
		mp4AtomicStore64(&this->cachedStartOffset, front->fileOffset);
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
	}

	while (sceKernelTryLockLwMutex(&this->ioMutex, 1) < 0) {
		if (this->threadRunning != 1)
			return MP4_ERR_STREAM_STOPPED;
		if (this->stalledFlag == 1)
			return MP4_ERR_BUFFER_UNDERRUN;
		sceKernelDelayThread(90);
	}

	if (this->threadRunning != 1) {
		sceKernelUnlockLwMutex(&this->ioMutex, 1);
		return MP4_ERR_STREAM_STOPPED;
	}

	uint32_t relOff = (uint32_t)((uint64_t)offset - front->fileOffset);
	if ((uint64_t)offset + length <= front->fileOffset + front->validSize) {
		if (relOff > 0x10000) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			mp4AtomicStore32(&this->bypassCache, 1);
			return MP4_ERR_BUFFER_UNDERRUN;
		}
		sceClibMemcpy_safe(dst, front->buffer + relOff, length);
	} else {
		SceSize copied = 0;
		while ((uint64_t)offset + length > front->fileOffset + front->validSize) {
			int32_t avail = (int32_t)front->validSize - (int32_t)relOff;
			if (avail > 0) {
				if (front->buffer == NULL) {
					sceKernelUnlockLwMutex(&this->ioMutex, 1);
					return MP4_ERR_STREAM_STOPPED;
				}
				sceClibMemcpy_safe(dst + copied, front->buffer + relOff, (SceSize)avail);
				copied += (SceSize)avail;
				relOff = 0;
			} else {
				relOff = (uint32_t)(-avail);
			}

			this->releaseChunk(front);
			mp4MspaceFree(front);

			if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
				return MP4_ERR_GENERIC;

			while ((int32_t)this->chunkQueue.count <= 0) {
				mp4SignalReaderCond(this);
				sceKernelDelayThread(900);
				if (this->threadRunning != 1 || this->activeFlag == 0)
					return MP4_ERR_STREAM_STOPPED;
				if (this->stalledFlag == 1)
					return MP4_ERR_BUFFER_UNDERRUN;
			}

			if (sceKernelLockLwMutex(&this->ioMutex, 1, NULL) < 0)
				return MP4_ERR_GENERIC;
			if (this->threadRunning != 1) {
				sceKernelUnlockLwMutex(&this->ioMutex, 1);
				return MP4_ERR_STREAM_STOPPED;
			}
			if ((int32_t)this->chunkQueue.count <= 0)
				mp4SignalReaderCond(this);
			front = mp4ChunkListFront(&this->chunkQueue);
			if (front == NULL) {
				sceKernelUnlockLwMutex(&this->ioMutex, 1);
				return MP4_ERR_STREAM_STOPPED;
			}
			mp4AtomicStore64(&this->cachedStartOffset, front->fileOffset);
		}

		if (relOff > 0x10000) {
			sceKernelUnlockLwMutex(&this->ioMutex, 1);
			return MP4_ERR_BUFFER_UNDERRUN;
		}
		sceClibMemcpy_safe(dst + copied, front->buffer + relOff, length - copied);
	}

	*outRead = length;
	if (sceKernelUnlockLwMutex(&this->ioMutex, 1) < 0)
		return MP4_ERR_STREAM_STOPPED;
	return MP4_OK;
}

int Mp4FileStreamReader::configureCacheLimit(int64_t interleaveLimit)
{
	char titleId[32];
	if (sceKernelGetProcessTitleId(titleId, 31) >= 0) {
		if (sceClibStrcmp(titleId, "PCSC00002") == 0 ||
		    sceClibStrcmp(titleId, "PCSD00003") == 0 ||
		    sceClibStrcmp(titleId, "PCSD00035") == 0 ||
		    sceClibStrcmp(titleId, "PCSF00024") == 0 ||
		    sceClibStrcmp(titleId, "PCSA00011") == 0 ||
		    sceClibStrcmp(titleId, "PCSC90001") == 0 ||
		    sceClibStrcmp(titleId, "PCSD00002") == 0 ||
		    sceClibStrcmp(titleId, "PCSA00013") == 0 ||
		    sceClibStrcmp(titleId, "PCSA00032") == 0 ||
		    sceClibStrcmp(titleId, "PCSF00067") == 0) {
			return 0;
		}
	}

	if (interleaveLimit < 0) {
		mp4AtomicStore32(&this->cacheDisabled, 1);
		return 1;
	}
	if (interleaveLimit <= 1966080) {
		mp4AtomicStore32(&this->cacheDisabled, 0);
		return 0;
	}

	mp4AtomicStore32(&this->cacheDisabled, 1);
	this->suspendStreaming();
	return 1;
}

Mp4FileStreamDataReader::~Mp4FileStreamDataReader()
{
	if (this->streamReader != NULL) {
		this->streamReader->stopAndClose();
		this->streamReader->~Mp4FileStreamReader();
		mp4MspaceFree(this->streamReader);
		this->streamReader = NULL;
	}
}

int64_t Mp4FileStreamDataReader::getLength()
{
	if (this->streamReader == NULL)
		return 0;
	return this->streamReader->queryFileSize();
}

int Mp4FileStreamDataReader::read(int64_t offset, uint32_t length, uint8_t *dst)
{
	if (this->streamReader == NULL)
		return MP4_ERR_NO_MEMORY;
	uint32_t bytesRead = 0;
	int err = this->streamReader->readAt(dst, offset, length, &bytesRead);
	if (err == MP4_OK && bytesRead != length)
		return MP4_ERR_EOF;
	return err;
}

Mp4DataReader::~Mp4DataReader()
{
}

int Mp4DataReader::read(int64_t offset, uint32_t length, uint8_t *dst)
{
	(void)offset;
	(void)length;
	(void)dst;
	return MP4_ERR_GENERIC;
}

int64_t Mp4DataReader::getLength()
{
	return 0;
}

int Mp4DataReader::isStreaming()
{
	return 0;
}

int Mp4DataReader::getBufferSize()
{
	return 0;
}

void Mp4DataReader::reset()
{
}
