/* Crashdump segment writer. */

#include <stddef.h>
#include <sysmem/debug.h>
#include "seg_internal.h"

extern SceSize kernelDumpPayloadSize;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod crashdumpMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf crashdumpSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 51, "_calcSize");
	segment->numPhdr = 1;
	segment->writtenSize = 0;
	segment->dataSize = (kernelDumpPayloadSize + sizeof(CoredumpNoteHeader) + 3) & ~3u;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 67, "_writeSegs");
	return 0;
}
