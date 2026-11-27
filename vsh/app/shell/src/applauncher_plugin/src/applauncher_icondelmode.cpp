/*
 * applauncher_icondelmode.cpp: AppTabIconDelMode, the mode of an icon's deletion
 * (0x810C8FD4-0x810C9284). The class is Sony's; the file's name and the functions' and
 * variables' names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include "../../../plugins.h"

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

/* (0x815442C4) */
const char *AppTabIconDelMode::s_className = "AppTabIconDelMode";

/* PLACEHOLDER: what the mode's delete dialog is opened with: the context menu's pointer
   (0x815442C8), the icon (0x81555A40) and the value (0x81555A48) */
static void *s_context = NULL;
static paf::common::SharedPtr<ApplauncherIcon> s_deleteIcon;
static int s_value;

/* (0x810C8FD4) */
const char *AppTabIconDelMode::GetName()
{
	return s_className;
}

/* the delete dialog opened with the mode, topmenu held, and the edit kept on a power event
   (0x810C8FE0) */
AppTabIconDelMode::AppTabIconDelMode()
{
	reserved = 0;
	applauncherDeleteDialogOpen(s_deleteIcon, s_context, s_value);
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GAME_HOLD])();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD_BEGIN])();
	applauncherEditModeHold();
#if !defined(FW_TYPE_CEX)
	if (s_deleteIcon.get() != NULL)
		ScePafAutoTestTty_9E10E873(1, "/livespace/home_page%d/delete_%s",
					   applauncherInterfaceGetFocusedPage(),
					   s_deleteIcon->m_data->titleId.c_str());
#endif
}

/* all of it undone with the mode, and what it was opened with let go of (0x810C903C,
   0x810C9104) */
AppTabIconDelMode::~AppTabIconDelMode()
{
	applauncherEditModeRelease();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD_END])();
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GAME_RELEASE])();
	applauncherDeleteDialogClose();
	s_deleteIcon.reset();
	s_context = NULL;
}

/* (0x810C9114) */
void applauncherIconDelEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon, void *context,
			     int value)
{
	s_deleteIcon = icon;
	s_context = context;
	s_value = value;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_ICON_DEL);
}
