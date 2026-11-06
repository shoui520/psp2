/*
 * ScebXCe's module entry points.
 */
#include <scetypes.h>
#include "bxce_internal.h"

extern "C" int module_start(SceSize args, const void *argp)
{
	return 0;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	bxceDeleteDefaultAllocator();
	return 0;
}
