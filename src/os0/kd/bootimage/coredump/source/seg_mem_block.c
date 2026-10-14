/* MEM_BLK_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfMemBlocks(SceUID pid);
extern void oalGetMemBlockIdList(SceUID pid, SceUID *ids, SceSize capacity);
extern void oalGetMemBlockInfo(SceUID pid, SceUID uid, CoredumpMemBlockInfo *info);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod memBlockMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _calcSize(SegIf *segment)
{
	unsigned int count;
	SceUInt32 numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfMemBlocks(coredumpRun.pid);
	numPhdr = count;
	dataSize = count;
	if (count != 0) {
		dataSize = count * sizeof(CoredumpMemBlockInfo) + 252;
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
	CoredumpMemBlockInfo info;
	SceUID *ids;
	unsigned int i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 82, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfMemBlocks(coredumpRun.pid);
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "MEM_BLK_INFO", 0x1007, 1,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetMemBlockIdList(coredumpRun.pid, ids, count);
	for (i = 0; i < count; ++i) {
		oalGetMemBlockInfo(coredumpRun.pid, ids[i], &info);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
	}
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
out:
	memMgrLocalFree(ids);
	return result;
}
