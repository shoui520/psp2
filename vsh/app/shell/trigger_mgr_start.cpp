/*
 * The trigger manager's start, a file of its own in Sony's build (its static constructor
 * follows it). The file's and the function's names are ours (no record names them).
 */
#include "shell.h"

int shellTriggerMgrStart(void)
{
	return shellTriggerMgrInit();
}
