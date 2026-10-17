/* SYS_DEVICE_INFO note. */

#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct CoredumpSystemDeviceNote {
	CoredumpNoteHeader header;
	CoredumpVitaCardInfo vitaCard;
	CoredumpMemoryCardInfo memoryCard;
} CoredumpSystemDeviceNote;

_Static_assert(sizeof(CoredumpSystemDeviceNote) == 208,
	"system device note size");

extern Writer *currentWriter;
extern int oalIsSystemProgram(SceUID pid);
extern void oalGetVitaCardInfo(CoredumpVitaCardInfo *out);
extern void oalGetMemoryCardInfo(CoredumpMemoryCardInfo *out);

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod systemDeviceMethods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf systemDeviceSegment;

static int _init(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 42, "_init");
	return segInitBase(segment);
}

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsSystemProgram(coredumpRun.pid) != 0) {
		segment->numPhdr = 1;
		segment->dataSize = sizeof(CoredumpNoteHeader) +
			sizeof(CoredumpVitaCardInfo) + sizeof(CoredumpMemoryCardInfo);
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpSystemDeviceNote note;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 77, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&note.header, "SYS_DEVICE_INFO", 0x102f, 17,
		segment->dataSize);
	oalGetVitaCardInfo(&note.vitaCard);
	oalGetMemoryCardInfo(&note.memoryCard);
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result == 0)
		segment->writtenSize = sizeof(note);
	return result;
}
