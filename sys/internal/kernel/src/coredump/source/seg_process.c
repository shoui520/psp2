/* PROCESS_INFO segment and process-path size capture. */

#include <stddef.h>
#include <string.h>
#include <sce/debug.h>
#include "oal_internal.h"
#include "seg_internal.h"
#include "writer_internal.h"

typedef struct ProcessInfoNote {
	CoredumpNoteHeader header;
	CoredumpProcessInfo info;
} ProcessInfoNote;

_Static_assert(sizeof(ProcessInfoNote) == 120, "process info note size");

extern int oalGetProcessPath(SceUID pid, char *path, SceSize *pathSize);
extern Writer *currentWriter;
extern void oalGetProcessInfo(SceUID pid, CoredumpProcessInfo *info);
extern void oalGetProcessArmExInfo(SceUID pid, CoredumpArmExInfo *info);
extern SceUInt64 oalGetProcessTime(SceUID pid);

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod processMethods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

ProcessSegment processSegment;

static int _init(SegIf *segment)
{
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 63, "_init");
	result = segInitBase(segment);
	if (result == 0)
		memset(((ProcessSegment *)segment)->path, 0,
		       sizeof(((ProcessSegment *)segment)->path));
	return result;
}

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

static int _writeSegs(SegIf *segment)
{
	ProcessSegment *process = (ProcessSegment *)segment;
	CoredumpArmExInfo armExInfo;
	ProcessInfoNote note;
	SceUInt64 processTime;
	SceSize pathSize, padding;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 104, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&note.header, "PROCESS_INFO", 0x1002, 1,
		segment->dataSize);
	oalGetProcessInfo(coredumpRun.pid, &note.info);
	pathSize = process->pathSize;
	note.info.pathSize = pathSize;
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(note);
	result = writerWrite(currentWriter, process->path, pathSize);
	if (result != 0)
		return result;
	segment->writtenSize += pathSize;
	padding = ((pathSize + 3) & ~3u) - pathSize;
	result = writerWritePaddingData(currentWriter, padding);
	if (result != 0)
		return result;
	segment->writtenSize += padding;
	oalGetProcessArmExInfo(coredumpRun.pid, &armExInfo);
	result = writerWrite(currentWriter, &armExInfo, sizeof(armExInfo));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(armExInfo);
	processTime = oalGetProcessTime(coredumpRun.pid);
	result = writerWrite(currentWriter, &processTime, sizeof(processTime));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(processTime);
	return result;
}
