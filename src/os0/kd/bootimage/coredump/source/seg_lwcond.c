/* LWCONDVAR_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfSyncObj(int type, SceUID pid);
extern void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids,
	SceSize capacity);
extern void oalGetLwCondInfo(SceUID pid, SceUID lwCondId,
	CoredumpLwCondInfo *info, void **work, ThreadmgrLwMutexWork **lwMutexWork);
extern void oalGetWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *threads, SceSize capacity);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int oalGetNumOfWaitingThreads(SceUID pid, SceUID waitId);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod lwCondMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _calcSize(SegIf *segment)
{
	SceUID *ids;
	unsigned int count, i, numWaiting = 0;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 58, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfSyncObj(6, coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetSyncObjIdList(6, coredumpRun.pid, ids, count);
	i = 0;
	if (count > 8) {
		do {
			unsigned int firstWaiting, secondWaiting, thirdWaiting;
			unsigned int fourthWaiting, fifthWaiting, sixthWaiting;
			unsigned int seventhWaiting, eighthWaiting;

			__builtin_prefetch(&ids[i + 10]);
			firstWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i]);
			secondWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 1]);
			thirdWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 2]);
			fourthWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 3]);
			fifthWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 4]);
			sixthWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 5]);
			seventhWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 6]);
			eighthWaiting = oalGetNumOfWaitingThreads(coredumpRun.pid,
				ids[i + 7]);
			numWaiting += firstWaiting + secondWaiting + thirdWaiting +
				fourthWaiting + fifthWaiting + sixthWaiting +
				seventhWaiting + eighthWaiting;
			i += 8;
		} while (count - 7 > i + 1);
	}
	do {
		numWaiting += oalGetNumOfWaitingThreads(coredumpRun.pid, ids[i++]);
	} while (count > i);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (numWaiting + count *
		(sizeof(CoredumpLwCondInfo) / sizeof(CoredumpWaitingThread))) *
		sizeof(CoredumpWaitingThread) + 76;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpLwCondInfo info;
	void *work;
	ThreadmgrLwMutexWork *lwMutexWork;
	CoredumpWaitingThread *threads = NULL;
	SceUID *ids = NULL;
	unsigned int i, j, numWaiting, totalWaiting;
	SceSize writtenSize;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 105, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfSyncObj(6, coredumpRun.pid);
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
	utilsMakeNoteHeader(&header, "LWCONDVAR_INFO", 0x1016, 6,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetSyncObjIdList(6, coredumpRun.pid, ids, count);
	totalWaiting = 0;
	for (i = 0; i < count; ++i) {
		oalGetLwCondInfo(coredumpRun.pid, ids[i], &info, &work, &lwMutexWork);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		numWaiting = (unsigned int)info.numWaitThreads;
		if (numWaiting >= 320)
			numWaiting = 320;
		segment->writtenSize += sizeof(info);
		oalGetWaitingThreadList(coredumpRun.pid, ids[i], threads,
			numWaiting);
		for (j = 0; j != numWaiting; ++j) {
			result = writerWrite(currentWriter, &threads[j],
				sizeof(*threads));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(*threads);
		}
		totalWaiting += numWaiting;
		result = writerWrite(currentWriter, &work, sizeof(work));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(work);
		result = writerWrite(currentWriter, &lwMutexWork, sizeof(lwMutexWork));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(lwMutexWork);
	}
	writtenSize = 36 + count * (sizeof(info) + sizeof(work) + sizeof(lwMutexWork)) +
		totalWaiting * sizeof(*threads);
	result = segWritePaddingData(segment, writtenSize);
	segment->writtenSize += segment->dataSize - writtenSize;
out:
	if (threads != NULL)
		memMgrLocalFree(threads);
	if (ids != NULL)
		memMgrLocalFree(ids);
	return result;
}
