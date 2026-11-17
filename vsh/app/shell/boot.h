/*
 * boot.h: SceShell's boot sequence (boot.cpp) and the application id stack it keeps. Names ours.
 */
#ifndef _SHELL_BOOT_H
#define _SHELL_BOOT_H

#include <scetypes.h>


bool shellBootIsLaunchRestricted(void);
bool shellBootArePluginsLoaded(void);
bool shellBootHasAppStack(void);
int shellBootGetDatabaseState(void);
void shellBootLoadColdbootPlugin(void);

#endif
