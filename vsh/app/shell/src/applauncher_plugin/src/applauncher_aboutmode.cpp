/*
 * applauncher_aboutmode.cpp: AppTabAboutThisGameMode, the mode of an icon's "about this game"
 * (0x810C86F4-0x810C88BA). The class is Sony's; the file's name and the functions' and
 * variable's names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

/* (0x815442B0) */
const char *AppTabAboutThisGameMode::s_className = "AppTabAboutThisGameMode";

/* PLACEHOLDER: the icon whose "about this game" the mode shows (0x815559A0) */
static paf::common::SharedPtr<ApplauncherIcon> s_aboutIcon;

/* (0x810C86F4) */
const char *AppTabAboutThisGameMode::GetName()
{
	return s_className;
}

/* the icon's "about this game" opened with the mode (0x810C8700) */
AppTabAboutThisGameMode::AppTabAboutThisGameMode()
{
	reserved = 0;
	applauncherAboutThisGameOpen(s_aboutIcon);
}

/* and closed with it (0x810C8728, 0x810C8748) */
AppTabAboutThisGameMode::~AppTabAboutThisGameMode()
{
	applauncherAboutThisGameClose();
}

/* (0x810C876C) */
void applauncherAboutThisGameEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon)
{
	s_aboutIcon = icon;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_ABOUT_THIS_GAME);
}
