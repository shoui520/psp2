/* SYS_DEVICE_INFO note. */

#include <sce/debug.h>
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
extern void oalGetVitaCardInfo(CoredumpVitaCardInfo *out);
extern void oalGetMemoryCardInfo(CoredumpMemoryCardInfo *out);
extern int systemDeviceInit(SegIf *segment);
extern int systemDeviceCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod systemDeviceMethods = {
	systemDeviceInit,
	systemDeviceCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

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
