#include "lowio_int.h"

/* =========================================================================
 * Module entry points
 * ========================================================================= */

s32 module_start(SceSize args, void *argp)
{
	sysregModuleStart(args);
	dmacplusModuleStart(args, argp);
	lcdcModuleStart(args, argp);
	ddrModuleStart(args, argp);
	return 0;
}

s32 module_reboot_before(SceSize args)
{
	ddrModuleStop(args);
	lcdcModuleStop(args);
	dmacplusModuleStop(args);
	sysregModuleStop(args);
	return 0;
}
