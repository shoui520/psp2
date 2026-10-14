/* SYSTEM_INFO note. */

#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct SystemNote {
	CoredumpNoteHeader header;
	CoredumpSystemInfo info;
} SystemNote;

_Static_assert(sizeof(SystemNote) == 124, "system note size");

extern Writer *currentWriter;
extern void oalGetSystemInfo(SceUID pid, CoredumpSystemInfo *info);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod systemMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->numPhdr = 1;
	segment->dataSize = sizeof(SystemNote);
	segment->writtenSize = 0;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SystemNote note;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 69, "_writeSegs");
	result = segment->numPhdr;
	if (result != 0) {
		utilsMakeNoteHeader(&note.header, "SYSTEM_INFO", 4097, 4,
			segment->dataSize);
		oalGetSystemInfo(coredumpRun.pid, &note.info);
		if (coredumpRun.systemInfoFlagsEnabled != 0)
			note.info.flagBits |= 1;
		result = writerWrite(currentWriter, &note, segment->dataSize);
		if (result == 0)
			segment->writtenSize += segment->dataSize;
	}
	return result;
}
