/*
 * SceSysmem: the heap library's second start phase. No code of it reaches this
 * link; the debug build keeps its record.
 */

#include <fw.h>
#include <sysmem/debug.h>

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, heaplibStartPhase2Record, 124, "HeaplibStartPhase2");
#endif
