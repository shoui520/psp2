/* EVENT_LOG_INFO note. */

#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetEventlog(void *buffer, SceSize size, SceSize *readBlocks);

typedef struct CoredumpEventLogNote {
	CoredumpNoteHeader header;
	SceUInt32 reserved;
	SceUInt16 readBlocks;
	SceUInt16 reservedTail;
} CoredumpEventLogNote;

_Static_assert(sizeof(CoredumpEventLogNote) == 40, "event-log note size");

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod eventLogMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf eventLogSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 48, "_calcSize");
	segment->numPhdr = 1;
	segment->dataSize = sizeof(CoredumpEventLogNote) + 2048;
	segment->writtenSize = 0;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceSize readBlocks;
	CoredumpEventLogNote note;
	unsigned char data[2048];
	SceSize padding;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 64, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&note.header, "EVENT_LOG_INFO", 0x102c, 9,
		segment->dataSize);
	memset(data, 0, sizeof(data));
	oalGetEventlog(data, sizeof(data), &readBlocks);
	note.reserved = 0;
	note.readBlocks = readBlocks;
	note.reservedTail = 0;
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(note);
	result = writerWrite(currentWriter, data, sizeof(data));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(data);
	padding = ((segment->writtenSize + 3) & ~3U) - segment->writtenSize;
	result = writerWritePaddingData(currentWriter, padding);
	if (result == 0)
		segment->writtenSize += padding;
	return result;
}
