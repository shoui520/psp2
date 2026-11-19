/*
 * applauncher_modemanager.cpp: the applauncher's modes and their manager (0x810C1FE6-
 * 0x810C2100). The file's name and the names here are ours (PLACEHOLDER).
 */
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

/* the modes, each under its parent (0x8150795C) */
static const ModeEntry s_applauncherModes[APPLAUNCHER_MODE_COUNT] = {
	{ MODE_MANAGER_NONE, ModeCreate<LauncherMode> },
	{ APPLAUNCHER_MODE_LAUNCHER, ModeCreate<AppTabEditMode> },
	{ APPLAUNCHER_MODE_EDIT, ModeCreate<AppTabSetThemeMode> },
	{ APPLAUNCHER_MODE_EDIT, ModeCreate<AppTabContextMenuMode> },
	{ APPLAUNCHER_MODE_CONTEXT_MENU, ModeCreate<AppTabIconDelMode> },
	{ APPLAUNCHER_MODE_CONTEXT_MENU, ModeCreate<AppTabChangeLogMode> },
	{ APPLAUNCHER_MODE_CONTEXT_MENU, ModeCreate<AppTabIntellectualProperty> },
	{ APPLAUNCHER_MODE_CONTEXT_MENU, ModeCreate<AppTabIconInfoMode> },
	{ APPLAUNCHER_MODE_CONTEXT_MENU, ModeCreate<AppTabAboutThisGameMode> },
};

/* (0x815079A4) */
static const ModeTable s_applauncherModeTable = {
	s_applauncherModes, APPLAUNCHER_MODE_COUNT
};

/* (0x81555408) */
ApplauncherModeManager *g_applauncherModeManager;

/* (0x810C1FE6) */
void applauncherModeManagerCreate(void)
{
	new ApplauncherModeManager(s_applauncherModeTable);
}

/* (0x810C2024) */
void applauncherModeManagerDestroy(void)
{
	if (g_applauncherModeManager != NULL)
		delete g_applauncherModeManager;
}

/* (0x810C203C, 0x810C2064) */
ApplauncherModeManager::~ApplauncherModeManager()
{
	g_applauncherModeManager = NULL;
}
