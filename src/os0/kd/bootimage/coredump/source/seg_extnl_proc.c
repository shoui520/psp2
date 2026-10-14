/* EXTNL_PROC_INFO note size. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern int oalGetNumOfExtnlProcs(SceUID pid);
extern int oalGetExtnlProcIdList(SceUID pid, SceUID *ids, int capacity);
extern int oalGetProcessPath(SceUID pid, char *path, SceSize *pathSize);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int extnlProcWriteSegs(SegIf *segment);

static int _calcSize(SegIf *segment);

const SegIfMethod extnlProcMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	extnlProcWriteSegs,
};

static int _calcSize(SegIf *segment)
{
	char path[256];
	SceSize pathSize, totalPathSize = 0;
	SceUID *ids;
	unsigned int count, i = 0;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 65, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfExtnlProcs(coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetExtnlProcIdList(coredumpRun.pid, ids, count);
	if (count > 8) {
		do {
			SceSize firstSize, secondSize, thirdSize, fourthSize;
			SceSize fifthSize, sixthSize, seventhSize, eighthSize;

			__builtin_prefetch(&ids[i + 9]);
			oalGetProcessPath(ids[i], path, &pathSize);
			firstSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 1], path, &pathSize);
			secondSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 2], path, &pathSize);
			thirdSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 3], path, &pathSize);
			fourthSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 4], path, &pathSize);
			fifthSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 5], path, &pathSize);
			sixthSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 6], path, &pathSize);
			seventhSize = (pathSize + 3) & ~3u;
			oalGetProcessPath(ids[i + 7], path, &pathSize);
			eighthSize = (pathSize + 3) & ~3u;
			totalPathSize += firstSize + secondSize + thirdSize +
				fourthSize + fifthSize + sixthSize + seventhSize +
				eighthSize;
			i += 8;
		} while (count - 7 > i + 1);
	}
	do {
		oalGetProcessPath(ids[i++], path, &pathSize);
		totalPathSize += (pathSize + 3) & ~3u;
	} while (count > i);
	segment->numPhdr = 1;
	segment->dataSize = totalPathSize + 108 * count + 312;
	memMgrLocalFree(ids);
	return 0;
}
