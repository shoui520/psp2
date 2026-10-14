/* HW_INFO note. */

#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct HardwareNote {
	CoredumpNoteHeader header;
	CoredumpHwInfo info;
} HardwareNote;

_Static_assert(sizeof(HardwareNote) == 156, "hardware note size");

extern Writer *currentWriter;
extern void oalGetHwInfo(SceUID pid, CoredumpHwInfo *info);
extern int oalIsSystemProgram(SceUID pid);
extern int oalIsRootProgram(SceUID pid);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod hardwareMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsSystemProgram(coredumpRun.pid) != 0 ||
	    oalIsRootProgram(coredumpRun.pid) != 0) {
		segment->dataSize = sizeof(HardwareNote);
		segment->numPhdr = 1;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	HardwareNote note;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 75, "_writeSegs");
	result = segment->numPhdr;
	if (result != 0) {
		utilsMakeNoteHeader(&note.header, "HW_INFO", 4122, 4,
			segment->dataSize);
		oalGetHwInfo(coredumpRun.pid, &note.info);
		result = writerWrite(currentWriter, &note, sizeof(note));
		if (result == 0)
			segment->writtenSize += sizeof(note);
	}
	return result;
}
