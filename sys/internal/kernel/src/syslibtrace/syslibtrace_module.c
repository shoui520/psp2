/*
 * SceSysLibTrace module start.
 */

#include <sysmem/debug.h>
#include <syslibtrace/syslibtrace.h>
#include "syslibtrace.h"

int SyslibtraceStart(SceSize args, void *argp)
{
	if (SyslibtraceInitialize() < 0)
		SCE_DBG_ASSERT_LEVEL(0, 0, 62);

	SceModulemgrForKernel_B23286B8(g_syslibtraceSyscallInfo, SyslibtraceSyscallTrampoline);
	SceSysrootForKernel_B39CD708(SceNidsymtblForDriver_985E2935);
	return 0;
}
