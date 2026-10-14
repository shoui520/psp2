/* TTY_INFO note. */

#include <string.h>
#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int ttyInit(SegIf *segment);
extern int ttyCalcSize(SegIf *segment);
extern int oalIsToolDipswEnabled(void);
extern int oalGetDebugTailLog(char *buffer, SceSize size);
extern int oalGetDebugTailLogPid(SceUID pid, char *buffer, SceSize size);

static int _writeSegs(SegIf *segment);

const SegIfMethod ttyMethods = {
	ttyInit,
	ttyCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	CoredumpTtyNote note;
	char data[4096];
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 75, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	memset(data, 0, sizeof(data));
	utilsMakeNoteHeader(&note.header, "TTY_INFO", 0x102a, 8,
		segment->dataSize);
	note.reserved = 0;
	note.logSize = sizeof(data);
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize = sizeof(note);
	if (oalIsToolDipswEnabled() != 0)
		oalGetDebugTailLog(data, sizeof(data));
	else
		oalGetDebugTailLogPid(coredumpRun.pid, data, sizeof(data));
	result = writerWrite(currentWriter, data, sizeof(data));
	if (result == 0)
		segment->writtenSize += sizeof(data);
	return result;
}
