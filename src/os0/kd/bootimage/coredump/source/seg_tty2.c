/* TTY_INFO2 note. */

#include <string.h>
#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int tty2Init(SegIf *segment);
extern int tty2CalcSize(SegIf *segment);
extern int oalGetDebugTailLog(char *buffer, SceSize size);

static int _writeSegs(SegIf *segment);

const SegIfMethod tty2Methods = {
	tty2Init,
	tty2CalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

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
