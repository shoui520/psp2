/*
 * SceSysmem: the access control manager's start. No code of it reaches this
 * link; the debug build keeps its record.
 */

#include <fw.h>
#include <sysmem/debug.h>

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, sceSblACMgrModuleStartRecord, 120, "sceSblACMgrModuleStart");
#endif
