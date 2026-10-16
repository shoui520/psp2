/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"

int module_start(SceSize args, void *argp)
{
	if (createDriverMutex() < 0)
		return 2;
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	terminateFileService();
	terminateDriver();
	deleteDriverMutex();
	return 0;
}
