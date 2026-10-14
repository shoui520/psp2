/* PROCESS_INFO segment and process-path size capture. */

#include <stddef.h>
#include <sce/debug.h>
#include "oal_internal.h"
#include "seg_internal.h"

typedef struct ProcessSegment {
	SegIf segment;
	char path[256];
	SceSize pathSize;
} ProcessSegment;

_Static_assert(sizeof(ProcessSegment) == 280, "process segment size");
_Static_assert(offsetof(ProcessSegment, path) == 20, "process path");
_Static_assert(offsetof(ProcessSegment, pathSize) == 276,
	"process path size");

extern int oalGetProcessPath(SceUID pid, char *path, SceSize *pathSize);
extern int processInit(SegIf *segment);
extern int processWriteSegs(SegIf *segment);

static int _calcSize(SegIf *segment);

const SegIfMethod processMethods = {
	processInit,
	_calcSize,
	segWritePtNoteHdr,
	processWriteSegs,
};

static int _calcSize(SegIf *segment)
{
	ProcessSegment *process = (ProcessSegment *)segment;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 79, "_calcSize");
	segment->writtenSize = 0;
	oalGetProcessPath(coredumpRun.pid, process->path, &process->pathSize);
	segment->numPhdr = 1;
	segment->dataSize = ((process->pathSize + 3) & ~3u) + 144;
	return 0;
}
