/* TTY_INFO2 note. */

#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod tty2Methods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf tty2Segment;

static int _init(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 41, "_init");
	return segInitBase(segment);
}

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 54, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsToolDipswEnabled() == 0) {
		segment->numPhdr = 1;
		segment->dataSize = sizeof(CoredumpTtyNote) + 4096;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpTtyNote note;
	char data[4096];
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 81, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	memset(data, 0, sizeof(data));
	utilsMakeNoteHeader(&note.header, "TTY_INFO2", 0x1031, 19,
		segment->dataSize);
	note.reserved = 0;
	note.logSize = sizeof(data);
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize = sizeof(note);
	oalGetDebugTailLog(data, sizeof(data));
	result = writerWrite(currentWriter, data, sizeof(data));
	if (result == 0)
		segment->writtenSize += sizeof(data);
	return result;
}
