/* TTY_INFO note. */

#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalIsToolDipswEnabled(void);
extern int oalGetDebugTailLog(char *buffer, SceSize size);
extern int oalGetDebugTailLogPid(SceUID pid, char *buffer, SceSize size);

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ttyMethods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ttySegment;

static int _init(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 41, "_init");
	return segInitBase(segment);
}

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 54, "_calcSize");
	segment->numPhdr = 1;
	segment->dataSize = sizeof(CoredumpTtyNote) + 4096;
	segment->writtenSize = 0;
	return 0;
}

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
