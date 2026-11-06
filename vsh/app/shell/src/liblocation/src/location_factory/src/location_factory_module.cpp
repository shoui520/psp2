/* SceLibLocationFactory's start and stop: SceTinyLibc's runtime up and down. */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include "../../../../../../lowlayer/src/tinylibc/tinylibc.h"

extern "C" int module_start(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	tinyLibcInit();
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	tinyLibcTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}
