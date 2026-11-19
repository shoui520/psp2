/*
 * applauncher_launchermode.cpp: LauncherMode, the root of the applauncher's modes, the home
 * itself (0x810C9770-0x810C983E). The class is Sony's; the file's name is ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

/* (0x815442DC) */
const char *LauncherMode::s_className = "LauncherMode";

/* (0x810C9770) */
const char *LauncherMode::GetName()
{
	return s_className;
}

/* (0x810C977C) */
LauncherMode::LauncherMode()
{
}

/* (0x810C9794, 0x810C97B0) */
LauncherMode::~LauncherMode()
{
}
