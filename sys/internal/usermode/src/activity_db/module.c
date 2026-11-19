/*
 * SceActivityDb's start and stop: the constructors of its static objects, and their
 * destructors (activity_db_init.c).
 */
#include <scetypes.h>
#include "activity_db_init.h"

int module_start(SceSize argSize, const void *argp)
{
	activityDbInit();
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	activityDbTerm();
	return 0;
}
