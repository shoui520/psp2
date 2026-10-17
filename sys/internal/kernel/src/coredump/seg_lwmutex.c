/* LWMUTEX_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfSyncObj(int type, SceUID pid);
extern void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids,
	SceSize capacity);
extern void oalGetLwMutexInfo(SceUID pid, SceUID lwMutexId,
	CoredumpLwMutexInfo *info);
extern void oalGetWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *threads, SceSize capacity);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int oalGetNumOfWaitingThreads(SceUID pid, SceUID waitId);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod lwMutexMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf lwMutexSegment;

static int _calcSize(SegIf *segment)
{
	SceUID *ids;
	unsigned int count, i, numWaiting = 0;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfSyncObj(4, coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetSyncObjIdList(4, coredumpRun.pid, ids, count);
	for (i = 0; i < count; i++)
		numWaiting += oalGetNumOfWaitingThreads(coredumpRun.pid, ids[i]);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (numWaiting + count *
		(sizeof(CoredumpLwMutexInfo) / sizeof(SceUID)) + 19) *
		sizeof(SceUID);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpLwMutexInfo info;
	CoredumpWaitingThread *threads = NULL;
	SceUID *ids = NULL;
	unsigned int i, j, numWaiting, totalWaiting;
	SceSize writtenSize;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 102, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfSyncObj(4, coredumpRun.pid);
	threads = memMgrLocalAlloc(320 * sizeof(*threads));
	if (threads == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	utilsMakeNoteHeader(&header, "LWMUTEX_INFO", 0x100d, 1,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetSyncObjIdList(4, coredumpRun.pid, ids, count);
	totalWaiting = 0;
	for (i = 0; i < count; ++i) {
		oalGetLwMutexInfo(coredumpRun.pid, ids[i], &info);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		numWaiting = (unsigned int)info.numWaitThreads;
		numWaiting = numWaiting < 320 ? numWaiting : 320;
		segment->writtenSize += sizeof(info);
		oalGetWaitingThreadList(coredumpRun.pid, ids[i], threads,
			numWaiting);
		for (j = 0; j != numWaiting; ++j) {
			result = writerWrite(currentWriter, &threads[j].userThreadId,
				sizeof(threads[j].userThreadId));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(threads[j].userThreadId);
		}
		totalWaiting += numWaiting;
	}
	writtenSize = 36 + count * sizeof(info) + totalWaiting * sizeof(SceUID);
	result = segWritePaddingData(segment, writtenSize);
	segment->writtenSize += segment->dataSize - writtenSize;
out:
	if (threads != NULL)
		memMgrLocalFree(threads);
	if (ids != NULL)
		memMgrLocalFree(ids);
	return result;
}
