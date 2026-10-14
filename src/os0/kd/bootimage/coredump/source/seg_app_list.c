/* APP_LIST_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfApp(void);
extern void oalGetAppIdList(int *ids, SceSize capacity);
extern void oalGetAppInfo(int appId, CoredumpAppInfo *info, char *path);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int appListCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod appListMethods = {
	segInitBase,
	appListCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

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
