/* APP_LIST_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalIsSystemProgram(SceUID pid);
extern int oalGetNumOfApp(void);
extern void oalGetAppIdList(int *ids, SceSize capacity);
extern void oalGetAppInfo(int appId, CoredumpAppInfo *info, char *path);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod appListMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf appListSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpAppInfo info;
	char path[256];
	SceSize pathSize = 0;
	unsigned int count, i;
	int *ids;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 64, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsSystemProgram(coredumpRun.pid) == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	count = oalGetNumOfApp();
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetAppIdList(ids, count);
	for (i = 0; i < count; i++) {
		oalGetAppInfo(ids[i], &info, path);
		pathSize += (info.appPathLength + 3) & ~3u;
	}
	segment->numPhdr = 1;
	segment->dataSize = pathSize + count * sizeof(CoredumpAppInfo) + 264;
	memMgrLocalFree(ids);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpAppInfo info;
	char path[256];
	int *ids;
	unsigned int i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 116, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfApp();
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "APP_LIST_INFO", 0x1020, 6,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetAppIdList(ids, count);
	for (i = 0; i < count; ++i) {
		oalGetAppInfo(ids[i], &info, path);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
		result = writerWrite(currentWriter, path,
			(info.appPathLength + 3) & ~3U);
		if (result != 0)
			goto out;
		segment->writtenSize += (info.appPathLength + 3) & ~3U;
	}
	if (segment->writtenSize < segment->dataSize) {
		result = writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		if (result == 0)
			segment->writtenSize = segment->dataSize;
	}
out:
	memMgrLocalFree(ids);
	return result;
}
