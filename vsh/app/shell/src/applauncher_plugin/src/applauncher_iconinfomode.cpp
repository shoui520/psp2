/*
 * applauncher_iconinfomode.cpp: AppTabIconInfoMode, the mode of an icon's information
 * (0x810C9284-0x810C94DC). The class is Sony's; the file's name and the functions' and
 * variables' names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

/* (0x815442CC) */
const char *AppTabIconInfoMode::s_className = "AppTabIconInfoMode";

/* PLACEHOLDER: what the mode's information is opened with: the context menu's pointer
   (0x815442D0) and the icon (0x81555A70) */
static void *s_context = NULL;
static paf::common::SharedPtr<ApplauncherIcon> s_infoIcon;

/* (0x810C9284) */
const char *AppTabIconInfoMode::GetName()
{
	return s_className;
}

/* the icon's information opened with the mode (0x810C9290) */
AppTabIconInfoMode::AppTabIconInfoMode()
{
	reserved = 0;
	applauncherIconInfoOpen(s_infoIcon, s_context);
}

/* closed with it, and what it was opened with let go of (0x810C92C4, 0x810C936C) */
AppTabIconInfoMode::~AppTabIconInfoMode()
{
	applauncherIconInfoClose();
	s_infoIcon.reset();
	s_context = NULL;
}

/* (0x810C937C) */
void applauncherIconInfoEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon,
			      void *context)
{
	s_infoIcon = icon;
	s_context = context;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_ICON_INFO);
}
