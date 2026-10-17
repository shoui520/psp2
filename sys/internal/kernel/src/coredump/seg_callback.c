/* CALLBACK_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfSyncObj(int type, SceUID pid);
extern void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids,
	SceSize capacity);
extern void oalGetCallbackInfo(SceUID pid, SceUID callbackId,
	CoredumpCallbackInfo *info);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod callbackMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf callbackSegment;

static int _calcSize(SegIf *segment)
{
	unsigned int count, numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfSyncObj(8, coredumpRun.pid);
	numPhdr = count;
	dataSize = count;
	if (count != 0) {
		dataSize = count * sizeof(CoredumpCallbackInfo) + 36;
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
	CoredumpCallbackInfo info;
	SceUID *ids;
	unsigned int i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 80, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfSyncObj(8, coredumpRun.pid);
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	utilsMakeNoteHeader(&header, "CALLBACK_INFO", 0x1011, 1,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetSyncObjIdList(8, coredumpRun.pid, ids, count);
	for (i = 0; i < count; ++i) {
		oalGetCallbackInfo(coredumpRun.pid, ids[i], &info);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
	}
out:
	if (ids != NULL)
		memMgrLocalFree(ids);
	return result;
}
