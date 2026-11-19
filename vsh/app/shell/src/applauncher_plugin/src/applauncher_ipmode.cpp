/*
 * applauncher_ipmode.cpp: AppTabIntellectualProperty, the mode of the intellectual property
 * notice (0x810C23E0-0x810C24D8). The class is Sony's; the file's name and the functions' names
 * are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

/* (0x8154428C) */
const char *AppTabIntellectualProperty::s_className = "AppTabIntellectualProperty";

/* (0x810C23E0) */
const char *AppTabIntellectualProperty::GetName()
{
	return s_className;
}

/* the notice opened with the mode (0x810C23EC) */
AppTabIntellectualProperty::AppTabIntellectualProperty()
{
	reserved = 0;
	applauncherIntellectualPropertyOpen();
}

/* and closed with it (0x810C240C, 0x810C242C) */
AppTabIntellectualProperty::~AppTabIntellectualProperty()
{
	applauncherIntellectualPropertyClose();
}

/* (0x810C2450) */
void applauncherIntellectualPropertyEnter(void)
{
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_INTELLECTUAL_PROPERTY);
}
