/* COREFILE_INFO note. */

#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct CorefileNote {
	CoredumpNoteHeader header;
	CoredumpCorefileInfo info;
} CorefileNote;

_Static_assert(sizeof(CorefileNote) == 68, "corefile note size");

extern Writer *currentWriter;
extern void oalGetCorefileInfo(SceUID pid, CoredumpCorefileInfo *info);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod corefileMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf corefileSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->numPhdr = 1;
	segment->dataSize = sizeof(CorefileNote);
	segment->writtenSize = 0;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CorefileNote note;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 69, "_writeSegs");
	result = segment->numPhdr;
	if (result != 0) {
		memset(&note, 0, sizeof(note));
		utilsMakeNoteHeader(&note.header, "COREFILE_INFO", 4096, 1,
			segment->dataSize);
		oalGetCorefileInfo(coredumpRun.pid, &note.info);
		result = writerWrite(currentWriter, &note, sizeof(note));
		if (result == 0)
			segment->writtenSize += sizeof(note);
	}
	return result;
}
