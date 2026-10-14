/* Crashdump segment writer. */

#include <sce/debug.h>
#include "seg_internal.h"

extern int crashdumpCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod crashdumpMethods = {
	segInitBase,
	crashdumpCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 67, "_writeSegs");
	return 0;
}
