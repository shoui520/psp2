/* SUMMARY_INFO note. */

#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int summaryInit(SegIf *segment);
extern int summaryCalcSize(SegIf *segment);

typedef struct CoredumpSummaryNote {
	CoredumpNoteHeader header;
	SceUInt32 reserved;
	SceUInt32 numSegments;
} CoredumpSummaryNote;

_Static_assert(sizeof(CoredumpSummaryNote) == 40, "summary note size");

static int _writeSegs(SegIf *segment);

const SegIfMethod summaryMethods = {
	summaryInit,
	summaryCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	SummarySegIf *summary = (SummarySegIf *)segment;
	CoredumpSummaryNote note;
	SceUInt32 type, size, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 85, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&note.header, "SUMMARY_INFO", 0x102e, 17,
		segment->dataSize);
	note.reserved = 0;
	note.numSegments = summary->numSegments;
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	type = 0;
	size = 0;
	for (i = 0; i < summary->numSegments; ++i) {
		type = summary->pairs[i].segment->segmentType;
		result = writerWrite(currentWriter, &type, sizeof(type));
		if (result != 0)
			return result;
		size = summary->pairs[i].segment->dataSize;
		result = writerWrite(currentWriter, &size, sizeof(size));
		if (result != 0)
			return result;
		size = summary->pairs[i].segment->writtenSize;
		result = writerWrite(currentWriter, &size, sizeof(size));
		if (result != 0)
			return result;
	}
	return 0;
}
