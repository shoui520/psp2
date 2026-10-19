/* LWCONDVAR_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod lwCondMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf lwCondSegment;

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
	for (i = 0; i < count; i++)
		numWaiting += oalGetNumOfWaitingThreads(coredumpRun.pid, ids[i]);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (numWaiting + count *
		((sizeof(CoredumpLwCondInfo) + 2 * sizeof(void *)) /
		 sizeof(CoredumpWaitingThread))) *
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
		numWaiting = numWaiting < 320 ? numWaiting : 320;
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
