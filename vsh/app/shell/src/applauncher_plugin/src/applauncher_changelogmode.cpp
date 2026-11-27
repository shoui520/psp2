/*
 * applauncher_changelogmode.cpp: AppTabChangeLogMode, the mode of an icon's change log
 * (0x810C88BC-0x810C8A82). The class is Sony's; the file's name and the functions' and
 * variable's names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

/* (0x815442B4) */
const char *AppTabChangeLogMode::s_className = "AppTabChangeLogMode";

/* PLACEHOLDER: the icon whose change log the mode shows (0x815559C8) */
static paf::common::SharedPtr<ApplauncherIcon> s_changeLogIcon;

/* (0x810C88BC) */
const char *AppTabChangeLogMode::GetName()
{
	return s_className;
}

/* the icon's change log opened with the mode (0x810C88C8) */
AppTabChangeLogMode::AppTabChangeLogMode()
{
	reserved = 0;
	applauncherChangeLogOpenIcon(s_changeLogIcon);
#if !defined(FW_TYPE_CEX)
	if (s_changeLogIcon.get() != NULL)
		ScePafAutoTestTty_9E10E873(1, "/livespace/home_page%d/history_%s",
					   applauncherInterfaceGetFocusedPage(),
					   s_changeLogIcon->m_data->titleId.c_str());
#endif
}

/* and closed with it (0x810C88F0, 0x810C8910) */
AppTabChangeLogMode::~AppTabChangeLogMode()
{
	applauncherChangeLogClose();
}

/* (0x810C8934) */
void applauncherChangeLogEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon)
{
	s_changeLogIcon = icon;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_CHANGE_LOG);
}
