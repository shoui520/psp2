/*
 * applauncher_iconinfomode.cpp: AppTabIconInfoMode, the mode of an icon's information
 * (0x810C9284-0x810C94DC). The class is Sony's; the file's name and the functions' and
 * variables' names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

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
#if !defined(FW_TYPE_CEX)
	if (s_infoIcon.get() != NULL)
		ScePafAutoTestTty_9E10E873(1, "/livespace/home_page%d/information_%s",
					   applauncherInterfaceGetFocusedPage(),
					   s_infoIcon->m_data->titleId.c_str());
#endif
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
