/* BUILD_VER_INFO note. */

#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct BuildVersionNote {
	CoredumpNoteHeader header;
	CoredumpBuildVerInfo info;
} BuildVersionNote;

_Static_assert(sizeof(BuildVersionNote) == 544, "build-version note size");

extern Writer *currentWriter;
extern void oalGetBuildVerInfo(SceUID pid, CoredumpBuildVerInfo *info);
extern int oalIsSystemProgram(SceUID pid);
extern int oalIsRootProgram(SceUID pid);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod buildVersionMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf buildVersionSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsSystemProgram(coredumpRun.pid) != 0 ||
	    oalIsRootProgram(coredumpRun.pid) != 0) {
		segment->dataSize = sizeof(BuildVersionNote);
		segment->numPhdr = 1;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	BuildVersionNote note;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 75, "_writeSegs");
	result = segment->numPhdr;
	if (result != 0) {
		utilsMakeNoteHeader(&note.header, "BUILD_VER_INFO", 4125, 6,
			segment->dataSize);
		oalGetBuildVerInfo(coredumpRun.pid, &note.info);
		result = writerWrite(currentWriter, &note, segment->dataSize);
		if (result == 0) {
			segment->writtenSize += segment->dataSize;
			return 0;
		}
	}
	return result;
}
