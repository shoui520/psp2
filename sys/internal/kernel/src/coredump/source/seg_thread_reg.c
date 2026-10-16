/* THREAD_REG_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfThreads(SceUID pid);
extern void oalGetThreadIdList(SceUID pid, SceUID *ids, unsigned int capacity);
extern int oalGetThreadRegInfo(SceUID pid, SceUID threadId,
	CoredumpThreadRegInfo *info);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod threadRegMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf threadRegSegment;

static int _calcSize(SegIf *segment)
{
	unsigned int count;
	SceUInt32 numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfThreads(coredumpRun.pid);
	numPhdr = count;
	dataSize = count;
	if (count != 0) {
		dataSize = count * sizeof(CoredumpThreadRegInfo) +
			sizeof(CoredumpNoteHeader) + sizeof(count);
		numPhdr = 1;
	}
	segment->dataSize = dataSize;
	segment->numPhdr = numPhdr;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpThreadRegInfo info;
	SceUID *threadIds;
	unsigned int i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 79, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfThreads(coredumpRun.pid);
	threadIds = memMgrLocalAlloc(count * sizeof(*threadIds));
	if (threadIds == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "THREAD_REG_INFO", 0x1004, 17,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetThreadIdList(coredumpRun.pid, threadIds, count);
	for (i = 0; i < count; ++i) {
		oalGetThreadRegInfo(coredumpRun.pid, threadIds[i], &info);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
	}
out:
	memMgrLocalFree(threadIds);
	return result;
}
