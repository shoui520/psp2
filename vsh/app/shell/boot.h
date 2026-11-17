/*
 * boot.h: SceShell's boot sequence (boot.cpp) and the application id stack it keeps. Names ours.
 */
#ifndef _SHELL_BOOT_H
#define _SHELL_BOOT_H

#include <scetypes.h>

/* a growing array of application ids, used as a stack: grows and shrinks by 10 */
struct ShellAppIdStack {
	int *data;
	int count;
	int capacity;
};

bool shellBootIsLaunchRestricted(void);
bool shellBootArePluginsLoaded(void);
bool shellBootHasAppStack(void);
int shellBootGetDatabaseState(void);
void shellBootLoadColdbootPlugin(void);

#endif
